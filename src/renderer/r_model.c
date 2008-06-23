/**
 * @file r_model.c
 * @brief model loading and caching
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

#include "r_local.h"

static int modfilelen;

model_t r_models[MAX_MOD_KNOWN];
int r_numModels;

model_t *r_mapTiles[MAX_MAPTILES];
int r_numMapTiles;

/* the inline * models from the current map are kept seperate */
model_t r_modelsInline[MAX_MOD_KNOWN];
int r_numModelsInline;

int registration_sequence;

/**
 * @brief Prints all loaded models
 */
void R_ModModellist_f (void)
{
	int i;
	model_t *mod;

	Com_Printf("Loaded models:\n");
	Com_Printf("Type | #Tris  | Filename\n");
	for (i = 0, mod = r_models; i < r_numModels; i++, mod++) {
		if (!mod->name[0])
			continue;
		switch(mod->type) {
		case mod_alias_md3:
			Com_Printf("MD3 ");
			break;
		case mod_alias_md2:
			Com_Printf("MD2 ");
			break;
		case mod_alias_dpm:
			Com_Printf("DPM ");
			break;
		default:
			Com_Printf("%3i ", mod->type);
			break;
		}
		Com_Printf(" | %6i | %s\n", mod->alias.meshes[0].num_tris, mod->name);
	}
}


/**
 * @brief Loads in a model for the given name
 * @sa R_RegisterModel
 */
