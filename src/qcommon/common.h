/**
 * @brief Common stuff.
 * @file common.h
 *
 */

/*
 * All original material Copyright (C) 2002-2006 UFO: Alien Invasion team.
 *
 * Derived from Quake 2 v3.21: quake2-2.31/qcommon/qcommon.h
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef COMMON_COMMON_H
#define COMMON_COMMON_H

#include "common/ufotypes.h"
#include "game/q_shared.h"

/*============================================================================ */

typedef struct sizebuf_s {
	bool_t allowoverflow;		/* if false, do a Com_Error */
	bool_t overflowed;		/* set to true if the buffer size failed */
	void *data;
	size_t maxsize;
	size_t cursize;
	int readcount;
} sizebuf_t;

void SZ_Init(sizebuf_t *buf, void *data, int length);
void SZ_Clear(sizebuf_t *buf);
void *SZ_GetSpace(sizebuf_t *buf, int length);
void SZ_Write(sizebuf_t *buf, void *data, int length);
void SZ_WriteString(sizebuf_t *buf, char *data);	/* strcats onto the sizebuf */

/*============================================================================ */

struct usercmd_s;
struct entity_state_s;

void MSG_WriteChar(sizebuf_t *sb, char c);
void MSG_WriteByte(sizebuf_t *sb, uint8_t c);
void MSG_WriteShort(sizebuf_t *sb, int16_t c);

void MSG_WriteLong(sizebuf_t *sb, int c);
void MSG_WriteFloat(sizebuf_t *sb, float f);
void MSG_WriteString(sizebuf_t *sb, char *s);
void MSG_WriteCoord(sizebuf_t *sb, float f);
void MSG_WritePos(sizebuf_t *sb, vec3_t pos);
void MSG_WriteGPos(sizebuf_t *sb, pos3_t pos);
void MSG_WriteAngle(sizebuf_t *sb, float f);
void MSG_WriteAngle16(sizebuf_t *sb, float f);
void MSG_WriteDir(sizebuf_t *sb, vec3_t vector);
void MSG_WriteFormat(sizebuf_t *sb, char *format, ...);


void MSG_BeginReading(sizebuf_t *sb);

char MSG_ReadChar(sizebuf_t *sb);
uint8_t MSG_ReadByte(sizebuf_t *sb);
uint16_t MSG_ReadShort(sizebuf_t *sb);
int MSG_ReadLong(sizebuf_t *sb);
float MSG_ReadFloat(sizebuf_t *sb);
char *MSG_ReadString(sizebuf_t *sb);
char *MSG_ReadStringLine(sizebuf_t *sb);

float MSG_ReadCoord(sizebuf_t *sb);
void MSG_ReadPos(sizebuf_t *sb, vec3_t pos);
void MSG_ReadGPos(sizebuf_t *sb, pos3_t pos);
float MSG_ReadAngle(sizebuf_t *sb);
float MSG_ReadAngle16(sizebuf_t *sb);
void MSG_ReadDir(sizebuf_t *sb, vec3_t vector);

void MSG_ReadData(sizebuf_t *sb, void *buffer, int size);
void MSG_ReadFormat(sizebuf_t *sb, char *format, ...);

int MSG_LengthFormat(sizebuf_t *sb, char *format);

/*============================================================================ */

extern qboolean bigendien;

extern short BigShort(short l);
extern short LittleShort(short l);
extern int BigLong(int l);
extern int LittleLong(int l);
extern float BigFloat(float l);
extern float LittleFloat(float l);

/*============================================================================ */


int COM_Argc(void);
char *COM_Argv(int arg);		/* range and null checked */
void COM_ClearArgv(int arg);
int COM_CheckParm(char *parm);
void COM_AddParm(char *parm);

void COM_Init(void);
void COM_InitArgv(int argc, char **argv);

char *CopyString(char *in);

/*============================================================================ */

#endif /* COMMON_COMMON_H */
