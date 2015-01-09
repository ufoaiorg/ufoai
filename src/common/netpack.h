/**
 * @file
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#define NUMVERTEXNORMALS	162
extern const vec3_t bytedirs[NUMVERTEXNORMALS];

void NET_WriteChar(dbuffer* buf, char c);
void NET_WriteByte(dbuffer* buf, byte c);
void NET_WriteShort(dbuffer* buf, int c);
void NET_WriteLong(dbuffer* buf, int c);
void NET_WriteString(dbuffer* buf, const char* str);
void NET_WriteRawString(dbuffer* buf, const char* str);
void NET_WriteCoord(dbuffer* buf, float f);
void NET_WritePos(dbuffer* buf, const vec3_t pos);
void NET_Write2Pos(dbuffer* buf, const vec2_t pos);
void NET_WriteGPos(dbuffer* buf, const pos3_t pos);
void NET_WriteAngle(dbuffer* buf, float f);
void NET_WriteAngle16(dbuffer* buf, float f);
void NET_WriteDir(dbuffer* buf, const vec3_t vector);
void NET_vWriteFormat(dbuffer* buf, const char* format, va_list ap);
void NET_WriteFormat(dbuffer* buf, const char* format, ...);
void NET_VPrintf(dbuffer* buf, const char* format, va_list ap, char* str, size_t length);

/* This frees the buf argument */
void NET_WriteMsg(struct net_stream* s, dbuffer& buf);

/* This keeps buf intact */
void NET_WriteConstMsg(struct net_stream* s, const dbuffer& buf);

void NET_OOB_Printf(struct net_stream* s, const char* format, ...) __attribute__((format(__printf__,2,3)));

int NET_ReadChar(dbuffer* buf);
int NET_ReadByte(dbuffer* buf);
int NET_ReadShort(dbuffer* buf);
int NET_PeekByte(const dbuffer* buf);
int NET_PeekShort(const dbuffer* buf);
int NET_PeekLong(const dbuffer* buf);
int NET_ReadLong(dbuffer* buf);
int NET_ReadString(dbuffer* buf, char* string, size_t length);
int NET_ReadStringLine(dbuffer* buf, char* string, size_t length);

float NET_ReadCoord(dbuffer* buf);
void NET_ReadPos(dbuffer* buf, vec3_t pos);
void NET_Read2Pos(dbuffer* buf, vec2_t pos);
void NET_ReadGPos(dbuffer* buf, pos3_t pos);
float NET_ReadAngle(dbuffer* buf);
float NET_ReadAngle16(dbuffer* buf);
void NET_ReadDir(dbuffer* buf, vec3_t vector);

void NET_ReadData(dbuffer* buf, void* buffer, int size);
void NET_vReadFormat(dbuffer* buf, const char* format, va_list ap);
void NET_ReadFormat(dbuffer* buf, const char* format, ...);
void NET_SkipFormat(dbuffer* buf, const char* format);
