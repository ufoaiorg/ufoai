#!/bin/bash

#######################################################
# functions
#######################################################

download()
{
	if [ -e "${DOWNLOAD_DIR}/$3" ]
	then
		return 0
	else
		mkdir -p ${DOWNLOAD_DIR}
		pushd ${DOWNLOAD_DIR}
		${WGET} $1$2 -O $3
		popd
		return $?
	fi
}

start_downloads()
{
	download http://downloads.sourceforge.net/mingw/ binutils-2.18.50-20080109-2.tar.gz binutils.tar.gz
	download http://downloads.sourceforge.net/mingw/ mingwrt-3.15.1-mingw32-dev.tar.gz mingwrt.tar.gz
	download http://downloads.sourceforge.net/mingw/ w32api-3.12-mingw32-dev.tar.gz w32api.tar.gz
	download http://downloads.sourceforge.net/mingw/ mingw32-make-3.81-20080326-3.tar.gz mingw32.tar.gz
	download http://downloads.sourceforge.net/mingw/ gdb-6.8-mingw-3.tar.bz2 gdb.tar.bz2

	download http://downloads.sourceforge.net/tdm-gcc/ gcc-4.3.2-tdm-1-core.tar.gz gcc.tar.gz
	download http://downloads.sourceforge.net/tdm-gcc/ gcc-4.3.2-tdm-1-g++.tar.gz g++.tar.gz

	download http://downloads.sourceforge.net/sourceforge/gnuwin32/ zlib-1.2.3-lib.zip zlib.zip
	download http://downloads.sourceforge.net/sourceforge/gnuwin32/ jpeg-6b-4-lib.zip libjpeg.zip
	download http://downloads.sourceforge.net/sourceforge/gnuwin32/ libpng-1.2.33-lib.zip libpng.zip
	download http://downloads.sourceforge.net/sourceforge/gnuwin32/ libiconv-1.9.2-1-lib.zip libiconv.zip
	download http://downloads.sourceforge.net/sourceforge/gnuwin32/ libintl-0.14.4-lib.zip libintl.zip
	download http://downloads.sourceforge.net/sourceforge/gnuwin32/ freetype-2.3.5-lib.zip freetype.zip
	download http://downloads.sourceforge.net/sourceforge/gnuwin32/ wget-1.11.4-1-bin.zip wget.zip

	# changing version of libcurl might also require minor fixes in extract_libcurl
	download http://curl.de-mirror.de/download/ libcurl-7.16.4-win32-nossl.zip libcurl.zip

	# changing versions of the sdl libs might also require minor fixes in extract_sdl
	download http://www.libsdl.org/release/ SDL-devel-1.2.13-mingw32.tar.gz sdl.tar.gz
	download http://www.libsdl.org/projects/SDL_ttf/release/ SDL_ttf-devel-2.0.9-VC8.zip sdl_ttf.zip
	download http://www.libsdl.org/projects/SDL_mixer/release/ SDL_mixer-devel-1.2.8-VC8.zip sdl_mixer.zip
	download http://www.libsdl.org/projects/SDL_image/release/ SDL_image-devel-1.2.6-VC8.zip sdl_image.zip

#	download http://downloads.xiph.org/releases/ogg/ libogg-1.1.3.tar.gz libogg.tar.gz
#	download http://downloads.xiph.org/releases/ogg/ libvorbis-1.2.0.tar.gz libvorbis.tar.gz

	download http://download.berlios.de/codeblocks/ wxmsw28u_gcc_cb_wx289.7z codeblocks_gcc.7z
	download http://download.berlios.de/codeblocks/ mingwm10_gcc421.7z codeblocks_mingw.7z
	download http://download.berlios.de/codeblocks/ CB_20090214_rev5456_win32.7z codeblocks.7z
}

extract_codeblocks()
{
	${UN7ZIP} x -y -o${CODEBLOCKS_DIR} ${DOWNLOAD_DIR}/codeblocks_mingw.7z
	${UN7ZIP} x -y -o${CODEBLOCKS_DIR} ${DOWNLOAD_DIR}/codeblocks_gcc.7z
	${UN7ZIP} x -y -o${CODEBLOCKS_DIR} ${DOWNLOAD_DIR}/codeblocks.7z
}

extract_mingw()
{
	${TAR} -xzf ${DOWNLOAD_DIR}/binutils.tar.gz -C ${MINGW_DIR}
	${TAR} -xzf ${DOWNLOAD_DIR}/mingwrt.tar.gz -C ${MINGW_DIR}
	${TAR} -xzf ${DOWNLOAD_DIR}/w32api.tar.gz -C ${MINGW_DIR}
	${TAR} -xzf ${DOWNLOAD_DIR}/mingw32.tar.gz -C ${MINGW_DIR}
	${TAR} -xjf ${DOWNLOAD_DIR}/gdb.tar.bz2 -C ${MINGW_DIR}
	${TAR} -xzf ${DOWNLOAD_DIR}/gcc.tar.gz -C ${MINGW_DIR}
	${TAR} -xzf ${DOWNLOAD_DIR}/g++.tar.gz -C ${MINGW_DIR}
}

extract_libs()
{
	${UNZIP} -o ${DOWNLOAD_DIR}/zlib.zip -d ${MINGW_DIR}
	${UNZIP} -o ${DOWNLOAD_DIR}/libjpeg.zip -d ${MINGW_DIR}
	${UNZIP} -o ${DOWNLOAD_DIR}/libpng.zip -d ${MINGW_DIR}
	${UNZIP} -o ${DOWNLOAD_DIR}/libiconv.zip -d ${MINGW_DIR}
	${UNZIP} -o ${DOWNLOAD_DIR}/libintl.zip -d ${MINGW_DIR}
	${UNZIP} -o ${DOWNLOAD_DIR}/freetype.zip -d ${MINGW_DIR}
}

