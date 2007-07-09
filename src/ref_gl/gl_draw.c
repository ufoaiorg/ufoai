/**
 * @file gl_draw.c
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

extern cvar_t *gl_drawclouds;
extern qboolean scrap_dirty;
void Scrap_Upload(void);
image_t *shadow;
image_t *blood;

static float globe_fog[4];
int spherelist;
#define GLOBE_TRIS 60
#define GLOBE_TEXES (GLOBE_TRIS+1)*(GLOBE_TRIS+1)*4
#define GLOBE_VERTS (GLOBE_TRIS+1)*(GLOBE_TRIS+1)*6
static float globetexes[GLOBE_TEXES];
static float globeverts[GLOBE_VERTS];
/**
 * @brief Draw a sphere! The verts and texcoords are precalculated for extra efficiency.
 * @note The sphere is put into a display list to reduce overhead even further.
 */
static void GL_DrawSphere (void)
{
	if (spherelist == -1) {
		int i;
		int j;

		int vertspos = 0;
		int texespos = 0;

		/* build the sphere display list */
		spherelist = qglGenLists(1);

		qglNewList(spherelist, GL_COMPILE);

		qglEnable(GL_NORMALIZE);

		for (i = 0; i < GLOBE_TRIS; i++) {
			qglBegin(GL_TRIANGLE_STRIP);

			for (j = 0; j <= GLOBE_TRIS; j++) {
				qglNormal3fv(&globeverts[vertspos]);
				qglTexCoord2fv(&globetexes[texespos]);
				qglVertex3fv(&globeverts[vertspos]);

				texespos += 2;
				vertspos += 3;

				qglNormal3fv(&globeverts[vertspos]);
				qglTexCoord2fv(&globetexes[texespos]);
				qglVertex3fv(&globeverts[vertspos]);

				texespos += 2;
				vertspos += 3;
			}

			qglEnd();
		}

		qglDisable(GL_NORMALIZE);

		qglEndList();
	}
}

/**
 * @brief Initialize the globe chain arrays
 */
static void GL_InitGlobeChain (void)
{
	const float drho = M_PI / GLOBE_TRIS;
	const float dtheta = M_PI / (GLOBE_TRIS / 2);

	const float ds = 1.0f / (GLOBE_TRIS * 2);
	const float dt = 1.0f / GLOBE_TRIS;

	float t = 1.0f;
	float s = 0.0f;

	int i;
	int j;

	int vertspos = 0;
	int texespos = 0;

	for (i = 0; i < GLOBE_TRIS; i++) {
		float rho = (float) i * drho;
		float srho = (float) (sin (rho));
		float crho = (float) (cos (rho));
		float srhodrho = (float) (sin (rho + drho));
		float crhodrho = (float) (cos (rho + drho));

		s = 0.0f;

		for (j = 0; j <= GLOBE_TRIS; j++) {
			float theta = (j == GLOBE_TRIS) ? 0.0f : j * dtheta;
			float stheta = (float) (-sin( theta));
			float ctheta = (float) (cos (theta));

			globetexes[texespos++] = s;
			globetexes[texespos++] = (t - dt);

			globeverts[vertspos++] = stheta * srhodrho * gl_3dmapradius->value;
			globeverts[vertspos++] = ctheta * srhodrho * gl_3dmapradius->value;
			globeverts[vertspos++] = crhodrho * gl_3dmapradius->value;

			globetexes[texespos++] = s;
			globetexes[texespos++] = t;

			globeverts[vertspos++] = stheta * srho * gl_3dmapradius->value;
			globeverts[vertspos++] = ctheta * srho * gl_3dmapradius->value;
			globeverts[vertspos++] = crho * gl_3dmapradius->value;

			s += ds;
		}

		t -= dt;
	}
	spherelist = -1;
}

/* console font */
image_t *draw_chars[2];

/**
 * @brief
 */
void Draw_InitLocal (void)
{
	shadow = GL_FindImage("pics/sfx/shadow", it_pic);
	if (!shadow)
		ri.Con_Printf(PRINT_ALL, "Could not find shadow image in game pics/sfx directory!\n");
	blood = GL_FindImage("pics/sfx/blood", it_pic);
	if (!blood)
		ri.Con_Printf(PRINT_ALL, "Could not find shadow image in game pics/sfx directory!\n");
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	/* load console characters (don't bilerp characters) */

	draw_chars[0] = GL_FindImage("pics/conchars", it_pic);
	if (!draw_chars[0])
		ri.Sys_Error(ERR_FATAL, "Could not find conchars image in game pics directory!\n");
	draw_chars[1] = GL_FindImage("pics/conchars_small", it_pic);
	if (!draw_chars[1])
		ri.Con_Printf(PRINT_ALL, "Could not find conchars2 image in game pics directory!\n");

	Font_Init();
	GL_InitGlobeChain();
	GL_DrawSphere();
}


