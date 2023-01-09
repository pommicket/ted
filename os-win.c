// windows implementation of OS functions

#include "os.h"
#include "util.h"
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

FsType fs_path_type(const char *path) {
	WCHAR wide_path[4100];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, arr_count(wide_path)) == 0) {
		return FS_NON_EXISTENT;
	}
	return windows_file_attributes_to_type(GetFileAttributesW(wide_path));
}

FsPermission fs_path_permission(const char *path) {
	FsPermission permission = 0;
	if (_access(path, 04) == 0) permission |= FS_PERMISSION_READ;
	if (_access(path, 02) == 0) permission |= FS_PERMISSION_WRITE;
	return permission;
}

bool fs_file_exists(const char *path) {
	return fs_path_type(path) == FS_FILE;
}

FsDirectoryEntry **fs_list_directory(const char *dirname) {
	char file_pattern[4100];
	FsDirectoryEntry **files = NULL;
	WIN32_FIND_DATAW find_data;
	HANDLE fhandle;
	assert(*dirname);
	sprintf_s(file_pattern, sizeof file_pattern, "%s%s*", dirname,
		dirname[strlen(dirname) - 1] == PATH_SEPARATOR ? "" : PATH_SEPARATOR_STR);
	wchar_t wide_pattern[4100] = {0};
	if (MultiByteToWideChar(CP_UTF8, 0, file_pattern, -1, wide_pattern, arr_count(wide_pattern)) == 0)
		return NULL;
	
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

int fs_mkdir(const char *path) {
	WCHAR wide_path[4100];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, arr_count(wide_path)) == 0)
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

int os_get_cwd(char *buf, size_t buflen) {
	assert(buf && buflen);
	wchar_t wide_path[4100];
	DWORD wide_pathlen = GetCurrentDirectoryW(sizeof wide_path - 1, wide_path);
	if (wide_pathlen == 0) return -1;
	if (WideCharToMultiByte(CP_UTF8, 0, wide_path, (int)wide_pathlen, buf, (int)buflen, NULL, NULL) == 0)
		return 0;
	return 1;
}

int os_rename_overwrite(const char *oldname, const char *newname) {
	wchar_t wide_oldname[4100];
	wchar_t wide_newname[4100];
	if (MultiByteToWideChar(CP_UTF8, 0, oldname, -1, wide_oldname, arr_count(wide_oldname)) == 0
		|| MultiByteToWideChar(CP_UTF8, 0, newname, -1, wide_newname, arr_count(wide_newname)) == 0)
		return -1;
	if (CopyFileW(wide_oldname, wide_newname, false) == 0)
		return -1;
	// ideally we would do this instead but clangd seems to have a problem with this:
	// it's keeping an open handle to main.c in ted. presumably blocks deletion but not writing.
	// ReplaceFileW has the same problem.
// 	if (CreateHardLinkW(wide_oldname, wide_newname, NULL) == 0)
// 		return -1;
	if (remove(oldname) != 0)
		return -1;
	return 0;
}

struct timespec time_last_modified(const char *path) {
	struct timespec ts = {0};
	FILETIME write_time = {0};
	WCHAR wide_path[4100];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, arr_count(wide_path)) == 0)
		return ts;
	HANDLE file = CreateFileW(wide_path, GENERIC_READ,
		FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return ts;
	
	if (GetFileTime(file, NULL, NULL, &write_time)) {
		u64 qword = (u64)write_time.dwLowDateTime
			| (u64)write_time.dwHighDateTime << 32;
		// annoyingly, windows gives time since jan 1, 1601 not 1970
		// https://www.wolframalpha.com/input?i=number+of+days+between+jan+1%2C+1601+and+jan+1%2C+1970
		qword -= (u64)10000000 * 134774 * 60 * 60 * 24;
		ts.tv_sec = qword / 10000000;
		ts.tv_nsec = (qword % 10000000) * 100;
	}
	
	CloseHandle(file);
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

struct Process {
	// NOTE: we do need to keep the ends of the pipes we aren't using open too
	HANDLE pipe_stdin_read, pipe_stdin_write,
		pipe_stdout_read, pipe_stdout_write,
		pipe_stderr_read, pipe_stderr_write;
	HANDLE job;
	PROCESS_INFORMATION process_info;
	char error[200];
};

static void get_last_error_str(char *out, size_t out_sz) {
	DWORD errnum = GetLastError();
	size_t size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errnum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), out, (DWORD)out_sz - 1, NULL);
	out[size] = 0;
	char *cr = strchr(out, '\r');
	if (cr) *cr = '\0'; // get rid of carriage return+newline at end of error
	str_printf(out + strlen(out), out_sz - strlen(out), " (error code %u)", (unsigned)errnum);
}

