#!/bin/sh
cd $(dirname $0) || exit 1
cargo run || exit 1
rclone copy -P dist linode-br:/ted.pommicket.com/