/**
 * @brief Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void Draw_Char (int x, int y, int num)
{
	int row, col;
	float frow, fcol;

	num &= 255;

	if ((num & 127) == 32)		/* space */
		return;

	if (y <= -con_fontHeight->integer)
		return;					/* totally off screen */

	row = num >> 4;
	col = num & 15;

	/* 0.0625 => 16 cols (conchars images) */
	frow = row * 0.0625;
	fcol = col * 0.0625;

	assert(con_font->integer < 2);
	GL_Bind(draw_chars[con_font->integer]->texnum);

	qglBegin(GL_QUADS);
	qglTexCoord2f(fcol, frow);
	qglVertex2f(x, y);
	qglTexCoord2f(fcol + 0.0625, frow);
	qglVertex2f(x + con_fontWidth->integer, y);
	qglTexCoord2f(fcol + 0.0625, frow + 0.0625);
	qglVertex2f(x + con_fontWidth->integer, y + con_fontHeight->integer);
	qglTexCoord2f(fcol, frow + 0.0625);
	qglVertex2f(x, y + con_fontHeight->integer);
	qglEnd();
}

/**
 * @brief
 */
void GL_DrawImagePixelData (const char *name, byte *frame, int width, int height)
{
	image_t *img;

	img = GL_FindImage(name, it_pic);
	if (!img)
		ri.Sys_Error(ERR_FATAL, "Could not find the searched image: %s\n", name);

	GL_Bind(img->texnum);

	if (img->width == width && img->height == height)
		qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img->width, img->height, GL_RGBA, GL_UNSIGNED_BYTE, frame);
	else {
		/* Reallocate the texture */
		img->width = width;
		img->height = height;
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame);
	}
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qglColor4ub(255, 255, 255, 255);
	qglBegin(GL_QUADS);
	qglTexCoord2f(0, 0);
	qglVertex2f(0, 0);
	qglTexCoord2f(1, 0);
	qglVertex2f(vid.width, 0);
	qglTexCoord2f(1, 1);
	qglVertex2f(vid.width, vid.height);
	qglTexCoord2f(0, 1);
	qglVertex2f(0, vid.height);
	qglEnd();
}

/**
 * @brief Change the color to given value
 * @param[in] rgba A pointer to a vec4_t with rgba color value
 * @note To reset the color let rgba be NULL
 * @note Enables openGL blend if alpha value is lower than 1.0
 */
void Draw_Color (const float *rgba)
{
	if (rgba) {
		if (rgba[3] < 1.0f)
			GLSTATE_ENABLE_BLEND
		qglColor4fv(rgba);
	} else {
		GLSTATE_DISABLE_BLEND
		qglColor4f(1, 1, 1, 1);
	}
}

/**
 * @brief Searches for an image in the image array
 * @param[in] name The name of the image
 * @note the imagename can contain a / or \ (relative to gamedir/) - otherwise it's relative to gamedir/pics
 * @note name may not be null and has to be longer than 4 chars
 * @return NULL on error or image_t pointer on success
 * @sa GL_FindImage
 */
image_t *Draw_FindPic (const char *name)
{
	image_t *gl;
	char fullname[MAX_QPATH];

	if (name[0] != '*' && name[1] != '*') {
		if (name[0] != '/' && name[0] != '\\')
			Com_sprintf(fullname, sizeof(fullname), "pics/%s", name);
		else
			Q_strncpyz(fullname, name + 1, MAX_QPATH);
	} else
		Q_strncpyz(fullname, name, MAX_QPATH);

	gl = GL_FindImage(fullname, it_pic);
	return gl;
}

/**
 * @brief Fills w and h with the width and height of a given pic
 * @note if w and h are -1 the pic was not found
 * @param[out] w Pointer to int value for width
 * @param[out] h Pointer to int value for height
 * @param[in] pic The name of the pic to get the values for
 * @sa Draw_FindPic
 */
