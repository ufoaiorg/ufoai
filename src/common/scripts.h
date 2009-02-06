/**
 * @file scripts.h
 * @brief Header for script parsing functions
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

#ifndef SCRIPTS_H
#define SCRIPTS_H

#ifndef ALIGNBYTES
#define ALIGNBYTES 1
#endif
/* darwin defines this in /usr/include/ppc/param.h */
#ifndef ALIGN
#define ALIGN(size)  size
/*((size) + ((ALIGN_BYTES - ((size) % ALIGN_BYTES)) % ALIGN_BYTES))*/
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
	V_FLOAT = 5,
	V_POS,
	V_VECTOR,
	V_COLOR,
	V_RGBA,
	V_STRING = 10,
	V_TRANSLATION_STRING,	/**< remove _ but don't translate */
	V_LONGSTRING,			/**< not buffer safe - use this only for menu node data array values! */
	V_ALIGN,
	V_BLEND,
	V_STYLE = 15,
	V_FADE,
	V_SHAPE_SMALL,			/**< space a weapon allocates in the inventory shapes, w, h */
	V_SHAPE_BIG,			/**< inventory shape, x, y, w, h */
	V_DMGTYPE,
	V_DMGWEIGHT = 20,
	V_DATE,
	V_RELABS,				/**< relative (e.g. 1.50) and absolute (e.g. +15) values */
	V_CLIENT_HUNK,			/**< only for client side data - not handled in Com_EParseValue */
	V_CLIENT_HUNK_STRING,	/**< same as for V_CLIENT_HUNK */
	V_MENUTEXTID = 25,
	V_BASEID,
	V_LONGLINES,
	V_TEAM,					/**< team string to int mapper */
	V_RACE,

	V_NUM_TYPES
} valueTypes_t;

extern const char *const vt_names[];

/** @brief We need this here for checking the boundaries from script values */
#define MAX_BASES 8

/** @brief linked into mn.menuText - defined in menu scripts via num */
typedef enum {
	TEXT_STANDARD,
	TEXT_LIST,
	TEXT_UFOPEDIA,
	TEXT_BUILDINGS,
	TEXT_BUILDING_INFO,
	TEXT_RESEARCH,
	TEXT_POPUP,
	TEXT_POPUP_INFO,
	TEXT_AIRCRAFT_LIST,
	TEXT_AIRCRAFT_INFO,
	TEXT_MESSAGESYSTEM,			/**< just a dummy for messagesystem - we use the stack */
	TEXT_CAMPAIGN_LIST,
	TEXT_MULTISELECTION,
	TEXT_PRODUCTION_LIST,
	TEXT_PRODUCTION_AMOUNT,
	TEXT_PRODUCTION_INFO,
	TEXT_EMPLOYEE,
	TEXT_MOUSECURSOR_RIGHT,
	TEXT_PRODUCTION_QUEUED,
	TEXT_STATS_BASESUMMARY,
	TEXT_STATS_MISSION,
	TEXT_STATS_BASES,
	TEXT_STATS_NATIONS,
	TEXT_STATS_EMPLOYEES,
	TEXT_STATS_COSTS,
	TEXT_STATS_INSTALLATIONS,
	TEXT_STATS_7,
	TEXT_BASE_LIST,
	TEXT_BASE_INFO,
	TEXT_TRANSFER_LIST,
	TEXT_MOUSECURSOR_PLAYERNAMES,
	TEXT_CARGO_LIST, /* unused, why? */
	TEXT_UFOPEDIA_MAILHEADER,
	TEXT_UFOPEDIA_MAIL,
	TEXT_MARKET_NAMES,
	TEXT_MARKET_STORAGE,
	TEXT_MARKET_MARKET,
	TEXT_MARKET_PRICES,
	TEXT_CHAT_WINDOW,
	TEXT_AIREQUIP_1,
	TEXT_AIREQUIP_2,
	TEXT_BASEDEFENCE_LIST,
	TEXT_TIPOFTHEDAY,
	TEXT_GENERIC,
	TEXT_XVI,
	TEXT_MOUSECURSOR_TOP,
	TEXT_MOUSECURSOR_BOTTOM,
	TEXT_MOUSECURSOR_LEFT,
	TEXT_MESSAGEOPTIONS,

	MAX_MENUTEXTS
} menuTextIDs_t;


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
	BLEND_BLEND,
	BLEND_ADD,
	BLEND_FILTER,
	BLEND_INVFILTER,

	BLEND_LAST
} blend_t;

typedef enum {
	STYLE_FACING,
	STYLE_ROTATED,
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
	FADE_BLEND,

	FADE_LAST
} fade_t;

typedef enum {
	LONGLINES_WRAP,
	LONGLINES_CHOP,
	LONGLINES_PRETTYCHOP,

	LONGLINES_LAST
} longlines_t;

extern const char *const align_names[];
extern const char *const blend_names[];
extern const char *const style_names[];
extern const char *const fade_names[];
extern const char *const longlines_names[];
extern const char *const air_slot_type_strings[];

/** used e.g. in our parsers */
typedef struct value_s {
	const char *string;
	const int type;
	const size_t ofs;
	const size_t size;
} value_t;

typedef enum {
	RESULT_ERROR = -1,
	RESULT_WARNING = -2,
	RESULT_OK = 0
} resultStatus_t;

#ifdef DEBUG
int Com_EParseValueDebug(void *base, const char *token, valueTypes_t type, int ofs, size_t size, const char* file, int line);
int Com_SetValueDebug(void *base, const void *set, valueTypes_t type, int ofs, size_t size, const char* file, int line);
#else
int Com_EParseValue(void *base, const char *token, valueTypes_t type, int ofs, size_t size);
int Com_SetValue(void *base, const void *set, valueTypes_t type, int ofs, size_t size);
#endif
const char *Com_ValueToStr(const void *base, const valueTypes_t type, const int ofs);
const char *Com_GetError(void);
int Com_ParseValue(void *base, const char *token, valueTypes_t type, int ofs, size_t size, size_t *writedByte);

/*==============================================================
SCRIPT PARSING
==============================================================*/

extern const char *const name_strings[];

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

const char *Com_GiveName(int gender, const char *category);
const char *Com_GiveModel(int type, int gender, const char *teamID);
int Com_GetCharacterValues(const char *team, character_t * chr);
const char* Com_GetActorSound(teamDef_t* td, int gender, actorSound_t soundType);
teamDef_t* Com_GetTeamDefinitionByID(const char *team);
mapDef_t* Com_GetMapDefinitionByID(const char *mapDefID);
void Com_ParseScripts(void);
void Com_PrecacheCharacterModels(void);
const char *COM_EParse(const char **text, const char *errhead, const char *errinfo);

#endif /* SCRIPTS_H */
