/**
 * @file stringhunk.h
 * @brief Header for string hunk management
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

#ifndef STRINGHUNK_H
#define STRINGHUNK_H

#include "ufotypes.h"

typedef struct stringHunk_s {
	size_t size;
	char *pos;
	char *hunk;
	int entries;
} stringHunk_t;

typedef void (*stringHunkVisitor_t)(const char *string);

qboolean STRHUNK_Add(stringHunk_t *hunk, const char *string);
void STRHUNK_Reset(stringHunk_t *hunk);
void STRHUNK_Visit(stringHunk_t *hunk, stringHunkVisitor_t visitor);
stringHunk_t *STRHUNK_Create(size_t size);
void STRHUNK_Delete(stringHunk_t **hunk);
int STRHUNK_Size(const stringHunk_t *hunk);
size_t STRHUNK_GetFreeSpace(const stringHunk_t *hunk);

#endif
