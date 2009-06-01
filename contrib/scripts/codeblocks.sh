#!/bin/bash

#######################################################
# functions
#######################################################

check_error() 
{
	errormessage=${1}
	if [ $? -ne "0" ]; then
		echo ${errormessage}
		echo ${errormessage} >> ${LOGFILE_NAME} 2>&1
		exit 1
	fi
}

download_archive()
{
	baseurl=${1}
	filename=${2}
	targetname=${3}
	if [ -e "${DOWNLOAD_DIR}/${targetname}" ]
	then
		echo "${3} already exists"
	else
		echo "downloading ${3}..."
		pushd ${DOWNLOAD_DIR} > /dev/null
		${WGET} -q ${baseurl}${filename} -O ${targetname} >> ${LOGFILE_NAME} 2>&1
		popd > /dev/null
		check_error "Could not fetch ${baseurl}${filename}"
	fi
}

extract_archive_gz() 
{
	file=${1}
	target=${2}
	pushd "${target}"
	${TAR} -xzf "${DOWNLOAD_DIR}/${file}" >> ${LOGFILE_NAME} 2>&1
	popd
	check_error "Failed to extract ${file}"
}

extract_archive_bz2() 
{
	file=${1}
	target=${2}
	pushd "${target}"
	${TAR} -xjf "${DOWNLOAD_DIR}/${file}" >> ${LOGFILE_NAME} 2>&1
	popd
	check_error "Failed to extract ${file}"
}

extract_archive_zip() 
{
	file=${1}
	target=${2}
	pushd "${target}"
	${UNZIP} -o "${DOWNLOAD_DIR}/${file}" >> ${LOGFILE_NAME} 2>&1
	popd
	check_error "Failed to extract ${file}"
}

extract_archive_7z() 
{
	file=${1}
	target=${2}
	pushd "${target}"
	${UN7ZIP} x -y "${DOWNLOAD_DIR}/${file}" >> ${LOGFILE_NAME} 2>&1
	popd
	check_error "Failed to extract ${file}"
}

