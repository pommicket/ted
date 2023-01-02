(work in progress)

As much as possible, OS-dependent functions should be put in `os.h/os-*.c`.
(But "pure" functions like `qsort_with_context`, which could
in theory be implemented on any platform with just plain C, should be put
in `util.c` even if they use OS-specific library functions.)

## Header files

## Drawing

## Adding settings

## Adding languages

## Releasing

When releasing a new version of `ted`:

- Update `TED_VERSION` in `ted.h`.
- Update `Version` in `control` for the `.deb` file.
- Run `make ted.deb` on Debian/Ubuntu.
- Run `make.bat release` on Windows.
- Open installer project, and increment version number.
- Build `ted.msi`.
- Create a new release on GitHub with `ted.deb` and `ted.msi`.
