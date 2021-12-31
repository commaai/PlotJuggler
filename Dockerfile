FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential \
  cmake \
  libbfd-dev \
  libdwarf-dev \
  libzmq3-dev \
  libdw-dev \
  python3 \
  python3-dev \
  python3-pip \
  python-is-python3 \
  wget \
  software-properties-common \

  qt5-default \
  qtbase5-dev \
  libqt5svg5-dev \
  libqt5websockets5-dev \
  libqt5opengl5-dev \
  libqt5x11extras5-dev \

  # opendbc/cereal
  curl \
  git \
  python-openssl \
  libssl-dev \
  libffi-dev \
  libreadline-dev \
  libsqlite3-dev \
  clang \
  ocl-icd-opencl-dev \
  opencl-headers


RUN cd /tmp && \
    VERSION=0.8.0 && \
    wget --no-check-certificate https://capnproto.org/capnproto-c++-${VERSION}.tar.gz && \
    tar xvf capnproto-c++-${VERSION}.tar.gz && \
    cd capnproto-c++-${VERSION} && \
    CXXFLAGS="-fPIC -O2" ./configure && \
    make -j$(nproc) && \
    make install

RUN cd /tmp && \
    wget --no-check-certificate ftp://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz && \
    tar xvfz bzip2-1.0.8.tar.gz && \
    cd bzip2-1.0.8 && \
    CFLAGS="-fPIC" make -f Makefile-libbz2_so && \
    make && \
    make install

RUN pip3 install pkgconfig jinja2

# installs scons, pycapnp, cython, etc.
ENV PYTHONPATH /tmp/plotjuggler/3rdparty
COPY 3rdparty/opendbc/requirements.txt /tmp/
RUN pip3 install Cython && pip3 install --no-cache-dir -r /tmp/requirements.txt
