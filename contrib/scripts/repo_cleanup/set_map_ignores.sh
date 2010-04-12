#!/bin/bash

MAPDIRS="../../../base/maps ../../../../data_source/maps"
PROPFILE=/tmp/map.properties

# set this to report directories where no action will be performed
#REPORTNEGATIVES=yes

# set this to report target settings
#REPORTTARGET

cd "$(dirname $0)"
SCRIPTDIR="$PWD"
. ../scripts_common

SVN="$(which svn)"
[ -x "$SVN" ] || fail "can't find svn executable"

[ -f "$PROPFILE" ] && fail "file '$PROPFILE' already exists, refusing to overwrite"

echo "*.bsp
*.bak
*autosave*
*.tmp
*.footsteps
*.map.original*
gmon.out" > $PROPFILE || fail "failed to write $PROPFILE"

[[ $REPORTTARGET ]] && {
	echo "svn:ignore will be set to:"
	cat "$PROPFILE"
}

for MAPDIR in $MAPDIRS; do
	[ -d "$MAPDIR" ] || {
		rm $PROPFILE
		fail "can't access map directory '$MAPDIR'"
	}
	while read DIRECTORY; do
		if [ "$(diff "$PROPFILE" <(svn pg svn:ignore "$DIRECTORY" | strings))" = "" ]; then
			[[ $REPORTNEGATIVES ]] && echo "not setting svn:ignore property for $DIRECTORY, already correct"
		else
			echo "*** setting svn:ignore property for $DIRECTORY"
			$SVN ps svn:ignore --file "$PROPFILE" "$DIRECTORY" || fail "failed to set property on $DIRECTORY"
		fi
	done < <(find "$MAPDIR" -type d ! -wholename '*/.svn*')
done

rm "$PROPFILE"
