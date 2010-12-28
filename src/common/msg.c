/**
 * @file msg.c
 * @brief Message IO functions - handles size buffers
 */

/*
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

#include "common.h"
#include "msg.h"

void SZ_Init (sizebuf_t * buf, byte * data, int length)
{
	OBJZERO(*buf);
	buf->data = data;
	buf->maxsize = length;
}

void SZ_Clear (sizebuf_t * buf)
{
	buf->cursize = 0;
}

static void *SZ_GetSpace (sizebuf_t * buf, int length)
{
	void *data;

	if (buf->cursize + length > buf->maxsize)
		Com_Error(ERR_FATAL, "SZ_GetSpace: overflow");

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write (sizebuf_t * buf, const void *data, int length)
{
	memcpy(SZ_GetSpace(buf, length), data, length);
}
