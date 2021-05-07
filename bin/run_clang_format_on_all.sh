#!/usr/bin/env bash

SCARAB_PATH="$(realpath $(dirname "${0}")/..)"

echo $SCARAB_PATH
for src_file in `find $SCARAB_PATH -path $SCARAB_PATH/src/ramulator -prune -o -name '*.c' -o -name '*.cpp' -o -name '*.cc' -o -name '*.h'`; do
  clang-format -i $src_file
done
