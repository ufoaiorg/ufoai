#!/bin/bash

## activate both for error checking
#set -e
#set -x

###################################################
# signal handlers
###################################################
trap exitscript SIGINT

#######################################################
# variables
#######################################################


ACTION=${1:-help}

# Some packages are using the version information at several places
# because we have to copy some files around
SDL_VERSION=1.2.14
SDL_MIXER_VERSION=1.2.8
SDL_TTF_VERSION=2.0.9
SDL_IMAGE_VERSION=1.2.6
CURL_VERSION=7.16.4
#//	CURL_VERSION=7.17.1
CUNIT_VERSION=2.1-0

# Use an absolute path here
TEMP_DIR=/tmp/tmp_codeblocks
TARGET_DIR=${TEMP_DIR}/UFOAIwin32BUILDenv
DOWNLOAD_DIR=${TEMP_DIR}/downloads
EXTRACT_DIR=${DOWNLOAD_DIR}/extract
if [ "$ACTION" = "update" ]; then
	MINGW_DIR=/
else
	MINGW_DIR=${TARGET_DIR}/MinGW
fi
CODEBLOCKS_DIR=${TARGET_DIR}
NSIS_DIR=${MINGW_DIR}/NSIS
CB_DIR=${TARGET_DIR}/codeblocks
#//	ARCHIVE_NAME=codeblocks
LOGFILE_NAME=codeblocks.log
UN7ZIP=$(which 7za)
WGET=$(which wget)
UNZIP=$(which unzip)
TAR=$(which tar)
sfx7ZIP=$(which 7z.sfx)
environment_ver=0.3.2

if [ -x "${sfx7ZIP}" ]; then
	ARCHIVE_NAME=UfoAI_win32_build_environment_${environment_ver}.exe
else
	ARCHIVE_NAME=UfoAI_win32_build_environment_${environment_ver}.7z
fi

export LC_ALL=C



# Most packages are somewhat "fragmented" /include folders with subfolders .....
# to find include folders the "header search and move/copy algo" start a search for it ( some packages store headers inside of libs too )
# but on one gcc package the include folder inside off libs should'nt be moved
# this will exclude files or folders from this search
declare exclude_from_find_result="-not -path \"*/lib/gcc/mingw32/*\""

# just a var to store some stdout output
declare what_do_we_try

# hold the folder structure of mingw/include for the "header search and move/copy algo"
declare	include_array=(	"AL"\
			"atk"\
			"CUnit"\
			"curl"\
			"ddk"\
			"fontconfig"\
			"freetype2"\
			"gail"\
			"gdk"\
			"gdk-pixbuf"\
			"gio"\
			"GL"\
			"glib"\
			"gobject"\
			"gtk"\
			"gtksourceview"\
			"libgail-util"\
			"libpng14"\
			"libxml"\
			"ncurses"\
			"ogg"\
			"pango"\
			"SDL"\
			"sys"\
			"theora"\
			"vorbis" )






#######################################################
# functions
#######################################################

function infooutput()
{
	magentaecho "${1}"
	what_do_we_try="${1}"
}

function redecho() {
	echo -e "\033[1;31m${1}\033[0m"
}

function whiteecho() {
	echo -e "\033[1;37m${1}\033[0m"
}

function magentaecho() {
	echo -e "\033[1;35m${1}\033[0m"
}

function greenecho() {
	echo -e "\033[1;32m${1}\033[0m"
}

function failure()
{
	magentaecho "${2}"
	redecho "${1}"
	echo -e "tried to do: ${2}\nFailed: ${1}"
	echo -e "tried to do: ${2}\nFailed: ${1}" >> ${LOGFILE_NAME} 2>&1
	exit 1
}

function check_error()
{
	error=${1}
	errormessage=${2}
	if [ $error -ne 0 ]; then
		failure "Error: ${errormessage}"
	fi
}









create()
{
	mkdir -p "${TEMP_DIR}"
	rm -rf "${TARGET_DIR}"
	mkdir -p "${TARGET_DIR}"
	mkdir -p "${CODEBLOCKS_DIR}"
	mkdir -p "${MINGW_DIR}"
	mkdir -p "${DOWNLOAD_DIR}"
	mkdir -p "${CB_DIR}"
	mkdir -p "${NSIS_DIR}"
	echo $(date) > ${LOGFILE_NAME}

	start_downloads

	correctme

	echo -n $environment_ver>"${MINGW_DIR}/bin/WINenvVER"

	echo -n "Used space in ${CODEBLOCKS_DIR}: "
	echo $(du -h -c ${CODEBLOCKS_DIR} | tail -1)

	infooutput "Start to create ${ARCHIVE_NAME}"

	if [ -x "${sfx7ZIP}" ]; then
		rm -f ~/${ARCHIVE_NAME}
		echo ${UN7ZIP} a -t7z -mx=9 -sfx7z.sfx ~/${ARCHIVE_NAME} "${CODEBLOCKS_DIR}"
		${UN7ZIP} a -t7z -mx=9 -sfx7z.sfx ~/${ARCHIVE_NAME} "${CODEBLOCKS_DIR}" || failure "cant create ${ARCHIVE_NAME}" "$what_do_we_try"
	else
		rm -f ~/${ARCHIVE_NAME}
		echo ${UN7ZIP} a -t7z -mx=9 ~/${ARCHIVE_NAME} "${CODEBLOCKS_DIR}"
		${UN7ZIP} a -t7z -mx=9 ~/${ARCHIVE_NAME} "${CODEBLOCKS_DIR}" || failure "cant create ${ARCHIVE_NAME}" "$what_do_we_try"
	fi
	greenecho "codeblock created\nFINISHED"

	echo $(date) >> ${LOGFILE_NAME} 2>&1
	echo "finished creating $(du -h ~/${ARCHIVE_NAME})"
}

