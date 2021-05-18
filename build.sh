#!/bin/bash -e

cd 3rdparty/cereal
docker pull docker.io/commaai/cereal:latest
docker build --cache-from docker.io/commaai/cereal:latest -t cereal -f Dockerfile .  # todo: left off here!
docker run --rm --volume ${PWD}:/tmp/plotjuggler/3rdparty/cereal --workdir /tmp/plotjuggler/3rdparty/cereal --shm-size 1G --name cereal cereal \
  /bin/sh -c "scons -c && scons -j8)"

cd ../../
docker build -f Dockerfile -t plotjuggler:latest .

docker run --rm --volume $PWD:/tmp/plotjuggler --workdir /tmp/plotjuggler plotjuggler:latest \
  /bin/bash -c "mkdir -p build && cd build && cmake .. && make -j8"
