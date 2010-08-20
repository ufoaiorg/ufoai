/**
 * @file cl_view.c
 * @brief Player rendering positioning.
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../cl_game.h"
#include "cl_particle.h"
#include "cl_localentity.h"
#include "cl_actor.h"
#include "cl_hud.h"
#include "cl_spawn.h"
#include "../cl_sequence.h"
#include "cl_view.h"
#include "../renderer/r_main.h"
#include "../renderer/r_entity.h"

cvar_t* cl_map_debug;
static cvar_t *cl_precache;

/**
 * @brief Call before entering a new level, or after vid_restart
 */
void CL_ViewLoadMedia (void)
{
	le_t *le;
	int i, max;
	char name[32];

	CL_ViewUpdateRenderData();

	if (!cl.configstrings[CS_TILES][0])
		return;					/* no map loaded */

	Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages), _("loading %s"), _(cl.configstrings[CS_MAPTITLE]));
	cls.loadingPercent = 0;

	/* register models, pics, and skins */
	SCR_UpdateScreen();
	R_ModBeginLoading(cl.configstrings[CS_TILES], atoi(cl.configstrings[CS_LIGHTMAP]), cl.configstrings[CS_POSITIONS], cl.configstrings[CS_NAME]);
	CL_SpawnParseEntitystring();

	Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages), _("loading models..."));
	cls.loadingPercent += 10.0f;
	SCR_UpdateScreen();

	LM_Register();
	CL_ParticleRegisterArt();

	for (i = 1, max = 0; i < MAX_MODELS && cl.configstrings[CS_MODELS + i][0]; i++)
		max++;

	max += csi.numODs;

	for (i = 1; i < MAX_MODELS && cl.configstrings[CS_MODELS + i][0]; i++) {
		Q_strncpyz(name, cl.configstrings[CS_MODELS + i], sizeof(name));
		/* skip inline bmodels */
		if (name[0] != '*') {
			Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages),
				_("loading %s"), name);
		}
		SCR_UpdateScreen();
		IN_SendKeyEvents();	/* pump message loop */
		cl.model_draw[i] = R_RegisterModelShort(cl.configstrings[CS_MODELS + i]);
		/* initialize clipping for bmodels */
		if (name[0] == '*')
			cl.model_clip[i] = CM_InlineModel(&cl.mapTiles, cl.configstrings[CS_MODELS + i]);
		else
			cl.model_clip[i] = NULL;
		if (!cl.model_draw[i])
			Com_Error(ERR_DROP, "Could not load model '%s'\n", cl.configstrings[CS_MODELS + i]);

		cls.loadingPercent += 100.0f / (float)max;
	}

	/* update le model references */
	le = NULL;
	while ((le = LE_GetNextInUse(le))) {
		if (le->modelnum1) {
			le->model1 = cl.model_draw[le->modelnum1];
			if (!le->model1)
				Com_Error(ERR_DROP, "No model for %i", le->modelnum1);
		}
		if (le->modelnum2) {
			le->model2 = cl.model_draw[le->modelnum2];
			if (!le->model2)
				Com_Error(ERR_DROP, "No model for %i", le->modelnum2);
		}
	}

	cls.loadingPercent = 100.0f;
	SCR_UpdateScreen();

	/* waiting for EV_START */
	Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages), _("Awaiting game start"));
	SCR_UpdateScreen();
	refdef.ready = qtrue;
	/** @todo Parse fog from configstrings */
	refdef.weather = WEATHER_NONE;
	refdef.fogColor[3] = 1.0;
	VectorSet(refdef.fogColor, 0.75, 0.75, 0.75);
}

/**
 * @brief Precache all menu models for faster access
 * @sa CL_ViewPrecacheModels
 * @todo Does not precache armoured models
 */
static void CL_PrecacheCharacterModels (void)
{
	teamDef_t *td;
	int i, j, num;
	char model[MAX_QPATH];
	const char *path;
	float loading = cls.loadingPercent;
	linkedList_t *list;
	const float percent = 55.0f;

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
				if (!R_RegisterModelShort(model))
					Com_Printf("Com_PrecacheCharacterModels: Could not register model %s\n", model);
				list = list->next;
				/* register head */
				Com_sprintf(model, sizeof(model), "%s/%s", path, list->data);
				if (!R_RegisterModelShort(model))
					Com_Printf("Com_PrecacheCharacterModels: Could not register model %s\n", model);

				/* skip skin */
				list = list->next;

				/* new path */
				list = list->next;

				cls.loadingPercent += percent / (td->numModels[j] * csi.numTeamDefs * NAME_LAST);
				SCR_DrawPrecacheScreen(qtrue);
			}
		}
	/* some genders may not have models - ensure that we do the wanted percent step */
	cls.loadingPercent = loading + percent;
}

/**
 * @brief Precaches all models at game startup - for faster access
 */
void CL_ViewPrecacheModels (void)
{
	int i;
	float percent = 40.0f;

	cls.loadingPercent = 5.0f;
	SCR_DrawPrecacheScreen(qtrue);

	if (cl_precache->integer)
		CL_PrecacheCharacterModels(); /* 55% */
	else
		percent = 95.0f;

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		if (od->type[0] == '\0' || od->isDummy)
			continue;

		if (od->model[0] != '\0') {
			cls.modelPool[i] = R_RegisterModelShort(od->model);
			if (cls.modelPool[i])
				Com_DPrintf(DEBUG_CLIENT, "CL_PrecacheModels: Registered object model: '%s' (%i)\n", od->model, i);
		}
		cls.loadingPercent += percent / csi.numODs;
		SCR_DrawPrecacheScreen(qtrue);
	}

	cls.loadingPercent = 100.0f;
	SCR_DrawPrecacheScreen(qtrue);

	/* now make sure that all the precached models are stored until we quit the game
	 * otherwise they would be freed with every map change */
	R_SwitchModelMemPoolTag();

	SCR_DrawPrecacheScreen(qfalse);
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

	if (cls.state != ca_active && cls.state != ca_sequence)
		return;

	if (!viddef.viewWidth || !viddef.viewHeight)
		return;

	/* still loading */
	if (!refdef.ready)
		return;

	refdef.numEntities = 0;

	switch (cls.state) {
	case ca_sequence:
		CL_SequenceRender();
		refdef.rendererFlags |= RDF_NOWORLDMODEL;
		break;
	default:
		/* tell the bsp thread to start */
		r_threadstate.state = THREAD_BSP;
		/* make sure we are really rendering the world */
		refdef.rendererFlags &= ~RDF_NOWORLDMODEL;
		/* add local models to the renderer chain */
		LM_AddToScene();
		/* add local entities to the renderer chain */
		LE_AddToScene();
		/* adds pathing data */
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
		break;
	}

	/* update ref def */
	CL_ViewUpdateRenderData();

	/* render the world */
	R_RenderFrame();

	if (cls.state == ca_sequence)
		CL_Sequence2D();
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
	cl_precache = Cvar_Get("cl_precache", "1", CVAR_ARCHIVE, "Precache character models at startup - more memory usage but smaller loading times in the game");
}
