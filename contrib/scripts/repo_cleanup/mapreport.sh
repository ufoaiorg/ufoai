#!/bin/bash

#CHECKS="ent con mbr lvl flv tex mfc"

VERBOSITY=2

for param in "$@"; do
	case $param in
		dir=*)
		PASSMAPDIR=$(echo $param | cut -d= -f2)
		;;
	esac
done

MAPDIR=${PASSMAPDIR:-base/maps}

while read map; do
	./ufo2map -v $VERBOSITY -check $map
done< <(find $MAPDIR -type f ! -wholename '*/.svn*' ! -name '*.autosave.*' ! -name '*_drop_*' -name '*.map')
