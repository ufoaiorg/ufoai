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

static float globe_fog[4];
static int spherelist = -1;
#define GLOBE_TRIS 10
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

		qglNewList (spherelist, GL_COMPILE);

		for (i = 0; i < GLOBE_TRIS; i++) {
			qglBegin (GL_TRIANGLE_STRIP);

			for (j = 0; j <= GLOBE_TRIS; j++) {
				qglTexCoord2fv (&globetexes[texespos]);
				qglVertex3fv (&globeverts[vertspos]);

				texespos += 2;
				vertspos += 3;

				qglTexCoord2fv (&globetexes[texespos]);
				qglVertex3fv (&globeverts[vertspos]);

				texespos += 2;
				vertspos += 3;
			}

			qglEnd ();
		}

		qglEndList ();
	}
}


/**
 * @brief Initialize the globe chain arrays
 */
static void GL_InitGlobeChain (void)
{
	const float drho = 0.3141592653589;
	const float dtheta = 0.6283185307180;

	const float ds = 0.1;
	const float dt = 0.1;

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
			globetexes[texespos++] = t;

			globeverts[vertspos++] = stheta * srho * 4096.0;
			globeverts[vertspos++] = ctheta * srho * 4096.0;
			globeverts[vertspos++] = crho * 4096.0;

			globetexes[texespos++] = s;
			globetexes[texespos++] = (t - dt);

			globeverts[vertspos++] = stheta * srhodrho * 4096.0;
			globeverts[vertspos++] = ctheta * srhodrho * 4096.0;
			globeverts[vertspos++] = crhodrho * 4096.0;

			s += ds;
		}

		t -= dt;
	}
}

/* console font */
image_t *draw_chars;

/**
 * @brief
 */
void Draw_InitLocal(void)
{
	shadow = GL_FindImage("pics/sfx/shadow.tga", it_pic);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	/* load console characters (don't bilerp characters) */
	draw_chars = GL_FindImage("pics/conchars.pcx", it_pic);
	GL_Bind(draw_chars->texnum);

	Font_Init();
	GL_InitGlobeChain();
	GL_DrawSphere();
}


/**
 * @brief Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void Draw_Char(int x, int y, int num)
{
	int row, col;
	float frow, fcol, size;

	num &= 255;

	if ((num & 127) == 32)		/* space */
		return;

	if (y <= -8)
		return;					/* totally off screen */

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;				/* 16 cols (conchars.pcx) */

	GL_Bind(draw_chars->texnum);

	qglBegin(GL_QUADS);
	qglTexCoord2f(fcol, frow);
	qglVertex2f(x, y);
	qglTexCoord2f(fcol + size, frow);
	qglVertex2f(x + 8, y);
	qglTexCoord2f(fcol + size, frow + size);
	qglVertex2f(x + 8, y + 8);
	qglTexCoord2f(fcol, frow + size);
	qglVertex2f(x, y + 8);
	qglEnd();
}

/**
 * @brief Change the color to given value
 * @param[in] rgba A pointer to a vec4_t with rgba color value
 * @note To reset the color let rgba be NULL
 * @note Enables openGL blend if alpha value is lower than 1.0
 */
