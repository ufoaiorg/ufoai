# Find VORBIS includes and library
# This module defines
#  VORBIS_INCLUDE_DIR, where to find VORBIS headers.
#  VORBIS_LIBRARIES, the libraries needed to use VORBIS.
#  VORBIS_FOUND, If false, do not try to use VORBIS.
#  VORBIS_LIBRARY, where to find the VORBIS library.
#  VORBIS_LIBRARY_FILE, where to find the VORBIS file library.

find_path(VORBIS_INCLUDE_DIR vorbis/vorbisfile.h)

find_library(VORBIS_LIBRARY NAMES vorbis)

find_library(VORBIS_LIBRARY_FILE NAMES vorbisfile)

# handle the QUIETLY and REQUIRED arguments and set VORBIS_FOUND to TRUE if
# all listed variables are TRUE
include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(VORBIS DEFAULT_MSG VORBIS_LIBRARY VORBIS_LIBRARY_FILE)

if(VORBIS_FOUND)
   set(VORBIS_LIBRARIES
      ${VORBIS_LIBRARY_FILE}
      ${VORBIS_LIBRARY}
   )
endif()

mark_as_advanced(VORBIS_LIBRARY)
