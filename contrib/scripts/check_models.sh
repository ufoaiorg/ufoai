#!/bin/bash

# note : this script currently is broken.
# md2.pl syntax has changed since it was introduced, and it has not been updated here.

# Environment
path="$(pwd)"

# make sure we can find any includes no matter where from we are launched
cd "$(dirname $0)"
SCRIPTDIR="$PWD"
. "$SCRIPTDIR/scripts_common" || {
	echo "can't include scripts_common"
	exit
}

logfile=~/check_models.log
svnsoft=$(which svn) || fail "couldn't find svn"
awksoft=$(which awk) || fail "couldn't find awk"
convertsoft=$(which convert) || fail "couldn't find convert (install imagemagick)"
md2soft=./md2.pl

# enable nullglob, required for some of the checks later
shopt -s nullglob

if [ ! -x "$md2soft" ]
then
	md2soft=md2.pl
	if [ ! -x "$md2soft" ]; then
	    md2soft=$SCRIPTDIR/../../src/tools/md2.pl
	    if [ ! -x "$md2soft" ]; then
		fail "can't find md2.pl"
	    fi
	fi
fi

[ -d "$path" ] || fail "Directory does not exist"

# Prepare logfile
> "$logfile"

# Remove pcx file if png file with the same name exists
remove_pcx_if_png()
{
	for i in *.pcx; do
		name=${i%.pcx}
		pngname="${name}.png"
		if [ -f "$pngname" ]
		then
			rm "$name.pcx"
			"$svnsoft" del "$name.pcx"
			echo "remove_pcx_if_png()... removed $name.pcx" >> "$logfile"
		fi
	done
}

# Convert png file to tga file
convert_png_to_tga()
{
	for i in *.png; do
		name=${i%.png}
		pngname="${name}.png"
		tganame="${name}.tga"
		if [ -f "$pngname" ]
		then
			"$convertsoft" "$pngname" "$tganame"
			echo "convert_png_to_tga()... converted $pngname to $tganame" >> "$logfile"
		fi
		if [ -f "$tganame" ]
		then
			rm "$pngname"
			"$svnsoft" del "$pngname"
			"$svnsoft" add "$tganame"
			echo "convert_png_to_tga()... removed $pngname" >> "$logfile"
			echo "convert_png_to_tga()... added $tganame" >> "$logfile"
		else
			echo
			echo "convert_png_to_tga()... something bad happened, didn't convert this png to tga: $pngname" >> "$logfile"
		fi
	done
}

# Check whether only one md2 and only one tga in current dir, then fix names
fix_md2_and_tga()
{
	howmanymd2=$(ls | egrep -c '(.*\.md2$)')
	howmanytga=$(ls | egrep -c '(.*\.tga$)')
	if [ "$howmanymd2" -lt 1 ] || [ "$howmanytga" -lt 1 ]
	then
		echo "fix_md2_and_tga()... no md2 models or no tga files in this directory, i cannot proceed" >> "$logfile"
	fi

	if [ "$howmanymd2" -gt 1 ] || [ "$howmanytga" -gt 1 ]
	then
		echo "fix_md2_and_tga()... Too many models or tga files in this directory, i cannot proceed" >> "$logfile"
	fi

	if [ "$howmanymd2" -eq 1 ] && [ "$howmanytga" -eq 1 ]
	then
		name=$(ls | egrep '(.*\.md2$)' | awk -F\. '{print $1}')
		md2name="${name}.md2"
		tganame="${name}.tga"
		if [ -f "$tganame" ]
		then
			echo "fix_md2_and_tga()... Only one model but tga file has the same name, doing nothing" >> "$logfile"
		else
			realtga=$(ls | egrep '(.*\.tga$)')
			cp "$realtga" "$tganame"
			echo "fix_md2_and_tga()... Renamed $realtga to $tganame" >> "$logfile"
			if [ -f "$tganame" ]
			then
				"$md2soft" skinedit "$md2name" "$md2name" ".$name" || fail "$md2soft failed on $md2name"
				echo "fix_md2_and_tga()... fixed md2 model: $md2name" >> "$logfile"
				$svnsoft del $realtga
				echo "fix_md2_and_tga()... removed $realtga" >> "$logfile"
			else
				echo "fix_md2_and_tga()... something bad happened, tga file not present" >> "$logfile"
			fi
		fi
	fi
}

# M A I N
while read i; do
	cd "$i"
	echo "current path: $i" >> "$logfile"
	remove_pcx_if_png
	convert_png_to_tga
	fix_md2_and_tga
done < <(find "$path" -type d ! -wholename '*.svn*' -print)
shopt -u nullglob
exit
