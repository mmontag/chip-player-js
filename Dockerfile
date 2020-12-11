FROM ubuntu:20.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && \
    apt-get install -y \
      gcc \
      g++ \
      zlib1g-dev \
      cmake \
      libasound2-dev \
      libpulse-dev \
      oss4-dev \
      mingw-w64 \
      make \
      curl && \
    mkdir -p /src/

# setup toolchain file - i686
RUN echo "set(CMAKE_SYSTEM_NAME Windows)" > /opt/i686-w64-mingw32.cmake && \
    echo "set(TOOLCHAIN_PREFIX i686-w64-mingw32)" >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_PREFIX}-gfortran)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH "/usr/${TOOLCHAIN_PREFIX};/opt/${TOOLCHAIN_PREFIX}")' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> /opt/i686-w64-mingw32.cmake

# setup toolchain file - x86_64
RUN echo "set(CMAKE_SYSTEM_NAME Windows)" > /opt/x86_64-w64-mingw32.cmake && \
    echo "set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)" >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_PREFIX}-gfortran)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH "/usr/${TOOLCHAIN_PREFIX};/opt/${TOOLCHAIN_PREFIX}")' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> /opt/x86_64-w64-mingw32.cmake

COPY . /src/libvgm

# regular ol linux compile
RUN mkdir -p /src/libvgm-build && \
    cd /src/libvgm-build && \
    cmake /src/libvgm && \
    make

# build zlib for win32 and win64
RUN cd /src && \
    curl -R -L -O https://zlib.net/zlib-1.2.11.tar.gz && \
    tar xf zlib-1.2.11.tar.gz && \
    mkdir /src/zlib-win32 && \
    cd /src/zlib-win32 && \
    cmake \
      -DCMAKE_INSTALL_PREFIX=/opt/i686-w64-mingw32 \
      -DCMAKE_TOOLCHAIN_FILE=/opt/i686-w64-mingw32.cmake \
      /src/zlib-1.2.11 && \
    make && \
    make install && \
    mkdir /src/zlib-win64 && \
    cd /src/zlib-win64 && \
    cmake \
      -DCMAKE_INSTALL_PREFIX=/opt/x86_64-w64-mingw32 \
      -DCMAKE_TOOLCHAIN_FILE=/opt/x86_64-w64-mingw32.cmake \
      /src/zlib-1.2.11 && \
    make && \
    make install && \
    mv /opt/i686-w64-mingw32/lib/libzlibstatic.a \
       /opt/i686-w64-mingw32/lib/libz.a && \
    rm /opt/i686-w64-mingw32/lib/libzlib.dll.a && \
    rm /opt/i686-w64-mingw32/bin/libzlib.dll && \
    mv /opt/x86_64-w64-mingw32/lib/libzlibstatic.a \
       /opt/x86_64-w64-mingw32/lib/libz.a && \
    rm /opt/x86_64-w64-mingw32/lib/libzlib.dll.a && \
    rm /opt/x86_64-w64-mingw32/bin/libzlib.dll

# ubuntu 20.04 includes mingw 7.0.0, xaudio2
# is supported starting with version 8.0.0,
# disabling xaudio2 until later upgrade of image
RUN mkdir -p /src/libvgm-build-win32 && \
    cd /src/libvgm-build-win32 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DAUDIODRV_XAUDIO2=OFF \
      -DCMAKE_TOOLCHAIN_FILE=/opt/i686-w64-mingw32.cmake \
      /src/libvgm && \
    make

RUN mkdir -p /src/libvgm-build-win64 && \
    cd /src/libvgm-build-win64 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DAUDIODRV_XAUDIO2=OFF \
      -DCMAKE_TOOLCHAIN_FILE=/opt/x86_64-w64-mingw32.cmake \
      /src/libvgm && \
    make
