#include "filesystem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

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
