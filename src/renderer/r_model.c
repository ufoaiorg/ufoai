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

model_t mod_known[MAX_MOD_KNOWN];
int mod_numknown;

model_t *rTiles[MAX_MAPTILES];
int rNumTiles;

/* the inline * models from the current map are kept seperate */
model_t mod_inline[MAX_MOD_KNOWN];
int numInline;

int registration_sequence;

/**
 * @brief Prints all loaded models
 */
void R_ModModellist_f (void)
{
	int i;
	model_t *mod;

	Com_Printf("Loaded models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		if (!mod->name[0])
			continue;
		switch(mod->type) {
		case mod_alias_md3:
			Com_Printf("MD3");
			break;
		case mod_alias_md2:
			Com_Printf("MD2");
			break;
		default:
			Com_Printf("%3i", mod->type);
			break;
		}
		Com_Printf(" %s\n", mod->name);
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
		if (i < 0 || i >= numInline)
			Sys_Error("bad inline model number '%s' (%i/%i)", name, i, numInline);
		return &mod_inline[i];
	}

	/* search the currently loaded models */
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		if (!mod->name[0])
			continue;
		if (!Q_strcmp(mod->name, name))
			return mod;
	}

	/* find a free model slot spot */
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		if (!mod->name[0])
			break;				/* free spot */
	}

	if (i == mod_numknown) {
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error("mod_numknown == MAX_MOD_KNOWN");
		mod_numknown++;
	}

	memset(mod, 0, sizeof(model_t));
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

	loadmodel = mod;

	/* call the apropriate loader */
	switch (LittleLong(*(unsigned *) buf)) {
	case IDALIASHEADER:
		/* MD2 header */
		R_ModLoadAliasMD2Model(mod, buf, modfilelen);
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
	mdl_md2_t *pheader;
	mAliasModel_t *pheader3;

	mod = R_ModForName(name, qfalse);
	if (mod) {
		/* register any images used by the models */
		switch (mod->type) {
		case mod_alias_md2:
			{
			char path[MAX_QPATH];
			char *skin, *slash, *end;

			pheader = (mdl_md2_t *) mod->alias.extraData;
			for (i = 0; i < pheader->num_skins; i++) {
				skin = (char *) pheader + pheader->ofs_skins + i * MD2_MAX_SKINNAME;
				if (skin[0] != '.')
					mod->alias.skins_img[i] = R_FindImage(skin, it_skin);
				else {
					Q_strncpyz(path, mod->name, sizeof(path));
					end = path;
					while ((slash = strchr(end, '/')) != 0)
						end = slash + 1;
					strcpy(end, skin + 1);
					mod->alias.skins_img[i] = R_FindImage(path, it_skin);
				}
			}
			mod->numframes = pheader->num_frames;
			break;
			}
		case mod_alias_md3:
			{
			pheader3 = (mAliasModel_t *)mod->alias.extraData;
			mod->numframes = pheader3->num_frames;
			break;
			}
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
	"md2", "md3", NULL
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

/**
 * @sa R_DrawAliasMD2Model
 * @sa R_DrawAliasMD3Model
 * @param[in] lightambient May not be null for fixed lightning
 */
void R_ModEnableLights (const entity_t* e)
{
	if (e->flags & RF_LIGHTFIXED) {
		/* add the fixed light */
		qglLightfv(GL_LIGHT0, GL_POSITION, e->lightparam);
		qglLightfv(GL_LIGHT0, GL_DIFFUSE, e->lightcolor);
		qglLightfv(GL_LIGHT0, GL_AMBIENT, e->lightambient);
	} else {
		float sumBright, max;
		vec4_t sumColor, trorigin, sumDelta;

		VectorClear(sumColor);
		VectorClear(sumDelta);
		sumDelta[3] = 1.0;
		GLVectorTransform(trafo[e - refdef.entities].matrix, sumDelta, trorigin);

		if (*e->lightparam != 0.0) {
			VectorScale(refdef.sun->dir, 1.0, sumDelta);
			sumBright = *e->lightparam;
			VectorScale(refdef.sun->color, sumBright, sumColor);
		} else {
			VectorClear(sumDelta);
			sumBright = 0;
		}

		/* normalize delta and color */
		VectorNormalize(sumDelta);
		VectorMA(trorigin, 512, sumDelta, sumDelta);
		sumDelta[3] = 0.0;
		if (sumBright > 0.)
			VectorScale(sumColor, sumBright, sumColor);
		sumColor[3] = 1.0;

		max = sumColor[0];
		if (sumColor[1] > max)
			max = sumColor[1];
		if (sumColor[2] > max)
			max = sumColor[2];
		if (max > 2.0)
			VectorScale(sumColor, 2.0 / max, sumColor);

		/* add the light */
		qglLightfv(GL_LIGHT0, GL_POSITION, sumDelta);
		qglLightfv(GL_LIGHT0, GL_DIFFUSE, sumColor);
		qglLightfv(GL_LIGHT0, GL_AMBIENT, refdef.sun->ambient);
	}

	/* enable the lighting */
	RSTATE_ENABLE_LIGHTING
}

/**
 * @brief Draws the model bounding box
 * @sa R_DrawAliasMD2Model
 */
void R_ModDrawModelBBox (vec4_t bbox[8], entity_t *e)
{
	if (!r_showbox->integer)
		return;

	qglDisable(GL_CULL_FACE);
	qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	/* Draw top and sides */
	qglBegin(GL_TRIANGLE_STRIP);
	qglVertex3fv(bbox[2] );
	qglVertex3fv(bbox[1]);
	qglVertex3fv(bbox[0]);
	qglVertex3fv(bbox[1]);
	qglVertex3fv(bbox[4]);
	qglVertex3fv(bbox[5]);
	qglVertex3fv(bbox[1]);
	qglVertex3fv(bbox[7]);
	qglVertex3fv(bbox[3]);
	qglVertex3fv(bbox[2]);
	qglVertex3fv(bbox[7]);
	qglVertex3fv(bbox[6]);
	qglVertex3fv(bbox[2]);
	qglVertex3fv(bbox[4]);
	qglVertex3fv(bbox[0]);
	qglEnd();

  	/* Draw bottom */
	qglBegin(GL_TRIANGLE_STRIP);
	qglVertex3fv(bbox[4]);
	qglVertex3fv(bbox[6]);
	qglVertex3fv(bbox[7]);
	qglEnd();

	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	qglEnable(GL_CULL_FACE);
}


/**
 * @brief Fallback if entity doesn't have any valid model
 */
void R_ModDrawNullModel (entity_t* e)
{
	int i;

	qglPushMatrix();

	qglMultMatrixf(trafo[e - refdef.entities].matrix);

	qglDisable(GL_TEXTURE_2D);

	qglBegin(GL_TRIANGLE_FAN);
	qglVertex3f(0, 0, -16);
	for (i = 0; i <= 4; i++) {
		qglColor3f(0.2 + 0.6 * (i % 2), 0.0, 0.2 + 0.6 * (i % 2));
		qglVertex3f(16 * cos(i * M_PI / 2), 16 * sin(i * M_PI / 2), 0);
	}
	qglEnd();

	qglBegin(GL_TRIANGLE_FAN);
	qglVertex3f(0, 0, 16);
	for (i = 4; i >= 0; i--) {
		qglColor3f(0.2 + 0.6 * (i % 2), 0.0, 0.2 + 0.6 * (i % 2));
		qglVertex3f(16 * cos(i * M_PI / 2), 16 * sin(i * M_PI / 2), 0);
	}
	qglEnd();

	R_Color(NULL);
	qglPopMatrix();
	qglEnable(GL_TEXTURE_2D);
}

static int static_mod_numknown;
#define MEM_TAG_STATIC_MODELS 1
/**
 * @brief After all static models are loaded, switch the pool tag for these models
 * to not free them everytime R_ShutdownModels is called
 * @sa CL_InitAfter
 * @sa R_FreeUnusedImages
 */
void R_SwitchModelMemPoolTag (void)
{
	int i, j;
	model_t* mod;

	static_mod_numknown = mod_numknown;
	Mem_ChangeTag(vid_modelPool, 0, MEM_TAG_STATIC_MODELS);

	/* mark the static model textures as it_statis, thus R_FreeUnusedImages
	 * won't free them */
	for (i = 0, mod = mod_known; i < static_mod_numknown; i++, mod++) {
		if (!mod->alias.num_skins)
			Com_Printf("Model '%s' has no skins\n", mod->name);
		for (j = 0; j < mod->alias.num_skins; j++) {
			if (mod->alias.skins_img[j])
				mod->alias.skins_img[j]->type = it_static;
			else
				Com_Printf("No skin for #%i of '%s'\n", j, mod->name);
		}
	}

	Com_Printf("%i static models loaded\n", mod_numknown);
}

/**
 * @brief Frees the model pool (but only tag 0)
 * @sa R_SwitchModelMemPoolTag
 */
void R_ShutdownModels (void)
{
	/* don't free the static models with the tag MEM_TAG_STATIC_MODELS */
	if (vid_modelPool)
		VID_FreeTags(vid_modelPool, 0);
	if (vid_lightPool)
		VID_FreeTags(vid_lightPool, 0);
	mod_numknown = static_mod_numknown;
}
