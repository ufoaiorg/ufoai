/**
 * @file r_image.c
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

#include "r_local.h"

/**
 * @brief
 */
void R_SetDefaultState (void)
{
	qglCullFace(GL_FRONT);

	qglEnable(GL_TEXTURE_2D);

	qglAlphaFunc(GL_GREATER, 0.1f);

	qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	qglClearColor(0, 0, 0, 0);

	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* doesn't really belong here... but works fine */
	viddef.rx = (float) viddef.width / VID_NORM_WIDTH;
	viddef.ry = (float) viddef.height / VID_NORM_HEIGHT;
}

/**
 * @brief
 * @sa R_SetupGL3D
 */
static void MYgluPerspective (GLdouble zNear, GLdouble zFar)
{
	GLdouble xmin, xmax, ymin, ymax, yaspect = (double) r_newrefdef.height / r_newrefdef.width;

	if (r_isometric->integer) {
		qglOrtho(-10 * r_newrefdef.fov_x, 10 * r_newrefdef.fov_x, -10 * r_newrefdef.fov_x * yaspect, 10 * r_newrefdef.fov_x * yaspect, -zFar, zFar);
	} else {
		xmax = zNear * tan(r_newrefdef.fov_x * M_PI / 360.0);
		xmin = -xmax;

		ymin = xmin * yaspect;
		ymax = xmax * yaspect;

		qglFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
	}
}

/**
 * @brief
 * @sa R_SetupGL2D
 */
void R_SetupGL3D (void)
{
	int x, x2, y2, y, w, h;

	/* set up viewport */
	x = floor(r_newrefdef.x * viddef.width / viddef.width);
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * viddef.width / viddef.width);
	y = floor(viddef.height - r_newrefdef.y * viddef.height / viddef.height);
	y2 = ceil(viddef.height - (r_newrefdef.y + r_newrefdef.height) * viddef.height / viddef.height);

	w = x2 - x;
	h = y - y2;

	qglViewport(x, y2, w, h);

	/* set up projection matrix */
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	MYgluPerspective(4.0, 4096.0);

	qglCullFace(GL_FRONT);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	qglRotatef(-90, 1, 0, 0);	/* put Z going up */
	qglRotatef(90, 0, 0, 1);	/* put Z going up */
	qglRotatef(-r_newrefdef.viewangles[2], 1, 0, 0);
	qglRotatef(-r_newrefdef.viewangles[0], 0, 1, 0);
	qglRotatef(-r_newrefdef.viewangles[1], 0, 0, 1);
	qglTranslatef(-r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2]);

	qglGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

	/* set drawing parms */
	qglEnable(GL_CULL_FACE);

	RSTATE_DISABLE_BLEND
	RSTATE_DISABLE_ALPHATEST
	qglEnable(GL_DEPTH_TEST);

#if 0
	qglHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
	qglHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);
#endif

	if (r_fog->integer && r_newrefdef.fog && r_state.fog_coord) {
		qglEnable(GL_FOG);
		qglFogi(GL_FOG_MODE, GL_EXP2);
		qglFogf(GL_FOG_START, 0.1 * r_newrefdef.fog);
		qglFogf(GL_FOG_END, r_newrefdef.fog);
		qglHint(GL_FOG_HINT, GL_DONT_CARE);
		qglFogf(GL_FOG_DENSITY, 0.005 * r_newrefdef.fog );
		qglFogfv(GL_FOG_COLOR, r_newrefdef.fogColor);
	}
}

/**
 * @brief
 * @sa R_SetupGL3D
 */
void R_SetupGL2D (void)
{
	/* set 2D virtual screen size */
	qglViewport(0, 0, viddef.width, viddef.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, viddef.width, viddef.height, 0, 9999, -9999);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);
	RSTATE_DISABLE_BLEND
	qglDisable(GL_FOG);
	RSTATE_ENABLE_ALPHATEST
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	R_TexEnv(GL_MODULATE);
	qglColor4f(1, 1, 1, 1);
}

/**
 * @brief
 */
void R_EnableMultitexture (qboolean enable)
{
	if (!qglSelectTextureSGIS && !qglActiveTextureARB)
		return;

	if (enable == r_state.multitexture)
		return;

	r_state.multitexture = enable;

	if (enable) {
		R_SelectTexture(gl_texture1);
		qglEnable(GL_TEXTURE_2D);
		R_TexEnv(GL_REPLACE);
	} else {
		R_SelectTexture(gl_texture1);
		qglDisable(GL_TEXTURE_2D);
		R_TexEnv(GL_REPLACE);
	}
	R_SelectTexture(gl_texture0);
	R_TexEnv(GL_REPLACE);
}

/**
 * @brief
 */
void R_SelectTexture (GLenum texture)
{
	int tmu;

	if (!qglSelectTextureSGIS && !qglActiveTextureARB)
		return;

	if (texture == gl_texture0)
		tmu = 0;
	else
		tmu = 1;

	if (tmu == r_state.currenttmu)
		return;

	r_state.currenttmu = tmu;

	if (qglSelectTextureSGIS) {
		qglSelectTextureSGIS(texture);
	} else if (qglActiveTextureARB) {
		qglActiveTextureARB(texture);
		qglClientActiveTextureARB(texture);
	}
}

/**
 * @brief
 */
void R_TexEnv (GLenum mode)
{
	static GLenum lastmodes[2] = { (GLenum) - 1, (GLenum) - 1 };

	if (mode != lastmodes[r_state.currenttmu]) {
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
		lastmodes[r_state.currenttmu] = mode;
	}
}

/**
 * @brief
 */
void R_Bind (int texnum)
{
	if (r_state.currenttextures[r_state.currenttmu] == texnum)
		return;
	r_state.currenttextures[r_state.currenttmu] = texnum;
	qglBindTexture(GL_TEXTURE_2D, texnum);
}

/**
 * @brief
 */
void R_MBind (GLenum target, int texnum)
{
	R_SelectTexture(target);
	if (target == gl_texture0) {
		if (r_state.currenttextures[0] == texnum)
			return;
	} else {
		if (r_state.currenttextures[1] == texnum)
			return;
	}
	R_Bind(texnum);
}

/*============================================================================*/

typedef struct {
	const char *name;
	int mode;
} gltmode_t;

static const gltmode_t gl_alpha_modes[] = {
	{"default", 4},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

#define NUM_R_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof (gltmode_t))

/**
 * @brief
 */
void R_TextureAlphaMode (const char *string)
{
	int i;

	for (i = 0; i < NUM_R_ALPHA_MODES; i++) {
		if (!Q_stricmp(gl_alpha_modes[i].name, string))
			break;
	}

	if (i == NUM_R_ALPHA_MODES) {
		Com_Printf("bad alpha texture mode name\n");
		return;
	}

	gl_alpha_format = gl_alpha_modes[i].mode;
}

static const gltmode_t gl_solid_modes[] = {
	{"default", 3},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
#ifdef GL_RGB2_EXT
	{"GL_RGB2", GL_RGB2_EXT},
#endif
};

#define NUM_R_SOLID_MODES (sizeof(gl_solid_modes) / sizeof (gltmode_t))

/**
 * @brief
 */
void R_TextureSolidMode (const char *string)
{
	int i;

	for (i = 0; i < NUM_R_SOLID_MODES; i++) {
		if (!Q_stricmp(gl_solid_modes[i].name, string))
			break;
	}

	if (i == NUM_R_SOLID_MODES) {
		Com_Printf("bad solid texture mode name\n");
		return;
	}

	gl_solid_format = gl_solid_modes[i].mode;
}
