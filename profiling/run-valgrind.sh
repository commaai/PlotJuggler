valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes \
  --callgrind-out-file=./callgrind.out.pj ../build/bin/plotjuggler \
  -d ./tmp0ic1nwvr.rlog
