#!/bin/sh

LANG=C

if [ $# -ne 1 ]
then
	echo "No binary given"
	exit 1
fi

APP_PATH=$1

APP=$(readlink -f "${APP_PATH}")

if [ ! -x "${APP}" ]
then
	echo "${APP} is no executable"
	exit 1
fi

PATH_ONLY=$(dirname "${APP}")
BIN_ONLY=$(basename "${APP}")

# path to the valgrind suppression file
SUP=$(readlink -f "${0}")
SUP=$(dirname "${SUP}")
SUP="${SUP}/valgrind.sup"

cd ${PATH_ONLY}
LOG=${PATH_ONLY}/valgrind.log

if [ -f "${SUP}" ]
then
	echo "Using valgrind suppression file from '${SUP}'"
	VALGRIND_OPTIONS="${VALGRIND_OPTIONS} --suppressions=${SUP}"
else
	echo "No valgrind suppression file at '${SUP}'"
fi

echo "Debugging ${APP_PATH}"
echo "Log file will be created at ${LOG}"

#--gen-suppressions=yes \
#--verbose \
#--demangle=yes \

valgrind \
	--show-reachable=yes \
	--leak-check=full \
	--track-fds=yes \
	--run-libc-freeres=no \
	--log-file=$LOG \
	$VALGRIND_OPTIONS \
	$APP
