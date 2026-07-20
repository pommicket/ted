#!/bin/sh

# Script for generating debian control file for ted

echo 'Package: ted'
printf 'Version: %s\n' $(grep '#define TED_VERSION' ted.h | cut -d'"' -f2)
echo 'Section: text
Priority: optional
Architecture: amd64
Essential: no
Maintainer: Pommicket <pommicket@gmail.com>
Description: A text editor.'
printf 'Installed-Size: %s\n' $(du -k $1 | tail -n1 | cut -f1)
echo 'Depends: libsdl3-0
Homepage: https://github.com/pommicket/ted'
