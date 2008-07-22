#!/bin/bash

DIR="${1:-.}"
echo "using directory $DIR"

# define REPORTNEGATIVES to report all files that had no modifications performed
#REPORTNEGATIVES=yes

# define REPORTPOSITIVES to report action on svn setting
#REPORTPOSITIVES=yes

set -f

echo "setting mime types"

# we use order created by sort_mimetypes.sh

. mimetypes
for ((EXTENSION=0; EXTENSION < ${#MIME_EXTENSION[@]}; EXTENSION++)); do
    [[ $NOTFIRST ]] && {
	EXPRESSION="$EXPRESSION -o "
    } || {
	NOTFIRST=yes
    }
    EXPRESSION="${EXPRESSION}-name *.${MIME_EXTENSION[$EXTENSION]}"
done

mime () {
    if [ "$(svn pg svn:mime-type "$3")" == "$2" ]; then
        [[ $REPORTNEGATIVES ]] && echo "not setting mime type for $3, already $2"
    else
        [[ $REPORTPOSITIVES ]] && echo "*** setting svn:mime-type to $2 for $3"
        svn ps svn:mime-type $2 "$3"
    fi
}

while read FILENAME; do
    for ((EXTENSION=0; EXTENSION < ${#MIME_EXTENSION[@]}; EXTENSION++)); do
	if [ "${FILENAME##*.}" == "${MIME_EXTENSION[$EXTENSION]}" ]; then
	    mime "${MIME_EXTENSION[$EXTENSION]}" "${MIME_TYPE[$EXTENSION]}" "$FILENAME"
	fi
    done
done < <(find "$DIR" -type f ! -wholename '*/.svn*' $EXPRESSION)
set +f
