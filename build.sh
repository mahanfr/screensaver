#!/bin/sh

set -xe

mkdir -p build/
clang -O3 -o build/screensaver screensaver.c dvd.c bubble.c -lraylib -lX11 -lm
