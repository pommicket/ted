#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

// Does this file exist? Returns false for directories.
static bool fs_file_exists(char const *path) {
	struct stat statbuf = {0};
	if (stat(path, &statbuf) != 0)
		return false;
	return S_ISREG(statbuf.st_mode);
}

// Returns a NULL-terminated array of the files/directories in this directory, or NULL if the directory does not exist.
// When you're done with the file names, call free on each one, then on the array.
// NOTE: The files aren't returned in any particular order!
static char **fs_list_directory(char const *dirname) {
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

static int fs_mkdir(char const *path) {
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
