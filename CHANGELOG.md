## 3.2.5 — 2026 Aug 1

- Make `text-size` update automatically when config is changed.
- Fix blurry rendering on Linux with high DPI displays due to SDL3 migration.
- Scale `text-size` by DPI on Linux — you might need to adjust your `text-size` for high DPI displays.
  It was already scaled on Windows. I never bothered doing it for Linux because it wasn't easy to
  look up the DPI scale with SDL2, but now with SDL3 it is :)
- ted website is now installed to `/usr/share/doc/ted` on Linux.

## 3.2.4 — 2026 Jul 26

Fix composition being shown in non-active buffers.

## 3.2.3 — 2026 Jul 20

Fix further SDL3 migration bugs.

## 3.2.2 — 2026 Jul 20

Fix issue with shift/ctrl+click.

## 3.2.1 — 2026 Jul 20

Fix crash when ted is closed on some systems.

## 3.2.0 — 2026 Jul 19

Upgrade to SDL3, IME input

## 3.1.9 — 2026 Jun 9

Fix home/end keys being weird with selector open.

## 3.1.8 — 2026 May 27

LSP didSave notifications

## 3.1.7 — 2026 May 10

`:copy-ref`, improved filefinder search

## 3.1.6 — 2026 Apr 22

`follow-symlinks` setting

## 3.1.5 — 2026 Apr 18

Fix another crash and active tab being wrong after dragging

## 3.1.4 — 2026 Apr 7

Fix split view crash

## 3.1.3 — 2026 Apr 5

File finder performance improvements

## 3.1.2 — 2026 Apr 1

File finder bug fix

## 3.1.1 — 2026 Mar 25

File finder improvements, `trust-lsp-symbol-filtering`, bug fixes

## 3.1.0 — 2026 Mar 24

File finder, more `:shell` environment variables

## 3.0.3 — 2026 Feb 24

File selector bug fix

## 3.0.2 — 2026 Feb 22

Better selector entry sorting

## 3.0.1 — 2026 Feb 19

Fix C++ raw string highlighting

## 3.0.0 — 2026 Feb 18

Remove path length limit, CSS/JS-in-HTML highlighting, &amp; more

## 2.9.1 — 2025 Oct 24

Small syntax highlighting fixes

## 2.9.0 — 2025 Sep 30

LSP code actions, bug fixes

## 2.8.4 — 2025 Sep 28

Keep cursor pos on reload, other small improvements

## 2.8.3 — 2025 Sep 1

Fix annoying auto-indent behaviour

## 2.8.2 — 2025 Jun 27

Fix syntax highlighting bug

## 2.8.1 — 2025 Jun 16

Better handling of automatic file reloading

## 2.8.0 — 2025 Jun 12

Add syntax highlighting for C#; improvements to other languages

## 2.7.8 — 2025 Mar 23

Fix occasional crash (bad settings pointer)

## 2.7.7 — 2025 Mar 5

Add prepareRename support, fix IDE hover

## 2.7.6 — 2024 Dec 8

Fix new LSP bug introduced by 2.7.5

## 2.7.5 — 2024 Dec 7

LSP bug fix

## 2.7.4 — 2024 Sep 8

find/replace and LSP bug fixes

## 2.7.3 — 2024 Sep 8

configure data directories, set indentation manually

## 2.7.2 — 2024 Jul 17

bug fixes, <code>sync</code> setting

## 2.7.1 — 2024 Feb 13

bug fixes, auto-detect indentation

## 2.7.0 — 2023 Oct 19

<code>.editorconfig</code> and local <code>.ted.cfg</code>

## 2.6.2 — 2023 Sep 24

fix cursor position issue, nicer logging, status in title bar

## 2.6.1 — 2023 Sep 14

LSP-related bugfixes

## 2.6 — 2023 Sep 10

LSP diagnostics, LSP over TCP, GDScript support, &amp; more

## 2.5.1 — 2023 Aug 26

Bug fixes

## 2.5 — 2023 Aug 15

Rename symbol, document links, bug fixes

## 2.4.3 — 2023 Aug 1

Some font related fixes

## 2.4.2 — 2023 Jul 21

Fix font absolute paths

## 2.4.1 — 2023 Jul 20

JSX highlighting fix, Windows DPI awareness

## 2.4 — 2023 Jul 19

Font overhaul — allow multiple fonts, and variable-width fonts.

## 2.3.4 — 2023 Jul 14

Unicode bugfix, <code>:copy-path</code>

## 2.3.3 — 2023 Jul 6

JS highlighting improvments, fix TODO highlighting for single-line comments

## 2.3.2 — 2023 Jun 17

Misc bugfixes

## 2.3.1 — 2023 May 22

Bugfixes, better undo chaining, highlight TODOs in comments.

## 2.3 — 2023 May 11

<code>:matching-bracket</code>, various minor improvements

## 2.2r1 — 2023 Mar 27

Minor bug fixes

## 2.2 — 2023 Mar 23

Keyboard macros

## 2.1 — 2023 Mar 7

Better interaction between path+language specific settings, themes, and other things.

## 2.0 — 2023 Jan 11

LSP support and a bunch of other things.

## 1.3r2 — 2022 Dec 7

Fixed high CPU usage on some devices.

## 1.3r1 — 2022 Nov 4

Fixed rust, python syntax highlighting.

## 1.3 — 2022 Nov 3

Custom background shader, some bugfixes.

## 1.2r2 — 2022 Sep 30

Shift+PgUp/PgDown, many rust-related fixes.

## 1.2r1 — 2022 Aug 19

Mouse X1/X2 bug fix, support for X1/X2 commands.

## 1.2 — 2022 Jul 29

Bug fixes, per-language settings

## 1.1 — 2022 Jul 22

Minor fixes, syntax highlighting for JavaScript, Java, and Go

## 1.0r3 — 2022 Jul 7

Better TeX syntax highlighting, move to cursor on backspace/delete

## 1.0r2 — 2022 Mar 26

Various bugfixes involving closing tabs and windows

## 1.0r1 — 2022 Jan 1

Windows-specific bugfixes, update to new version of PCRE2

## 1.0 — 2021 Apr 20

Bugfixes, small additional features, installers

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

