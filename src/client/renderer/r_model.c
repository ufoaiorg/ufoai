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

static model_t r_models[MAX_MOD_KNOWN];
static int r_numModels;
static int r_numModelsStatic;

model_t *r_mapTiles[MAX_MAPTILES];
int r_numMapTiles;

/* the inline * models from the current map are kept separate */
model_t r_modelsInline[MAX_MOD_KNOWN];
int r_numModelsInline;

static char *r_actorSkinNames[MAX_ACTORSKINNAME];
static int r_numActorSkinName;

/**
 * @brief all supported model formats
 * @sa modtype_t
 */
static char const* const mod_extensions[] = {
	"md2", "md3", "dpm", "obj", NULL
};

/**
 * @brief Prints all loaded models
 */
void R_ModModellist_f (void)
{
	int i;
	model_t *mod;

	Com_Printf("Loaded models:\n");
	Com_Printf("Type | #Slot | #Tris   | #Meshes | Filename\n");
	for (i = 0, mod = r_models; i < r_numModels; i++, mod++) {
		if (!mod->name[0]) {
			Com_Printf("Empty slot %i\n", i);
			continue;
		}
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
		case mod_bsp:
			Com_Printf("BSP ");
			break;
		case mod_bsp_submodel:
			Com_Printf("SUB ");
			break;
		case mod_obj:
			Com_Printf("OBJ ");
			break;
		default:
			Com_Printf("%3i ", mod->type);
			break;
		}
		if (mod->alias.num_meshes) {
			int j;
			Com_Printf(" | %5i | %7i | %7i | %s (skins: %i)\n", i, mod->alias.num_meshes, mod->alias.meshes[0].num_tris, mod->name, mod->alias.meshes[0].num_skins);
			for (j = 0; j < mod->alias.meshes[0].num_skins; j++) {
				mAliasSkin_t *skin = &mod->alias.meshes[0].skins[j];
				Com_Printf("     \\-- skin %i: '%s' (texnum %i and image type %i)\n", j + 1, skin->name, skin->skin->texnum, skin->skin->type);
			}
		} else
			Com_Printf(" | %5i | %7i | unknown | %s\n", i, mod->alias.num_meshes, mod->name);
	}
	Com_Printf("%4i models loaded\n", r_numModels);
	Com_Printf(" - %4i static models\n", r_numModelsStatic);
	Com_Printf(" - %4i bsp models\n", r_numMapTiles);
	Com_Printf(" - %4i inline models\n", r_numModelsInline);
}

model_t *R_AllocModelSlot (void)
{
	/* get new model */
	if (r_numModels < 0 || r_numModels >= MAX_MOD_KNOWN)
		Com_Error(ERR_DROP, "R_ModAddMapTile: r_numModels >= MAX_MOD_KNOWN");
	return &r_models[r_numModels++];
}

/**
 * @brief Loads in a model for the given name
 * @param[in] filename Filename relative to base dir and with extension (models/model.md2)
 * @param[inout] mod Structure to initialize
 * @return True if the loading was succeed. True or false structure mod was edited.
 */
static qboolean R_LoadModel (model_t *mod, const char *filename)
{
	byte *buf;
	int modfilelen;

	if (filename[0] == '\0')
		Com_Error(ERR_FATAL, "R_ModForName: NULL name");

	/* load the file */
	modfilelen = FS_LoadFile(filename, &buf);
	if (!buf)
		return qfalse;

	OBJZERO(*mod);
	Q_strncpyz(mod->name, filename, sizeof(mod->name));

	/* call the appropriate loader */
	switch (LittleLong(*(unsigned *) buf)) {
	case IDALIASHEADER:
		/* MD2 header */
		R_ModLoadAliasMD2Model(mod, buf, modfilelen, qtrue);
		break;

	case DPMHEADER:
		R_ModLoadAliasDPMModel(mod, buf, modfilelen);
		break;

	case IDMD3HEADER:
		/* MD3 header */
		R_ModLoadAliasMD3Model(mod, buf, modfilelen);
		break;

	case IDBSPHEADER:
		Com_Error(ERR_FATAL, "R_ModForName: don't load BSPs with this function");
		break;

	default:
	{
		const char* ext = Com_GetExtension(filename);
		if (ext != NULL && !Q_strcasecmp(ext, "obj"))
			R_LoadObjModel(mod, buf, modfilelen);
		else
			Com_Error(ERR_FATAL, "R_ModForName: unknown fileid for %s", mod->name);
	}
	}

	/* load the animations */
	Com_StripExtension(mod->name, mod->alias.animname, sizeof(mod->alias.animname));
	Com_DefaultExtension(mod->alias.animname, sizeof(mod->alias.animname), ".anm");

	/* try to load the animation file */
	if (FS_CheckFile("%s", mod->alias.animname) != -1) {
		/* load the tags */
		byte *animbuf = NULL;
		FS_LoadFile(mod->alias.animname, &animbuf);
		R_ModLoadAnims(&mod->alias, (const char *)animbuf);
		FS_FreeFile(animbuf);
	}

	FS_FreeFile(buf);

	return qtrue;
}

