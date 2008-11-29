#!/bin/sh

LANG=C
DIR=.

if [ $# -eq 1 ]
then
	echo "Use $1"
	DIR="$1"
fi


APP=${DIR}/ufo
LOG=${DIR}/valgrind.log
valgrind \
	--verbose \
	--show-reachable=yes \
	--log-file=$LOG \
	--leak-check=full \
	--track-fds=yes \
	--run-libc-freeres=no \
	$APP
