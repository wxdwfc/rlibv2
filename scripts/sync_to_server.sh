#!/usr/bin/env bash
# this script will sync the project to the remote server

user="lfm"
target=("val14")

for machine in ${target[*]}
do
      rsync -i -rtuv \
            $PWD/../core $PWD/../tests $PWD/../examples $PWD/../benchs $PWD/../CMakeLists.txt \
            ${user}@${machine}:/home/${user}/rlib
done
