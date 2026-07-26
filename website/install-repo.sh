#!/bin/sh
curl -o /etc/apt/sources.list.d/pommicket.sources \
	https://ted.pommicket.com/pommicket.sources || exit 1
curl -o /usr/share/keyring/pommicket.gpg \
	https://ted.pommicket.com/pommicket.gpg || exit 1
echo 'Debian package repository installed.'
echo 'To remove the repository, remove the file /etc/apt/sources.list.d/pommicket.sources.'