extract_libcurl()
{
	mkdir ${TEMP_DIR}/tmp
	${UNZIP} -o ${DOWNLOAD_DIR}/libcurl.zip -d ${TEMP_DIR}/tmp
	cp ${TEMP_DIR}/tmp/libcurl-7.16.4/* -R ${MINGW_DIR}
	rm -rf ${TEMP_DIR}/tmp
}

extract_sdl()
{
	mkdir ${TEMP_DIR}/tmp
	${UNZIP} -o ${DOWNLOAD_DIR}/sdl_mixer.zip -d ${TEMP_DIR}/tmp
	${UNZIP} -o ${DOWNLOAD_DIR}/sdl_ttf.zip -d ${TEMP_DIR}/tmp
	${UNZIP} -o ${DOWNLOAD_DIR}/sdl_image.zip -d ${TEMP_DIR}/tmp
	${TAR} -xzf ${DOWNLOAD_DIR}/sdl.tar.gz -C ${TEMP_DIR}/tmp
	pushd ${TEMP_DIR}/tmp/SDL-1.2.13
	make install-sdl prefix=${MINGW_DIR}
	popd
	cp ${TEMP_DIR}/tmp/SDL_mixer-1.2.8/include/* ${MINGW_DIR}/include/SDL
	cp ${TEMP_DIR}/tmp/SDL_mixer-1.2.8/lib/* ${MINGW_DIR}/lib
	cp ${TEMP_DIR}/tmp/SDL_ttf-2.0.9/include/* ${MINGW_DIR}/include/SDL
	cp ${TEMP_DIR}/tmp/SDL_ttf-2.0.9/lib/* ${MINGW_DIR}/lib
	cp ${TEMP_DIR}/tmp/SDL_image-1.2.6/include/* ${MINGW_DIR}/include/SDL
	cp ${TEMP_DIR}/tmp/SDL_image-1.2.6/lib/* ${MINGW_DIR}/lib
	rm -rf ${TEMP_DIR}/tmp
}

extract_tools()
{
	# wget
	mkdir ${TEMP_DIR}/tmp
	${UNZIP} -o ${DOWNLOAD_DIR}/wget.zip -d ${TEMP_DIR}/tmp
	cp ${TEMP_DIR}/tmp/bin/wget.exe ${MINGW_DIR}/bin
	rm -rf ${TEMP_DIR}/tmp
}

#######################################################
# variables
#######################################################

TEMP_DIR=$(pwd)/codeblocks_tmp
TARGET_DIR=${TEMP_DIR}/codeblocks
DOWNLOAD_DIR=${TEMP_DIR}/downloads
MINGW_DIR=${TARGET_DIR}/MinGW
CODEBLOCKS_DIR=${TARGET_DIR}
ARCHIVE_NAME=codeblocks.zip
UN7ZIP=$(which 7z)
WGET=$(which wget)
UNZIP=$(which unzip)
TAR=$(which tar)

if [ ! -e ${UN7ZIP} ]; then
	echo "you need 7zip installed to run this script"
	exit 1;
fi

if [ ! -e ${WGET} ]; then
	echo "you need wget installed to run this script"
	exit 1;
fi

if [ ! -e ${UNZIP} ]; then
	echo "you need unzip installed to run this script"
	exit 1;
fi

if [ ! -e ${TAR} ]; then
	echo "you need tar installed to run this script"
	exit 1;
fi

#######################################################
# don't change anything below here
#######################################################

create()
{
	mkdir -p ${TEMP_DIR}
	rm -rf ${TARGET_DIR}
	mkdir -p ${TARGET_DIR}
	mkdir -p ${CODEBLOCKS_DIR}
	mkdir -p ${MINGW_DIR}

	start_downloads

	extract_codeblocks

	extract_mingw

	extract_libs

	extract_sdl

	extract_libcurl

	extract_tools

	${UN7ZIP} a -tzip -mx=9 ${ARCHIVE_NAME} ${CODEBLOCKS_DIR}

	if [ ! -e "${ARCHIVE_NAME}" ]; then
		echo "Failed to create ${ARCHIVE_NAME}"
		exit 1;
	fi
	echo "finished creating ${ARCHIVE_NAME} in $(pwd)"
}

ACTION=${1:-help}

if [ "$ACTION" == "create" ]; then
	create
elif [ "$ACTION" == "help" ]; then
	echo "Valid parameters are: help, create and clean"
	echo " - create  : creates the ${ARCHIVE_NAME} with the latest versions"
	echo "             of the needed programs and libs"
	echo " - clean   : removes the downloads and the temp dirs - but not"
	echo "             the created archive file"
	echo " - help    : prints this help message"
	echo " - upload  : uploads the created archive - ./ssh/config must be"
	echo "             set up for this to work"
elif [ "$ACTION" == "upload" ]; then
	if [ ! -e "${ARCHIVE_NAME}" ]; then
		echo "${ARCHIVE_NAME} doesn't exist. Hit any key to create it."
		read
		create
	fi
	scp ${ARCHIVE_NAME} ufo:~/public_html/downloads
elif [ "$ACTION" == "clean" ]; then
	rm -rf ${TEMP_DIR}
	echo "clean finished"
else
	echo "unknown action given"
fi
