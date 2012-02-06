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
#include "../../common/mem.h"
#include "../../shared/shared.h"
#include "../../common/filesys.h"
#include "../../common/qfiles.h"
#include "../../shared/typedefs.h"
#include "../../common/mem.h"
#include "../../client/renderer/r_material.h"
#include "../../client/renderer/r_image.h"
#include "../../client/renderer/r_model.h"
#include "../../client/renderer/r_state.h"
#include "../../shared/images.h"

#define VERSION "0.2"

rstate_t r_state;
image_t *r_noTexture;

typedef enum {
	ACTION_NONE,

	ACTION_MDX,
	ACTION_SKINEDIT,
	ACTION_CHECK,
	ACTION_SKINFIX,
	ACTION_GLCMDSREMOVE
} ufoModelAction_t;

typedef struct modelConfig_s {
	qboolean overwrite;
	qboolean verbose;
	char fileName[MAX_QPATH];
	ufoModelAction_t action;
	float smoothness;
	char inputName[MAX_QPATH];
} modelConfig_t;

static modelConfig_t config;

struct memPool_s *com_genericPool;
struct memPool_s *com_fileSysPool;
struct memPool_s *vid_modelPool;
struct memPool_s *vid_lightPool;	/* compilation dependency */
struct memPool_s *vid_imagePool;

static void Exit(int exitCode) __attribute__ ((__noreturn__));

static void Exit (int exitCode)
{
	Mem_Shutdown();

	exit(exitCode);
}

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
	if (config.verbose) {
		char outBuffer[4096];
		va_list argptr;

		va_start(argptr, fmt);
		Q_vsnprintf(outBuffer, sizeof(outBuffer), fmt, argptr);
		va_end(argptr);

		Com_Printf("%s", outBuffer);
	}
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

	image = (image_t *)Mem_PoolAlloc(sizeof(*image), vid_imagePool, 0);
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

