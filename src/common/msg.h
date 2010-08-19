/**
 * @file msg.c
 * @brief Message IO functions - handles byte ordering and avoids alignment errors
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

#ifndef MSG_H
#define MSG_H

typedef struct sizebuf_s {
	byte *data;					/**< pointer to the data in the buffer */
	int maxsize;				/**< max. size of the allocated buffer */
	int cursize;				/**< current used size of the buffer */
	int readcount;				/**< current read position */
} sizebuf_t;

void SZ_Init(sizebuf_t * buf, byte * data, int length);
void SZ_Clear(sizebuf_t * buf);
void SZ_Write(sizebuf_t * buf, const void *data, int length);

#endif
