#!/bin/bash -e

docker build -f Dockerfile -t plotjuggler:latest .

docker run --rm --volume $PWD:/tmp/plotjuggler --workdir /tmp/plotjuggler plotjuggler:latest \
  /bin/bash -c "(cd 3rdparty && cp opendbc/SConstruct . && cp -r opendbc/site_scons . && scons -j8) \
                && mkdir -p build && cd build && cmake .. && make -j8"
