/**
 * @file cl_view.c
 * @brief Player rendering positioning.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/cl_view.c
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

#include "../client.h"
#include "../cl_screen.h"
#include "cl_particle.h"
#include "cl_localentity.h"
#include "cl_actor.h"
#include "cl_hud.h"
#include "cl_spawn.h"
#include "cl_view.h"
#include "../renderer/r_main.h"
#include "../renderer/r_entity.h"

cvar_t* cl_map_debug;
static cvar_t *cl_precache;
static cvar_t* cl_map_displayavailablecells;

/**
 * @brief Call before entering a new level, or after vid_restart
 */
void CL_ViewLoadMedia (void)
{
	le_t *le;
	int i, max;
	float loadingPercent;
	char loadingMessages[96];

	CL_ViewUpdateRenderData();

	if (CL_GetConfigString(CS_TILES)[0] == '\0')
		return;					/* no map loaded */

	Com_sprintf(loadingMessages, sizeof(loadingMessages), _("loading %s"), _(CL_GetConfigString(CS_MAPTITLE)));
	loadingPercent = 0;

	/* register models, pics, and skins */
	SCR_DrawLoading(loadingPercent, loadingMessages);
	R_ModBeginLoading(CL_GetConfigString(CS_TILES), CL_GetConfigStringInteger(CS_LIGHTMAP), CL_GetConfigString(CS_POSITIONS), CL_GetConfigString(CS_NAME));
	CL_SpawnParseEntitystring();

	Com_sprintf(loadingMessages, sizeof(loadingMessages), _("loading models..."));
	loadingPercent += 10.0f;
	SCR_DrawLoading(loadingPercent, loadingMessages);

	LM_Register();
	CL_ParticleRegisterArt();

	for (i = 1, max = 0; i < MAX_MODELS && CL_GetConfigString(CS_MODELS + i)[0] != '\0'; i++)
		max++;

	max += csi.numODs;

	for (i = 1; i < MAX_MODELS; i++) {
		const char *name = CL_GetConfigString(CS_MODELS + i);
		if (name[0] == '\0')
			break;
		/* skip inline bmodels */
		if (name[0] != '*') {
			Com_sprintf(loadingMessages, sizeof(loadingMessages), _("loading %s"), name);
		}
		SCR_DrawLoading(loadingPercent, loadingMessages);
		cl.model_draw[i] = R_FindModel(name);
		if (!cl.model_draw[i]) {
			Cmd_ExecuteString("fs_info\n");
			Com_Error(ERR_DROP, "Could not load model '%s'\n", name);
		}

		/* initialize clipping for bmodels */
		if (name[0] == '*')
			cl.model_clip[i] = CM_InlineModel(cl.mapTiles, name);
		else
			cl.model_clip[i] = NULL;

		loadingPercent += 100.0f / (float)max;
	}

	/* update le model references */
	le = NULL;
	while ((le = LE_GetNextInUse(le))) {
		if (le->modelnum1 > 0)
			le->model1 = LE_GetDrawModel(le->modelnum1);
		if (le->modelnum2 > 0)
			le->model2 = LE_GetDrawModel(le->modelnum2);
	}

	loadingPercent = 100.0f;

	refdef.ready = qtrue;

	/* waiting for EV_START */
	Com_sprintf(loadingMessages, sizeof(loadingMessages), _("Awaiting game start"));
	SCR_DrawLoading(loadingPercent, loadingMessages);

	SCR_EndLoadingPlaque();
}

/**
 * @brief Precache all menu models for faster access
 * @sa CL_ViewPrecacheModels
 * @todo Does not precache armoured models
 */
static float CL_PrecacheCharacterModels (float alreadyLoadedPercent)
{
	teamDef_t *td;
	int i, j, num;
	char model[MAX_QPATH];
	const char *path;
	linkedList_t *list;
	const float percent = 40.0f;

	if (!cl_precache->integer)
		return 0;

	/* search the name */
	for (i = 0, td = csi.teamDef; i < csi.numTeamDefs; i++, td++)
		for (j = NAME_NEUTRAL; j < NAME_LAST; j++) {
			/* no models for this gender */
			if (!td->numModels[j])
				continue;
			/* search one of the model definitions */
			list = td->models[j];
			assert(list);
			for (num = 0; num < td->numModels[j]; num++) {
				assert(list);
				path = (const char*)list->data;
				list = list->next;
				/* register body */
				Com_sprintf(model, sizeof(model), "%s/%s", path, list->data);
				if (!R_FindModel(model))
					Com_Printf("Com_PrecacheCharacterModels: Could not register model %s\n", model);
				list = list->next;
				/* register head */
				Com_sprintf(model, sizeof(model), "%s/%s", path, list->data);
				if (!R_FindModel(model))
					Com_Printf("Com_PrecacheCharacterModels: Could not register model %s\n", model);

				/* skip skin */
				list = list->next;

				/* new path */
				list = list->next;

				alreadyLoadedPercent += percent / (td->numModels[j] * csi.numTeamDefs * NAME_LAST);
				SCR_DrawLoadingScreen(qtrue, alreadyLoadedPercent);
			}
		}
	/* some genders may not have models - ensure that we do the wanted percent step */
	return percent;
}

/**
 * @brief Precaches all models at game startup - for faster access
 */