void Draw_GetPicSize (int *w, int *h, const char *pic)
{
	image_t *gl;

	gl = Draw_FindPic(pic);
	if (!gl) {
		*w = *h = -1;
		return;
	}
	*w = gl->width;
	*h = gl->height;
}

/**
 * @brief Draws an image or parts of it
 * @param[in] x X position to draw the image to
 * @param[in] y Y position to draw the image to
 * @param[in] w Width of the image
 * @param[in] h Height of the image
 * @param[in] sh Right x corner coord of the square to draw
 * @param[in] th Lower y corner coord of the square to draw
 * @param[in] sl Left x corner coord of the square to draw
 * @param[in] tl Upper y corner coord of the square to draw
 * @param[in] align The alignment we should use for placing the image onto the screen (see align_t)
 * @param[in] blend Enable the blend mode (for alpha channel images)
 * @param[in] name The name of the image - relative to base/pics
 * @sa Draw_FindPic
 * @note All these parameter are normalized to VID_NORM_WIDTH and VID_NORM_HEIGHT
 * they are adjusted in this function
 */
void Draw_NormPic (float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, qboolean blend, const char *name)
{
	float nx, ny, nw = 0.0, nh = 0.0;
	image_t *gl;

	gl = Draw_FindPic(name);
	if (!gl) {
		ri.Con_Printf(PRINT_ALL, "Can't find pic: %s\n", name);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload();

	/* normalize */
	nx = x * vid.rx;
	ny = y * vid.ry;

	if (w && h) {
		nw = w * vid.rx;
		nh = h * vid.ry;
	}

	if (sh && th) {
		if (!w || !h) {
			nw = (sh - sl) * vid.rx;
			nh = (th - tl) * vid.ry;
		}
		sl = sl / (float)gl->width;
		sh = sh / (float)gl->width;
		tl = tl / (float)gl->height;
		th = th / (float)gl->height;
	} else {
		if (!w || !h) {
			nw = (float)gl->width * vid.rx;
			nh = (float)gl->height * vid.ry;
		}
		sh = 1.0f;
		th = 1.0f;
	}

	/* get alignement */
	/* (do nothing if left aligned or unknown) */
	if (align > 0 && align < ALIGN_LAST) {
		switch (align % 3) {
		case 1:
			nx -= nw * 0.5;
			break;
		case 2:
			nx -= nw;
			break;
		}

		switch (align / 3) {
		case 1:
			ny -= nh * 0.5;
			break;
		case 2:
			ny -= nh;
			break;
		}
	}

	if (blend)
		GLSTATE_ENABLE_BLEND

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qfalse);
#endif

	GL_Bind(gl->texnum);
	qglBegin(GL_QUADS);
	qglTexCoord2f(sl, tl);
	qglVertex2f(nx, ny);
	qglTexCoord2f(sh, tl);
	qglVertex2f(nx + nw, ny);
	qglTexCoord2f(sh, th);
	qglVertex2f(nx + nw, ny + nh);
	qglTexCoord2f(sl, th);
	qglVertex2f(nx, ny + nh);
	qglEnd();

	if (blend)
		GLSTATE_DISABLE_BLEND

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qtrue);
#endif
}


/**
 * @brief
 */
void Draw_Pic (int x, int y, const char *pic)
{
	image_t *gl;

	gl = Draw_FindPic(pic);
	if (!gl) {
		ri.Con_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload();

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qfalse);
#endif
	GL_Bind(gl->texnum);
	qglBegin(GL_QUADS);
	qglTexCoord2f(0, 0);
	qglVertex2f(x, y);
	qglTexCoord2f(1, 0);
	qglVertex2f(x + gl->width, y);
	qglTexCoord2f(1, 1);
	qglVertex2f(x + gl->width, y + gl->height);
	qglTexCoord2f(0, 1);
	qglVertex2f(x, y + gl->height);
	qglEnd();
#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qtrue);
#endif
}

/**
 * @brief This repeats a 64*64 tile graphic to fill the screen around a sized down refresh window.
 */
