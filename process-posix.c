#include "process.h"

#include <fcntl.h>
#include <sys/wait.h>

struct Process {
	pid_t pid;
	int pipe;
	bool stopped;
	int exit_status; // INT_MAX if process was terminated
};

static char process_error[64];
static Process *processes;

static void process_sigaction_sigchld(int signal, siginfo_t *info, void *context) {
	(void)context;
	if (signal == SIGCHLD) {
		pid_t pid = info->si_pid;
		arr_foreach_ptr(processes, Process, process) {
			if (process->pid == pid) {
				process->stopped = true;
				if (info->si_code == CLD_EXITED)
					process->exit_status = info->si_status;
				else
					process->exit_status = INT_MAX;
			}
		}
	}
}

void process_init(void) {
	struct sigaction act = {0};
	act.sa_sigaction = process_sigaction_sigchld;
	act.sa_flags = SA_SIGINFO | SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);
}



Process *process_exec(char const *program, char **argv) {
	Process *proc = NULL;

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
			proc = arr_addp(processes);
			// parent process
			close(pipefd[1]);
			fcntl(pipefd[0], F_SETFL, fcntl(pipefd[0], F_GETFL) | O_NONBLOCK); // set pipe to non-blocking
			proc->pid = pid;
			proc->pipe = pipefd[0];
		}
	} else {
		strbuf_printf(process_error, "%s", strerror(errno));
	}
	return proc;
}

char const *process_geterr(void) {
	return *process_error ? process_error : NULL;
}

long long process_read(Process *proc, char *data, size_t size) {
	assert(proc->pipe);
	ssize_t bytes_read = read(proc->pipe, data, size);
	if (bytes_read >= 0) {
		return (long long)bytes_read;
	} else if (errno == EAGAIN) {
		return -1;
	} else {
		strbuf_printf(process_error, "%s", strerror(errno));
		return -2;
	}

}

void process_stop(Process *proc) {
	kill(proc->pid, SIGKILL);
	// get rid of zombie process
	waitpid(proc->pid, NULL, 0);
	close(proc->pipe);
	for (u32 i = 0; i < arr_len(processes); ++i)
		if (processes[i].pid == proc->pid)
			arr_remove(processes, i);
}
#include <sys/resource.h>
int process_check_status(Process *proc, char *message, size_t message_size) {
	if (proc->stopped) {
		int status = proc->exit_status;
		process_stop(proc);

		switch (status) {
		case 0:
			if (message) str_printf(message, message_size, "exited successfully");
			return +1;
		case INT_MAX:
			if (message) str_printf(message, message_size, "terminated by a signal");
			return -1;
		default:
			if (message) str_printf(message, message_size, "exited with code %d", status);
			return -1;
		}
	} else {
		return 0;
	}
}