image_t *R_FindImage (const char *pname, imagetype_t type)
{
	char lname[MAX_QPATH];
	image_t *image;
	SDL_Surface *surf;

	if (!pname || !pname[0])
		Com_Error(ERR_FATAL, "R_FindImage: NULL name");

	/* drop extension */
	Com_StripExtension(pname, lname, sizeof(lname));

	if ((surf = Img_LoadImage(lname))) {
		image = R_LoadImageData(lname, (byte *)surf->pixels, surf->w, surf->h, type);
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

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	fprintf(stderr, "Error: %s\n", msg);
	Exit(1);
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

	mod = (model_t *)Mem_PoolAlloc(sizeof(*mod), vid_modelPool, 0);
	Q_strncpyz(mod->name, name, sizeof(mod->name));

	/* call the appropriate loader */
	switch (LittleLong(*(unsigned *) buf)) {
	case IDALIASHEADER:
		/* MD2 header */
		R_ModLoadAliasMD2Model(mod, buf, modfilelen, qfalse);
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
			Com_Error(ERR_FATAL, "LoadModel: unknown fileid for %s", mod->name);
	}

	FS_FreeFile(buf);

	return mod;
}

static void WriteToFile (const model_t *mod, const mAliasMesh_t *mesh, const char *fileName)
{
	int i;
	qFILE f;
	uint32_t version = MDX_VERSION;
	int32_t numIndexes, numVerts;

	Com_Printf("  \\ - writing to file '%s'\n", fileName);

	FS_OpenFile(fileName, &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("  \\ - can not open '%s' for writing\n", fileName);
		return;
	}

	FS_Write(IDMDXHEADER, strlen(IDMDXHEADER), &f);
	version = LittleLong(version);
	FS_Write(&version, sizeof(version), &f);

	numIndexes = LittleLong(mesh->num_tris * 3);
	numVerts = LittleLong(mesh->num_verts);
	FS_Write(&numVerts, sizeof(numVerts), &f);
	FS_Write(&numIndexes, sizeof(numIndexes), &f);

	for (i = 0; i < mesh->num_tris * 3; i++) {
		const int32_t idx = LittleLong(mesh->indexes[i]);
		FS_Write(&idx, sizeof(idx), &f);
	}

	FS_CloseFile(&f);
}

static int PrecalcNormalsAndTangents (const char *filename)
{
	char mdxFileName[MAX_QPATH];
	model_t *mod;
	int i;
	int cntCalculated = 0;

	Com_Printf("- model '%s'\n", filename);

	Com_StripExtension(filename, mdxFileName, sizeof(mdxFileName));
	Q_strcat(mdxFileName, ".mdx", sizeof(mdxFileName));

	if (!config.overwrite && FS_CheckFile("%s", mdxFileName) != -1) {
		Com_Printf("  \\ - mdx already exists\n");
		return 0;
	}

	mod = LoadModel(filename);
	if (!mod)
		Com_Error(ERR_DROP, "Could not load %s", filename);

	Com_Printf("  \\ - # meshes '%i', # frames '%i'\n", mod->alias.num_meshes, mod->alias.num_frames);

	for (i = 0; i < mod->alias.num_meshes; i++) {
		mAliasMesh_t *mesh = &mod->alias.meshes[i];
		R_ModCalcUniqueNormalsAndTangents(mesh, mod->alias.num_frames, config.smoothness);
		/** @todo currently md2 models only have one mesh - for
		 * md3 files this would get overwritten for each mesh */
		WriteToFile(mod, mesh, mdxFileName);

		cntCalculated++;
	}

	return cntCalculated;
}

static void PrecalcNormalsAndTangentsBatch (const char *pattern)
{
	const char *filename;
	int cntCalculated, cntAll;

	FS_BuildFileList(pattern);

	cntAll = cntCalculated = 0;

	while ((filename = FS_NextFileFromFileList(pattern)) != NULL) {
		cntAll++;
		cntCalculated += PrecalcNormalsAndTangents(filename);
	}

	FS_NextFileFromFileList(NULL);

	Com_Printf("%i/%i\n", cntCalculated, cntAll);
}

static void Usage (void)
{
	Com_Printf("Usage:\n");
	Com_Printf(" -mdx                     generate mdx files\n");
	Com_Printf(" -skinfix                 fix skins for md2 models\n");
	Com_Printf(" -glcmds                  remove the unused glcmds from md2 models\n");
	Com_Printf(" -check                   perform general checks for all the models\n");
	Com_Printf(" -skinedit <filename>     edit skin of a model\n");
	Com_Printf(" -overwrite               overwrite existing mdx files\n");
	Com_Printf(" -s <float>               sets the smoothness value for normal-smoothing (in the range -1.0 to 1.0)\n");
	Com_Printf(" -f <filename>            build tangentspace for the specified model file\n");
	Com_Printf(" -v --verbose             print debug messages\n");
	Com_Printf(" -h --help                show this help screen\n");
}

static void UM_DefaultParameter (void)
{
	config.smoothness = 0.5;
}

/**
 * @brief Parameter parsing
 */
static void UM_Parameter (int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (Q_streq(argv[i], "-overwrite")) {
			config.overwrite = qtrue;
		} else if (Q_streq(argv[i], "-f") && (i + 1 < argc)) {
			Q_strncpyz(config.inputName, argv[++i], sizeof(config.inputName));
		} else if (Q_streq(argv[i], "-s") && (i + 1 < argc)) {
			config.smoothness = strtod(argv[++i], NULL);
			if (config.smoothness < -1.0 || config.smoothness > 1.0){
				Usage();
				Exit(1);
			}
		} else if (Q_streq(argv[i], "-mdx")) {
			config.action = ACTION_MDX;
		} else if (Q_streq(argv[i], "-glcmds")) {
			config.action = ACTION_GLCMDSREMOVE;
		} else if (Q_streq(argv[i], "-skinfix")) {
			config.action = ACTION_SKINFIX;
		} else if (Q_streq(argv[i], "-check")) {
			config.action = ACTION_CHECK;
		} else if (Q_streq(argv[i], "-skinedit")) {
			config.action = ACTION_SKINEDIT;
			if (i + 1 == argc) {
				Usage();
				Exit(1);
			}
			Q_strncpyz(config.fileName, argv[i + 1], sizeof(config.fileName));
			i++;
		} else if (Q_streq(argv[i], "-v") || Q_streq(argv[i], "--verbose")) {
			config.verbose = qtrue;
		} else if (Q_streq(argv[i], "-h") || Q_streq(argv[i], "--help")) {
			Usage();
			Exit(0);
		} else {
			Com_Printf("Parameters unknown. Try --help.\n");
			Usage();
			Exit(1);
		}
	}
}

