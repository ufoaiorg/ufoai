#!/bin/sh

if [ -z "`which convert`" -o -z "`which bc`" ]; then
echo You sohuld install ImageMagick and bc packages:
echo sudo apt-get install imagemagick bc
exit 1
fi

DIR=../../base
OUTDIR=../../base_resized
echo Reading from $DIR, writing to $OUTDIR

if [ -n "$1" ] ; then
DIR="$1"
fi
if [ -n "$2" ] ; then
OUTDIR="$2"
fi
DIR=`cd $DIR && pwd`
mkdir -p "$OUTDIR"
OUTDIR=`cd $OUTDIR && pwd`

cd "$DIR"
rm -f "$OUTDIR/imagesizes.txt"
find . -name "*.png" -o -name "*.jpg" -o -name "*.tga" | while read IMG; do
W=`identify -format "%w" $IMG`
H=`identify -format "%h" $IMG`
if [ $W -gt 256 -o $H -gt 256 ] ; then
MAX=$W
if [ $H -gt $MAX ] ; then
	MAX=$H
fi
PERCENT=`echo "scale=20; 256 * 100 / $MAX" | bc`
echo $IMG $W x $H - resizing to 256 by $PERCENT%
mkdir -p "`dirname $OUTDIR/$IMG`"
convert "$IMG" -filter Cubic -resize $PERCENT% "$OUTDIR/$IMG"
echo "`echo $IMG | sed 's@[.]png\|[.]jpg\|[.]tga@@' | sed 's@./@@'`" $W $H >> "$OUTDIR/imagesizes.txt"
fi
done
