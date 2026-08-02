## 3.2.5 — 2026 Aug 1

- Fix IME composition underline color being syntax highlighting for end of line, instead of cursor position.
- Fix searching in “Go to definition…” menu being broken for ctags.
- Make `text-size` update automatically when config is changed.
- Fix blurry rendering on Linux with high DPI displays due to SDL3 migration.
- Scale `text-size` by DPI on Linux — you might need to adjust your `text-size` for high DPI displays.
  It was already scaled on Windows. I never bothered doing it for Linux because it wasn't easy to
  look up the DPI scale with SDL2, but now with SDL3 it is :)
- ted website is now installed to `/usr/share/doc/ted` on Linux and `C:\Program Files (x86)\ted\doc` on Windows.
- You can now open the [guide](guide.html) with `:open-guide`.

## 3.2.4 — 2026 Jul 26

Fix composition being shown in non-active buffers.

## 3.2.3 — 2026 Jul 20

3rd bugfix release in one day… such are the joys of upgrading to a new major library version

- fixed hover/highlight-key being broken
- fixed autocomplete-related crash
- fixed release build being broken on windows. so the MSI installers no longer accidentally use

## 3.2.2 — 2026 Jul 20

- fixed an issue with shift/ctrl+click no longer registering the modifier keys

## 3.2.1 — 2026 Jul 20

- fixed a crash on some systems when closing ted

interestingly, this bug existed before, but it seems that it was only activated by the switch to SDL3

## 3.2.0 — 2026 Jul 19

there were a number of changes needed to move to SDL3, so there may be bugs.

- upgraded to SDL3
- added support for input using IME (input method editor) software with ted
- `ime-underline-thickness` and `ime-selection-underline-thickness` control the thickness of the underline for the composition and the selected clause of the composition respectively (not all IMEs have the concept of “selected clauses”)
- fixed a small bug with `.cfg` highlighting, where the character after a number was highlighted wrong

## 3.1.9 — 2026 Jun 9

- fixed an annoyance with how home/end keys now work with selector. specifically, instead of home/end just selecting the first/last option, they now move the cursor to the start/end of the line buffer, but if the cursor is already at the start/end, the first/last option is selected as before.

## 3.1.8 — 2026 May 27

a little release which will hopefully fix the problem of "updating a header file but the source files still show outdated errors for a little while"

- added LSP `didSave` notifications

## 3.1.7 — 2026 May 10

a couple of niceties:
- added the `:copy-ref` command, which copies `<file path>:<line>` to the clipboard (useful for gdb, etc.)
- filefinder search can now have multiple space-separated terms

## 3.1.6 — 2026 Apr 22

a couple of little niceties in this update:
- added `follow-symlinks` option which resolves symlinks in all opened paths (on by default). the file browser already followed symlinks for directories, but now it does so for files as well, and symlinks are now resolved in paths opened by other means (command line argument, go-to-definition, etc.). this doesn't work on windows because I couldn't be bothered and symlinks are rare there anyways.
- `:open` command now accepts a string argument for the path. so you can make a keyboard shortcut for opening your favorite file :)

## 3.1.5 — 2026 Apr 18

just another bugfix update
- Fixed crashes involving moving tabs around with the mouse
- Fixed active tab being wrong when moving tabs around with the mouse

## 3.1.4 — 2026 Apr 7

Fix split view crash

## 3.1.3 — 2026 Apr 5

some improvements to the file finder:
- pressing escape while the index process is running now cancels it
- searching for files is now much faster (for large projects)

## 3.1.2 — 2026 Apr 1

I did promise there would be bugs in the file finder!
- Fix file finder going to wrong place/crashing if you select an entry while it's filtering for a new search term

## 3.1.1 — 2026 Mar 25