start_downloads()
{
	# using an older version here - as windres is buggy in higher versions. Latest tested version is 2.19.1
	download_archive http://downloads.sourceforge.net/mingw/ binutils-2.16.91-20060119-1.tar.gz binutils.tar.gz
	#download_archive http://downloads.sourceforge.net/mingw/ binutils-2.19.1-mingw32-bin.tar.gz binutils.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ mingwrt-3.15.2-mingw32-dev.tar.gz mingwrt.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ w32api-3.13-mingw32-dev.tar.gz w32api.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ gdb-6.8-mingw-3.tar.bz2 gdb.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ mingw32-make-3.81-20080326-3.tar.gz make.tar.gz

	download_archive http://downloads.sourceforge.net/mingw/ bash-3.1-MSYS-1.0.11-1.tar.bz2 msys-bash.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ bzip2-1.0.3-MSYS-1.0.11-1.tar.bz2 msys-bzip2.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ tar-1.19.90-MSYS-1.0.11-2-bin.tar.gz msys-tar.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2 msys-coreutils.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ findutils-4.3.0-MSYS-1.0.11-3-bin.tar.gz msys-findutils.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ MSYS-1.0.11-20090120-dll.tar.gz msys.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ msysCORE-1.0.11-20080826.tar.gz msys-core.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ autoconf-4-1-bin.tar.bz2 msys-autoconf.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ autoconf2.5-2.61-1-bin.tar.bz2 msys-autoconf2.5.tar.bz2 
 	download_archive http://downloads.sourceforge.net/mingw/ automake-3-1-bin.tar.bz2 msys-automake.tar.bz2
 	download_archive http://downloads.sourceforge.net/mingw/ automake1.10-1.10-1-bin.tar.bz2 msys-automake1.10.tar.bz2
 	
	download_archive http://downloads.sourceforge.net/tdm-gcc/ gcc-4.3.3-tdm-1-core.tar.gz gcc.tar.gz
	download_archive http://downloads.sourceforge.net/tdm-gcc/ gcc-4.3.3-tdm-1-g++.tar.gz g++.tar.gz

	download_archive http://downloads.sourceforge.net/gnuwin32/ freetype-2.3.5-1-lib.zip freetype.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ gettext-0.14.4-bin.zip gettext.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ jpeg-6b-4-lib.zip libjpeg.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ libiconv-1.9.2-1-lib.zip libiconv.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ libiconv-1.9.2-1-bin.zip libiconv-bin.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ libintl-0.14.4-lib.zip libintl.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ libpng-1.2.35-lib.zip libpng.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ pdcurses-2.6-lib.zip libpdcurses.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ libxml2-2.4.12-1-lib.zip libxml2.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ tiff-3.8.2-1-lib.zip libtiff.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ wget-1.11.4-1-bin.zip wget.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ zlib-1.2.3-lib.zip zlib.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ unzip-5.51-1-bin.zip unzip.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ openssl-0.9.8h-1-bin.zip openssl.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ gawk-3.1.6-1-bin.zip gawk.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ sed-4.2-bin.zip sed.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ regex-2.7-bin.zip regex.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ pcre-7.0-bin.zip pcre.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ grep-2.5.4-bin.zip grep.zip
	
	download_archive http://downloads.sourceforge.net/sevenzip/ 7za465.zip 7zip.zip

	download_archive http://curl.de-mirror.de/download/ libcurl-${CURL_VERSION}-win32-nossl.zip libcurl.zip

	download_archive http://www.libsdl.org/release/ SDL-devel-${SDL_VERSION}-mingw32.tar.gz sdl.tar.gz
	download_archive http://www.libsdl.org/projects/SDL_ttf/release/ SDL_ttf-devel-${SDL_TTF_VERSION}-VC8.zip sdl_ttf.zip
	download_archive http://www.libsdl.org/projects/SDL_mixer/release/ SDL_mixer-devel-${SDL_MIXER_VERSION}-VC8.zip sdl_mixer.zip
	download_archive http://www.libsdl.org/projects/SDL_image/release/ SDL_image-devel-${SDL_IMAGE_VERSION}-VC8.zip sdl_image.zip

	download_archive http://oss.netfarm.it/mplayer/pkgs/ libvorbis-mingw32-1.2.0-gcc45.tar.bz2 libvorbis.tar.bz2
	download_archive http://oss.netfarm.it/mplayer/pkgs/ libtheora-mingw32-1.1alpha1-gcc45.tar.bz2 libtheora.tar.bz2
	download_archive http://oss.netfarm.it/mplayer/pkgs/ xvidcore-mingw32-1.2.1-gcc45.tar.bz2 xvidcore.tar.bz2
	download_archive http://oss.netfarm.it/mplayer/pkgs/ libogg-mingw32-1.1.3-gcc45.tar.bz2 libogg.tar.bz2
	
#	download_archive http://downloads.xiph.org/releases/ogg/ libogg-1.1.3.tar.gz libogg.tar.gz
#	download_archive http://downloads.xiph.org/releases/ogg/ libvorbis-1.2.0.tar.gz libvorbis.tar.gz

	download_archive http://download.berlios.de/codeblocks/ wxmsw28u_gcc_cb_wx2810.7z codeblocks_gcc.7z
	download_archive http://download.berlios.de/codeblocks/ mingwm10_gcc421.7z codeblocks_mingw.7z
	download_archive http://download.berlios.de/codeblocks/ CB_20090601_rev5616_win32.7z codeblocks.7z

	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.18/ glib-dev_2.18.4-1_win32.zip glib-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.14/ gtk+-dev_2.14.7-1_win32.zip gtk+-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.22/ pango-dev_1.22.4-1_win32.zip pango-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/atk/1.24/ atk-dev_1.24.0-1_win32.zip atk-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ cairo-dev_1.8.6-1_win32.zip cairo-dev.zip

	download_archive http://mattn.ninex.info/download/ gtkglext-1.2.tar.gz gtkglext-dev.tar.gz

	download_archive http://subversion.tigris.org/files/documents/15/45600/ svn-win32-1.6.1.zip svn.zip

	download_archive http://www.libsdl.org/extras/win32/common/ directx-devel.tar.gz directx.tar.gz
}

extract_codeblocks()
{
	extract_archive_7z codeblocks_mingw.7z "${CODEBLOCKS_DIR}"
	extract_archive_7z codeblocks_gcc.7z "${CODEBLOCKS_DIR}"
	extract_archive_7z codeblocks.7z "${CODEBLOCKS_DIR}"
}

