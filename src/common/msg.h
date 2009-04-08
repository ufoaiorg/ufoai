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
	qboolean allowoverflow;		/**< if false, do a Com_Error */
	qboolean overflowed;		/**< set to true if the buffer size failed */
	byte *data;					/**< pointer to the data in the buffer */
	int maxsize;				/**< max. size of the allocated buffer */
	int cursize;				/**< current used size of the buffer */
	int readcount;				/**< current read position */
} sizebuf_t;

void SZ_Init(sizebuf_t * buf, byte * data, int length);
void SZ_Clear(sizebuf_t * buf);
void *SZ_GetSpace(sizebuf_t * buf, int length);
void SZ_Write(sizebuf_t * buf, const void *data, int length);
void SZ_Print(sizebuf_t * buf, const char *data);	/* strcats onto the sizebuf */


void MSG_WriteChar(sizebuf_t * sb, int c);

#ifdef DEBUG
#define MSG_WriteByte(buffer, char) MSG_WriteByteDebug( buffer, char, __FILE__, __LINE__ )
#define MSG_WriteShort(buffer, char) MSG_WriteShortDebug( buffer, char, __FILE__, __LINE__ )
void MSG_WriteByteDebug(sizebuf_t * sb, int c, const char *file, int line);
void MSG_WriteShortDebug(sizebuf_t * sb, int c, const char* file, int line);
#else
void MSG_WriteByte(sizebuf_t * sb, int c);
void MSG_WriteShort(sizebuf_t * sb, int c);
#endif
void MSG_WriteLong(sizebuf_t * sb, int c);
void MSG_WriteFloat(sizebuf_t * sb, float f);
void MSG_WriteString(sizebuf_t * sb, const char *s);
void MSG_WriteCoord(sizebuf_t * sb, float f);
void MSG_WritePos(sizebuf_t * sb, const vec3_t pos);
void MSG_Write2Pos(sizebuf_t * sb, const vec2_t pos);
void MSG_WriteGPos(sizebuf_t * sb, pos3_t pos);
void MSG_WriteAngle(sizebuf_t * sb, float f);
void MSG_WriteAngle16(sizebuf_t * sb, float f);
void MSG_WriteDir(sizebuf_t * sb, vec3_t vector);
void MSG_vWriteFormat(sizebuf_t * sb, const char *format, va_list ap);
void MSG_WriteFormat(sizebuf_t * sb, const char *format, ...);

void MSG_BeginReading(sizebuf_t * sb);

int MSG_ReadChar(sizebuf_t * sb);
int MSG_ReadByte(sizebuf_t * sb);
int MSG_ReadShort(sizebuf_t * sb);
int MSG_ReadLong(sizebuf_t * sb);
float MSG_ReadFloat(sizebuf_t * sb);
char *MSG_ReadString(sizebuf_t * sb);

float MSG_ReadCoord(sizebuf_t * sb);
void MSG_ReadPos(sizebuf_t * sb, vec3_t pos);
void MSG_Read2Pos(sizebuf_t * sb, vec2_t pos);
void MSG_ReadGPos(sizebuf_t * sb, pos3_t pos);
float MSG_ReadAngle(sizebuf_t * sb);
float MSG_ReadAngle16(sizebuf_t * sb);
void MSG_ReadDir(sizebuf_t * sb, vec3_t vector);

void MSG_ReadData(sizebuf_t * sb, void *buffer, int size);
void MSG_vReadFormat(sizebuf_t * sb, const char *format, va_list ap);
void MSG_ReadFormat(sizebuf_t * sb, const char *format, ...);

int MSG_LengthFormat(sizebuf_t * sb, const char *format);

#endif
