/**
 * @file r_material_edit.c
 * @brief Material in-place editing related code
 */

/*
 Copyright (C) 1997-2008 UFO:AI Team

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

#include "r_local.h"
#include "r_material.h"
#include "r_material_edit.h"

static const char *textureDir = "textures/";

/**
 * @sa R_ConstByName
 */
static const char *R_ConstToName (unsigned int blend)
{
	switch (blend) {
	case GL_ONE:
		return "GL_ONE";
	case GL_ZERO:
		return "GL_ZERO";
	case GL_SRC_ALPHA:
		return "GL_SRC_ALPHA";
	case GL_ONE_MINUS_SRC_ALPHA:
		return "GL_ONE_MINUS_SRC_ALPHA";
	case GL_SRC_COLOR:
		return "GL_SRC_COLOR";
	case GL_DST_COLOR:
		return "GL_DST_COLOR";
	default:
		return "INVALID";
	}
}

static void R_MaterialToBuffer (char *buffer, size_t length)
{
	int i;

	for (i = 0; i < r_numImages; i++) {
		char buf[512] = "";
		const image_t *image = &r_images[i];
		const material_t *m = &image->material;
		const materialStage_t *s = m->stages;
		const float specular = m->specular ? m->specular : DEFAULT_SPECULAR;
		const float parallax = m->parallax ? m->parallax : DEFAULT_PARALLAX;
		const float bump = m->bump ? m->bump : DEFAULT_BUMP;
		const float hardness = m->hardness ? m->hardness : DEFAULT_HARDNESS;

		/* only world textures have materials assigned */
		if (image->type != it_world)
			continue;

		/* no material definition for this image */
		if (!m->bump && !m->parallax && !m->hardness && !m->specular && !m->num_stages)
			continue;

		Com_sprintf(buf, sizeof(buf), "{\n  material %s\n  bump %f\n  parallax %f\n  hardness %f\n  specular %f\n",
				image->name + strlen(textureDir), bump, parallax, hardness, specular);
		Q_strcat(buffer, buf, length);
		while (s) {
			Q_strcat(buffer, "  {\n", length);
			if (s->image) {
				Com_sprintf(buf, sizeof(buf), "    texture %s\n", s->image->name);
				Q_strcat(buffer, buf, length);
			}
			if (VectorLength(s->color)) {
				Com_sprintf(buf, sizeof(buf), "    color %f %f %f\n", s->color[0], s->color[1], s->color[2]);
				Q_strcat(buffer, buf, length);
			}
			if (s->blend.src || s->blend.dest) {
				Com_sprintf(buf, sizeof(buf), "    blend %s %s\n", R_ConstToName(s->blend.src), R_ConstToName(
						s->blend.dest));
				Q_strcat(buffer, buf, length);
			}
			if (s->pulse.dhz || s->pulse.hz) {
				Com_sprintf(buf, sizeof(buf), "    pulse %f %f\n", s->pulse.dhz, s->pulse.hz);
				Q_strcat(buffer, buf, length);
			}
			if (s->rotate.hz) {
				Com_sprintf(buf, sizeof(buf), "    rotate %f\n", s->rotate.hz);
				Q_strcat(buffer, buf, length);
			}
			if (s->scroll.s) {
				Com_sprintf(buf, sizeof(buf), "    scroll.s %f\n", s->scroll.s);
				Q_strcat(buffer, buf, length);
			}
			if (s->scroll.t) {
				Com_sprintf(buf, sizeof(buf), "    scroll.t %f\n", s->scroll.t);
				Q_strcat(buffer, buf, length);
			}
			if (s->scale.s) {
				Com_sprintf(buf, sizeof(buf), "    scale.s %f\n", s->scale.s);
				Q_strcat(buffer, buf, length);
			}
			if (s->anim.num_frames || s->anim.fps) {
				Com_sprintf(buf, sizeof(buf), "    anim %i %f\n", s->anim.num_frames, s->anim.fps);
				Q_strcat(buffer, buf, length);
			}
			if (s->scale.t) {
				Com_sprintf(buf, sizeof(buf), "    scale.t %f\n", s->scale.t);
				Q_strcat(buffer, buf, length);
			}
			if (s->stretch.amp || s->stretch.hz) {
				Com_sprintf(buf, sizeof(buf), "    stretch %f %f\n", s->stretch.amp, s->stretch.hz);
				Q_strcat(buffer, buf, length);
			}
			if (s->terrain.floor || s->terrain.ceil) {
				Com_sprintf(buf, sizeof(buf), "    terrain %f %f\n", s->terrain.floor, s->terrain.ceil);
				Q_strcat(buffer, buf, length);
			}
			if (s->flags & STAGE_LIGHTMAP)
				Q_strcat(buffer, "    lightmap\n", length);
			if (s->flags & STAGE_ENVMAP)
				Q_strcat(buffer, "    envmap 0\n", length);

			s = s->next;
			Q_strcat(buffer, "  }\n", length);
		}
		Q_strcat(buffer, "}\n", length);
	}
}

