#include "filesystem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

FsType fs_path_type(char const *path) {
	struct stat statbuf = {0};
	if (stat(path, &statbuf) != 0)
		return FS_NON_EXISTENT;
	if (S_ISREG(statbuf.st_mode))
		return FS_FILE;
	if (S_ISDIR(statbuf.st_mode))
		return FS_DIRECTORY;
	return FS_OTHER;
}

FsPermission fs_path_permission(char const *path) {
	int bits = access(path, R_OK | W_OK);
	FsPermission perm = 0;
	if (!(bits & R_OK)) perm |= FS_PERMISSION_READ;
	if (!(bits & W_OK)) perm |= FS_PERMISSION_WRITE;
	return perm;
}

bool fs_file_exists(char const *path) {
	return fs_path_type(path) == FS_FILE;
}

char **fs_list_directory(char const *dirname) {
	char **ret = NULL;
	DIR *dir = opendir(dirname);
	if (dir) {
		struct dirent *ent;
		char **filenames = NULL;
		size_t nentries = 0;
		size_t filename_idx = 0;

		while (readdir(dir)) ++nentries;
		rewinddir(dir);
		filenames = (char **)calloc(nentries+1, sizeof *filenames);

		while ((ent = readdir(dir))) {
			char const *filename = ent->d_name;
			size_t len = strlen(filename);
			char *filename_copy = (char *)malloc(len+1);
			if (!filename_copy) break;
			strcpy(filename_copy, filename);
			if (filename_idx < nentries) // this could actually fail if someone creates files between calculating nentries and here. 
				filenames[filename_idx++] = filename_copy;
		}
		ret = filenames;
		closedir(dir);
	}
	return ret;
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
