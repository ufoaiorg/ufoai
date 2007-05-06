#!/bin/bash

ZIP_PROG=/usr/bin/zip
UNZIP_PROG=/usr/bin/unzip
EXT=pk3
VERSION=0.1

#default values
UPDATE=1
VERBOSE=0

GenPK3_Usage()
{
    echo "Usage: $0 mapname (without extension)"
    echo "params can be one or more of the following :"
    echo "    --version | -v  : Print out Makeself version number and exit"
    echo "    --help | -h     : Print out this help message"
    echo "    --verbose       : Verbose output"
    echo "    --only-new | -n : Only add missing files to the pk3 - don't update existing ones"
    exit 1
}

CheckExistenceInPK3()
{
	if test "$VERBOSE" == 1; then
		echo "Check for $1 in pk3 files"
	fi
	# check whether file exists in pk3 archives
	# FIXME
	return 1
}

if ! test -d "./maps"; then
	echo "$0 must be in the base dir"
	exit 1
fi

if ! test -x "$ZIP_PROG"; then
	echo "You must have zip installed ($ZIP_PROG)"
	exit 1
fi

if ! test -x "$UNZIP_PROG"; then
	echo "You must have unzip installed ($UNZIP_PROG)"
	exit 1
fi

while true
do
    case "$1" in
    --version | -v)
	echo GenPK3 version $VERSION
	exit 0
	;;
    --help | -h)
	GenPK3_Usage
	exit 0
	;;
    --only-new | -n)
	UPDATE=0
	shift
	;;
    --verbose)
	VERBOSE=1
	shift
	;;
    *)
	break
	;;
	esac
done

ZIP_PARM=-9

if test "$UPDATE" == 1; then
	ZIP_PARM+=u
fi

if test "$VERBOSE" == 1; then
	ZIP_PARM+=v
else
	ZIP_PARM+=q
fi

if test $# -lt 1; then
	GenPK3_Usage
else
	mapname=`basename "$1"`
	echo "Generating $mapname.pk3"
	echo ".. zip options: $ZIP_PARM"
	for daynight in `echo "d n"`; do
		if test -f "maps/$mapname$daynight.bsp"; then
			if test -f "maps/$mapname$daynight.map"; then
				echo "... adding maps/$mapname$daynight.map"
				echo "... adding maps/$mapname$daynight.bsp"
				zip $ZIP_PARM $mapname.$EXT maps/$mapname$daynight.map maps/$mapname$daynight.bsp
			else
				echo "$mapname$daynight.map (source map) does not exist."
				exit 1
			fi
		else
			echo "$1$daynight.bsp (compiled map) does not exist."
			exit 1
		fi
	done

	# now the loading screens and thumbnail shots
	if test -f "pics/maps/loading/$mapname.jpg"; then
		echo "... adding pics/maps/loading/$mapname.jpg"
		zip $ZIP_PARM $mapname.$EXT pics/maps/loading/$1.jpg
	else
		echo "Warning: pics/maps/loading/$mapname.jpg (loading screen) does not exist."
	fi
	if test -f "pics/maps/shots/$mapname.jpg"; then
		echo "... adding pics/maps/shots/$mapname.jpg"
		zip $ZIP_PARM $mapname.$EXT pics/maps/shots/$mapname.jpg
	else
		echo "Warning: pics/loading/$mapname.jpg (loading screen) does not exist."
	fi
	for shotnr in `echo "2 3"`; do
		if test -f "pics/maps/shots/$1_$shotnr.jpg"; then
			echo "... adding pics/maps/shots/$mapname_$shotnr.jpg"
			zip $ZIP_PARM $mapname.$EXT pics/maps/shots/$mapname_$shotnr.jpg
		else
			echo "Warning: pics/loading/$mapname_$shotnr.jpg (loading screen) does not exist."
		fi
	done

	filename=FIXME
	# now search for models given in the map file that are not in the already
	# existing [0-9].*\.pk3 files
	for daynight in `echo "d n"`; do
		# no need to check the existence of the map - done above
		# search for misc_model in the *.map files
		CheckExistenceInPK3 $filename;
		if [ $? -eq 0 ]
		then
			zip $ZIP_PARM $mapname.$EXT $filename
		fi
	done

	# now search for textures given in the map file that are not in the already
	# existing [0-9].*\.pk3 files
	for daynight in `echo "d n"`; do
		# no need to check the existence of the map - done above
		CheckExistenceInPK3 $filename;
		if [ $? -eq 0 ]
		then
			zip $ZIP_PARM $mapname.$EXT $filename
		fi
	done
fi
