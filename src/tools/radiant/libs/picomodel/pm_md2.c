/* -----------------------------------------------------------------------------

 PicoModel Library

 Copyright (c) 2002, Randy Reddig & seaw0lf
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list
 of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 Neither the names of the copyright holders nor the names of its contributors may
 be used to endorse or promote products derived from this software without
 specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ----------------------------------------------------------------------------- */

/*
 Nurail: Used pm_md3.c (Randy Reddig) as a template.
 */

/* marker */
#define PM_MD2_C

/* dependencies */
#include "picointernal.h"
#include "../../../shared/byte.h"

/* md2 model format */
#define MD2_MAGIC				"IDP2"
#define MD2_VERSION				8

#define MD2_NUMVERTEXNORMALS	162
#define MD2_MAX_SKINNAME		64
#define MD2_MAX_TRIANGLES		4096
#define MD2_MAX_VERTS			2048
#define MD2_MAX_FRAMES			512
#define MD2_MAX_MD2SKINS		32
#define MD2_MAX_SKINNAME		64

#ifndef byte
#define byte unsigned char
#endif

typedef struct
{
	short s;
	short t;
} md2St_t;

typedef struct
{
	short index_xyz[3];
	short index_st[3];
} md2Triangle_t;

typedef struct
{
	byte v[3]; /* scaled byte to fit in frame mins/maxs */
	byte lightnormalindex;
} md2XyzNormal_t;

typedef struct md2Frame_s
{
	float scale[3]; /* multiply byte verts by this */
	float translate[3]; /* then add this */
	char name[16]; /* frame name from grabbing */
	md2XyzNormal_t verts[1]; /* variable sized */
} md2Frame_t;

/* md2 model file md2 structure */
typedef struct md2_s
{
	char magic[4];
	int version;

	int skinWidth;
	int skinHeight;
	int frameSize;

	int numSkins;
	int numXYZ;
	int numST;
	int numTris;
	int numGLCmds;
	int numFrames;

	int ofsSkins;
	int ofsST;
	int ofsTris;
	int ofsFrames;
	int ofsGLCmds;
	int ofsEnd;
} md2_t;