static void MD2HeaderCheck (const dMD2Model_t *md2, const char *fileName, int bufSize)
{
	/* sanity checks */
	const uint32_t version = LittleLong(md2->version);
	const uint32_t numSkins = LittleLong(md2->num_skins);
	const uint32_t numTris = LittleLong(md2->num_tris);
	const uint32_t numVerts = LittleLong(md2->num_verts);

	if (version != MD2_ALIAS_VERSION)
		Com_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", fileName, version, MD2_ALIAS_VERSION);

	if (bufSize != LittleLong(md2->ofs_end))
		Com_Error(ERR_DROP, "model %s broken offset values (%i, %i)", fileName, bufSize, LittleLong(md2->ofs_end));

	if (numSkins == 0 || numSkins >= MD2_MAX_SKINS)
		Com_Error(ERR_DROP, "model '%s' has invalid skin number: %i", fileName, numSkins);

	if (numVerts == 0 || numVerts >= MD2_MAX_VERTS)
		Com_Error(ERR_DROP, "model %s has too many (or no) vertices (%i/%i)", fileName, numVerts, MD2_MAX_VERTS);

	if (numTris == 0 || numTris >= MD2_MAX_TRIANGLES)
		Com_Error(ERR_DROP, "model %s has too many (or no) triangles (%i/%i)", fileName, numTris, MD2_MAX_TRIANGLES);
}

static void MD2SkinEdit (const byte *buf, const char *fileName, int bufSize, void *userData)
{
	const char *md2Path;
	uint32_t numSkins;
	int i;
	const dMD2Model_t *md2 = (const dMD2Model_t *)buf;

	MD2HeaderCheck(md2, fileName, bufSize);

	md2Path = (const char *) md2 + LittleLong(md2->ofs_skins);
	numSkins = LittleLong(md2->num_skins);

	for (i = 0; i < numSkins; i++) {
		const char *name = md2Path + i * MD2_MAX_SKINNAME;
		Com_Printf("  \\ - skin %i: %s\n", i, name);
		/** @todo: Implement this */
	}
}

typedef void (*modelWorker_t) (const byte *buf, const char *fileName, int bufSize, void *userData);

/**
 * @brief The caller has to ensure that the model is from the expected format
 * @param worker The worker callback
 * @param fileName The file name of the model to load
 * @param userData User data that is passed to the worker function
 */
static void ModelWorker (modelWorker_t worker, const char *fileName, void *userData)
{
	byte *buf = NULL;
	int modfilelen;

	/* load the file */
	modfilelen = FS_LoadFile(fileName, &buf);
	if (!buf)
		Com_Error(ERR_FATAL, "%s not found", fileName);

	switch (LittleLong(*(unsigned *) buf)) {
	case IDALIASHEADER:
	case DPMHEADER:
	case IDMD3HEADER:
	case IDBSPHEADER:
		worker(buf, fileName, modfilelen, userData);
		break;

	default:
		if (!Q_strcasecmp(fileName + strlen(fileName) - 4, ".obj"))
			worker(buf, fileName, modfilelen, userData);
		else
			Com_Error(ERR_DROP, "ModelWorker: unknown fileid for %s", fileName);
	}

	FS_FreeFile(buf);
}

static void MD2GLCmdsRemove (const byte *buf, const char *fileName, int bufSize, void *userData)
{
	uint32_t numGLCmds;
	const dMD2Model_t *md2 = (const dMD2Model_t *)buf;

	MD2HeaderCheck(md2, fileName, bufSize);

	numGLCmds = LittleLong(md2->num_glcmds);

	if (numGLCmds > 0) {
		dMD2Model_t *fixedMD2 = (dMD2Model_t *)Mem_Dup(buf, bufSize);
		const size_t delta = numGLCmds * sizeof(uint32_t);
		const uint32_t offset = LittleLong(fixedMD2->ofs_glcmds);

		if (LittleLong(fixedMD2->ofs_skins) > offset || LittleLong(fixedMD2->ofs_frames) > offset
		 || LittleLong(fixedMD2->ofs_st) > offset || LittleLong(fixedMD2->ofs_tris) > offset) {
			Com_Error(ERR_DROP, "Unexpected order of the different data lumps");
		}

		fixedMD2->ofs_end = LittleLong(fixedMD2->ofs_end - delta);
		fixedMD2->ofs_glcmds = 0;
		fixedMD2->num_glcmds = 0;

		bufSize -= delta;

		FS_WriteFile(fixedMD2, bufSize, fileName);

		Mem_Free(fixedMD2);

		*(size_t *)userData += delta;
		Com_Printf("  \\ - removed %i glcmds from '%s' (save "UFO_SIZE_T" bytes)\n",
				numGLCmds, fileName, delta);
	}
}

