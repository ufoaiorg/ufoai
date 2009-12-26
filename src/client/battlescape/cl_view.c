/**
 * @file cl_view.c
 * @brief Player rendering positioning.
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../cl_sequence.h"
#include "cl_view.h"
#include "../renderer/r_main.h"
#include "../renderer/r_entity.h"
#include "../../shared/parse.h"

/** position in the spawnflags */
#define MISC_MODEL_GLOW 9
#define SPAWNFLAG_NO_DAY 8

cvar_t* cl_map_debug;

/**
 * @brief Parse the map entity string and spawns those entities that are client-side only
 * @sa G_SendEdictsAndBrushModels
 * @sa CL_AddBrushModel
 */
static void V_ParseEntitystring (void)
{
	char keyname[256];
	char classname[MAX_VAR];
	char animname[MAX_QPATH], model[MAX_QPATH], particle[MAX_QPATH], sound[MAX_QPATH];
	vec3_t origin, angles, scale;
	vec2_t wait;
	int maxLevel = 8, maxMultiplayerTeams = 2, entnum = 0;
	int skin, frame, spawnflags;
	float volume;
	const int dayLightmap = atoi(cl.configstrings[CS_LIGHTMAP]);
	const char *es = CM_EntityString();

	cl.mapMaxLevel = PATHFINDING_HEIGHT;
	if (cl.mapMaxLevelBase > 0 && cl.mapMaxLevelBase < PATHFINDING_HEIGHT)
		cl.mapMaxLevel = maxLevel = cl.mapMaxLevelBase;

	/* vid restart? */
	if (cl.numMapParticles || cl.numLMs)
		return;

	/* parse ents */
	while (1) {
		/* parse the opening brace */
		const char *entityToken = Com_Parse(&es);
		/* memorize the start */
		const char *strstart = es;
		if (!es)
			break;

		if (entityToken[0] != '{')
			Com_Error(ERR_DROP, "V_ParseEntitystring: found %s when expecting {", entityToken);

		/* initialize */
		VectorCopy(vec3_origin, origin);
		VectorCopy(vec3_origin, angles);
		VectorSet(scale, 1, 1, 1);
		Vector2Clear(wait);
		volume = SND_VOLUME_DEFAULT;
		spawnflags = frame = skin = 0;
		animname[0] = model[0] = particle[0] = '\0';

		/* go through all the dictionary pairs */
		while (1) {
			/* parse key */
			entityToken = Com_Parse(&es);
			if (entityToken[0] == '}')
				break;
			if (!es)
				Com_Error(ERR_DROP, "V_ParseEntitystring: EOF without closing brace");

			Q_strncpyz(keyname, entityToken, sizeof(keyname));

			/* parse value */
			entityToken = Com_Parse(&es);
			if (!es)
				Com_Error(ERR_DROP, "V_ParseEntitystring: EOF without closing brace");

			if (entityToken[0] == '}')
				Com_Error(ERR_DROP, "V_ParseEntitystring: closing brace without data");

			/* filter interesting keys */
			if (!strcmp(keyname, "classname"))
				Q_strncpyz(classname, entityToken, sizeof(classname));
			else if (!strcmp(keyname, "model"))
				Q_strncpyz(model, entityToken, sizeof(model));
			else if (!strcmp(keyname, "frame"))
				frame = atoi(entityToken);
			else if (!strcmp(keyname, "volume"))
				volume = atof(entityToken);
			else if (!strcmp(keyname, "anim"))
				Q_strncpyz(animname, entityToken, sizeof(animname));
			else if (!strcmp(keyname, "particle"))
				Q_strncpyz(particle, entityToken, sizeof(particle));
			else if (!strcmp(keyname, "noise"))
				Q_strncpyz(sound, entityToken, sizeof(sound));
			else if (!strcmp(keyname, "modelscale_vec"))
				Com_EParseValue(scale, entityToken, V_VECTOR, 0, sizeof(scale));
			else if (!strcmp(keyname, "origin"))
				Com_EParseValue(origin, entityToken, V_VECTOR, 0, sizeof(origin));
			else if (!strcmp(keyname, "angles") && !angles[YAW])
				/* pitch, yaw, roll */
				Com_EParseValue(angles, entityToken, V_VECTOR, 0, sizeof(angles));
			else if (!strcmp(keyname, "angle") && !angles[YAW])
				angles[YAW] = atof(entityToken);
			else if (!strcmp(keyname, "wait"))
				Com_EParseValue(wait, entityToken, V_POS, 0, sizeof(wait));
			else if (!strcmp(keyname, "spawnflags"))
				spawnflags = atoi(entityToken);
			else if (!strcmp(keyname, "maxlevel"))
				maxLevel = atoi(entityToken);
			else if (!strcmp(keyname, "maxteams"))
				maxMultiplayerTeams = atoi(entityToken);
			else if (!strcmp(keyname, "skin"))
				skin = atoi(entityToken);
		}

		/* analyze values - there is one worlspawn per maptile */
		if (!strcmp(classname, "worldspawn")) {
			/* maximum level */
			cl.mapMaxLevel = maxLevel;

			if (GAME_IsMultiplayer() && (cl_teamnum->integer > maxMultiplayerTeams
			 || cl_teamnum->integer <= TEAM_CIVILIAN)) {
				Com_Printf("The selected team is not usable. "
					"The map doesn't support %i teams but only %i teams\n",
					cl_teamnum->integer, maxMultiplayerTeams);
				Cvar_SetValue("cl_teamnum", TEAM_DEFAULT);
				Com_Printf("Set teamnum to %i\n", cl_teamnum->integer);
			}
		} else if (!strcmp(classname, "misc_model")) {
			localModel_t *lm;
			int renderFlags = 0;

			if (model[0] == '\0') {
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
		} else if (!strcmp(classname, "misc_particle")) {
			if (!(dayLightmap && (spawnflags & (1 << SPAWNFLAG_NO_DAY))))
				CL_AddMapParticle(particle, origin, wait, strstart, (spawnflags & 0xFF));
		} else if (!strcmp(classname, "misc_sound")) {
			if (!(dayLightmap && (spawnflags & (1 << SPAWNFLAG_NO_DAY))))
				LE_AddAmbientSound(sound, origin, (spawnflags & 0xFF), volume);
		}

		entnum++;
	}
}

/**
 * @brief Call before entering a new level, or after vid_restart
 */
void V_LoadMedia (void)
{
	le_t *le;
	int i, max;
	char name[32];

	V_UpdateRefDef();

	if (!cl.configstrings[CS_TILES][0])
		return;					/* no map loaded */

	Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages), _("loading %s"), _(cl.configstrings[CS_MAPTITLE]));
	cls.loadingPercent = 0;

	/* register models, pics, and skins */
	SCR_UpdateScreen();
	R_ModBeginLoading(cl.configstrings[CS_TILES], atoi(cl.configstrings[CS_LIGHTMAP]), cl.configstrings[CS_POSITIONS], cl.configstrings[CS_NAME]);
	V_ParseEntitystring();

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
	for (i = 0, le = LEs; i < cl.numLEs; i++, le++)
		if (le->inuse) {
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
void V_CalcFovX (void)
{
	if (cl_isometric->integer) {
		const float zoom =  3.6 * (cl.cam.zoom - cl_camzoommin->value) + 0.3 * cl_camzoommin->value;
		refdef.fieldOfViewX = max(min(FOV / zoom, 140.0), 1.0);
	} else {
		refdef.fieldOfViewX = max(min(FOV / cl.cam.zoom, 95.0), 55.0);
	}
}

/**
 * @sa V_CalcFovX
 */
static inline void V_CalcFovY (const float width, const float height)
{
	const float x = width / tan(refdef.fieldOfViewX / 360.0 * M_PI);
	refdef.fieldOfViewY = atan(height / x) * 360.0 / M_PI;
}

/**
 * @brief Updates the refdef
 */
void V_UpdateRefDef (void)
{
	VectorCopy(cl.cam.camorg, refdef.viewOrigin);
	VectorCopy(cl.cam.angles, refdef.viewAngles);

	V_CalcFovY(viddef.viewWidth, viddef.viewHeight);

	/* setup refdef */
	refdef.time = cl.time * 0.001;
	refdef.worldlevel = cl_worldlevel->integer;
}


/**
 * @sa SCR_UpdateScreen
 */
void V_RenderView (void)
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

	r_numEntities = 0;

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
		if (cl_map_debug->integer & MAPDEBUG_PATHING)
			CL_AddPathing();
		/* adds floor arrows */
		if (cl_map_debug->integer & MAPDEBUG_CELLS)
			CL_DisplayFloorArrows();
		/* adds wall arrows */
		if (cl_map_debug->integer & MAPDEBUG_WALLS)
			CL_DisplayObstructionArrows();
		/* adds target cursor */
		CL_AddTargeting();
		break;
	}

	/* update ref def */
	V_UpdateRefDef();

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
void V_CenterView (const pos3_t pos)
{
	vec3_t vec;

	PosToVec(pos, vec);
	VectorCopy(vec, cl.cam.origin);
	Cvar_SetValue("cl_worldlevel", pos[2]);
}