void Draw_TileClear (int x, int y, int w, int h, const char *name)
{
	image_t *image;

	image = Draw_FindPic(name);
	if (!image) {
		ri.Con_Printf(PRINT_ALL, "Can't find pic: %s\n", name);
		return;
	}

#ifdef HAVE_SHADERS
	if (image->shader)
		SH_UseShader(image->shader, qfalse);
#endif
	GL_Bind(image->texnum);
	qglBegin(GL_QUADS);
	qglTexCoord2f(x / 64.0, y / 64.0);
	qglVertex2f(x, y);
	qglTexCoord2f((x + w) / 64.0, y / 64.0);
	qglVertex2f(x + w, y);
	qglTexCoord2f((x + w) / 64.0, (y + h) / 64.0);
	qglVertex2f(x + w, y + h);
	qglTexCoord2f(x / 64.0, (y + h) / 64.0);
	qglVertex2f(x, y + h);
	qglEnd();
#ifdef HAVE_SHADERS
	if (image->shader)
		SH_UseShader(image->shader, qtrue);
#endif
}


/**
 * @brief Fills a box of pixels with a single color
 */
void Draw_Fill (int x, int y, int w, int h, int align, const vec4_t color)
{
	float nx, ny, nw, nh;

	nx = x * vid.rx;
	ny = y * vid.ry;
	nw = w * vid.rx;
	nh = h * vid.ry;

	Draw_Color(color);

	qglDisable(GL_TEXTURE_2D);
	qglBegin(GL_QUADS);

	switch (align) {
	case ALIGN_CL:
		qglVertex2f(nx, ny);
		qglVertex2f(nx + nh, ny);
		qglVertex2f(nx + nh, ny - nw);
		qglVertex2f(nx, ny - nw);
		break;
	case ALIGN_CC:
		qglVertex2f(nx, ny);
		qglVertex2f(nx + nh, ny - nh);
		qglVertex2f(nx + nh, ny - nw - nh);
		qglVertex2f(nx, ny - nw);
		break;
	case ALIGN_UC:
		qglVertex2f(nx, ny);
		qglVertex2f(nx + nw, ny);
		qglVertex2f(nx + nw - nh, ny + nh);
		qglVertex2f(nx - nh, ny + nh);
		break;
	default:
		qglVertex2f(nx, ny);
		qglVertex2f(nx + nw, ny);
		qglVertex2f(nx + nw, ny + nh);
		qglVertex2f(nx, ny + nh);
		break;
	}

	qglEnd();
	Draw_Color(NULL);
	qglEnable(GL_TEXTURE_2D);
}

/**
 * @brief
 */
void Draw_FadeScreen (void)
{
	GLSTATE_ENABLE_BLEND
	qglDisable(GL_TEXTURE_2D);
	qglColor4f(0, 0, 0, 0.8);
	qglBegin(GL_QUADS);

	qglVertex2f(0, 0);
	qglVertex2f(vid.width, 0);
	qglVertex2f(vid.width, vid.height);
	qglVertex2f(0, vid.height);

	qglEnd();
	qglColor4f(1, 1, 1, 1);
	qglEnable(GL_TEXTURE_2D);
	GLSTATE_DISABLE_BLEND
}

static float lastQ;
/**
 * @brief Draw the day and night images of a flat geoscape - multitexture feature is used to blend the images
 * @note If no multitexture is available only the day map is drawn
 */
void Draw_DayAndNight (int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, const char *map)
{
	image_t *gl;
	float nx, ny, nw, nh;

	/* normalize */
	nx = x * vid.rx;
	ny = y * vid.ry;
	nw = w * vid.rx;
	nh = h * vid.ry;

	/* load day image */
	gl = GL_FindImage(va("pics/menu/%s_day", map), it_wrappic);

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qfalse);
#endif

	/* draw day image */
	GL_Bind(gl->texnum);
	qglBegin(GL_QUADS);
	qglTexCoord2f(cx - iz, cy - iz);
	qglVertex2f(nx, ny);
	qglTexCoord2f(cx + iz, cy - iz);
	qglVertex2f(nx + nw, ny);
	qglTexCoord2f(cx + iz, cy + iz);
	qglVertex2f(nx + nw, ny + nh);
	qglTexCoord2f(cx - iz, cy + iz);
	qglVertex2f(nx, ny + nh);
	qglEnd();

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qtrue);
#endif
	/* test for multitexture and env_combine support */
	if (!qglSelectTextureSGIS && !qglActiveTextureARB)
		return;

	gl = GL_FindImage(va("pics/menu/%s_night", map), it_wrappic);
	/* maybe the campaign map doesn't have a night image */
	if (!gl)
		return;

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qfalse);
#endif
	/* init combiner */
	GLSTATE_ENABLE_BLEND

	GL_SelectTexture(gl_texture0);
	GL_Bind(gl->texnum);

	GL_SelectTexture(gl_texture1);
	if (!DaN || lastQ != q) {
		GL_CalcDayAndNight(q);
		lastQ = q;
	}

	assert(DaN);
