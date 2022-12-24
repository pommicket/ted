#include "process.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

struct Process {
	pid_t pid;
	int stdout_pipe;
	// only applicable if separate_stderr was specified.
	int stderr_pipe;
	int stdin_pipe;
	char error[64];
};

int process_get_id(void) {
	return getpid();
}

static void set_nonblocking(int fd) {
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

bool process_run_ex(Process *proc, const char *command, const ProcessSettings *settings) {
	memset(proc, 0, sizeof *proc);

	int stdin_pipe[2] = {0}, stdout_pipe[2] = {0}, stderr_pipe[2] = {0};
	if (pipe(stdin_pipe) != 0) {
		strbuf_printf(proc->error, "%s", strerror(errno));
		return false; 
	}
	if (pipe(stdout_pipe) != 0) {
		strbuf_printf(proc->error, "%s", strerror(errno));
		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
		return false;
	}
	if (settings->separate_stderr) {
		if (pipe(stderr_pipe) != 0) {
			strbuf_printf(proc->error, "%s", strerror(errno));
			close(stdin_pipe[0]);
			close(stdin_pipe[1]);
			close(stdout_pipe[0]);
			close(stdout_pipe[1]);
			return false;
		}
	}
	
	bool success = false;
	pid_t pid = fork();
	if (pid == 0) {
		// child process
		chdir(settings->working_directory);
		// put child in its own group. it will be in this group with all of its descendents,
		// so by killing everything in the group, we kill all the descendents of this process.
		// if we didn't do this, we would just be killing the sh process in process_kill.
		setpgid(0, 0);
		// pipe stuff
		dup2(stdout_pipe[1], STDOUT_FILENO);
		if (stderr_pipe[1])
			dup2(stderr_pipe[1], STDERR_FILENO);
		else
			dup2(stdout_pipe[1], STDERR_FILENO);
		dup2(stdin_pipe[0],  STDIN_FILENO);
		// don't need these file descriptors anymore
		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		if (stderr_pipe[0]) {
			close(stderr_pipe[0]);
			close(stderr_pipe[1]);
		}
		
		char *program = "/bin/sh";
		char *argv[] = {program, "-c", (char *)command, NULL};
		if (execv(program, argv) == -1) {
			dprintf(STDERR_FILENO, "%s: %s\n", program, strerror(errno));
			exit(127);
		}
	} else if (pid > 0) {
		// parent process
		
		// we're reading from (the child's) stdout/stderr and writing to stdin,
		// so we don't need the write end of the stdout pipe or the
		// read end of the stdin pipe.
		close(stdout_pipe[1]);
		if (stderr_pipe[1])	
			close(stderr_pipe[1]);
		close(stdin_pipe[0]);
		// set pipes to non-blocking
		if (!settings->stdout_blocking)
			set_nonblocking(stdout_pipe[0]);
		if (stderr_pipe[0] && !settings->stderr_blocking)
			set_nonblocking(stderr_pipe[0]);
		if (!settings->stdin_blocking)
			set_nonblocking(stdin_pipe[1]);
		proc->pid = pid;
		proc->stdout_pipe = stdout_pipe[0];
		if (stderr_pipe[0])
			proc->stderr_pipe = stderr_pipe[0];
		proc->stdin_pipe = stdin_pipe[1];
		success = true;
	}
	return success;
}

bool process_run(Process *proc, char const *command) {
	const ProcessSettings settings = {0};
	return process_run_ex(proc, command, &settings);
}


char const *process_geterr(Process *p) {
	return *p->error ? p->error : NULL;
}

long long process_write(Process *proc, const char *data, size_t size) {
	assert(proc->stdin_pipe); // check that process hasn't been killed
	ssize_t bytes_written = write(proc->stdin_pipe, data, size);
	if (bytes_written >= 0) {
		return (long long)bytes_written;
	} else if (errno == EAGAIN) {
		return 0;
	} else {
		strbuf_printf(proc->error, "%s", strerror(errno));
		return -2;
	}
}

static long long process_read_fd(Process *proc, int fd, char *data, size_t size) {
	assert(fd);
	ssize_t bytes_read = read(fd, data, size);
	if (bytes_read >= 0) {
		return (long long)bytes_read;
	} else if (errno == EAGAIN) {
		return -1;
	} else {
		strbuf_printf(proc->error, "%s", strerror(errno));
		return -2;
	}
}

long long process_read(Process *proc, char *data, size_t size) {
	return process_read_fd(proc, proc->stdout_pipe, data, size);
}

long long process_read_stderr(Process *proc, char *data, size_t size) {
	return process_read_fd(proc, proc->stderr_pipe, data, size);
}

static void process_close_pipes(Process *proc) {
	close(proc->stdin_pipe);
	close(proc->stdout_pipe);
	close(proc->stderr_pipe);
	proc->stdin_pipe = 0;
	proc->stdout_pipe = 0;
	proc->stderr_pipe = 0;
}

void process_kill(Process *proc) {
	kill(-proc->pid, SIGKILL); // kill everything in process group
	// get rid of zombie process
	waitpid(proc->pid, NULL, 0);
	proc->pid = 0;
	process_close_pipes(proc);
}

int process_check_status(Process *proc, char *message, size_t message_size) {
	int wait_status = 0;
	int ret = waitpid(proc->pid, &wait_status, WNOHANG);
	if (ret == 0) {
		// process still running
		return 0;
	} else if (ret > 0) {
		if (WIFEXITED(wait_status)) {
			process_close_pipes(proc);
			int code = WEXITSTATUS(wait_status);
			if (code == 0) {
				str_printf(message, message_size, "exited successfully");
				return +1;
			} else {
				str_printf(message, message_size, "exited with code %d", code);
				return -1;
			}
		} else if (WIFSIGNALED(wait_status)) {
			process_close_pipes(proc);
			str_printf(message, message_size, "terminated by signal %d", WTERMSIG(wait_status));
			return -1;
		}
		return 0;
	} else {
		// this process is gone or something?
		process_close_pipes(proc);
		str_printf(message, message_size, "process ended unexpectedly");
		return -1;
	}
}
