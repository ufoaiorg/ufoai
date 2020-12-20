# - try to find GTKGL modules required for compiling UFORadiant
#  GTKGL_INCLUDE_DIRS   - Directories to include to use GTKGL
#  GTKGL_LIBRARIES     - Files to link against to use GTKGL
#  GTKGL_FOUND         - GTKGL was found

find_path(GTKGL_INCLUDE_DIR_GTKGL NAMES gtk/gtkgl.h
   PATH_SUFFIXES gtkglext-1.0
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /usr/lib64/glib/include
   /usr/lib32/glib/include
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
   /usr/lib64/gtkglext-1.0/include
   /usr/lib32/gtkglext-1.0/include
)

find_library(GTKGL_LIBRARY_GTKGL
   NAMES  gtkglext gtkglext-x11 gtkglext-x11-1.0 gtkglext-win32-1.0
   PATHS
   /lib
   /lib64
   /lib32
   /usr/lib
   /usr/lib64
   /usr/lib32
   /usr/openwin/lib
   /opt/gnome/lib
)

find_library(GTKGL_LIBRARY_GDKGL
   NAMES  gdkgl gdkglext-x11 gdkglext-x11-1.0 gdkglext-win32-1.0
   PATHS
   /lib
   /lib64
   /lib32
   /usr/lib
   /usr/lib64
   /usr/lib32
   /usr/openwin/lib
   /opt/gnome/lib
)

if(GTKGL_INCLUDE_DIR_GTKGL)
   message(STATUS "Found gtkgl.h: ${GTKGL_INCLUDE_DIR_GTKGL}")
else()
   message(FATAL_ERROR "Couldn't find gtkgl.h!!")
endif()

if(GTKGL_INCLUDE_DIR_GDKGLEXTCONFIG)
   message(STATUS "Found gdkglext-config.h: ${GTKGL_INCLUDE_DIR_GDKGLEXTCONFIG}")
else()
   message(FATAL_ERROR "Couldn't find gdkglext-config.h!!")
endif()

if(GTKGL_LIBRARY_GTKGL)
   message(STATUS "Found library gtkglext: ${GTKGL_LIBRARY_GTKGL}")
else()
   message(FATAL_ERROR "Couldn't find library gtkglext!!")
endif()

if(GTKGL_LIBRARY_GDKGL)
   message(STATUS "Found library gdkgl: ${GTKGL_LIBRARY_GDKGL}")
else()
   message(FATAL_ERROR "Couldn't find library gdkgl!!")
endif()

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
