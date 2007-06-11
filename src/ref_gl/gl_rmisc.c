/**
 * @file gl_rmisc.c
 * @brief
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

static const byte dottexture[8][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 1, 1, 0, 0, 0, 0},
	{0, 1, 1, 1, 1, 0, 0, 0},
	{0, 1, 1, 1, 1, 0, 0, 0},
	{0, 0, 1, 1, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
};

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

/**
 * @brief
 */
void R_InitParticleTexture (void)
{
	int x, y;
	byte data[8][8][4];

	/* particle texture */
	for (x = 0; x < 8; x++) {
		for (y = 0; y < 8; y++) {
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] * 255;
		}
	}
	r_particletexture = GL_LoadPic("***particle***", (byte *) data, 8, 8, it_sprite, 32);

	/* also use this for bad textures, but without alpha */
	for (x = 0; x < 8; x++) {
		for (y = 0; y < 8; y++) {
			data[y][x][0] = gridtexture[x][y] * 255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}
	r_notexture = GL_LoadPic("***r_notexture***", (byte *) data, 8, 8, it_wall, 32);
}


/*
==============================================================================
SCREEN SHOTS
==============================================================================
*/

typedef struct _TargaHeader {
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

enum {
	SSHOTTYPE_JPG,
	SSHOTTYPE_PNG,
	SSHOTTYPE_TGA,
};

/**
 * @brief
 */
extern void GL_ScreenShot_f (void)
{
	char	checkName[MAX_OSPATH];
	int		type, shotNum, quality = 100;
	char	*ext;
	byte	*buffer;
	FILE	*f;

	/* Find out what format to save in */
	if (ri.Cmd_Argc() > 1)
		ext = ri.Cmd_Argv(1);
	else
		ext = gl_screenshot->string;

	if (!Q_stricmp (ext, "png"))
		type = SSHOTTYPE_PNG;
	else if (!Q_stricmp (ext, "jpg"))
		type = SSHOTTYPE_JPG;
	else
		type = SSHOTTYPE_TGA;

	/* Set necessary values */
	switch (type) {
	case SSHOTTYPE_TGA:
		ri.Con_Printf(PRINT_ALL, "Taking TGA screenshot...\n");
		ext = "tga";
		break;

	case SSHOTTYPE_PNG:
		ri.Con_Printf(PRINT_ALL, "Taking PNG screenshot...\n");
		ext = "png";
		break;

	case SSHOTTYPE_JPG:
		if (ri.Cmd_Argc() == 3)
			quality = atoi(ri.Cmd_Argv(2));
		else
			quality = gl_screenshot_jpeg_quality->integer;
		if (quality > 100 || quality <= 0)
			quality = 100;

		ri.Con_Printf(PRINT_ALL, "Taking JPG screenshot (at %i%% quality)...\n", quality);
		ext = "jpg";
		break;
	}

	/* Find a file name to save it to */
	for (shotNum = 0; shotNum < 1000; shotNum++) {
		Com_sprintf(checkName, MAX_OSPATH, "%s/scrnshot/ufo%i%i.%s", ri.FS_Gamedir(), shotNum / 10, shotNum % 10, ext);
		f = fopen(checkName, "rb");
		if (!f)
			break;
		fclose (f);
	}

	ri.FS_CreatePath(checkName);

	/* Open it */
	f = fopen(checkName, "wb");

	if (!f) {
		ri.Con_Printf(PRINT_ALL, "GL_ScreenShot_f: Couldn't create file: %s\n", checkName);
		return;
	}

	if (shotNum == 1000) {
		ri.Con_Printf(PRINT_ALL, "GL_ScreenShot_f: screenshot limit (of 1000) exceeded!\n");
		fclose(f);
		return;
	}

	/* Allocate room for a copy of the framebuffer */
	buffer = malloc(vid.width * vid.height * 3);
	if (!buffer) {
		ri.Con_Printf(PRINT_ALL, "GL_ScreenShot_f: Could not allocate %i bytes for screenshot!\n", vid.width * vid.height * 3);
		fclose(f);
		return;
	}

	/* Read the framebuffer into our storage */
	qglReadPixels(0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

	/* Write */
	switch (type) {
	case SSHOTTYPE_TGA:
		WriteTGA(f, buffer, vid.width, vid.height);
		break;

	case SSHOTTYPE_PNG:
		WritePNG(f, buffer, vid.width, vid.height);
		break;

	case SSHOTTYPE_JPG:
		WriteJPG(f, buffer, vid.width, vid.height, quality);
		break;
	}

	/* Finish */
	fclose(f);
	free(buffer);

	ri.Con_Printf(PRINT_ALL, "Wrote %s\n", checkName);
}


/**
 * @brief
 */
void GL_SetDefaultState (void)
{
	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.1f);
	gl_state.alpha_test = qtrue;

	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_BLEND);
	gl_state.blend = qfalse;

	qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	qglClearColor(0, 0, 0, 0);

	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	GL_TextureMode(gl_texturemode->string);
	GL_TextureAlphaMode(gl_texturealphamode->string);
	GL_TextureSolidMode(gl_texturesolidmode->string);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_TexEnv(GL_REPLACE);

	if (qglPointParameterfEXT) {
		float attenuations[3];

		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;

		qglEnable(GL_POINT_SMOOTH);
		qglPointParameterfEXT(GL_POINT_SIZE_MIN_EXT, gl_particle_min_size->value);
		qglPointParameterfEXT(GL_POINT_SIZE_MAX_EXT, gl_particle_max_size->value);
		qglPointParameterfvEXT(GL_DISTANCE_ATTENUATION_EXT, attenuations);
	}

	GL_UpdateSwapInterval();

	/* doesn't really belong here... but works fine */
	vid.rx = (float) vid.width / VID_NORM_WIDTH;
	vid.ry = (float) vid.height / VID_NORM_HEIGHT;
}

