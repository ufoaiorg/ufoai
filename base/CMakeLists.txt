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
project(base DESCRIPTION "Generate the pk3 archives for the installers" LANGUAGES NONE)

set(PACKS base materials ufos shaders media videos music snd models textures pics)

file(GLOB_RECURSE base RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        *.cfg mapcycle*.txt ai/*.lua save/*/*)
file(GLOB_RECURSE materials RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        materials/*.mat)
file(GLOB_RECURSE ufos RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        ufos/*.ufo ufos/ui/*.lua)
file(GLOB_RECURSE shaders RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        shaders/*.glsl)
file(GLOB_RECURSE media RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        media/*.ttf media/*.otf)
file(GLOB_RECURSE videos RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        videos/*.roq videos/*.ogm)
file(GLOB_RECURSE music RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        music/*.ogg)
file(GLOB_RECURSE snd RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        sound/*.ogg sound/*.wav)
file(GLOB_RECURSE pics RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        pics/*.jpg pics/*.tga pics/*.png)
file(GLOB_RECURSE textures RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        textures/*.jpg textures/*.tga textures/*.png)
file(GLOB_RECURSE models RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
        models/*.mdx models/*.md2 models/*.md3 models/*.dpm models/*.obj
        models/*.jpg models/*.png models/*.tga models/*.anm)

add_custom_target(pk3)

foreach(PK3 IN ITEMS ${PACKS})
   add_custom_command(OUTPUT 0${PK3}.pk3
      COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_CURRENT_BINARY_DIR}/0${PK3}.pk3 --format=zip ${${PK3}}
      DEPENDS "${${PK3}}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Creating ${PK3} pack..."
   )
   add_custom_target(0${PK3} DEPENDS 0${PK3}.pk3)
   add_dependencies(pk3 0${PK3})
endforeach(PK3)
