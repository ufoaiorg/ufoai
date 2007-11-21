#!/bin/bash

# Script to search all invalid licenses in base/textures
# Written by Zenerka

LANG=C;

for i in `find base/textures/ -name '*.jpg' -print`;
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