/**
 * @brief Tries to load a model
 * @param[in] name The model path or name (with or without extension) - see notes
 * this parameter is always relative to the game base dir - it can also be relative
 * to the models/ dir in the game folder
 * @note trying all supported model formats is only supported when you are using
 * a name without extension and relative to models/ dir
 * @note if first char of name is a '*' - this is an inline model
 * @note if there is not extension in the given filename the function will
 * try to load one of the supported model formats
 * @return NULL if no model could be found with the given name, model_t otherwise
 */
model_t *R_FindModel (const char *name)
{
	model_t *mod;
	model_t model;
	qboolean loaded = qfalse;
	int i;

	if (!name || !name[0])
		return NULL;

	/* search for existing models */
	mod = R_GetModel(name);
	if (mod != NULL)
		return mod;

	/* load model */
	if (name[0] != '*' && (strlen(name) < 4 || name[strlen(name) - 4] != '.')) {
		char filename[MAX_QPATH];

		for (i = 0; mod_extensions[i] != NULL; i++) {
			Com_sprintf(filename, sizeof(filename), "models/%s.%s", name, mod_extensions[i]);
			loaded = R_LoadModel(&model, filename);
			if (loaded) {
				/* use short name */
				Q_strncpyz(model.name, name, sizeof(model.name));
				break;
			}
		}
	} else {
		/** @todo this case should be useless, do we ever use extension? */
		loaded = R_LoadModel(&model, name);
	}

	if (!loaded) {
		Com_Printf("R_FindModel: Could not find: '%s'\n", name);
		return NULL;
	}

	/* register the new model only after the loading is finished */

	/* find a free model slot spot */
	for (i = 0, mod = r_models; i < r_numModels; i++, mod++) {
		if (!mod->name[0])
			break;
	}
	if (i == r_numModels) {
		if (r_numModels == MAX_MOD_KNOWN)
			Com_Error(ERR_FATAL, "r_numModels == MAX_MOD_KNOWN");
		r_numModels++;
	}

	/* copy the model to the slot */
	r_models[i] = model;
	return &r_models[i];
}

/**
 * @brief Get a model for the given name already loaded.
 * @return A model for the given name, else NULL.
 * @sa R_FindModel
 * @param[in] name Short name of the model relative to base dir without (models/model)
 */
model_t *R_GetModel (const char *name)
{
	model_t *mod;
	int i;

	if (name[0] == '\0')
		Com_Error(ERR_FATAL, "R_ModForName: NULL name");

	/* inline models are grabbed only from worldmodel */
	if (name[0] == '*') {
		i = atoi(name + 1) - 1;
		if (i < 0 || i >= r_numModelsInline)
			Com_Error(ERR_FATAL, "bad inline model number '%s' (%i/%i)", name, i, r_numModelsInline);
		return &r_modelsInline[i];
	}

	/* search the currently loaded models */
	for (i = 0, mod = r_models; i < r_numModels; i++, mod++)
		if (Q_streq(mod->name, name))
			return mod;
	return NULL;
}

#define MEM_TAG_STATIC_MODELS 1
/**
 * @brief After all static models are loaded, switch the pool tag for these models
 * to not free them everytime R_ShutdownModels is called
 * @sa CL_InitAfter
 * @sa R_FreeWorldImages
 */