Process *process_run_ex(const char *command, const ProcessSettings *settings) {
	// thanks to https://devblogs.microsoft.com/oldnewthing/20131209-00/?p=2433 for the job code
	Process *process = calloc(1, sizeof *process);
	char *command_line = str_dup(command);

	// we need to create a "job" for this, because when you kill a process on windows,
	// all its children just keep going. so cmd.exe would die, but not the actual build process.
	// jobs fix this, apparently.
	HANDLE job = CreateJobObjectA(NULL, NULL);
	if (job) {
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = {0};
		job_info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(job, JobObjectExtendedLimitInformation, &job_info, sizeof job_info);
		HANDLE pipe_stdin_read = 0, pipe_stdin_write = 0, pipe_stdout_read = 0,
			pipe_stdout_write = 0, pipe_stderr_read = 0, pipe_stderr_write = 0;
		SECURITY_ATTRIBUTES security_attrs = {sizeof(SECURITY_ATTRIBUTES)};
		security_attrs.bInheritHandle = TRUE;
		bool created_pipes = true;
		created_pipes &= CreatePipe(&pipe_stdin_read, &pipe_stdin_write, &security_attrs, 0) != 0;
		created_pipes &= CreatePipe(&pipe_stdout_read, &pipe_stdout_write, &security_attrs, 0) != 0;
		if (settings->separate_stderr)
			created_pipes &= CreatePipe(&pipe_stderr_read, &pipe_stderr_write, &security_attrs, 0) != 0;
		
		if (created_pipes) {
			STARTUPINFOA startup = {sizeof(STARTUPINFOA)};
			startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
			startup.hStdOutput = pipe_stdout_write;
			startup.hStdError = settings->separate_stderr ? pipe_stderr_write : pipe_stdout_write;
			startup.hStdInput = pipe_stdin_read;
			startup.wShowWindow = SW_HIDE;
			PROCESS_INFORMATION *process_info = &process->process_info;
			if (CreateProcessA(NULL, command_line, NULL, NULL, TRUE, CREATE_NEW_CONSOLE | CREATE_SUSPENDED,
				NULL, settings->working_directory, &startup, process_info)) {
				// create a suspended process, add it to the job, then resume (unsuspend) the process
				if (AssignProcessToJobObject(job, process_info->hProcess)) {
					if (ResumeThread(process_info->hThread) != (DWORD)-1) {
						process->job = job;
						process->pipe_stdin_read   = pipe_stdin_read;
						process->pipe_stdin_write  = pipe_stdin_write;
						process->pipe_stdout_read  = pipe_stdout_read;
						process->pipe_stdout_write = pipe_stdout_write;
						process->pipe_stderr_read  = pipe_stderr_read;
						process->pipe_stderr_write = pipe_stderr_write;
					} else {
						strbuf_printf(process->error, "Couldn't start thread");
					}
				} else {
					strbuf_printf(process->error, "Couldn't assign process to job object.");
				}
				if (*process->error) {
					TerminateProcess(process_info->hProcess, 1);
					CloseHandle(process_info->hProcess);
					CloseHandle(process_info->hThread);
				}
			} else {
				char buf[150];
				get_last_error_str(buf, sizeof buf);
				strbuf_printf(process->error, "Couldn't run `%s`: %s", command, buf);
			}
			if (*process->error) {
				if (pipe_stdin_read)   CloseHandle(pipe_stdin_read);
				if (pipe_stdin_write)  CloseHandle(pipe_stdin_write);
				if (pipe_stdout_read)  CloseHandle(pipe_stdout_read);
				if (pipe_stdout_write) CloseHandle(pipe_stdout_write);
				if (pipe_stderr_read)  CloseHandle(pipe_stderr_read);
				if (pipe_stderr_write) CloseHandle(pipe_stderr_write);
			}
		} else {
			char buf[150];
			get_last_error_str(buf, sizeof buf);
			strbuf_printf(process->error, "Couldn't create pipe: %s", buf);
		}
		if (*process->error)
			CloseHandle(job);
	}
	free(command_line);
	return process;
}

