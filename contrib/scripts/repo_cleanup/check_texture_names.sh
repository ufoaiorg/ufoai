#!/bin/bash

TEXTUREDIR=${1:-../../../base/textures}

while read file; do
	texturepath=${file#*base/textures/}
	length=$(echo $texturepath | wc -m)
	# we assume extension to always be separated by a single dot & 3 characters
	if [ "$length" -gt "36" ]; then
		echo "$file exceeds allowed texture name length";
	fi
done < <(find $TEXTUREDIR -type f ! -wholename '*/.svn*')