download()
{
	mkdir -p "${TEMP_DIR}"
	mkdir -p "${TARGET_DIR}"
	mkdir -p "${CODEBLOCKS_DIR}"
	mkdir -p "${CB_DIR}"
	mkdir -p "${NSIS_DIR}"
	mkdir -p "${MINGW_DIR}"
	mkdir -p "${DOWNLOAD_DIR}"

	start_downloads
}

update()
{
	mkdir -p "${DOWNLOAD_DIR}"

	infooutput "try to find WINenvVER"
	local cur_env=$(cat /bin/WINenvVER)

	if [ -z "$cur_env" ] ;then
		redecho "I'm unable to determine your MinGW version you are on\nUse the switch create instead"
		exit 1
	fi
	if [ -z "$(echo $cur_env | grep '^0.3.')" ] ;then
		redecho "Your MinGW version is $cur_env\nBut i can only update 0.3.x\nUse the switch create instead"
		exit 1
	fi
	if [ "$cur_env" = "0.3.0" ] ;then
		redecho "Your MinGW version is 0.3.0\nNo way to update\nUse the switch create instead"
		exit 1
	fi
	if [ "$cur_env" = "0.3.1" ] ;then
		download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ UfoAI_win32_build_environment_${environment_ver}_update.7z UfoAI_win32_build_environment.7z

		rm -f /lib/libcunit_dll.a
		rm -f /bin/cunit.dll

		cur_env="0.3.2"
		echo -n $cur_env>"/bin/WINenvVER"
	fi
	greenecho "finished"
}

function download_archive()
{
	baseurl=${1}
	filename=${2}
	targetname=${3}

	local what_do_we_try="downloading ${filename}..."
	magentaecho "$what_do_we_try"

	for ((a=0; a<=25; a++))
	do
		# if the mirror is down get another mirror
		sleep 1
		${WGET} -c --tries=1 --timeout=30 --progress=bar:force -O "${DOWNLOAD_DIR}/${targetname}" ${baseurl}${filename} && break
		if [ ${a} -eq 5 ] ;then
			failure "Could not fetch ${baseurl}${filename}" "$what_do_we_try"
		fi
	done
	greenecho "${filename} successfully downloaded"

	extract ${3}
}

