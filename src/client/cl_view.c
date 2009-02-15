/**
 * @file cl_view.c
 * @brief Player rendering positioning.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "client.h"
#include "cl_game.h"
#include "cl_particle.h"
#include "cl_actor.h"
#include "cl_hud.h"
#include "cl_sequence.h"
#include "cl_view.h"
#include "renderer/r_main.h"
#include "renderer/r_entity.h"
#include "../shared/parse.h"

/** position in the spawnflags */
#define MISC_MODEL_GLOW 9
#define SPAWNFLAG_NO_DAY 8

/**
 * @brief Parse the map entity string and spawns those entities that are client-side only
 * @sa G_SendEdictsAndBrushModels
 * @sa CL_AddBrushModel
 */
static void CL_ParseEntitystring (const char *es)
{
	char keyname[256];
	char classname[MAX_VAR];
	char animname[MAX_QPATH], model[MAX_QPATH], particle[MAX_QPATH], sound[MAX_QPATH];
	vec3_t origin, angles, scale;
	vec2_t wait;
	int maxlevel = 8, maxmultiplayerteams = 2, entnum = 0;
	int skin, frame, spawnflags;
	float volume;
	const int dayLightmap = atoi(cl.configstrings[CS_LIGHTMAP]);

	cl.map_maxlevel = 8;
	if (cl.map_maxlevel_base >= 1)
		cl.map_maxlevel = maxlevel = cl.map_maxlevel_base;

	/* vid restart? */
	if (numMPs || numLMs)
		return;

	/* clear local entities */
	numLEs = 0;

	/* parse ents */
	while (1) {
		/* parse the opening brace */
		const char *entity_token = COM_Parse(&es);
		/* memorize the start */
		const char *strstart = es;
		if (!es)
			break;

		if (entity_token[0] != '{')
			Com_Error(ERR_DROP, "CL_ParseEntitystring: found %s when expecting {", entity_token);

		/* initialize */
		VectorCopy(vec3_origin, origin);
		VectorCopy(vec3_origin, angles);
		VectorSet(scale, 1, 1, 1);
		Vector2Clear(wait);
		spawnflags = frame = skin = 0;
		animname[0] = model[0] = particle[0] = '\0';
		volume = MIX_MAX_VOLUME / 2;

		/* go through all the dictionary pairs */
		while (1) {
			/* parse key */
			entity_token = COM_Parse(&es);
			if (entity_token[0] == '}')
				break;
			if (!es)
				Com_Error(ERR_DROP, "CL_ParseEntitystring: EOF without closing brace");

			Q_strncpyz(keyname, entity_token, sizeof(keyname));

			/* parse value */
			entity_token = COM_Parse(&es);
			if (!es)
				Com_Error(ERR_DROP, "CL_ParseEntitystring: EOF without closing brace");

			if (entity_token[0] == '}')
				Com_Error(ERR_DROP, "CL_ParseEntitystring: closing brace without data");

			/* filter interesting keys */
			if (!Q_strcmp(keyname, "classname"))
				Q_strncpyz(classname, entity_token, sizeof(classname));
			else if (!Q_strcmp(keyname, "model"))
				Q_strncpyz(model, entity_token, sizeof(model));
			else if (!Q_strcmp(keyname, "frame"))
				frame = atoi(entity_token);
			else if (!Q_strcmp(keyname, "anim"))
				Q_strncpyz(animname, entity_token, sizeof(animname));
			else if (!Q_strcmp(keyname, "particle"))
				Q_strncpyz(particle, entity_token, sizeof(particle));
			else if (!Q_strcmp(keyname, "noise"))
				Q_strncpyz(sound, entity_token, sizeof(sound));
			else if (!Q_strcmp(keyname, "volume"))
				volume = atof(entity_token);
			else if (!Q_strcmp(keyname, "modelscale_vec"))
				Com_EParseValue(scale, entity_token, V_VECTOR, 0, sizeof(vec3_t));
			else if (!Q_strcmp(keyname, "origin"))
				Com_EParseValue(origin, entity_token, V_VECTOR, 0, sizeof(vec3_t));
			else if (!Q_strcmp(keyname, "angles") && !angles[YAW])
				/* pitch, yaw, roll */
				Com_EParseValue(angles, entity_token, V_VECTOR, 0, sizeof(vec3_t));
			else if (!Q_strcmp(keyname, "angle") && !angles[YAW])
				angles[YAW] = atof(entity_token);
			else if (!Q_strcmp(keyname, "wait"))
				Com_EParseValue(wait, entity_token, V_POS, 0, sizeof(vec2_t));
			else if (!Q_strcmp(keyname, "spawnflags"))
				spawnflags = atoi(entity_token);
			else if (!Q_strcmp(keyname, "maxlevel"))
				maxlevel = atoi(entity_token);
			else if (!Q_strcmp(keyname, "maxteams"))
				maxmultiplayerteams = atoi(entity_token);
			else if (!Q_strcmp(keyname, "skin"))
				skin = atoi(entity_token);
		}

		/* analyze values - there is one worlspawn per maptile */
		if (!Q_strcmp(classname, "worldspawn")) {
			/* maximum level */
			cl.map_maxlevel = maxlevel;

			if (GAME_IsMultiplayer() && (cl_teamnum->integer > maxmultiplayerteams
			 || cl_teamnum->integer <= TEAM_CIVILIAN)) {
				Com_Printf("The selected team is not useable. "
					"The map doesn't support %i teams but only %i teams\n",
					cl_teamnum->integer, maxmultiplayerteams);
				Cvar_SetValue("cl_teamnum", DEFAULT_TEAMNUM);
				Com_Printf("Set teamnum to %i\n", cl_teamnum->integer);
			}
		} else if (!Q_strcmp(classname, "misc_model")) {
			localModel_t *lm;
			int renderFlags = 0;

			if (!model[0]) {
				Com_Printf("misc_model without \"model\" specified\n");
				continue;
			}

			if (spawnflags & (1 << MISC_MODEL_GLOW))
				renderFlags |= RF_PULSE;

			/* add it */
			lm = LM_AddModel(model, particle, origin, angles, entnum, (spawnflags & 0xFF), renderFlags, scale);
			if (lm) {
				lm->skin = skin;
				lm->frame = frame;
				if (!lm->frame)
					Q_strncpyz(lm->animname, animname, sizeof(lm->animname));
				else
					Com_Printf("Warning: Model has frame and anim parameters - using frame (no animation)\n");
			}
		} else if (!Q_strcmp(classname, "misc_particle")) {
			if (!(dayLightmap && (spawnflags & (1 << SPAWNFLAG_NO_DAY))))
				CL_AddMapParticle(particle, origin, wait, strstart, (spawnflags & 0xFF));
		} else if (!Q_strcmp(classname, "misc_sound")) {
			LE_AddAmbientSound(sound, origin, volume, (spawnflags & 0xFF));
		}

		entnum++;
	}
}

