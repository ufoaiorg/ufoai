#!/bin/sh

BASE_DIR=$(dirname $(readlink -f ${0}))
BASE_DIR="${BASE_DIR%/*}" # cut off MacOS
APP_DIR="${BASE_DIR%/*}" # cut off Contents

export DYLD_LIBRARY_PATH="$BASE_DIR/Libraries"
export PANGO_RC_FILE="$BASE_DIR/Resources/etc/pango/pangorc"
#export GTK2_RC_FILES="$BASE_DIR/Resources/etc/gtk-2.0/gtkrc"
export GDK_PIXBUF_MODULE_FILE="$BASE_DIR/Resources/etc/gtk-2.0/gdk-pixbuf.loaders"
export FONTCONFIG_FILE="$BASE_DIR/Resources/etc/fonts/fonts.conf"

cd ${APP_DIR}
if [ -x /usr/bin/open-x11 ]; then
    env LC_ALL="en_US.UTF-8" /usr/bin/open-x11 $BASE_DIR/MacOS/uforadiant "$@" &
else
    env LC_ALL="en_US.UTF-8" $BASE_DIR/MacOS/uforadiant "$@" &
fi
