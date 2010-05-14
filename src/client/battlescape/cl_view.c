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
			cl.model_clip[i] = CM_InlineModel(cl.configstrings[CS_MODELS + i]);
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
