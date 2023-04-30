#!/bin/sh

export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH

./fontvideo $*
