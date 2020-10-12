# - try to find GTKSourceView2 module required for compiling UFORadiant
#  GTKSourceView2_INCLUDE_DIR   - Directories to include to use GTKSourceView2
#  GTKSourceView2_LIBRARY       - Files to link against to use GTKSourceView2
#  GTKSourceView2_FOUND         - GTKSourceView2 was found

find_path(GTKSourceView2_INCLUDE_DIR NAMES gtksourceview/gtksourceview.h
   PATH_SUFFIXES gtksourceview-2.0
   PATHS
   /usr/openwin/share/include
   /usr/lib/glib/include
   /opt/gnome/include
)

find_library(GTKSourceView2_LIBRARY
   NAMES  gtksourceview gtksourceview-2.0
   PATHS
   /lib
   /usr/lib
   /usr/openwin/lib
   /opt/gnome/lib
)

if (GTKSourceView2_INCLUDE_DIR AND
    GTKSourceView2_LIBRARY
   )
   set(GTKSourceView2_FOUND "YES")
   message(STATUS "GTKSourceView2 found: " ${GTKSourceView2_LIBRARY})
else()
   message(FATAL_ERROR "Couldn't find GTKSourceView2 !!")
endif()
