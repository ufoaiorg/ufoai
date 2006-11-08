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

model_t *loadmodel;
static int modfilelen;

static void Mod_LoadSpriteModel(model_t * mod, void *buffer);
static void Mod_LoadAliasModel(model_t * mod, void *buffer);

model_t mod_known[MAX_MOD_KNOWN];
static int mod_numknown;

model_t *rTiles[MAX_MAPTILES];
int rNumTiles;

/* the inline * models from the current map are kept seperate */
static model_t mod_inline[MAX_MOD_KNOWN];
static int numInline;

int registration_sequence;

/**
 * @brief Prints all loaded models
 */
void Mod_Modellist_f(void)
{
	int i;
	model_t *mod;
	int total;

	total = 0;
	ri.Con_Printf(PRINT_ALL, "Loaded models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		if (!mod->name[0])
			continue;
		ri.Con_Printf(PRINT_ALL, "%8i : %s\n", mod->extradatasize, mod->name);
		total += mod->extradatasize;
	}
	ri.Con_Printf(PRINT_ALL, "Total resident: %i\n", total);
}


/**
 * @brief Loads in a model for the given name
 */
static model_t *Mod_ForName(const char *name, qboolean crash)
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
			ri.Sys_Error(ERR_DROP, "bad inline model number");
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
	Q_strncpyz(mod->name, name, MAX_QPATH);
/*	Com_Printf( "name: %s\n", name ); */

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
/* 	case MD3_IDENT: */
/* 		Mod_LoadMd3Model (mod, buf); */
/* 		break; */
	case IDALIASHEADER:
		loadmodel->extradata = Hunk_Begin(0x400000);
		Mod_LoadAliasModel(mod, buf);
		break;

	case IDSPRITEHEADER:
		loadmodel->extradata = Hunk_Begin(0x10000);
		Mod_LoadSpriteModel(mod, buf);
		break;

	case IDBSPHEADER:
		ri.Sys_Error(ERR_DROP, "Mod_ForName: don't load BSPs with this function");
		break;

	default:
		ri.Sys_Error(ERR_DROP, "Mod_ForName: unknown fileid for %s", mod->name);
		break;
	}

	loadmodel->extradatasize = Hunk_End();

	ri.FS_FreeFile(buf);

	return mod;
}

/*
===============================================================================
BRUSHMODEL LOADING
===============================================================================
*/

static byte *mod_base;
static int shift[3];

/**
 * @brief
 */
static void Mod_LoadLighting(lump_t * l)
{
	if (!l->filelen) {
		loadmodel->lightdata = NULL;
		return;
	}
	loadmodel->lightdata = Hunk_Alloc(l->filelen);
	loadmodel->lightquant = *(byte *) (mod_base + l->fileofs);
	memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}


/**
 * @brief
 */
static void Mod_LoadVertexes(lump_t * l)
{
	dvertex_t *in;
	mvertex_t *out;
	int i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

/**
 * @brief
 */
static float RadiusFromBounds(vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return VectorLength(corner);
}


/**
 * @brief
 */
static void Mod_LoadSubmodels(lump_t * l)
{
	dmodel_t *in;
	mmodel_t *out;
	int i, j, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		/* spread the mins / maxs by a pixel */
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleFloat(in->mins[j]) - 1 + shift[j];
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1 + shift[j];
			out->origin[j] = LittleFloat(in->origin[j]) + shift[j];
		}
		out->radius = RadiusFromBounds(out->mins, out->maxs);
		out->headnode = LittleLong(in->headnode);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

/**
 * @brief
 */
static void Mod_LoadEdges(lump_t * l)
{
	dedge_t *in;
	medge_t *out;
	int i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc((count + 1) * sizeof(*out));

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

/**
 * @brief
 */
static void Mod_LoadTexinfo(lump_t * l)
{
	texinfo_t *in;
	mtexinfo_t *out, *step;
	int i, j, count;
	char name[MAX_QPATH];
	int next;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 8; j++)
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);

		out->flags = LittleLong(in->flags);
		next = LittleLong(in->nexttexinfo);
		if (next > 0)
			out->next = loadmodel->texinfo + next;
		else
			out->next = NULL;
		Com_sprintf(name, sizeof(name), "textures/%s.wal", in->texture);

		out->image = GL_FindImage(name, it_wall);
		if (!out->image) {
			ri.Con_Printf(PRINT_ALL, "Couldn't load %s\n", name);
			out->image = r_notexture;
		}
	}

	/* count animation frames */
	for (i = 0; i < count; i++) {
		out = &loadmodel->texinfo[i];
		out->numframes = 1;
		for (step = out->next; step && step != out; step = step->next)
			out->numframes++;
	}
}

