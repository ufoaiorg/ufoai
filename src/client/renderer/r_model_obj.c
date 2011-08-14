/**
 * @file r_model_obj.c
 * @brief obj model loading
 */

/*
Copyright (C) 1997-2009 Quake2World

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

/* object model memory representation */
typedef struct mobjvert_s {
	int vert;
	int normal;
	int texcoord;
} mobjvert_t;

typedef struct mobjtri_s {
	mobjvert_t verts[3];
} mobjtri_t;

typedef struct mobj_s {
	int num_verts;
	int num_verts_parsed;
	float *verts;

	int num_normals;
	int num_normals_parsed;
	float *normals;

	int num_texcoords;
	int num_texcoords_parsed;
	float *texcoords;

	int num_tris;
	int num_tris_parsed;
	mobjtri_t *tris;
} mobj_t;

#include "r_local.h"
#include "../../shared/parse.h"

static void R_LoadObjModelVertexArrays (mobj_t *obj, model_t *mod)
{
	const mobjtri_t *t;
	int i, j, vertind, coordind;
	mAliasMesh_t *mesh = mod->alias.meshes;
	const int v = obj->num_tris * 3 * 3;
	const int st = obj->num_tris * 3 * 2;

	vertind = coordind = 0;

	t = obj->tris;

	mesh->num_tris = obj->num_tris;
	mesh->num_verts = obj->num_verts;

	mesh->verts = (float *)Mem_PoolAlloc(sizeof(float) * v, vid_modelPool, 0);
	mesh->normals = (float *)Mem_PoolAlloc(sizeof(float) * v, vid_modelPool, 0);
	mesh->texcoords = (float *)Mem_PoolAlloc(sizeof(float) * st, vid_modelPool, 0);

	/* fill the arrays */
	for (i = 0; i < obj->num_tris; i++, t++) {
		const mobjvert_t *v = t->verts;

		/* each vert */
		for (j = 0; j < 3; j++, v++) {
			assert(v->vert - 1 >= 0);
			VectorCopy((&obj->verts[(v->vert - 1) * 3]), (&mesh->verts[vertind + j * 3]));

			if (v->normal) {
				assert(v->normal - 1 >= 0);
				VectorCopy((&obj->normals[(v->normal - 1) * 3]), (&mesh->normals[vertind + j * 3]));
			}

			if (v->texcoord) {
				assert(v->texcoord - 1 >= 0);
				memcpy(&mesh->texcoords[coordind + j * 2], &obj->texcoords[(v->texcoord - 1) * 2], sizeof(vec2_t));
			}
		}

		coordind += 6;
		vertind += 9;
	}
}

#define MAX_OBJ_FACE_VERTS 128

/**
 * @brief Assembles count tris on the model from the specified array of verts.
 */
static void R_LoadObjModelTris (mobj_t *obj, const mobjvert_t *verts, int count)
{
	int i;

	if (!obj->tris)
		return;

	assert(count < MAX_OBJ_FACE_VERTS);

	for (i = 0; i < count; i++) {  /* walk around the polygon */
		const int v0 = 0;
		const int v1 = 1 + i;
		const int v2 = 2 + i;

		mobjtri_t *t = &obj->tris[obj->num_tris_parsed + i];
		assert(obj->num_tris_parsed + i < obj->num_tris);

		t->verts[0] = verts[v0];
		t->verts[1] = verts[v1];
		t->verts[2] = verts[v2];
	}
}

/**
 * @brief Each line consists of 3 or more vertex definitions, e.g.
 *
 * @code 57/13/31 58/14/32 59/15/33 21/15/19 @endcode
 *
 * Tokenize the line with @c Com_Parse, and parse each vertex definition.
 * Faces with more than 3 vertices must be broken down into triangles.
 *
 * @return the number of triangles produced for the specified line.
 */
