#!/usr/bin/bash

# Work-in-Progress script for initializing a MinGW crossbuild environment in a Debian chroot and configuring UFO:AI

# Note: This is more a list of commands to run yet, than an actual all-in-one-script

export PS1='\u@chroot:\w\$ '
export DEVEL_DIR=/development

mkdir -p $DEVEL_DIR

apt-get update

exit

apt-get install \
	bison \
	g++-mingw-w64 \
	gcc-mingw-w64 \
	gawk \
	git \
	libz-mingw-w64-dev \
	linux-headers-6.1.0-22-common \
	make \
	mingw-w64-tools \
	python3 \
	wget \
	zip


# --- SDL2 ---

wget -O $DEVEL_DIR/SDL2-devel-2.30.6-mingw.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-2.30.6/SDL2-devel-2.30.6-mingw.tar.gz
tar -C $DEVEL_DIR -xzf $DEVEL_DIR/SDL2-devel-2.30.6-mingw.tar.gz
pushd $DEVEL_DIR/SDL2-2.30.6
make cross CROSS_PATH=/usr
popd


# --- SDL2-Mixer ---

wget -O $DEVEL_DIR/SDL2_mixer-devel-2.8.0-mingw.tar.gz https://github.com/libsdl-org/SDL_mixer/releases/download/release-2.8.0/SDL2_mixer-devel-2.8.0-mingw.tar.gz
tar -C $DEVEL_DIR -xzf $DEVEL_DIR/SDL2_mixer-devel-2.8.0-mingw.tar.gz
pushd $DEVEL_DIR/SDL2-2.30.6
make cross CROSS_PATH=/usr
popd



# --- LUA 5.4 ---

wget -O $DEVEL_DIR/lua-5.4.7.tar.gz https://www.lua.org/ftp/lua-5.4.7.tar.gz
tar -C $DEVEL_DIR -xzf $DEVEL_DIR/lua-5.4.7.tar.gz
pushd $DEVEL_DIR/lua-5.4.7

# 64bit
OLDPATH=$PATH
PATH=$OLDPATH:/usr/x86_64-w64-mingw32/bin
LUA_MAKEFILE_ARGS=( \
	PLAT=mingw \
	INSTALL_TOP=/usr/x86_64-w64-mingw32 \
	CC='x86_64-w64-mingw32-gcc' \
	TO_BIN='lua.exe luac.exe' \
	TO_LIB='liblua.a lua54.dll' \
)
make "${LUA_MAKEFILE_ARGS[@]}"
make "${LUA_MAKEFILE_ARGS[@]}" install
make "${LUA_MAKEFILE_ARGS[@]}" clean
ln -s liblua.a /usr/x86_64-w64-mingw32/lib/liblua5.4.a

# 32bit
# @TODO: Should we care about 32bit still? MinGW will drop 32bit support soonish
PATH=$OLDPATH:/usr/i686-w64-mingw32/bin
LUA_MAKEFILE_ARGS=( \
	PLAT=mingw \
	INSTALL_TOP=/usr/i686-w64-mingw32 \
	CC='i686-w64-mingw32-gcc' \
	TO_BIN='lua.exe luac.exe' \
	TO_LIB='liblua.a lua54.dll' \
)
make "${LUA_MAKEFILE_ARGS[@]}"
make "${LUA_MAKEFILE_ARGS[@]}" install
make "${LUA_MAKEFILE_ARGS[@]}" clean
ln -s liblua.a /usr/i686-w64-mingw32/lib/liblua5.4.a

PATH=$OLDPATH
popd


# --- CURL ---

