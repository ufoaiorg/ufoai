/**
 * @file parsed.h
 * @brief Shared parsing functions.
 */

/*
All original materal Copyright (C) 2002-2009 UFO: Alien Invasion team.

Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


/* flags for Com_Parse_O */
#define PARSE_NO_COMMENTS 			0 /**< Com_Parse_O ignores all comments */
#define PARSE_C_STYLE_COMMENTS 		1 /**< Com_Parse_O does not ignore c-style comments / * */
#define PARSE_CPP_STYLE_COMMENTS 	2 /**< Com_Parse_O does not ignore c++ style comments "//" */

const char *COM_Parse_O(const char **data_p, const int options);
const char *COM_Parse(const char **data_p);

