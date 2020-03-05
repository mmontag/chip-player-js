FROM ubuntu:20.04

RUN apt-get update && \
    apt-get install -y \
      gcc \
      g++ \
      zlib1g-dev \
      cmake \
      libasound2-dev \
      libpulse-dev \
      oss4-dev \
      make && \
    mkdir -p /src/

COPY . /src/libvgm

RUN mkdir -p /src/libvgm-build && \
    cd /src/libvgm-build && \
    cmake /src/libvgm && \
    make VERBOSE=1
