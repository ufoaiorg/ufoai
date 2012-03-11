/**
 * @file cl_spawn.c
 * @brief Client side entity spawning from map entity string
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "../cgame/cl_game.h"
#include "cl_particle.h"
#include "cl_spawn.h"
#include "../../shared/parse.h"

/* position in the spawnflags */
#define MISC_MODEL_GLOW 9
#define SPAWNFLAG_NO_DAY 8

static const value_t localEntityValues[] = {
	{"skin", V_INT, offsetof(localEntityParse_t, skin), MEMBER_SIZEOF(localEntityParse_t, skin)},
	{"maxteams", V_INT, offsetof(localEntityParse_t, maxteams), MEMBER_SIZEOF(localEntityParse_t, maxteams)},
	{"spawnflags", V_INT, offsetof(localEntityParse_t, spawnflags), MEMBER_SIZEOF(localEntityParse_t, spawnflags)},
	{"maxlevel", V_INT, offsetof(localEntityParse_t, maxLevel), MEMBER_SIZEOF(localEntityParse_t, maxLevel)},
	{"attenuation", V_FLOAT, offsetof(localEntityParse_t, attenuation), MEMBER_SIZEOF(localEntityParse_t, attenuation)},
	{"volume", V_FLOAT, offsetof(localEntityParse_t, volume), MEMBER_SIZEOF(localEntityParse_t, volume)},
	{"frame", V_INT, offsetof(localEntityParse_t, frame), MEMBER_SIZEOF(localEntityParse_t, frame)},
	{"angle", V_FLOAT, offsetof(localEntityParse_t, angle), MEMBER_SIZEOF(localEntityParse_t, angle)},
	{"wait", V_POS, offsetof(localEntityParse_t, wait), MEMBER_SIZEOF(localEntityParse_t, wait)},
	{"angles", V_VECTOR, offsetof(localEntityParse_t, angles), MEMBER_SIZEOF(localEntityParse_t, angles)},
	{"origin", V_VECTOR, offsetof(localEntityParse_t, origin), MEMBER_SIZEOF(localEntityParse_t, origin)},
	{"color", V_VECTOR, offsetof(localEntityParse_t, color), MEMBER_SIZEOF(localEntityParse_t, color)},
	{"_color", V_VECTOR, offsetof(localEntityParse_t, color), MEMBER_SIZEOF(localEntityParse_t, color)},
	{"modelscale_vec", V_VECTOR, offsetof(localEntityParse_t, scale), MEMBER_SIZEOF(localEntityParse_t, scale)},
	{"wait", V_POS, offsetof(localEntityParse_t, wait), MEMBER_SIZEOF(localEntityParse_t, wait)},
	{"classname", V_STRING, offsetof(localEntityParse_t, classname), 0},
	{"model", V_STRING, offsetof(localEntityParse_t, model), 0},
	{"anim", V_STRING, offsetof(localEntityParse_t, anim), 0},
	{"particle", V_STRING, offsetof(localEntityParse_t, particle), 0},
	{"noise", V_STRING, offsetof(localEntityParse_t, noise), 0},
	{"tag", V_STRING, offsetof(localEntityParse_t, tagname), 0},
	{"target", V_STRING, offsetof(localEntityParse_t, target), 0},
	{"targetname", V_STRING, offsetof(localEntityParse_t, targetname), 0},
	{"light", V_INT, offsetof(localEntityParse_t, light), MEMBER_SIZEOF(localEntityParse_t, light)},
	{"ambient_day", V_VECTOR, offsetof(localEntityParse_t, ambientDayColor), MEMBER_SIZEOF(localEntityParse_t, ambientDayColor)},
	{"light_day", V_FLOAT, offsetof(localEntityParse_t, dayLight), MEMBER_SIZEOF(localEntityParse_t, dayLight)},
	{"angles_day", V_POS, offsetof(localEntityParse_t, daySunAngles), MEMBER_SIZEOF(localEntityParse_t, daySunAngles)},
	{"color_day", V_VECTOR, offsetof(localEntityParse_t, daySunColor), MEMBER_SIZEOF(localEntityParse_t, daySunColor)},
	{"ambient_night", V_VECTOR, offsetof(localEntityParse_t, ambientNightColor), MEMBER_SIZEOF(localEntityParse_t, ambientNightColor)},
	{"light_night", V_FLOAT, offsetof(localEntityParse_t, nightLight), MEMBER_SIZEOF(localEntityParse_t, nightLight)},
	{"angles_night", V_POS, offsetof(localEntityParse_t, nightSunAngles), MEMBER_SIZEOF(localEntityParse_t, nightSunAngles)},
	{"color_night", V_VECTOR, offsetof(localEntityParse_t, nightSunColor), MEMBER_SIZEOF(localEntityParse_t, nightSunColor)},

	{NULL, 0, 0, 0}
};

