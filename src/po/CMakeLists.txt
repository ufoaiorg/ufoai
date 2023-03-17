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

# Setting up Internationalisation (i18n)
find_package(Intl REQUIRED)
find_package(Gettext REQUIRED)

find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext)
find_program(GETTEXT_MSGMERGE_EXECUTABLE msgmerge)
find_program(GETTEXT_MSGFMT_EXECUTABLE msgfmt)

add_custom_target(lang ALL)
        
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/LINGUAS"
)

if (GETTEXT_MSGFMT_EXECUTABLE)

    message(DEBUG " msgmerge: ${GETTEXT_MSGFMT_EXECUTABLE}")
    message(DEBUG " i18n domains: ${TEXTDOMAINS}")

    foreach(TEXTDOMAIN IN ITEMS ${TEXTDOMAINS})
        file(GLOB ${TEXTDOMAIN}_POFILES ${CMAKE_CURRENT_SOURCE_DIR}/${TEXTDOMAIN}-*.po)
        if("${TEXTDOMAIN}" STREQUAL "${PROJECT_NAME}")
	    set(OUTDIR base)
	elseif("${TEXTDOMAIN}" STREQUAL "uforadiant")
	    set(OUTDIR radiant)
        endif()
        foreach(POFILE IN ITEMS ${${TEXTDOMAIN}_POFILES})
            string(REGEX REPLACE ".*${TEXTDOMAIN}-([A-Za-z_]+)\.po" "\\1" LANG "${POFILE}")
            set(MO_DIRECTORY ${CMAKE_BINARY_DIR}/${OUTDIR}/i18n/${LANG}/LC_MESSAGES)
	    set(MOFILE ${MO_DIRECTORY}/${TEXTDOMAIN}.mo)
	    string(APPEND ${TEXTDOMAIN}_LINGUAS "${LANG};")
	    string(APPEND ${TEXTDOMAIN}_MO_DIRECTORIES "${MO_DIRECTORY};")
	    string(APPEND ${TEXTDOMAIN}_MOFILES "${MOFILE};")
        endforeach(POFILE)

	message(DEBUG " ${TEXTDOMAIN} linguas: ${${TEXTDOMAIN}_LINGUAS}")

	file(MAKE_DIRECTORY ${${TEXTDOMAIN}_MO_DIRECTORIES})

        add_custom_target(
	    ${TEXTDOMAIN}_mofiles
	    DEPENDS ${${TEXTDOMAIN}_MOFILES}
        )
        
        add_dependencies(lang ${TEXTDOMAIN}_mofiles)

        foreach(LANG IN ITEMS ${${TEXTDOMAIN}_LINGUAS})
	    set(POFILE ${TEXTDOMAIN}-${LANG}.po)
	    set(MO_DIRECTORY ${CMAKE_BINARY_DIR}/${OUTDIR}/i18n/${LANG}/LC_MESSAGES)
	    set(MOFILE ${MO_DIRECTORY}/${TEXTDOMAIN}.mo)
            add_custom_command(OUTPUT ${MOFILE}
		COMMAND
                    ${GETTEXT_MSGFMT_EXECUTABLE}
	                --output-file=${MOFILE}
                        ${POFILE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		DEPENDS ${POFILE}
                COMMENT "${TEXTDOMAIN} po-compile: ${LANG}"
		VERBATIM
            )
	endforeach(LANG)

    endforeach(TEXTDOMAIN)

endif (GETTEXT_MSGFMT_EXECUTABLE)