extract_mingw()
{
	extract_archive_gz binutils.tar.gz "${MINGW_DIR}"
	extract_archive_gz mingwrt.tar.gz "${MINGW_DIR}"
	extract_archive_gz w32api.tar.gz "${MINGW_DIR}"
	extract_archive_bz2 gdb.tar.bz2 "${MINGW_DIR}"
	extract_archive_gz gcc.tar.gz "${MINGW_DIR}"
	extract_archive_gz g++.tar.gz "${MINGW_DIR}"
	extract_archive_gz make.tar.gz "${MINGW_DIR}"
	extract_archive_bz2 msys-bash.tar.bz2 "${MINGW_DIR}"
	extract_archive_bz2 msys-bzip2.tar.bz2 "${MINGW_DIR}"
	extract_archive_gz msys-tar.tar.gz "${MINGW_DIR}"
	extract_archive_gz msys-findutils.tar.gz "${MINGW_DIR}"
	extract_archive_gz msys.tar.gz "${MINGW_DIR}"
	extract_archive_gz msys-core.tar.gz "${MINGW_DIR}"
	extract_archive_bz2 msys-autoconf.tar.bz2 "${MINGW_DIR}"
	extract_archive_bz2 msys-autoconf2.5.tar.bz2 "${MINGW_DIR}"
	extract_archive_bz2 msys-automake.tar.bz2 "${MINGW_DIR}"
	extract_archive_bz2 msys-automake1.10tar.bz2 "${MINGW_DIR}"
	extract_archive_bz2 msys-coreutils.tar.bz2 "${MINGW_DIR}"
	cp -R ${MINGW_DIR}/usr/local/* "${MINGW_DIR}"
	rm -rf ${MINGW_DIR}/usr/local
	cp -R ${MINGW_DIR}/coreutils*/* "${MINGW_DIR}"
	rm -rf ${MINGW_DIR}/coreutils*
}

