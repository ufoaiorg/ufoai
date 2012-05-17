#!/bin/sh

BASE_DIR="${0%/*}" # cut off the script name
BASE_DIR="${BASE_DIR%/*}" # cut off MacOS

export DYLD_LIBRARY_PATH="$BASE_DIR/Libraries"
export PANGO_RC_FILE="$BASE_DIR/Resources/etc/pango/pangorc"
#export GTK2_RC_FILES="$BASE_DIR/Resources/etc/gtk-2.0/gtkrc"
export GDK_PIXBUF_MODULE_FILE="$BASE_DIR/Resources/etc/gtk-2.0/gdk-pixbuf.loaders"
export FONTCONFIG_FILE="$BASE_DIR/Resources/etc/fonts/fonts.conf"

cd "$BASE_DIR/MacOS"
if [ -x /usr/bin/open-x11 ]; then
    env LC_ALL="en_US.UTF-8" /usr/bin/open-x11 ./uforadiant "$@" &
else
    env LC_ALL="en_US.UTF-8" ./uforadiant "$@" &
fi
