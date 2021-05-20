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
  python3-pip \
  wget \
  software-properties-common \
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
    VERSION=0.7.0 && \
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

RUN pip3 install jinja2
ENV PYTHONPATH /tmp/plotjuggler/3rdparty

RUN add-apt-repository "deb http://archive.ubuntu.com/ubuntu bionic main restricted" && \
    add-apt-repository "deb http://archive.ubuntu.com/ubuntu bionic universe" && \
    apt-get update && apt-get install -y --no-install-recommends -t bionic \
    qt5-default \
    qtbase5-dev \
    libqt5svg5-dev \
    libqt5websockets5-dev \
    libqt5opengl5-dev \
    libqt5x11extras5-dev

# opendbc and cereal
RUN curl -L https://github.com/pyenv/pyenv-installer/raw/master/bin/pyenv-installer | bash
ENV PATH="/root/.pyenv/bin:/root/.pyenv/shims:${PATH}"
RUN pyenv install 3.8.5 && \
    pyenv global 3.8.5 && \
    pyenv rehash

# installs scons, pycapnp, cython, etc.
COPY 3rdparty/opendbc/requirements.txt /tmp/
RUN pip install --no-cache-dir -r /tmp/requirements.txt
