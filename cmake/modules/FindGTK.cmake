# - try to find GTK, glib and all GTK related modules required for compiling UFOAI
#  GTK_INCLUDE_DIRS   - Directories to include to use GTK
#  GTK_LIBRARIES     - Files to link against to use GTK
#  GTK_FOUND         - GTK was found

find_path(GTK_INCLUDE_DIR_GTK NAMES gtk/gtk.h
   PATH_SUFFIXES gtk-1.2 gtk12 gtk-2.0
   PATHS
   /usr/openwin/share/include
   /usr/openwin/include
   /opt/gnome/include
)

find_path(GTK_INCLUDE_DIR_GLIB NAMES glib.h
   PATH_SUFFIXES gtk-1.2 gtk12 gtk-2.0 glib-1.2 glib12 glib-2.0
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

find_path(GTK_INCLUDE_DIR_GTKSOURCEVIEW NAMES gtksourceview/gtksourceview.h
   PATH_SUFFIXES gtksourceview-2.0
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

find_path(GTK_INCLUDE_DIR_GTKGL NAMES gtk/gtkgl.h
   PATH_SUFFIXES gtkglext-1.0
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

find_path(GTK_INCLUDE_DIR_GDKPIXBUF NAMES gdk-pixbuf/gdk-pixbuf.h
   PATH_SUFFIXES gdk-pixbuf-2.0
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

find_path(GTK_INCLUDE_DIR_GDK NAMES gdk/gdk.h
   PATH_SUFFIXES gtk-1.2 gtk12 gtk-2.0
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

# glibconfig.h may be in a different directory than gtk/glib
find_path(GTK_INCLUDE_DIR_GLIBCONFIG NAMES glibconfig.h
   PATH_SUFFIXES gtk-1.2 gtk12 gtk-2.0 glib-1.2 glib12 glib-2.0
   PATHS
   /usr/openwin/share/include
   /usr/local/include/glib12
   /usr/lib/glib/include
   /usr/local/lib/glib/include
   /opt/gnome/include
   /opt/gnome/lib/glib/include
   /usr/lib/i386-linux-gnu/glib-2.0/include/
)

# gdkconfig.h may be in a different directory than gtk/glib
find_path(GTK_INCLUDE_DIR_GDKCONFIG NAMES gdkconfig.h
   PATH_SUFFIXES gtk-1.2 gtk12 gtk-2.0
   PATHS
   /usr/openwin/share/include
   /usr/local/include/glib12
   /usr/lib/glib/include
   /usr/local/lib/glib/include
   /opt/gnome/include
   /opt/gnome/lib/glib/include
   /usr/lib/i386-linux-gnu/gtk-2.0/include
)

# gdkglext-config.h may be in a different directory than gtk/glib
find_path(GTK_INCLUDE_DIR_GDKGLEXTCONFIG NAMES gdkglext-config.h
   PATH_SUFFIXES gtk-1.2 gtk12 gtk-2.0
   PATHS
   /usr/openwin/share/include
   /usr/local/include/glib12
   /usr/lib/glib/include
   /usr/local/lib/glib/include
   /opt/gnome/include
   /opt/gnome/lib/glib/include
   /usr/lib/gtkglext-1.0/include
)

find_path(GTK_INCLUDE_DIR_ATK NAMES atk/atk.h
   PATH_SUFFIXES atk-1.0
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

find_path(GTK_INCLUDE_DIR_CAIRO NAMES cairo.h
   PATH_SUFFIXES cairo
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

find_path(GTK_INCLUDE_DIR_PANGO NAMES pango/pango.h
   PATH_SUFFIXES pango-1.0
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

find_path(GTK_INCLUDE_DIR_XML2 NAMES libxml/parser.h
   PATH_SUFFIXES libxml2
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

find_library(GTK_LIBRARY_GTK
   NAMES  gtk gtk12 gtk-2.0 gtk-x11-2.0 gtk-win32-2.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTK_LIBRARY_GTKSOURCEVIEW
   NAMES  gtksourceview gtksourceview-2.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTK_LIBRARY_GTKGL
   NAMES  gtkglext gtkglext-x11 gtkglext-x11-1.0 gtkglext-win32-1.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTK_LIBRARY_GDKPIXBUF
   NAMES  gdk_pixbuf gdk_pixbuf-2.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTK_LIBRARY_GDKGL
   NAMES  gdkgl gdkglext-x11 gdkglext-x11-1.0 gdkglext-win32-1.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTK_LIBRARY_GDK
   NAMES  gdk gdk12 gdk-x11-2.0 gdk-win32-2.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTK_LIBRARY_GOBJECT
   NAMES  gobject gobject-2.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTK_LIBRARY_GLIB
   NAMES  glib glib-2.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTK_LIBRARY_PANGO
   NAMES  pango pango-1.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTK_LIBRARY_XML2
   NAMES  xml2
   PATHS
   /usr/openwin/lib
   /opt/gnome/lib
)

if(GTK_INCLUDE_DIR_GTK
   AND GTK_INCLUDE_DIR_GLIBCONFIG
   AND GTK_INCLUDE_DIR_GDKCONFIG
   AND GTK_INCLUDE_DIR_GDKGLEXTCONFIG
   AND GTK_INCLUDE_DIR_GLIB
   AND GTK_INCLUDE_DIR_GTKSOURCEVIEW
   AND GTK_INCLUDE_DIR_GTKGL
   AND GTK_INCLUDE_DIR_GDKPIXBUF
   AND GTK_INCLUDE_DIR_GDK
   AND GTK_INCLUDE_DIR_ATK
   AND GTK_INCLUDE_DIR_CAIRO
   AND GTK_INCLUDE_DIR_PANGO
   AND GTK_INCLUDE_DIR_XML2
   AND GTK_LIBRARY_GTK
   AND GTK_LIBRARY_GTKSOURCEVIEW
   AND GTK_LIBRARY_GTKGL
   AND GTK_LIBRARY_GDKPIXBUF
   AND GTK_LIBRARY_GDKGL
   AND GTK_LIBRARY_GDK
   AND GTK_LIBRARY_GOBJECT
   AND GTK_LIBRARY_GLIB
   AND GTK_LIBRARY_PANGO
   AND GTK_LIBRARY_XML2
   )

   # If gtk, glib and GDK were found we assume the rest is also found

   set(GTK_FOUND "YES")
   set(GTK_INCLUDE_DIRS
      ${GTK_INCLUDE_DIR_GTK}
      ${GTK_INCLUDE_DIR_GLIBCONFIG}
      ${GTK_INCLUDE_DIR_GDKCONFIG}
      ${GTK_INCLUDE_DIR_GDKGLEXTCONFIG}
      ${GTK_INCLUDE_DIR_GLIB}
      ${GTK_INCLUDE_DIR_GTKSOURCEVIEW}
      ${GTK_INCLUDE_DIR_GTKGL}
      ${GTK_INCLUDE_DIR_GDKPIXBUF}
      ${GTK_INCLUDE_DIR_GDK}
      ${GTK_INCLUDE_DIR_ATK}
      ${GTK_INCLUDE_DIR_CAIRO}
      ${GTK_INCLUDE_DIR_PANGO}
      ${GTK_INCLUDE_DIR_XML2}
   )
   set(GTK_LIBRARIES
      ${GTK_LIBRARY_GTK}
      ${GTK_LIBRARY_GTKSOURCEVIEW}
      ${GTK_LIBRARY_GTKGL}
      ${GTK_LIBRARY_GDKPIXBUF}
      ${GTK_LIBRARY_GDKGL}
      ${GTK_LIBRARY_GDK}
      ${GTK_LIBRARY_GOBJECT}
      ${GTK_LIBRARY_GLIB}
      ${GTK_LIBRARY_PANGO}
      ${GTK_LIBRARY_XML2}
   )
endif()
