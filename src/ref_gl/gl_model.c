/**
 * @file gl_model.c
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

#include "gl_local.h"

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
void Mod_Modellist_f (void)
{
	int i;
	model_t *mod;

	ri.Con_Printf(PRINT_ALL, "Loaded models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		if (!mod->name[0])
			continue;
		switch(mod->type) {
		case mod_alias_md3:
			ri.Con_Printf(PRINT_ALL, "MD3");
			break;
		case mod_alias_md2:
			ri.Con_Printf(PRINT_ALL, "MD2");
			break;
		case mod_sprite:
			ri.Con_Printf(PRINT_ALL, "SP2");
			break;
		case mod_obj:
			ri.Con_Printf(PRINT_ALL, "OBJ");
			break;
		default:
			ri.Con_Printf(PRINT_ALL, "%3i", mod->type);
			break;
		}
		ri.Con_Printf(PRINT_ALL, " %s\n", mod->name);
	}
}


/**
 * @brief Loads in a model for the given name
 * @sa R_RegisterModel
 */
static model_t *Mod_ForName (const char *name, qboolean crash)
{
	model_t *mod;
	unsigned *buf;
	int i;

	if (!name[0])
		ri.Sys_Error(ERR_DROP, "Mod_ForName: NULL name");

	/* inline models are grabbed only from worldmodel */
	if (name[0] == '*') {
		i = atoi(name + 1) - 1;
		if (i < 0 || i >= numInline)
			ri.Sys_Error(ERR_DROP, "bad inline model number '%s' (%i/%i)", name, i, numInline);
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
			ri.Sys_Error(ERR_DROP, "mod_numknown == MAX_MOD_KNOWN");
		mod_numknown++;
	}

	memset(mod, 0, sizeof(model_t));
	Q_strncpyz(mod->name, name, sizeof(mod->name));
/*	Com_Printf("name: %s\n", name); */

	/* load the file */
	modfilelen = ri.FS_LoadFile(mod->name, (void **) &buf);
	if (!buf) {
		if (crash)
			ri.Sys_Error(ERR_DROP, "Mod_ForName: %s not found", mod->name);
		memset(mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	loadmodel = mod;

	/* register */
	mod->registration_sequence = registration_sequence;

	/* call the apropriate loader */
	switch (LittleLong(*(unsigned *) buf)) {
	case IDALIASHEADER:
		/* MD2 header */
		Mod_LoadAliasMD2Model(mod, buf, modfilelen);
		break;

	case IDMD3HEADER:
		/* MD3 header */
		Mod_LoadAliasMD3Model(mod, buf, modfilelen);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel(mod, buf, modfilelen);
		break;

	case IDBSPHEADER:
		ri.Sys_Error(ERR_DROP, "Mod_ForName: don't load BSPs with this function");
		break;

	default:
		if (!Mod_LoadOBJModel(mod, buf, modfilelen))
			ri.Sys_Error(ERR_DROP, "Mod_ForName: unknown fileid for %s", mod->name);
		break;
	}

	ri.FS_FreeFile(buf);

	return mod;
}

/**
 * @brief Register model and skins
 * @sa R_RegisterModelShort
 * @sa Mod_ForName
 */
static model_t *R_RegisterModel (const char *name)
{
	model_t *mod;
	int i;
	dsprite_t *sprout;
	mdl_md2_t *pheader;
	mAliasModel_t *pheader3;

	mod = Mod_ForName(name, qfalse);
	if (mod) {
		/* register any images used by the models */
		switch (mod->type) {
		case mod_sprite:
			sprout = (dsprite_t *) mod->alias.extraData;
			for (i = 0; i < sprout->numframes; i++)
				mod->alias.skins_img[i] = GL_FindImage(sprout->frames[i].name, it_sprite);
			break;
		case mod_alias_md2:
			{
			char path[MAX_QPATH];
			char *skin, *slash, *end;

			pheader = (mdl_md2_t *) mod->alias.extraData;
			for (i = 0; i < pheader->num_skins; i++) {
				skin = (char *) pheader + pheader->ofs_skins + i * MD2_MAX_SKINNAME;
				if (skin[0] != '.')
					mod->alias.skins_img[i] = GL_FindImage(skin, it_skin);
				else {
					Q_strncpyz(path, mod->name, sizeof(path));
					end = path;
					while ((slash = strchr(end, '/')) != 0)
						end = slash + 1;
					strcpy(end, skin + 1);
					mod->alias.skins_img[i] = GL_FindImage(path, it_skin);
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
		case mod_obj:
			/* @todo: */
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
	"md2", "md3", "obj", NULL
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
 * @brief
 * @sa Mod_FreeAll
 */
static void Mod_Free (model_t * mod)
{
	if (mod->alias.extraData)
		ri.TagFree(mod->alias.extraData);

	memset(mod, 0, sizeof(*mod));
}

/**
 * @brief
 * @sa GL_BeginLoading
 */
void GL_EndLoading (void)
{
	int i;
	model_t *mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		if (!mod->name[0])
			continue;
		/* don't need this model */
		if (mod->registration_sequence != registration_sequence)
			Mod_Free(mod);
	}

	GL_FreeUnusedImages();
}

/**
 * @brief
 * @sa Mod_Free
 */
void Mod_FreeAll (void)
{
	GL_ShutdownModels();
	mod_numknown = 0;
}

/**
 * @brief Draws the model bounding box
 * @sa R_DrawAliasMD2Model
 */
void Mod_DrawModelBBox (vec4_t bbox[8], entity_t *e)
{
	if (!gl_showbox->integer)
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
void Mod_DrawNullModel (void)
{
	vec3_t shadelight;
	int i;

	R_LightPoint(currententity->origin, shadelight);

	qglPushMatrix();

	qglMultMatrixf(trafo[currententity - r_newrefdef.entities].matrix);

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

	qglColor3f(1, 1, 1);
	qglPopMatrix();
	qglEnable(GL_TEXTURE_2D);
}

/**
 * @brief Frees the model pool
 */
void GL_ShutdownModels (void)
{
	ri.FreeTags(ri.modelPool, 0);
	ri.FreeTags(ri.lightPool, 0);
}
