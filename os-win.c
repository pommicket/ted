#include "filesystem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <sysinfoapi.h>

static FsType windows_file_attributes_to_type(DWORD attrs) {
	if (attrs == INVALID_FILE_ATTRIBUTES)
		return FS_NON_EXISTENT;
	else if (attrs & FILE_ATTRIBUTE_DIRECTORY)
		return FS_DIRECTORY;
	else
		return FS_FILE;
}

FsType fs_path_type(char const *path) {
	WCHAR wide_path[4100];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, sizeof wide_path) == 0) {
		return FS_NON_EXISTENT;
	}
	return windows_file_attributes_to_type(GetFileAttributesW(wide_path));
}

FsPermission fs_path_permission(char const *path) {
	FsPermission permission = 0;
	if (_access(path, 04) == 0) permission |= FS_PERMISSION_READ;
	if (_access(path, 02) == 0) permission |= FS_PERMISSION_WRITE;
	return permission;
}

bool fs_file_exists(char const *path) {
	return fs_path_type(path) == FS_FILE;
}

FsDirectoryEntry **fs_list_directory(char const *dirname) {
	char file_pattern[1000] = {0};
	FsDirectoryEntry **files = NULL;
	WIN32_FIND_DATAW find_data;
	HANDLE fhandle;
	assert(*dirname);
	sprintf_s(file_pattern, sizeof file_pattern, "%s%s*", dirname,
		dirname[strlen(dirname) - 1] == PATH_SEPARATOR ? "" : PATH_SEPARATOR_STR);
	wchar_t wide_pattern[1024] = {0};
	MultiByteToWideChar(CP_UTF8, 0, file_pattern, -1, wide_pattern, sizeof wide_pattern - 1);
	
	fhandle = FindFirstFileW(wide_pattern, &find_data);
	if (fhandle != INVALID_HANDLE_VALUE) {
		// first, figure out number of files
		int nfiles = 1, idx = 0;
		while (FindNextFileW(fhandle, &find_data))  {
			++nfiles;
		}
		FindClose(fhandle);
		// now, fill out files array
		files = calloc(nfiles + 1, sizeof *files);
		if (files) {
			fhandle = FindFirstFileW(wide_pattern, &find_data);
			if (fhandle != INVALID_HANDLE_VALUE) {
				do {
					if (idx < nfiles) {
						LPWSTR wide_filename = find_data.cFileName;
						size_t wide_len = wcslen(wide_filename);
						size_t cap = 4 * wide_len + 4;
						FsDirectoryEntry *entry = calloc(1, sizeof *entry + cap);
						
						
						if (entry) {
							if (WideCharToMultiByte(CP_UTF8, 0, wide_filename, (int)wide_len, entry->name, (int)cap - 1, NULL, NULL) == 0)
								break;
							DWORD attrs = find_data.dwFileAttributes;
							entry->type = windows_file_attributes_to_type(attrs);
							files[idx++] = entry;
						} else break; // stop now
					}
				} while (FindNextFileW(fhandle, &find_data));
				FindClose(fhandle);
			}
		}
	}
	return files;
}

int fs_mkdir(char const *path) {
	WCHAR wide_path[4100];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, sizeof wide_path) == 0)
		return -1;
	
	if (CreateDirectoryW(wide_path, NULL)) {
		// directory created successfully
		return 1;
	} else {
		if (GetLastError() == ERROR_ALREADY_EXISTS) // directory already exists
			return 0;
		else
			return -1; // some other error
	}
}

int fs_get_cwd(char *buf, size_t buflen) {
	assert(buf && buflen);
	wchar_t wide_path[4100];
	DWORD wide_pathlen = GetCurrentDirectoryW(sizeof wide_path - 1, wide_path);
	if (wide_pathlen == 0) return -1;
	if (WideCharToMultiByte(CP_UTF8, 0, wide_path, (int)wide_pathlen, buf, (int)buflen, NULL, NULL) == 0)
		return 0;
	return 1;
}

struct timespec time_last_modified(char const *filename) {
	// windows' _stat does not have st_mtim
	struct _stat statbuf = {0};
	struct timespec ts = {0};
	_stat(filename, &statbuf);
	ts.tv_sec = statbuf.st_mtime;
	return ts;
}


struct timespec time_get(void) {
	struct timespec ts = {0};
	timespec_get(&ts, TIME_UTC);
	return ts;
}

void time_sleep_ns(u64 ns) {
	// windows....
	Sleep((DWORD)(ns / 1000000));
}

#error "@TODO :  implement process_write, separate_stderr, working_directory"

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
