#!/bin/sh

# Usage: makedeb.sh <package name>
# <package name> must be one of "ted" or "ted-static-sdl"


# If you change this, make sure to change control.sh as well.
DEBTMP="deb-tmp-$1"

if [ $# != 1 ]; then
	echo 'Please provide exactly two arguments.' 1>&2
	exit 1
fi

run() {
	echo $@
	$@ || exit 1
}

if cargo --version 2>&1 >/dev/null; then
	cd website
	run cargo run
	cd ..
else
	printf 'Rust not found, so documentation will not be built. Is this okay [y/n]? '
	read ok
	if [ "$ok" != 'y' ]; then
		echo 'Aborting.'
		exit 1
	fi
fi
run rm -rf $DEBTMP
run mkdir -p $DEBTMP/ted/DEBIAN
run mkdir -p $DEBTMP/ted${BIN_DIR}
run mkdir -p $DEBTMP/ted${GLOBAL_DATA_DIR}
run mkdir -p $DEBTMP/ted/usr/share/icons/hicolor/48x48/apps/
run convert assets/icon.bmp -resize 48x48 $DEBTMP/ted/usr/share/icons/hicolor/48x48/apps/ted.png
run mkdir -p $DEBTMP/ted/usr/share/applications
run cp ted.desktop $DEBTMP/ted/usr/share/applications
run cp $1.release $DEBTMP/ted${BIN_DIR}/ted
run cp -r assets themes ted.cfg $DEBTMP/ted${GLOBAL_DATA_DIR}/
if [ -d website/dist ]; then
	run mkdir -p $DEBTMP/ted${DOC_DIR}/
	run cp website/dist/* $DEBTMP/ted${DOC_DIR}/
fi
echo Running "./control.sh $1 > $DEBTMP/ted/DEBIAN/control"
./control.sh $1 > $DEBTMP/ted/DEBIAN/control || exit 1
run dpkg-deb --root-owner-group --build $DEBTMP/ted
run mv $DEBTMP/ted.deb ./$1.deb
run rm -rf $DEBTMP
