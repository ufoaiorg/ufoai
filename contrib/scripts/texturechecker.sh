#!/bin/bash


# Script to search all invalid licenses in base/textures
# Written by Zenerka

LANG=C;

#
# directory with maps, relative to current
MAPS="base/maps/"
#
# working directory
DIR=".texturechecker/"
#
# temp file with list of maps using current texture
MAPFILE=$DIR"mapfile.txt"
#
# temp file storing the info about found texture in map
ISTHERE=$DIR"isthere.txt"
#
# temp file storing found textures without license
NOLIC=$DIR"nolicense.txt"
#
# temp file storing fixed texture names
FIXNAMES=$DIR"fixed.txt"

SearchFiles() {
	directory="$1"
	extension="$2"
	for i in $(find $directory -name $extension -print);
		do svn proplist -R -v $i | awk '{
			k++;
			{
				if (k == 1 && $1 ~ /Properties/) {
					printf $NF" ";
					count=1
				}
			};
			{
				if (k > 1 && $1 ~ /svn:license/) {
					if ($0 ~ /GNU/ || $0 ~ /GPL/) {
						print "license OK!";
						count=0
					} else {
						print "probably wrong " $0;
						count=0
					}
				}
			}
			{
				if (k > 1 && count && $0 !~ /svn:source/) {
					print "NO LICENSE!";
				}
			}
		}';
	done
}

#
# Function searching textures without license
FindNoLicense() {
	directory="$1"
	extension="$2"
	for i in $(find $directory -name $extension -print);
		do svn proplist -R -v $i | awk '{
		k++;
		{
			if (k == 1 && $1 ~ /Properties/) {
				name=$3;
				count=1;
			}
		};
		{
			if (k > 1 && $1 ~ /svn:license/) {
				if ($0 ~ /GNU/ || $0 ~ /GPL/) {
					print "license OK!";
					count=0
				} else {
					count=0
				}
			}
		}
		{
			if (k > 1 && count && $0 !~ /svn:source/) {
				print name;
			}
		}
	}';
	done
}

#
# Function fixing the texture names to use them in search engine among maps
FixNames() {
	awk -F\/ '{print $3"/"$4}' $NOLIC | awk -F\' '{print $1}' | awk -F\. '{print $1}' | sort -uf
}

#
# Function searching given textures without license in all maps
CheckInMaps() {
	for i in $(cat $FIXNAMES); do
		if [ -f "$MAPFILE" ];
		then
			rm $MAPFILE
		fi
		touch $MAPFILE
		if [ $i = "/" ];
		then
			continue
		fi
		for j in $(find $MAPS -name '*.map' -print); do
			grep $i $j > $ISTHERE;
			THEREIS=$(wc -l $ISTHERE | awk '{print $1}')
			if [ "$THEREIS" -ne 0 ];
			then
				echo $j >> $MAPFILE;
			fi
		done;
		THEREIS=$(wc -l $MAPFILE | awk '{print $1}')
		if [ "$THEREIS" -eq 0 ];
		then
			echo -n "Texture ";
			echo -n $i
			echo " is NOT used in any map";
		else
			echo -n "Texture ";
			echo -n $i
			echo " used in maps:";
			cat $MAPFILE;
		fi
	done
}

Usage() {
        echo "Usage ${0} -list | -checkmaps [extension]"
        echo " list             - prints textures and license info"
        echo " checkmaps        - searches for textures without license"
        echo "                    in maps"
	echo " [extension]	- png/tga/jpg for -list param"
}

rm -rf $DIR
mkdir $DIR
if [ $# -eq 0 ]
then
	Usage
else
	case "$1" in
		"-list")
			if [ $# -eq 2 ]
			then
				echo "Building texture list...";
				ext="*.${2}"
				SearchFiles base/textures/ $ext
			else
				Usage
			fi
			;;
		"-checkmaps")
			echo "Prepare to find textures with no license..."
			touch $NOLIC
			FindNoLicense base/textures/ '*.jpg' >> $NOLIC
			FindNoLicense base/textures/ '*.png' >> $NOLIC
			FindNoLicense base/textures/ '*.tga' >> $NOLIC
			echo "Found textures without license, prepare to fix names..."
			FixNames > $FIXNAMES
			echo "Prepare to search textures without license in map files..."
			CheckInMaps
			;;
		*)
			Usage
			;;
	esac
fi
#	SearchFiles base/textures/ '*.jpg'
#	SearchFiles base/textures/ '*.tga'
#	SearchFiles base/textures/ '*.png'

rm -rf $DIR