#ifdef DEBUG
	if (!DaN)
		return;					/* never reached. need for code analyst. */
#endif

	GL_Bind(DaN->texnum);
	qglEnable(GL_TEXTURE_2D);

	/* draw night image */
	qglBegin(GL_QUADS);
	qglMTexCoord2fSGIS(gl_texture0, cx - iz, cy - iz);
	qglMTexCoord2fSGIS(gl_texture1, p + cx - iz, cy - iz);
	qglVertex2f(nx, ny);
	qglMTexCoord2fSGIS(gl_texture0, cx + iz, cy - iz);
	qglMTexCoord2fSGIS(gl_texture1, p + cx + iz, cy - iz);
	qglVertex2f(nx + nw, ny);
	qglMTexCoord2fSGIS(gl_texture0, cx + iz, cy + iz);
	qglMTexCoord2fSGIS(gl_texture1, p + cx + iz, cy + iz);
	qglVertex2f(nx + nw, ny + nh);
	qglMTexCoord2fSGIS(gl_texture0, cx - iz, cy + iz);
	qglMTexCoord2fSGIS(gl_texture1, p + cx - iz, cy + iz);
	qglVertex2f(nx, ny + nh);
	qglEnd();

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qtrue);
#endif

	/* reset mode */
	qglDisable(GL_TEXTURE_2D);
	GL_SelectTexture(gl_texture0);

	GLSTATE_DISABLE_BLEND
	if (gl_drawclouds->integer)
		Draw_Clouds(x, y, w, h, p, q, cx, cy, iz, map);
}

/**
 * @brief Draw the clouds for flat geoscape
 */
void Draw_Clouds (int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, const char *map)
{
	image_t *gl;
	float nx, ny, nw, nh;

	/* load clouds image */
	gl = GL_FindImage(va("pics/menu/%s_clouds", map), it_wrappic);
	/* maybe the campaign map doesn't have a clouds image */
	if (!gl)
		return;

	/* normalize */
	nx = x * vid.rx;
	ny = y * vid.ry;
	nw = w * vid.rx;
	nh = h * vid.ry;

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qfalse);
#endif
	/* init combiner */
	GLSTATE_ENABLE_BLEND
	qglBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);

	qglEnable(GL_TEXTURE_2D);

	/* draw day image */
	GL_Bind(gl->texnum);
	qglBegin(GL_QUADS);
	qglTexCoord2f(q-p + cx - iz, cy - iz);
	qglVertex2f(nx, ny);
	qglTexCoord2f(q-p + cx + iz, cy - iz);
	qglVertex2f(nx + nw, ny);
	qglTexCoord2f(q-p + cx + iz, cy + iz);
	qglVertex2f(nx + nw, ny + nh);
	qglTexCoord2f(q-p + cx - iz, cy + iz);
	qglVertex2f(nx, ny + nh);
	qglEnd();

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qtrue);
#endif
	/* reset mode */
	qglDisable(GL_TEXTURE_2D);

	GLSTATE_DISABLE_BLEND
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/**
 * @brief Draws a circle out of lines
 * @param[in] mid Center of the circle
 * @param[in] radius Radius of the circle
 * @param[in] color The color of the circle lines
 * @sa R_DrawPtlCircle
 * @sa Draw_LineStrip
 */
void Draw_Circle (vec3_t mid, float radius, const vec4_t color, int thickness)
{
	float theta;
	const float accuracy = 5.0f;

	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_LINE_SMOOTH);
	GLSTATE_ENABLE_BLEND

	Draw_Color(color);

	assert(radius > thickness);

	/* scale it */
	radius *= vid.rx;
	thickness *= vid.rx;

	/* store the matrix - we are using glTranslate */
	qglPushMatrix();

	/* translate the position */
	qglTranslated(mid[0], mid[1], mid[2]);

	if (thickness <= 1) {
		qglBegin(GL_LINE_STRIP);
		for (theta = 0.0f; theta <= 2.0f * M_PI; theta += M_PI / (radius * accuracy)) {
			qglVertex3f(radius * cos(theta), radius * sin(theta), 0.0);
		}
		qglEnd();
	} else {
		qglBegin(GL_TRIANGLE_STRIP);
		for (theta = 0; theta <= 2 * M_PI; theta += M_PI / (radius * accuracy)) {
			qglVertex3f(radius * cos(theta), radius * sin(theta), 0);
			qglVertex3f(radius * cos(theta - M_PI / (radius * accuracy)), radius * sin(theta - M_PI / (radius * accuracy)), 0);
			qglVertex3f((radius - thickness) * cos(theta - M_PI / (radius * accuracy)), (radius - thickness) * sin(theta - M_PI / (radius * accuracy)), 0);
			qglVertex3f((radius - thickness) * cos(theta), (radius - thickness) * sin(theta), 0);
		}
		qglEnd();
	}

	qglPopMatrix();

	Draw_Color(NULL);

	GLSTATE_DISABLE_BLEND
	qglDisable(GL_LINE_SMOOTH);
	qglEnable(GL_TEXTURE_2D);
}