static void MD2SkinFix (const byte *buf, const char *fileName, int bufSize, void *userData)
{
	const char *md2Path;
	uint32_t numSkins;
	int i;
	const dMD2Model_t *md2 = (const dMD2Model_t *)buf;
	byte *model = NULL;

	MD2HeaderCheck(md2, fileName, bufSize);

	md2Path = (const char *) md2 + LittleLong(md2->ofs_skins);
	numSkins = LittleLong(md2->num_skins);

	for (i = 0; i < numSkins; i++) {
		const char *extension;
		int errors = 0;
		const char *name = md2Path + i * MD2_MAX_SKINNAME;

		if (name[0] != '.')
			errors++;
		else
			/* skip the . to not confuse the extension extraction below */
			name++;

		extension = Com_GetExtension(name);
		if (extension != NULL)
			errors++;

		if (errors > 0) {
			dMD2Model_t *fixedMD2;
			char *skinPath;
			char path[MD2_MAX_SKINNAME];
			char pathBuf[MD2_MAX_SKINNAME];
			const char *fixedPath;
			if (model == NULL) {
				model = (byte *)Mem_Dup(buf, bufSize);
				Com_Printf("model: %s\n", fileName);
			}
			fixedMD2 = (dMD2Model_t *)model;
			skinPath = (char *) fixedMD2 + LittleLong(fixedMD2->ofs_skins) + i * MD2_MAX_SKINNAME;

			OBJZERO(path);

			if (extension != NULL) {
				Com_StripExtension(name, pathBuf, sizeof(pathBuf));
				fixedPath = pathBuf;
			} else {
				fixedPath = name;
			}
			if (name[0] != '.')
				Com_sprintf(path, sizeof(path), ".%s", Com_SkipPath(fixedPath));
			Com_Printf("  \\ - skin %i: changed path to '%s'\n", i + 1, path);
			if (R_AliasModelGetSkin(fileName, path) == r_noTexture) {
				Com_Printf("    \\ - could not load the skin with the new path\n");
			} else {
				memcpy(skinPath, path, sizeof(path));
			}
		}
	}
	if (model != NULL) {
		FS_WriteFile(model, bufSize, fileName);
		Mem_Free(model);
	}
}

static void MD2Check (const byte *buf, const char *fileName, int bufSize, void *userData)
{
	const char *md2Path;
	uint32_t numSkins;
	int i;
	qboolean headline = qfalse;
	const dMD2Model_t *md2 = (const dMD2Model_t *)buf;

	MD2HeaderCheck(md2, fileName, bufSize);

	md2Path = (const char *) md2 + LittleLong(md2->ofs_skins);
	numSkins = LittleLong(md2->num_skins);

	for (i = 0; i < numSkins; i++) {
		const char *extension;
		int errors = 0;
		const char *name = md2Path + i * MD2_MAX_SKINNAME;

		if (name[0] != '.')
			errors++;
		else
			/* skip the . to not confuse the extension extraction below */
			name++;

		extension = Com_GetExtension(name);
		if (extension != NULL)
			errors++;

		if (errors > 0) {
			if (!headline) {
				Com_Printf("model: %s\n", fileName);
				headline = qtrue;
			}
			Com_Printf("  \\ - skin %i: %s - %i errors/warnings\n", i + 1, name, errors);
			if (name[0] != '.')
				Com_Printf("    \\ - skin contains full path\n");
			if (extension != NULL)
				Com_Printf("    \\ - skin contains extension '%s'\n", extension);
			if (R_AliasModelGetSkin(fileName, md2Path + i * MD2_MAX_SKINNAME) == r_noTexture)
				Com_Printf("  \\ - could not load the skin\n");
		}
	}
}

static void MD2Visitor (modelWorker_t worker, void *userData)
{
	const char *fileName;
	const char *pattern = "**.md2";

	FS_BuildFileList(pattern);

	while ((fileName = FS_NextFileFromFileList(pattern)) != NULL)
		ModelWorker(worker, fileName, userData);

	FS_NextFileFromFileList(NULL);
}

