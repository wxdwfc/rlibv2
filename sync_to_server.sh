#!/usr/bin/env bash

#target="$1"
target="wxd@cube1"
## this script will sync the project to the remote server
rsync -i -rtuv \
      $PWD/core $PWD/tests $PWD/examples $PWD/CMakeLists.txt $target:/raid/wxd/rlib/
