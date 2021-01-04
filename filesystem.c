#include <sys/types.h>
#include <sys/stat.h>
#if __unix__
#include <unistd.h>
#endif

static bool fs_file_exists(char const *path) {
#if _WIN32
	struct _stat statbuf = {0};
	if (_stat(path, &statbuf) != 0)
		return false;
	return statbuf.st_mode == _S_IFREG;
#else
	struct stat statbuf = {0};
	if (stat(path, &statbuf) != 0)
		return false;
	return S_ISREG(statbuf.st_mode);
#endif
}
