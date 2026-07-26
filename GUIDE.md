## Getting started with ted

After installing ted, you can just start using it like you would any other editor. The keyboard shortcuts
are mostly what you'd expect them to be (Ctrl+o for open, Ctrl+n for new, Ctrl+s for save, etc.).

### Tips

- Even if you don't want to change anything with ted, it's a good idea to look at the config file (see below) to
check out all of the keyboard shortcuts!
- You can use Ctrl+f for "find", but if you want to search for something across multiple files, you can do
Ctrl+! (run shell command), then run `grep -n search_term *.py`, for example (on Windows, you will need to have
cygwin or something in your PATH for this to work). The `-n` ensures that
ted can jump to the results, just like jumping to build errors.
- ted uses PCRE for regular expressions. This means that when using find+replace, if you want to
replace with a captured group, you need to use `$1`, not (as you might expect) `\1`.

### Configuration

At any time, you can check out all the keyboard shortcuts, and add your own, by opening your ted.cfg file.
To do this, press Ctrl+Shift+p
to open the command palette, and select "open-config". There are several sections to this config file:

- `[core]` for core settings
- `[keyboard]` for keyboard shortcuts
- `[colors]` for colors
- `[extensions]` for which file extensions should be mapped to which programming languages

Comments begin with `#`, and all other lines are of the form `key = value`.
Strings can span multiple lines and can either be delimited with " or \`.

You can also include files with `%include` (note: the included file must start with a section header).

By default ted's settings will automatically update when you save the config file.

The `core` section's settings should be pretty familiar (font size, etc.) or should have comments on the previous line
explaining what they do. `yes`, `on`, and `true` are all synonyms, as are `no`, `off`, and `false`.

