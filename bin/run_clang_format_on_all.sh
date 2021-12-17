#!/usr/bin/env bash

SCARAB_PATH="$(realpath $(dirname "${0}")/..)"

echo $SCARAB_PATH
for src_file in `find $SCARAB_PATH -regextype posix-extended -regex "$SCARAB_PATH/src/(deps|ramulator|build)" -prune -o -name '*.c' -o -name '*.cpp' -o -name '*.cc' -o -name '*.h'`; do
  clang-format -i $src_file
done