static model_t *R_ModForName (const char *name, qboolean crash)
{
	model_t *mod;
	unsigned *buf;
	int i;

	if (!name[0])
		Sys_Error("R_ModForName: NULL name");

	/* inline models are grabbed only from worldmodel */
	if (name[0] == '*') {
		i = atoi(name + 1) - 1;
		if (i < 0 || i >= r_numModelsInline)
			Sys_Error("bad inline model number '%s' (%i/%i)", name, i, r_numModelsInline);
		return &r_modelsInline[i];
	}

	/* search the currently loaded models */
	for (i = 0, mod = r_models; i < r_numModels; i++, mod++) {
		if (!mod->name[0])
			continue;
		if (!Q_strcmp(mod->name, name))
			return mod;
	}

	/* find a free model slot spot */
	for (i = 0, mod = r_models; i < r_numModels; i++, mod++) {
		if (!mod->name[0])
			break;				/* free spot */
	}

	if (i == r_numModels) {
		if (r_numModels == MAX_MOD_KNOWN)
			Sys_Error("r_numModels == MAX_MOD_KNOWN");
		r_numModels++;
	}

	memset(mod, 0, sizeof(*mod));
	Q_strncpyz(mod->name, name, sizeof(mod->name));
/*	Com_DPrintf(DEBUG_RENDERER, "name: %s\n", name); */

	/* load the file */
	modfilelen = FS_LoadFile(mod->name, (byte **) &buf);
	if (!buf) {
		if (crash)
			Sys_Error("R_ModForName: %s not found", mod->name);
		memset(mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	/* call the apropriate loader */
	switch (LittleLong(*(unsigned *) buf)) {
	case IDALIASHEADER:
		/* MD2 header */
		R_ModLoadAliasMD2Model(mod, buf, modfilelen);
		break;

	case DPMHEADER:
		R_ModLoadAliasDPMModel(mod, buf, modfilelen);
		break;

	case IDMD3HEADER:
		/* MD3 header */
		R_ModLoadAliasMD3Model(mod, buf, modfilelen);
		break;

	case IDBSPHEADER:
		Sys_Error("R_ModForName: don't load BSPs with this function");
		break;

	default:
		Sys_Error("R_ModForName: unknown fileid for %s", mod->name);
		break;
	}

	FS_FreeFile(buf);

	return mod;
}

/**
 * @brief Register model and skins
 * @sa R_RegisterModelShort
 * @sa R_ModForName
 */
static model_t *R_RegisterModel (const char *name)
{
	model_t *mod;
	int i;

	mod = R_ModForName(name, qfalse);
	if (mod) {
		/* register any images used by the models */
		switch (mod->type) {
		case mod_alias_dpm:
		case mod_alias_md2:
		case mod_alias_md3:
			break;
		case mod_brush:
			for (i = 0; i < mod->bsp.numtexinfo; i++)
				mod->bsp.texinfo[i].image->registration_sequence = registration_sequence;
			break;
		default:
			break;
		}
	}
	return mod;
}

/**
 * @brief all supported model formats
 * @sa modtype_t
 */
static const char *mod_extensions[] = {
	"md2", "md3", "dpm", NULL
};

/**
 * @brief Tries to load a model
 * @param[in] name The model path or name (with or without extension) - see notes
 * this parameter is always relativ to the game base dir - it can also be relative
 * to the models/ dir in the game folder
 * @note trying all supported model formats is only supported when you are using
 * a name without extension and relative to models/ dir
 * @note if first char of name is a '*' - this is an inline model
 * @note if there is not extension in the given filename the function will
 * try to load one of the supported model formats
 * @return NULL if no model could be found with the given name, model_t otherwise
 * @sa R_RegisterModel
 */
model_t *R_RegisterModelShort (const char *name)
{
	model_t *mod;
	int i = 0;

	if (!name || !name[0])
		return NULL;

	if (name[0] != '*' && (strlen(name) < 4 || name[strlen(name) - 4] != '.')) {
		char filename[MAX_QPATH];

		while (mod_extensions[i]) {
			Com_sprintf(filename, sizeof(filename), "models/%s.%s", name, mod_extensions[i]);
				mod = R_RegisterModel(filename);
			if (mod)
				return mod;
			i++;
		}
		Com_Printf("R_RegisterModelShort: Could not find any supported model with the basename: '%s'\n", name);
		return NULL;
	} else
		return R_RegisterModel(name);
}

/**
 * @sa R_ModBeginLoading
 */
void R_ModEndLoading (void)
{
	R_FreeUnusedImages();
}

static int r_numModelsStatic;
#define MEM_TAG_STATIC_MODELS 1
/**
 * @brief After all static models are loaded, switch the pool tag for these models
 * to not free them everytime R_ShutdownModels is called
 * @sa CL_InitAfter
 * @sa R_FreeUnusedImages
 */
void R_SwitchModelMemPoolTag (void)
{
	int i, j, k;
	model_t* mod;

	r_numModelsStatic = r_numModels;
	Mem_ChangeTag(vid_modelPool, 0, MEM_TAG_STATIC_MODELS);

	/* mark the static model textures as it_statis, thus R_FreeUnusedImages
	 * won't free them */
	for (i = 0, mod = r_models; i < r_numModelsStatic; i++, mod++) {
		for (j = 0; j < mod->alias.num_meshes; j++) {
			if (!mod->alias.meshes[j].num_skins)
				Com_Printf("Model '%s' has no skins\n", mod->name);
			for (k = 0; k < mod->alias.meshes[j].num_skins; k++) {
				if (mod->alias.meshes[j].skins[k].skin)
					mod->alias.meshes[j].skins[k].skin->type = it_static;
				else
					Com_Printf("No skin for #%i of '%s'\n", j, mod->name);
			}
		}
	}

	Com_Printf("%i static models loaded\n", r_numModels);
}

/**
 * @brief Frees the model pool (but only tag 0)
 * @sa R_SwitchModelMemPoolTag
 */
void R_ShutdownModels (void)
{
	int i;
	model_t *mod;

	/* free the vertex buffer - but not for the static models */
	for (i = r_numModelsStatic; i < r_numModels; i++) {
		mod = &r_models[i];

		if (mod->bsp.vertex_buffer)
			qglDeleteBuffers(1, &mod->bsp.vertex_buffer);
		if (mod->bsp.texcoord_buffer)
			qglDeleteBuffers(1, &mod->bsp.texcoord_buffer);
		if (mod->bsp.lmtexcoord_buffer)
			qglDeleteBuffers(1, &mod->bsp.lmtexcoord_buffer);
		if (mod->bsp.normal_buffer)
			qglDeleteBuffers(1, &mod->bsp.normal_buffer);
	}

	/* don't free the static models with the tag MEM_TAG_STATIC_MODELS */
	if (vid_modelPool)
		Mem_FreeTag(vid_modelPool, 0);
	if (vid_lightPool)
		Mem_FreeTag(vid_lightPool, 0);
	r_numModels = r_numModelsStatic;
}


image_t* R_AliasModelState (const model_t *mod, int *mesh, int *frame, int *oldFrame, int *skin)
{
	/* check animations */
	if ((*frame >= mod->alias.num_frames) || *frame < 0) {
		Com_Printf("R_AliasModelState %s: no such frame %d (# %i)\n", mod->name, *frame, mod->alias.num_frames);
		*frame = 0;
	}
	if ((*oldFrame >= mod->alias.num_frames) || *oldFrame < 0) {
		Com_Printf("R_AliasModelState %s: no such oldframe %d (# %i)\n", mod->name, *oldFrame, mod->alias.num_frames);
		*oldFrame = 0;
	}

	if (*mesh < 0 || *mesh >= mod->alias.num_meshes)
		*mesh = 0;

	if (!mod->alias.meshes)
		return NULL;

	/* use default skin - this is never null - but maybe the placeholder texture */
	if (*skin < 0 || *skin >= mod->alias.meshes[*mesh].num_skins)
		*skin = 0;

	return mod->alias.meshes[*mesh].skins[*skin].skin;
}

image_t* R_AliasModelGetSkin (const model_t* mod, const char *skin)
{
	char path[MAX_QPATH];
	char *slash, *end;

	if (skin[0] != '.')
		return R_FindImage(skin, it_skin);
	else {
		Q_strncpyz(path, mod->name, sizeof(path));
		end = path;
		while ((slash = strchr(end, '/')) != 0)
			end = slash + 1;
		strcpy(end, skin + 1);
		return R_FindImage(path, it_skin);
	}
}
