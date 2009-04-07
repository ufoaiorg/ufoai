#!/bin/bash

# this is a helper script for mimetype setting. it checks repository for file type distribution (only for files that will
#  have mimetype set and output a sorted list by occurrence
# those should be set in fix_mimetypes.sh then
# this script should be run whenever mimetypes.in changes or there are significant changes in file type distribution
# pass target directory to this script, usually it would be trunk directory

DIR=${1:-.}
TMPFILE=/tmp/typestats
INFILE=mimetypes.in

# define SHOWSTATS to show distribution statistics after building the list
SHOWSTATS=yes

cd "$(dirname $0)"
SCRIPTDIR="$PWD"
. ../scripts_common

[[ -d "$DIR" ]] || fail "no such directory $DIR"

[[ -f "$TMPFILE" ]] && fail "$TMPFILE exists, refusing to overwrite"

while read line; do
	extension=$(echo $line | cut -d" " -f1)
	mimetype=$(echo $line | cut -d" " -f2)
	echo -n "$extension $mimetype " >> "$TMPFILE"
	find "$DIR" -type f -name "*.$extension" ! -wholename '*/.svn*' | wc -l >> "$TMPFILE"
done < <(grep -v ^# "$INFILE")

echo -n "MIME_EXTENSION=(" > mimetypes.ext
echo -n "MIME_TYPE=(" > mimetypes.mime

while read line; do
	extension=$(echo $line | cut -d" " -f1)
	mimetype=$(echo $line | cut -d" " -f2)
	echo -n "$extension " >> "mimetypes.ext"
	echo -n "$mimetype " >> "mimetypes.mime"
done < <(sort -nr -k 3 "$TMPFILE")
[[ $SHOWSTATS ]] && sort -nr -k 3 "$TMPFILE"
sed -i 's/ $//g' mimetypes.ext mimetypes.mime
echo ")" >> mimetypes.ext
echo ")" >> mimetypes.mime
cat mimetypes.ext mimetypes.mime > mimetypes

rm "$TMPFILE" mimetypes.ext mimetypes.mime
