# Find OGG includes and library
# This module defines
#  OGG_INCLUDE_DIR, where to find OGG headers.
#  OGG_LIBRARIES, the libraries needed to use OGG.
#  OGG_FOUND, If false, do not try to use OGG.
# also defined, but not for general use are
#  OGG_LIBRARY, where to find the OGG library.

find_path(OGG_INCLUDE_DIR ogg/ogg.h)

find_library(OGG_LIBRARY NAMES ogg)

# handle the QUIETLY and REQUIRED arguments and set OGG_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(OGG DEFAULT_MSG OGG_LIBRARY OGG_INCLUDE_DIR)

if(OGG_FOUND)
  set(OGG_LIBRARIES ${OGG_LIBRARY})
endif()

mark_as_advanced(OGG_LIBRARY OGG_INCLUDE_DIR)
