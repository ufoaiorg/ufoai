#!/bin/bash

MSGCMP=$(which msgcmp)

function usage() {
	echo "Usage: $0 def.pot translation.po"
	exit 1
}

DEF="$1"
TR="$2"

test -e ${TR} || usage
test -e ${DEF} || usage
test -x ${MSGCMP} || usage

LC_ALL=C ${MSGCMP} ${TR} ${DEF} 2>&1 | \
	awk '/this message needs to be reviewed by the translator/ {fuzzy++}
		/this message is untranslated/ {untr++}
		END {print "fuzzy: "fuzzy"\nuntranslated: "untr}'

cat ${DEF} | \
	awk '/msgid/ {total++}
		END {print "total: "total-1}'
