#!/bin/sh
set -e

if [ $(which time) ]; then
	TIMEIT=time
fi

mkdir -p .package

$TIMEIT cc main.c -I/usr/local/share/aqua/lib/c/ -shared -fPIC -o .package/entry.native
$TIMEIT aqua-manager --layout
$TIMEIT iar --pack .package/ --output package.zpk