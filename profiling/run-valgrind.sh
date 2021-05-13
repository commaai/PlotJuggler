#!/bin/bash -e

cd ../
export LD_PROFILE_OUTPUT=$(pwd)
cd profiling

export LD_PROFILE=libDataLoadRlog.so
rm -f $LD_PROFILE.profile

# Run build.sh on your own if ../build exists but files have been updated
[[ -d ../build ]] || (cd ../; sh build.sh)

valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes \
  --callgrind-out-file=callgrind.out.pj ../build/bin/plotjuggler -d tmpaq3p1j9w.rlog

# Close plotjuggler window manually after it loads log

kcachegrind callgrind.out.pj