function extract()
{
	infooutput "test for extraction ${1}"

	rm -rf "${EXTRACT_DIR}"
	${UN7ZIP} l "${DOWNLOAD_DIR}/${1}" || failure "${1} cant be extracted" "$what_do_we_try"
	greenecho "a 7z extractable archive"

	infooutput "extracting ${1}"

	${UN7ZIP} x -y -o"${EXTRACT_DIR}" "${DOWNLOAD_DIR}/${1}" || failure "unable to extract ${1}" "$what_do_we_try"
	greenecho "${1} extracted"

	local isTAR=$(find "${EXTRACT_DIR}" -maxdepth 1 -iname "*.tar" -type f)
	echo "[$isTAR]"
	if [ -n "$isTAR" ] ;then
		infooutput "extracting $isTAR"

		${UN7ZIP} x -y -o"${EXTRACT_DIR}" "$isTAR" || failure "unable to extract $isTAR" "$what_do_we_try"
		rm -f "$isTAR"
		greenecho "$isTAR extracted"
	fi


	if [ $(echo ${1} | grep -i -c -e "^codeblocks" -e "^NSIS.zip$" -e "^Python.7z$" -e "^make_UfoAI_win32.7z$") -eq 0 ] ;then
		infooutput "try to find usable directories"

		for ((a=0; a<=25; a++))
		do
			local find_output=$(find "${EXTRACT_DIR}" -maxdepth ${a} -type d -iname "bin" -o -iname "contrib" -o -iname "doc" -o -iname "etc" -o -iname "home" -o -iname "include" -o -iname "info" -o -iname "lib" -o -iname "libexec" -o -iname "man" -o -iname "manifest" -o -iname "mingw32" -o -iname "share")
			if [ -n "$find_output" ] ;then
				greenecho "found usable directories"








				# header search and move/copy algo

#				if [ -d "${EXTRACT_DIR}/include" ] ;then	# if there is a ${EXTRACT_DIR}/include folder
#					local find_include=$(find "${EXTRACT_DIR}" -iname "include" -type d )	# search for include subfolders; /libs/glib-2.0/include
#					if [ -n "$find_include" ] ;then
#						local -a find_include_array
#						find_include_array=(`echo -e "$find_include" | tr '\n' ' '`)
#						for find_include_array_element in ${find_include_array[@]}
#						do
#							infooutput "found include folder $find_include_array_element"
#
#							local find_include_subdirs=$(eval "find "$find_include_array_element" $exclude_from_find_result -iname "*.h" -type f -exec dirname {} \; | sort | uniq") # look for header files inside the include folder, but exclude things from var exclude_from_find_result
#							if [ -n "$find_include_subdirs" ] ;then
#								local -a find_include_subdirs_array
#								find_include_subdirs_array=(`echo -e "$find_include_subdirs" | tr '\n' ' '`)
#
#								for find_include_subdirs_array_element in ${find_include_subdirs_array[@]}
#								do
#									infooutput "found include subdirs $find_include_subdirs_array_element"
#
#									if [ "$find_include_subdirs_array_element" != "${EXTRACT_DIR}/include" ] ;then	# if the header files are not inside of ..../include ; in that case we dont need to move things
#										if [ -d "$find_include_subdirs_array_element" ] ;then
#											if [ "$find_include_subdirs_array_element" = "$find_include_array_element" ] ;then	# if the headers are inside of /libs/glib-2.0/include ,and not /include/glib-2.0/gtk
#												infooutput "cp -vR \"$find_include_subdirs_array_element/\"* \"${EXTRACT_DIR}/include\""
#
#												cp -vR "$find_include_subdirs_array_element/"* "${EXTRACT_DIR}/include" || failure "unable to move $find_include_subdirs_array_element" "$what_do_we_try"
#												rm -rf "$find_include_subdirs_array_element"
#												find "${EXTRACT_DIR}" -depth -type d -empty -exec rmdir {} \;
#
#												greenecho "\"$find_include_subdirs_array_element\" moved"
#											else
#												for (( i=0; i<${#include_array[@]}; i++ )) #include_array_element in ${include_array[@]} # hold dirs that must be at mingw/include
#												do
#										whiteecho "${i} ${include_array[${i}]}$"
#													if [ $( echo $find_include_subdirs_array_element | grep -ice "${include_array[${i}]}$" ) -gt 0 ] ;then # if the subfolder inside of /somwhere/include must stay at mingw/include
#														if [ $(readlink -f "$find_include_subdirs_array_element/..") != "${EXTRACT_DIR}/include" ] ;then # to be shure the directory is'nt copied on itself
#															infooutput "mv -vf \"$find_include_subdirs_array_element\" \"${EXTRACT_DIR}/include\""
#
#															mv -vf "$find_include_subdirs_array_element" "${EXTRACT_DIR}/include" || failure "unable to move $find_include_subdirs_array_element" "$what_do_we_try"
#															rm -rf "$find_include_subdirs_array_element"
#															find "${EXTRACT_DIR}" -depth -type d -empty -exec rmdir {} \;
#
#															greenecho "\"$find_include_subdirs_array_element\" moved"
#														fi
#														break
#													fi
#													if [ $(((i+1))) -eq ${#include_array[@]} ] ;then	# if the folder is not part of mingw/include like /include/cairo then move all things from that folder at mingw/include
#														infooutput "mv -vf \"$find_include_subdirs_array_element/\"* \"${EXTRACT_DIR}/include\""
#
#														mv -vf "$find_include_subdirs_array_element/"* "${EXTRACT_DIR}/include" || failure "unable to move $find_include_subdirs_array_element" "$what_do_we_try"
#														rm -rf "$find_include_subdirs_array_element"
#														find "${EXTRACT_DIR}" -depth -type d -empty -exec rmdir {} \;
#
#														greenecho "\"$find_include_subdirs_array_element/\"* moved"
#													fi
#												done
#											fi
#										fi
#									else
#										break
#									fi
#								done
#							fi
#						done
#					fi
#					find "${EXTRACT_DIR}" -depth -type d -empty -exec rmdir {} \;
#				fi
				# END header search and move/copy algo






				infooutput "copy usable directories"

				local -a array
				array=(`echo -e "$find_output" | tr '\n' ' '`)

				for s in ${array[@]}
				do
					if [ -d "$s" ] ;then
						cp -v -R "$s" "${MINGW_DIR}" || failure "unable to copy $s" "$what_do_we_try"
						greenecho "$s copied"
					fi
				done
				break
			fi
			if [ ${a} -eq 25 ] ;then
				failure "cant find usable directories" "$what_do_we_try"
			fi
		done
	else
		infooutput "found codeblocks or NSIS or Python or make_UfoAI_win32 archive"

		if [ $(echo ${1} | grep -i -c -e "^codeblocks") -gt 0 ] ;then
			infooutput "try to copy codeblocks files"

			cp -v -R "${EXTRACT_DIR}/"* "${CB_DIR}" || failure "unable to copy codeblocks files" "$what_do_we_try"
			greenecho "codeblocks copied"
		fi
#		if [ $(echo ${1} | grep -i -c -e "^7zip") -gt 0 ] ;then
#			infooutput "try to copy 7zip files"
#
#			if [ -e "${EXTRACT_DIR}/7za.exe" ] ;then
#				cp -v -R "${EXTRACT_DIR}/7za.exe" "${MINGW_DIR}/bin" || failure "unable to copy 7za.exe" "$what_do_we_try"
#				greenecho "7za.exe copied"
#			else
#				failure "unable to find 7za.exe" "$what_do_we_try"
#			fi
#			if [ -e "${EXTRACT_DIR}/7zS.sfx" ] ;then
#				cp -v -R "${EXTRACT_DIR}/7zS.sfx" "${MINGW_DIR}/bin" || failure "unable to copy 7zS.sfx" "$what_do_we_try"
#				greenecho "7zS.sfx copied"
#			else
#				failure "unable to find 7zS.sfx" "$what_do_we_try"
#			fi
#		fi
		if [ $(echo ${1} | grep -i -c -e "^NSIS.zip$") -gt 0 ] ;then
			infooutput "try to copy NSIS files"

			local makenspath=$(find "${EXTRACT_DIR}" -iname "makensisw.exe" -type f | sed 's/makensisw.exe//gI')
			if [ -n "$makenspath" ] ;then
				cp -v -R "$makenspath"* "${NSIS_DIR}" || failure "unable to copy NSIS files" "$what_do_we_try"
				greenecho "NSIS copied"
			else
				failure "unable to find makensisw.exe" "$what_do_we_try"
			fi
		fi
#		if [ $(echo ${1} | grep -i -c -e "^rxvt.7z$") -gt 0 ] ;then
#			infooutput "try to copy rxvt files"
#
#			cp -v -R "${EXTRACT_DIR}/"* "${MINGW_DIR}" || failure "unable to copy rxvt files" "$what_do_we_try"
#			greenecho "rxvt copied"
#		fi
		if [ $(echo ${1} | grep -i -c -e "^Python.7z$") -gt 0 ] ;then
			infooutput "try to copy Python files"

			cp -v -R "${EXTRACT_DIR}/"* "${MINGW_DIR}" || failure "unable to copy Python" "$what_do_we_try"
			greenecho "make_UfoAI_win32 copied"
		fi
		if [ $(echo ${1} | grep -i -c -e "^make_UfoAI_win32.7z$") -gt 0 ] ;then
			infooutput "try to copy make_UfoAI_win32 files"

			cp -v -R "${EXTRACT_DIR}/"* "${CODEBLOCKS_DIR}" || failure "unable to copy make_UfoAI_win32" "$what_do_we_try"
			greenecho "make_UfoAI_win32 copied"
		fi
	fi
	sleep 1
#	rm -rf "${EXTRACT_DIR}"
}

