#!/bin/bash

DIR="${1:-../../../base/maps}"
#silent=no
[[ $silent ]] && echo "using directory $DIR"

TRUNKDIR=$(cd $(dirname $0)/../../..; pwd)

while read MAP; do
	for TEXTURE in $(grep "^( " $MAP | cut -d" " -f 16 | sort -u); do
		[[ $(ls "$TRUNKDIR"/base/textures/"$TEXTURE."* 2>/dev/null) ]] || {
			((HASMISSINGTEXTS++))
			MISSINGTEXTURE[$HASMISSINGTEXTS]=$TEXTURE
		}
	done
	[[ $HASMISSINGTEXTS ]] && {
		echo "*** ${MAP#$DIR} : $HASMISSINGTEXTS missing textures"
		for ((missingprint=1; missingprint<=$HASMISSINGTEXTS; missingprint++)); do
			echo ${MISSINGTEXTURE[$missingprint]}
		done
		echo
	}
	unset HASMISSINGTEXTS
done < <(find "$DIR" -type f ! -wholename '*/.svn*' ! -wholename '*/old/*' -name '*.map' ! -name '*.autosave.*')
