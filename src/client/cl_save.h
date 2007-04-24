/**
 * @file cl_save.h
 * @brief Defines some savefile structures
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef CLIENT_CL_SAVE_H
#define CLIENT_CL_SAVE_H

#define MAX_SAVESUBSYSTEMS 32

#define SAVE_FILE_VERSION 2
/* MAX_GAMESAVESIZE has room for 3MB for dynamic data, eg geoscape messages */
#define MAX_GAMESAVESIZE	3145728
#define MAX_COMMENTLENGTH	32
typedef struct saveFileHeader_s {
	int version; /**< which savegame version */
	int compressed; /**< is this file compressed via zlib */
	int dummy[14]; /**< maybe we have to extend this later */
	char gameVersion[16]; /**< game version that was used to save this file */
	char name[32]; /**< savefile name */
	char gameDate[32]; /**< internal game date */
	char realDate[32]; /**< real datestring when the user saved this game */
} saveFileHeader_t;

typedef struct saveSubsystems_s {
	char *name;
	qboolean (*save) (sizebuf_t *sb, void *data);	/**< return false if saving failed */
	qboolean (*load) (sizebuf_t *sb, void *data);	/**< return false if loading failed */
	int check;
} saveSubsystems_t;

extern saveSubsystems_t saveSubsystems[MAX_SAVESUBSYSTEMS];
extern int saveSubsystemsAmount;

/** and now the save and load prototypes for every subsystem */
qboolean CP_Save(sizebuf_t *sb, void *data);
qboolean CP_Load(sizebuf_t *sb, void *data);
qboolean B_Save(sizebuf_t* sb, void* data);
qboolean B_Load(sizebuf_t* sb, void* data);
qboolean BS_Save(sizebuf_t* sb, void* data);
qboolean BS_Load(sizebuf_t* sb, void* data);
qboolean AIR_Save(sizebuf_t* sb, void* data);
qboolean AIR_Load(sizebuf_t* sb, void* data);
qboolean AC_Save(sizebuf_t* sb, void* data);
qboolean AC_Load(sizebuf_t* sb, void* data);
qboolean E_Save(sizebuf_t* sb, void* data);
qboolean E_Load(sizebuf_t* sb, void* data);
qboolean RS_Save(sizebuf_t* sb, void* data);
qboolean RS_Load(sizebuf_t* sb, void* data);
qboolean PR_Save(sizebuf_t* sb, void* data);
qboolean PR_Load(sizebuf_t* sb, void* data);
qboolean MS_Save(sizebuf_t* sb, void* data);
qboolean MS_Load(sizebuf_t* sb, void* data);
qboolean STATS_Save(sizebuf_t* sb, void* data);
qboolean STATS_Load(sizebuf_t* sb, void* data);
qboolean NA_Save(sizebuf_t* sb, void* data);
qboolean NA_Load(sizebuf_t* sb, void* data);

#endif /* CLIENT_CL_SAVE_H */
