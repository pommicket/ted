# ted

A text editor.

<img src="ted.png">

This repository is for the latest experimental version of ted. If you want a stable version,
go to the [releases](https://github.com/pommicket/ted/releases). You will also find
installers for Windows and Debian/Ubuntu there.

To get autocomplete and go-to-definiton you will need [ctags](https://github.com/universal-ctags/ctags).
You can install ctags on Debian/Ubuntu with:

```bash
sudo apt install universal-ctags
```

## Why?

There are a lot of text editors out there. ted doesn't do anything new.
I made ted because I wanted a text editor that starts up practically instantaneously,
and performs well on reasonably-sized files.
ted isn't incredibly complicated, but it does have some nice features you might not find
in other editors.

## Supported features

- Customization of (pretty much) all colours and keyboard commands.
- Basic text editing like copy+paste, undo+redo, etc.
- Multiple tabs, each with a different file
- Split screen
- Auto-indent
- Syntax highlighting for C, C++, HTML, LaTeX, Markdown, Python, and Rust.
- Find and replace (with regular expressions!)
- Run build command, go to errors
- Run any shell command
- Go to definition
- Go to line number
- Autocomplete
- Indent/dedent selection, comment/uncomment selection

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

You need to restart ted when you make a change to ted.cfg.

The `core` section's settings should be pretty familiar (font size, etc.) or should have comments on the previous line
explaining what they do. Keyboard shortcuts are of the form `key combo = action`, where `action` is an argument (number or string),
followed by a command. The commands match the things in the command palette (Ctrl+Shift+p), but `:` is added to the beginning to make
it clear it's a command. Colors are formatted like `#rgb`, `#rgba`, `#rrggbb` or `#rrggbbaa`, where r, g, b, and a are red, green,
blue, and alpha (transparency/opacity). You can use a [color picker](https://www.google.com/search?q=color+picker) to help you out. 
The extensions section is fairly self-explanatory.

To reset your ted configuration to the default settings, delete your ted.cfg file (`~/.local/share/ted/ted.cfg` on Linux,
`C:\Users\<your user name>\AppData\Local\ted\ted.cfg` on Windows) or move it somewhere else.

### IDE-like features

If you are working in a compiled language, like C, you can press F4 to compile your code. The default is to run `make` in
the current directory or one of its parents, depending on where `Makefile` is. On Windows, if `make.bat` exists, it will be run.
If a `Cargo.toml` file exists in this directory or one of its parents, F4 will run `cargo build`. You can set the default build command
in the `[core]` section of the config file.

Jump to definition and autocompletion both depend on [ctags](https://github.com/universal-ctags/ctags). You can press Ctrl+T
at any time to generate or re-generate tags. Once you have a tags file, you can Ctrl+Click on an identifier
to go to its definition. You can also press Ctrl+D to get a searchable list of all functions/types where you can select one to go to
its definition.

Press Ctrl+space to autocomplete. If there is only one possible completion from the tags file, it will be selected automatically.
Otherwise, you'll get a popup showing all possible completions. You can press tab to select a completion (or click on it), and press
Ctrl+space/Ctrl+shift+space to cycle between suggestions. Note that autocomplete just completes to stuff in the tags file, so it won't complete local
variable names. Sorry.

## Building from source

To install `ted` from source on Linux, you will need:

- A C compiler
- The SDL2 development libraries
- wget, unzip (for downloading, extracting PCRE2)
- cmake (for PCRE2)

These can be installed on Ubuntu/Debian with:

```
sudo apt install clang libsdl2-dev wget unzip cmake
```

Then run

```
wget https://ftp.pcre.org/pub/pcre/pcre2-10.36.zip
sudo make install -j4
```

You can also run `make ted.deb` to build the .deb installer.

On Windows (64-bit), first you will need to install Microsoft Visual Studio, then find and add vcvarsall.bat to your PATH.
Next you will need the SDL2 VC development libraries: https://www.libsdl.org/download-2.0.php  
Extract the zip, copy SDL2-2.x.y into the ted directory, and rename it to SDL2. Also copy SDL2\\lib\\x64\\SDL2.dll
to the ted directory.  
You will also need PCRE2. Download it here: https://ftp.pcre.org/pub/pcre/pcre2-10.36.zip,
unzip it, and put pcre2-10.36 in the same folder as ted.
Then run `make.bat release`.

To build the .msi file, you will need Visual Studio, as well as the
[Visual Studio Installer Projects extension](https://marketplace.visualstudio.com/items?itemName=VisualStudioClient.MicrosoftVisualStudio2017InstallerProjects).
Then, open windows\_installer\\ted\\ted.sln, and build.

## Version history

<table>
<tr><th>Version</th> <th>Description</th> <th>Date</th></tr>
<tr><td>0.0</td> <td>Very basic editor</td> <td>2021 Jan 31</td></tr>
<tr><td>0.1</td> <td>Syntax highlighting</td> <td>2021 Feb 3</td></tr>
<tr><td>0.2</td> <td>Line numbers, check if file changed by another program</td> <td>2021 Feb 5</td></tr>
<tr><td>0.3</td> <td>Find+replace, highlight matching parentheses, indent/dedent selection</td> <td>2021 Feb 11</td></tr>
<tr><td>0.3a</td> <td>Find+replace bug fixes, view-only mode</td> <td>2021 Feb 14</td></tr>
<tr><td>0.4</td> <td>:build</td> <td>2021 Feb 18</td></tr>
<tr><td>0.5</td> <td>Go to definition</td> <td>2021 Feb 22</td></tr>
<tr><td>0.5a</td> <td>Several bugfixes, go to line</td> <td>2021 Feb 23</td></tr>
<tr><td>0.6</td> <td>Split-screen</td> <td>2021 Feb 28</td></tr>
<tr><td>0.7</td> <td>Restore session, command selector, :shell, big bug fixes</td> <td>2021 Mar 3</td></tr>
<tr><td>0.8</td> <td>Autocomplete</td> <td>2021 Mar 4</td></tr>
<tr><td>1.0</td> <td>Bugfixes, small additional features, installers</td> <td>2021 Apr 20</td></tr>
</table>

## License

ted is in the public domain (see `LICENSE.txt`).

## Reporting bugs

You can report a bug by sending an email to `pommicket at pommicket.com`.

