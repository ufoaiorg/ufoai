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
#include "cl_global.h"

/* global vars */
cvar_t *cursor;

int map_maxlevel;
int map_maxlevel_base = 0;
sun_t map_sun;

/* static vars */
static cvar_t *cl_stats;

static int r_numdlights;
static dlight_t r_dlights[MAX_DLIGHTS];

static int r_numentities;
static entity_t r_entities[MAX_ENTITIES];

static lightstyle_t r_lightstyles[MAX_LIGHTSTYLES];

static dlight_t map_lights[MAX_MAP_LIGHTS];
static int map_numlights;

static float map_fog;
static vec4_t map_fogColor;

/**
 * @brief
 * @sa V_RenderView
 */
static void V_ClearScene (void)
{
	r_numdlights = 0;
	r_numentities = 0;
}


/**
 * @brief
 * @sa
 */
entity_t *V_GetEntity (void)
{
	return r_entities + r_numentities;
}


/**
 * @brief
 * @param[in] ent
 * @sa
 */
void V_AddEntity (entity_t * ent)
{
	if (r_numentities >= MAX_ENTITIES)
		return;

	r_entities[r_numentities++] = *ent;
}


/**
 * @brief
 * @param[in] intensity The radius of the light
 * @sa
 */
void V_AddLight (vec3_t org, float intensity, vec3_t color)
{
	dlight_t *dl;

	if (r_numdlights >= MAX_DLIGHTS)
		return;
	dl = &r_dlights[r_numdlights++];
	VectorCopy(org, dl->origin);
	dl->intensity = intensity;
	VectorCopy(color, dl->color);
}


/**
 * @brief
 * @param
 * @sa CL_AddLightStyles
 */
void V_AddLightStyle (int style, float r, float g, float b)
{
	lightstyle_t *ls;

	if (style < 0 || style > MAX_LIGHTSTYLES)
		Com_Error(ERR_DROP, "Bad light style %i", style);
	ls = &r_lightstyles[style];

	ls->white = r + g + b;
	ls->rgb[0] = r;
	ls->rgb[1] = g;
	ls->rgb[2] = b;
}

/**
 * @brief
 * @param
 * @sa
 */