static void SP_worldspawn(const localEntityParse_t *entData);
static void SP_misc_model(const localEntityParse_t *entData);
static void SP_misc_particle(const localEntityParse_t *entData);
static void SP_misc_sound(const localEntityParse_t *entData);
static void SP_light(const localEntityParse_t *entData);

typedef struct {
	const char *name;
	void (*spawn) (const localEntityParse_t *entData);
} spawn_t;

static const spawn_t spawns[] = {
	{"worldspawn", SP_worldspawn},
	{"misc_model", SP_misc_model},
	{"misc_particle", SP_misc_particle},
	{"misc_sound", SP_misc_sound},
	{"light", SP_light},

	{NULL, NULL}
};

/**
 * @brief Finds the spawn function for the entity and calls it
 */
static void CL_SpawnCall (const localEntityParse_t *entData)
{
	const spawn_t *s;

	if (entData->classname[0] == '\0')
		return;

	/* check normal spawn functions */
	for (s = spawns; s->name; s++) {
		/* found it */
		if (Q_streq(s->name, entData->classname)) {
			s->spawn(entData);
			return;
		}
	}
}

/**
 * @brief Parse the map entity string and spawns those entities that are client-side only
 * @sa G_SendEdictsAndBrushModels
 * @sa CL_AddBrushModel
 */
void CL_SpawnParseEntitystring (void)
{
	const char *es = cl.mapData->mapEntityString;
	int entnum = 0, maxLevel;

	if (cl.mapMaxLevel > 0 && cl.mapMaxLevel < PATHFINDING_HEIGHT)
		maxLevel = cl.mapMaxLevel;
	else
		maxLevel = PATHFINDING_HEIGHT;

	/* vid restart? */
	if (cl.numMapParticles || cl.numLMs)
		return;

	/* parse ents */
	while (1) {
		localEntityParse_t entData;
		/* parse the opening brace */
		const char *entityToken = Com_Parse(&es);
		/* memorize the start */
		if (!es)
			break;

		if (entityToken[0] != '{')
			Com_Error(ERR_DROP, "V_ParseEntitystring: found %s when expecting {", entityToken);

		/* initialize */
		OBJZERO(entData);
		VectorSet(entData.scale, 1, 1, 1);
		entData.volume = SND_VOLUME_DEFAULT;
		entData.maxLevel = maxLevel;
		entData.entStringPos = es;
		entData.entnum = entnum;
		entData.maxMultiplayerTeams = TEAM_MAX_HUMAN;
		entData.attenuation = SOUND_ATTN_IDLE;

		/* go through all the dictionary pairs */
		while (1) {
			const value_t *v;
			/* parse key */
			entityToken = Com_Parse(&es);
			if (entityToken[0] == '}')
				break;
			if (!es)
				Com_Error(ERR_DROP, "V_ParseEntitystring: EOF without closing brace");

			for (v = localEntityValues; v->string; v++)
				if (Q_streq(entityToken, v->string)) {
					/* found a definition */
					entityToken = Com_Parse(&es);
					if (!es)
						Com_Error(ERR_DROP, "V_ParseEntitystring: EOF without closing brace");
					Com_EParseValue(&entData, entityToken, v->type, v->ofs, v->size);
					break;
				}
		}
		CL_SpawnCall(&entData);

		entnum++;
	}

	/* after we have parsed all the entities we can resolve the target, targetname
	 * connections for the misc_model entities */
	LM_Think();
}

