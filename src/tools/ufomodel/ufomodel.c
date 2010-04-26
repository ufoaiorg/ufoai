/**
 * @file ufomodel.c
 * @brief Starting point for model tool
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

#include "../../shared/ufotypes.h"
#include "../../common/unzip.h"
#include "../../common/mem.h"
#include "../../common/filesys.h"
#include "../../common/common.h"
#include "../../shared/shared.h"
#include "../../shared/mathlib.h"
#include "../../shared/defines.h"
#include "../../shared/typedefs.h"
#include "../../shared/byte.h"
#include "../../client/renderer/r_local.h"
#include "../../shared/images.h"

#define VERSION "0.1"

rstate_t r_state;
image_t *r_noTexture;


struct memPool_s *com_genericPool;
struct memPool_s *com_fileSysPool;
struct memPool_s *vid_modelPool;
struct memPool_s *vid_imagePool;

void Com_Printf (const char *format, ...)
{
	char out_buffer[4096];
	va_list argptr;

	va_start(argptr, format);
	Q_vsnprintf(out_buffer, sizeof(out_buffer), format, argptr);
	va_end(argptr);

	printf("%s", out_buffer);
}

void Com_DPrintf (int level, const char *fmt, ...)
{
}

image_t *R_LoadImageData (const char *name, byte * pic, int width, int height, imagetype_t type)
{
	image_t *image;
	size_t len;

	len = strlen(name);
	if (len >= sizeof(image->name))
		Com_Error(ERR_DROP, "R_LoadImageData: \"%s\" is too long", name);
	if (len == 0)
		Com_Error(ERR_DROP, "R_LoadImageData: name is empty");

	image = Mem_PoolAlloc(sizeof(*image), vid_imagePool, 0);
	image->has_alpha = qfalse;
	image->type = type;
	image->width = width;
	image->height = height;

	Q_strncpyz(image->name, name, sizeof(image->name));
	/* drop extension */
	if (len >= 4 && image->name[len - 4] == '.') {
		image->name[len - 4] = '\0';
		Com_Printf("Image with extension: '%s'\n", name);
	}

	return image;
}

#ifdef DEBUG
image_t *R_FindImageDebug (const char *pname, imagetype_t type, const char *file, int line)
#else
image_t *R_FindImage (const char *pname, imagetype_t type)
#endif
{
	char lname[MAX_QPATH];
	image_t *image;
	SDL_Surface *surf;

	if (!pname || !pname[0])
		Com_Error(ERR_FATAL, "R_FindImage: NULL name");

	/* drop extension */
	Com_StripExtension(pname, lname, sizeof(lname));

	if (Img_LoadImage(lname, &surf)) {
		image = R_LoadImageData(lname, surf->pixels, surf->w, surf->h, type);
		SDL_FreeSurface(surf);
	}

	/* no fitting texture found */
	if (!image)
		image = r_noTexture;

	return image;
}

/**
 * @note Both client and server can use this, and it will
 * do the appropriate things.
 */
void Com_Error (int code, const char *fmt, ...)
{
	va_list argptr;
	static char msg[1024];
	static qboolean recursive = qfalse;

	if (recursive)
		Sys_Error("recursive error after: %s", msg);
	recursive = qtrue;

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	recursive = qfalse;
	Sys_Backtrace();
	Sys_Error("%s", msg);
}


/**
 * @brief Loads in a model for the given name
 * @param[in] name Filename relative to base dir and with extension (models/model.md2)
 */
static model_t *LoadModel (const char *name)
{
	model_t *mod;
	byte *buf;
	int modfilelen;

	/* load the file */
	modfilelen = FS_LoadFile(name, &buf);
	if (!buf) {
		Com_Printf("Could not load '%s'\n", name);
		return NULL;
	}

	mod = Mem_PoolAlloc(sizeof(*mod), vid_modelPool, 0);
	Q_strncpyz(mod->name, name, sizeof(mod->name));

	/* call the appropriate loader */
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

	default:
		if (!Q_strcasecmp(mod->name + strlen(mod->name) - 4, ".obj"))
			R_LoadObjModel(mod, buf, modfilelen);
		else
			Com_Error(ERR_FATAL, "R_ModForName: unknown fileid for %s", mod->name);
	}

	FS_FreeFile(buf);

	return mod;
}

static void WriteToFile (const model_t *mod, const mAliasMesh_t *mesh)
{
	int i;
	const char *fileName;
	qFILE f;

	fileName = va("models/%s.mdx", mod->name);

	FS_OpenFile(fileName, &f, FILE_WRITE);

	for (i = 0; i < mesh->num_verts; i++) {
		mAliasVertex_t *v = &mesh->vertexes[i];
		int j;
		for (j = 0; j < 3; j++) {
			v->normal[j] = LittleFloat(v->normal[j]);
			v->tangent[j] = LittleFloat(v->tangent[j]);
		}
		FS_Write(v->normal, sizeof(v->normal), &f);
		FS_Write(v->tangent, sizeof(v->tangent), &f);
	}

	FS_CloseFile(&f);
}

static void PrecalcNormalsAndTangents (void)
{
	const char *filename;
	FS_BuildFileList("**.md2");
	while ((filename = FS_NextFileFromFileList("**.md2")) != NULL) {
		model_t *mod = LoadModel(filename);
		int i;

		Com_Printf("%s\n", filename);

		for (i = 0; i < mod->alias.num_meshes; i++) {
			int j;
			mAliasMesh_t *mesh = &mod->alias.meshes[i];
			for (j = 0; j < mod->alias.num_frames; j++) {
				R_ModCalcNormalsAndTangents(mesh, mesh->num_verts * j);
				WriteToFile(mod, mesh);
			}
		}
	}
	FS_NextFileFromFileList(NULL);
}

int main (int argc, const char **argv)
{
	Com_Printf("---- ufomodel "VERSION" ----\n");
	Com_Printf(BUILDSTRING);

	Swap_Init();
	Mem_Init();

	com_genericPool = Mem_CreatePool("ufomodel");
	com_fileSysPool = Mem_CreatePool("ufomodel filesys");
	vid_modelPool = Mem_CreatePool("ufomodel model");
	vid_imagePool = Mem_CreatePool("ufomodel image");

	FS_InitFilesystem(qfalse);

	r_noTexture = Mem_PoolAlloc(sizeof(*r_noTexture), vid_imagePool, 0);

	PrecalcNormalsAndTangents();

	Mem_Shutdown();

	return 0;
}
