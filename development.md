(work in progress)

As much as possible, OS-dependent functions should be put in `os.h/os-*.c`.
(But "pure" functions like `qsort_with_context`, which could
in theory be implemented on any platform with just plain C, should be put
in `util.c` even if they use OS-specific library functions.)

## Header files

## Unicode

- UTF-32
- codepoints are characters
- stuff outside BMP on windows

## Drawing

## Adding settings

## Adding commands

## Adding languages

## Syntax highlighting

Obviously we don't want to re-highlight the whole file every time a change is made.
Ideally, we would just re-highlight the line that was edited, but that might
not work, because some syntax highlighting (e.g. multi-line comments) spans multiple lines.
So we keep track of a "syntax state" for the start of each line (whether we're in a multi-line comment or not,
whether we're in a multi-line string or not, etc.). A line's syntax highlighting can only change
if it is edited, or if its syntax state changes.

## Adding LSP features

## Releasing

When releasing a new version of `ted`:

- Update `TED_VERSION` in `ted.h`.
- Update `Version` in `control` for the `.deb` file.
- Run `make ted.deb` on Debian/Ubuntu.
- Run `make.bat release` on Windows.
- Open installer project, and increment version number.
- Build `ted.msi`.
- Create a new release on GitHub with `ted.deb` and `ted.msi`.
