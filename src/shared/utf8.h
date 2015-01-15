/**
 * @file
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

#include <stddef.h>

/** Is this the second or later byte of a multibyte UTF-8 character? */
/* The definition of UTF-8 guarantees that the second and later
 * bytes of a multibyte character have high bits 10, and that
 * singlebyte characters and the start of multibyte characters
 * never do. */
#define UTF8_CONTINUATION_BYTE(c) (((c) & 0xc0) == 0x80)

int UTF8_delete_char_at(char* s, int pos);
int UTF8_insert_char_at(char* s, int n, int pos, int codepoint);
int UTF8_char_len(unsigned char c);
int UTF8_next(const char** str);
int UTF8_encoded_len(int codepoint);
size_t UTF8_strlen(const char* str);
int UTF8_char_offset_to_byte_offset (char* str, int pos);
char* UTF8_strncpyz(char* dest, const char* src, size_t limit);