just a few small things:
- allow searching full path rather than just file name in filefinder (but only if search term contains a path separator)
- filefinder filtering should hopefully be a little faster for large projects now (I'm hoping to improve it more in the future)
- fix `TED_LINE/COLUMN1` environment vars being set to 1 instead of the cursor line/column if there is no selection
- fix tiny memory leak
- fix filefinder not showing directory names on windows if command outputs `/`-delimited paths

## 3.1.0 — 2026 Mar 24

this release has a feature I'm really happy about: a project-wide file searcher (for people who aren't crazy enough to put all the files in a single directory like me :P). default key binding is <kbd>ctrl+shift+O</kbd> (`:filefinder-open`), and by default it uses `git ls-files` to find the files, but you can configure that with the `filefinder-command` setting. you can also use <kbd>ctrl+shift+I</kbd> (`:filefinder-reindex`) to re-generate the list of files if you add or remove any. right now the file indexes aren't ever deleted, so they could in theory clog up memory if you have a ton of large problems (seems unlikely, but who knows). if that happens or if you run into other weird problems (please report them!), you can run `:filefinder-reset` to clear all the indexes. this feature is definitely still in the "experimental" phase, so don't expect it to be flawless/bugless! also,
- `:shell` commands now get the `TED_LINE1/2` and `TED_COLUMN1/2` environment variables for getting the start/end position of the current selection, as well as `TED_SELECTION` which has the selected text

## 3.0.3 — 2026 Feb 24

- Fixed a bug where file selector entries were only being sorted the first time the file selector was opened (oops!)

## 3.0.2 — 2026 Feb 22

- improved sorting of selector entries (now ted prefers exact matches most, then case insensitive matches, then exact extensions, then case insensitive extensions, then sorts alphabetically)
- command palette is now sorted alphabetically instead of arbitrarily

## 3.0.1 — 2026 Feb 19

Just fixing a single bug because it's quite annoying. Oddly enough it has nothing to do with all of yesterday's changes.
- Fixed detection of end of C++ raw string (I must have been very tired when I wrote that code because it made no sense…)

## 3.0.0 — 2026 Feb 18

perhaps this release doesn't justify a major version increase, but `2.10.0` is ugly, so why not?

- inline `<style>` and `<script>` element contents are now highlighted as CSS/JS. doesn't work if they have attributes (e.g. `<script defer>`) yet (maybe I'll fix this eventually)
- environment variables `TED_FILE`, `TED_LINE`, and `TED_COLUMN` are now passed to build/`:shell` commands. this lets you do cool things like
```
Ctrl+b = `git blame "$TED_FILE" -L $TED_LINE,$TED_LINE` :shell
```
to get a git-blame for the line the cursor is on using the shortcut <kbd>Ctrl+b</kbd>.
- fix issue where wrapped text in error messages was missing a character at the wrap
- removed path length limits (previously ted didn't handle files whose paths are longer than 1024 bytes)

a lot of code has changed, so there will likely be bugs/crashes! please report them by github issue or e-mail :)

## 2.9.1 — 2025 Oct 24

Some small fixes to syntax highlighting:
- `/*/ comment */` is now highlighted correctly in relevant languages
- multi-line comments containing `//` are no longer mis-highlighted in JavaScript
- multi-line things are now correctly highlighted after manual `:set-language`

## 2.9.0 — 2025 Sep 30

ted now has support for LSP "code actions" (things like quick-fixing errors, extracting functions, etc.) — default key combination is <kbd>Alt</kbd>+<kbd>Space</kbd>. Some actions that appear in other editors might be missing in ted because the spec is deliberately unclear on how to implement them properly.* Also some bug fixes/small improvements:

