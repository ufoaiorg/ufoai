/**
 * @file
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
#include "r_misc.h"
#include "r_error.h"
#include "r_font.h"
#include "r_model.h"
#include "../../shared/images.h"

static const byte gridtexture[8][8] = {
	{1, 1, 1, 1, 1, 1, 1, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 1, 1, 1, 1, 1, 1, 1},
};

static const byte dummytexture[4] = {255, 255, 255, 255};

#define MISC_TEXTURE_SIZE 16
void R_InitMiscTexture (void)
{
	int x, y;
	byte data[MISC_TEXTURE_SIZE][MISC_TEXTURE_SIZE][4];

	OBJZERO(data);

	/* also use this for bad textures, but without alpha */
	for (x = 0; x < 8; x++) {
		for (y = 0; y < 8; y++) {
			data[y][x][0] = gridtexture[x][y] * 255;
			data[y][x][3] = 255;
		}
	}
	r_noTexture = R_LoadImageData("***r_notexture***", (byte*) data, 8, 8, it_effect);

	for (x = 0; x < MISC_TEXTURE_SIZE; x++) {
		for (y = 0; y < MISC_TEXTURE_SIZE; y++) {
			data[y][x][0] = rand() % 255;
			data[y][x][1] = rand() % 255;
			data[y][x][2] = rand() % 48;
			data[y][x][3] = rand() % 48;
		}
	}
	r_warpTexture = R_LoadImageData("***r_warptexture***", (byte*)data, MISC_TEXTURE_SIZE, MISC_TEXTURE_SIZE, it_effect);

	/* 1x1 pixel white texture to be used when texturing is required, but texture is not available */
	r_dummyTexture = R_LoadImageData("***r_dummytexture***", dummytexture, 1, 1, it_effect);

	/* empty pic in the texture chain for cinematic frames */
	R_LoadImageData("***cinematic***", nullptr, VID_NORM_WIDTH, VID_NORM_HEIGHT, it_pic);
}

/*
==============================================================================
SCREEN SHOTS
==============================================================================
*/

enum {
	SSHOTTYPE_JPG,
	SSHOTTYPE_PNG,
	SSHOTTYPE_TGA,
	SSHOTTYPE_TGA_COMP
};

/**
 * Take a screenshot of the frame buffer
 * @param[in] x
 * @param[in] y
 * @param[in] width
 * @param[in] height
 * @param[in] filename Force to use a filename. Else nullptr to autogen a filename
 * @param[in] ext Force to use an image format (tga/png/jpg). Else nullptr to use value of r_screenshot_format
 */
void R_ScreenShot (int x, int y, int width, int height, const char* filename, const char* ext)
{
	char	checkName[MAX_OSPATH];
	int		type, quality = 100;
	int rowPack;

	glGetIntegerv(GL_PACK_ALIGNMENT, &rowPack);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	/* Find out what format to save in */
	if (ext == nullptr)
		ext = r_screenshot_format->string;

	if (!Q_strcasecmp(ext, "png"))
		type = SSHOTTYPE_PNG;
	else if (!Q_strcasecmp(ext, "jpg"))
		type = SSHOTTYPE_JPG;
	else
		type = SSHOTTYPE_TGA_COMP;

	/* Set necessary values */
	switch (type) {
	case SSHOTTYPE_TGA_COMP:
		Com_Printf("Taking TGA screenshot...\n");
		ext = "tga";
		break;

	case SSHOTTYPE_PNG:
		Com_Printf("Taking PNG screenshot...\n");
		ext = "png";
		break;

	case SSHOTTYPE_JPG:
		if (Cmd_Argc() == 3)
			quality = atoi(Cmd_Argv(2));
		else
			quality = r_screenshot_jpeg_quality->integer;
		if (quality > 100 || quality <= 0)
			quality = 100;

		Com_Printf("Taking JPG screenshot (at %i%% quality)...\n", quality);
		ext = "jpg";
		break;
	}

	/* Find a file name to save it to */
	if (filename) {
		Com_sprintf(checkName, sizeof(checkName), "scrnshot/%s.%s", filename, ext);
	} else {
		int shotNum;
		for (shotNum = 0; shotNum < 1000; shotNum++) {
			Com_sprintf(checkName, sizeof(checkName), "scrnshot/ufo%i%i.%s", shotNum / 10, shotNum % 10, ext);
			if (FS_CheckFile("%s", checkName) == -1)
				break;
		}
		if (shotNum == 1000) {
			Com_Printf("R_ScreenShot_f: screenshot limit (of 1000) exceeded!\n");
			return;
		}
	}

	/* Open it */
	ScopedFile f;
	FS_OpenFile(checkName, &f, FILE_WRITE);
	if (!f) {
		Com_Printf("R_ScreenShot_f: Couldn't create file: %s\n", checkName);
		return;
	}

	/* Allocate room for a copy of the framebuffer */
	byte* const buffer = Mem_PoolAllocTypeN(byte, width * height * 3, vid_imagePool);
	if (!buffer) {
		Com_Printf("R_ScreenShot_f: Could not allocate %i bytes for screenshot!\n", width * height * 3);
		return;
	}

	/* Read the framebuffer into our storage */
	glReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	R_CheckError();

	/* Write */
	switch (type) {
	case SSHOTTYPE_TGA_COMP:
		R_WriteCompressedTGA(&f, buffer, width, height);
		break;

	case SSHOTTYPE_PNG:
		R_WritePNG(&f, buffer, width, height);
		break;

	case SSHOTTYPE_JPG:
		R_WriteJPG(&f, buffer, width, height, quality);
		break;
	}

	/* Finish */
	Mem_Free(buffer);

	Com_Printf("Wrote %s to %s\n", checkName, FS_Gamedir());
	glPixelStorei(GL_PACK_ALIGNMENT, rowPack);
}

