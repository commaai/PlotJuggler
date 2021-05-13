#!/bin/bash -e

# Run build.sh on your own if ../build exists but files have been updated
[[ -d ../build ]] || (cd ../; sh build.sh)

valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes \
  --callgrind-out-file=./callgrind.out.pj ../build/bin/plotjuggler \
  -d ./tmp0ic1nwvr.rlog

# Close plotjuggler window manually after it loads log

kcachegrind ./callgrind.out.pj