static void CL_ParseEntitystring (const char *es)
{
	const char *strstart, *entity_token;
	char keyname[256];

	char classname[MAX_VAR];
	char animname[MAX_QPATH];
	char model[MAX_QPATH];
	char particle[MAX_QPATH];
	char sound[MAX_QPATH];
	float light;
	vec3_t color, ambient, origin, angles, lightangles;
	vec2_t wait;
	int spawnflags;
	int maxlevel = 8;
	int maxmultiplayerteams = 2;
	int entnum;
	int skin;
	int frame;
	float volume = 255.0f;
	/* FIXME: use the  SOUND_LOOPATTENUATE value here as soon as the spatialize function is fixed */
	float attenuation = 0.0f;

	VectorSet(map_fogColor, 0.5f, 0.5f, 0.5f);
	map_fogColor[3] = 1.0f;
	map_maxlevel = 8;
	if (map_maxlevel_base >= 1) {
		map_maxlevel = maxlevel = map_maxlevel_base;
	}
	map_numlights = 0;
	map_fog = 0.0;
	entnum = 0;
	numLMs = 0;
	numMPs = 0;

	/* parse ents */
	while (1) {
		/* initialize */
		VectorCopy(vec3_origin, ambient);
		color[0] = color[1] = color[2] = 1.0f;
		VectorCopy(vec3_origin, origin);
		VectorCopy(vec3_origin, angles);
		wait[0] = wait[1] = 0;
		spawnflags = 0;
		light = 0;
		frame = 0;
		animname[0] = 0;
		model[0] = 0;
		particle[0] = 0;
		skin = 0;

		/* parse the opening brace */
		entity_token = COM_Parse(&es);
		if (!es)
			break;
		if (entity_token[0] != '{')
			Com_Error(ERR_DROP, "CL_ParseEntitystring: found %s when expecting {", entity_token);

		/* memorize the start */
		strstart = es;

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
			else if (!Q_strcmp(keyname, "sound"))
				Q_strncpyz(sound, entity_token, sizeof(sound));
			else if (!Q_strcmp(keyname, "attenuation"))
				attenuation = atof(entity_token);
			else if (!Q_strcmp(keyname, "volume"))
				volume = atof(entity_token);
			else if (!Q_strcmp(keyname, "_color") || !Q_strcmp(keyname, "lightcolor"))
				Com_ParseValue(color, entity_token, V_VECTOR, 0, sizeof(vec3_t));
			else if (!Q_strcmp(keyname, "origin"))
				Com_ParseValue(origin, entity_token, V_VECTOR, 0, sizeof(vec3_t));
			else if (!Q_strcmp(keyname, "ambient") || !Q_strcmp(keyname, "lightambient"))
				Com_ParseValue(ambient, entity_token, V_VECTOR, 0, sizeof(vec3_t));
			else if (!Q_strcmp(keyname, "angles") && !angles[YAW])
				/* pitch, yaw, roll */
				Com_ParseValue(angles, entity_token, V_VECTOR, 0, sizeof(vec3_t));
			else if (!Q_strcmp(keyname, "angle") && !angles[YAW])
				angles[YAW] = atof(entity_token);
			else if (!Q_strcmp(keyname, "wait"))
				Com_ParseValue(wait, entity_token, V_POS, 0, sizeof(vec2_t));
			else if (!Q_strcmp(keyname, "light"))
				light = atof(entity_token);
			else if (!Q_strcmp(keyname, "lightangles"))
				Com_ParseValue(lightangles, entity_token, V_VECTOR, 0, sizeof(vec3_t));
			else if (!Q_strcmp(keyname, "spawnflags"))
				spawnflags = atoi(entity_token);
			else if (!Q_strcmp(keyname, "maxlevel"))
				maxlevel = atoi(entity_token);
			else if (!Q_strcmp(keyname, "maxteams"))
				maxmultiplayerteams = atoi(entity_token);
			else if (!Q_strcmp(keyname, "fog"))
				map_fog = atof(entity_token);
			else if (!Q_strcmp(keyname, "fogcolor"))
				Com_ParseValue(map_fogColor, entity_token, V_VECTOR, 0, sizeof(vec3_t));
			else if (!Q_strcmp(keyname, "skin"))
				skin = atoi(entity_token);
		}

		/* analyze values */
		if (!Q_strcmp(classname, "worldspawn")) {
			/* init sun */
			angles[YAW] *= torad;
			angles[PITCH] *= torad;
			map_sun.dir[0] = cos(angles[YAW]) * sin(angles[PITCH]);
			map_sun.dir[1] = sin(angles[YAW]) * sin(angles[PITCH]);
			map_sun.dir[2] = cos(angles[PITCH]);

			VectorNormalize(color);
			VectorScale(color, light / 100, map_sun.color);
			map_sun.color[3] = 1.0;

			/* init ambient */
			VectorScale(ambient, 1.4, map_sun.ambient);
			map_sun.ambient[3] = 1.0;

			/* maximum level */
			map_maxlevel = maxlevel;

			if (!ccs.singleplayer && (teamnum->integer > maxmultiplayerteams
									|| teamnum->integer <= TEAM_CIVILIAN)) {
				Com_Printf("The selected team is not useable. "
					"The map doesn't support %i teams but only %i teams\n",
					teamnum->integer, maxmultiplayerteams);
				Cvar_SetValue("teamnum", DEFAULT_TEAMNUM);
				Com_Printf("Set teamnum to %i\n", teamnum->integer);
			}
		} else if (!Q_strcmp(classname, "light") && light) {
			dlight_t *newlight;

			/* add light to list */
			if (map_numlights >= MAX_MAP_LIGHTS)
				Com_Error(ERR_DROP, "Too many lights...");

			newlight = &(map_lights[map_numlights++]);
			VectorCopy(origin, newlight->origin);
			VectorNormalize2(color, newlight->color);
			newlight->intensity = light;
		} else if (!Q_strcmp(classname, "misc_model")) {
			localModel_t *lm;

			if (!model[0]) {
				Com_Printf("misc_model without \"model\" specified\n");
				continue;
			}

			/* add it */
			lm = CL_AddLocalModel(model, particle, origin, angles, entnum, (spawnflags & 0xFF));

			/* get light parameters */
			if (lm) {
				if (light != 0.0) {
					lm->flags |= LMF_LIGHTFIXED;
					VectorCopy(color, lm->lightcolor);
					VectorCopy(ambient, lm->lightambient);
					lm->lightcolor[3] = light;
					lm->lightambient[3] = 1.0;

					lightangles[YAW] *= torad;
					lightangles[PITCH] *= torad;
					lm->lightorigin[0] = cos(lightangles[YAW]) * sin(lightangles[PITCH]);
					lm->lightorigin[1] = sin(lightangles[YAW]) * sin(lightangles[PITCH]);
					lm->lightorigin[2] = cos(lightangles[PITCH]);
					lm->lightorigin[3] = 0.0;
				}
				lm->skin = skin;
				lm->frame = frame;
				if (!lm->frame)
					Q_strncpyz(lm->animname, animname, sizeof(lm->animname));
				else
					Com_Printf("Warning: Model has frame and anim parameters - using frame (no animation)\n");
			}
		} else if (!Q_strcmp(classname, "misc_particle")) {
			CL_AddMapParticle(particle, origin, wait, strstart, (spawnflags & 0xFF));
		} else if (!Q_strcmp(classname, "misc_sound")) {
			LE_AddAmbientSound(sound, origin, volume, attenuation);
		} else if (!Q_strcmp(classname, "misc_rope")) {
			Com_Printf("implement misc_rope\n");
		} else if (!Q_strcmp(classname, "misc_decal")) {
			Com_Printf("implement misc_decal\n");
		} else if (!Q_strcmp(classname, "func_breakable")
			|| !Q_strcmp(classname, "func_door")) {
			VectorClear(angles);
			CL_AddLocalModel(model, particle, origin, angles, entnum, (spawnflags & 0xFF));
			Com_DPrintf(DEBUG_CLIENT, "Add %i as local model (%s)\n", entnum, classname);
		}

		entnum++;
	}
}