static int R_LoadObjModelFace (const model_t *mod, mobj_t *obj, const char *line)
{
	mobjvert_t *v, verts[MAX_OBJ_FACE_VERTS];
	const char *d;
	char *e;
	char tok[32];
	int i, tris;

	OBJZERO(verts);
	i = 0;

	while (qtrue) {
		const char *c = Com_Parse(&line);

		if (c[0] == '\0')  /* done */
			break;

		if (i == MAX_OBJ_FACE_VERTS)
			Com_Error(ERR_DROP, "R_LoadObjModelFace: too many vertexes: %s.", mod->name);

		/* simply count verts */
		if (!obj->tris) {
			i++;
			continue;
		}

		d = c;
		v = &verts[i++];

		OBJZERO(tok);
		e = tok;

		/* parse the vertex definition */
		while (d[0] != '\0') {
			/* index delimiter, parse the token */
			if (d[0] == '/') {
				if (!v->vert)
					v->vert = atoi(tok);
				else if (!v->texcoord)
					v->texcoord = atoi(tok);
				else if (!v->normal)
					v->normal = atoi(tok);

				OBJZERO(tok);
				e = tok;

				d++;
				continue;
			}

			*e++ = *d++;
		}

		/* parse whatever is left in the token */
		if (!v->vert)
			v->vert = atoi(tok);
		else if (!v->texcoord)
			v->texcoord = atoi(tok);
		else if (!v->normal)
			v->normal = atoi(tok);

		if (v->vert < 0 || v->texcoord < 0 || v->normal < 0)
			Com_Error(ERR_DROP, "R_LoadObjModelFace: bad indices: %s (%i:%i:%i).",
				mod->name, v->vert, v->texcoord, v->normal);
	}

	/* number of triangles from parsed verts */
	tris = i - 2;

	if (tris < 1)
		Com_Error(ERR_DROP, "R_LoadObjModelFace: too few vertexes: %s.", mod->name);

	/* break verts up into tris */
	R_LoadObjModelTris(obj, verts, tris);

	return tris;
}


/**
 * @brief Parse the object file line. If the structures have been allocated,
 * populate them. Otherwise simply accumulate counts.
 */
static void R_LoadObjModelLine (model_t *mod, mobj_t *obj, char *line)
{
	if (Q_strnull(line))  /* don't bother */
		return;

	if (!strncmp(line, "v ", 2)) {  /* vertex */
		if (obj->verts) {  /* parse it */
			float *f = obj->verts + obj->num_verts_parsed * 3;

			if (sscanf(line + 2, "%f %f %f", &f[0], &f[2], &f[1]) != 3)
				Com_Error(ERR_DROP, "R_LoadObjModelLine: Malformed vertex for %s: %s.",
						mod->name, line);

			obj->num_verts_parsed++;
		} else  /* or just count it */
			obj->num_verts++;
	} else if (!strncmp(line, "vn ", 3)) {  /* normal */
		if (obj->normals) {  /* parse it */
			float *f = obj->normals + obj->num_normals_parsed * 3;

			if (sscanf(line + 3, "%f %f %f", &f[0], &f[1], &f[2]) != 3)
				Com_Error(ERR_DROP, "R_LoadObjModelLine: Malformed normal for %s: %s.",
						mod->name, line);

			obj->num_normals_parsed++;
		} else  /* or just count it */
			obj->num_normals++;
	} else if (!strncmp(line, "vt ", 3)) {  /* texcoord */
		if (obj->texcoords) {  /* parse it */
			float *f = obj->texcoords + obj->num_texcoords_parsed * 2;

			if (sscanf(line + 3, "%f %f", &f[0], &f[1]) != 2)
				Com_Error(ERR_DROP, "R_LoadObjModelLine: Malformed texcoord for %s: %s.",
						mod->name, line);

			f[1] = -f[1];
			obj->num_texcoords_parsed++;
		} else  /* or just count it */
			obj->num_texcoords++;
	} else if (!strncmp(line, "f ", 2)) {  /* face */
		if (obj->tris)  /* parse it */
			obj->num_tris_parsed += R_LoadObjModelFace(mod, obj, line + 2);
		else  /* or just count it */
			obj->num_tris += R_LoadObjModelFace(mod, obj, line + 2);
	} else {
		Com_DPrintf(DEBUG_RENDERER, "R_LoadObjModelLine: Unsupported line for %s: %s.\n",
				mod->name, line);
	}
}

