#include "process.h"

#include <fcntl.h>
#include <sys/wait.h>

struct Process {
	pid_t pid;
	int pipe;
	char error[64];
};

bool process_exec(Process *proc, char const *program, char **argv) {
	bool success = false;

	int pipefd[2];
	if (pipe(pipefd) == 0) {
		pid_t pid = fork();
		if (pid == 0) {
			// child process
			// send stdout and stderr to pipe
			dup2(pipefd[1], STDOUT_FILENO);
			dup2(pipefd[1], STDERR_FILENO);
			close(pipefd[0]);
			close(pipefd[1]);
			if (execv(program, argv) == -1) {
				dprintf(STDERR_FILENO, "%s: %s\n", program, strerror(errno));
				exit(127);
			}
		} else if (pid > 0) {
			// parent process
			close(pipefd[1]);
			fcntl(pipefd[0], F_SETFL, fcntl(pipefd[0], F_GETFL) | O_NONBLOCK); // set pipe to non-blocking
			proc->pid = pid;
			proc->pipe = pipefd[0];
			success = true;
		}
	} else {
		strbuf_printf(proc->error, "%s", strerror(errno));
	}
	return success;
}

char const *process_geterr(Process *p) {
	return p->error;
}

long long process_read(Process *proc, char *data, size_t size) {
	assert(proc->pipe);
	ssize_t bytes_read = read(proc->pipe, data, size);
	if (bytes_read >= 0) {
		return (long long)bytes_read;
	} else if (errno == EAGAIN) {
		return -1;
	} else {
		strbuf_printf(proc->error, "%s", strerror(errno));
		return -2;
	}

}

void process_stop(Process *proc) {
	kill(proc->pid, SIGKILL);
	close(proc->pipe);
	proc->pid = 0;
	proc->pipe = 0;
}

int process_check_status(Process *proc, char *message, size_t message_size) {
	int wait_status = 0;
	if (waitpid(proc->pid, &wait_status, WNOHANG) >= 0) {
		if (WIFEXITED(wait_status)) {
			int code = WEXITSTATUS(wait_status);
			if (code == 0) {
				str_printf(message, message_size, "exited successfully");
				return +1;
			} else {
				str_printf(message, message_size, "exited with code %d", code);
				return -1;
			}
		} else if (WIFSIGNALED(wait_status)) {
			str_printf(message, message_size, "terminated by signal %d", WTERMSIG(wait_status));
			return -1;
		}
		return 0;
	} else if (errno == ECHILD) {
		// this process is gone or something?
		str_printf(message, message_size, "process ended unexpectedly");
		return -1;
	} else {
		// probably shouldn't happen
		return 0;
	}
}
