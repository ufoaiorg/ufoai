#!/bin/bash

# Script to search all invalid licenses in base/textures
# Written by Zenerka

LANG=C;

function searchFiles {
	directory=$1;
	extension=$2;

	for i in `find ${directory} -name '*.${extension}' -print`;
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
					print "NO LICENSE!"
				}
			}
		}';
	done
}

# check whether the files that are printed as NO LICENSE are
# used in the map source files
function checkmaps {
	echo "TODO"
}

function usage {
	echo "Usage ${0} [-checkmaps] [-extension <fileextension>]"
	echo " checkmaps      - checks whether the textures that don't"
	echo "                  have a license set are used in the maps"
	echo " extension      - the files to check"
}

if [ $# -eq 0 ]
then
	searchFiles "base/textures/" "jpg"
	searchFiles "base/textures/" "tga"
	searchFiles "base/textures/" "png"
else
	case "$1" in
		"-checkmaps")
			checkmaps
			;;
		"-extension")
			if [ $# -eq 2 ]
			then
				searchFiles "base/textures/" $2
			else
				usage
			fi
			;;
		 *)
			usage
			;;
	esac
fi

