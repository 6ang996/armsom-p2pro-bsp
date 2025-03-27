#!/bin/sh
set -x
make -C kernel/ ARCH=arm64 "$@"