- Document link requests are no longer being spammed 60 times a second at the LSP server when you hold down the activation key
- Document links are now underlined when you hover over them while holding the activation key
- Fixed issue with inotify when multiple events happen in a single frame (probably wasn't causing any problems)
- Fixed a small memory leak
- Fixed issue with parsing of `\u` sequences in LSP responses that could have led to some weird behaviour

*Their idea, I guess, is that if you maintain a language server, you must also maintain all the plug-ins for every single editor

## 2.8.4 — 2025 Sep 28

Keep cursor pos on reload, other small improvements
Some small improvements:

- Cursor position is no longer reset when the file is changed externally
- Added syntax highlighting for the upcoming Python 3.14 template strings (`t"foo"`)
- Fix syntax highlighting for HTML tag names containing `-` (and also some more unusual but legal characters)
- Better highlighting for Markdown links. Now `<https://example.com>` and `[a](https://example.com?q=(a))` are highlighted correctly.
- Fixed `bg-shader` setting not being applied
- Removed `bg-texture` setting (obscure feature, required lots o code to support it).

## 2.8.3 — 2025 Sep 1

Relatively small release with a few improvements:
- Fix odd auto-indent behaviour when there is white space to the right of the cursor (now only the white space *before* the cursor copied to the next line).
- Update to PCRE2-10.46 (fixes a security bug, but probably no one is pasting random untrusted regexes into a text editor anyways…)
- Recover memory when lines are removed/shortened in a buffer.
- Other minor performance optimizations.

## 2.8.2 — 2025 Jun 27

A bugfix release
- fix multi-line syntax highlighting being broken after file reload (introduced by 2.8.1)
- fix ted's permissions from being messed up if installing via the .deb installer (turns out dpkg can install files with non-root owners?? and this is done by default if you use dpkg-deb as a non-root user?? who knew!!)

## 2.8.1 — 2025 Jun 16

- This release fixes a long-standing bug where very rarely file contents would get screwed up after an automatic reload. I'm still not sure how that could happen- seems like it must be that the file is edited without its modification time getting updated? Now we use inotify on Linux (in addition to mtime) to fix this. I haven't had this issue on Windows, so this is a Linux-only fix, but it should be possible to use `FindFirstChangeNotificationW` on Windows if needed.
- Reloading file from disk no longer clears undo history (you can now undo the reload).

## 2.8.0 — 2025 Jun 12

A relatively small update:

- Syntax highlighting for the Microsoft's C♯ language. Please open an issue if you find any nontrivial errors in the highlighting.
- Syntax highlighting for string interpolation in Python, JavaScript/TypeScript, and C# — string literals and braces inside of interpolations are not highlighted correctly (e.g. `f"my string is {'a'+'b'} and my set is { {1,2} - {3,4} }"`), but that's pretty rare anyways.

## 2.7.8 — 2025 Mar 23

This release fixes a bug where ted would occasionally crash when closing a tab (due to a stale settings pointer).

## 2.7.7 — 2025 Mar 5

Some small things:

- fixed IDE hover info getting screwed up when you are on not on the first tab in a node
- add support for LSP `textDocument/prepareRename` request. (this means that the region highlighted when you rename something will be more accurate, and if your cursor is not on a valid rename target, you'll get an error immediately)

## 2.7.6 — 2024 Dec 8

- Fix a new LSP bug introduced by 2.7.5 (accidentally disabled pretty much all LSP behaviour (i should not try to release a new update late at night…))

## 2.7.5 — 2024 Dec 7

- Properly handle LSP error responses for the most part, e.g. if an error occurs during a rename operation, the rename is cancelled and the cursor flashes red (previously would keep "loading" until you hit escape)
- Add address of main to Linux crash dumps for easier debugging w/ ASLR

## 2.7.4 — 2024 Sep 8

it's another bugfix release

- fixed formatting of numbers in LSP json (they were only being formatted with up to 6 decimals … ted PIDs above 999,999 were being written in scientific notation causing rust-analyzer to crash)
- fixed find+replace handling of when the replacement matches the find pattern. so now e.g. you can replace `unsigned` with `unsigned long` and it'll work as expected

## 2.7.3 — 2024 Sep 8

- you can now configure ted's data directories when building it! this lets you install ted without root access, as documented in the README
- indentation can now be set manually for buffers through the command palette via the `:indent-with-spaces`, `:indent-with-tabs`, and `:set-tab-width` commands (these can also be used in ted.cfg to configure indentation via keyboard shortcuts). if `:indent-with-spaces/tabs` is given an argument > 1, it will also set the tab width to that
- fixed supplying arguments in command palette. you can also supply string arguments, although they have to be non-numbers (e.g. you can't insert the text "53" via the command palette with `:insert-text`)

## 2.7.2 — 2024 Jul 17

Very minor release:
- added `sync` option with values `none`, `data`, `full` to control how much data ted syncs to disk when you save a file
- fixed highlighting of `'\u{1234}'` in Rust
- fixed error when opening file while build output is active

## 2.7.1 — 2024 Feb 13

just a minor release!

- handle longer lines in .editorconfig files. i didn't know that some projects use editorconfig values which aren't in the spec! and so i assumed lines would be relatively short because everything in the spec was . how foolish .
- ted backup files (which only exist temporarily upon saving to prevent loss of data) are now named `file~` instead of `file.ted-bk` to be nicer to gitignores, etc.
- backup files are now created with mode 600 on unix , to prevent leaking file contents to other users
- ted now does fdatasync/_commit after writing to ensure that even if power is lost mid-save, no data can ever be lost (at least if it is, it won't be ted's fault…)
- indentation can now be auto-detected from file contents. hopefully the detection isnt wrong too often…

## 2.7.0 — 2023 Oct 19

overhauled a lot of ted's config code.
so now ted has [editorconfig](https://editorconfig.org) support, and support for local `.ted.cfg` files.
also, path-specific settings now use regular expressions which can be nice, but might break your config if any paths mentioned in it contain any of the characters `\^.$|()[]*+?{}-` (these need to be escaped with a backslash now)

minor features:
- `remove-trailing-whitespace` option
- `crlf` option not just on windows
- no more string length limits in ted.cfg files

bug fixes:
- signature help no longer blocks cursor when cursor is near bottom of buffer
- ted now switches back to the file you started in after performing an LSP rename operation

there may be new bugs!! please report them if you  find any.

## 2.6.2 — 2023 Sep 24

small improvements:
- cursor no longer moves on dedent (see #1)
- escaped quotes now work correctly in ted.cfg strings (oops)
- log file is no longer truncated on startup (instead, once it reaches 500KB it gets renamed to log.1.txt, overwriting previous log.1.txt)
- log file now shows PID, timestamp on every line
- LSP error responses are no longer displayed (they are still logged though) — this was a bit spammy with clangd
- LSP status and whether tabs/spaces are being used is now shown in title bar

## 2.6.1 — 2023 Sep 14

as promised, there were some bugs in the last release!
so here are some bugfixes:
- diagnostics now go away immediately if the LSP server crashes / is disabled
- diagnostic highlights are now properly confined to the buffer
- fix use-after-free which broke synchronization with LSP server (caused some "invalid offset" errors with rust-analyzer for example)
- fix tiny memory leak related to phantom completions

performance improvement:
- ted now does one `write()` call per batch of LSP messages rather than one per message

## 2.6 — 2023 Sep 10

new features:
- files with the same name are now disambiguated in the tab bar (so if you have files `/something/foo/a.txt` and `/other/bar/a.txt` they get displayed as `foo/a.txt` and `bar/a.txt`)
- we now show diagnostics (errors and warnings) from the LSP server (you can disable this by setting `show-diagnostics = no`)
- we now support LSP over TCP in addition to stdio (enable this with  the `lsp-port` setting)
- syntax highlighting for GDScript
- LSP code formatting with the `:format-file` and `:format-selection` commands (formats code similar to `rustfmt` or `clang-tidy`)
- `lsp-delay` setting to avoid overwhelming LSP servers

bug fixes:
- doing <kbd>Shift+Up</kbd>, <kbd>Shift+End</kbd>, <kbd>Backspace</kbd> at the end of a file used to act weird, but it doesn't anymore
- selector detail (e.g. locations in "go to definition..." menu) is no longer cut off by 2 pixels
- exact matches are always put first in the file selector menu
- renames where the symbol in question appeared multiple times on the same line and its length changed were kinda screwed up but that's fixed now
- LSP servers which only support "full sync" can now be used

this is a pretty big release, so again there may be bugs!

## 2.5.1 — 2023 Aug 26

fixing some bugs:

- selector menus (e.g. "open file" menu) no longer put the top line of text in a weird place when you scroll
- fix uninitialized read in build.c (that's been around for a long time... not sure if it caused any weirdness though)
- fix occasional crash when you type in a buffer that's being searched

## 2.5 — 2023 Aug 15

new features:

- LSP symbol rename: press ctrl+r (command `:rename-symbol`) while your cursor is on an identifier to rename it
- LSP document link: ctrl+clicking on “links” will now open them. what exactly counts as a “link” is up to your LSP server.

small things:

- you can now use home/end in selectors (e.g. “open menu” file selector)
- the default `:save-all` shortcut is now Ctrl+Alt+s rather than Ctrl+Shift+Alt+s (so it's a bit easier to press)
- ted will no longer open files with overlong UTF-8 (which is bad UTF-8) or null characters (which might've caused weird behavior)
- scroll to cursor on undo/redo
- really long paths no longer overflow the “open file”/“save as” menu :)
- “robust” find — find results no longer move around when you type stuff

also a lot of code has been changed behind the scenes:

```sh
$ git diff --stat 2.4.3 2.5 | tail -n1
 55 files changed, 5003 insertions(+), 3231 deletions(-)
```

mostly cleaning up and preparing for plug-in support (not quite ready yet...)

so there might be some new bugs........

## 2.4.3 — 2023 Aug 1

a boring bugfix release:
- fixed inconsistencies between cursor position and text (most noticeable when a tab appears mid-line)
- version number and help message are no longer displayed over the build window when no buffers are open

## 2.4.2 — 2023 Jul 21

small fix: you can now use absolute paths in the `font` option. i thought that was possible before but it wasn't.

## 2.4.1 — 2023 Jul 20

some fixes:
- the installers in the last release didn't include the changes from 2.3.4 oops!
- JSX self-closing tags (e.g. `<Foo />`) are no longer highlighted as regex literals
- the windows version of ted is now DPI-aware. this means that it will no longer look blurry if you have your UI scale set to something other than 100%

## 2.4 — 2023 Jul 19

over the past few days i've completely changed how ted renders text.
- variable-width fonts are now supported
- there is now a `font` option which allows setting multiple fonts to use as fallbacks when a character is not available. (this is especially useful if you commonly use a non-Latin script which isn't supported by the base noto sans mono font, you can now add a font which supports it)
- ted now includes noto emoji by default, so emoji will be properly rendered in code 🙂 (unfortunately the stb_truetype library doesn't support color, so they're just drawn in black-and-white)

a lot of code has been changed, so i wouldn't be surprised if there are bugs. if you do find any, please create an issue. :3

## 2.3.4 — 2023 Jul 14

- fix bug where unicode characters between U+8000 and U+FFFF were being written with overlong UTF-8 (oops!)
- add `:copy-path` command to copy the path to the current file to the clipboard (by default, no keyboard shortcut is set for this)

## 2.3.3 — 2023 Jul 6

Small improvements:

- TODOs are now highlighted in single-line comments as well
- more javascript syntax highlighting (e.g. `of`, `document`, `console`)
- add .jsx, .ejs, .tsx to javascript/typescript extensions by default (you will need to remove the `JavaScript`/`TypeScript` settings in the `[extensions]` section of your config to update this)

## 2.3.2 — 2023 Jun 17

some smallish bug fixes

- fixed problem where highlight matching brackets would highlight eof for unmatched brackets
- prevent /= from being parsed as a regex literal in javascript
- ignore duplicate completions for phantom suggestions

## 2.3.1 — 2023 May 22

fixed a very oops bug i just noticed recently:

- javascript division is no longer highlighted as regex literal

and also some minor features:

- better undo chaining (ctrl+z undoes a more "intuitive" amount of stuff now)
- `TODO`, `FIXME`, etc. are now highlighted in comments (this can be disabled by setting the `todo` color to the same as `comment`)

## 2.3 — 2023 May 11

ted now has a `:matching-bracket` command for going to the matching bracket. that's nice.

and some relatively small improvements:

- Better syntax highlighting for number literals (e.g. `1isize` is now correctly highlighted in Rust, `'` separators in C/C++, hex float literals are highlighted correctly).
- `jump-to-build-error` setting
- `crlf-windows` setting
- fixed the way backups are created so that saving a hard-linked file keeps the linkedness (also added a `save-backup` setting to control whether or not backups are created at all)
- fixed matching bracket highlighting which was broken in certain cases

## 2.2r1 — 2023 Mar 27

Small bug fix:

- `:increment-number`/`:decrement-number`/`:insert-text` no longer crash ted when no buffer is active

## 2.2 — 2023 Mar 23

for a while now, i've had to switch back to vim every once in a while because of its very good macro support. well now ted has macros!
other than that, in this release:

- typing text now scrolls to the cursor (i think this was broken in a recent release but it's fixed now)
- glsl is highlighted correctly again (it was being highlighted as html due to a typo)
- `:increment-number`/`:decrement-number` (default: <kbd>Ctrl+0</kbd> / <kbd>Ctrl+9</kbd>) to change numbers — useful with macros!
- `:previous-position` no longer crashes when no buffer is active
- jumping-to-build-errors (<kbd>Ctrl+[</kbd> and <kbd>Ctrl+]</kbd>) now includes rust cross-file references (e.g. `::: somefile.rs:5:3`)
- arguments to commands can now be bigger than 2<sup>62</sup> — you can go all the way from −2<sup>63</sup> to 2<sup>63</sup> now! i dont know why you would, but you can!

## 2.1 — 2023 Mar 7

this release has a bunch of "things i;ve been meaning to get around to"
- themes
- better interaction between path-specific and language-specific settings
- better handling of backspace/delete with `indent-with-spaces = yes`
- CSS syntax highlighting
- `:previous-position` command (default key binding: <kbd>ctrl</kbd>+<kbd>p</kbd>) to return to the previous cursor position
- <kbd>ctrl</kbd>+scroll to adjust text size
- you can now control how <kbd>ctrl</kbd>+<kbd>/</kbd> makes comments by setting `comment-start` and `comment-end` in `ted.cfg`
- fixed `ted.cfg` syntax highlighting for multiline `"` strings
- ted config files can now %include other files
- fix bug (only applicable to linux) where file permissions were reset on save (e.g. saving a file with execute permission would make it no longer executable)
- keys used to show LSP highlight/hover information is now configurable
- different colors for "read" and "write" LSP highlights
- if `regenerate-tags-if-not-found = yes`, tags are now also regenerated when there are no completion results

## 2.0 — 2023 Jan 11

this is the biggest ted release since 1.0!
here's what's new in ted v. 2.0:

- it took over a month but ted now has support for LSP servers! (autocomplete, go-to-definition/declaration/type-definition, signature help, hover, highlight, references)
- autocomplete for tags is also better now! it looks nicer too!
- "phantom" completions
- go-to-error will now strip `../` from file names in build errors if it can't find the file (e.g. https://gitlab.kitware.com/cmake/cmake/-/issues/13894 ha! this problem was easy to fix for me!! i only needed to make a whole text editor...)
- syntax highlighting for GLSL, XML, JSON, and TypeScript — if you are upgrading from an earlier version of ted, you will probably need to remove the `[extensions]` section from your local `ted.cfg` or copy it from the global `/usr/share/ted/ted.cfg` to get GLSL/XML highlighting to work since your `[extensions]` section probably includes `C = ..., .glsl, ...` which overrides the `GLSL = .glsl` setting (i now see that it was a mistake to do that)
- `framerate-cap` setting (you can now run ted at >60FPS if you want)
- `build-command` setting for overriding ted's inferred build command if you have Cargo.toml/Makefile/etc.
- `:go-to-definition-at-cursor` command
- `:up-blank-line` and `:down-blank-line` commands for moving up/down to previous/next blank line. these are now the default for Ctrl+up/Ctrl+down but you will need to remove the `Ctrl+Up = 10 :up` etc. from your local ted.cfg if you are upgrading from an older version of ted
- ted now fixes problems with orphaned nodes (buffers which are open but you can't see them). i'm still not sure what was causing that problem and i can't reproduce it but it should at least be less annoying now if it does happen.
- switched from scancodes to keycodes for keyboard commands (i realized that almost all applications use keycodes). **if you are using a non-QWERTY keyboard layout** you might have to rejig your keyboard shortcuts. i'm sorry i should have used keycodes from the beginning.
- tags files with escaped slashes are now handled correctly (this used to cause some "tag not found" errors)
- if the command-line argument to ted doesn't exist, it now creates a buffer with that path instead of an untitled buffer
- matching < and > are now highlighted in HTML and XML
- ted.cfg strings can now be delimited with \`
- fixed highlighting of # in multiline strings in ted.cfg
- ted now first writes to `<path>.ted-tmp` then renames to `<path>` when saving files so that you don't lose all the file contents if the power goes out mid-write or something
- build commands with non-ASCII characters should now be supported on windows
- there is now a "Text" language so you can write settings which should be applied when no programming language is active

## 1.3r2 — 2022 Dec 7

- turns out `glFinish()` busy loops on some devices. that's gone now. i dont even remember why i called it.
- switched to `//` for C comments. if you are a c89 evangelist or something, too bad.

## 1.3r1 — 2022 Nov 4

- fixed rust/python syntax highlighting of certain keywords

## 1.3 — 2022 Nov 3

a bunch of cool stuff in this release!

new features:
- path-specific settings   e.g.  `[/my/special/project//core]`  or `[C:\Users\Me\Documents//Javascript.keyboard]`
- custom background shader!! see ted.cfg for an example
- `:insert-text` command, multi-line strings in ted.cfg

small fixes:
- fixed rust char literal highlighting
- byte literals + byte string literals in rust have better highlighting
- in rust, <number>.into()  no longer highlights the "." as a number
- byte/raw/f strings in python have better highlighting

## 1.2r2 — 2022 Sep 30

new improvements and bugfixes:

- fixed go-to-error when running build command not in current directory
- shift+pageup/pagedown (`:select-page-up`, `:select-page-down`)
- .rs, .go files are now scanned for ctags
- rust raw identifiers (e.g. `r#if`) are now highlighted correctly
- rust attributes (e.g. `#[repr(C)]`) are now highlighted

## 1.2r1 — 2022 Aug 19

- Fixed a very major bug: ted no longer crashes shortly after pressing the mouse X1/X2 buttons.
- Also, you can now create commands for mouse X1/X2.
- The event queue is now cleared at startup because ted was getting events meant for the WM on my computer.

## 1.2 — 2022 Jul 29

Lots of bug fixes including:
- non-ASCII paths now work on Windows
- ctrl+q no longer behaves weirdly if pressed twice when there are unsaved changes
- ctrl+/ (comment selection) now works in Go
- extremely long lines and lines containing certain unicode characters (e.g. zero-width space) don't mess up the cursor position anymore
- javascript regex literals are now highlighted
- other small syntax highlighting fixes

Also new features:
- you can now set settings for individual languages, e.g. `[HTML.core]`, `[Java.keyboard]`
- `indent-with-spaces` option
- config is now automatically reloaded when saved

## 1.1 — 2022 Jul 22

- syntax highlighting for JavaScript, Java, and Go
- :goto-line (ctrl+g) now clamps line numbers <1 and >nlines instead of rejecting them
- fixed syntax highlighting right after :save-as
- other minor bugfixes

## 1.0r3 — 2022 Jul 7

Small improvements:

- underscore no longer treated as tex identifier character
- backspace/delete now scrolls to cursor

## 1.0r2 — 2022 Mar 26

Fixed some annoying bugs:
- ctrl+w no longer quits everything when the find buffer/build output is selected
- closing ted while the find buffer/build output is selected no longer creates a broken session file

## 1.0r1 — 2022 Jan 1

Some little fixes:
- tags files on Windows with paths containing '/' instead of '\\' are now handled correctly
- no more mysterious carriage returns in files on Windows?
- instructions for downloading PCRE2 are now up-to-date

## 1.0 — 2021 Apr 20

Finally, a stable version of ted! (Or at least, it's been stable for me)

## 0.8 — 2021 Mar 4

Autocomplete

## 0.7 — 2021 Mar 3

Restore session, command selector, :shell, big bug fixes

## 0.6 — 2021 Feb 28

Split-screen

## 0.5a — 2021 Feb 23

Several bugfixes, go to line

## 0.5 — 2021 Feb 22

Go to definition

## 0.4 — 2021 Feb 18

:build

## 0.3a — 2021 Feb 14

Find+replace bug fixes, view-only mode

## 0.3 — 2021 Feb 11

Find+replace, highlight matching parentheses, indent/dedent selection

## 0.2 — 2021 Feb 5

Line numbers, check if file changed by another program

## 0.1 — 2021 Feb 3

Syntax highlighting

## 0.0 — 2021 Jan 31

Very basic editor

