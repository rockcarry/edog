#!/bin/sh

set -e

case "$1" in
"")
    gcc -Wall -DTEST -o edog edog.c
    strip edog
    ;;
clean)
    rm -rf *.edt *.edb edog
    ;;
esac
