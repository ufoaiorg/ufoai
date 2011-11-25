/**
 * @file src/shared/parse.h
 * @brief Shared parsing functions.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef PARSE_H
#define PARSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ufotypes.h"

const char *Com_Parse(const char **data_p);
int Com_CountTokensInBuffer(const char *buffer);
void Com_UnParseLastToken(void);
qboolean Com_ParsedTokenIsQuoted(void);
void Com_EnableFunctionScriptToken(qboolean enable);

#ifdef __cplusplus
}
#endif

#endif
