#!/bin/bash -e

docker build -f Dockerfile -t plotjuggler:latest .

docker run --rm --volume $PWD:/tmp/plotjuggler --workdir /tmp/plotjuggler plotjuggler:latest \
  /bin/bash -c "mkdir -p build && cd build && cmake -DCMAKE_POSITION_INDEPENDENT_CODE=1 .. && make -j8"
