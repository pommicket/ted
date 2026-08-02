#!/bin/sh
# Get the current version of ted. This is stored in ted.h.
# (If you're changing this, make sure you change website/src/main.rs as well.)
sed -n '/#define TED_VERSION/s/.*"\([0-9.]*\)"/\1/p' `dirname "$0"`/ted.h || exit 1
