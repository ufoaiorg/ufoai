#! /bin/sh

mime () {
	for D in "$@"; do
		find "$D" -type f | while read I; do
			case "$I" in
				*.3ds)   TYPE=application/x-3ds;;
				*.blend) TYPE=application/x-blender;;
				*.bmp)   TYPE=image/bmp;;
				*.bz2)   TYPE=application/bzip2;;
				*.doc)   TYPE=application/msword;;
				*.exe)   TYPE=application/x-executable;;
				*.gif)   TYPE=image/gif;;
				*.html)  TYPE=text/html;;
				*.ico)   TYPE=image/x-icon;;
				*.jpeg)  TYPE=image/jpeg;;
				*.jpg)   TYPE=image/jpeg;;
				*.odg)   TYPE=application/vnd.oasis.opendocument.graphics;;
				*.ogg)   TYPE=audio/ogg;;
				*.pdf)   TYPE=application/pdf;;
				*.pl)    TYPE=text/x-perl;;
				*.png)   TYPE=image/png;;
				*.po)    TYPE=text/x-gettext-translation;;
				*.psd)   TYPE=image/psd;;
				*.py)    TYPE=text/x-python;;
				*.sh)    TYPE=text/x-sh;;
				*.svg)   TYPE=image/svg+xml;;
				*.tga)   TYPE=image/x-tga;;
				*.tif)   TYPE=image/tiff;;
				*.ttf)   TYPE=application/x-truetype-font;;
				*.txt)   TYPE=text/plain;;
				*.wav)   TYPE=audio/x-wav;;
				*.xbm)   TYPE=image/x-xbitmap;;
				*.xcf)   TYPE=image/x-xcf;;
				*.xpm)   TYPE=image/x-xpixmap;;
				*.zip)   TYPE=application/zip;;
				*)       continue;;
			esac
			svn ps -q svn:mime-type "$TYPE" "$I"
		done
	done
}

if [ "$#" = 0 ]; then
	mime .
else
	mime "$@"
fi