/**
 * @brief Call before entering a new level, or after vid_restart
 * @sa S_RegisterSounds
 */
void CL_LoadMedia (void)
{
	le_t *le;
	int i, max;
	char name[32];

	/* HACK Prepare environment. This is needed hack to allow /devmap and /map usage.
	 * When an user has base built and calls /devmap or /map, cls.missionaircraft has to be set. */
	if (!cls.missionaircraft)
		cls.missionaircraft = AIR_AircraftGetFromIDX(0);

	V_UpdateRefDef();

	if (!cl.configstrings[CS_TILES][0])
		return;					/* no map loaded */

	Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages), _("loading %s"), _(cl.configstrings[CS_MAPTITLE]));
	cls.loadingPercent = 0;

	HUD_ResetWeaponButtons();

	/* register models, pics, and skins */
	Com_Printf("Map: %s\n", cl.configstrings[CS_NAME]);
	if (GAME_IsMultiplayer())
		SCR_SetLoadingBackground(cl.configstrings[CS_NAME]);
	SCR_UpdateScreen();
	R_ModBeginLoading(cl.configstrings[CS_TILES], atoi(cl.configstrings[CS_LIGHTMAP]), cl.configstrings[CS_POSITIONS], cl.configstrings[CS_NAME]);
	CL_ParseEntitystring(map_entitystring);

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
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse) {
			if (le->modelnum1) {
				le->model1 = cl.model_draw[le->modelnum1];
				assert(le->model1);
			}
			if (le->modelnum2) {
				le->model2 = cl.model_draw[le->modelnum2];
				assert(le->model2);
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
	refdef.fog_color[3] = 1.0;
	VectorSet(refdef.fog_color, 0.75, 0.75, 0.75);
}

/**
 * @brief Calculates refdef's FOV_X.
 * Should generally be called after any changes are made to the zoom level (via cl.cam.zoom)
 * @sa V_CalcFovY
 */
void V_CalcFovX (void)
{
	if (cl_isometric->integer) {
		const float zoom =  3.6 * (cl.cam.zoom - cl_camzoommin->value) + 0.3 * cl_camzoommin->value;
		refdef.fov_x = max(min(FOV / zoom, 140.0), 1.0);
	} else {
		refdef.fov_x = max(min(FOV / cl.cam.zoom, 95.0), 55.0);
	}
}

/**
 * @sa V_CalcFovX
 */
static inline void V_CalcFovY (const float width, const float height)
{
	const float x = width / tan(refdef.fov_x / 360.0 * M_PI);
	refdef.fov_y = atan(height / x) * 360.0 / M_PI;
}

/**
 * @brief Updates the refdef
 */
void V_UpdateRefDef (void)
{
	VectorCopy(cl.cam.camorg, refdef.vieworg);
	VectorCopy(cl.cam.angles, refdef.viewangles);

	V_CalcFovY(viddef.viewWidth, viddef.viewHeight);

	/* setup refdef */
	refdef.x = viddef.x;
	refdef.y = viddef.y;
	refdef.width = viddef.viewWidth;
	refdef.height = viddef.viewHeight;
	refdef.time = cl.time * 0.001;
	refdef.worldlevel = cl_worldlevel->integer;
}


/**
 * @sa SCR_UpdateScreen
 */
void V_RenderView (void)
{
	refdef.brush_count = 0;
	refdef.alias_count = 0;

	if (cls.state != ca_active && cls.state != ca_sequence)
		return;

	if (!viddef.viewWidth || !viddef.viewHeight)
		return;

	/* still loading */
	if (!refdef.ready)
		return;

	r_numEntities = 0;

	switch (cls.state) {
	case ca_sequence:
		CL_SequenceRender();
		refdef.rdflags |= RDF_NOWORLDMODEL;
		break;
	default:
		/* tell the bsp thread to start */
		r_threadstate.state = THREAD_BSP;
		/* make sure we are really rendering the world */
		refdef.rdflags &= ~RDF_NOWORLDMODEL;
		/* add local models to the renderer chain */
		LM_AddToScene();
		/* add local entities to the renderer chain */
		LE_AddToScene();
		/* adds pathing data */
		if (cl_mapDebug->integer & MAPDEBUG_PATHING)
			CL_AddPathing();
		/* adds floor arrows */
		if (cl_mapDebug->integer & MAPDEBUG_CELLS)
			CL_DisplayFloorArrows();
		/* adds wall arrows */
		if (cl_mapDebug->integer & MAPDEBUG_WALLS)
			CL_DisplayObstructionArrows();
		/* adds target cursor */
		CL_AddTargeting();
		break;
	}

	/* update ref def - do this even in non 3d mode - we need shaders at loading time */
	V_UpdateRefDef();

	/* render the world */
	R_RenderFrame();

	if (cls.state == ca_sequence)
		CL_Sequence2D();
}

/**
 * @brief Centers the camera on a given grid field
 * @sa CL_CameraMove
 */
void V_CenterView (const pos3_t pos)
{
	vec3_t vec;

	PosToVec(pos, vec);
	VectorCopy(vec, cl.cam.origin);
	Cvar_SetValue("cl_worldlevel", pos[2]);
}