#define MAX_LINEVERTS 256
/**
 * @brief
 * @sa Draw_Circle
 * @sa Draw_LineLoop
 */
void Draw_LineStrip (int points, int *verts)
{
	static int vs[MAX_LINEVERTS * 2];
	int i;

	/* fit it on screen */
	if (points > MAX_LINEVERTS * 2)
		points = MAX_LINEVERTS * 2;

	for (i = 0; i < points * 2; i += 2) {
		vs[i] = verts[i] * vid.rx;
		vs[i + 1] = verts[i + 1] * vid.ry;
	}

	/* init vertex array */
	qglDisable(GL_TEXTURE_2D);
	qglEnableClientState(GL_VERTEX_ARRAY);
	qglVertexPointer(2, GL_INT, 0, vs);

	/* draw */
	qglDrawArrays(GL_LINE_STRIP, 0, points);

	/* reset state */
	qglDisableClientState(GL_VERTEX_ARRAY);
	qglEnable(GL_TEXTURE_2D);
}

/**
 * @brief
 * @sa Draw_Circle
 * @sa Draw_LineStrip
 */
void Draw_LineLoop (int points, int *verts)
{
	static int vs[MAX_LINEVERTS * 2];
	int i;

	/* fit it on screen */
	if (points > MAX_LINEVERTS * 2)
		points = MAX_LINEVERTS * 2;

	for (i = 0; i < points * 2; i += 2) {
		vs[i] = verts[i] * vid.rx;
		vs[i + 1] = verts[i + 1] * vid.ry;
	}

	/* init vertex array */
	qglDisable(GL_TEXTURE_2D);
	qglEnableClientState(GL_VERTEX_ARRAY);
	qglVertexPointer(2, GL_INT, 0, vs);

	/* draw */
	qglDrawArrays(GL_LINE_LOOP, 0, points);

	/* reset state */
	qglDisableClientState(GL_VERTEX_ARRAY);
	qglEnable(GL_TEXTURE_2D);
}


/**
 * @brief
 * @sa Draw_Circle
 * @sa Draw_LineStrip
 * @sa Draw_LineLoop
 */
void Draw_Polygon (int points, int *verts)
{
	static int vs[MAX_LINEVERTS * 2];
	int i;

	/* fit it on screen */
	if (points > MAX_LINEVERTS * 2)
		points = MAX_LINEVERTS * 2;

	for (i = 0; i < points * 2; i += 2) {
		vs[i] = verts[i] * vid.rx;
		vs[i + 1] = verts[i + 1] * vid.ry;
	}

	/* init vertex array */
	qglDisable(GL_TEXTURE_2D);
	qglEnableClientState(GL_VERTEX_ARRAY);
	qglVertexPointer(2, GL_INT, 0, vs);

	/* draw */
	qglDrawArrays(GL_POLYGON, 0, points);

	/* reset state */
	qglDisableClientState(GL_VERTEX_ARRAY);
	qglEnable(GL_TEXTURE_2D);
}

#define MARKER_SIZE 60.0
/**
 * @brief Draw a 3D Marker on the 3D geoscape
 * @sa MAP_Draw3DMarkerIfVisible
 */
