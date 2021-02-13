#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

typedef enum {
	FS_NON_EXISTENT,
	FS_FILE,
	FS_DIRECTORY,
	FS_OTHER
} FsType;

enum {
	FS_PERMISSION_READ = 0x01,
	FS_PERMISSION_WRITE = 0x02,
};
typedef u8 FsPermission;

// returns what kind of thing this is.
FsType fs_path_type(char const *path);
FsPermission fs_path_permission(char const *path);
// Does this file exist? Returns false for directories.
bool fs_file_exists(char const *path);
// Returns a NULL-terminated array of the files/directories in this directory, or NULL if the directory does not exist.
// When you're done with the file names, call free on each one, then on the array.
// NOTE: The files aren't returned in any particular order!
char **fs_list_directory(char const *dirname);
// Create the directory specified by `path`
// Returns:
// 1  if the directory was created successfully
// 0  if the directory already exists
// -1 if the path already exists, but it's not a directory, or if there's another error (e.g. don't have permission to create directory).
int fs_mkdir(char const *path);
// Puts the current working directory into buf, including a null-terminator, writing at most buflen bytes.
// Returns:
// 1  if the working directory was inserted into buf successfully
// 0  if buf is too short to hold the cwd
// -1 if we can't get the cwd for whatever reason.
int fs_get_cwd(char *buf, size_t buflen);


#endif // FILESYSTEM_H_