extract_libs()
{
	extract_archive_zip zlib.zip "${MINGW_DIR}"
	extract_archive_zip libjpeg.zip "${MINGW_DIR}"
	extract_archive_zip libpng.zip "${MINGW_DIR}"
	extract_archive_zip libiconv-bin.zip "${MINGW_DIR}"
	extract_archive_zip libiconv.zip "${MINGW_DIR}"
	extract_archive_zip libintl.zip "${MINGW_DIR}"
	extract_archive_zip freetype.zip "${MINGW_DIR}"
	extract_archive_zip libtiff.zip "${MINGW_DIR}"
	extract_archive_zip gettext.zip "${MINGW_DIR}"
	extract_archive_gz directx.tar.gz "${MINGW_DIR}"
	extract_archive_zip libpdcurses.zip "${MINGW_DIR}"
	extract_archive_zip libxml2.zip "${MINGW_DIR}"
	cp -R ${MINGW_DIR}/include/libxml2/* ${MINGW_DIR}/include >> ${LOGFILE_NAME} 2>&1
	rm -r ${MINGW_DIR}/include/libxml2 >> ${LOGFILE_NAME} 2>&1
}

extract_libcurl()
{
	mkdir ${TEMP_DIR}/tmp >> ${LOGFILE_NAME} 2>&1
	extract_archive_zip libcurl.zip "${TEMP_DIR}/tmp"
	cp ${TEMP_DIR}/tmp/libcurl-${CURL_VERSION}/* -R ${MINGW_DIR} >> ${LOGFILE_NAME} 2>&1
	rm -rf ${TEMP_DIR}/tmp >> ${LOGFILE_NAME} 2>&1
}

extract_sdl()
{
	mkdir ${TEMP_DIR}/tmp >> ${LOGFILE_NAME} 2>&1
	extract_archive_zip sdl_mixer.zip "${TEMP_DIR}/tmp"
	extract_archive_zip sdl_ttf.zip "${TEMP_DIR}/tmp"
	extract_archive_zip sdl_image.zip "${TEMP_DIR}/tmp"
	extract_archive_gz sdl.tar.gz "${TEMP_DIR}/tmp"
	pushd ${TEMP_DIR}/tmp/SDL-${SDL_VERSION} > /dev/null
	make install-sdl prefix="${MINGW_DIR}" >> ${LOGFILE_NAME} 2>&1
	popd > /dev/null
	cp ${TEMP_DIR}/tmp/SDL_mixer-${SDL_MIXER_VERSION}/include/* ${MINGW_DIR}/include/SDL >> ${LOGFILE_NAME} 2>&1
	cp ${TEMP_DIR}/tmp/SDL_mixer-${SDL_MIXER_VERSION}/lib/* ${MINGW_DIR}/lib >> ${LOGFILE_NAME} 2>&1
	cp ${TEMP_DIR}/tmp/SDL_ttf-${SDL_TTF_VERSION}/include/* ${MINGW_DIR}/include/SDL >> ${LOGFILE_NAME} 2>&1
	cp ${TEMP_DIR}/tmp/SDL_ttf-${SDL_TTF_VERSION}/lib/* ${MINGW_DIR}/lib >> ${LOGFILE_NAME} 2>&1
	cp ${TEMP_DIR}/tmp/SDL_image-${SDL_IMAGE_VERSION}/include/* ${MINGW_DIR}/include/SDL >> ${LOGFILE_NAME} 2>&1
	cp ${TEMP_DIR}/tmp/SDL_image-${SDL_IMAGE_VERSION}/lib/* ${MINGW_DIR}/lib >> ${LOGFILE_NAME} 2>&1
	rm -rf ${TEMP_DIR}/tmp >> ${LOGFILE_NAME} 2>&1
}

extract_tools()
{
	mkdir ${TEMP_DIR}/tmp >> ${LOGFILE_NAME} 2>&1
	extract_archive_zip wget.zip "${MINGW_DIR}"
	extract_archive_zip unzip.zip "${MINGW_DIR}"
	extract_archive_zip openssl.zip "${MINGW_DIR}"
	extract_archive_zip gawk.zip "${MINGW_DIR}"
	extract_archive_zip sed.zip "${MINGW_DIR}"
	extract_archive_zip regex.zip "${MINGW_DIR}"
	extract_archive_zip pcre.zip "${MINGW_DIR}"
	extract_archive_zip grep.zip "${MINGW_DIR}"
	extract_archive_zip 7zip.zip "${TEMP_DIR}/tmp"
	extract_archive_zip svn.zip "${TEMP_DIR}/tmp"
	#some parts of openssl are also included in the svn package
	cp ${TEMP_DIR}/tmp/7za.exe ${MINGW_DIR}/bin/7z.exe >> ${LOGFILE_NAME} 2>&1
	cp ${TEMP_DIR}/tmp/7za.exe ${MINGW_DIR}/bin/7za.exe >> ${LOGFILE_NAME} 2>&1
	cp -R ${TEMP_DIR}/tmp/svn-win32*/* ${MINGW_DIR} >> ${LOGFILE_NAME} 2>&1
	rm -rf ${TEMP_DIR}/tmp >> ${LOGFILE_NAME} 2>&1
}

