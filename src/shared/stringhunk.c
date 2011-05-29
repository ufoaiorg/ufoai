/**
 * @file stringhunk.c
 * @brief String hunk management
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "stringhunk.h"
#include "../common/common.h"

void STRHUNK_Add (stringHunk_t *hunk, const char *string)
{
	const size_t remaining = hunk->size - (ptrdiff_t)(hunk->pos - hunk->hunk);
	const size_t strLength = strlen(string);

	if (hunk->pos != hunk->hunk)
		*hunk->pos++ = '\xFF';

	Q_strncpyz(hunk->pos, string, remaining - 1);
	hunk->pos += strLength + 1;
	hunk->entries++;
}

void STRHUNK_Reset (stringHunk_t *hunk)
{
	hunk->pos = hunk->hunk;
	hunk->entries = 0;
	hunk->hunk[0] = '\0';
}

void STRHUNK_Visit (stringHunk_t *hunk, stringHunkVisitor_t visitor)
{
	const char *string = hunk->hunk;
	for (;;) {
		visitor(string);
		string += strlen(string) + 1;
		if (*string != '\xFF')
			break;
		string++;
	}
}

int STRHUNK_Size (const stringHunk_t *hunk)
{
	return hunk->entries;
}

size_t STRHUNK_Free (const stringHunk_t *hunk)
{
	return hunk->size - (ptrdiff_t)(hunk->pos - hunk->hunk);
}

stringHunk_t *STRHUNK_Create (size_t size)
{
	stringHunk_t *strHunk = Mem_AllocType(stringHunk_t);
	strHunk->size = size;
	strHunk->hunk = Mem_Alloc(size);
	strHunk->pos = strHunk->hunk;
	return strHunk;
}

void STRHUNK_Delete (stringHunk_t **hunk)
{
	Mem_Free(*hunk);
	*hunk = NULL;
}
