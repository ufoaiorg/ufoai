/**
 * @file netpack.h
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/
Copyright (C) 1997-2001 Id SoftwaR_ Inc.

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

#ifndef _COMMON_NETPACK_H
#define _COMMON_NETPACK_H

#define NUMVERTEXNORMALS	162
extern const vec3_t bytedirs[NUMVERTEXNORMALS];

void NET_WriteChar(struct dbuffer *buf, char c);
void NET_WriteByte(struct dbuffer *buf, byte c);
void NET_WriteShort(struct dbuffer *buf, int c);
void NET_WriteLong(struct dbuffer *buf, int c);
void NET_WriteString(struct dbuffer *buf, const char *str);
void NET_WriteRawString(struct dbuffer *buf, const char *str);
void NET_WriteCoord(struct dbuffer *buf, float f);
void NET_WritePos(struct dbuffer *buf, const vec3_t pos);
void NET_Write2Pos(struct dbuffer *buf, const vec2_t pos);
void NET_WriteGPos(struct dbuffer *buf, const pos3_t pos);
void NET_WriteAngle(struct dbuffer *buf, float f);
void NET_WriteAngle16(struct dbuffer *buf, float f);
void NET_WriteDir(struct dbuffer *buf, const vec3_t vector);
void NET_vWriteFormat(struct dbuffer *buf, const char *format, va_list ap);
void NET_WriteFormat(struct dbuffer *buf, const char *format, ...);
void NET_VPrintf(struct dbuffer *buf, const char *format, va_list ap, char *str, size_t length);

/* This frees the buf argument */
void NET_WriteMsg(struct net_stream *s, struct dbuffer *buf);

/* This keeps buf intact */
void NET_WriteConstMsg(struct net_stream *s, const struct dbuffer *buf);

void NET_OOB_Printf(struct net_stream *s, const char *format, ...) __attribute__((format(printf,2,3)));

int NET_ReadChar(struct dbuffer *buf);
int NET_ReadByte(struct dbuffer *buf);
int NET_ReadShort(struct dbuffer *buf);
int NET_PeekShort(const struct dbuffer *buf);
int NET_ReadLong(struct dbuffer *buf);
int NET_ReadString(struct dbuffer *buf, char *string, size_t length);
int NET_ReadStringLine(struct dbuffer *buf, char *string, size_t length);

float NET_ReadCoord(struct dbuffer *buf);
void NET_ReadPos(struct dbuffer *buf, vec3_t pos);
void NET_Read2Pos(struct dbuffer *buf, vec2_t pos);
void NET_ReadGPos(struct dbuffer *buf, pos3_t pos);
float NET_ReadAngle(struct dbuffer *buf);
float NET_ReadAngle16(struct dbuffer *buf);
void NET_ReadDir(struct dbuffer *buf, vec3_t vector);

void NET_ReadData(struct dbuffer *buf, void *buffer, int size);
void NET_vReadFormat(struct dbuffer *buf, const char *format, va_list ap);
void NET_ReadFormat(struct dbuffer *buf, const char *format, ...);

struct dbuffer *NET_ReadMsg(struct net_stream *s);

#endif
