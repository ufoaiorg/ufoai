/**
 * @file gl_model.h
 * @brief Brush model header file
 * @note d*_t structures are on-disk representations
 * @note m*_t structures are in-memory
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

#define VERTEXSIZE	7

#include "gl_model_alias.h"
#include "gl_model_brush.h"
#include "gl_model_md2.h"
#include "gl_model_md3.h"
#include "gl_model_sp2.h"

/* Whole model */

/**
 * @brief All supported model formats
 * @sa mod_extensions
 */
typedef enum {mod_bad, mod_brush, mod_sprite, mod_alias_md2, mod_alias_md3, mod_obj} modtype_t;

typedef struct model_s {
	/** the name needs to be the first entry in the struct */
	char name[MAX_QPATH];

	int registration_sequence;

	modtype_t type;	/**< model type */
	int numframes;

	int flags;

	/** volume occupied by the model graphics */
	vec3_t mins, maxs;
	float radius;

	/** solid volume for clipping */
	qboolean clipbox;
	vec3_t clipmins, clipmaxs;

	mBspModel_t bsp;

	/** for alias models and skins */
	mAliasModel_t alias;
} model_t;

/*============================================================================ */

#define MAX_MOD_KNOWN   512

void Mod_ClearAll(void);
void Mod_Modellist_f(void);
void Mod_FreeAll(void);
void Mod_DrawModelBBox(vec4_t bbox[8], entity_t *e);
void Mod_DrawNullModel(void);

extern model_t mod_known[MAX_MOD_KNOWN];
extern int mod_numknown;

extern model_t mod_inline[MAX_MOD_KNOWN];
extern int numInline;

extern model_t *loadmodel;