/**
 * @note Material editor callback
 */
static void R_MaterialListCallback (material_t *material, int stageIndex, const char *key, const char *value)
{
	char *buffer;
	const int size = 16384;

	buffer = Mem_PoolAlloc(size, vid_genericPool, 0);
	R_MaterialToBuffer(buffer, size);

	Com_Printf("%s", buffer);
	Mem_Free(buffer);
}

/**
 * @brief Writes the generated or modified material file to disc
 * @note Material editor callback
 */
static void R_MaterialSaveCallback (material_t *material, int stageIndex, const char *key, const char *value)
{
	char path[MAX_QPATH];
	char *buffer;
	const int size = 16384;

	buffer = Mem_PoolAlloc(size, vid_genericPool, 0);

	R_MaterialToBuffer(buffer, size);

	Com_sprintf(path, sizeof(path), "%s/materials/%s.mat", FS_Gamedir(), COM_SkipPath(refdef.mapName));
	if (!FS_WriteFile(buffer, strlen(buffer), path))
		Com_Printf("failed to write '%s'\n", path);
	else
		Com_Printf("wrote material file to '%s'\n", path);
	Mem_Free(buffer);
}

/**
 * @note Material editor callback
 */
static void R_MaterialDeleteMaterialCallback (material_t *material, int stageIndex, const char *key, const char *value)
{
	if (material) {
		int i;

		for (i = 0; i < r_numImages; i++) {
			const image_t *image = &r_images[i];

			/* only world textures have materials assigned */
			if (image->type != it_world)
				continue;

			if (memcmp(material, &image->material, sizeof(*material))) {
				memset(material, 0, sizeof(*material));
				return;
			}
		}
	} else {
		Com_Printf("No material found\n");
	}
}

/**
 * @note Material editor callback
 */
static void R_MaterialEditMaterialCallback (material_t *material, int stageIndex, const char *key, const char *value)
{
	if (!key) {
		Com_Printf("No key given\n");
		return;
	}

	if (!material) {
		Com_Printf("No material given\n");
		return;
	}

	if (!value) {
		Com_Printf("Expected a float parameter as value for '%s'\n", key);
		return;
	}

	if (!Q_strcmp(key, "bump"))
		material->bump = atof(value);
	else if (!Q_strcmp(key, "specular"))
		material->specular = atof(value);
	else if (!Q_strcmp(key, "parallax"))
		material->parallax = atof(value);
	else if (!Q_strcmp(key, "hardness"))
		material->hardness = atof(value);
	else
		Com_Printf("Unknown key: '%s'\n", key);
}

/**
 * @note Material editor callback
 */
static void R_MaterialEditStageCallback (material_t *material, int stageIndex, const char *key, const char *value)
{
	materialStage_t *stage;
	char buffer[MAX_VAR];
	const char *buf;

	if (!key) {
		Com_Printf("No key given\n");
		return;
	}

	if (!material) {
		Com_Printf("No material given\n");
		return;
	}

	if (stageIndex >= material->num_stages) {
		Com_Printf("Stage with index %i not found - create it first\n", stageIndex);
		return;
	}

	stage = material->stages;
	while (--stageIndex >= 0)
		stage = stage->next;

	/* keys that don't need a value given */
	if (!Q_strcmp(key, "lightmap")) {
		stage->flags |= STAGE_LIGHTMAP;
		return;
	}

	if (!value) {
		Com_Printf("No value for '%s' given\n", key);
		return;
	}

	Com_sprintf(buffer, sizeof(buffer), "{ %s %s }", key, value);
	buf = buffer;
	R_ParseStage(stage, &buf);

	/* load animation frame images */
	if (stage->flags & STAGE_ANIM)
		R_LoadAnimImages(stage);
}

/**
 * @note Material editor callback
 */
