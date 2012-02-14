#!/bin/sh

if [ -z "`which convert`" -o -z "`which bc`" ]; then
echo You should install ImageMagick and bc packages:
echo sudo apt-get install imagemagick bc
exit 1
fi

DIR=../../base
OUTDIR=../../base_resized

if [ -n "$1" ] ; then
	DIR="$1"
fi
if [ -n "$2" ] ; then
	OUTDIR="$2"
fi
DIR=`cd $DIR && pwd`
mkdir -p "$OUTDIR"
OUTDIR=`cd $OUTDIR && pwd`

echo Reading from $DIR, writing to $OUTDIR

cd "$DIR"
rm -f "$OUTDIR/downsampledimages.txt"
find . -name "*.png" -o -name "*.jpg" -o -name "*.tga" | while read IMG; do
	W=`identify -format "%w" $IMG`
	H=`identify -format "%h" $IMG`
	if [ $W -gt 512 -o $H -gt 512 ] ; then
		MAX=$W
		MIN=$H
		if [ $H -gt $MAX ] ; then
			MAX=$H
			MIN=$W
		fi
		TARGET=256
		RATIO=`echo "scale=0; $MAX * 1000 / $MIN" | bc`
		if [ $RATIO -ge 2000 -a -n "`echo $IMG | grep '^[.]/pics/'`" ] ; then
			TARGET=512 # Convert to 512x256 or 256x512, many UI elements are narrow like that, and the earth image won't look ugly
		fi
		PERCENT=`echo "scale=20; $TARGET * 100 / $MAX" | bc`
		echo $IMG $W x $H - resizing to $TARGET x `expr $TARGET '*' 1000 / $RATIO` by $PERCENT%
		mkdir -p "`dirname $OUTDIR/$IMG`"
		convert "$IMG" -filter Cubic -resize $PERCENT% "$OUTDIR/$IMG-$$"
		mv -f "$OUTDIR/$IMG-$$" "$OUTDIR/$IMG"
		echo "`echo $IMG | sed 's@[.]png\|[.]jpg\|[.]tga@@' | sed 's@./@@'`" $W $H >> "$OUTDIR/downsampledimages.txt"
	else
		NPOTH=true
		NPOTW=true
		POTW=$W
		POTH=$H
		for Q in 16384 8192 4096 2048 1024 512 256 128 64 32 16 8 4 2 1; do
			if [ "$H" = "$Q" ]; then
				NPOTH=false
			fi
			if [ "$W" = "$Q" ]; then
				NPOTW=false
			fi
			if [ "$H" -le "$Q" ]; then
				POTH=$Q
			fi
			if [ "$W" -le "$Q" ]; then
				POTW=$Q
			fi
		done
		if $NPOTH || $NPOTW; then
			if [ "$POTW.$POTH" = "512.512" ]; then # Avoid worst-case scenario
				POTW=256
				POTH=256
			fi
			echo $IMG $W x $H - resizing to $POTW x $POTH, because there is Android-specific bug with non-power-of-two textures which I could not fix
			mkdir -p "`dirname $OUTDIR/$IMG`"
			convert "$IMG" -filter Cubic -resize ${POTW}x${POTH}\! "$OUTDIR/$IMG-$$"
			mv -f "$OUTDIR/$IMG-$$" "$OUTDIR/$IMG"
			echo "`echo $IMG | sed 's@[.]png\|[.]jpg\|[.]tga@@' | sed 's@./@@'`" $W $H >> "$OUTDIR/downsampledimages.txt"
		fi
	fi
done