/**
 * @brief Call before entering a new level, or after changing dlls
 */
void CL_LoadMedia (void)
{
	le_t *le;
	int i, max;
	char name[37];
	char *str;

	/* Prepare environment. This is needed hack to allow /devmap and /map usage.
	   When an user has base built and calls /devmap or /map, cls.missionaircraft has to be set. */
	if (!cls.missionaircraft) {
		cls.missionaircraft = AIR_AircraftGetFromIdx(0);
		if (cls.missionaircraft)
			cls.missionaircraft->homebase = &gd.bases[0];
		else
			Com_Printf("Could not set mission aircraft\n");
	}

	/* this is needed to get shaders/image filters in game */
	/* the renderer needs to know them before the textures get loaded */
	V_UpdateRefDef();

	if (!cl.configstrings[CS_TILES][0])
		return;					/* no map loaded */

	cls.loadingMessage = qtrue;
	Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages), _("loading %s"), _(cl.configstrings[CS_MAPTITLE]));
	cls.loadingPercent = 0;

	CL_ResetWeaponButtons();

	/* register models, pics, and skins */
	Com_Printf("Map: %s\n", cl.configstrings[CS_NAME]);
	if (!ccs.singleplayer)
		SCR_SetLoadingBackground(cl.configstrings[CS_NAME]);
	SCR_UpdateScreen();
	R_ModBeginLoading(cl.configstrings[CS_TILES], cl.configstrings[CS_POSITIONS]);
	CL_ParseEntitystring(map_entitystring);
	Com_Printf("                                     \r");

	Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages), _("loading models..."));
	cls.loadingPercent += 10.0f;
	/* precache status bar pics */
	Com_Printf("pics\n");
	SCR_UpdateScreen();
	SCR_TouchPics();
	Com_Printf("                                     \r");

	CL_RegisterLocalModels();
	CL_ParticleRegisterArt();

	for (i = 1, max = 0; i < MAX_MODELS && cl.configstrings[CS_MODELS+i][0]; i++)
		max++;

	max += csi.numODs;

	for (i = 1; i < MAX_MODELS && cl.configstrings[CS_MODELS + i][0]; i++) {
		Q_strncpyz(name, cl.configstrings[CS_MODELS + i], sizeof(name));
		if (name[0] != '*') {
			Com_Printf("%s\r", name);
			Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages),
				_("loading %s"), (strlen(name) > 40) ? &name[strlen(name) - 40] : name);
		}
		SCR_UpdateScreen();
		IN_SendKeyEvents();	/* pump message loop */
		cl.model_draw[i] = R_RegisterModelShort(cl.configstrings[CS_MODELS + i]);
		if (name[0] == '*')
			cl.model_clip[i] = CM_InlineModel(cl.configstrings[CS_MODELS + i]);
		else
			cl.model_clip[i] = NULL;
		if (name[0] != '*')
			Com_Printf("                                     \r");

		cls.loadingPercent += 80.0f / (float)max;
	}

	/* update le model references */
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse) {
			if (le->modelnum1)
				le->model1 = cl.model_draw[le->modelnum1];
			if (le->modelnum2)
				le->model2 = cl.model_draw[le->modelnum2];
		}

	/* load weapons stuff */
	for (i = 0; i < csi.numODs; i++) {
		cls.loadingPercent += 90.0f / (float)max;

		/* don't load any other item models */
		if (csi.ods[i].buytype > MAX_SOLDIER_EQU_BUYTYPES)
			continue;
		str = csi.ods[i].model;
		SCR_UpdateScreen();
		cl.model_weapons[i] = R_RegisterModelShort(str);
		Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages),
			_("loading %s"), (strlen(str) > 40) ? &str[strlen(str) - 40] : str);
	}

	cls.loadingPercent = 100.0f;
	SCR_UpdateScreen();

	/* the renderer can now free unneeded stuff */
	R_ModEndLoading();

	/* waiting for EV_START */
	Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages), _("Awaiting game start"));
	SCR_UpdateScreen();
	cl.refresh_prepped = qtrue;
	cl.force_refdef = qtrue;	/* make sure we have a valid refdef */
}

