#!/bin/sh
[ "$1" = "" ] && { echo "please provide path to repo"; exit 1; }
[ -d "$1/pool/main" ] || { echo "expected $1/pool/main to exist, but it doesn't"; exit 1; }
make ted.deb || exit 1
VERSION=$(grep '^Version: ' control | sed 's/^Version: //' | grep '[0-9]*\.[0-9]*\.[0-9]*' || exit 1)
cp ted.deb "$1/pool/main/ted_${VERSION}-1_amd64.deb"
echo "Added ted v. $VERSION"