static const float md2_normals[MD2_NUMVERTEXNORMALS][3] = { { -0.525731f, 0.000000f, 0.850651f }, { -0.442863f,
		0.238856f, 0.864188f }, { -0.295242f, 0.000000f, 0.955423f }, { -0.309017f, 0.500000f, 0.809017f }, {
		-0.162460f, 0.262866f, 0.951056f }, { 0.000000f, 0.000000f, 1.000000f }, { 0.000000f, 0.850651f, 0.525731f }, {
		-0.147621f, 0.716567f, 0.681718f }, { 0.147621f, 0.716567f, 0.681718f }, { 0.000000f, 0.525731f, 0.850651f }, {
		0.309017f, 0.500000f, 0.809017f }, { 0.525731f, 0.000000f, 0.850651f }, { 0.295242f, 0.000000f, 0.955423f }, {
		0.442863f, 0.238856f, 0.864188f }, { 0.162460f, 0.262866f, 0.951056f }, { -0.681718f, 0.147621f, 0.716567f }, {
		-0.809017f, 0.309017f, 0.500000f }, { -0.587785f, 0.425325f, 0.688191f }, { -0.850651f, 0.525731f, 0.000000f },
		{ -0.864188f, 0.442863f, 0.238856f }, { -0.716567f, 0.681718f, 0.147621f },
		{ -0.688191f, 0.587785f, 0.425325f }, { -0.500000f, 0.809017f, 0.309017f },
		{ -0.238856f, 0.864188f, 0.442863f }, { -0.425325f, 0.688191f, 0.587785f },
		{ -0.716567f, 0.681718f, -0.147621f }, { -0.500000f, 0.809017f, -0.309017f }, { -0.525731f, 0.850651f,
				0.000000f }, { 0.000000f, 0.850651f, -0.525731f }, { -0.238856f, 0.864188f, -0.442863f }, { 0.000000f,
				0.955423f, -0.295242f }, { -0.262866f, 0.951056f, -0.162460f }, { 0.000000f, 1.000000f, 0.000000f }, {
				0.000000f, 0.955423f, 0.295242f }, { -0.262866f, 0.951056f, 0.162460f }, { 0.238856f, 0.864188f,
				0.442863f }, { 0.262866f, 0.951056f, 0.162460f }, { 0.500000f, 0.809017f, 0.309017f }, { 0.238856f,
				0.864188f, -0.442863f }, { 0.262866f, 0.951056f, -0.162460f }, { 0.500000f, 0.809017f, -0.309017f }, {
				0.850651f, 0.525731f, 0.000000f }, { 0.716567f, 0.681718f, 0.147621f }, { 0.716567f, 0.681718f,
				-0.147621f }, { 0.525731f, 0.850651f, 0.000000f }, { 0.425325f, 0.688191f, 0.587785f }, { 0.864188f,
				0.442863f, 0.238856f }, { 0.688191f, 0.587785f, 0.425325f }, { 0.809017f, 0.309017f, 0.500000f }, {
				0.681718f, 0.147621f, 0.716567f }, { 0.587785f, 0.425325f, 0.688191f }, { 0.955423f, 0.295242f,
				0.000000f }, { 1.000000f, 0.000000f, 0.000000f }, { 0.951056f, 0.162460f, 0.262866f }, { 0.850651f,
				-0.525731f, 0.000000f }, { 0.955423f, -0.295242f, 0.000000f }, { 0.864188f, -0.442863f, 0.238856f }, {
				0.951056f, -0.162460f, 0.262866f }, { 0.809017f, -0.309017f, 0.500000f }, { 0.681718f, -0.147621f,
				0.716567f }, { 0.850651f, 0.000000f, 0.525731f }, { 0.864188f, 0.442863f, -0.238856f }, { 0.809017f,
				0.309017f, -0.500000f }, { 0.951056f, 0.162460f, -0.262866f }, { 0.525731f, 0.000000f, -0.850651f }, {
				0.681718f, 0.147621f, -0.716567f }, { 0.681718f, -0.147621f, -0.716567f }, { 0.850651f, 0.000000f,
				-0.525731f }, { 0.809017f, -0.309017f, -0.500000f }, { 0.864188f, -0.442863f, -0.238856f }, {
				0.951056f, -0.162460f, -0.262866f }, { 0.147621f, 0.716567f, -0.681718f }, { 0.309017f, 0.500000f,
				-0.809017f }, { 0.425325f, 0.688191f, -0.587785f }, { 0.442863f, 0.238856f, -0.864188f }, { 0.587785f,
				0.425325f, -0.688191f }, { 0.688191f, 0.587785f, -0.425325f }, { -0.147621f, 0.716567f, -0.681718f }, {
				-0.309017f, 0.500000f, -0.809017f }, { 0.000000f, 0.525731f, -0.850651f }, { -0.525731f, 0.000000f,
				-0.850651f }, { -0.442863f, 0.238856f, -0.864188f }, { -0.295242f, 0.000000f, -0.955423f }, {
				-0.162460f, 0.262866f, -0.951056f }, { 0.000000f, 0.000000f, -1.000000f }, { 0.295242f, 0.000000f,
				-0.955423f }, { 0.162460f, 0.262866f, -0.951056f }, { -0.442863f, -0.238856f, -0.864188f }, {
				-0.309017f, -0.500000f, -0.809017f }, { -0.162460f, -0.262866f, -0.951056f }, { 0.000000f, -0.850651f,
				-0.525731f }, { -0.147621f, -0.716567f, -0.681718f }, { 0.147621f, -0.716567f, -0.681718f }, {
				0.000000f, -0.525731f, -0.850651f }, { 0.309017f, -0.500000f, -0.809017f }, { 0.442863f, -0.238856f,
				-0.864188f }, { 0.162460f, -0.262866f, -0.951056f }, { 0.238856f, -0.864188f, -0.442863f }, {
				0.500000f, -0.809017f, -0.309017f }, { 0.425325f, -0.688191f, -0.587785f }, { 0.716567f, -0.681718f,
				-0.147621f }, { 0.688191f, -0.587785f, -0.425325f }, { 0.587785f, -0.425325f, -0.688191f }, {
				0.000000f, -0.955423f, -0.295242f }, { 0.000000f, -1.000000f, 0.000000f }, { 0.262866f, -0.951056f,
				-0.162460f }, { 0.000000f, -0.850651f, 0.525731f }, { 0.000000f, -0.955423f, 0.295242f }, { 0.238856f,
				-0.864188f, 0.442863f }, { 0.262866f, -0.951056f, 0.162460f }, { 0.500000f, -0.809017f, 0.309017f }, {
				0.716567f, -0.681718f, 0.147621f }, { 0.525731f, -0.850651f, 0.000000f }, { -0.238856f, -0.864188f,
				-0.442863f }, { -0.500000f, -0.809017f, -0.309017f }, { -0.262866f, -0.951056f, -0.162460f }, {
				-0.850651f, -0.525731f, 0.000000f }, { -0.716567f, -0.681718f, -0.147621f }, { -0.716567f, -0.681718f,
				0.147621f }, { -0.525731f, -0.850651f, 0.000000f }, { -0.500000f, -0.809017f, 0.309017f }, {
				-0.238856f, -0.864188f, 0.442863f }, { -0.262866f, -0.951056f, 0.162460f }, { -0.864188f, -0.442863f,
				0.238856f }, { -0.809017f, -0.309017f, 0.500000f }, { -0.688191f, -0.587785f, 0.425325f }, {
				-0.681718f, -0.147621f, 0.716567f }, { -0.442863f, -0.238856f, 0.864188f }, { -0.587785f, -0.425325f,
				0.688191f }, { -0.309017f, -0.500000f, 0.809017f }, { -0.147621f, -0.716567f, 0.681718f }, {
				-0.425325f, -0.688191f, 0.587785f }, { -0.162460f, -0.262866f, 0.951056f }, { 0.442863f, -0.238856f,
				0.864188f }, { 0.162460f, -0.262866f, 0.951056f }, { 0.309017f, -0.500000f, 0.809017f }, { 0.147621f,
				-0.716567f, 0.681718f }, { 0.000000f, -0.525731f, 0.850651f }, { 0.425325f, -0.688191f, 0.587785f }, {
				0.587785f, -0.425325f, 0.688191f }, { 0.688191f, -0.587785f, 0.425325f }, { -0.955423f, 0.295242f,
				0.000000f }, { -0.951056f, 0.162460f, 0.262866f }, { -1.000000f, 0.000000f, 0.000000f }, { -0.850651f,
				0.000000f, 0.525731f }, { -0.955423f, -0.295242f, 0.000000f }, { -0.951056f, -0.162460f, 0.262866f }, {
				-0.864188f, 0.442863f, -0.238856f }, { -0.951056f, 0.162460f, -0.262866f }, { -0.809017f, 0.309017f,
				-0.500000f }, { -0.864188f, -0.442863f, -0.238856f }, { -0.951056f, -0.162460f, -0.262866f }, {
				-0.809017f, -0.309017f, -0.500000f }, { -0.681718f, 0.147621f, -0.716567f }, { -0.681718f, -0.147621f,
				-0.716567f }, { -0.850651f, 0.000000f, -0.525731f }, { -0.688191f, 0.587785f, -0.425325f }, {
				-0.587785f, 0.425325f, -0.688191f }, { -0.425325f, 0.688191f, -0.587785f }, { -0.425325f, -0.688191f,
				-0.587785f }, { -0.587785f, -0.425325f, -0.688191f }, { -0.688191f, -0.587785f, -0.425325f }, };

