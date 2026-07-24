#!/bin/sh

# Script for generating debian control file for ted
# Usage: control.sh <package name>
# <package name> must be one of "ted" or "ted-static-sdl"

if [ $# != 1 ]; then
	echo 'Please provide exactly one argument.' 1>&2
	exit 1
fi


if [ "$1" = 'ted' ]; then
	PACKAGE_SPECIFIC='Depends: libsdl3-0'
elif [ "$1" = 'ted-static-sdl' ]; then
	PACKAGE_SPECIFIC='Conflicts: ted'
else
	echo 'Argument must be one of "ted" or "ted-static-sdl"' 1>&2
	exit 1
fi

echo 'Package: '"$1"
printf 'Version: %s\n' $(grep '#define TED_VERSION' ted.h | cut -d'"' -f2)
echo 'Section: text
Priority: optional
Architecture: amd64
Essential: no
Maintainer: Pommicket <pommicket@gmail.com>
Description: A text editor.'
printf 'Installed-Size: %s\n' $(du -k deb-tmp-$1 | tail -n1 | cut -f1)
echo "$PACKAGE_SPECIFIC"
echo 'Homepage: https://github.com/pommicket/ted'