# 64bit
wget -O $DEVEL_DIR/curl-8.9.1_1-win64-mingw.zip https://curl.se/windows/dl-8.9.1_1/curl-8.9.1_1-win64-mingw.zip
unzip -d $DEVEL_DIR/ $DEVEL_DIR/curl-8.9.1_1-win64-mingw.zip
pushd $DEVEL_DIR/curl-8.9.1_1-win64-mingw
cp -r bin/* /usr/x86_64-w64-mingw32/bin/
cp -r include/* /usr/x86_64-w64-mingw32/include/
cp -r lib/* /usr/x86_64-w64-mingw32/lib/
popd

# 32bit
wget -O $DEVEL_DIR/curl-8.9.1_1-win32-mingw.zip https://curl.se/windows/dl-8.9.1_1/curl-8.9.1_1-win32-mingw.zip
unzip -d $DEVEL_DIR/ $DEVEL_DIR/curl-8.9.1_1-win32-mingw.zip
pushd $DEVEL_DIR/curl-8.9.1_1-win32-mingw
cp -r bin/* /usr/i686-w64-mingw32/bin/
cp -r include/* /usr/i686-w64-mingw32/include/
cp -r lib/* /usr/i686-w64-mingw32/lib/
popd


# --- LibJPEG ---

wget -O $DEVEL_DIR/libjpeg-turbo-3.0.3.tar.gz https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/3.0.3.tar.gz
tar -C $DEVEL_DIR -xzf $DEVEL_DIR/libjpeg-turbo-3.0.3.tar.gz

# 64bit
mkdir -p $DEVEL_DIR/libjpeg-turbo-3.0.3/.build64
pushd $DEVEL_DIR/libjpeg-turbo-3.0.3/.build64
cat > toolchain64.cmake <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)
set(CMAKE_ASM_NASM_COMPILER /usr/bin/nasm)
EOF
cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=toolchain64.cmake \
	-DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
	..
make
make install
popd

#32bit
# TODO
	# mkdir -p $DEVEL_DIR/libjpeg-turbo-3.0.3/.build32
	# pushd $DEVEL_DIR/libjpeg-turbo-3.0.3/.build32
	# cat > toolchain64.cmake <<EOF
	# set(CMAKE_SYSTEM_NAME Windows)
	# set(CMAKE_SYSTEM_PROCESSOR X86)
	# set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
	# set(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)
	# set(CMAKE_ASM_NASM_COMPILER /usr/bin/nasm)
	# EOF
	# cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=toolchain32.cmake \
	# 	-DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
	# 	..
	# make
	# make install
	# popd


# --- LibPNG ---
wget -O $DEVEL_DIR/libpng-1.6.43.tar.gz https://sourceforge.net/projects/libpng/files/libpng16/1.6.43/libpng-1.6.43.tar.gz/download
tar -C $DEVEL_DIR -xzf $DEVEL_DIR/libpng-1.6.43.tar.gz

mkdir -p $DEVEL_DIR/libpng-1.6.43/.build64
pushd $DEVEL_DIR/libpng-1.6.43/.build64

#cmake . -DCMAKE_INSTALL_PREFIX=/path
cat > toolchain64.cmake <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)
EOF
cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=toolchain64.cmake \
	-DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
	..
make install
popd


# --- LibIConv ---

wget -O $DEVEL_DIR/libiconv-1.17.tar.gz https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.17.tar.gz
tar -C $DEVEL_DIR -xzf $DEVEL_DIR/libiconv-1.17.tar.gz

mkdir -p $DEVEL_DIR/libiconv-1.17/.build64
pushd $DEVEL_DIR/libiconv-1.17/.build64

../configure \
	--prefix=/usr/x86_64-w64-mingw32/ \
	--build=x86_64-pc-linux-gnu \
	--host=x86_64-w64-mingw32 \
	--target=x86_64-w64-mingw32 \
make install

popd

# --- INTL ---
wget -O $DEVEL_DIR/gettext-0.22.5.tar.gz https://ftp.gnu.org/pub/gnu/gettext/gettext-0.22.5.tar.gz
tar -C $DEVEL_DIR -xzf $DEVEL_DIR/gettext-0.22.5.tar.gz

mkdir -p $DEVEL_DIR/gettext-0.22.5/.build64
pushd $DEVEL_DIR/gettext-0.22.5/.build64

../configure \
	--prefix=/usr/x86_64-w64-mingw32/ \
	--build=x86_64-pc-linux-gnu \
	--host=x86_64-w64-mingw32 \
	--target=x86_64-w64-mingw32 \
make install
popd


# --- LibOGG ---

wget -O $DEVEL_DIR/libogg-1.3.5.tar.gz https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.gz
tar -C $DEVEL_DIR -xzf $DEVEL_DIR/libogg-1.3.5.tar.gz

mkdir -p $DEVEL_DIR/libogg-1.3.5/.build64
pushd $DEVEL_DIR/libogg-1.3.5/.build64
cat > toolchain64.cmake <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)
EOF
cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=toolchain64.cmake \
	-DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
	..
make install
popd


# --- Vorbis ---

wget -O $DEVEL_DIR/libvorbis-1.3.7.tar.gz https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.gz
tar -C $DEVEL_DIR -xzf $DEVEL_DIR/libvorbis-1.3.7.tar.gz

mkdir -p $DEVEL_DIR/libvorbis-1.3.7/.build64
pushd $DEVEL_DIR/libvorbis-1.3.7/.build64
cat > toolchain64.cmake <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)
EOF
cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=toolchain64.cmake \
	-DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
	..
make install
popd


# --- UFO:AI ---

git clone git://git.code.sf.net/p/ufoai/code $DEVEL_DIR/ufoai.git
pushd $DEVEL_DIR/ufoai.git
./configure --target-os=mingw64_64
popd