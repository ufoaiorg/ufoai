/**
 * @file
 * @brief Shared parsing functions.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "ufotypes.h"
#include "cxx.h"

enum Com_TokenType_t {
	TT_EOF = 0,
	TT_BEGIN_BLOCK = '{',
	TT_END_BLOCK = '}',
	TT_COMMA = ',',
	TT_BEGIN_LIST = '(',
	TT_END_LIST = ')',

	TT_CONTENT = 0xFF,
	TT_WORD,
	TT_QUOTED_WORD
};

const char* Com_GetToken(const char** data_p);
Com_TokenType_t Com_GetType(const char** data_p);
Com_TokenType_t Com_NextToken(const char** data_p);

const char* Com_Parse(const char** data_p, char* target = nullptr, size_t size = 0, bool replaceWhitespaces = true);
int Com_CountTokensInBuffer(const char* buffer);
void Com_UnParseLastToken(void);
void Com_SkipBlock(const char** text);
int Com_GetBlock(const char** text, const char** start);
