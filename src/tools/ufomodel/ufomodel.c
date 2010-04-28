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

typedef struct modelConfig_s {
	qboolean overwrite;
} modelConfig_t;

static modelConfig_t config;

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
	} else {
		image = NULL;
	}

	/* no fitting texture found */
	if (!image) {
		Com_Printf("  \\ - could not load skin '%s'\n", pname);
		image = r_noTexture;
	}

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

static void WriteToFile (const model_t *mod, const mAliasMesh_t *mesh, const char *fileName)
{
	int i;
	qFILE f;

	Com_Printf("  \\ - writing to file '%s'\n", fileName);

	FS_OpenFile(fileName, &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("  \\ - can not open '%s' for writing\n", fileName);
		return;
	}

	for (i = 0; i < mesh->num_verts; i++) {
		mAliasVertex_t *v = &mesh->vertexes[i];
		int j;
		for (j = 0; j < 3; j++) {
			v->normal[j] = LittleFloat(v->normal[j]);
			v->tangent[j] = LittleFloat(v->tangent[j]);
		}
	}

	for (i = 0; i < mesh->num_verts; i++) {
		mAliasVertex_t *v = &mesh->vertexes[i];
		const size_t length = sizeof(vec3_t);
		FS_Write(v->normal, length, &f);
	}

	for (i = 0; i < mesh->num_verts; i++) {
		mAliasVertex_t *v = &mesh->vertexes[i];
		const size_t length = sizeof(vec3_t);
		FS_Write(v->tangent, length, &f);
	}

	FS_CloseFile(&f);
}

static void PrecalcNormalsAndTangents (const char *pattern)
{
	const char *filename;
	int cntCalculated, cntAll;

	FS_BuildFileList(pattern);

	cntAll = cntCalculated = 0;

	while ((filename = FS_NextFileFromFileList(pattern)) != NULL) {
		char mdxFileName[MAX_QPATH];
		model_t *mod;
		int i;

		cntAll++;

		Com_Printf("- model '%s'\n", filename);

		Com_StripExtension(filename, mdxFileName, sizeof(mdxFileName));
		Q_strcat(mdxFileName, ".mdx", sizeof(mdxFileName));

		if (!config.overwrite && FS_CheckFile("%s", mdxFileName) != -1) {
			Com_Printf("  \\ - mdx already exists\n");
			continue;
		}

		mod = LoadModel(filename);

		Com_Printf("  \\ - # meshes '%i'\n", mod->alias.num_meshes);

		for (i = 0; i < mod->alias.num_meshes; i++) {
			int j;
			mAliasMesh_t *mesh = &mod->alias.meshes[i];
			for (j = 0; j < mod->alias.num_frames; j++) {
				R_ModCalcNormalsAndTangents(mesh, mesh->num_verts * j);
			}
			/** @todo currently md2 models only have one mesh - for
			 * md3 files this would get overwritten for each mesh */
			WriteToFile(mod, mesh, mdxFileName);

			cntCalculated++;
		}
	}
	FS_NextFileFromFileList(NULL);

	Com_Printf("%i/%i\n", cntCalculated, cntAll);
}

static void Usage (void)
{
	Com_Printf("Usage:\n");
	Com_Printf(" -overwrite     will overwrite existing mdx files\n");
	Com_Printf(" -h --help      will show this help screen\n");
}

/**
 * @brief Parameter parsing
 */
static void UM_Parameter (int argc, const char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-overwrite")) {
			config.overwrite = qtrue;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			Usage();
			exit(0);
		} else {
			Com_Printf("Parameters unknown. Try --help.\n");
			Usage();
			exit(0);
		}
	}
}

int main (int argc, const char **argv)
{
	Com_Printf("---- ufomodel "VERSION" ----\n");
	Com_Printf(BUILDSTRING"\n");

	UM_Parameter(argc, argv);

	Swap_Init();
	Mem_Init();

	com_genericPool = Mem_CreatePool("ufomodel");
	com_fileSysPool = Mem_CreatePool("ufomodel filesys");
	vid_modelPool = Mem_CreatePool("ufomodel model");
	vid_imagePool = Mem_CreatePool("ufomodel image");

	FS_InitFilesystem(qfalse);

	r_noTexture = Mem_PoolAlloc(sizeof(*r_noTexture), vid_imagePool, 0);
	Q_strncpyz(r_noTexture->name, "noTexture", sizeof(r_noTexture->name));

	PrecalcNormalsAndTangents("**.md2");
	PrecalcNormalsAndTangents("**.md3");
	PrecalcNormalsAndTangents("**.dpm");
	PrecalcNormalsAndTangents("**.obj");

	Mem_Shutdown();

	return 0;
}