int process_get_id(void) {
	return (int)GetCurrentProcessId();
}


Process *process_run(const char *command) {
	const ProcessSettings settings = {0};
	return process_run_ex(command, &settings);
}

const char *process_geterr(Process *p) {
	return *p->error ? p->error : NULL;
}

static long long process_read_handle(Process *process, HANDLE pipe, char *data, size_t size) {
	if (size > U32_MAX) {
		strbuf_printf(process->error, "Too much data to read.");
		return -2;
	}
	DWORD bytes_read = 0, bytes_avail = 0, bytes_left = 0;
	if (PeekNamedPipe(pipe, data, (DWORD)size, &bytes_read, &bytes_avail, &bytes_left)) {
		if (bytes_read == 0) {
			return -1;
		} else {
			ReadFile(pipe, data, (DWORD)size, &bytes_read, NULL); // make sure data is removed from pipe
			return bytes_read;
		}
	} else {
		char buf[150];
		get_last_error_str(buf, sizeof buf);
		strbuf_printf(process->error, "Couldn't read from pipe: %s", buf);
		return -2;
	}
}

long long process_read(Process *process, char *data, size_t size) {
	if (!process) {
		// already killed
		assert(0);
		return -2;
	}
	return process_read_handle(process, process->pipe_stdout_read, data, size);
}

long long process_read_stderr(Process *process, char *data, size_t size) {
	if (!process) {
		// already killed
		assert(0);
		return -2;
	}
	return process_read_handle(process, process->pipe_stderr_read, data, size);
}

long long process_write(Process *process, const char *data, size_t size) {
	if (!process) {
		// already killed
		assert(0);
		return -2;
	}
	
	if (size > LLONG_MAX) {
		strbuf_printf(process->error, "Too much data to read.");
		return -2;
	}
	size_t total_written = 0;
	DWORD written = 0;
	while (total_written < size) {
		bool success = WriteFile(process->pipe_stdin_write, data,
			size > U32_MAX ? U32_MAX : (DWORD)size,
			&written,
			NULL);
		if (!success) {
			char buf[150];
			get_last_error_str(buf, sizeof buf);
			strbuf_printf(process->error, "Couldn't write to pipe: %s", buf);
			return -2;
		}
		total_written += written;
	}
	return (long long)total_written;
}

void process_kill(Process **pprocess) {
	Process *process = *pprocess;
	if (!process) {
		// already killed
		return;
	}
	CloseHandle(process->job);
	CloseHandle(process->pipe_stdin_read);
	CloseHandle(process->pipe_stdin_write);
	CloseHandle(process->pipe_stdout_read);
	CloseHandle(process->pipe_stdout_write);
	if (process->pipe_stderr_read) CloseHandle(process->pipe_stderr_read);
	if (process->pipe_stderr_write) CloseHandle(process->pipe_stderr_write);
	CloseHandle(process->process_info.hProcess);
	CloseHandle(process->process_info.hThread);
	free(process);
	*pprocess = NULL;
}

int process_check_status(Process **pprocess, ProcessExitInfo *info) {
	Process *process = *pprocess;
	if (!process) {
		// already killed
		return -1;
	}
	HANDLE hProcess = process->process_info.hProcess;
	DWORD exit_code = 1;
	if (GetExitCodeProcess(hProcess, &exit_code)) {
		if (exit_code == STILL_ACTIVE) {
			return 0;
		} else {
			process_kill(pprocess);
			info->exited = true;
			info->exit_code = (int)exit_code;
			if (exit_code == 0) {
				strbuf_printf(info->message, "exited successfully");
				return +1;
			} else {
				strbuf_printf(info->message, "exited with code %d", (int)exit_code);
				return -1;
			}
		}
	} else {
		// something has gone wrong.
		strbuf_printf(info->message, "couldn't get process exit status");
		process_kill(pprocess);
		return -1;
	}
}
