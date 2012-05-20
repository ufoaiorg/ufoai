#!/bin/sh

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="${BASE_DIR%/*}" # cut off MacOS
APP_DIR="${BASE_DIR%/*}" # cut off Contents

export DYLD_LIBRARY_PATH="$BASE_DIR/Libraries"
export PANGO_RC_FILE="$BASE_DIR/Resources/etc/pango/pangorc"
#export GTK2_RC_FILES="$BASE_DIR/Resources/etc/gtk-2.0/gtkrc"
#export GTK_IM_MODULE_FILE="$bundle_etc/gtk-2.0/gtk.immodules"
export GDK_PIXBUF_MODULE_FILE="$BASE_DIR/Resources/etc/gtk-2.0/gdk-pixbuf.loaders"
export FONTCONFIG_FILE="$BASE_DIR/Resources/etc/fonts/fonts.conf"
export GTK_DATA_PREFIX="$BASE_DIR/Resources"
export GTK_EXE_PREFIX="$BASE_DIR/Resources"
export GTK_PATH="$BASE_DIR/Resources"
export XDG_DATA_HOME="$BASE_DIR/Resources"
export XDG_DATA_DIRS="$BASE_DIR/Resources"

if test -f "$BASE_DIR/Resources/etc/charset.alias"; then
    export CHARSETALIASDIR="$BASE_DIR/Resources/etc"
fi

cd ${APP_DIR}
if [ -x /usr/bin/open-x11 ]; then
    env LC_ALL="en_US.UTF-8" /usr/bin/open-x11 $BASE_DIR/MacOS/uforadiant "$@" &
else
    env LC_ALL="en_US.UTF-8" $BASE_DIR/MacOS/uforadiant "$@" &
fi