/**
 * @brief Fills in s->texturemins[] and s->extents[]
 */
static void CalcSurfaceExtents(msurface_t * s)
{
	float mins[2], maxs[2], val;

/* 	vec3_t	pos; */
	int i, j, e;
	mvertex_t *v;
	mtexinfo_t *tex;
	int bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = loadmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j = 0; j < 2; j++) {
			val = v->position[0] * tex->vecs[j][0] + v->position[1] * tex->vecs[j][1] + v->position[2] * tex->vecs[j][2] + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++) {
		/* tiny rounding hack, not sure if it works */
		bmins[i] = floor(mins[i] / (1 << s->lquant));
		bmaxs[i] = ceil(maxs[i] / (1 << s->lquant));

		s->texturemins[i] = bmins[i] << s->lquant;
		s->extents[i] = (bmaxs[i] - bmins[i]) << s->lquant;
	}
}


void GL_BuildPolygonFromSurface(msurface_t * fa, int shift[3]);
void GL_CreateSurfaceLightmap(msurface_t * surf);

/**
 * @brief
 */
static void Mod_LoadFaces(lump_t * l)
{
	dface_t *in;
	msurface_t *out;
	int i, count, surfnum;
	int planenum, side;
	int ti;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	currentmodel = loadmodel;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;
		out->polys = NULL;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		ti = LittleShort(in->texinfo);
		if (ti < 0 || ti >= loadmodel->numtexinfo)
			ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: bad texinfo number");
		out->texinfo = loadmodel->texinfo + ti;

		out->lquant = loadmodel->lightquant;
		CalcSurfaceExtents(out);

		/* lighting info */
		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + i;

		/* set the drawing flags */
		if (out->texinfo->flags & SURF_WARP) {
			out->flags |= SURF_DRAWTURB;
			for (i = 0; i < 2; i++) {
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			/* cut up polygon for warps */
			GL_SubdivideSurface(out);
		}

		/* create lightmaps and polygons */
		if (!(out->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
			GL_CreateSurfaceLightmap(out);

		if (!(out->texinfo->flags & SURF_WARP))
			GL_BuildPolygonFromSurface(out, shift);
	}
}


/**
 * @brief
 */
static void Mod_SetParent(mnode_t * node, mnode_t * parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	Mod_SetParent(node->children[0], node);
	Mod_SetParent(node->children[1], node);
}

/**
 * @brief
 */
static void Mod_LoadNodes(lump_t * l)
{
	int i, j, count, p;
	dnode_t *in;
	mnode_t *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]) + shift[j];
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]) + shift[j];
		}

		p = LittleLong(in->planenum);
		if (in->planenum == -1)
			out->plane = NULL;
		else
			out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);
		/* differentiate from leafs */
		out->contents = -1;

		for (j = 0; j < 2; j++) {
			p = LittleLong(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *) (loadmodel->leafs + (-1 - p));
		}
	}

	/* sets nodes and leafs */
	Mod_SetParent(loadmodel->nodes, NULL);
}

/**
 * @brief
 */
static void Mod_LoadLeafs(lump_t * l)
{
	dleaf_t *in;
	mleaf_t *out;
	int i, j, count, p;

/*	glpoly_t	*poly; */

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]) + shift[j];
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]) + shift[j];
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->firstmarksurface = loadmodel->marksurfaces + LittleShort(in->firstleafface);
		out->nummarksurfaces = LittleShort(in->numleaffaces);
	}
}

/**
 * @brief
 */