static void R_LoadObjSkin (model_t *mod)
{
	char skinPath[MAX_QPATH];
	mAliasMesh_t *mesh = &mod->alias.meshes[0];

	Com_StripExtension(mod->name, skinPath, sizeof(skinPath));
	if (FS_CheckFile("%s.mtl", skinPath) != -1) {
		const char *buffer;
		byte *buf;
		int i;

		FS_LoadFile(va("%s.mtl", skinPath), &buf);

		buffer = (const char *)buf;
		for (;;) {
			const char *token = Com_Parse(&buffer);
			if (token[0] == '\0')
				break;

			if (Q_streq(token, "map_Kd")) {
				mesh->num_skins++;
			}
		}
		mesh->skins = (mAliasSkin_t *)Mem_PoolAlloc(sizeof(mAliasSkin_t) * mesh->num_skins, vid_modelPool, 0);

		buffer = (const char *)buf;
		i = 0;
		for (;;) {
			const char *token = Com_Parse(&buffer);
			if (token[0] == '\0')
				break;

			if (Q_streq(token, "map_Kd")) {
				const char *skin = Com_Parse(&buffer);
				mAliasSkin_t *aliasSkin = &mesh->skins[i++];

				Com_sprintf(skinPath, sizeof(skinPath), ".%s", skin);

				aliasSkin->skin = R_AliasModelGetSkin(mod->name, skinPath);
				Q_strncpyz(aliasSkin->name, aliasSkin->skin->name, sizeof(aliasSkin->name));
			}
		}

		FS_FreeFile(buf);
	} else {
		mesh->num_skins = 1;
		mesh->skins = (mAliasSkin_t *)Mem_PoolAlloc(sizeof(mAliasSkin_t), vid_modelPool, 0);
		mesh->skins[0].skin = R_AliasModelGetSkin(mod->name, skinPath);
		Q_strncpyz(mesh->skins[0].name, mesh->skins[0].skin->name, sizeof(mesh->skins[0].name));
	}
}

/**
 * @brief Drives the actual parsing of the object file.  The file is read twice:
 * once to acquire primitive counts, and a second time to load them.
 */
static void R_LoadObjModel_ (model_t *mod, mobj_t *obj, const byte *buffer, int bufSize)
{
	char line[MAX_STRING_CHARS];
	const byte *c;
	qboolean comment;
	int i;

	c = buffer;
	comment = qfalse;
	i = 0;

	while (c[0] != '\0') {
		if (c[0] == '#') {
			comment = qtrue;
			c++;
			continue;
		}

		if (c[0] == '\r' || c[0] == '\n') {
			line[i++] = 0;
			i = 0;

			if (!comment)
				R_LoadObjModelLine(mod, obj, Com_Trim(line));

			comment = qfalse;
			c++;
			continue;
		}

		line[i++] = *c++;
	}
}

void R_LoadObjModel (model_t *mod, byte *buffer, int bufSize)
{
	mobj_t obj;
	const float *v;
	int i;

	mod->type = mod_obj;

	mod->alias.num_frames = 1;
	mod->alias.num_meshes = 1;

	OBJZERO(obj);

	/* resolve primitive counts */
	R_LoadObjModel_(mod, &obj, buffer, bufSize);

	if (!obj.num_verts || !obj.num_texcoords || !obj.num_tris)
		Com_Error(ERR_DROP, "R_LoadObjModel: Failed to resolve model data: %s (%i %i %i %i)\n",
			mod->name, obj.num_verts, obj.num_texcoords, obj.num_tris, obj.num_normals);

	/* allocate the primitives */
	obj.verts = (float *)Mem_PoolAlloc(obj.num_verts * sizeof(float) * 3, vid_modelPool, 0);
	if (obj.num_normals)
		obj.normals = (float *)Mem_PoolAlloc(obj.num_normals * sizeof(float) * 3, vid_modelPool, 0);
	obj.texcoords = (float *)Mem_PoolAlloc(obj.num_texcoords * sizeof(float) * 2, vid_modelPool, 0);
	obj.tris = (mobjtri_t *)Mem_PoolAlloc(obj.num_tris * sizeof(mobjtri_t), vid_modelPool, 0);

	/* load the primitives */
	R_LoadObjModel_(mod, &obj, buffer, bufSize);

	v = obj.verts;
	/* resolve mins/maxs */
	for (i = 0; i < obj.num_verts; i++, v += 3)
		AddPointToBounds(v, mod->mins, mod->maxs);

	/* we only have one mesh in obj files */
	mod->alias.meshes = (mAliasMesh_t *)Mem_PoolAlloc(sizeof(mAliasMesh_t), vid_modelPool, 0);

	/* load the skin */
	R_LoadObjSkin(mod);

	/* and finally the arrays */
	R_LoadObjModelVertexArrays(&obj, mod);

	/* this is no longer needed - we loaded everything into the generic model structs */
	Mem_Free(obj.verts);
	Mem_Free(obj.normals);
	Mem_Free(obj.texcoords);
	Mem_Free(obj.tris);
}