extract_gtk()
{
	extract_archive_zip glib-dev.zip "${MINGW_DIR}"
	extract_archive_zip gtk+-dev.zip "${MINGW_DIR}"
	extract_archive_zip pango-dev.zip "${MINGW_DIR}"
	extract_archive_zip atk-dev.zip "${MINGW_DIR}"
	extract_archive_zip cairo-dev.zip "${MINGW_DIR}"
	extract_archive_gz gtkglext-dev.tar.gz "${MINGW_DIR}"

	for i in $(echo "glib-2.0 gail-1.0 gtk-2.0 pango-1.0 atk-1.0 cairo gtkglext-1.0 gtkglextmm-1.2"); do
		cp -R ${MINGW_DIR}/include/${i}/* ${MINGW_DIR}/include >> ${LOGFILE_NAME} 2>&1
		rm -r ${MINGW_DIR}/include/${i} >> ${LOGFILE_NAME} 2>&1
		cp -R ${MINGW_DIR}/lib/${i}/include/* ${MINGW_DIR}/include >> ${LOGFILE_NAME} 2>&1
		rm -r ${MINGW_DIR}/lib/${i} >> ${LOGFILE_NAME} 2>&1
	done
}

extract_ogg()
{
	extract_archive_bz2 libogg.tar.bz2 "${MINGW_DIR}"
	extract_archive_bz2 xvidcore.tar.bz2 "${MINGW_DIR}"
	extract_archive_bz2 libtheora.tar.bz2 "${MINGW_DIR}"
	extract_archive_bz2 libvorbis.tar.bz2 "${MINGW_DIR}"
}

#######################################################
# variables
#######################################################

# Some packages are using the version information at several places
# because we have to copy some files around
SDL_VERSION=1.2.13
SDL_MIXER_VERSION=1.2.8
SDL_TTF_VERSION=2.0.9
SDL_IMAGE_VERSION=1.2.6
CURL_VERSION=7.16.4

TEMP_DIR=$(pwd)/tmp_codeblocks
TARGET_DIR=${TEMP_DIR}/codeblocks
DOWNLOAD_DIR=${TEMP_DIR}/downloads
MINGW_DIR=${TARGET_DIR}/MinGW
CODEBLOCKS_DIR=${TARGET_DIR}
ARCHIVE_NAME=codeblocks.zip
LOGFILE_NAME=codeblocks.log
UN7ZIP=$(which 7z)
WGET=$(which wget)
UNZIP=$(which unzip)
TAR=$(which tar)

if [ ! -x "${UN7ZIP}" ]; then
	echo "you need 7zip installed to run this script"
	exit 1;
fi

if [ ! -x "${WGET}" ]; then
	echo "you need wget installed to run this script"
	exit 1;
fi

if [ ! -x "${UNZIP}" ]; then
	echo "you need unzip installed to run this script"
	exit 1;
fi

if [ ! -x "${TAR}" ]; then
	echo "you need tar installed to run this script"
	exit 1;
fi

#######################################################
# don't change anything below here
#######################################################

create()
{
	mkdir -p "${TEMP_DIR}"
	rm -rf "${TARGET_DIR}"
	mkdir -p "${TARGET_DIR}"
	mkdir -p "${CODEBLOCKS_DIR}"
	mkdir -p "${MINGW_DIR}"
	mkdir -p "${DOWNLOAD_DIR}"
	echo $(date) > ${LOGFILE_NAME}

	start_downloads

	echo "Start to extract and prepare the downloaded archives"

	extract_codeblocks

	extract_mingw

	extract_libs

	extract_sdl

	extract_libcurl

	extract_gtk

	extract_ogg

	extract_tools

	${UN7ZIP} a -tzip -mx=9 ${ARCHIVE_NAME} ${CODEBLOCKS_DIR}

	if [ ! -e "${ARCHIVE_NAME}" ]; then
		echo "Failed to create ${ARCHIVE_NAME}"
		exit 1;
	fi
	echo $(date) >> ${LOGFILE_NAME} 2>&1
	echo "finished creating ${ARCHIVE_NAME} in $(pwd)"
}

download() 
{
	mkdir -p "${TEMP_DIR}"
	mkdir -p "${TARGET_DIR}"
	mkdir -p "${CODEBLOCKS_DIR}"
	mkdir -p "${MINGW_DIR}"
	mkdir -p "${DOWNLOAD_DIR}"

	start_downloads
}

ACTION=${1:-help}

if [ "$ACTION" == "create" ]; then
	create
elif [ "$ACTION" == "download" ]; then
	download
elif [ "$ACTION" == "help" ]; then
	echo "Valid parameters are: help, create and clean"
	echo " - create   : creates the ${ARCHIVE_NAME} with the latest versions"
	echo "              of the needed programs and libs"
	echo " - download : only performs the downloading"
	echo " - clean    : removes the downloads and the temp dirs - but not"
	echo "              the created archive file"
	echo " - help     : prints this help message"
	echo " - upload   : uploads the created archive - ./ssh/config must be"
	echo "              set up for this to work"
elif [ "$ACTION" == "upload" ]; then
	if [ ! -e "${ARCHIVE_NAME}" ]; then
		echo "${ARCHIVE_NAME} doesn't exist. Hit any key to create it."
		read
		create
	fi
	scp ${ARCHIVE_NAME} ufo:~/public_html/download
elif [ "$ACTION" == "clean" ]; then
	rm -rf "${TEMP_DIR}"
	echo "clean finished"
else
	echo "unknown action given"
fi
