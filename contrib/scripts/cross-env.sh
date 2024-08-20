#!/usr/bin/bash

# Work-in-Progress script for initializing a MinGW crossbuild environment in a Debian chroot and configuring UFO:AI

# Note: This is more a list of commands to run yet, than an actual all-in-one-script

export DEVEL_DIR=/development
export JOBS="-j4"

DEPENDENCIES=(
	https://github.com/libsdl-org/SDL/releases/download/release-2.30.6/SDL2-devel-2.30.6-mingw.tar.gz,SDL2-2.30.6,sdl-make
	https://github.com/libsdl-org/SDL_mixer/releases/download/release-2.8.0/SDL2_mixer-devel-2.8.0-mingw.tar.gz,SDL2_mixer-2.8.0,sdl-make
	https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-devel-2.22.0-mingw.tar.gz,SDL2_ttf-2.22.0,sdl-make
	https://www.lua.org/ftp/lua-5.4.7.tar.gz,lua-5.4.7,lua-make
	https://curl.se/windows/dl-8.9.1_1/curl-8.9.1_1-win64-mingw.zip,curl-8.9.1_1-win64-mingw,curl-copy
	https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.0.3/libjpeg-turbo-3.0.3.tar.gz,libjpeg-turbo-3.0.3,cmake
	https://download.sourceforge.net/libpng/libpng-1.6.43.tar.gz,libpng-1.6.43,cmake
	https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.17.tar.gz,libiconv-1.17,automake
	https://ftp.gnu.org/pub/gnu/gettext/gettext-0.22.5.tar.gz,gettext-0.22.5,automake
	https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.gz,libogg-1.3.5,cmake
	https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.gz,libvorbis-1.3.7,cmake
	https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.4.0.tar.gz,libwebp-1.4.0,cmake
	https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz,googletest-1.15.2,cmake
)

mkdir -p $DEVEL_DIR

echo; echo
echo "Installing dependencies from the repository"
apt-get update > "${DEVEL_DIR}/apt-get.log" 2>&1
apt-get install -y \
	cmake \
	git \
	libz-mingw-w64-dev \
	make \
	mingw-w64 \
	mingw-w64-tools \
	nasm \
	python3 \
	wget \
	zip \
	>> "${DEVEL_DIR}/apt-get.log" 2>&1


update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix >> "${DEVEL_DIR}/apt-get.log" 2>&1
update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix >> "${DEVEL_DIR}/apt-get.log" 2>&1

