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

# Project settings
project(doxygen-docs DESCRIPTION "Doxygen documentation generating" LANGUAGES NONE)

find_package(Doxygen REQUIRED OPTIONAL_COMPONENTS dot)

set(SRC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/base/shaders)
set(DOXYGEN_PROJECT_NAME "UFO: Alien Invasion")
set(DOXYGEN_OUTPUT_DIRECTORY doxygen)
set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
set(DOXYGEN_TYPEDEF_HIDES_STRUCT YES)
set(DOXYGEN_EXTRACT_ALL YES)
set(DOXYGEN_EXTRACT_PRIVATE YES)
set(DOXYGEN_EXTRACT_STATIC YES)
set(DOXYGEN_EXTRACT_LOCAL_METHODS YES)
set(DOXYGEN_INTERNAL_DOCS YES)
set(DOXYGEN_QUIET YES)
set(DOXYGEN_WARN_NO_PARAMDOC YES)
set(DOXYGEN_WARN_LOGFILE doxygen.warnings)
set(DOXYGEN_SOURCE_BROWSER YES)
set(DOXYGEN_REFERENCED_BY_RELATION YES)
set(DOXYGEN_REFERENCES_RELATION YES)
set(DOXYGEN_HTML_TIMESTAMP YES)
set(DOXYGEN_GENERATE_TREEVIEW YES)
set(DOXYGEN_GENERATE_TAGFILE ${CMAKE_CURRENT_BINARY_DIR}/ufosource.tag)
set(DOXYGEN_UML_LOOK YES)
set(DOXYGEN_FILE_PATTERNS *.c *.cpp *.h *.m *.mm *.glsl)
set(DOXYGEN_EXCLUDE
	${CMAKE_SOURCE_DIR}/src/libs
	${CMAKE_SOURCE_DIR}/src/tools/radiant
)

doxygen_add_docs(${PROJECT_NAME} ${SRC} COMMENT "Compile the developer code documentation")

set_target_properties(${PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES "doxygen.warnings;ufosource.tag")
