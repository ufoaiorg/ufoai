# Find OPENAL includes and library
# This module defines
#  OPENAL_INCLUDE_DIR, where to find OPENAL headers.
#  OPENAL_LIBRARIES, the libraries needed to use OPENAL.
#  OPENAL_FOUND, If false, do not try to use OPENAL.
# also defined, but not for general use are
#  OPENAL_LIBRARY, where to find the OPENAL library.

if(APPLE)
   set(OPENAL_PATH_SEARCH "OpenAL/al.h")
else()
   set(OPENAL_PATH_SEARCH "AL/al.h")
endif()

find_path(OPENAL_INCLUDE_DIR ${OPENAL_PATH_SEARCH}
  HINTS
    ENV OPENALDIR
  PATH_SUFFIXES include
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt
  [HKEY_LOCAL_MACHINE\\SOFTWARE\\Creative\ Labs\\OpenAL\ 1.1\ Software\ Development\ Kit\\1.00.0000;InstallDir]
)

find_library(OPENAL_LIBRARY
  NAMES OpenAL al openal OpenAL32
  HINTS
    ENV OPENALDIR
  PATH_SUFFIXES lib64 lib libs64 libs libs/Win32 libs/Win64
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /sw
  /opt/local
  /opt/csw
  /opt
  [HKEY_LOCAL_MACHINE\\SOFTWARE\\Creative\ Labs\\OpenAL\ 1.1\ Software\ Development\ Kit\\1.00.0000;InstallDir]
)

# handle the QUIETLY and REQUIRED arguments and set OPENAL_FOUND to TRUE if
# all listed variables are TRUE
include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OPENAL DEFAULT_MSG OPENAL_LIBRARY OPENAL_INCLUDE_DIR)

if(OPENAL_FOUND)
  set(OPENAL_LIBRARIES ${OPENAL_LIBRARY})
endif()

mark_as_advanced(OPENAL_LIBRARY OPENAL_INCLUDE_DIR)
