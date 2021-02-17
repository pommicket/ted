#include "filesystem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

FsType fs_path_type(char const *path) {
	struct _stat statbuf = {0};
	if (_stat(path, &statbuf) != 0)
		return FS_NON_EXISTENT;
	if (statbuf.st_mode & _S_IFREG)
		return FS_FILE;
	if (statbuf.st_mode & _S_IFDIR)
		return FS_DIRECTORY;
	return FS_OTHER;
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

char **fs_list_directory(char const *dirname) {
	char file_pattern[256] = {0};
	char **ret = NULL;
	WIN32_FIND_DATA find_data;
	HANDLE fhandle;
	assert(*dirname);
	sprintf_s(file_pattern, sizeof file_pattern, "%s%s*", dirname,
		dirname[strlen(dirname) - 1] == PATH_SEPARATOR ? "" : PATH_SEPARATOR_STR);
	fhandle = FindFirstFileA(file_pattern, &find_data);
	if (fhandle != INVALID_HANDLE_VALUE) {
		// first, figure out number of files
		int nfiles = 1, idx = 0;
		char **files;
		while (FindNextFile(fhandle, &find_data))  {
			++nfiles;
		}
		FindClose(fhandle);
		// now, fill out files array
		files = malloc((nfiles + 1) * sizeof *files);
		if (files) {
			fhandle = FindFirstFileA(file_pattern, &find_data);
			if (fhandle != INVALID_HANDLE_VALUE) {
				do {
					if (idx < nfiles) {
						char *dup = _strdup(find_data.cFileName); 
						if (dup) {
							files[idx++] = dup;
						} else break; // stop now
					}
				} while (FindNextFile(fhandle, &find_data));
				files[idx] = NULL;
				FindClose(fhandle);
				ret = files;
			}
		}
	}
	return ret;
}

int fs_mkdir(char const *path) {
	if (CreateDirectoryA(path, NULL)) {
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
	DWORD pathlen = GetCurrentDirectory((DWORD)buflen, buf);
	if (pathlen == 0) {
		return -1;
	} else if (pathlen < buflen) { // it's confusing, but this is < and not <=
		return 1;
	} else {
		return 0;
	}
}
