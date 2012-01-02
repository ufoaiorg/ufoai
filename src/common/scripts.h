/**
 * @file scripts.h
 * @brief Header for script parsing functions
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

#ifndef SCRIPTS_H
#define SCRIPTS_H

#include "common.h"

#ifndef ALIGN_PTR
#define ALIGN_PTR(value,size) (void*)(((uintptr_t)value + (size - 1)) & (~(size - 1)))
#endif

#define MEMBER_SIZEOF(TYPE, MEMBER) sizeof(((TYPE *)0)->MEMBER)

/**
 * @brief Allow to add extra bit into the type
 * @note If valueTypes_t is bigger than 63 the mask must be changed
 * @sa valueTypes_t
 */
#define V_BASETYPEMASK 0x3F

/**
 * @brief possible values for parsing functions
 * @sa vt_names
 * @sa vt_sizes
 */
typedef enum {
	V_NULL,
	V_BOOL,
	V_CHAR,
	V_INT,
	V_INT2,
	V_FLOAT,
	V_POS,
	V_VECTOR,
	V_COLOR,
	V_RGBA,
	V_STRING,
	V_TRANSLATION_STRING,	/**< remove _ but don't translate */
	V_LONGSTRING,			/**< not buffer safe - use this only for menu node data array values! */
	V_ALIGN,
	V_BLEND,
	V_STYLE,
	V_FADE,
	V_SHAPE_SMALL,			/**< space a weapon allocates in the inventory shapes, w, h */
	V_SHAPE_BIG,			/**< inventory shape, x, y, w, h */
	V_DMGTYPE,
	V_DMGWEIGHT,
	V_DATE,
	V_RELABS,				/**< relative (e.g. 1.50) and absolute (e.g. +15) values */
	V_HUNK_STRING,			/**< store the string in a mem pool */
	V_TEAM,					/**< team string to int mapper */
	V_RACE,
	V_UFO,					/**< @brief Valid ufo types
							 * @note Use the same values for the names as we are already using in the scriptfiles
							 * here, otherwise they are not translatable because they don't appear in the po files
							 * @note Every ufotype (id) that doesn't have nogeoscape set to true must have an assembly
							 * in the ufocrash[dn].ump files */
	V_UFOCRASHED,
	V_AIRCRAFTTYPE,
	V_LIST,

	V_NUM_TYPES
} valueTypes_t;

extern const char *const vt_names[];

/** @brief We need this here for checking the boundaries from script values */

/** possible align values - see also align_names */
typedef enum {
	ALIGN_UL,
	ALIGN_UC,
	ALIGN_UR,
	ALIGN_CL,
	ALIGN_CC,
	ALIGN_CR,
	ALIGN_LL,
	ALIGN_LC,
	ALIGN_LR,
	ALIGN_UL_RSL,
	ALIGN_UC_RSL,
	ALIGN_UR_RSL,
	ALIGN_CL_RSL,
	ALIGN_CC_RSL,
	ALIGN_CR_RSL,
	ALIGN_LL_RSL,
	ALIGN_LC_RSL,
	ALIGN_LR_RSL,

	ALIGN_LAST
} align_t;

/** possible blend modes - see also blend_names */
typedef enum {
	BLEND_REPLACE,
	BLEND_ONE,
	BLEND_BLEND,
	BLEND_ADD,
	BLEND_FILTER,
	BLEND_INVFILTER,

	BLEND_LAST
} blend_t;

typedef enum {
	STYLE_FACING, /**< rotates a sprint into the camera direction */
	STYLE_ROTATED, /**< use the particle angles vector */
	STYLE_BEAM,
	STYLE_LINE,
	STYLE_AXIS,
	STYLE_CIRCLE,

	STYLE_LAST
} style_t;

typedef enum {
	FADE_NONE,
	FADE_IN,
	FADE_OUT,
	FADE_SIN,
	FADE_SAW,

	FADE_LAST
} fade_t;

/**
 * @brief All different types of UFOs.
 */
typedef enum {
	UFO_SCOUT,
	UFO_FIGHTER,
	UFO_HARVESTER,
	UFO_CORRUPTER,
	UFO_BOMBER,
	UFO_CARRIER,
	UFO_SUPPLY,
	UFO_GUNBOAT,
	UFO_RIPPER,
	UFO_MOTHERSHIP,

	UFO_MAX
} ufoType_t;

extern const char *const align_names[];
extern const char *const blend_names[];
extern const char *const style_names[];
extern const char *const fade_names[];
extern const char *const longlines_names[];
extern const char *const air_slot_type_strings[];

