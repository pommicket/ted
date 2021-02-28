# ted

A text editor.

**ted is still very new, and there are almost certainly bugs. There are also definitely important features missing. I don't recommend using this as your main text editor yet.**

<img src="ted.png">


To install ted, you will need to build it from source (see below). Eventually there will be a nice installer, but only when it's stable and bug-free enough for
ordinary use.

## Why?

There are a lot of text editors out there. ted doesn't do anything new.
But in the modern world of text editors running browsers internally, it can be nice to have
a simple editor that starts up practically instantaneously, and performs well on reasonably-sized files.

## Supported features (more coming soon)

- Multiple tabs, each with a different file
- Auto-indent
- Customization of (pretty much) all colours and keyboard commands.
- Syntax highlighting for C, C++, Rust, and Python.
- Find and replace (with regular expressions!)
- Run build command (default keybinding F4), go to errors
- Go to definition (ctrl+click)
- Go to line (Ctrl+G)

## Building from source

To install `ted` on Linux, you will need:

- A C compiler
- The SDL2 development libraries
- wget, unzip (for downloading, extracting PCRE2)

These can be installed on Ubuntu/Debian with:

```
sudo apt install gcc libsdl2-dev wget unzip
```

Then run

```
wget https://ftp.pcre.org/pub/pcre/pcre2-10.36.zip
sudo make install -j4
```

On Windows (64-bit), first you will need to install Microsoft Visual Studio, then find and add vcvarsall.bat to your PATH.
Next you will need the SDL2 VC development libraries: https://www.libsdl.org/download-2.0.php  
Extract the zip, copy SDL2-2.x.y into the ted directory, and rename it to SDL2. Also copy SDL2\lib\x64\SDL2.dll
to the ted directory.  
You will also need PCRE2. Download it here: https://ftp.pcre.org/pub/pcre/pcre2-10.36.zip,
unzip it, and put pcre2-10.36 in the same folder as ted.

Then run `make.bat`.

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
</table>

## License

ted is in the public domain (see `LICENSE.txt`).

## Reporting bugs

You can report a bug by sending an email to `pommicket at pommicket.com`.

