#include "os.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

static FsType statbuf_path_type(const struct stat *statbuf) {
	if (S_ISREG(statbuf->st_mode))
		return FS_FILE;
	if (S_ISDIR(statbuf->st_mode))
		return FS_DIRECTORY;
	return FS_OTHER;	
}

FsType fs_path_type(const char *path) {
	struct stat statbuf = {0};
	if (stat(path, &statbuf) != 0)
		return FS_NON_EXISTENT;
	return statbuf_path_type(&statbuf);
}

FsPermission fs_path_permission(const char *path) {
	FsPermission perm = 0;
	if (access(path, R_OK) == 0) perm |= FS_PERMISSION_READ;
	if (access(path, W_OK) == 0) perm |= FS_PERMISSION_WRITE;
	return perm;
}

bool fs_file_exists(const char *path) {
	return fs_path_type(path) == FS_FILE;
}

FsDirectoryEntry **fs_list_directory(const char *dirname) {
	FsDirectoryEntry **entries = NULL;
	DIR *dir = opendir(dirname);
	if (dir) {
		struct dirent *ent;
		size_t nentries = 0;
		int fd = dirfd(dir);
		if (fd != -1) {
			while (readdir(dir)) ++nentries;
			rewinddir(dir);
			entries = (FsDirectoryEntry **)calloc(nentries+1, sizeof *entries);
			if (entries) {
				size_t idx = 0;
				while ((ent = readdir(dir))) {
					const char *filename = ent->d_name;
					size_t len = strlen(filename);
					FsDirectoryEntry *entry = (FsDirectoryEntry *)calloc(1, sizeof *entry + len + 1);
					if (!entry) break;
					memcpy(entry->name, filename, len);
					switch (ent->d_type) {
					case DT_REG:
						entry->type = FS_FILE;
						break;
					case DT_DIR:
						entry->type = FS_DIRECTORY;
						break;
					case DT_LNK: // we need to dereference the link
					case DT_UNKNOWN: { // information not available directly from dirent, we need to get it ourselves
						struct stat statbuf = {0};
						fstatat(fd, filename, &statbuf, 0);
						entry->type = statbuf_path_type(&statbuf);
					} break;
					default:
						entry->type = FS_OTHER;
					}
					if (idx < nentries) // this could actually fail if someone creates files between calculating nentries and here. 
						entries[idx++] = entry;
				}
			}
		}
		closedir(dir);
	}
	return entries;
}

int fs_mkdir(const char *path) {
	if (mkdir(path, 0755) == 0) {
		// directory created successfully 
		return 1;
	} else if (errno == EEXIST) {
		struct stat statbuf = {0};
		if (stat(path, &statbuf) == 0) {
			if (S_ISDIR(statbuf.st_mode)) {
				// already exists, and it's a directory 
				return 0;
			} else {
				// already exists, but not a directory 
				return -1;
			}
		} else {
			return -1;
		}
	} else {
		return -1;
	}
}

int fs_get_cwd(char *buf, size_t buflen) {
	assert(buf && buflen);
	if (getcwd(buf, buflen)) {
		return 1;
	} else if (errno == ERANGE) {
		return 0;
	} else {
		return -1;
	}
}

struct timespec time_last_modified(const char *filename) {
	struct stat statbuf = {0};
	stat(filename, &statbuf);
	return statbuf.st_mtim;
}

struct timespec time_get(void) {
	struct timespec ts = {0};
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts;
}

void time_sleep_ns(u64 ns) {
	struct timespec rem = {0}, req = {
		(time_t)(ns / 1000000000),
		(long)(ns % 1000000000)
	};
	
	while (nanosleep(&req, &rem) == EINTR) // sleep interrupted by signal
		req = rem;
}


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

bool process_run(Process *proc, const char *command) {
	const ProcessSettings settings = {0};
	return process_run_ex(proc, command, &settings);
}


const char *process_geterr(Process *p) {
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