void Draw_Color(const float *rgba)
{
	if (rgba) {
		if (rgba[3] < 1.0f)
			qglEnable(GL_BLEND);
		qglColor4fv(rgba);
	} else {
		qglDisable(GL_BLEND);
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
image_t *Draw_FindPic(const char *name)
{
	image_t *gl;
	char fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
		Com_sprintf(fullname, sizeof(fullname), "pics/%s", name);
	else
		Q_strncpyz(fullname, name + 1, MAX_QPATH);

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
void Draw_GetPicSize(int *w, int *h, char *pic)
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
 * @brief
 */
void Draw_StretchPic(int x, int y, int w, int h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic(pic);
	if (!gl) {
		ri.Con_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload();

#ifdef SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qfalse);
#endif

	GL_Bind(gl->texnum);
	qglBegin(GL_QUADS);
	qglTexCoord2f(gl->sl, gl->tl);
	qglVertex2f(x, y);
	qglTexCoord2f(gl->sh, gl->tl);
	qglVertex2f(x + w, y);
	qglTexCoord2f(gl->sh, gl->th);
	qglVertex2f(x + w, y + h);
	qglTexCoord2f(gl->sl, gl->th);
	qglVertex2f(x, y + h);
	qglEnd();

#ifdef SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qtrue);
#endif
}


/**
 * @brief
 */
void Draw_NormPic(float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, qboolean blend, char *name)
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
		sl = sl / gl->width;
		sh = sh / gl->width;
		tl = tl / gl->height;
		th = th / gl->height;
	} else {
		if (!w || !h) {
			nw = (float) gl->width * vid.rx;
			nh = (float) gl->height * vid.ry;
		}
		sh = 1;
		th = 1;
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
		qglEnable(GL_BLEND);

#ifdef SHADERS
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
		qglDisable(GL_BLEND);

#ifdef SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qtrue);
#endif
}


/**
 * @brief
 */
void Draw_Pic(int x, int y, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic(pic);
	if (!gl) {
		ri.Con_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload();

#ifdef SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qfalse);
#endif
	GL_Bind(gl->texnum);
	qglBegin(GL_QUADS);
	qglTexCoord2f(gl->sl, gl->tl);
	qglVertex2f(x, y);
	qglTexCoord2f(gl->sh, gl->tl);
	qglVertex2f(x + gl->width, y);
	qglTexCoord2f(gl->sh, gl->th);
	qglVertex2f(x + gl->width, y + gl->height);
	qglTexCoord2f(gl->sl, gl->th);
	qglVertex2f(x, y + gl->height);
	qglEnd();
#ifdef SHADERS
	if (gl->shader)
		SH_UseShader(gl->shader, qtrue);
#endif
}

/**
 * @brief This repeats a 64*64 tile graphic to fill the screen around a sized down refresh window.
 */
void Draw_TileClear(int x, int y, int w, int h, char *name)
{
	image_t *image;

	image = Draw_FindPic(name);
	if (!image) {
		ri.Con_Printf(PRINT_ALL, "Can't find pic: %s\n", name);
		return;
	}

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
}


/**
 * @brief Fills a box of pixels with a single color
 */
void Draw_Fill(int x, int y, int w, int h, int style, const vec4_t color)
{
	float nx, ny, nw, nh;

	nx = x * vid.rx;
	ny = y * vid.ry;
	nw = w * vid.rx;
	nh = h * vid.ry;

	Draw_Color(color);

	qglDisable(GL_TEXTURE_2D);
	qglBegin(GL_QUADS);

	switch (style) {
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
void Draw_FadeScreen(void)
{
	qglEnable(GL_BLEND);
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
	qglDisable(GL_BLEND);
}

static float lastQ;
/**
 * @brief Draw the day and night images of a flat geoscape - multitexture feature is used to blend the images
 * @note If no multitexture is available only the day map is drawn
 */
void Draw_DayAndNight(int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, char *map)
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

	/* test for multitexture and env_combine support */
	if (!qglSelectTextureSGIS && !qglActiveTextureARB)
		return;

	gl = GL_FindImage(va("pics/menu/%s_night", map), it_wrappic);
	/* maybe the campaign map doesn't have a night image */
	if (!gl)
		return;

	/* init combiner */
	qglEnable(GL_BLEND);

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

	/* reset mode */
	qglDisable(GL_TEXTURE_2D);
	GL_SelectTexture(gl_texture0);

	qglDisable(GL_BLEND);
	if (gl_drawclouds->value)
		Draw_Clouds(x, y, w, h, p, q, cx, cy, iz, map);
}

/**
 * @brief Draw the clouds for flat geoscape
 */
void Draw_Clouds(int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, char *map)
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

	/* init combiner */
	qglEnable(GL_BLEND);
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

	/* reset mode */
	qglDisable(GL_TEXTURE_2D);

	qglDisable(GL_BLEND);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

#define MAX_LINEVERTS 256
/**
 * @brief
 */
void Draw_LineStrip(int points, int *verts)
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
 */
void Draw_3DMapMarkers(float latitude, float longitude, char *image)
{

}

/**
 * @brief This is only used from makeearth, to plot a line on the globe.
 * There is some difficultish maths in this one.
 * "to" and "from" in the comments in the code refers to the two
 * points given as arguments.
 */
void Draw_3DMapLine(int n, float dist, vec2_t * path)
{
}

/**
 * @brief responsible for drawing the 3d globe on geoscape
 */
void Draw_3DGlobe(int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, char *map)
{
	static float rotateX = 0.0f;
	static float rotateY = 0.0f;
	static float lastcx = 0.0f, lastcy = 0.0f;
	/* globe scaling */
	float value = iz;
	float fullscale = value / 4;
	float halfscale = value / 8;
	float reducedfull = fullscale * 0.9;
	float reducedhalf = halfscale * 0.9;

	image_t* gl;
	float nx, ny, nw, nh;

	/* normalize */
	nx = x * vid.rx;
	ny = y * vid.ry;
	nw = w * vid.rx;
	nh = h * vid.ry;

	/* load day image */
	gl = GL_FindImage(va("pics/menu/%s_day", map), it_wrappic);

	/* turn on fogging.  fog looks good on the skies - it gives them a more */
	/* "airy" far-away look, and has the knock-on effect of preventing the */
	/* old "texture distortion at the poles" problem. */
	if (gl_fog->value) {
		qglFogi (GL_FOG_MODE, GL_LINEAR);
		qglFogfv (GL_FOG_COLOR, globe_fog);
		qglFogf (GL_FOG_START, 5);

		/* must tweak the fog end too!!! */
		qglFogf (GL_FOG_END, r_newrefdef.fog);
		qglEnable (GL_FOG);
	}

	/* globe texture scaling */
	/* previous releases made a tiled version of the globe texture.  here i just shrink it using the */
	/* texture matrix, for much the same effect */
	qglMatrixMode (GL_TEXTURE);
	qglLoadIdentity ();
	qglScalef (2, 1, 1);
	qglMatrixMode (GL_MODELVIEW);

	/* background */
	/* go to a new matrix */
	qglPushMatrix ();

	/* center it */
	qglTranslatef (x+w/2, y+h/2, 0);

	/* flatten the sphere */
	qglScalef (fullscale, fullscale, halfscale);

	if (lastcx != cx || lastcy != cy) {
		rotateX += cx;
		rotateY += cy;
		lastcx = cx;
		lastcy = cy;
		if (rotateX > 360.0f)
			rotateX -= 360.0f;
		if (rotateY > 360.0f)
			rotateY -= 360.0f;
		if (rotateX < 0.0f)
			rotateX += 360.0f;
		if (rotateY < 0.0f)
			rotateY += 360.0f;
	}

	/* rotate in x direction */
	qglRotatef (rotateX, 1, 0, 0);

	/* rotate in y direction */
	qglRotatef (rotateY, 0, 1, 0);

	/* solid globe texture */
	qglBindTexture (GL_TEXTURE_2D, gl->texnum);

	/* draw the sphere */
	qglCallList (spherelist);

	/* restore the previous matrix */
	qglPopMatrix ();

	/* foreground */
	/* go to a new matrix */
	qglPushMatrix ();

	/* center it */
	qglTranslatef (x+w/2, y+h/2, 0);

	/* flatten the sphere and shrink it a little - the reduced scale prevents artefacts appearing when */
	/* corners on the globesphere may potentially overlap */
	qglScalef (reducedfull, reducedfull, reducedhalf);

	/* orient it so that the poles are unobtrusive */
	qglRotatef (rotateX, 1, 0, 0);

	/* make it not always at right angles to the player */
	qglRotatef (rotateY, 0, 1, 0);

	gl = GL_FindImage(va("pics/menu/%s_night", map), it_wrappic);
	/* maybe the campaign map doesn't have a night image */
	if (gl) {
		/* blend mode */
		qglEnable (GL_BLEND);

		/* alpha globe texture */
		qglBindTexture (GL_TEXTURE_2D, gl->texnum);

		/* draw the sphere */
		qglCallList (spherelist);

		/* back to normal mode */
		qglDisable (GL_BLEND);
	}

	/* restore the previous matrix */
	qglPopMatrix ();

	if (gl_fog->value) {
		/* turn off fog */
		qglDisable (GL_FOG);
	}

	qglMatrixMode (GL_TEXTURE);
	qglLoadIdentity ();
	qglMatrixMode (GL_MODELVIEW);
}
