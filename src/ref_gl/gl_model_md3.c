/**
 * @file gl_model_md3.c
 * @brief md3 alias model loading
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

/*
==============================================================================
MD3 ALIAS MODELS
==============================================================================
*/

/**
 * @brief Load MD3 models from file.
 * @note Some Vic code here not fully used
 */
extern void Mod_LoadAliasMD3Model (model_t *mod, void *buffer, int bufSize)
{
	int					version, i, j, l;
	dmd3_t				*pinmodel;
	dmd3frame_t			*pinframe;
	dmd3tag_t			*pintag;
	dmd3mesh_t			*pinmesh;
	dmd3skin_t			*pinskin;
	dmd3coord_t			*pincoord;
	dmd3vertex_t		*pinvert;
	unsigned int		*pinindex, *poutindex;
	maliasvertex_t		*poutvert;
	maliascoord_t		*poutcoord;
	maliasskin_t		*poutskin;
	maliasmesh_t		*poutmesh;
	maliastag_t			*pouttag;
	maliasframe_t		*poutframe;
	maliasmodel_t		*poutmodel;
	char				name[MAX_QPATH];
	float				lat, lng;
	char path[MAX_QPATH];
	char *slash, *end;

	pinmodel = (dmd3_t *)buffer;
	version = LittleLong(pinmodel->version);

	if (version != MD3_ALIAS_VERSION) {
		ri.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)",
				mod->name, version, MD3_ALIAS_VERSION);
	}

	poutmodel = ri.TagMalloc(ri.modelPool, sizeof(maliasmodel_t), 0);

	/* byte swap the header fields and sanity check */
	poutmodel->num_frames = LittleLong(pinmodel->num_frames);
	poutmodel->num_tags = LittleLong(pinmodel->num_tags);
	poutmodel->num_meshes = LittleLong(pinmodel->num_meshes);
	poutmodel->num_skins = 0;

	if (poutmodel->num_frames <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no frames", mod->name);
	else if (poutmodel->num_frames > MD3_MAX_FRAMES)
		ri.Sys_Error(ERR_DROP, "model %s has too many frames", mod->name);

	if (poutmodel->num_tags > MD3_MAX_TAGS)
		ri.Sys_Error(ERR_DROP, "model %s has too many tags", mod->name);
	else if (poutmodel->num_tags < 0)
		ri.Sys_Error(ERR_DROP, "model %s has invalid number of tags", mod->name);

	if (poutmodel->num_meshes <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no meshes", mod->name);
	else if (poutmodel->num_meshes > MD3_MAX_MESHES)
		ri.Sys_Error(ERR_DROP, "model %s has too many meshes", mod->name);

	/* load the frames */
	pinframe = (dmd3frame_t *)((byte *)pinmodel + LittleLong(pinmodel->ofs_frames));
	poutframe = poutmodel->frames = ri.TagMalloc(ri.modelPool, sizeof(maliasframe_t) * poutmodel->num_frames, 0);

	mod->radius = 0;
	ClearBounds(mod->mins, mod->maxs);

	for (i = 0; i < poutmodel->num_frames; i++, pinframe++, poutframe++) {
		for (j = 0; j < 3; j++) {
			poutframe->mins[j] = LittleFloat(pinframe->mins[j]);
			poutframe->maxs[j] = LittleFloat(pinframe->maxs[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}

		/* @todo:
		poutframe->radius = LittleFloat(pinframe->radius);
		*/
		mod->radius = max(mod->radius, poutframe->radius);
		AddPointToBounds(poutframe->mins, mod->mins, mod->maxs);
		AddPointToBounds(poutframe->maxs, mod->mins, mod->maxs);
	}

	/* load the tags */
	if (poutmodel->num_tags) {
		pintag = (dmd3tag_t *)((byte *)pinmodel + LittleLong(pinmodel->ofs_tags));
		pouttag = poutmodel->tags = ri.TagMalloc(ri.modelPool, sizeof(maliastag_t) * poutmodel->num_frames * poutmodel->num_tags, 0);

		for (i = 0; i < poutmodel->num_frames; i++) {
			for (l = 0; l < poutmodel->num_tags; l++, pintag++, pouttag++) {
				memcpy(pouttag->name, pintag->name, MD3_MAX_PATH);
				for (j = 0; j < 3; j++) {
					pouttag->orient.origin[j] = LittleFloat(pintag->orient.origin[j] );
					pouttag->orient.axis[0][j] = LittleFloat(pintag->orient.axis[0][j] );
					pouttag->orient.axis[1][j] = LittleFloat(pintag->orient.axis[1][j] );
					pouttag->orient.axis[2][j] = LittleFloat(pintag->orient.axis[2][j] );
				}
				ri.Con_Printf(PRINT_ALL, "X: (%f %f %f) Y: (%f %f %f) Z: (%f %f %f)\n", pouttag->orient.axis[0][0], pouttag->orient.axis[0][1], pouttag->orient.axis[0][2], pouttag->orient.axis[1][0], pouttag->orient.axis[1][1], pouttag->orient.axis[1][2], pouttag->orient.axis[2][0], pouttag->orient.axis[2][1], pouttag->orient.axis[2][2]);
			}
		}
	}

	/* load the meshes */
	pinmesh = (dmd3mesh_t *)((byte *)pinmodel + LittleLong(pinmodel->ofs_meshes));
	poutmesh = poutmodel->meshes = ri.TagMalloc(ri.modelPool, sizeof(maliasmesh_t) * poutmodel->num_meshes, 0);

	for (i = 0; i < poutmodel->num_meshes; i++, poutmesh++) {
		memcpy(poutmesh->name, pinmesh->name, MD3_MAX_PATH);

		if (Q_strncmp(pinmesh->id, "IDP3", 4)) {
			ri.Sys_Error(ERR_DROP, "mesh %s in model %s has wrong id (%s should be %i)",
					poutmesh->name, mod->name, pinmesh->id, IDMD3HEADER);
		}

		poutmesh->num_tris = LittleLong(pinmesh->num_tris);
		poutmesh->num_skins = LittleLong(pinmesh->num_skins);
		poutmesh->num_verts = LittleLong(pinmesh->num_verts);

		if (poutmesh->num_skins <= 0)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has no skins", i, mod->name);
		else if (poutmesh->num_skins > MD3_MAX_SHADERS)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has too many skins", i, mod->name);

		if (poutmesh->num_tris <= 0)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has no triangles", i, mod->name);
		else if (poutmesh->num_tris > MD3_MAX_TRIANGLES)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has too many triangles", i, mod->name);

		if (poutmesh->num_verts <= 0)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has no vertices", i, mod->name);
		else if (poutmesh->num_verts > MD3_MAX_VERTS)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has too many vertices", i, mod->name);

		/* register all skins */
		pinskin = (dmd3skin_t *)((byte *)pinmesh + LittleLong(pinmesh->ofs_skins));
		poutskin = poutmesh->skins = ri.TagMalloc(ri.modelPool, sizeof(maliasskin_t) * poutmesh->num_skins, 0);

		for (j = 0; j < poutmesh->num_skins; j++, pinskin++, poutskin++) {
			memcpy(name, pinskin->name, MD3_MAX_PATH);
			if (name[1] == 'o')
				name[0] = 'm';
			if (name[1] == 'l')
				name[0] = 'p';
			/* FIXME: support the . feature for model textures like for md2 */
			memcpy(poutskin->name, name, MD3_MAX_PATH);
			if (name[0] != '.')
				mod->skins[i] = GL_FindImage(name, it_skin);
			else {
				Q_strncpyz(path, mod->name, sizeof(path));
				end = path;
				while ((slash = strchr(end, '/')) != 0)
					end = slash + 1;
				strcpy(end, name + 1);
				mod->skins[i] = GL_FindImage(path, it_skin);
			}
		}

		/* load the indexes */
		pinindex = (unsigned int *)((byte *)pinmesh + LittleLong(pinmesh->ofs_tris));
		poutindex = poutmesh->indexes = ri.TagMalloc(ri.modelPool, sizeof(unsigned int) * poutmesh->num_tris * 3, 0);

		for (j = 0; j < poutmesh->num_tris; j++, pinindex += 3, poutindex += 3) {
			poutindex[0] = (unsigned int)LittleLong(pinindex[0]);
			poutindex[1] = (unsigned int)LittleLong(pinindex[1]);
			poutindex[2] = (unsigned int)LittleLong(pinindex[2]);
		}

		/* load the texture coordinates */
		pincoord = (dmd3coord_t *)((byte *)pinmesh + LittleLong(pinmesh->ofs_tcs));
		poutcoord = poutmesh->stcoords = ri.TagMalloc(ri.modelPool, sizeof(maliascoord_t) * poutmesh->num_verts, 0);

		for (j = 0; j < poutmesh->num_verts; j++, pincoord++, poutcoord++) {
			poutcoord->st[0] = LittleFloat(pincoord->st[0]);
			poutcoord->st[1] = LittleFloat(pincoord->st[1]);
		}

		/* load the vertexes and normals */
		pinvert = (dmd3vertex_t *)((byte *)pinmesh + LittleLong(pinmesh->ofs_verts));
		poutvert = poutmesh->vertexes = ri.TagMalloc(ri.modelPool, poutmodel->num_frames * poutmesh->num_verts * sizeof(maliasvertex_t), 0);

		for (l = 0; l < poutmodel->num_frames; l++) {
			for (j = 0; j < poutmesh->num_verts; j++, pinvert++, poutvert++) {
				poutvert->point[0] = LittleShort(pinvert->point[0]) * MD3_XYZ_SCALE;
				poutvert->point[1] = LittleShort(pinvert->point[1]) * MD3_XYZ_SCALE;
				poutvert->point[2] = LittleShort(pinvert->point[2]) * MD3_XYZ_SCALE;

				lat = (pinvert->norm >> 8) & 0xff;
				lng = (pinvert->norm & 0xff);

				lat *= M_PI/128;
				lng *= M_PI/128;

				poutvert->normal[0] = cos(lat) * sin(lng);
				poutvert->normal[1] = sin(lat) * sin(lng);
				poutvert->normal[2] = cos(lng);
			}
		}
		pinmesh = (dmd3mesh_t *)((byte *)pinmesh + LittleLong(pinmesh->meshsize));

		/* find neighbours */
		poutmesh->trneighbors = ri.TagMalloc(ri.modelPool, sizeof(int) * poutmesh->num_tris * 3, 0);
		/* FIXME */
		/*Mod_BuildTriangleNeighbors(poutmesh->trneighbors, poutmesh->indexes, poutmesh->num_tris);*/
	}
	mod->type = mod_alias_md3;
	mod->extraData = poutmodel;
}
