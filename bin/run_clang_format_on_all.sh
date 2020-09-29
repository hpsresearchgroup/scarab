#!/usr/bin/bash

SCARAB_PATH="$(realpath $(dirname "${0}")/..)"

echo $SCARAB_PATH
for src_file in `find $SCARAB_PATH -name '*.c' -o -name '*.cpp' -o -name '*.cc' -o -name '*.h'`; do
  clang-format -i $src_file
done
