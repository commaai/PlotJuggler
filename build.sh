#!/bin/bash -e

cd 3rdparty/cereal
docker pull $(grep -ioP '(?<=^from)\s+\S+' Dockerfile)  # todo: required?
docker pull docker.io/commaai/cereal:latest
docker build --cache-from docker.io/commaai/cereal:latest -t cereal -f Dockerfile .
docker run --rm --volume ${PWD}:/tmp/plotjuggler/3rdparty/cereal --workdir /tmp/plotjuggler/3rdparty/cereal --shm-size 1G --name cereal cereal \
  /bin/sh -c "scons -j8"

cd ../opendbc
docker pull $(grep -ioP '(?<=^from)\s+\S+' Dockerfile)  # todo: required?
docker pull docker.io/commaai/opendbc:latest
docker build --cache-from docker.io/commaai/opendbc:latest -t opendbc -f Dockerfile .
docker run --rm --volume ${PWD}:/project/opendbc --shm-size 1G --name opendbc opendbc \
  /bin/sh -c "scons -j8"

#docker run --rm --volume ${PWD}:/tmp/plotjuggler/3rdparty/cereal --workdir /tmp/plotjuggler/3rdparty/cereal --shm-size 1G --name cereal cereal \
#  /bin/sh -c "scons -j8"

cd ../../
docker build -f Dockerfile -t plotjuggler:latest .

docker run --rm --volume $PWD:/tmp/plotjuggler --workdir /tmp/plotjuggler plotjuggler:latest \
  /bin/bash -c "mkdir -p build && cd build && cmake .. && make -j8"