void R_SwitchModelMemPoolTag (void)
{
	int i, j, k;
	model_t* mod;

	r_numModelsStatic = r_numModels;
	Mem_ChangeTag(vid_modelPool, 0, MEM_TAG_STATIC_MODELS);

	/* mark the static model textures as it_static, thus R_FreeWorldImages
	 * won't free them */
	for (i = 0, mod = r_models; i < r_numModelsStatic; i++, mod++) {
		if (!mod->alias.num_meshes)
			Com_Printf("Model '%s' has no meshes\n", mod->name);
		for (j = 0; j < mod->alias.num_meshes; j++) {
			mAliasMesh_t *mesh = &mod->alias.meshes[j];
			if (!mesh->num_skins)
				Com_Printf("Model '%s' has no skins\n", mod->name);
			for (k = 0; k < mesh->num_skins; k++) {
				mAliasSkin_t *modelSkin = &mesh->skins[k];
				if (modelSkin->skin != r_noTexture)
					modelSkin->skin->type = it_static;
				else
					Com_Printf("No skin for #%i of '%s'\n", j, mod->name);
			}
		}
	}

	Com_Printf("%i static models loaded\n", r_numModels);
}

/**
 * @brief Register an actorskin name
 * @return The id where the actorskin is registered
 */
int R_ModAllocateActorSkin (const char* name)
{
	if (r_numActorSkinName >= lengthof(r_actorSkinNames))
		return -1;

	r_actorSkinNames[r_numActorSkinName] = Mem_StrDup(name);

	return r_numActorSkinName++;
}

qboolean R_UseActorSkin (void)
{
	return r_numActorSkinName != 0;
}

static const char* R_GetActorSkin (int id)
{
	assert(id >= 0 && id < r_numActorSkinName);
	return r_actorSkinNames[id];
}

/**
 * @brief Load actor skins from a default skin to a a mesh.
 * @param outMesh Mesh target of skins
 * @param defaultSkin Default skin of the mesh
 */
void R_LoadActorSkinsFromModel (mAliasMesh_t *outMesh, image_t * defaultSkin)
{
	int i;
	assert(outMesh);

	outMesh->num_skins = r_numActorSkinName;
	outMesh->skins = (mAliasSkin_t *)Mem_PoolAlloc(sizeof(mAliasSkin_t) * outMesh->num_skins, vid_modelPool, 0);

	if (defaultSkin == r_noTexture)
		Com_Printf("R_LoadActorSkinsFromModel: No default skin found for model \"%s\"\n", outMesh->name);

	for (i = 0; i < outMesh->num_skins; i++) {
		mAliasSkin_t *modelSkin = &outMesh->skins[i];
		if (i == 0) {
			modelSkin->skin = defaultSkin;
		} else {
			const char *skin = R_GetActorSkin(i);
			modelSkin->skin = R_AliasModelGetSkin(NULL, va("%s_%s", defaultSkin->name, skin));
			/** @todo should we add warning here? */
			if (modelSkin->skin == r_noTexture)
				modelSkin->skin = defaultSkin;
		}
		Q_strncpyz(modelSkin->name, modelSkin->skin->name, sizeof(outMesh->skins[i].name));
	}
}

/**
 * @brief Frees the model pool
 * @param complete If this is true the static mesh models are freed, too
 * @sa R_SwitchModelMemPoolTag
 */
void R_ShutdownModels (qboolean complete)
{
	int i;
	const int start = complete ? 0 : r_numModelsStatic;

	/* free the vertex buffer - but not for the static models
	 * the world, the submodels and all the misc_models are located in the
	 * r_models array */
	for (i = start; i < r_numModels; i++) {
		model_t *mod = &r_models[i];
		mBspModel_t *bsp = &mod->bsp;

		if (bsp->vertex_buffer)
			qglDeleteBuffers(1, &bsp->vertex_buffer);
		if (bsp->texcoord_buffer)
			qglDeleteBuffers(1, &bsp->texcoord_buffer);
		if (bsp->lmtexcoord_buffer)
			qglDeleteBuffers(1, &bsp->lmtexcoord_buffer);
		if (bsp->normal_buffer)
			qglDeleteBuffers(1, &bsp->normal_buffer);
		if (bsp->tangent_buffer)
			qglDeleteBuffers(1, &bsp->tangent_buffer);
	}

	/* don't free the static models with the tag MEM_TAG_STATIC_MODELS */
	if (complete) {
		if (vid_modelPool)
			Mem_FreePool(vid_modelPool);
		if (vid_lightPool)
			Mem_FreePool(vid_lightPool);
		r_numModels = 0;
		r_numModelsInline = 0;
		r_numMapTiles = 0;
		OBJZERO(r_models);
	} else {
		if (vid_modelPool)
			Mem_FreeTag(vid_modelPool, 0);
		if (vid_lightPool)
			Mem_FreeTag(vid_lightPool, 0);
		r_numModels = r_numModelsStatic;
	}
}
