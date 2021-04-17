#include "process.h"

struct Process {
	HANDLE pipe_read, pipe_write;
	HANDLE job;
	PROCESS_INFORMATION process_info;
	char error[200];
};

static void get_last_error_str(char *out, size_t out_sz) {
	size_t size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
         NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), out, (DWORD)out_sz - 1, NULL);
	out[size] = 0;
	char *cr = strchr(out, '\r');
	if (cr) *cr = '\0'; // get rid of carriage return+newline at end of error
}

bool process_run(Process *process, char const *command) {
	// thanks to https://stackoverflow.com/a/35658917 for the pipe code
	// thanks to https://devblogs.microsoft.com/oldnewthing/20131209-00/?p=2433 for the job code

	bool success = false;
	memset(process, 0, sizeof *process);
	char *command_line = str_dup(command);
	if (!command_line) {
		strbuf_printf(process->error, "Out of memory.");
		return false;
	}
	// we need to create a "job" for this, because when you kill a process on windows,
	// all its children just keep going. so cmd.exe would die, but not the actual build process.
	// jobs fix this, apparently.
	HANDLE job = CreateJobObjectA(NULL, NULL);
	if (job) {
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = {0};
		job_info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(job, JobObjectExtendedLimitInformation, &job_info, sizeof job_info);
		HANDLE pipe_read, pipe_write;
		SECURITY_ATTRIBUTES security_attrs = {sizeof(SECURITY_ATTRIBUTES)};
		security_attrs.bInheritHandle = TRUE;
		if (CreatePipe(&pipe_read, &pipe_write, &security_attrs, 0)) {
			STARTUPINFOA startup = {sizeof(STARTUPINFOA)};
			startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
			startup.hStdOutput = pipe_write;
			startup.hStdError = pipe_write;
			startup.wShowWindow = SW_HIDE;
			PROCESS_INFORMATION *process_info = &process->process_info;
			if (CreateProcessA(NULL, command_line, NULL, NULL, TRUE, CREATE_NEW_CONSOLE | CREATE_SUSPENDED,
				NULL, NULL, &startup, process_info)) {
				// create a suspended process, add it to the job, then resume (unsuspend) the process
				if (AssignProcessToJobObject(job, process_info->hProcess)) {
					if (ResumeThread(process_info->hThread) != (DWORD)-1) {
						process->job = job;
						process->pipe_read = pipe_read;
						process->pipe_write = pipe_write;
						success = true;
					}
				}
				if (!success) {
					TerminateProcess(process_info->hProcess, 1);
					CloseHandle(process_info->hProcess);
					CloseHandle(process_info->hThread);
				}
			} else {
				char buf[150];
				get_last_error_str(buf, sizeof buf);
				strbuf_printf(process->error, "Couldn't run `%s`: %s", command, buf);
			}
			free(command_line);
			if (!success) {
				CloseHandle(pipe_read);
				CloseHandle(pipe_write);
			}
		} else {
			char buf[150];
			get_last_error_str(buf, sizeof buf);
			strbuf_printf(process->error, "Couldn't create pipe: %s", buf);
		}
		if (!success)
			CloseHandle(job);
	}
	return success;
}

char const *process_geterr(Process *p) {
	return *p->error ? p->error : NULL;
}

long long process_read(Process *process, char *data, size_t size) {
	DWORD bytes_read = 0, bytes_avail = 0, bytes_left = 0;
	if (PeekNamedPipe(process->pipe_read, data, (DWORD)size, &bytes_read, &bytes_avail, &bytes_left)) {
		if (bytes_read == 0) {
			return -1;
		} else {
			ReadFile(process->pipe_read, data, (DWORD)size, &bytes_read, NULL); // make sure data is removed from pipe
			return bytes_read;
		}
	} else {
		char buf[150];
		get_last_error_str(buf, sizeof buf);
		strbuf_printf(process->error, "Couldn't read from pipe: %s", buf);
		return -2;
	}
}

void process_kill(Process *process) {
	CloseHandle(process->job);
	CloseHandle(process->pipe_read);
	CloseHandle(process->pipe_write);
	CloseHandle(process->process_info.hProcess);
	CloseHandle(process->process_info.hThread);
}

int process_check_status(Process *process, char *message, size_t message_size) {
	HANDLE hProcess = process->process_info.hProcess;
	DWORD exit_code = 1;
	if (GetExitCodeProcess(hProcess, &exit_code)) {
		if (exit_code == STILL_ACTIVE) {
			return 0;
		} else {
			process_kill(process);
			if (exit_code == 0) {
				str_printf(message, message_size, "exited successfully");
				return +1;
			} else {
				str_printf(message, message_size, "exited with code %d", (int)exit_code);
				return -1;
			}
		}
	} else {
		// something has gone wrong.
		str_printf(message, message_size, "couldn't get process exit status");
		process_kill(process);
		return -1;
	}
}
