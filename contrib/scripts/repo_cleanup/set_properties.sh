#! /bin/sh

prop () {
	for D in "$@"; do
		find "$D" -type f | while read I; do
			case "$I" in
				*/irc_motd.txt)
					svn ps -q svn:eol-style LF "$I";;

				*.6       | \
				*.ac      | \
				*.anm     | \
				*.bat     | \
				*.c       | \
				*.cfg     | \
				*.cpp     | \
				*.def     | \
				*.desktop | \
				*.dirs    | \
				*.glsl    | \
				*.h       | \
				*.html    | \
				*.in      | \
				*.install | \
				*.lang    | \
				*.launch  | \
				*.lua     | \
				*.m       | \
				*.menu    | \
				*.ms      | \
				*.mtl     | \
				*.map     | \
				*.mat     | \
				*.mk      | \
				*.obj     | \
				*.pl      | \
				*.po      | \
				*.pot     | \
				*.py      | \
				*.qe4     | \
				*.rc      | \
				*.rng     | \
				*.sh      | \
				*.tex     | \
				*.tpl     | \
				*.txt     | \
				*.ufo     | \
				*.ump     | \
				*.xml)
					svn ps -q svn:eol-style native "$I";;
			esac

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
				*.mo)    TYPE=application/x-gettext-translation;;
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
	prop .
else
	prop "$@"
fi
