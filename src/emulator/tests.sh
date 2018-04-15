#!/bin/sh
root_dir="$( cd "$(dirname "$0")" ; pwd -P )"
cd $root_dir
for t in tests/*.bin; do echo $t; lua run.lua --batch --boot-image=$t || echo FAIL; done