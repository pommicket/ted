#include "os.h"
#include <sys/types.h>
#include <sys/stat.h>
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

FsType fs_path_type(char const *path) {
	struct stat statbuf = {0};
	if (stat(path, &statbuf) != 0)
		return FS_NON_EXISTENT;
	return statbuf_path_type(&statbuf);
}

FsPermission fs_path_permission(char const *path) {
	FsPermission perm = 0;
	if (access(path, R_OK) == 0) perm |= FS_PERMISSION_READ;
	if (access(path, W_OK) == 0) perm |= FS_PERMISSION_WRITE;
	return perm;
}

bool fs_file_exists(char const *path) {
	return fs_path_type(path) == FS_FILE;
}

FsDirectoryEntry **fs_list_directory(char const *dirname) {
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
					char const *filename = ent->d_name;
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

int fs_mkdir(char const *path) {
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

struct timespec time_last_modified(char const *filename) {
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