void R_ScreenShot_f (void)
{
	const char* ext = nullptr;
	if (Cmd_Argc() > 1)
		ext = Cmd_Argv(1);
	R_ScreenShot(0, 0, viddef.context.width, viddef.context.height, nullptr, ext);
}

/**
 * @brief Perform translate, rotate and scale operations on the current matrix
 * @note Every parameter may be @c nullptr and is ignored then
 * @param[in] transform Translation vector
 * @param[in] rotate Rotation vector
 * @param[in] scale Scale vector (keep in mind to not set this to @c vec3_origin or zero)
 * @sa R_PushMatrix
 * @sa R_PopMatrix
 */
void R_Transform (const vec3_t transform, const vec3_t rotate, const vec3_t scale)
{
	if (transform != nullptr) {
		glTranslatef(transform[0], transform[1], transform[2]);
	}

	if (rotate != nullptr) {
		glRotatef(rotate[0], 0, 0, 1);
		glRotatef(rotate[1], 0, 1, 0);
		glRotatef(rotate[2], 1, 0, 0);
	}

	if (scale != nullptr) {
		glScalef(scale[0], scale[1], scale[2]);
	}
}

/**
 * @brief Push a new matrix to the stack
 */
void R_PushMatrix (void)
{
	glPushMatrix();
}

/**
 * @brief Removes the current matrix from the stack
 */
void R_PopMatrix (void)
{
	glPopMatrix();
}

/**
 * @brief Dumps OpenGL state for debugging - typically every capability set with glEnable().
 */
void R_DumpOpenGlState (void)
{
#define CAPABILITY( X ) {GL_ ## X, # X}
	/* List taken from here: http://www.khronos.org/opengles/sdk/1.1/docs/man/glIsEnabled.xml */
	const struct { GLenum idx; const char*  text; } openGLCaps[] = {
		CAPABILITY(ALPHA_TEST),
		CAPABILITY(BLEND),
		CAPABILITY(COLOR_ARRAY),
		CAPABILITY(COLOR_LOGIC_OP),
		CAPABILITY(COLOR_MATERIAL),
		CAPABILITY(CULL_FACE),
		CAPABILITY(DEPTH_TEST),
		CAPABILITY(DITHER),
		CAPABILITY(FOG),
		CAPABILITY(LIGHTING),
		CAPABILITY(LINE_SMOOTH),
		CAPABILITY(MULTISAMPLE),
		CAPABILITY(NORMAL_ARRAY),
		CAPABILITY(NORMALIZE),
		CAPABILITY(POINT_SMOOTH),
		CAPABILITY(POLYGON_OFFSET_FILL),
		CAPABILITY(RESCALE_NORMAL),
		CAPABILITY(SAMPLE_ALPHA_TO_COVERAGE),
		CAPABILITY(SAMPLE_ALPHA_TO_ONE),
		CAPABILITY(SAMPLE_COVERAGE),
		CAPABILITY(SCISSOR_TEST),
		CAPABILITY(STENCIL_TEST),
		CAPABILITY(VERTEX_ARRAY)
	};
#undef CAPABILITY

	char s[1024] = "";
	GLint i;
	GLint maxTexUnits = 0;
	GLint activeTexUnit = 0;
	GLint activeClientTexUnit = 0;
	GLint activeTexId = 0;
	GLfloat texEnvMode = 0;
	const char*  texEnvModeStr = "UNKNOWN";
	GLfloat color[4];

	for (int i = 0; i < lengthof(openGLCaps); i++) {
		if (glIsEnabled(openGLCaps[i].idx)) {
			Q_strcat(s, sizeof(s), "%s ", openGLCaps[i].text);
		}
	}
	glGetFloatv(GL_CURRENT_COLOR, color);

	Com_Printf("OpenGL enabled caps: %s color %f %f %f %f \n", s, color[0], color[1], color[2], color[3]);

	glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexUnit);
	glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &activeClientTexUnit);

	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxTexUnits);
	for (i = GL_TEXTURE0; i < GL_TEXTURE0 + maxTexUnits; i++) {
		qglActiveTexture(i);
		qglClientActiveTexture(i);

		strcpy(s, "");
		if (glIsEnabled (GL_TEXTURE_2D))
			strcat(s, "enabled, ");
		if (glIsEnabled (GL_TEXTURE_COORD_ARRAY))
			strcat(s, "with texcoord array, ");
		if (i == activeTexUnit)
			strcat(s, "active, ");
		if (i == activeClientTexUnit)
			strcat(s, "client active, ");

		glGetIntegerv(GL_TEXTURE_BINDING_2D, &activeTexId);
		glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &texEnvMode);
		if (fabs(texEnvMode - GL_ADD) < 0.1f)
			texEnvModeStr = "ADD";
		if (fabs(texEnvMode - GL_MODULATE) < 0.1f)
			texEnvModeStr = "MODULATE";
		if (fabs(texEnvMode - GL_DECAL) < 0.1f)
			texEnvModeStr = "DECAL";
		if (fabs(texEnvMode - GL_BLEND) < 0.1f)
			texEnvModeStr = "BLEND";
		if (fabs(texEnvMode - GL_REPLACE) < 0.1f)
			texEnvModeStr = "REPLACE";
		if (fabs(texEnvMode - GL_COMBINE) < 0.1f)
			texEnvModeStr = "COMBINE";

		Com_Printf("Texunit: %d texID %d %s texEnv mode %s\n", i - GL_TEXTURE0, activeTexId, s, texEnvModeStr);
	}

	qglActiveTexture(activeTexUnit);
	qglClientActiveTexture(activeClientTexUnit);
}