static void R_MaterialDeleteStageCallback (material_t *material, int stageIndex, const char *key, const char *value)
{
	if (!material) {
		Com_Printf("No material given\n");
		return;
	}

	if (stageIndex >= material->num_stages) {
		Com_Printf("Can't delete stage %i, the material only has %i stages\n", stageIndex, material->num_stages);
		return;
	}
}

/**
 * @note Material editor callback
 */
static void R_MaterialAddStageCallback (material_t *material, int stageIndex, const char *key, const char *value)
{
	int i;
	materialStage_t *s;
	image_t *image = NULL;

	if (!material) {
		Com_Printf("No material given\n");
		return;
	}

	for (i = 0; i < r_numImages; i++) {
		/* only world textures have materials assigned */
		if (r_images[i].type != it_world)
			continue;

		if (memcmp(material, &r_images[i].material, sizeof(*material))) {
			image = &r_images[i];
			break;
		}
	}


	s = (materialStage_t *)Mem_PoolAlloc(sizeof(*s), vid_imagePool, 0);
	s->image = image;

	/* append the stage to the chain */
	if (!material->stages)
		material->stages = s;
	else {
		materialStage_t *ss = material->stages;
		while (ss->next)
			ss = ss->next;
		ss->next = s;
	}

	material->flags |= s->flags;
	material->num_stages++;
}

#define MAX_EDIT_COMMANDS 16

typedef struct materialEditCommands_s {
	void (*callback) (material_t *material, int stageIndex, const char *key, const char *value);
	const char *description;
	const char *call;
	const char *name;
	const int expectedParameterCount;
	const char *parameters[MAX_EDIT_COMMANDS]; /** @todo also add the expected parameters somewhere */
} materialEditCommands_t;

/**
 * @brief Material editor callback list
 */
static const materialEditCommands_t materialEditCommands[] = {
	{R_MaterialListCallback, "Shows all stages of all materials", "", "list", 0, {NULL}},
	{R_MaterialSaveCallback, "Saves the currently loaded materials to file", "", "save", 0, {NULL}},
	{R_MaterialDeleteMaterialCallback, "Deletes specific stage", "<material>", "delmaterial", 1, {NULL}},
	{R_MaterialEditMaterialCallback, "Allows to edit a given material", "<material> <key> \"<value>\"", "editmaterial", 3,
			{"bump", "specular", "parallax", "hardness", NULL}},
	{R_MaterialEditStageCallback, "Allows to edit a specific stage", "<material> <key> \"<value>\" <stage>", "editstage", 4,
			{"terrain", "lightmap", "pulse", "envmap", "texture", "color", "blend", "stretch",
					"rotate", "scroll.t", "scroll.s", "scale.s", "scale.t", "anim", NULL}},
	/** @todo stage as second parameter isn't parsed yet */
	{R_MaterialDeleteStageCallback, "Deletes specific stage", "<material> <stage>", "delstage", 2, {NULL}},
	{R_MaterialAddStageCallback, "Adds a new stage to a given material", "<material>", "addstage", 1, {NULL}},

	{NULL, NULL, NULL}
};

static void R_MaterialEditPrintHelp (const materialEditCommands_t *materialEditCommand)
{
	if (materialEditCommand == NULL) {
		const materialEditCommands_t *commands = materialEditCommands;
		while (commands->name) {
			Com_Printf("  %12s: %s - %s\n", commands->name, commands->description, commands->call);
			commands++;
		}
	} else {
		Com_Printf("  %12s: %s - %s\n", materialEditCommand->name, materialEditCommand->description, materialEditCommand->call);
	}
}

static material_t *R_GetFromTexture (const char *textureName)
{
	int i;

	if (!textureName)
		return NULL;

	for (i = 0; i < r_numImages; i++) {
		image_t *image = &r_images[i];

		/* only world textures have materials assigned */
		if (image->type != it_world)
			continue;

		if (!Q_strcmp(image->name, textureName))
			return &image->material;
	}

	Com_Printf("No material for texture '%s' found\n", textureName);

	/* not found */
	return NULL;
}

/**
 * @brief Console callback for the material editor
 */
