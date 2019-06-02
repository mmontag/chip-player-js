#!/bin/bash

djgpp_prefix=/usr/local/djgpp
#cmake_toolchain_root=/home/vitaly/_git_repos/libADLMIDI/cmake/djgpp

# export PKG_CONFIG_LIBDIR="${djgpp_prefix}/lib/pkgconfig"

# djgpp_c_flags="-O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions --param=ssp-buffer-size=4"
# export CFLAGS="$djgpp_c_flags"
# export CXXFLAGS="$djgpp_c_flags"

CUSTOM_PATH=${cmake_toolchain_root}:${djgpp_prefix}/bin:${djgpp_prefix}/libexec/gcc/i586-pc-msdosdjgpp/7.2.0:$PATH

if [[ "$1" != '--build' ]]; then
    echo "KEK [${CUSTOM_PATH}]"

    PATH=${CUSTOM_PATH} cmake \
        -DCMAKE_INSTALL_PREFIX:PATH=${djgpp_prefix} \
        -DCMAKE_INSTALL_LIBDIR:PATH=${djgpp_prefix}/lib \
        -DBUILD_SHARED_LIBS:BOOL=OFF \
        -DCMAKE_TOOLCHAIN_FILE=${cmake_toolchain_root}/toolchain-djgpp.cmake \
        "$@"

#        -DINCLUDE_INSTALL_DIR:PATH=${djgpp_prefix}/include \\
#        -DLIB_INSTALL_DIR:PATH=${djgpp_prefix}/lib \\
#        -DSYSCONF_INSTALL_DIR:PATH=${djgpp_prefix}/etc \\
#        -DSHARE_INSTALL_DIR:PATH=${djgpp_prefix}/share \\

else
    PATH=${CUSTOM_PATH} cmake "$@"
fi

#-DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/i686-pc-msdosdjgpp-wine

