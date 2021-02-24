FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential \
  cmake \
  libbfd-dev \
  libdwarf-dev \
  libdw-dev \
  libbz2-dev \
  libcapnp-dev \
  python3 \
  python3-pip

RUN apt-get update

RUN pip3 install jinja2
ENV PYTHONPATH /tmp/plotjuggler/3rdparty

RUN echo "deb http://archive.ubuntu.com/ubuntu bionic main restricted" > /etc/apt/sources.list.d/bionic.list
RUN echo "deb http://archive.ubuntu.com/ubuntu bionic-updates main restricted" >> /etc/apt/sources.list.d/bionic.list

RUN echo "deb http://archive.ubuntu.com/ubuntu bionic universe" >> /etc/apt/sources.list.d/bionic.list
RUN echo "deb http://archive.ubuntu.com/ubuntu bionic-updates universe" >> /etc/apt/sources.list.d/bionic.list

RUN echo "deb http://archive.ubuntu.com/ubuntu bionic multiverse" >> /etc/apt/sources.list.d/bionic.list
RUN echo "deb http://archive.ubuntu.com/ubuntu bionic-updates multiverse" >> /etc/apt/sources.list.d/bionic.list

RUN echo "deb http://archive.ubuntu.com/ubuntu bionic-backports main restricted universe multiverse" >> /etc/apt/sources.list.d/bionic.list

RUN echo "deb http://archive.ubuntu.com/ubuntu bionic-security main restricted" >> /etc/apt/sources.list.d/bionic.list
RUN echo "deb http://archive.ubuntu.com/ubuntu bionic-security universe" >> /etc/apt/sources.list.d/bionic.list
RUN echo "deb http://archive.ubuntu.com/ubuntu bionic-security multiverse" >> /etc/apt/sources.list.d/bionic.list

RUN apt-get update && apt-get install -y --no-install-recommends -t bionic \
    qt5-default \
    qtbase5-dev \
    libqt5svg5-dev \
    libqt5websockets5-dev \
    libqt5opengl5-dev \
    libqt5x11extras5-dev
