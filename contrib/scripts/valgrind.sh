#!/bin/sh

LANG=C
TOOL=memcheck

usage() {
	echo "Usage: $0 [--help|-h] [--suppression|-s <file>] [--gensup|-g] [--nolog] [--tool|-t <tool>] --binary|-b <binary>"
	echo " --help -h         - Shows this help message"
	echo " --suppression -s  - Use the given suppression file - otherwise the standard"
	echo "                     file will be used (if found)"
	echo " --gensup -g       - Generate suppression statements"
	echo " --tool -t         - Valgrind tool (default is memcheck)"
	echo " --binary -b       - Execute valgrind for the given binary"
	echo " --nolog           - Don't log into a file but to stdout"
	exit 1
}

error() {
	echo "Error: $@"
	exit 1
}

while [ $# -gt 0 ]; do
	case "$1" in
		--binary|-b)
			APP_PATH=$2
			shift
			shift
			;;
		--tool|-t)
			TOOL=$2
			shift
			shift
			;;
		--suppresion|-s)
			SUP=$2
			shift
			shift
			;;
		--gensup|-g)
			GENERATE_SUPPRESSION=X
			shift
			;;
		--nolog)
			NO_LOG=X
			shift
			;;
		--help|-h|*)
			usage
			;;
		-*)
			error "invalid option $1"
			;;
			
	esac
done

[ -z "$APP_PATH" ] && error "No binary given"

APP=$(readlink -f "${APP_PATH}")

[ ! -x "${APP}" ] && error "${APP} is no executable"

PATH_ONLY=$(dirname "${APP}")
BIN_ONLY=$(basename "${APP}")

echo "Debugging ${APP_PATH}"

if [ ${GENERATE_SUPPRESSION} ]
then
	valgrind \
		--gen-suppressions=yes \
		--verbose \
		--demangle=yes \
		$VALGRIND_OPTIONS \
		$APP
else
	if [ $TOOL = "memcheck" ]
	then
		if [ -z "$SUP" ]
		then
			# path to the valgrind suppression file
			SUP=$(readlink -f "${0}")
			SUP=$(dirname "${SUP}")
			SUP="${SUP}/valgrind.sup"
		fi

		if [ -f "${SUP}" ]
		then
			echo "Using valgrind suppression file from '${SUP}'"
			VALGRIND_OPTIONS="${VALGRIND_OPTIONS} --suppressions=${SUP}"
		else
			echo "No valgrind suppression file at '${SUP}'"
		fi

		if [ -z "$NO_LOG" ]
		then
			LOG=${PATH_ONLY}/valgrind.log
			VALGRIND_OPTIONS="--log-file=$LOG $VALGRIND_OPTIONS"
			echo "Log file will be created at ${LOG}"
		fi
		VALGRIND_OPTIONS="--show-reachable=yes $VALGRIND_OPTIONS"
		VALGRIND_OPTIONS="--leak-check=full $VALGRIND_OPTIONS"
		VALGRIND_OPTIONS="--track-fds=yes $VALGRIND_OPTIONS"
		VALGRIND_OPTIONS="--run-libc-freeres=no $VALGRIND_OPTIONS"
	else
		VALGRIND_OPTIONS="--tool=$TOOL $VALGRIND_OPTIONS"
	fi

	cd ${PATH_ONLY}
	valgrind $VALGRIND_OPTIONS $APP
fi
