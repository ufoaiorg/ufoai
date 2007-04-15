#!/bin/bash

# Environment
path=`pwd`
subdirfile=~/subdir.list
logfile=~/check_models.log
svnsoft=`which svn`
awksoft=`which awk`
convertsoft=`which convert`
md2soft=./md2.pl

if [ ! -e $md2soft ]
then
	md2soft=md2.pl
fi

if [ ! -d $path ]
then
	echo "Directory does not exist"
	exit
fi

# Prepare workspace
find $path -type d -print > $subdirfile

# Prepare logfile
if [ -f "$logfile" ]
then
	rm $logfile
	touch $logfile
else
	touch $logfile
fi

# Remove pcx file if png file with the same name exist
remove_pcx_if_png()
{
	for i in `ls | egrep '(.*\.pcx$)'`; do
		name=`echo $i | awk -F\. '{print $1}'`
		pngname=`echo -n $name; echo ".png"`
		if [ -f "$pngname" ]
		then
			rm $name.pcx
			$svnsoft del $name.pcx
			echo "remove_pcx_if_png()... removed $name.pcx" >> $logfile
		fi
	done
}

# Convert png file to tga file
convert_png_to_tga()
{
	for i in `ls | egrep '(.*\.png$)'`; do
		name=`echo $i | awk -F\. '{print $1}'`
		pngname=`echo -n $name; echo ".png"`
		tganame=`echo -n $name; echo ".tga"`
		if [ -f "$pngname" ]
		then
			$convertsoft $pngname $tganame
			echo "convert_png_to_tga()... converted $pngname to $tganame" >> $logfile
		fi
		if [ -f "$tganame" ]
		then
			rm $pngname
			$svnsoft del $pngname
			$svnsoft add $tganame
			echo "convert_png_to_tga()... removed $pngname" >> $logfile
			echo "convert_png_to_tga()... added $tganame" >> $logfile
		else
			echo
			echo "convert_png_to_tga()... something bad happened, didn't convert this png to tga: $pngname" >> $logfile
		fi
	done
}

# Try to fix texture names
fix_textures()
{
	md2name=`ls | egrep '(.*\.md2$)' | awk -F\. '{print $1}'`
	texname=`ls | awk -F\. '(($0~/\.png/)||($0~/\.tga/)||($0~/\.pcx/)) {print $1}'`
	texfull=`ls | awk '(($0~/\.png/)||($0~/\.tga/)||($0~/\.pcx/)) {print}'`

	if [ "$md2name" = "$texname" ]
	then
		echo "fix_textures()... texture file name matches md2 file name, doing nothing" >> $logfile
	else
		ext=`echo $texfull | awk -F\. '{print $2}'`
		$svnsoft move $texfull $md2name.$ext
		echo "fix_textures()... moved $texfull to $md2name.$ext" >> $logfile
		touch changed.log
	fi
}

# Check whether only one md2 and only one tga in current dir, then fix names
fix_md2_and_tga()
{
	howmanymd2=`ls | egrep -c '(.*\.md2$)'`
	howmanytex=`ls | awk '(($0~/\.png/)||($0~/\.tga/)||($0~/\.pcx/)) {print}'|wc -l`
	if [ "$howmanymd2" -lt 1 ] || [ "$howmanytex" -lt 1 ]
	then
		echo "fix_md2_and_tga()... no md2 models or no tga files in this directory, i cannot proceed" >> $logfile
	fi

	if [ "$howmanymd2" -gt 1 ] || [ "$howmanytex" -gt 1 ]
	then
		echo "fix_md2_and_tga()... Too many models or tga files in this directory, i cannot proceed" >> $logfile
	fi

	if [ "$howmanymd2" -eq 1 ] && [ "$howmanytex" -eq 1 ]
	then
		fix_textures
		name=`ls | egrep '(.*\.md2$)' | awk -F\. '{print $1}'`
		md2name=`echo -n $name; echo ".md2"`
		texname=`ls | awk '(($0~/\.png/)||($0~/\.tga/)||($0~/\.pcx/)) {print}'`

#		if [ -f changed.log ]
#		then
			if [ -f "$texname" ]
			then
				$md2soft $md2name $md2name .$name
				echo "fix_md2_and_tga()... fixed md2 model: $md2name" >> $logfile
			else
				echo "fix_md2_and_tga()... something bad happened, tex file not present" >> $logfile
			fi
#		else
#			 echo "fix_md2_and_tga()... Only one model but tex file has the same name, doing nothing" >> $logfile
#		fi
	fi
}

# M A I N
for i in `cat $subdirfile`; do
	cd $i
	echo "current path: $i" >> $logfile
	remove_pcx_if_png
	convert_png_to_tga
	fix_md2_and_tga
done
rm $subdirfile
exit

