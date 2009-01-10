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

#include "../../common/msg.h"

extern cvar_t *cl_lastsave;

#define MAX_SAVESUBSYSTEMS 32

/**
 * HISTORY
 * version id   | game version | compatible with trunk
 * ===================================================
 *          1   | 2.1.1        | no
 *          2   | 2.2          | yes
 *          3   | 2.3          | yes
 */
#define SAVE_FILE_VERSION 3
#define MPTEAM_SAVE_FILE_VERSION 2

/* MAX_GAMESAVESIZE has room for 3MB for dynamic data, eg geoscape messages */
#define MAX_GAMESAVESIZE	3145728
#define MAX_COMMENTLENGTH	32

#include <zlib.h>

qboolean SAV_QuickSave(void);
void SAV_Init(void);
#define MAX_ARRAYINDEXES	512

/** @brief Indexes of presaveArray. DON'T MESS WITH ORDER. */
typedef enum {
	PRE_NUMODS,	/* Number of Objects in csi.ods */
	PRE_NUMIDS,	/* Number of Containers */
	PRE_BASESI,	/* #define BASE_SIZE */
	PRE_MAXBUI,	/* #define MAX_BUILDINGS */
	PRE_ACTTEA,	/* #define MAX_ACTIVETEAM */
	PRE_MAXEMP,	/* #define MAX_EMPLOYEES */
	PRE_MCARGO,	/* #define MAX_CARGO */
	PRE_MAXAIR,	/* #define MAX_AIRCRAFT */
	PRE_AIRSTA,	/* AIR_STATS_MAX in aircraftParams_t */
	PRE_MAXCAP,	/* MAX_CAP in baseCapacities_t */
	PRE_EMPTYP,	/* MAX_EMPL in employeeType_t */
	PRE_MAXBAS,	/* #define MAX_BASES */
	PRE_NATION,	/* gd.numNations */
	PRE_KILLTP,	/* KILLED_NUM_TYPES in killtypes_t */
	PRE_SKILTP,	/* SKILL_NUM_TYPES in abilityskills_t */
	PRE_NMTECH,	/* gd.numTechnologies */
	PRE_TECHMA,	/* TECHMAIL_MAX in techMailType_t */
	PRE_NUMTDS,	/* numTeamDesc */
	PRE_NUMALI,	/* gd.numAliensTD */
	PRE_NUMUFO,	/* gd.numUFOs */
	PRE_MAXREC,	/* #define MAX_RECOVERIES */
	PRE_MAXTRA, /* #define MAX_TRANSFERS */
	PRE_MAXOBJ, /* #define MAX_OBJDEFS */
	PRE_MAXMPR, /* #define MAX_MULTIPLE_PROJECTILES */
	PRE_MBUITY, /* MAX_BUILDING_TYPE in buildingType_t */
	PRE_MAXALB,	/* #define MAX_ALIEN_BASES */
	PRE_MAXCAT, /* INTERESTCATEGORY_MAX in interestCategory_t */
	PRE_MAXINST, /* MAX_INSTALLATIONS */

	PRE_MAX
} presaveType_t;

/**
 * @brief Presave array of arrays indexes.
 * @note Needs to be loaded first, values from here should be used in every loops.
 */
extern int presaveArray[MAX_ARRAYINDEXES];

/**
 * @brief True if a game is currently loading.
 * @note Used to execute some actions only while loading in function used ingame and while loading.
 */
extern qboolean loading;

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
qboolean TR_Save(sizebuf_t* sb, void* data);
qboolean TR_Load(sizebuf_t* sb, void* data);
qboolean AB_Save(sizebuf_t* sb, void* data);
qboolean AB_Load(sizebuf_t* sb, void* data);
qboolean XVI_Save(sizebuf_t* sb, void* data);
qboolean XVI_Load(sizebuf_t* sb, void* data);
qboolean INS_Save(sizebuf_t* sb, void* data);
qboolean INS_Load(sizebuf_t* sb, void* data);
qboolean MSO_Save(sizebuf_t* sb, void* data);
qboolean MSO_Load(sizebuf_t* sb, void* data);

void RS_PostLoadInit(void);
void B_PostLoadInit(void);
#endif /* CLIENT_CL_SAVE_H */
