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

/** conditions for V_IF */
typedef enum menuIfCondition_s {
	IF_EQ, /**< == */
	IF_LE, /**< <= */
	IF_GE, /**< >= */
	IF_GT, /**< > */
	IF_LT, /**< < */
	IF_NE, /**< != */
	IF_EXISTS, /**< only cvar give - check for existence */

	IF_SIZE
} menuIfCondition_t;

/** @sa menuIfCondition_t */
typedef struct menuDepends_s {
	char var[MAX_VAR];
	char value[MAX_VAR];
	int cond;
} menuDepends_t;

/* end of V_IF */

#ifndef ALIGN_BYTES
#define ALIGN_BYTES 1
#endif
#define ALIGN(size)  ((size) + ((ALIGN_BYTES - ((size) % ALIGN_BYTES)) % ALIGN_BYTES))

#define MEMBER_SIZEOF(TYPE, MEMBER) sizeof(((TYPE *)0)->MEMBER)

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
	V_TRANSLATION_STRING,	/**< translate via gettext */
	V_TRANSLATION2_STRING,		/**< remove _ but don't translate */
	V_LONGSTRING,
	V_ALIGN,
	V_BLEND = 15,
	V_STYLE,
	V_FADE,
	V_SHAPE_SMALL,
	V_SHAPE_BIG,
	V_DMGTYPE = 20,
	V_DATE,
	V_IF,
	V_RELABS,					/**< relative (e.g. 1.50) and absolute (e.g. +15) values */
	V_CLIENT_HUNK,				/**< only for client side data - not handled in Com_ParseValue */
	V_CLIENT_HUNK_STRING = 25,	/**< same as for V_CLIENT_HUNK */

	V_NUM_TYPES
} valueTypes_t;

extern const char *vt_names[V_NUM_TYPES];

/** possible alien values - see also align_names */
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

extern const char *align_names[ALIGN_LAST];
extern const char *blend_names[BLEND_LAST];
extern const char *style_names[STYLE_LAST];
extern const char *fade_names[FADE_LAST];

/** used e.g. in our parsers */
typedef struct value_s {
	const char *string;
	const int type;
	const size_t ofs;
	const size_t size;
} value_t;

#ifdef DEBUG
int Com_ParseValueDebug(void *base, const char *token, valueTypes_t type, int ofs, size_t size, const char* file, int line);
int Com_SetValueDebug(void *base, const void *set, valueTypes_t type, int ofs, size_t size, const char* file, int line);
#else
int Com_ParseValue(void *base, const char *token, valueTypes_t type, int ofs, size_t size);
int Com_SetValue(void *base, const void *set, valueTypes_t type, int ofs, size_t size);
#endif
const char *Com_ValueToStr(void *base, valueTypes_t type, int ofs);

/*==============================================================
SCRIPT PARSING
==============================================================*/

#ifndef SCRIPTS_H
#define SCRIPTS_H

#define MAX_TEAMDEFS	128

#define LASTNAME	3
typedef enum {
	NAME_NEUTRAL,
	NAME_FEMALE,
	NAME_MALE,

	NAME_LAST,
	NAME_FEMALE_LAST,
	NAME_MALE_LAST,

	NAME_NUM_TYPES
} nametypes_t;

typedef struct teamDesc_s {
	char id[MAX_VAR];
	qboolean alien;			/**< is this an alien teamdesc definition */
	qboolean armor, weapons; /**< able to use weapons/armor */
	char name[MAX_VAR];
	char tech[MAX_VAR]; /**< tech id from research.ufo */
} teamDesc_t;

extern teamDesc_t teamDesc[MAX_TEAMDEFS];
extern int numTeamDesc;

extern const char *name_strings[NAME_NUM_TYPES];

char *Com_GiveName(int gender, const char *category);
char *Com_GiveModel(int type, int gender, const char *category);
int Com_GetModelAndName(const char *team, character_t * chr);
const char* Com_GetActorSound(int category, int gender, actorSound_t soundType);

void Com_AddObjectLinks(void);
void Com_ParseScripts(void);
void Com_PrecacheCharacterModels(void);

#endif /* SCRIPTS_H */
