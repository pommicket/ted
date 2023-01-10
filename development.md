(work in progress)

## Building

To build the debug version of `ted`, you will need ninja-build (package `ninja` on Debian/Ubuntu).
On Windows you don't need to install it since it comes with MSVC. Then run just `make` (or `make.bat`).

## Header files

TODO

As much as possible, OS-dependent functions should be put in `os.h/os-*.c`.
(But "pure" functions like `qsort_with_context`, which could
in theory be implemented on any platform with just plain C, should be put
in `util.c` even if they use OS-specific library functions.)

## Unicode

ted stores text as UTF-32. We assume that code points are characters.
This is not correct for combining diacritics and will hopefully be fixed at some point.

All paths are stored as UTF-8, and annoyingly have to be converted to UTF-16 for Windows
functions (why hasn't Windows made UTF-8 versions of all their API functions yet to save
everyone the trouble...)

## Drawing

## Build details

Currently, ted uses cmake for debug builds and plain old make (batch on windows) for
release builds. My hope is that `ted` will always be compilable with:
```
cc main.c
```
Of course this is not possible because ted uses libraries. But at least we have
```
cc main.c libpcre.a -lSDL2 -lm
```
or something.


I don't like complicated build systems, and I'm only using cmake because it can
(through ninja) output `compile_commands.json` which is used for clangd.

Both `make.bat` and `Makefile` will run all the right cmake and ninja commands
for you (including generating `compile_commands.json`),
so you shouldn't have to worry about that.

## Adding source files

When you add a source file to ted, make sure you:

1. `#include` it in main.c
2. Add it to the `SOURCES` variable in CMakeLists.txt

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
