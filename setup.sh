#!/bin/bash

export CURRENT_DIR=$(pwd)
export PATH=$PATH:${CURRENT_DIR}/arm-linux-musleabi-cross/bin

export CROSS_COMPILE=arm-linux-musleabi-
export CC=${CROSS_COMPILE}gcc
export CXX=${CROSS_COMPILE}g++
export AR=${CROSS_COMPILE}ar
export RANLIB=${CROSS_COMPILE}ranlib