void CL_ViewPrecacheModels (void)
{
	int i;
	float percent = 30.0f;
	float alreadyLoadedPercent = 30.0f;
	float loaded;

	loaded = CL_PrecacheCharacterModels(alreadyLoadedPercent);
	alreadyLoadedPercent += loaded;
	if (loaded == 0)
		percent = 100 - alreadyLoadedPercent;

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);

		alreadyLoadedPercent += percent / csi.numODs;
		SCR_DrawLoadingScreen(qtrue, alreadyLoadedPercent);

		if (od->type[0] == '\0' || od->isDummy)
			continue;

		if (od->model[0] != '\0') {
			cls.modelPool[i] = R_FindModel(od->model);
			if (cls.modelPool[i])
				Com_DPrintf(DEBUG_CLIENT, "CL_PrecacheModels: Registered object model: '%s' (%i)\n", od->model, i);
		}
	}

	/* now make sure that all the precached models are stored until we quit the game
	 * otherwise they would be freed with every map change */
	R_SwitchModelMemPoolTag();

	SCR_DrawLoadingScreen(qfalse, 100.f);
}

/**
 * @brief Calculates refdef's FOV_X.
 * Should generally be called after any changes are made to the zoom level (via cl.cam.zoom)
 * @sa V_CalcFovY
 */
void CL_ViewCalcFieldOfViewX (void)
{
	if (cl_isometric->integer) {
		const float zoom =  3.6 * (cl.cam.zoom - cl_camzoommin->value) + 0.3 * cl_camzoommin->value;
		refdef.fieldOfViewX = max(min(FOV / zoom, 140.0), 1.0);
	} else {
		refdef.fieldOfViewX = max(min(FOV / cl.cam.zoom, 95.0), 55.0);
	}
}

/**
 * @sa CL_ViewCalcFieldOfViewX
 */
static inline void CL_ViewCalcFieldOfViewY (const float width, const float height)
{
	refdef.fieldOfViewY = atan(tan(refdef.fieldOfViewX * (M_PI / 360.0)) * (height / width)) * (360.0 / M_PI);
}

/**
 * @brief Updates the refdef
 */
void CL_ViewUpdateRenderData (void)
{
	VectorCopy(cl.cam.camorg, refdef.viewOrigin);
	VectorCopy(cl.cam.angles, refdef.viewAngles);

	CL_ViewCalcFieldOfViewY(viddef.viewWidth, viddef.viewHeight);

	/* setup refdef */
	refdef.time = cl.time * 0.001;
	refdef.worldlevel = cl_worldlevel->integer;
}

/**
 * @sa SCR_UpdateScreen
 */
void CL_ViewRender (void)
{
	refdef.brushCount = 0;
	refdef.aliasCount = 0;
	refdef.batchCount = 0;
	refdef.FFPToShaderCount = 0;
	refdef.shaderToShaderCount = 0;
	refdef.shaderToFFPCount = 0;

	if (cls.state != ca_active)
		return;

	if (!viddef.viewWidth || !viddef.viewHeight)
		return;

	/* still loading */
	if (!refdef.ready)
		return;

	refdef.numEntities = 0;
	refdef.mapTiles = cl.mapTiles;

	/* tell the bsp thread to start */
	r_threadstate.state = THREAD_BSP;
	/* make sure we are really rendering the world */
	refdef.rendererFlags &= ~RDF_NOWORLDMODEL;
	/* add local models to the renderer chain */
	LM_AddToScene();
	/* add local entities to the renderer chain */
	LE_AddToScene();

	/* adds pathing data */
	if (cl_map_displayavailablecells->integer) {
		CL_AddActorPathing();
	}

	if (cl_map_debug->integer) {
		if (cl_map_debug->integer & MAPDEBUG_PATHING)
			CL_AddPathing();
		/* adds floor arrows */
		if (cl_map_debug->integer & MAPDEBUG_CELLS)
			CL_DisplayFloorArrows();
		/* adds wall arrows */
		if (cl_map_debug->integer & MAPDEBUG_WALLS)
			CL_DisplayObstructionArrows();
	}


	/* adds target cursor */
	CL_AddTargeting();

	/* update ref def */
	CL_ViewUpdateRenderData();

	/* render the world */
	R_RenderFrame();
}

/**
 * @brief Centers the camera on a given grid field
 * @sa CL_CameraMove
 * @sa LE_CenterView
 * @sa CL_CameraRoute
 */
void CL_ViewCenterAtGridPosition (const pos3_t pos)
{
	vec3_t vec;

	PosToVec(pos, vec);
	VectorCopy(vec, cl.cam.origin);
	Cvar_SetValue("cl_worldlevel", pos[2]);
}

void CL_ViewInit (void)
{
	cl_precache = Cvar_Get("cl_precache", "0", CVAR_ARCHIVE, "Precache character models at startup - more memory usage but smaller loading times in the game");
	cl_map_displayavailablecells = Cvar_Get("cl_map_displayavailablecells", "0", 0, "Display cells where a solider can move");
	cl_map_draw_rescue_zone = Cvar_Get("cl_map_draw_rescue_zone", "2", 0, "Draw rescue zone: 1 - draw perimeter, 2 - draw circles on the ground, 3 - draw both");
}