/**
 * @brief Calculates refdef's FOV_X.
 * Should generally be called after any changes are made to the zoom level (via cl.cam.zoom)
 * @sa V_CalcFovY
 */
void V_CalcFovX (void)
{
	if (cl_isometric->integer) {
		float zoom =  3.6 * (cl.cam.zoom - cl_camzoommin->value) + 0.3 * cl_camzoommin->value;
		refdef.fov_x = max(min(FOV / zoom, 140.0), 1.0);
	} else {
		refdef.fov_x = max(min(FOV / cl.cam.zoom, 95.0), 55.0);
	}
}

/**
 * @brief
 * @param
 * @sa V_CalcFovX
 */
static void V_CalcFovY (float width, float height)
{
	float x;

	x = width / tan(refdef.fov_x / 360.0 * M_PI);
	refdef.fov_y = atan(height / x) * 360.0 / M_PI;
}

/**
 * @brief Updates the refdef
 */
void V_UpdateRefDef (void)
{
	VectorCopy(cl.cam.camorg, refdef.vieworg);
	VectorCopy(cl.cam.angles, refdef.viewangles);

	V_CalcFovY(scr_vrect.width, scr_vrect.height);

	/* setup refdef */
	refdef.rdflags &= ~RDF_NOWORLDMODEL;
	refdef.x = scr_vrect.x;
	refdef.y = scr_vrect.y;
	refdef.width = scr_vrect.width;
	refdef.height = scr_vrect.height;
	refdef.time = cl.time * 0.001;	refdef.worldlevel = cl_worldlevel->integer;
	refdef.num_entities = r_numentities;
	refdef.entities = r_entities;
	refdef.num_shaders = r_numshaders;
	refdef.shaders = r_shaders;
	refdef.num_dlights = r_numdlights;
	refdef.dlights = r_dlights;
	refdef.lightstyles = r_lightstyles;
	refdef.fog = map_fog;
	refdef.fogColor = map_fogColor;

	refdef.num_ptls = numPtls;
	refdef.ptls = ptl;
	refdef.ptl_art = ptlArt;

	refdef.sun = &map_sun;
	if (cls.state == ca_sequence || cls.state == ca_ptledit)
		refdef.num_lights = 0;
	else {
		refdef.ll = map_lights;
		refdef.num_lights = map_numlights;
	}
	VectorClear(refdef.blend);
}

/**
 * @brief
 * @sa SCR_UpdateScreen
 */
void V_RenderView (void)
{
	if (cls.state != ca_active && cls.state != ca_sequence && cls.state != ca_ptledit)
		return;

	if (!scr_vrect.width || !scr_vrect.height)
		return;

	/* still loading */
	if (!cl.refresh_prepped)
		return;

	V_ClearScene();

	switch (cls.state) {
	case ca_sequence:
		CL_SequenceRender();
		refdef.rdflags |= RDF_NOWORLDMODEL;
		break;
	case ca_ptledit:
		PE_RenderParticles();
		refdef.rdflags |= RDF_NOWORLDMODEL;
		break;
	default:
		LM_AddToScene();
		LE_AddToScene();
		CL_AddTargeting();
		CL_AddLightStyles();
		break;
	}

	/* update ref def - do this even in non 3d mode - we need shaders at loading time */
	V_UpdateRefDef();

	/* render the frame */
	R_RenderFrame();
	if (cl_stats->integer)
		Com_Printf("ent:%i  lt:%i\n", r_numentities, r_numdlights);
	if (log_stats->integer && (log_stats_file != 0))
		fprintf(log_stats_file, "%i,%i,", r_numentities, r_numdlights);

	if (cls.state == ca_sequence)
		CL_Sequence2D();
}


/**
 * @brief
 * @param pos
 * @sa
 */
void V_CenterView (pos3_t pos)
{
	vec3_t vec;

	PosToVec(pos, vec);
	VectorCopy(vec, cl.cam.reforg);
	Cvar_SetValue("cl_worldlevel", pos[2]);
}


/**
 * @brief Just prints the vieworg vector to console
 */
static void V_Viewpos_f (void)
{
	Com_Printf("(%i %i %i) : %i\n", (int) refdef.vieworg[0], (int) refdef.vieworg[1], (int) refdef.vieworg[2], (int) refdef.viewangles[YAW]);
}

/**
 * @brief
 * @param
 * @sa CL_Init
 */
void V_Init (void)
{
	Cmd_AddCommand("viewpos", V_Viewpos_f, NULL);
	Cmd_AddCommand("shaderlist", CL_ShaderList_f, NULL);

	cursor = Cvar_Get("cursor", "1", CVAR_ARCHIVE, "Which cursor should be shown - 0-9");

	cl_stats = Cvar_Get("cl_stats", "0", 0, NULL);
}
