#!/bin/sh
source device/rockchip/.BoardConfig.mk
set -x
make -C buildroot O=output/${RK_CFG_BUILDROOT} "$@"