/**
 * @brief Checks whether this module can load the given file
 */
static int _md2_canload (PM_PARAMS_CANLOAD)
{
	md2_t *md2;

	/* sanity check */
	if (bufSize < (sizeof(*md2) * 2))
		return PICO_PMV_ERROR_SIZE;

	/* set as md2 */
	md2 = (md2_t*) buffer;

	/* check md2 magic */
	if (*((const int*) md2->magic) != *((const int*) MD2_MAGIC))
		return PICO_PMV_ERROR_IDENT;

	/* check md2 version */
	if (_pico_little_long(md2->version) != MD2_VERSION)
		return PICO_PMV_ERROR_VERSION;

	/* file seems to be a valid md2 */
	return PICO_PMV_OK;
}

/**
 * @brief Loads a ufo md2 model file.
 */
static picoModel_t *_md2_load (PM_PARAMS_LOAD)
{
	int i, j;

	char skinname[MD2_MAX_SKINNAME];
	md2_t *md2;
	md2St_t *texCoord;
	md2Frame_t *frame;
	md2Triangle_t *triangle;

	picoByte_t *bb;
	picoModel_t *picoModel;
	picoSurface_t *picoSurface;
	picoShader_t *picoShader;
	picoColor_t color;

	int numIndexes, numVerts;
	double isw;
	int tempIndex[MD2_MAX_TRIANGLES * 3];
	int tempSTIndex[MD2_MAX_TRIANGLES * 3];
	int indRemap[MD2_MAX_TRIANGLES * 3];
	int *outIndex;

	/* set as md2 */
	bb = (picoByte_t*) buffer;
	md2 = (md2_t*) buffer;

	/* check ident and version */
	if (*((const int*) md2->magic) != *((const int*) MD2_MAGIC) || _pico_little_long(md2->version) != MD2_VERSION) {
		/* not an md2 file (todo: set error) */
		_pico_printf(PICO_ERROR, "%s is not an MD2 File!", fileName);
		return NULL;
	}

	/* swap md2 */
	md2->version = _pico_little_long(md2->version);

	md2->skinWidth = _pico_little_long(md2->skinWidth);
	md2->skinHeight = _pico_little_long(md2->skinHeight);
	md2->frameSize = _pico_little_long(md2->frameSize);

	md2->numSkins = _pico_little_long(md2->numSkins);
	md2->numXYZ = _pico_little_long(md2->numXYZ);
	md2->numST = _pico_little_long(md2->numST);
	md2->numTris = _pico_little_long(md2->numTris);
	md2->numGLCmds = _pico_little_long(md2->numGLCmds);
	md2->numFrames = _pico_little_long(md2->numFrames);

	md2->ofsSkins = _pico_little_long(md2->ofsSkins);
	md2->ofsST = _pico_little_long(md2->ofsST);
	md2->ofsTris = _pico_little_long(md2->ofsTris);
	md2->ofsFrames = _pico_little_long(md2->ofsFrames);
	md2->ofsGLCmds = _pico_little_long(md2->ofsGLCmds);
	md2->ofsEnd = _pico_little_long(md2->ofsEnd);

	/* do frame check */
	if (md2->numFrames < 1) {
		_pico_printf(PICO_ERROR, "%s has 0 frames!", fileName);
		return NULL;
	}

	if (frameNum < 0 || frameNum >= md2->numFrames) {
		_pico_printf(PICO_ERROR, "Invalid or out-of-range MD2 frame specified");
		return NULL;
	}

	/* Setup Frame */
	frame = (md2Frame_t *) (bb + md2->ofsFrames);

	/* swap frame scale and translation */
	for (i = 0; i < 3; i++) {
		frame->scale[i] = _pico_little_float(frame->scale[i]);
		frame->translate[i] = _pico_little_float(frame->translate[i]);
	}

	/* swap triangles */
	triangle = (md2Triangle_t *) ((picoByte_t *) (bb + md2->ofsTris));
	for (i = 0; i < md2->numTris; i++, triangle++) {
		for (j = 0; j < 3; j++) {
			triangle->index_xyz[j] = _pico_little_short(triangle->index_xyz[j]);
			triangle->index_st[j] = _pico_little_short(triangle->index_st[j]);
		}
	}

	/* swap st coords */
	texCoord = (md2St_t*) ((picoByte_t *) (bb + md2->ofsST));
	for (i = 0; i < md2->numST; i++, texCoord++) {
		texCoord->s = _pico_little_short(texCoord->s);
		texCoord->t = _pico_little_short(texCoord->t);
	}

	/* create new pico model */
	picoModel = PicoNewModel();
	if (picoModel == NULL) {
		_pico_printf(PICO_ERROR, "Unable to allocate a new model");
		return NULL;
	}

	/* do model setup */
	PicoSetModelFrameNum(picoModel, frameNum);
	PicoSetModelNumFrames(picoModel, md2->numFrames); /* sea */
	PicoSetModelName(picoModel, fileName);
	PicoSetModelFileName(picoModel, fileName);

	for (i = 0; i < md2->numSkins; i++) {
		char *offsetSkin = (char*) (bb + md2->ofsSkins) + i * MD2_MAX_SKINNAME;
		/* set Skin Name */
		strncpy(skinname, offsetSkin, MD2_MAX_SKINNAME);

		/* detox Skin name */
		if (skinname[0] == '.') {/* special case ufoai skinpath */
			char path[MD2_MAX_SKINNAME];
			char skinnameRelative[MD2_MAX_SKINNAME];
			strncpy(path, fileName, MD2_MAX_SKINNAME);
			strncpy(skinnameRelative, skinname, MD2_MAX_SKINNAME);
			_pico_unixify(path);
			for (j = MD2_MAX_SKINNAME; j--;) {/* skip filename */
				if (path[j] == '/')
					break;
				path[j] = '\0';
			}
			snprintf(skinname, MD2_MAX_SKINNAME, "%s%s", path, &skinnameRelative[1]);
		}
		_pico_setfext(skinname, "");

		picoShader = PicoNewShader(picoModel);
		if (picoShader == NULL) {
			_pico_printf(PICO_ERROR, "Unable to allocate a new model shader");
			PicoFreeModel(picoModel);
			return NULL;
		}

		PicoSetShaderName(picoShader, skinname);
	}

	/* allocate new pico surface */
	picoSurface = PicoNewSurface(picoModel);
	if (picoSurface == NULL) {
		_pico_printf(PICO_ERROR, "Unable to allocate a new model surface");
		PicoFreeModel(picoModel);
		return NULL;
	}

	PicoSetSurfaceType(picoSurface, PICO_TRIANGLES);
	PicoSetSurfaceName(picoSurface, frame->name);

	/* associate current surface with newly created shader */
	PicoSetSurfaceShader(picoSurface, PicoGetModelShader(picoModel, 0));

	/* Build Picomodel */
	_pico_set_color(color, 255, 255, 255, 255);

	isw = 1.0 / (double)md2->skinWidth;

	/* load triangle lists */
	triangle = (md2Triangle_t *) ((picoByte_t *) bb + md2->ofsTris);
	texCoord = (md2St_t *) ((picoByte_t *) bb + md2->ofsST);

	for (i = 0; i < md2->numTris; i++) {
		for (j = 0; j < 3; j++) {
			tempIndex[i * 3 + j] = triangle[i].index_xyz[j];
			tempSTIndex[i * 3 + j] = triangle[i].index_st[j];
		}
	}

	/* build list of unique vertices */
	numIndexes = md2->numTris * 3;
	numVerts = 0;
	outIndex = (int *) _pico_alloc(sizeof(int) * numIndexes);

	for (i = 0; i < numIndexes; i++)
		indRemap[i] = -1;

	for (i = 0; i < numIndexes; i++) {
		if (indRemap[i] != -1)
			continue;

		/* remap duplicates */
		for (j = i + 1; j < numIndexes; j++) {
			if (tempIndex[j] != tempIndex[i])
				continue;
			if (texCoord[tempSTIndex[j]].s != texCoord[tempSTIndex[i]].s
			 || texCoord[tempSTIndex[j]].t != texCoord[tempSTIndex[i]].t)
				continue;

			indRemap[j] = i;
			outIndex[j] = numVerts;
		}

		/* add unique vertex */
		indRemap[i] = i;
		outIndex[i] = numVerts++;
	}

	for (i = 0; i < numIndexes; i++) {
		if (indRemap[i] == i)
			continue;

		outIndex[i] = outIndex[indRemap[i]];
	}

	for (j = 0; j < numIndexes; j++) {
		const int index = outIndex[j];
		picoVec3_t xyz, normal;
		picoVec2_t st;

		st[0] = (float)(((double)texCoord[tempSTIndex[indRemap[j]]].s) + 0.5) * isw;
		st[1] = (float)(((double)texCoord[tempSTIndex[indRemap[j]]].t) + 0.5) * isw;

		xyz[0] = (int16_t)frame->verts[tempIndex[indRemap[j]]].v[0] * frame->scale[0] + frame->translate[0];
		xyz[1] = (int16_t)frame->verts[tempIndex[indRemap[j]]].v[1] * frame->scale[1] + frame->translate[1];
		xyz[2] = (int16_t)frame->verts[tempIndex[indRemap[j]]].v[2] * frame->scale[2] + frame->translate[2];

		/* set normal */
		normal[0] = md2_normals[frame->verts[tempIndex[indRemap[j]]].lightnormalindex][0];
		normal[1] = md2_normals[frame->verts[tempIndex[indRemap[j]]].lightnormalindex][1];
		normal[2] = md2_normals[frame->verts[tempIndex[indRemap[j]]].lightnormalindex][2];

		PicoSetSurfaceIndex(picoSurface, j, index);
		PicoSetSurfaceXYZ(picoSurface, index, xyz);
		PicoSetSurfaceNormal(picoSurface, index, normal);
		PicoSetSurfaceST(picoSurface, 0, index, st);
		/* set color */
		PicoSetSurfaceColor(picoSurface, 0, index, color);
	}

	/* Free malloc'ed LUTs */
	_pico_free(outIndex);

	/* return the new pico model */
	return picoModel;
}

/* pico file format module definition */
const picoModule_t picoModuleMD2 = { "0.875", /* module version string */
"Quake 2 MD2", /* module display name */
"Nurail", /* author's name */
"2003 Nurail", /* module copyright */
{ "md2", NULL, NULL, NULL /* default extensions to use */
}, _md2_canload, /* validation routine */
_md2_load, /* load routine */
NULL, /* save validation routine */
NULL /* save routine */
};
