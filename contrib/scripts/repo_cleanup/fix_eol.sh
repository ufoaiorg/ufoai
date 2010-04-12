#!/bin/bash

DIR="${1:-.}"
#silent=no
[[ $silent ]] && echo "using directory $DIR"

EXT="ac am c h cpp m pl po py qe4 rc sh tex txt ufo ump py map xml html"
# define REPORTNEGATIVES to report all files that had no modifications performed
#REPORTNEGATIVES=yes

# define REPORTPOSITIVES to report action on svn setting
#REPORTPOSITIVES=yes

[[ $silent ]] && echo "setting eol style to native"
set -f
for i in $EXT; do
	[[ $NOTFIRST ]] && {
		EXPRESSION="$EXPRESSION -o "
	} || {
		NOTFIRST=yes
	}
	EXPRESSION="${EXPRESSION}-name *.$i"
done

while read FILENAME; do
	if [ "$(svn pg svn:eol-style "$FILENAME")" = "native" ]; then
		[[ $REPORTNEGATIVES ]] && echo "not setting eol style for $FILENAME, already native"
	else
		# make sure there are no dos style newlines before setting eol style - that can fail if there are mixed newlines
		sed -i 's/\r//' $FILENAME
		# remove trailing whitespaces
		sed -i 's/[ \t]*$//' $FILENAME
		[[ $REPORTPOSITIVES ]] && echo "*** setting svn:eol-style native for $FILENAME"
		svn ps svn:eol-style native "$FILENAME"
	fi
done < <(find "$DIR" -type f ! -wholename '*/.svn*' $EXPRESSION)
set +f