Keyboard shortcuts are of the form `key combo = action`, where `action` is an argument (number or string),
followed by a command. The commands match the things in the command palette (Ctrl+Shift+p), but `:` is added to the beginning to make
it clear it's a command.
A list of keys can be found [here](https://wiki.libsdl.org/SDL3/SDL_Keycode).

Colors are formatted like `#rgb`, `#rgba`, `#rrggbb` or `#rrggbbaa`, where r, g, b, and a are red, green,
blue, and alpha (transparency/opacity). You can use a [color picker](https://www.google.com/search?q=color+picker) to help you out.
The extensions section is fairly self-explanatory.

You can set settings for specific programming languages like this:

```
[HTML.core]
# set tab width for HTML files to 2
tab-width = 2
```

Or settings for specific paths like this:

```
[/foo//core]
# set tab width in /foo to 17
tab-width = 17

# PCRE regex is supported! (the start is anchored but the end isn't)
[~/foo/.*\.hmtgf//core]
# set tab width for .hmtgf files in ~/foo
tab-width = 9
```

or put a file called `.ted.cfg` in any directory to have settings apply to files in that directory.

ted reads [.editorconfig](https://editorconfig.org) files automatically. you can override them by putting `.ted.cfg`
with the settings you want in the same directory.

To reset your ted configuration to the default settings, delete your ted.cfg file (`~/.local/share/ted/ted.cfg` on Linux,
`C:\Users\<your user name>\AppData\Local\ted\ted.cfg` on Windows) or move it somewhere else.

#### Themes

At the top of `ted.cfg` you will see a line which includes a theme.
To modify just one color in the theme, you can do something like

```
%include themes/classic.ted.cfg

[colors]
# replace background color with solid red!
bg = #f00
```

but you can also change to a different theme. Currently `classic`,
`classic-light`, and `extradark` are available.

No matter what you should include a built-in theme (even if you
replace every single color), because more colors may be added to ted in the future,
and you will want them to be set to something reasonable.

### Keyboard macros

To record a macro, press Ctrl+F1/2/3/etc. While recording a macro,
you won't be able to click or drag (this is to make sure your macro works consistently).
Then press Ctrl+F*n* again to stop recording. You can execute the macro with Shift+F*n*.

Currently macros are always lost when ted is closed. The ability to save macros will probably
be added eventually.

### IDE-like features

If you are working in a compiled language, like C, you can press F4 to compile your code. The default is to run `make` in
the current directory or one of its parents, depending on where `Makefile` is. On Windows, if `make.bat` exists, it will be run.
If a `Cargo.toml` file exists in this directory or one of its parents, F4 will run `cargo build`. You can set the default build command
in the `[core]` section of the config file.

You can press Ctrl+\[ and Ctrl+\] to navigate between build errors.

### ctags vs LSP

`ted` has support for two separate systems for IDE features. `ctags`
is very lightweight (a ctags installation is just 1.6 MB), and allows
for go-to-definition and limited autocompletion. This has very low CPU usage,
and will work just fine on very large projects (for large projects I would
recommend increasing `tags-max-depth` and turning `regenerate-tags-if-not-found` off).

LSP servers have lots of features but use lots of CPU and memory,
and may take longer to come up with completions/find definitions, especially
for large projects. However the LSP server runs in a separate thread, so it will not slow down
the ordinary text editing features of `ted` (unless the server starts
using 100% of all CPU cores, which is unlikely).

I would recommend trying out an LSP server if you're unsure about which one to use.

## LSP support

ted has support for [LSP servers](https://microsoft.github.io/language-server-protocol/)!

All the functionality listed below is only available if the server supports it.

You can Ctrl+Click on an identifier to go to its definition, or Ctrl+Shift+Click to go
to its declaration, or Ctrl+Alt+Click to go to its type's definition.

You can also press Ctrl+D to get a searchable list of all functions/types where you can select one to go to
its definition.

Press Ctrl+space to autocomplete. If there is only one possible completion, it will be selected automatically.
Otherwise, you'll get a popup showing all possible completions. You can press tab to select a completion (or click on it), and press
Ctrl+space/Ctrl+shift+space to cycle between suggestions.

When there is only one possible completion and the autocomplete window isn't open, a "phantom completion" will appear in gray text.
Press tab to use it or continue typing if it isn't what you want. (This can be turned off in the settings if you don't like it.)

Hover over an identifier and press F1 to see its type and documentation ("hover information").

While your cursor is over an identifier, you can press F2 to highlight where it is used
("document highlights"). If you turn on `highlight-auto` in the settings, the highlights
will appear even if you don't press F2.

Press Ctrl+U to see usages of the identifier under the cursor. You can use Ctrl+\[ and Ctrl+\]
to navigate between them, just like build errors.

If these features aren't working properly and you don't know why, try running ted in a terminal (non-Windows) or a debugger (Windows)
so you can see the stderr output from the server, or turn on the `lsp-log` setting and inspect ted's log (which is called `log.txt`
and is in the same directory as your local `ted.cfg`).

If an LSP server crashes or is having difficulty, you can run the `lsp-reset` command (via the command palette)
to reset all running LSP servers.

You can integrate any LSP server with ted by setting the `lsp` option in the `[core.<language>]` section of `ted.cfg`
to the command which starts the server. Some defaults will already be there, and are listed below. Make
sure you install the LSP server(s) you want and put the executables in your PATH (or change the `lsp` variable
to use the absolute path if not). You can also set configuration options with the `lsp-configuration` option.
Make sure the configuration you provide is valid JSON.

### C/C++

[clangd](https://clangd.llvm.org/installation)
is enabled by default. On Debian/Ubuntu you can install it with:

```bash
sudo apt install clangd-15  # replace 15 with the highest number you can get
sudo ln -s /usr/bin/clangd-15 /usr/bin/clangd
```

On Windows it can be downloaded [from here](https://github.com/clangd/clangd/releases).

For "go to definition" and "find usages" to work properly, you may need to
create [a compile\_commands.json file](https://clangd.llvm.org/installation#compile_commandsjson).

### Go

The Go team's `go-pls` is enabled by default. You can download it
[here](https://github.com/golang/tools/tree/master/gopls).


### Java

Eclipse's `jdtls` is enabled by default.
You can download it [here](https://download.eclipse.org/jdtls/milestones/?d).

### JavaScript/TypeScript

`typescript-language-server` is enabled by default.
You can download it by following
[the instructions here](https://github.com/typescript-language-server/typescript-language-server).

### LaTeX

`texlab` is enabled by default. You can download it
[here](https://github.com/latex-lsp/texlab).

### Python
`python-lsp-server` is enabled by default.
You can download it [here](https://github.com/python-lsp/python-lsp-server).

### Rust

`rust-analyzer` is enabled by default. You can install it with:

```sh
rustup component add rust-analyzer
```

## Tags (lightweight LSP alternative)

If an LSP is too much for you, you can also use [ctags](https://github.com/universal-ctags/ctags)
for autocompletion and jump to definition. You can run the `:generate-tags` command
at any time to generate or re-generate tags.
Ctrl+Click (go to definition), Ctrl+D (see all definitions), and autocomplete are all supported.
Autocomplete will just complete to stuff in the tags file, so it won't complete local
variable names for example.