function correctme()
{
	#here we do some correction for all possible things
	infooutput "doing some corrections if necessary"

	#glib-2.0
#	if [ -f "${MINGW_DIR}/include/glib-2.0/glib.h" ] ;then
#		infooutput "try to move header for glib-2.0"
#
#		mv -f -v "${MINGW_DIR}/include/glib-2.0/"* "${MINGW_DIR}/include" || failure "unable to move headers" "$what_do_we_try"
#		greenecho "headers moved"
#		rm -rf "${MINGW_DIR}/include/glib-2.0"
#	fi
}

function start_downloads()
{
	download_archive http://downloads.sourceforge.net/mingw/ autoconf2.5-2.67-1-mingw32-bin.tar.lzma autoconf-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ automake1.11-1.11.1-1-mingw32-bin.tar.lzma automake-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ bash-3.1.17-3-msys-1.0.13-bin.tar.lzma bash-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ binutils-2.20.51-1-mingw32-bin.tar.lzma binutils-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ bzip2-1.0.5-2-mingw32-bin.tar.gz bzip2-bin.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ bzip2-1.0.5-2-mingw32-dev.tar.gz bzip2-dev.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ coreutils-5.97-3-msys-1.0.13-bin.tar.lzma coreutils-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ coreutils-5.97-3-msys-1.0.13-ext.tar.lzma coreutils-ext.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ diffutils-2.8.7.20071206cvs-3-msys-1.0.13-bin.tar.lzma diffutils-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ file-5.04-1-msys-1.0.13-bin.tar.lzma file-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ findutils-4.4.2-2-msys-1.0.13-bin.tar.lzma findutils-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ gawk-3.1.7-2-msys-1.0.13-bin.tar.lzma gawk-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ gdb-7.2-1-mingw32-bin.tar.lzma gdb-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ gettext-0.17-1-mingw32-bin.tar.lzma gettext-bin.tar.lzma
#//	download_archive http://downloads.sourceforge.net/mingw/ gettext-0.17-1-mingw32-dev.tar.lzma gettext-dev.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ grep-2.5.4-2-msys-1.0.13-bin.tar.lzma grep-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ gzip-1.3.12-2-msys-1.0.13-bin.tar.lzma gzip-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ inetutils-1.7-1-msys-1.0.13-bin.tar.lzma inetutils-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ less-436-2-msys-1.0.13-bin.tar.lzma less-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libasprintf-0.17-1-mingw32-dll-0.tar.lzma libasprintf-dll-0.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libbz2-1.0.5-2-mingw32-dll-2.tar.gz libbz2-dll-2.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ libcharset-1.13.1-1-mingw32-dll-1.tar.lzma libcharset-dll-1.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libgettextpo-0.17-1-mingw32-dll-0.tar.lzma libgettextpo-dll-0.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libiconv-1.13.1-1-mingw32-bin.tar.lzma libiconv-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libiconv-1.13.1-1-mingw32-dev.tar.lzma libiconv-dev.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libiconv-1.13.1-1-mingw32-dll-2.tar.lzma libiconv-dll-2.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libiconv-1.13.1-2-msys-1.0.13-dll-2.tar.lzma libiconv-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libintl-0.17-1-mingw32-dll-8.tar.lzma libintl-dll-8.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libintl-0.17-2-msys-dll-8.tar.lzma libintl-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libltdl-2.2.11a-1-mingw32-dev.tar.lzma libltdl-dev.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libltdl-2.2.11a-1-mingw32-dll-7.tar.lzma libltdl-dll-7.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libmagic-5.04-1-msys-1.0.13-dll-1.tar.lzma libmagic-dll-1.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libmpc-0.8.1-1-mingw32-dll-2.tar.lzma libmpc-dll-2.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libmpfr-2.4.1-1-mingw32-dll-1.tar.lzma libmpfr-dll-1.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libpdcurses-3.4-1-mingw32-dev.tar.lzma libpdcurses-dev.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libpdcurses-3.4-1-mingw32-dll.tar.lzma libpdcurses-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libpthread-2.8.0-3-mingw32-dll-2.tar.lzma libpthread-dll-2.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libregex-1.20090805-2-msys-1.0.13-dll-1.tar.lzma libregex-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libtermcap-0.20050421_1-2-msys-1.0.13-dll-0.tar.lzma libtermcap-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libtool-2.2.11a-1-mingw32-bin.tar.lzma libtool-bin.tar.lzma
#//	download_archive http://downloads.sourceforge.net/mingw/ libz-1.2.3-1-mingw32-dev.tar.gz libz-dev.tar.gz
#//	download_archive http://downloads.sourceforge.net/mingw/ libz-1.2.3-1-mingw32-dll-1.tar.gz libz-dll-1.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ locate-4.4.2-2-msys-1.0.13-bin.tar.lzma locate-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ m4-1.4.14-1-msys-1.0.13-bin.tar.lzma m4-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ make-3.81-3-msys-1.0.13-bin.tar.lzma make-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ make-3.82-3-mingw32-bin.tar.lzma make-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ mingwrt-3.18-mingw32-dev.tar.gz mingwrt-dev.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ mingwrt-3.18-mingw32-dll.tar.gz mingwrt-dll.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ mintty-0.8.3--msys-1.0.15-bin.tar.lzma mintty-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ mktemp-1.6-2-msys-1.0.13-bin.tar.lzma mktemp-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ mpc-0.8.1-1-mingw32-dev.tar.lzma mpc-dev.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ mpfr-2.4.1-1-mingw32-dev.tar.lzma mpfr-dev.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ msysCORE-1.0.15-1-msys-1.0.15-bin.tar.lzma msysCORE-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ msysCORE-1.0.15-1-msys-1.0.15-ext.tar.lzma msysCORE-ext.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ patch-2.6.1-1-msys-1.0.13-bin.tar.lzma patch-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ PDCurses-3.4-1-mingw32-bin.tar.lzma PDCurses-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ perl-5.6.1_2-2-msys-1.0.13-bin.tar.lzma perl-bin.tar.lzma
#//	download_archive http://downloads.sourceforge.net/mingw/ pthreads-w32-2.8.0-3-mingw32-dev.tar.lzma pthreads-dev.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ rxvt-2.7.2-3-msys-1.0.14-bin.tar.lzma rxvt-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ sed-4.2.1-2-msys-1.0.13-bin.tar.lzma sed-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ tar-1.23-1-msys-1.0.13-bin.tar.lzma tar-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ texinfo-4.13a-2-msys-1.0.13-bin.tar.lzma texinfo-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ unzip-6.0-1-msys-1.0.13-bin.tar.lzma unzip-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ w32api-3.15-1-mingw32-dev.tar.lzma w32api-dev.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ wget-1.12-1-msys-1.0.13-bin.tar.lzma wget-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ zlib-1.2.3-2-msys-1.0.13-dll.tar.lzma zlib-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ crypt-1.1_1-3-msys-1.0.13-bin.tar.lzma crypt-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libcrypt-1.1_1-3-msys-1.0.13-dll-0.tar.lzma libcrypt-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libopts-5.10.1-1-msys-1.0.15-dll-25.tar.lzma libopts.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ gmp-5.0.1-1-mingw32-dev.tar.lzma gmp-dev.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libgmp-5.0.1-1-mingw32-dll-10.tar.lzma libgmp-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libgmpxx-5.0.1-1-mingw32-dll-4.tar.lzma libgmpxx-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ gcc-c++-4.5.0-1-mingw32-bin.tar.lzma gcc-c++-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ gcc-core-4.5.0-1-mingw32-bin.tar.lzma gcc-core-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libgcc-4.5.0-1-mingw32-dll-1.tar.lzma libgcc-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libssp-4.5.0-1-mingw32-dll-0.tar.lzma libssp-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libstdc++-4.5.0-1-mingw32-dll-6.tar.lzma libstdc++-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ autogen-5.10.1-1-msys-1.0.15-bin.tar.lzma autogen-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libxml2-2.7.6-1-msys-1.0.13-dll-2.tar.lzma libxml2-dll.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ zip-3.0-1-msys-1.0.14-bin.tar.lzma zip-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ openssh-5.4p1-1-msys-1.0.13-bin.tar.lzma openssh-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libminires-1.02_1-2-msys-1.0.13-dll.tar.lzma libminires-dll.tar.lzma



	download_archive http://downloads.sourceforge.net/gnuwin32/ pcre-7.0-bin.zip pcre.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ regex-2.7-bin.zip regex.zip
#//	download_archive http://downloads.sourceforge.net/gnuwin32/ tiff-3.8.2-1-lib.zip libtiff.zip





#//	download_archive http://downloads.sourceforge.net/tdm-gcc/ gcc-4.5.1-tdm-1-c++.tar.lzma gcc-c++.tar.lzma
#//	download_archive http://downloads.sourceforge.net/tdm-gcc/ gcc-4.5.1-tdm-1-core.tar.lzma gcc-core.tar.lzma





#	download_archive http://curl.oslevel.de/download/ libcurl-${CURL_VERSION}-win32-nossl.zip libcurl.zip





#	download_archive http://downloads.sourceforge.net/cunit/ CUnit-${CUNIT_VERSION}-winlib.zip cunit.zip





#	download_archive http://www.libsdl.org/projects/SDL_image/release/ SDL_image-devel-${SDL_IMAGE_VERSION}-VC8.zip sdl_image.zip
#	download_archive http://www.libsdl.org/projects/SDL_mixer/release/ SDL_mixer-devel-${SDL_MIXER_VERSION}-VC8.zip sdl_mixer.zip
#	download_archive http://www.libsdl.org/projects/SDL_ttf/release/ SDL_ttf-devel-${SDL_TTF_VERSION}-VC8.zip sdl_ttf.zip
#	download_archive http://www.libsdl.org/release/ SDL-devel-${SDL_VERSION}-mingw32.tar.gz sdl.tar.gz





#	download_archive http://oss.netfarm.it/mplayer/pkgs/ libogg-mingw32-1.2.0-gcc42.tar.bz2 libogg.tar.bz2
#	download_archive http://oss.netfarm.it/mplayer/pkgs/ libtheora-mingw32-1.1.1-gcc42.tar.bz2 libtheora.tar.bz2
#	download_archive http://oss.netfarm.it/mplayer/pkgs/ libvorbis-mingw32-1.3.1-gcc42.tar.bz2 libvorbis.tar.bz2
#	download_archive http://oss.netfarm.it/mplayer/pkgs/ xvidcore-mingw32-1.2.2-vaq-gcc42.tar.bz2 xvidcore.tar.bz2





	download_archive http://download.berlios.de/codeblocks/ wxmsw28u_gcc_cb_wx2810_gcc441.7z codeblocks_gcc.7z
	download_archive http://download.berlios.de/codeblocks/ mingwm10_gcc441.7z codeblocks_mingw.7z
	download_archive http://download.berlios.de/codeblocks/ CB_20100912_rev6583_CC_BRANCH_win32.7z codeblocks.7z





	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/atk/1.32/ atk-dev_1.32.0-1_win32.zip atk-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/atk/1.32/ atk_1.32.0-1_win32.zip atk.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ cairo-dev_1.10.0-1_win32.zip cairo-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ cairo_1.10.0-1_win32.zip cairo.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ expat-dev_2.0.1-1_win32.zip expat-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ expat_2.0.1-1_win32.zip expat.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ fontconfig-dev_2.8.0-2_win32.zip fontconfig-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ fontconfig_2.8.0-2_win32.zip fontconfig.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ freetype-dev_2.4.2-1_win32.zip freetype-dev.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ gettext-runtime-dev_0.18.1.1-2_win32.zip gettext-runtime-dev.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ gettext-runtime_0.18.1.1-2_win32.zip gettext-runtime.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ gettext-tools-dev_0.18.1.1-2_win32.zip gettext-tools-dev.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ jpeg-dev_8-1_win32.zip jpeg-dev.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ libpng-dev_1.4.3-1_win32.zip libpng-dev.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ libtiff-dev_3.9.2-1_win32.zip libtiff-dev.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ libxml2-dev_2.7.7-1_win32.zip libxml2-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ pkg-config_0.23-3_win32.zip pkg-config.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ zlib-dev_1.2.5-2_win32.zip zlib-dev.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ zlib_1.2.5-2_win32.zip zlib.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.24/ glib-dev_2.24.2-2_win32.zip glib-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.24/ glib_2.24.2-2_win32.zip glib.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.20/ gtk+-dev_2.20.1-3_win32.zip gtk+-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.20/ gtk+_2.20.1-3_win32.zip gtk+.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/gtksourceview/2.10/ gtksourceview-dev-2.10.0.zip gtksourceview-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/gtksourceview/2.10/ gtksourceview-2.10.0.zip gtksourceview.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.28/ pango-dev_1.28.3-1_win32.zip pango-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.28/ pango_1.28.3-1_win32.zip pango.zip





	download_archive http://mattn.ninex.info/download/ gtkglext-1.2.zip gtkglext-dev.zip
	download_archive http://mattn.ninex.info/download/ openal.zip libopenal-dev.zip





	download_archive http://www.libsdl.org/extras/win32/common/ directx-devel.tar.gz directx.tar.gz





	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ 7zip-9.17.7z 7zip.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ make_UfoAI_win32.7z make_UfoAI_win32.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ svn-win32-1.6.12.7z svn.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ git-1.7.1.7z git.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ Python.7z Python.7z


	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ flac-1.2.1.7z flac.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ freetype-2.4.2.7z freetype.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ gettext-0.18.1.1.7z gettext.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ libjpeg-turbo-1.0.1.7z libjpeg.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ libogg-1.2.0.7z libogg.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ libpng-1.4.3.7z libpng.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ libtheora-1.1.1.7z libtheora.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ libvorbis-1.3.1.7z libvorbis.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ libxml2-2.7.7.7z libxml2.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ SDL-1.2.14.7z SDL.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ SDL_image-1.2.10.7z SDL_image.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ SDL_mixer-1.2.11.7z SDL_mixer.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ SDL_ttf-2.0.10.7z SDL_ttf.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ smpeg-0.4.5.7z smpeg.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ tiff-3.9.4.7z tiff.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ xvidcore-1.2.2.7z xvidcore.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ zlib-1.2.5.7z zlib.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ libcurl-7.19.4.7z libcurl.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/libs/ cunit-2.1.2.7z cunit.7z


	download_archive http://downloads.sourceforge.net/nsis/ nsis-2.46.zip NSIS.zip



	download_archive http://downloads.sourceforge.net/mingw/ openssl-1.0.0-1-msys-1.0.13-bin.tar.lzma openssl-bin.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libopenssl-1.0.0-1-msys-1.0.13-dll-100.tar.lzma libopenssl-dll.tar.lzma
}

