# Copyright (C) 2002-2023 UFO: Alien Invasion.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#[=======================================================================[.rst:
FindMXML
-------

Locate the Mini-XML library

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``MXML_INCLUDE_DIR``
  where to find mxml.h
``MXML_LIBRARY``
  the name of the library to link against
``MXML_FOUND``
  if false, do not try to link to MXML
``MXML_VERSION``
  the human-readable string containing the version of MXML if found
``MXML_VERSION_MAJOR``
  MXML major version
``MXML_VERSION_MINOR``
  MXML minor version

#]=======================================================================]

find_path(MXML_INCLUDE_DIR mxml.h)
find_library(MXML_LIBRARY NAMES mxml)

if(MXML_INCLUDE_DIR AND EXISTS "${MXML_INCLUDE_DIR}/mxml.h")
	file(STRINGS "${MXML_INCLUDE_DIR}/mxml.h" MXML_VERSION_MAJOR_LINE REGEX "^#[ \t]+define[ \t]+MXML_MAJOR_VERSION[ \t]+[0-9]+")
	file(STRINGS "${MXML_INCLUDE_DIR}/mxml.h" MXML_VERSION_MINOR_LINE REGEX "^#[ \t]+define[ \t]+MXML_MINOR_VERSION[ \t]+[0-9]+")
	string(REGEX REPLACE "^#[ \t]+define[ \t]+MXML_MAJOR_VERSION[ \t]+([0-9]+).*$" "\\1" MXML_VERSION_MAJOR "${MXML_VERSION_MAJOR_LINE}")
	string(REGEX REPLACE "^#[ \t]+define[ \t]+MXML_MINOR_VERSION[ \t]+([0-9]+).*$" "\\1" MXML_VERSION_MINOR "${MXML_VERSION_MINOR_LINE}")
	unset(MXML_VERSION_MAJOR_LINE)
	unset(MXML_VERSION_MINOR_LINE)
	if (MXML_VERSION_MAJOR AND MXML_VERSION_MINOR)
		set(MXML_VERSION ${MXML_VERSION_MAJOR}.${MXML_VERSION_MINOR})
	endif()
endif()

find_package_handle_standard_args(MXML
	REQUIRED_VARS MXML_LIBRARY MXML_INCLUDE_DIR
	VERSION_VAR MXML_VERSION
)