void Draw_3DMapMarkers (vec3_t angles, float zoom, vec3_t position, const char *model)
{
	modelInfo_t mi;
	char path[MAX_QPATH] = "";
	vec2_t model_center;
	vec4_t model_color = {0.5, 0.5, 0.5, 1};

	memset(&mi, 0, sizeof(modelInfo_t));

	Com_sprintf(path, sizeof(path), "geoscape/%s", model);
	mi.model = R_RegisterModelShort(path);
	mi.name = path;
	if (!mi.model) {
		Com_Printf("Could not find '%s'\n", path);
		mi.model = R_RegisterModelShort(path);
		mi.name = path;
		/*return;*/
	}

	mi.origin = position;
	mi.angles = angles;
	mi.scale = NULL;
	mi.color = model_color;

	model_center[0] = MARKER_SIZE * zoom;
	model_center[1] = MARKER_SIZE * zoom;
	mi.center = model_center;

	R_DrawModelDirect(&mi, NULL, NULL);
}

/**
 * @brief responsible for drawing the 3d globe on geoscape
 */
void Draw_3DGlobe (int x, int y, int w, int h, float p, vec3_t rotate, float zoom, const char *map)
{
	/* globe scaling */
	float fullscale = zoom / 4.0f;
	vec4_t lightPos = {0.0f, 0.0f, 0.0f, 0.0f};
	vec4_t lightColor = {1.0f, 1.0f, 1.0f, 1.0f};

	image_t* gl = NULL;
	float nx, ny, nw, nh;

	/* normalize */
	nx = x * vid.rx;
	ny = y * vid.ry;
	nw = w * vid.rx;
	nh = h * vid.ry;

	/* load day image */
	gl = GL_FindImage(va("pics/menu/%s_day", map), it_wrappic);
	if (!gl) {
		ri.Con_Printf(PRINT_ALL, "Could not find pics/menu/%s_day\n", map);
		return;
	}

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qfalse);
#endif

	/* turn on fogging. fog looks good on the skies - it gives them a more */
	/* "airy" far-away look, and has the knock-on effect of preventing the */
	/* old "texture distortion at the poles" problem. */
	if (gl_fog->integer) {
		qglFogi(GL_FOG_MODE, GL_LINEAR);
		qglFogfv(GL_FOG_COLOR, globe_fog);
		qglFogf(GL_FOG_START, 5.0);

		/* must tweak the fog end too!!! */
		qglFogf(GL_FOG_END, r_newrefdef.fog);
		qglEnable(GL_FOG);
	}

	/* globe texture scaling */
	/* previous releases made a tiled version of the globe texture.  here i just shrink it using the */
	/* texture matrix, for much the same effect */
	qglMatrixMode(GL_TEXTURE);
	qglLoadIdentity();
	/* scale the texture */
	qglScalef(2, 1, 1);
	qglMatrixMode(GL_MODELVIEW);

	/* background */
	/* go to a new matrix */
	qglPushMatrix();

	/* center it */
	qglTranslatef((nx+nw)/2, (ny+nh)/2, 0);

	/* flatten the sphere */
	/* this will also scale the normal vectors */
	qglScalef(fullscale * vid.rx, fullscale * vid.ry, fullscale);

#if 0
	/* call this to rescale the normal vectors */
	qglEnable(GL_RESCALE_NORMAL);
#endif

	/* rotate the globe as given in ccs.angles */
	qglRotatef(rotate[YAW], 1, 0, 0);
	qglRotatef(rotate[ROLL], 0, 1, 0);
	qglRotatef(rotate[PITCH], 0, 0, 1);

	/* solid globe texture */
	qglBindTexture(GL_TEXTURE_2D, gl->texnum);

	qglEnable(GL_CULL_FACE);
	qglCullFace(GL_BACK);

	/* add the light */
	lightPos[0] = cos(p);
	lightPos[1] = -sin(p);
	qglLightfv(GL_LIGHT0, GL_POSITION, lightPos);
	qglLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
	qglLightfv(GL_LIGHT0, GL_AMBIENT, lightColor);

	/* enable the lighting */
	qglEnable(GL_LIGHTING);
	qglEnable(GL_LIGHT0);

	/* draw the sphere */
	qglCallList(spherelist);

#ifdef HAVE_SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qfalse);
#endif

	qglDisable(GL_CULL_FACE);
	/* revert the cullface mode */
	qglCullFace(GL_FRONT);

	if (gl_fog->integer) {
		/* turn off fog */
		qglDisable(GL_FOG);
	}

	/* disable 3d geoscape lightning */
	qglDisable(GL_LIGHTING);

	/* restore the previous matrix */
	qglPopMatrix();
	qglMatrixMode(GL_TEXTURE);
	qglLoadIdentity();
	qglMatrixMode(GL_MODELVIEW);
}
