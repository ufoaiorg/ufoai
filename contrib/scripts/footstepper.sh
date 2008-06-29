#!/bin/sh

#
# files with texturelist
FLE="/base/maps/"
#
# file with SORTED texturelist
SRT="sorted.txt"
#
# original terrain.ufo
TER="/base/ufos/terrain.ufo"
#
# updated terrain.ufo
OUT="terrain.ufo.new"
#
# dummy footstep sound to add
SND="footsteps/water_in"
#
# working directory
DIR=".footstepper/"

AppendFirst() {
	echo -n "terrain "
}

AppendLast() {
	echo " {"
	echo "footstepsound \"$SND\""
	echo "}"
}

AppendNew() {
	echo "" >> $DIR$OUT
	AppendFirst >> $DIR$OUT
	echo -n $1 >> $DIR$OUT
	AppendLast >> $DIR$OUT
}

rm -rf $DIR;mkdir $DIR
touch $DIR$OUT
touch $DIR$SRT

if [ "$1" = "" ];then
	echo "specify the path to UFO:AI"
	exit 1
fi

if [ ! -f "$1$TER" ]; then
	echo "wrong path or no base/ufos/terrain.ufo"
	exit 1
fi
if [ ! -d $1$FLE ]; then
	echo "wrong path or no base/maps/"
	exit 1
fi

echo "Preparing to find all footstep files..."
touch $DIR"footstep.files";
for i in `find $1$FLE -name '*.footsteps' -print`; do
	echo $i >> $DIR"footstep.files"
done
echo "Preparing to sort texturelist... this may take a while..."
for i in `cat $DIR"footstep.files"`; do
	echo -n "processing file: "
	echo $i
	cat $i|sort -f|uniq > $DIR"tmp.txt"
	touch $DIR"sortednew.txt"
	for j in `cat $DIR"tmp.txt"`; do
		if awk '{print $1}' $DIR$SRT | grep $j > /dev/null; then
			MAPA=`echo $i|awk -F\. '{print $1}' |awk -F\/ '{print $NF}'`
			cat $DIR$SRT|awk -v a=$j -v b=$MAPA '{if ($0~a) {printf $0" "; print b}}' >> $DIR"sortednew.txt"
		else
			echo -n $j" " >> $DIR"sortednew.txt"
			echo $i|awk -F\. '{print $1}' |awk -F\/ '{print $NF}' >> $DIR"sortednew.txt"
		fi
	done

	if wc -l $DIR$SRT > 0; then
		counter=1
		for j in `cat $DIR$SRT|awk '{print $1}'`; do
			if grep $j $DIR"sortednew.txt" > /dev/null; then
				sleep 1
			else
				awk -v cnt=$counter '{{start++};if (start==cnt) {print $0}}' $DIR$SRT >> $DIR"sortednew.txt"
			fi
			counter=$((counter+1))
		done
	fi
	mv $DIR"sortednew.txt" $DIR$SRT
done
echo "Preparing new terrain.ufo, that will be $DIR$OUT"
cat $1$TER > $DIR$OUT
if grep footstepper $DIR$OUT > /dev/null; then
	sleep 1
else
	echo "###" >> $DIR$OUT
	echo "# Not sorted, added by footstepper.sh" >> $DIR$OUT
	echo "###" >> $DIR$OUT
fi
echo "Building new terrain.ufo... this may take a while..."
for i in `cat $DIR$SRT|awk '{print $1}'`; do
	if awk '{if ($1~/^terrain/) {print $0}}' $DIR$OUT|grep $i > /dev/null; then
		maps=`grep $i $DIR$SRT`
		awk '{print; if ($0~"'$i'") {print "//", "'"$maps"'"}}' $DIR$OUT > $DIR"final.txt"
		mv $DIR"final.txt" $DIR$OUT
	else
		AppendNew $i
	fi
done
echo "Replacing the original terrain.ufo..."
mv $DIR$OUT $1$TER

rm -rf $DIR