#define MIN_AMBIENT_COMPONENT 0.1
#define MIN_AMBIENT_SUM 0.50

/** @note Defaults should match those of ufo2map, or lighting will be inconsistent between world and models */
static void SP_worldspawn (const localEntityParse_t *entData)
{
	const int dayLightmap = CL_GetConfigStringInteger(CS_LIGHTMAP);
	int i;
	vec3_t sunAngles;
	vec4_t sunColor;
	vec_t sunIntensity;

	/* maximum level */
	cl.mapMaxLevel = entData->maxLevel;

	if (GAME_IsMultiplayer()) {
		if (cl_teamnum->integer > entData->maxMultiplayerTeams || cl_teamnum->integer <= TEAM_CIVILIAN) {
			Com_Printf("The selected team is not usable. "
				"The map doesn't support %i teams but only %i teams\n",
				cl_teamnum->integer, entData->maxMultiplayerTeams);
			Cvar_SetValue("cl_teamnum", TEAM_DEFAULT);
			Com_Printf("Set teamnum to %i\n", cl_teamnum->integer);
		}
	}

	/** @todo - make sun position/color vary based on local time at location? */

	/** @note Some vectors have exra elements to comply with mathlib and/or OpenGL conventions, but handled as shorter ones */
	if (dayLightmap) {
		/* set defaults for daylight */
		Vector4Set(refdef.ambientColor, 0.26, 0.26, 0.26, 1.0);
		sunIntensity = 280;
		VectorSet(sunAngles, -75, 100, 0);
		Vector4Set(sunColor, 0.90, 0.75, 0.65, 1.0);

		/* override defaults with data from worldspawn entity, if any */
		if (VectorNotEmpty(entData->ambientDayColor))
			VectorCopy(entData->ambientDayColor, refdef.ambientColor);

		if (entData->dayLight)
			sunIntensity = entData->dayLight;

		if (Vector2NotEmpty(entData->daySunAngles))
			Vector2Copy(entData->daySunAngles, sunAngles);

		if (VectorNotEmpty(entData->daySunColor))
			VectorCopy(entData->daySunColor, sunColor);

		Vector4Set(refdef.sunSpecularColor, 1.0, 1.0, 0.9, 1);
	} else {
		/* set defaults for night light */
		Vector4Set(refdef.ambientColor, 0.16, 0.16, 0.17, 1.0);
		sunIntensity = 15;
		VectorSet(sunAngles, -80, 220, 0);
		Vector4Set(sunColor, 0.25, 0.25, 0.35, 1.0);

		/* override defaults with data from worldspawn entity, if any */
		if (VectorNotEmpty(entData->ambientNightColor))
			VectorCopy(entData->ambientNightColor, refdef.ambientColor);

		if (entData->nightLight)
			sunIntensity = entData->nightLight;

		if (Vector2NotEmpty(entData->nightSunAngles))
			Vector2Copy(entData->nightSunAngles, sunAngles);

		if (VectorNotEmpty(entData->nightSunColor))
			VectorCopy(entData->nightSunColor, sunColor);

		Vector4Set(refdef.sunSpecularColor, 0.5, 0.5, 0.7, 1);
	}

	ColorNormalize(sunColor, sunColor);
	VectorScale(sunColor, sunIntensity/255.0, sunColor);
	Vector4Copy(sunColor, refdef.sunDiffuseColor);

	/* clamp ambient for models */
	Vector4Copy(refdef.ambientColor, refdef.modelAmbientColor);
	for (i = 0; i < 3; i++)
		if (refdef.modelAmbientColor[i] < MIN_AMBIENT_COMPONENT)
			refdef.modelAmbientColor[i] = MIN_AMBIENT_COMPONENT;

	/* scale it into a reasonable range, the clamp above ensures this will work */
	while (VectorSum(refdef.modelAmbientColor) < MIN_AMBIENT_SUM)
		VectorScale(refdef.modelAmbientColor, 1.25, refdef.modelAmbientColor);

	AngleVectors(sunAngles, refdef.sunVector, NULL, NULL);
	refdef.sunVector[3] = 0.0; /* to use as directional light source in OpenGL */

	/** @todo Parse fog from worldspawn config */
	refdef.weather = WEATHER_NONE;
	refdef.fogColor[3] = 1.0;
	VectorSet(refdef.fogColor, 0.75, 0.75, 0.75);
}