/**
 * @brief Re-initializes OpenGL state machine, all textures and renderer variables, this needed when application is put to background on Android.
 */
void R_ReinitOpenglContext (void)
{
	/* De-allocate old GL state, these functinos will call glDeleteTexture(), so they should go before everything else */
	R_FontCleanCache();
	R_ShutdownFBObjects();
	R_ShutdownPrograms();
	R_BeginBuildingLightmaps(); /* This function will also call glDeleteTexture() */
	/* Re-initialize GL state */
	R_SetDefaultState();
	R_InitPrograms();
	R_ResetArrayState();
	/* Re-upload all textures */
	R_InitMiscTexture();
	R_ReloadImages();
	/* Re-upload other GL stuff */
	R_InitFBObjects();
	R_UpdateDefaultMaterial("", "", "", nullptr);

	/* Re-upload the battlescape terrain geometry */
	if (!qglBindBuffer)
		return;

	for (int tile = 0; tile < r_numMapTiles; tile++) {
		model_t* mod = r_mapTiles[tile];

		int vertind = 0, coordind = 0, tangind = 0;
		mBspSurface_t* surf = mod->bsp.surfaces;

		for (int i = 0; i < mod->bsp.numsurfaces; i++, surf++) {
			vertind += 3 * surf->numedges;
			coordind += 2 * surf->numedges;
			tangind += 4 * surf->numedges;
		}

		qglGenBuffers(1, &mod->bsp.vertex_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.vertex_buffer);
		qglBufferData(GL_ARRAY_BUFFER, vertind * sizeof(GLfloat), mod->bsp.verts, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.texcoord_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.texcoord_buffer);
		qglBufferData(GL_ARRAY_BUFFER, coordind * sizeof(GLfloat), mod->bsp.texcoords, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.lmtexcoord_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.lmtexcoord_buffer);
		qglBufferData(GL_ARRAY_BUFFER, coordind * sizeof(GLfloat), mod->bsp.lmtexcoords, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.normal_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.normal_buffer);
		qglBufferData(GL_ARRAY_BUFFER, vertind * sizeof(GLfloat), mod->bsp.normals, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.tangent_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.tangent_buffer);
		qglBufferData(GL_ARRAY_BUFFER, tangind * sizeof(GLfloat), mod->bsp.tangents, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.index_buffer);
		qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mod->bsp.index_buffer);
		qglBufferData(GL_ELEMENT_ARRAY_BUFFER, mod->bsp.numIndexes * sizeof(glElementIndex_t), mod->bsp.indexes, GL_STATIC_DRAW);
		qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		for (int i = 0; i < mod->bsp.numsurfaces; i++)
			R_CreateSurfaceLightmap(&mod->bsp.surfaces[i]);
	}

	R_EndBuildingLightmaps();
	qglBindBuffer(GL_ARRAY_BUFFER, 0);
}
