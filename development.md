(work in progress)

## Building

To build the debug version of ted, run `make` (or `make.bat` on Windows).
By default we use the `ninja` build system (`sudo apt install ninja` or similar).

## Architecture

The big `Ted` struct has basically all of the data for ted.

In general, fixed-size types such as `u32` should be preferred to `unsigned int` & co.


## Files

Most function declarations should go in `ted.h`.
The exceptions are only for self-contained files, like `text.c`,
which gets its own header file `text.h`.

Everything includes `base.h`, which has useful type definitions and stuff.
This includes shorter names for fixed-size types (e.g. `i8` for `int8_t`).

`util.h`/`util.c` has "utility" functions which aren't specific to ted.

`os.h`/`os-*.c` has OS-dependent functions such as file system
stuff which is (annoyingly) lacking from standard C.

## Unicode

ted stores text as UTF-32. Left/right move by code points,
which isn't correct for e.g. combining diacritics. Hopefully this will
be fixed at some point.

All paths are stored as UTF-8, and annoyingly have to be converted to UTF-16 for Windows
functions (why hasn't Windows made UTF-8 versions of all their API functions yet to save
everyone the trouble...)

## Drawing

All drawing is done through either the `gl_geometry_*` functions or
the functions declared in `text.h`.
After using those functions, you need to call `gl_geometry_draw`
or `text_render` for it to actually be displayed.

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
output `compile_commands.json` which is needed for clangd.

Both `make.bat` and `make` will run all the right cmake and/or make and/or ninja commands
for you (including generating `compile_commands.json`),
so you shouldn't have to worry about that.

## Adding source files

When you add a source file to ted, make sure you:

1. `#include` it in main.c
2. Add it to the `SOURCES` variable in CMakeLists.txt

## Adding settings

Find the `Settings` struct in `ted.h` and add the new member.
Then go to `config.c` and edit the `settings_<type>` array.

## Adding commands

Go to `command.h` and add the command to the enum. Then
go to `command.c` and add the name of the command to the
`command_names` array,
and implement the command in the `command_execute` function.

## Adding (built-in) languages

Add a new member to the `Language` enum in `ted.h`.
Update `syntax_register_builtin_languages` accordingly.
Make sure to define `comment-start` and `comment-end`
for the language in `ted.cfg`.

You will probably want to update `keywords.py` when implementing
syntax highlighting.

### Syntax highlighting

Obviously we don't want to re-highlight the whole file every time a change is made.
Ideally, we would just re-highlight the line that was edited, but that might
not work, because some syntax highlighting (e.g. multi-line comments) spans multiple lines.
So we keep track of a "syntax state" for the start of each line (whether we're in a multi-line comment or not,
whether we're in a multi-line string or not, etc.). A line's syntax highlighting can only change
if it is edited, or if its syntax state changes.

At the top of `syntax.c` there are a bunch of `SYNTAX_STATE_*` constants.
Create a new enum for your language, and add any state that needs to be remembered across lines.
Then implement the `syntax_highlight_<language>` function similar to the other ones.

## Glossary

- **buffer** - a text document
- **column** - one space width
- **line buffer** - a single-line buffer, e.g. the file name input in the "open file" menu

## Releasing

When releasing a new version of `ted`:

- Update `TED_VERSION` in `ted.h`.
- Update `Version` in `control` for the `.deb` file.
- Run `make ted.deb` on Debian/Ubuntu.
- Run `make.bat release` on Windows.
- Run `python update-windows-installer.py`.
- Build `ted.msi`.
- Create a new release on GitHub with `ted.deb` and `ted.msi`.
