# - try to find GTKGL modules required for compiling UFORadiant
#  GTKGL_INCLUDE_DIRS   - Directories to include to use GTKGL
#  GTKGL_LIBRARIES     - Files to link against to use GTKGL
#  GTKGL_FOUND         - GTKGL was found

find_path(GTKGL_INCLUDE_DIR_GTKGL NAMES gtk/gtkgl.h
   PATH_SUFFIXES gtkglext-1.0
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

# gdkglext-config.h may be in a different directory than gtk/glib
find_path(GTKGL_INCLUDE_DIR_GDKGLEXTCONFIG NAMES gdkglext-config.h
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

find_library(GTKGL_LIBRARY_GTKGL
   NAMES  gtkglext gtkglext-x11 gtkglext-x11-1.0 gtkglext-win32-1.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTKGL_LIBRARY_GDKGL
   NAMES  gdkgl gdkglext-x11 gdkglext-x11-1.0 gdkglext-win32-1.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

if(GTKGL_INCLUDE_DIR_GTKGL
    AND GTKGL_INCLUDE_DIR_GDKGLEXTCONFIG
    AND GTKGL_LIBRARY_GTKGL
    AND GTKGL_LIBRARY_GDKGL)

   set(GTKGL_FOUND "YES")
   set(GTKGL_INCLUDE_DIRS
      ${GTKGL_INCLUDE_DIR_GDKGLEXTCONFIG}
      ${GTKGL_INCLUDE_DIR_GTKGL}
   )
   set(GTKGL_LIBRARIES
      ${GTKGL_LIBRARY_GTKGL}
      ${GTKGL_LIBRARY_GDKGL}
   )
   message(STATUS "GTKGL Found: " ${GTKGL_LIBRARIES})
else()
   message(FATAL_ERROR "Couldn't find GTKGL !!")
endif()
