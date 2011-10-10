/**
 * @file cl_keys.h
 * @brief Header file for keyboard handler.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/keys.h
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

#ifndef CLIENT_KEY_H
#define CLIENT_KEY_H

/* these are the key numbers that should be passed to KeyEvent */

/* normal keys should be passed as lowercased ascii */

typedef enum {
	K_FIRST_KEY,

	K_BACKSPACE = 8,
	K_TAB = 9,
	K_ENTER = 13,
	K_PAUSE = 19,
	K_ESCAPE = 27,
	K_SPACE = 32,
	K_DEL = 127,

	K_MOUSE1 = 200,
	K_MOUSE2 = 201,
	K_MOUSE3 = 202,
	K_MWHEELDOWN = 239,
	K_MWHEELUP = 240,
	K_MOUSE4 = 241,
	K_MOUSE5 = 242,

	K_KP_INS = 256,
	K_KP_END = 257,
	K_KP_DOWNARROW = 258,
	K_KP_PGDN = 259,
	K_KP_LEFTARROW = 260,
	K_KP_5 = 261,
	K_KP_RIGHTARROW = 262,
	K_KP_HOME = 263,
	K_KP_UPARROW = 264,
	K_KP_PGUP = 265,
	K_KP_DEL = 266,
	K_KP_SLASH = 267,
	K_KP_MINUS = 269,
	K_KP_PLUS = 270,
	K_KP_ENTER = 271,
	K_KP_NUMLOCK = 272,

	K_UPARROW = 273,
	K_DOWNARROW = 274,
	K_RIGHTARROW = 275,
	K_LEFTARROW = 276,

	K_HOME = 278,
	K_END = 279,
	K_PGUP = 280,
	K_PGDN = 281,
	K_F1 = 282,
	K_F2 = 283,
	K_F3 = 284,
	K_F4 = 285,
	K_F5 = 286,
	K_F6 = 287,
	K_F7 = 288,
	K_F8 = 289,
	K_F9 = 290,
	K_F10 = 291,
	K_F11 = 292,
	K_F12 = 293,
	K_F13 = 294,
	K_F14 = 295,
	K_F15 = 296,
	K_INS = 277,
	K_SHIFT = 304,
	K_CTRL = 306,
	K_ALT = 308,

	K_JOY1,
	K_JOY2,
	K_JOY3,
	K_JOY4,
	K_JOY5,
	K_JOY6,
	K_JOY7,
	K_JOY8,
	K_JOY9,
	K_JOY10,
	K_JOY11,
	K_JOY12,
	K_JOY13,
	K_JOY14,
	K_JOY15,
	K_JOY16,
	K_JOY17,
	K_JOY18,
	K_JOY19,
	K_JOY20,
	K_JOY21,
	K_JOY22,
	K_JOY23,
	K_JOY24,
	K_JOY25,
	K_JOY26,
	K_JOY27,
	K_JOY28,
	K_JOY29,
	K_JOY30,
	K_JOY31,
	K_JOY32,

	K_AUX1,
	K_AUX2,
	K_AUX3,
	K_AUX4,
	K_AUX5,
	K_AUX6,
	K_AUX7,
	K_AUX8,
	K_AUX9,
	K_AUX10,
	K_AUX11,
	K_AUX12,
	K_AUX13,
	K_AUX14,
	K_AUX15,
	K_AUX16,

	K_NUMLOCK,
	K_SUPER,
	K_COMPOSE,
	K_MODE,
	K_HELP,
	K_PRINT,
	K_SYSREQ,
	K_SCROLLOCK,
	K_BREAK,
	K_MENU,
	K_EURO,
	K_UNDO,

	K_COMMAND,
	K_CAPSLOCK,
	K_POWER,
	K_APPS,

	K_LAST_KEY = 511,	/* to support as many chars as posible */
	K_KEY_SIZE = 512
} keyNum_t;

typedef enum {
	KEYSPACE_UI,
	KEYSPACE_GAME,
	KEYSPACE_BATTLE,

	KEYSPACE_MAX
} keyBindSpace_t;

#define MAXKEYLINES 32

extern int msgMode;
extern char msgBuffer[MAXCMDLINE];
extern size_t msgBufferLen;
extern char keyLines[MAXKEYLINES][MAXCMDLINE];
extern uint32_t keyLinePos;
extern int historyLine;
extern int editLine;
extern char *keyBindings[K_KEY_SIZE];
extern char *menuKeyBindings[K_KEY_SIZE];
extern char *battleKeyBindings[K_KEY_SIZE];

qboolean Key_IsDown(unsigned int key);
void Key_SetDest(int key_dest);
void Key_Event(unsigned int key, unsigned short unicode, qboolean down, unsigned time);
void Key_Init(void);
void Key_WriteBindings(const char* filename);
const char* Key_GetBinding(const char *binding, keyBindSpace_t space);
const char *Key_KeynumToString(int keynum);
int Key_StringToKeynum(const char *str);
void Key_SetBinding(int keynum, const char *binding, keyBindSpace_t space);

#endif /* CLIENT_KEY_H */
