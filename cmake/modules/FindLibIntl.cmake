# Find LIBINTL includes and library
# This module defines
#  LIBINTL_INCLUDE_DIR, where to find LIBINTL headers.
#  LIBINTL_LIBRARIES, the libraries needed to use LIBINTL.
#  LIBINTL_FOUND, If false, do not try to use LIBINTL.
# also defined, but not for general use are
#  LIBINTL_LIBRARY, where to find the LIBINTL library.

find_path(LIBINTL_INCLUDE_DIR libintl.h)

find_library(LIBINTL_LIBRARY NAMES intl)

# handle the QUIETLY and REQUIRED arguments and set LIBINTL_FOUND to TRUE if
# all listed variables are TRUE
include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBINTL DEFAULT_MSG LIBINTL_LIBRARY LIBINTL_INCLUDE_DIR)

if(LIBINTL_FOUND)
  set(LIBINTL_LIBRARIES ${LIBINTL_LIBRARY})
endif()

mark_as_advanced(LIBINTL_LIBRARY LIBINTL_INCLUDE_DIR)