static void SP_misc_model (const localEntityParse_t *entData)
{
	localModel_t *lm;
	int renderFlags = 0;

	if (entData->model[0] == '\0') {
		Com_Printf("misc_model without \"model\" specified\n");
		return;
	}

	if (entData->spawnflags & (1 << MISC_MODEL_GLOW))
		renderFlags |= RF_PULSE;

	/* add it */
	lm = LM_AddModel(entData->model, entData->origin, entData->angles, entData->entnum, (entData->spawnflags & 0xFF), renderFlags, entData->scale);
	if (lm) {
		if (LM_GetByID(entData->targetname) != NULL)
			Com_Error(ERR_DROP, "Ambiguous targetname '%s'", entData->targetname);
		Q_strncpyz(lm->id, entData->targetname, sizeof(lm->id));
		Q_strncpyz(lm->target, entData->target, sizeof(lm->target));
		Q_strncpyz(lm->tagname, entData->tagname, sizeof(lm->tagname));

		if (lm->animname[0] != '\0' && lm->tagname[0] != '\0') {
			Com_Printf("Warning: Model has animation set, but also a tag - use the tag and skip the animation\n");
			lm->animname[0] = '\0';
		}

		if (lm->tagname[0] != '\0' && lm->target[0] == '\0') {
			Com_Error(ERR_DROP, "Warning: Model has tag set, but no target given");
		}

		lm->think = LMT_Init;
		lm->skin = entData->skin;
		lm->frame = entData->frame;
		if (!lm->frame)
			Q_strncpyz(lm->animname, entData->anim, sizeof(lm->animname));
		else
			Com_Printf("Warning: Model has frame and anim parameters - using frame (no animation)\n");
	}
}

static void SP_misc_particle (const localEntityParse_t *entData)
{
	const int dayLightmap = CL_GetConfigStringInteger(CS_LIGHTMAP);
	if (!(dayLightmap && (entData->spawnflags & (1 << SPAWNFLAG_NO_DAY))))
		CL_AddMapParticle(entData->particle, entData->origin, entData->wait, entData->entStringPos, (entData->spawnflags & 0xFF));
}

static void SP_misc_sound (const localEntityParse_t *entData)
{
	const int dayLightmap = CL_GetConfigStringInteger(CS_LIGHTMAP);
	if (!(dayLightmap && (entData->spawnflags & (1 << SPAWNFLAG_NO_DAY))))
		LE_AddAmbientSound(entData->noise, entData->origin, (entData->spawnflags & 0xFF), entData->volume, entData->attenuation);
}

/**
 * @param entData
 */
static void SP_light (const localEntityParse_t *entData)
{
	const int dayLightmap = CL_GetConfigStringInteger(CS_LIGHTMAP);

	if (entData->light < 1.0)
		return;

	if (!(dayLightmap && (entData->spawnflags & (1 << SPAWNFLAG_NO_DAY)))) {
		R_AddStaticLight(entData->origin, entData->light, entData->color);
	}
}
