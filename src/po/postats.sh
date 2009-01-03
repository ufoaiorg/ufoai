#!/bin/bash

## Add an ufosf entry to your ~/.ssh/config to upload the images
## replace 'username' by your sf.net account
#	Host ufosf
#		Compression	yes
#		CompressionLevel	9
#		Hostname	web.sourceforge.net
#		user		username,ufoai
#

FUZZY_COLOR=#ffd800
UNTRTD_COLOR=#ff0000
TRTD_COLOR=#00ff06
WIDTH=400
HEIGHT=20

function usage() {
	echo "Usage: $0 <dir with .po files> .pot <path to images>"
	exit 1
}

PO_DIR="${1:-./src/po}"
POT_FILE="${2:-./src/po/ufoai.pot}"
IMAGES_DIR="${3:-./src/po/stats}"
MSGSTATS=$(dirname "${0}")/pomsgstats.sh

echo "using po files from ${PO_DIR}"
echo "against pot file ${POT_FILE}"
echo "writing images into ${IMAGES_DIR}"
echo "msgstats: ${MSGSTATS}"

test -d "${PO_DIR}" || usage
test -e "${POT_FILE}" || usage
test -d "${IMAGES_DIR}" || mkdir -p "${IMAGES_DIR}"

for po in $(ls ${PO_DIR}/ufoai-*.po); do
	echo -n "Generating stats for locale "$(basename "${po}" .po)"..."
	fuzzy_x=$(${MSGSTATS} "${POT_FILE}" "${po}" | \
	awk -v width=${WIDTH} '/fuzzy:/ {fuzzy = $2}
			/total:/ {total = $2}
			END {print int((fuzzy / total) * width)}')
	untr_x=$(${MSGSTATS} "${POT_FILE}" "${po}" | \
	awk -v width=$WIDTH '/untranslated:/ {untr = $2}
			/total:/ {total = $2}
			END {print int((untr / total) * width)}')
	untrtd_x0=$((${WIDTH} - ${untr_x}))
	untrtd_x1=${WIDTH}
	fuzzy_x0=$((${untrtd_x0} - ${fuzzy_x}))
	fuzzy_x1=${untrtd_x0}
	y1=$((${HEIGHT} - 1))
	convert -size ${WIDTH}x${HEIGHT} xc:${TRTD_COLOR} \
		-draw "fill ${FUZZY_COLOR} rectangle ${fuzzy_x0},0 ${fuzzy_x1},19
				fill ${UNTRTD_COLOR} rectangle ${untrtd_x0},0 ${untrtd_x1},19" \
		${IMAGES_DIR}/$(basename "${po}" .po).png
	echo " done"
done

scp ${IMAGES_DIR}/*.png ufosf:/home/groups/u/uf/ufoai/htdocs/img/postats