static void Mod_LoadMarksurfaces(lump_t * l)
{
	int i, j, count;
	short *in;
	msurface_t **out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleShort(in[i]);
		if (j < 0 || j >= loadmodel->numsurfaces)
			ri.Sys_Error(ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/**
 * @brief
 */
static void Mod_LoadSurfedges(lump_t * l)
{
	int i, count;
	int *in, *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: bad surfedges count in %s: %i", loadmodel->name, count);

	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}


/**
 * @brief
 */
static void Mod_LoadPlanes(lump_t * l)
{
	int i, j;
	cplane_t *out;
	dplane_t *in;
	int count;
	int bits;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * 2 * sizeof(*out));

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0; i < count; i++, in++, out++) {
		bits = 0;
		for (j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat(in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;
	}
}


/**
 * @brief
 */
static void Mod_ShiftTile(void)
{
	mvertex_t *vert;
	cplane_t *plane;
	int i, j;

	/* we can't do this instantly, because of rounding errors on extents calculation */
	/* shift vertexes */
	for (i = 0, vert = loadmodel->vertexes; i < loadmodel->numvertexes; i++, vert++)
		for (j = 0; j < 3; j++)
			vert->position[j] += shift[j];

	/* shift planes */
	for (i = 0, plane = loadmodel->planes; i < loadmodel->numplanes; i++, plane++)
		for (j = 0; j < 3; j++)
			plane->dist += plane->normal[j] * shift[j];
}


/**
 * @brief
 */
static void R_AddMapTile(const char *name, int sX, int sY, int sZ)
{
	int i;
	unsigned *buffer;
	dheader_t *header;
	mmodel_t *bm;

	/* get new model */
	if ((mod_numknown < 0) || (mod_numknown >= MAX_MOD_KNOWN)) {
		ri.Sys_Error(ERR_DROP, "mod_numknown >= MAX_MOD_KNOWN");
		return;					/* never reached. need for code analyst. */
	}

	if ((rNumTiles < 0) || (rNumTiles >= MAX_MAPTILES)) {
		ri.Sys_Error(ERR_DROP, "Too many map tiles");
		return;					/* never reached. need for code analyst. */
	}

	/* alloc model and tile */
	loadmodel = &mod_known[mod_numknown++];
	rTiles[rNumTiles++] = loadmodel;
	memset(loadmodel, 0, sizeof(model_t));
	Com_sprintf(loadmodel->name, sizeof(loadmodel->name), "maps/%s.bsp", name);

	/* load the file */
	modfilelen = ri.FS_LoadFile(loadmodel->name, (void **) &buffer);
	if (!buffer) {
		ri.Sys_Error(ERR_DROP, "Mod_LoadBrushModel: %s not found", loadmodel->name);
		return;					/* never reached. need for code analyst. */
	}

	/* init */
	loadmodel->registration_sequence = registration_sequence;
	loadmodel->extradata = Hunk_Begin(0x1500000);
	loadmodel->type = mod_brush;

	/* prepare shifting */
	VectorSet(shift, sX * UNIT_SIZE, sY * UNIT_SIZE, sZ * UNIT_SIZE);

	/* test version */
	header = (dheader_t *) buffer;
	i = LittleLong(header->version);
	if (i != BSPVERSION)
		ri.Sys_Error(ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", loadmodel->name, i, BSPVERSION);

	/* swap all the lumps */
	mod_base = (byte *) header;

	for (i = 0; i < (int)sizeof(dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong(((int *) header)[i]);

	/* load into heap */
	Mod_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_ShiftTile();
	Mod_LoadMarksurfaces(&header->lumps[LUMP_LEAFFACES]);
	Mod_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels(&header->lumps[LUMP_MODELS]);
	/* regular and alternate animation */
	loadmodel->numframes = 2;

	loadmodel->extradatasize = Hunk_End();

	/* set up the submodels, the first 255 submodels */
	/* are the models of the different levels, don't */
	/* care about them */
	for (i = 258; i < loadmodel->numsubmodels; i++) {
		model_t *starmod;

		bm = &loadmodel->submodels[i];
		starmod = &mod_inline[numInline++];

		*starmod = *loadmodel;

		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->firstnode = bm->headnode;
		if (starmod->firstnode >= loadmodel->numnodes)
			ri.Sys_Error(ERR_DROP, "Inline model %i has bad firstnode", i);

		VectorCopy(bm->maxs, starmod->maxs);
		VectorCopy(bm->mins, starmod->mins);
		starmod->radius = bm->radius;

		if (i == 0)
			*loadmodel = *starmod;

		starmod->numleafs = bm->visleafs;
	}

}

/*
==============================================================================
ALIAS MODELS
==============================================================================
*/

/**
 * @brief
 */
static void Mod_LoadTags(model_t * mod, void *buffer)
{
	dtag_t *pintag, *pheader;
	int version;
	int i, j, size;
	float *inmat, *outmat;

	pintag = (dtag_t *) buffer;

	version = LittleLong(pintag->version);
	if (version != TAG_VERSION)
		ri.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", mod->tagname, version, TAG_VERSION);

	size = LittleLong(pintag->ofs_extractend);
	mod->tagdata = Hunk_Alloc(size);
	pheader = mod->tagdata;

	/* byte swap the header fields and sanity check */
	for (i = 0; i < (int)sizeof(dtag_t) / 4; i++)
		((int *) pheader)[i] = LittleLong(((int *) buffer)[i]);

	if (pheader->num_tags <= 0)
		ri.Sys_Error(ERR_DROP, "tag file %s has no tags", mod->tagname);

	if (pheader->num_frames <= 0)
		ri.Sys_Error(ERR_DROP, "tag file %s has no frames", mod->tagname);

	/* load tag names */
	memcpy((char *) pheader + pheader->ofs_names, (char *) pintag + pheader->ofs_names, pheader->num_tags * MAX_SKINNAME);

	/* load tag matrices */
	inmat = (float *) ((byte *) pintag + pheader->ofs_tags);
	outmat = (float *) ((byte *) pheader + pheader->ofs_tags);

	for (i = 0; i < pheader->num_tags * pheader->num_frames; i++) {
		for (j = 0; j < 4; j++) {
			*outmat++ = LittleFloat(*inmat++);
			*outmat++ = LittleFloat(*inmat++);
			*outmat++ = LittleFloat(*inmat++);
			*outmat++ = 0.0;
		}
		outmat--;
		*outmat++ = 1.0;
	}

/*	ri.Sys_Error( ERR_DROP, "TAGS: read: %i expected: %i ", (byte *)outmat - (byte *)pheader, pheader->ofs_extractend ); */
}


/**
 * @brief
 */
static void Mod_LoadAnims(model_t * mod, void *buffer)
{
	char *text, *token;
	manim_t *anim;
	int n;

	for (n = 0, text = buffer; text; n++)
		COM_Parse(&text);
	n /= 4;
	if (n > MAX_ANIMS)
		n = MAX_ANIMS;

	mod->animdata = Hunk_Alloc(n * sizeof(manim_t));
	anim = mod->animdata;
	text = buffer;
	mod->numanims = 0;

	do {
		/* get the name */
		token = COM_Parse(&text);
		if (!text)
			break;
		Q_strncpyz(anim->name, token, MAX_ANIMNAME);

		/* get the start */
		token = COM_Parse(&text);
		if (!text)
			break;
		anim->from = atoi(token);

		/* get the end */
		token = COM_Parse(&text);
		if (!text)
			break;
		anim->to = atoi(token);

		/* get the fps */
		token = COM_Parse(&text);
		if (!text)
			break;
		anim->time = (atof(token) > 0.01) ? (1000.0 / atof(token)) : (1000.0 / 0.01);

		/* add it */
		mod->numanims++;
		anim++;
	} while (mod->numanims < MAX_ANIMS);

/*	ri.Con_Printf( PRINT_ALL, "anims: %i\n", mod->numanims ); */
}


/**
 * @brief
 */
static void Mod_LoadAliasModel(model_t * mod, void *buffer)
{
	int i, j;
	dmdl_t *pinmodel, *pheader;
	dstvert_t *pinst, *poutst;
	dtriangle_t *pintri, *pouttri;
	daliasframe_t *pinframe, *poutframe;
	int *pincmd, *poutcmd;
	int version;

	byte *tagbuf, *animbuf;

	pinmodel = (dmdl_t *) buffer;

	version = LittleLong(pinmodel->version);
	if (version != ALIAS_VERSION)
		ri.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", mod->name, version, ALIAS_VERSION);

	pheader = Hunk_Alloc(LittleLong(pinmodel->ofs_end));

	/* byte swap the header fields and sanity check */
	for (i = 0; i < (int)sizeof(dmdl_t) / 4; i++)
		((int *) pheader)[i] = LittleLong(((int *) buffer)[i]);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		ri.Sys_Error(ERR_DROP, "model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);

	if (pheader->num_xyz <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no vertices", mod->name);

	if (pheader->num_xyz > MAX_VERTS)
		ri.Sys_Error(ERR_DROP, "model %s has too many vertices", mod->name);

	if (pheader->num_st <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no st vertices", mod->name);

	if (pheader->num_tris <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no triangles", mod->name);

	if (pheader->num_frames <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no frames", mod->name);

	/* load base s and t vertices (not used in gl version) */
	pinst = (dstvert_t *) ((byte *) pinmodel + pheader->ofs_st);
	poutst = (dstvert_t *) ((byte *) pheader + pheader->ofs_st);

	for (i = 0; i < pheader->num_st; i++) {
		poutst[i].s = LittleShort(pinst[i].s);
		poutst[i].t = LittleShort(pinst[i].t);
	}

	/* load triangle lists */
	pintri = (dtriangle_t *) ((byte *) pinmodel + pheader->ofs_tris);
	pouttri = (dtriangle_t *) ((byte *) pheader + pheader->ofs_tris);

	for (i = 0; i < pheader->num_tris; i++) {
		for (j = 0; j < 3; j++) {
			pouttri[i].index_xyz[j] = LittleShort(pintri[i].index_xyz[j]);
			pouttri[i].index_st[j] = LittleShort(pintri[i].index_st[j]);
		}
	}

	/* load the frames */
	for (i = 0; i < pheader->num_frames; i++) {
		pinframe = (daliasframe_t *) ((byte *) pinmodel + pheader->ofs_frames + i * pheader->framesize);
		poutframe = (daliasframe_t *) ((byte *) pheader + pheader->ofs_frames + i * pheader->framesize);

		memcpy(poutframe->name, pinframe->name, sizeof(poutframe->name));
		for (j = 0; j < 3; j++) {
			poutframe->scale[j] = LittleFloat(pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}
		/* verts are all 8 bit, so no swapping needed */
		memcpy(poutframe->verts, pinframe->verts, pheader->num_xyz * sizeof(dtrivertx_t));
	}

	mod->type = mod_alias;

	/* load the glcmds */
	pincmd = (int *) ((byte *) pinmodel + pheader->ofs_glcmds);
	poutcmd = (int *) ((byte *) pheader + pheader->ofs_glcmds);
	for (i = 0; i < pheader->num_glcmds; i++)
		poutcmd[i] = LittleLong(pincmd[i]);

	/* copy skin names */
	memcpy((char *) pheader + pheader->ofs_skins, (char *) pinmodel + pheader->ofs_skins, pheader->num_skins * MAX_SKINNAME);

	mod->mins[0] = -32;
	mod->mins[1] = -32;
	mod->mins[2] = -32;
	mod->maxs[0] = 32;
	mod->maxs[1] = 32;
	mod->maxs[2] = 32;

	/* load the tags */
	Q_strncpyz(mod->tagname, mod->name, MAX_QPATH);
	i = strlen(mod->tagname) - 4;
	strcpy(&(mod->tagname[i]), ".tag");

	/* try to load the tag file */
	if (ri.FS_CheckFile(mod->tagname) != -1) {
		/* load the tags */
		ri.FS_LoadFile(mod->tagname, (void **) &tagbuf);
		Mod_LoadTags(mod, tagbuf);
		ri.FS_FreeFile(tagbuf);
	}

	/* load the animations */
	Q_strncpyz(mod->animname, mod->name, MAX_QPATH);
	i = strlen(mod->animname) - 4;
	strcpy(&(mod->animname[i]), ".anm");

	/* try to load the animation file */
	if (ri.FS_CheckFile(mod->animname) != -1) {
		/* load the tags */
		ri.FS_LoadFile(mod->animname, (void **) &animbuf);
		Mod_LoadAnims(mod, animbuf);
		ri.FS_FreeFile(animbuf);
	}
}

/*
==============================================================================
SPRITE MODELS
==============================================================================
*/

/**
 * @brief
 */
static void Mod_LoadSpriteModel(model_t * mod, void *buffer)
{
	dsprite_t *sprin, *sprout;
	int i;

	sprin = (dsprite_t *) buffer;
	sprout = Hunk_Alloc(modfilelen);

	sprout->ident = LittleLong(sprin->ident);
	sprout->version = LittleLong(sprin->version);
	sprout->numframes = LittleLong(sprin->numframes);

	if (sprout->version != SPRITE_VERSION)
		ri.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", mod->name, sprout->version, SPRITE_VERSION);

	if (sprout->numframes > MAX_MD2SKINS)
		ri.Sys_Error(ERR_DROP, "%s has too many frames (%i > %i)", mod->name, sprout->numframes, MAX_MD2SKINS);

	/* byte swap everything */
	for (i = 0; i < sprout->numframes; i++) {
		sprout->frames[i].width = LittleLong(sprin->frames[i].width);
		sprout->frames[i].height = LittleLong(sprin->frames[i].height);
		sprout->frames[i].origin_x = LittleLong(sprin->frames[i].origin_x);
		sprout->frames[i].origin_y = LittleLong(sprin->frames[i].origin_y);
		memcpy(sprout->frames[i].name, sprin->frames[i].name, MAX_SKINNAME);
		mod->skins[i] = GL_FindImage(sprout->frames[i].name, it_sprite);
	}

	mod->type = mod_sprite;
}

/*============================================================================= */

void GL_EndBuildingLightmaps(void);
void GL_BeginBuildingLightmaps(void);

typedef model_t *model_p;

/**
 * @brief Specifies the model that will be used as the world
 */
void R_BeginRegistration(char *tiles, char *pos)
{
	char *token;
	char name[MAX_VAR];
	char base[MAX_QPATH];
	int sh[3];
	int i;

	/* next registration sequence */
	registration_sequence++;

	/* fix this, currently needed, slows down loading times */
	Mod_FreeAll();

	/* init */
	GL_BeginBuildingLightmaps();
	numInline = 0;
	rNumTiles = 0;

	/* load tiles */
	while (tiles) {
		/* get tile name */
		token = COM_Parse(&tiles);
		if (!tiles) {
			/* finish */
			GL_EndBuildingLightmaps();
			return;
		}

		/* get base path */
		if (token[0] == '-') {
			Q_strncpyz(base, token + 1, MAX_QPATH);
			continue;
		}

		/* get tile name */
		if (token[0] == '+')
			Com_sprintf(name, MAX_VAR, "%s%s", base, token + 1);
		else
			Q_strncpyz(name, token, MAX_VAR);

		if (pos && pos[0]) {
			/* get position and add a tile */
			for (i = 0; i < 2; i++) {
				token = COM_Parse(&pos);
				if (!pos)
					ri.Sys_Error(ERR_DROP, "R_BeginRegistration: invalid positions\n");
				sh[i] = atoi(token);
			}
			R_AddMapTile(name, sh[0], sh[1], 0);
		} else {
			/* load only a single tile, if no positions are specified */
			R_AddMapTile(name, 0, 0, 0);
			GL_EndBuildingLightmaps();
			return;
		}
	}

	ri.Sys_Error(ERR_DROP, "R_BeginRegistration: invalid tile names\n");
}

/**
 * @brief
 */
static signed int Mod_GetTris(short p1, short p2, dtriangle_t * side1, dmdl_t * hdr)
{
	dtriangle_t *tris = (dtriangle_t *) ((unsigned char *) hdr + hdr->ofs_tris);
	int i;

	for (i = 0; i < hdr->num_tris; i++, tris++) {
		if (tris == side1)
			continue;

		if (tris->index_xyz[0] == p2 && tris->index_xyz[1] == p1)
			return i;
		if (tris->index_xyz[1] == p2 && tris->index_xyz[2] == p1)
			return i;
		if (tris->index_xyz[2] == p2 && tris->index_xyz[0] == p1)
			return i;

	}
	tris--;
	return -1;
}

/**
 * @brief
 */
static void Mod_FindSharedEdges(model_t * mod)
{
	dmdl_t *hdr;
	dtriangle_t *tris;
	int i, o;

	assert (mod->type == mod_alias);
	hdr = (dmdl_t *) mod->extradata;
	tris = (dtriangle_t *) ((unsigned char *) hdr + hdr->ofs_tris);

	mod->noshadow = qfalse;

	for (i = 0; i < hdr->num_tris; i++) {
		mod->edge_tri[i][0] = Mod_GetTris(tris->index_xyz[0], tris->index_xyz[1], tris, hdr);
		mod->edge_tri[i][1] = Mod_GetTris(tris->index_xyz[1], tris->index_xyz[2], tris, hdr);
		mod->edge_tri[i][2] = Mod_GetTris(tris->index_xyz[2], tris->index_xyz[0], tris, hdr);

		for (o = 0; o < 3; o++)
			if (mod->edge_tri[i][o] == -1)
				mod->noshadow = qtrue;

		tris++;
	}
}


/**
 * @brief
 */
struct model_s *R_RegisterModel(const char *name)
{
	model_t *mod;
	int i;
	dsprite_t *sprout;
	dmdl_t *pheader;

	mod = Mod_ForName(name, qfalse);
	if (mod) {
		/* register any images used by the models */
		if (mod->type == mod_sprite) {
			sprout = (dsprite_t *) mod->extradata;
			for (i = 0; i < sprout->numframes; i++)
				mod->skins[i] = GL_FindImage(sprout->frames[i].name, it_sprite);
		} else if (mod->type == mod_alias) {
			char path[MAX_QPATH];
			char *skin, *slash, *end;

			pheader = (dmdl_t *) mod->extradata;
			for (i = 0; i < pheader->num_skins; i++) {
				skin = (char *) pheader + pheader->ofs_skins + i * MAX_SKINNAME;
				if (skin[0] != '.')
					mod->skins[i] = GL_FindImage(skin, it_skin);
				else {
					Q_strncpyz(path, mod->name, MAX_QPATH);
					end = path;
					while ((slash = strchr(end, '/')) != 0)
						end = slash + 1;
					strcpy(end, skin + 1);
					mod->skins[i] = GL_FindImage(path, it_skin);
				}
			}
			mod->numframes = pheader->num_frames;
			if (gl_shadows->value == 2)
				Mod_FindSharedEdges(mod);
		} else if (mod->type == mod_brush) {
			for (i = 0; i < mod->numtexinfo; i++)
				mod->texinfo[i].image->registration_sequence = registration_sequence;
		}
	}
	return mod;
}

/**
 * @brief
 */
struct model_s *R_RegisterModelShort(const char *name)
{
	if (!name || !name[0])
		return NULL;

	if (name[0] != '*' && (strlen(name) < 4 || name[strlen(name) - 4] != '.')) {
		char filename[MAX_QPATH];

		Com_sprintf(filename, MAX_QPATH, "models/%s.md2", name);
		return R_RegisterModel(filename);
	} else
		return R_RegisterModel(name);
}

/**
 * @brief
 * @sa Mod_FreeAll
 */
static void Mod_Free(model_t * mod)
{
	if (mod->extradatasize)
		Hunk_Free(mod->extradata);
	memset(mod, 0, sizeof(*mod));
}

/**
 * @brief
 * @sa R_BeginRegistration
 */
void R_EndRegistration(void)
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
void Mod_FreeAll(void)
{
	int i;

	for (i = 0; i < mod_numknown; i++) {
		if (mod_known[i].extradatasize)
			Mod_Free(&mod_known[i]);
	}
	mod_numknown = 0;
}