function exitscript()
{
	exit 2
}









if [ ! -x "${UN7ZIP}" ]; then
	echo "you need 7zip installed to run this script"
	exit 1
fi

if [ ! -x "${WGET}" ]; then
	echo "you need wget installed to run this script"
	exit 1
fi







if [ "$ACTION" = "create" ]; then
	create
elif [ "$ACTION" = "download" ]; then
	download
elif [ "$ACTION" = "update" ]; then
	update
elif [ "$ACTION" = "help" ]; then
	echo "Valid parameters are:"
	echo " - create   : creates the ${ARCHIVE_NAME} with the"
	echo "              latest versions of the needed programs and libs"
	echo " - download : only performs the downloading"
	echo " - update   : tries to update your current environment with the"
	echo "              latest versions of the needed programs and libs"
	echo " - clean    : removes the downloads and the temp dirs - but not"
	echo "              the created archive file"
	echo " - help     : prints this help message"
	echo " - upload   : uploads the created archive - ./ssh/config must be"
	echo "              set up for this to work"
elif [ "$ACTION" = "upload" ]; then
	if [ ! -e "${ARCHIVE_NAME}" ]; then
		echo "${ARCHIVE_NAME} doesn't exist. Hit any key to create it."
		read
		create
	fi
	scp ${ARCHIVE_NAME} ufo:~/public_html/mattn/download