static void R_MaterialEdit_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <command>\n", Cmd_Argv(0));
		R_MaterialEditPrintHelp(NULL);
		return;
	}

	if (Cmd_Argc() >= 2) {
		const char *cmd = Cmd_Argv(1);
		const materialEditCommands_t *commands = materialEditCommands;
		while (commands->name) {
			if (!Q_strcmp(commands->name, cmd)) {
				const char *key = NULL;
				const char *value = NULL;
				int stage = 0;
				material_t *m = NULL;

				/* parameters expected? */
				if (*commands->parameters) {
					/* material edit command + sub command should not be taken into account here */
					if (Cmd_Argc() - 2 == commands->expectedParameterCount) {
						const char *texture = Cmd_Argv(2);
						char fileName[MAX_QPATH];

						Com_sprintf(fileName, sizeof(fileName), "%s%s", textureDir, texture);
						m = R_GetFromTexture(fileName);
						if (!m)
							return;

						/* key that should be modified */
						if (Cmd_Argc() >= 4)
							key = Cmd_Argv(3);
						else
							key = texture;

						/* value that should be set - must be in " if e.g. setting a vector */
						if (Cmd_Argc() >= 5)
							value = Cmd_Argv(4);

						/* stage index - if needed */
						if (Cmd_Argc() == 6)
							stage = atoi(Cmd_Argv(5));
					} else {
						Com_Printf("Invalid amount of parameters given - expected %i - got %i\n", commands->expectedParameterCount, Cmd_Argc() - 1);
						return;
					}
				} else {
					if (Cmd_Argc() >= 3) {
						const char *texture = Cmd_Argv(2);
						char fileName[MAX_QPATH];

						Com_sprintf(fileName, sizeof(fileName), "%s%s", textureDir, texture);
						m = R_GetFromTexture(fileName);
					}
				}
				commands->callback(m, stage, key, value);
				break;
			}
			commands++;
		}
	}
}

/**
 * @param[in] partial The stuff you already typed and want to complete
 * @param[out] match The target where the completed value (if completed at all) should go to
 */
static int R_MaterialEditCompleteCommand (const char *partial, const char **match)
{
	int i, matches = 0;
	const char *localMatch[MAX_COMPLETE];
	size_t len;
	const char *commands;

	if (!refdef.mapName)
		return 0;

	localMatch[matches] = NULL;

	len = strlen(partial);
	if (!len) {
		const materialEditCommands_t *commands = materialEditCommands;
		int found = 0;
		while (commands->name) {
			Com_Printf("%s\n", commands->name);
			commands++;
			found++;
		}
		return found;
	}

	commands = strstr(partial, " ");
	if (!commands) {
		const materialEditCommands_t *commands = materialEditCommands;
		while (commands->name) {
			if (!Q_strncmp(partial, commands->name, len)) {
				Com_Printf("%s - %s\n", commands->name, commands->call);
				localMatch[matches++] = commands->name;
			}
			commands++;
		}
	} else {
		/* skip that whitespace and store length of the partial material name */
		const char *material = ++commands;
		const size_t textureDirLength = strlen(textureDir);
		int found = 0;
		const char *action = strstr(material, " ");

		if (!action) {
			/* set new length for relative texture paths */
			len = strlen(material);

			for (i = 0; i < r_numImages; i++) {
				const image_t *image = &r_images[i];
				const char *relativeImageName = &image->name[textureDirLength];

				/* only world textures have materials assigned */
				if (image->type != it_world)
					continue;

#ifdef DEBUG
				if (!strstr(image->name, textureDir))
					Sys_Error("World texture that is not inside '%s' directory", textureDir);
#endif

				if (!len || !Q_strncmp(material, relativeImageName, len)) {
					Com_Printf("%s\n", relativeImageName);
					/* only add to auto completion if the name of the material
					 * already has been entered partially */
					if (len)
						localMatch[matches++] = &image->name[textureDirLength];
					else
						found++; /* no auto completion - just print to console */
				}
			}
			if (found)
				return found;
		} else {
			const materialEditCommands_t *commands = materialEditCommands;
			char buf[MAX_VAR];
			char *seperator;

			Q_strncpyz(buf, partial, sizeof(buf));
			seperator = strstr(buf, " ");
			assert(seperator);
			*seperator = '\0';

			while (commands->name) {
				if (!Q_strcmp(commands->name, buf)) {
					/** @todo auto complete these, too */
					const char* const *parameters = commands->parameters;
					while (*parameters) {
						Com_Printf("%s\n", *parameters);
						parameters++;
					}
					break;
				}
				commands++;
			}
		}
	}

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

/**
 * @brief Registers commands
 */
void R_MaterialEditInit (void)
{
	Cmd_AddCommand("materialedit", R_MaterialEdit_f, "Allow in place editing of the currently loaded material definitions");
	Cmd_AddParamCompleteFunction("materialedit", R_MaterialEditCompleteCommand);
}
