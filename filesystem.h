#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

typedef enum {
	FS_NON_EXISTENT,
	FS_FILE,
	FS_DIRECTORY,
	FS_OTHER
} FsType;

// returns what kind of thing this is.
FsType fs_path_type(char const *path);
// Does this file exist? Returns false for directories.
bool fs_file_exists(char const *path);
// Returns a NULL-terminated array of the files/directories in this directory, or NULL if the directory does not exist.
// When you're done with the file names, call free on each one, then on the array.
// NOTE: The files aren't returned in any particular order!
char **fs_list_directory(char const *dirname);
// create the directory specified by `path`
// returns:
// 1  if the directory was created successfully
// 0  if the directory already exists
// -1 if the path already exists, but it's not a directory, or if there's another error (e.g. don't have permission to create directory).
int fs_mkdir(char const *path);

#endif // FILESYSTEM_H_