/**
 * @brief
 */
void GL_UpdateSwapInterval(void)
{
	if (gl_swapinterval->modified) {
		gl_swapinterval->modified = qfalse;

#ifdef _WIN32
		if (!gl_state.stereo_enabled) {
			if (qwglSwapIntervalEXT)
				qwglSwapIntervalEXT(gl_swapinterval->value);
		}
#endif
	}
}


/*
==============================================================================
SOME DRAWING
==============================================================================
*/


/**
 * @brief Draws the field marker entity is specified in cl_actor.c CL_AddTargeting
 * @sa CL_AddTargeting
 */
void R_DrawBox (entity_t * e)
{
	vec3_t upper, lower;
	float dx, dy;

/*	if (R_CullBox(e->origin, e->oldorigin)) */
/*		return; */

	qglDepthMask(GL_FALSE);
	GLSTATE_ENABLE_BLEND
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_TEXTURE_2D);
	if (!gl_wire->value)
		qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	qglEnable(GL_LINE_SMOOTH);

	qglColor4f(e->angles[0], e->angles[1], e->angles[2], e->alpha);

	VectorCopy(e->origin, lower);
	VectorCopy(e->origin, upper);
	upper[2] = e->oldorigin[2];

	dx = e->oldorigin[0] - e->origin[0];
	dy = e->oldorigin[1] - e->origin[1];

	qglBegin(GL_QUAD_STRIP);

	qglVertex3fv(lower);
	qglVertex3fv(upper);
	lower[0] += dx;
	upper[0] += dx;
	qglVertex3fv(lower);
	qglVertex3fv(upper);
	lower[1] += dy;
	upper[1] += dy;
	qglVertex3fv(lower);
	qglVertex3fv(upper);
	lower[0] -= dx;
	upper[0] -= dx;
	qglVertex3fv(lower);
	qglVertex3fv(upper);
	lower[1] -= dy;
	upper[1] -= dy;
	qglVertex3fv(lower);
	qglVertex3fv(upper);

	qglEnd();

	qglDisable(GL_LINE_SMOOTH);
	if (!gl_wire->value)
		qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	qglEnable(GL_TEXTURE_2D);
	qglEnable(GL_CULL_FACE);
	GLSTATE_DISABLE_BLEND
	qglDepthMask(GL_TRUE);
}
