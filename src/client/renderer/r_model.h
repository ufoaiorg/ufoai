/**
 * @file r_model.h
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

#ifndef R_MODEL_H
#define R_MODEL_H

#include "r_entity.h"
#include "r_model_alias.h"
#include "r_model_brush.h"
#include "r_model_dpm.h"
#include "r_model_md2.h"
#include "r_model_md3.h"
#include "r_model_obj.h"

/**
 * @brief All supported model formats
 * @sa mod_extensions
 */
typedef enum {mod_bad, mod_bsp, mod_bsp_submodel, mod_alias_md2, mod_alias_md3, mod_alias_dpm, mod_obj} modtype_t;

typedef struct model_s {
	char name[MAX_QPATH];	/**< path relative to base/ */

	modtype_t type;	/**< model type */

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

#define MAX_ACTORSKINNAME		32

/*============================================================================ */

void R_ModModellist_f(void);
image_t* R_AliasModelState(const model_t *mod, int *mesh, int *frame, int *oldFrame, int *skin);
image_t* R_AliasModelGetSkin(const char *modelFileName, const char *skin);
void R_DrawAliasModel(entity_t *e);
void R_ShutdownModels(qboolean complete);
void R_ModReloadSurfacesArrays(void);
int R_ModAllocateActorSkin(const char* name);
void R_LoadActorSkinsFromModel(mAliasMesh_t *outMesh, image_t * defaultSkin);
qboolean R_UseActorSkin(void);

model_t *R_FindModel(const char *name);
model_t *R_GetModel(const char *name);
model_t *R_AllocModelSlot(void);

/** @brief The world model(s) */
extern model_t *r_mapTiles[MAX_MAPTILES];
extern int r_numMapTiles;

extern model_t r_modelsInline[MAX_MOD_KNOWN];
extern int r_numModelsInline;

#endif
