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
project(manual DESCRIPTION "The user manual(s)" LANGUAGES NONE)

find_package(LATEX COMPONENTS PDFLATEX REQUIRED) 

add_custom_target(manual) # Entry point of execution.

file(GLOB TEX_MANUALS ufo-manual_*.tex)

foreach (TEX_MANUAL IN ITEMS ${TEX_MANUALS})
	string(REGEX REPLACE ".*ufo-manual_([A-Za-z_]+)\.tex" "\\1" LANG "${TEX_MANUAL}")
	set(PDF_MANUAL ufo-manual_${LANG}.pdf)
	set(HEADER header_${LANG}.tex)
	file(GLOB CHAPTERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} chapters_${LANG}/*.tex)
	string(REPLACE ".tex" ".aux" BYPRODUCTS "${CHAPTERS}")
	list(APPEND BYPRODUCTS
		header_${LANG}.aux
		ufo-manual_${LANG}.aux
		ufo-manual_${LANG}.log
		ufo-manual_${LANG}.out
		ufo-manual_${LANG}.toc
		ufo-manual_${LANG}.pdfsync
	)

	add_custom_command(OUTPUT ${PDF_MANUAL}
		COMMAND ${PDFLATEX_COMPILER} -output-directory=${CMAKE_BINARY_DIR}/src/docs/tex ${TEX_MANUAL}
		COMMAND ${PDFLATEX_COMPILER} -output-directory=${CMAKE_BINARY_DIR}/src/docs/tex ${TEX_MANUAL}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Assembling the pdf manual for lang ${LANG}."
		DEPENDS ${TEX_MANUAL} ${HEADER} ${CHAPTERS}
		BYPRODUCTS ${BYPRODUCTS}
	)

	add_custom_target(manual-${LANG} DEPENDS ${PDF_MANUAL})

	add_dependencies(manual manual-${LANG})
endforeach(TEX_MANUAL)