shopt -s extglob
echo; echo
for DEPENDENCY in ${DEPENDENCIES[@]}; do
	URL=${DEPENDENCY%%,*}
	DIRECTORY=${DEPENDENCY%,*}
	DIRECTORY=${DIRECTORY#*,}
	METHOD=${DEPENDENCY##*,}

	# Download
	echo
	ARCHIVE=${URL##*/}
	if [ -s "${DEVEL_DIR}/${ARCHIVE}" ]; then
		echo "Already downloaded: ${ARCHIVE}"
	else
		echo "Downloading ${ARCHIVE}"
		wget -O "${DEVEL_DIR}/${ARCHIVE}" "$URL" > "${DEVEL_DIR}/download_${ARCHIVE}.log" 2>&1
		if [ "$?" != "0" ]; then
			echo "Failed! Check download_${AARCHIVE}.log for details!"
			continue
		fi
	fi

	# Extract
	ARCHIVE_NAME=${ARCHIVE/%?(.tar).@([7gbx]z|zip)/}
	ARCHIVE_EXT=${ARCHIVE/#${ARCHIVE_NAME}/}
	if [ ! -d "${DEVEL_DIR}/${DIRECTORY}" ]; then
		if [ "${ARCHIVE_EXT}" == ".zip" ]; then
			echo "Unzipping ${ARCHIVE}"
			unzip -d "${DEVEL_DIR}" "${DEVEL_DIR}/${ARCHIVE}" > "${DEVEL_DIR}/unzip_${ARCHIVE}.log" 2>&1
			if [ "$?" != "0" ]; then
				echo "Failed! Check unzip_${ARCHIVE}.log"
				continue
			fi
		elif [ "${ARCHIVE_EXT%.*}" == ".tar" ] ; then
			if [ "${ARCHIVE_EXT##*.}" == "gz" ]; then
				TAR_COMPRESS_FLAG="z"
			elif [ "${ARCHIVE_EXT##*.}" == "bz" ]; then
				TAR_COMPRESS_FLAG="j"
			elif [ "${ARCHIVE_EXT##*.}" == "xz" ]; then
				TAR_COMPRESS_FLAG="J"
			fi
			echo "Extracting ${ARCHIVE}"
			tar -C "${DEVEL_DIR}" -x${TAR_COMPRESS_FLAG}f "${DEVEL_DIR}/${ARCHIVE}" > "${DEVEL_DIR}/untar${TAR_COMPRESS_FLAG}_${ARCHIVE}.log" 2>&1
			if [ "$?" != "0" ]; then
				echo "Failed! Check untar_${TAR_COMPRESS_FLAG}z_${ARCHIVE}.log"
				continue
			fi
		else
			echo "Unsupported archive format"
			continue
		fi
	else
		echo "Already extracted ${ARCHIVE}"
	fi

	# Compile & Install
	echo "Compiling ${DIRECTORY} (${METHOD})"
	COMPILE_LOG="${DEVEL_DIR}/compile_${DIRECTORY}.log"
	cat > "${DEVEL_DIR}/toolchain64.cmake" <<EOF
		set(CMAKE_SYSTEM_NAME Windows)
		set(CMAKE_SYSTEM_PROCESSOR AMD64)
		set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
		set(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)
		set(CMAKE_ASM_NASM_COMPILER /usr/bin/nasm)
EOF
	pushd "${DEVEL_DIR}/${DIRECTORY}" > "${COMPILE_LOG}" 2>&1
	if [ "${METHOD}" == "lua-make" ]; then
		OLDPATH=$PATH
		PATH=$OLDPATH:/usr/x86_64-w64-mingw32/bin
		LUA_MAKEFILE_ARGS=( \
			PLAT=mingw \
			INSTALL_TOP=/usr/x86_64-w64-mingw32 \
			CC='x86_64-w64-mingw32-gcc' \
			TO_BIN='lua.exe luac.exe' \
			TO_LIB='liblua.a lua54.dll' \
		)
		make "${JOBS}" "${LUA_MAKEFILE_ARGS[@]}" >> "${COMPILE_LOG}" 2>&1
		if [ "$?" != "0" ]; then
			echo "Failed! Check compile_${DIRECTORY}.log"
			continue
		fi
		make "${JOBS}" "${LUA_MAKEFILE_ARGS[@]}" install >> "${COMPILE_LOG}" 2>&1
		if [ "$?" != "0" ]; then
			echo "Failed! Check compile_${DIRECTORY}.log"
			continue
		fi
		ln -fs liblua.a /usr/x86_64-w64-mingw32/lib/liblua5.4.a
		PATH=$OLDPATH
		unset OLDPATH
	elif [ "${METHOD}" == "sdl-make" ]; then
		make "${JOBS}" cross CROSS_PATH=/usr >> "${COMPILE_LOG}" 2>&1
		if [ "$?" != "0" ]; then
			echo "Failed! Check compile_${DIRECTORY}.log"
			continue
		fi
		make "${JOBS}" install >> "${COMPILE_LOG}" 2>&1
		if [ "$?" != "0" ]; then
			echo "Failed! Check compile_${DIRECTORY}.log"
			continue
		fi
	elif [ "${METHOD}" == "curl-copy" ]; then
		cp -r bin/* /usr/x86_64-w64-mingw32/bin/ >> "${COMPILE_LOG}" 2>&1
		cp -r include/* /usr/x86_64-w64-mingw32/include/ >> "${COMPILE_LOG}" 2>&1
		cp -r lib/* /usr/x86_64-w64-mingw32/lib/ >> "${COMPILE_LOG}" 2>&1
	elif [ "${METHOD}" == "cmake" ]; then
		mkdir -p build64
		cd build64
		cmake \
			-G"Unix Makefiles" \
			-DCMAKE_TOOLCHAIN_FILE="${DEVEL_DIR}/toolchain64.cmake" \
			-DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
			.. >> "${COMPILE_LOG}" 2>&1
		if [ "$?" != "0" ]; then
			echo "Failed! Check compile_${DIRECTORY}.log"
			continue
		fi
		make "${JOBS}" install >> "${COMPILE_LOG}" 2>&1
		if [ "$?" != "0" ]; then
			echo "Failed! Check compile_${DIRECTORY}.log"
			continue
		fi
	elif [ "${METHOD}" == "automake" ]; then
		mkdir -p build64
		cd build64
		../configure \
			--prefix=/usr/x86_64-w64-mingw32/ \
			--build=x86_64-pc-linux-gnu \
			--host=x86_64-w64-mingw32 \
			--target=x86_64-w64-mingw32 >> "${COMPILE_LOG}" 2>&1
		if [ "$?" != "0" ]; then
			echo "Failed! Configure sources with CMake, check compile_${DIRECTORY}.log"
			continue
		fi
		make "${JOBS}" install >> "${COMPILE_LOG}" 2>&1
		if [ "$?" != "0" ]; then
			echo "Failed! Configure sources with CMake, check compile_${DIRECTORY}.log"
			continue
		fi
	fi
	popd >> "${COMPILE_LOG}" 2>&1
done