static void ModelCheck (void)
{
	MD2Visitor(MD2Check, NULL);
}

static void SkinFix (void)
{
	MD2Visitor(MD2SkinFix, NULL);
}

static void GLCmdsRemove (void)
{
	size_t bytes = 0;
	MD2Visitor(MD2GLCmdsRemove, &bytes);
	Com_Printf("Saved "UFO_SIZE_T"bytes after removing all glcmds from the md2 files\n", bytes);
}

void R_ReallocateStateArrays(int size)
{
	/* TODO: check if stub without code would be sufficient here */
	if (size <= r_state.array_size)
		return;
	r_state.vertex_array_3d = (GLfloat *) Mem_SafeReAlloc(r_state.vertex_array_3d, size * 3 * sizeof(GLfloat));
	r_state.vertex_array_2d = (GLshort *) Mem_SafeReAlloc(r_state.vertex_array_2d, size * 2 * sizeof(GLshort));
	r_state.color_array = (GLfloat *) Mem_SafeReAlloc(r_state.color_array, size * 4 * sizeof(GLfloat));
	r_state.normal_array = (GLfloat *) Mem_SafeReAlloc(r_state.normal_array, size * 3 * sizeof(GLfloat));
	r_state.tangent_array = (GLfloat *) Mem_SafeReAlloc(r_state.tangent_array, size * 4 * sizeof(GLfloat));
	r_state.next_vertex_array_3d = (GLfloat *) Mem_SafeReAlloc(r_state.next_vertex_array_3d, size * 3 * sizeof(GLfloat));
	r_state.next_normal_array = (GLfloat *) Mem_SafeReAlloc(r_state.next_normal_array, size * 3 * sizeof(GLfloat));
	r_state.next_tangent_array = (GLfloat *) Mem_SafeReAlloc(r_state.next_tangent_array, size * 4 * sizeof(GLfloat));
	r_state.array_size = size;
}

void R_ReallocateTexunitArray(gltexunit_t * texunit, int size)
{
	/* TODO: check if stub without code would be sufficient here */
	if (size <= texunit->array_size)
		return;
	texunit->texcoord_array = (GLfloat *) Mem_SafeReAlloc(texunit->texcoord_array, size * 2 * sizeof(GLfloat));
	texunit->array_size = size;
	if (!r_state.active_texunit)
		r_state.active_texunit = texunit;
}

#ifdef main
#undef main
#endif

int main (int argc, char **argv)
{
	Com_Printf("---- ufomodel "VERSION" ----\n");

	UM_DefaultParameter();
	UM_Parameter(argc, argv);

	if (config.action == ACTION_NONE) {
		Usage();
		Exit(1);
	}

	com_genericPool = Mem_CreatePool("ufomodel");
	com_fileSysPool = Mem_CreatePool("ufomodel filesys");
	vid_modelPool = Mem_CreatePool("ufomodel model");
	vid_imagePool = Mem_CreatePool("ufomodel image");

	Swap_Init();
	Mem_Init();

	FS_InitFilesystem(qfalse);

	r_noTexture = (image_t *)Mem_PoolAlloc(sizeof(*r_noTexture), vid_imagePool, 0);
	Q_strncpyz(r_noTexture->name, "noTexture", sizeof(r_noTexture->name));

	switch (config.action) {
	case ACTION_MDX:
		if (config.inputName[0] == '\0') {
			PrecalcNormalsAndTangentsBatch("**.md2");
			PrecalcNormalsAndTangentsBatch("**.md3");
			PrecalcNormalsAndTangentsBatch("**.dpm");
			/** @todo see https://sourceforge.net/tracker/?func=detail&aid=2993773&group_id=157793&atid=805242 */
			/*PrecalcNormalsAndTangentsBatch("**.obj");*/
		} else {
			PrecalcNormalsAndTangents(config.inputName);
		}
		break;

	case ACTION_SKINEDIT:
		ModelWorker(MD2SkinEdit, config.fileName, NULL);
		break;

	case ACTION_CHECK:
		ModelCheck();
		break;

	case ACTION_SKINFIX:
		SkinFix();
		break;

	case ACTION_GLCMDSREMOVE:
		GLCmdsRemove();
		break;

	default:
		Exit(1);
	}

	Mem_Shutdown();

	return 0;
}