/** used e.g. in our parsers */
typedef struct value_s {
	const char *string;
	valueTypes_t type;
	size_t ofs;
	size_t size;
} value_t;

typedef enum {
	RESULT_ERROR = -1,
	RESULT_WARNING = -2,
	RESULT_OK = 0
} resultStatus_t;

#ifdef DEBUG
int Com_EParseValueDebug(void *base, const char *token, valueTypes_t type, int ofs, size_t size, const char* file, int line);
int Com_SetValueDebug(void *base, const void *set, valueTypes_t type, int ofs, size_t size, const char* file, int line);
#define Com_SetValue(base, set, type, ofs, size) Com_SetValueDebug(base, set, type, ofs, size, __FILE__, __LINE__)
#define Com_EParseValue(base, token, type, ofs, size) Com_EParseValueDebug(base, token, type, ofs, size, __FILE__, __LINE__)
#else
int Com_EParseValue(void *base, const char *token, valueTypes_t type, int ofs, size_t size);
int Com_SetValue(void *base, const void *set, valueTypes_t type, int ofs, size_t size);
#endif
qboolean Com_ParseBlock(const char *name, const char **text, void *base, const value_t *values, struct memPool_s *mempool);
qboolean Com_ParseBlockToken(const char *name, const char **text, void *base, const value_t *values, struct memPool_s *mempool, const char *token);
void* Com_AlignPtr(const void *memory, valueTypes_t type);
const char *Com_ValueToStr(const void *base, const valueTypes_t type, const int ofs);
const char *Com_GetLastParseError(void);
int Com_ParseValue(void *base, const char *token, valueTypes_t type, int ofs, size_t size, size_t *writtenBytes);
qboolean Com_ParseBoolean(const char *token);

/*==============================================================
SCRIPT PARSING
==============================================================*/

extern const char *const name_strings[];

#define SND_VOLUME_FOOTSTEPS 0.2f

/** @brief Different terrain definitions for footsteps and particles */
typedef struct terrainType_s {
	const char *texture;			/**< script id is the texture name/path */
	const char *footStepSound;		/**< sound to play when walking on this terrain type */
	const char *particle;			/**< particle to spawn when walking on this type of terrain */
	float bounceFraction;			/**< the impact on the bounce fraction given in the weapon definition */
	float footStepVolume;			/**< footstep sound volume */
	struct terrainType_s *hash_next;	/**< next entry in the hash list */
} terrainType_t;

const terrainType_t* Com_GetTerrainType(const char *textureName);

/**
 * @brief list of script aliases to register
 * @note must be terminated with a NULL ({NULL, -1}) entry!
 * @sa saveEmployeeConstants[]
 */
typedef struct constListEntry_s {
	const char *name;
	int value;
} constListEntry_t;

qboolean Com_GetConstInt(const char *name, int *value);
qboolean Com_GetConstIntFromNamespace(const char *space, const char *variable, int *value);
const char* Com_GetConstVariable(const char *space, int value);
qboolean Com_UnregisterConstVariable(const char *name);
void Com_RegisterConstInt(const char *name, int value);
void Com_RegisterConstList(const constListEntry_t constList[]);
qboolean Com_UnregisterConstList(const constListEntry_t constList[]);

void Com_ParseScripts(qboolean onlyServer);
const char *Com_EParse(const char **text, const char *errhead, const char *errinfo);
const char *Com_GetRandomMapAssemblyNameForCraft(const char *craftID);
const char *Com_GetRandomMapAssemblyNameForCrashedCraft(const char *craftID);
ufoType_t Com_UFOShortNameToID(const char *token);
const char* Com_UFOTypeToShortName(ufoType_t type);
const char* Com_UFOCrashedTypeToShortName(ufoType_t type);
int Com_GetScriptChecksum(void);
void Com_Shutdown(void);

#include "../game/q_shared.h"

const ugv_t *Com_GetUGVByIDSilent(const char *ugvID);
const ugv_t *Com_GetUGVByID(const char *ugvID);
const char* Com_DropShipTypeToShortName(humanAircraftType_t type);
humanAircraftType_t Com_DropShipShortNameToID(const char *token);
void Com_GetCharacterValues(const char *teamDefition, character_t * chr);
const char* Com_GetActorSound(teamDef_t* td, int gender, actorSound_t soundType);
const teamDef_t* Com_GetTeamDefinitionByID(const char *team);
const chrTemplate_t* Com_GetCharacterTemplateByID(const char *chrTemplate);

#endif /* SCRIPTS_H */
