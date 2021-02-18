#include "process.h"

struct Process {
	HANDLE pipe_read, pipe_write;
	PROCESS_INFORMATION process_info;
	char error[64];
};

bool process_exec(Process *process, char const *program, char **argv) {
	// thanks to https://stackoverflow.com/a/35658917
	bool success = false;
	memset(process, 0, sizeof *process);
	HANDLE pipe_read, pipe_write;
	SECURITY_ATTRIBUTES security_attrs = {sizeof(SECURITY_ATTRIBUTES)};
	security_attrs.bInheritHandle = TRUE;
	if (CreatePipe(&pipe_read, &pipe_write, &security_attrs, 0)) {
		STARTUPINFOA startup = {sizeof(STARTUPINFOA)};
		startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		startup.hStdOutput = pipe_write;
		startup.hStdError = pipe_write;
		startup.wShowWindow = SW_HIDE;
		
		// fuckin windows
		char command_line[4096];
		strbuf_cpy(command_line, program);
		strbuf_catf(command_line, " ");
		for (int i = 0; argv[i]; ++i) {
			strbuf_catf(command_line, "%s ", argv[i]);
		}
		
		if (CreateProcessA(NULL, command_line, NULL, NULL, TRUE, CREATE_NEW_CONSOLE,
			NULL, NULL, &startup, &process->process_info)) {
			process->pipe_read = pipe_read;
			process->pipe_write = pipe_write;
			success = true;
		} else {
			strbuf_printf(process->error, "Couldn't create process (error code %u)", (unsigned)GetLastError());
		}
		if (!success) {
			CloseHandle(pipe_read);
			CloseHandle(pipe_write);
		}
	} else {
		strbuf_printf(process->error, "Couldn't create pipe (error code %u)", (unsigned)GetLastError());
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
		strbuf_printf(process->error, "Couldn't read from pipe (error code %u)", (unsigned)GetLastError());
		return -2;
	}
}

void process_kill(Process *process) {
	TerminateProcess(process->process_info.hProcess, 1);
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