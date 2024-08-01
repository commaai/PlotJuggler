#!/bin/bash -e

docker build -f Dockerfile -t plotjuggler:latest .
# docker build -f 3rdparty/cereal -t cereal:latest .

docker run \
  --rm \
  --volume $PWD:/tmp/plotjuggler \
  --workdir /tmp/plotjuggler plotjuggler:latest \
  /bin/bash -c "mkdir -p build && cd build && cmake .. && make -j$(nproc)"
