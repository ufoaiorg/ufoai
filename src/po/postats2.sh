#!/bin/bash

## Generate 2 files postats.wiki and postats.json from translation status
# JSON file is not used
# WIKI data must be pasted into http://ufoai.org/wiki/index.php/Template:Translation_status/Data
#
# Usage
# move to ./src/po
# execute ./postats2.sh . ufoai.pot
#

function usage() {
	echo "Usage: $0 <dir with .po files> .pot <path to images>"
	exit 1
}

PO_DIR="${1:-./src/po}"
POT_FILE="${2:-./src/po/ufoai.pot}"
MSGSTATS=$(dirname "${0}")/pomsgstats.sh

echo "using po files from ${PO_DIR}"
echo "against pot file ${POT_FILE}"
echo "msgstats: ${MSGSTATS}"

test -d "${PO_DIR}" || usage
test -e "${POT_FILE}" || usage

echo -n "" > postats.json
echo -n "" > postats.wiki

echo "{" >> postats.json
echo "{{#switch:{{{lang}}}_{{{type}}}" > postats.wiki

for po in $(ls ${PO_DIR}/ufoai-*.po); do
	echo -n "Generating stats for locale "$(basename "${po}" .po)"..."

	lang=$(echo "${po}" | sed -e 's/^.*ufoai-\(.*\)\.po.*/\1/')

	total=$(${MSGSTATS} "${POT_FILE}" "${po}" | \
	awk '/total:/ {total = $2}
			END {print int(total)}')
	missing=$(${MSGSTATS} "${POT_FILE}" "${po}" | \
	awk '/untranslated:/ {missing = $2}
			END {print int(missing)}')
	fuzzy=$(${MSGSTATS} "${POT_FILE}" "${po}" | \
	awk '/fuzzy:/ {fuzzy = $2}
			END {print int(fuzzy)}')

	echo "\"${lang}\": {\"total\": ${total}, \"fuzzy\": ${fuzzy}, \"missing\": ${missing}}," >> postats.json
	echo "|${lang}_total= ${total}" >> postats.wiki
	echo "|${lang}_missing= ${missing}" >> postats.wiki
	echo "|${lang}_fuzzy= ${fuzzy}" >> postats.wiki
	echo " done"
done

echo "}" >> postats.json
echo "}}" >> postats.wiki