elif [ "$ACTION" = "clean" ]; then
	rm -rf "${TEMP_DIR}"
	echo "clean finished"
else
	echo "unknown action given"
	exit 1
fi







# changelog
#	0.1.0 UfoAI devs
#	0.2.0 Muton (based on codeblocks.sh rev. 28557)
#		The script:
#		will now use 7za only for extraction
#		add everything needed to build UfoAI incl. NSIS and keep things separated; codeblock is codeblock, mingw is mingw
#		added my script make_UfoAI_win32 too
#		changed the way the script calls functions; I download and extract package by package
#		made a lot of error checking
#		header search and move/copy algo to move header files to the correct place (include_array)
#		The package is now called UfoAI_win32_build_environment and the folder UFOAIwin32BUILDenv
#		new
#			added variable environment_ver, exclude_from_find_result, what_do_we_try, EXTRACT_DIR, NSIS_DIR, CB_DIR, sfx7ZIP
#			added array include_array
#			added function infooutput(), redecho(), whiteecho(), magentaecho(), greenecho(), extract(), correctme()
#			added changelog
#			store a txt file at "${MINGW_DIR}/bin/WINenvVER" containing the version of the last build
#		changed
#			changed variable ARCHIVE_NAME, UN7ZIP, TARGET_DIR
#			changed function failure(), start_downloads(), create(), download(), download_archive(),
#			moved global vars topside
#			removed function extract_ogg(), extract_openal(), extract_gtk(), extract_tools(), extract_sdl(), extract_libcurl(), extract_cunit(), extract_libs(), extract_mingw(), extract_codeblocks(), extract_archive_gz(), extract_archive_bz2(), extract_archive_zip(), extract_archive_7z(), extract_archive_tar7z(),
#		update package
#			svn
#			pango
#			gtksourceview
#			gtk+-dev
#			glib-dev
#			glib
#			libpng
#			libiconv
#			gettext
#			libtool
#			automake
#			autoconf
#			msysCORE
#			findutils
#			coreutils
#			tar
#			bzip2
#			bash
#			make
#	      Muton (2010.04.17)
#		update package
#			libvorbis
#			xvidcore
#			libogg
#	0.3.1 Muton (2010.10.26)
#		The script:
#		The first release that is capable to compile ufo inside of MinGW without the need of codeblocks
#		Even python 2.7 and 3.1 is now included ( which python27.dll | sed 's/27.dll//', which python31.dll | sed 's/31.dll//' )
#		Nsis is no below MinGW insted steperated ( which nsis )
#		new
#			/include/wspiapi.h
#			autogen
#			crypt
#			expat
#			flac
#			git
#			gmp
#			jpeg-turbo
#			inetutils
#			libasprintf
#			libcharset
#			libcrypt
#			libgcc
#			libgettextpo
#			libgmp
#			libgmpxx
#			libltdl
#			libmagic
#			libmpc
#			libmpfr
#			libopenssl
#			libopts
#			libpdcurses
#			libpthread
#			libregex
#			libssp
#			libstdc++
#			libtermcap
#			mintty
#			mpc
#			mpfr
#			openssl
#			Python27 and 3.1
#			smpeg
#		changed
#			bug at line 239, "-maxdepth 1" added
#			Nsis folder is stored under MinGW
#		removed
#			jpeg
#		update package
#			7zip
#			atk
#			autoconf
#			automake
#			bash
#			binutils
#			cairo
#			codeblock
#			coreutils
#			diffutils
#			file
#			findutils
#			fontconfig
#			freetype
#			gawk
#			gcc-c++
#			gcc-core
#			gdb
#			gettext
#			glib
#			grep
#			gtk+
#			gtksourceview
#			gzip
#			less
#			libcurl
#			libintl
#			libogg
#			libpng
#			libtheora
#			libtool
#			libvorbis
#			libxml2
#			locate
#			m4
#			make
#			mingwm10_gcc
#			mingwrt
#			mktemp
#			msysCORE
#			pango
#			patch
#			PDCurses
#			perl
#			pkg-config
#			rxvt
#			SDL
#			SDL_image
#			SDL_mixer
#			SDL_ttf
#			sed
#			svn
#			tar
#			texinfo
#			tiff
#			unzip
#			w32api
#			wget
#			wxmsw28u_gcc
#			xvidcore
#			zip
#			zlib
#			openssh
#			libminires
#	0.3.2 Muton (2010.11.14)
#		update package
#			cunit
#		new
#			added a update function
