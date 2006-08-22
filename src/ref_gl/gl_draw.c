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

#ifdef __linux__
#define min(a,b) ((a)<(b) ? (a):(b))
#endif

extern qboolean scrap_dirty;
void Scrap_Upload(void);
image_t *shadow;


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
image_t *Draw_FindPic(char *name)
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
void Draw_Fill(int x, int y, int w, int h, int style, vec4_t color)
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

extern unsigned r_rawpalette[256];
/**
 * @brief
 */
void Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte * data)
{
	unsigned *image32;
	size_t img_size = 256 * 256 * sizeof(image32[0]);
	int i, j, trows;
	byte *source;
	int frac, fracstep;
	float hscale;
	int row;
	float t;

	image32 = malloc(img_size);
	if (!image32) {
		ri.Sys_Error(ERR_FATAL, "malloc: failed on allocation of %i bytes", img_size);
		return;					/* never reached. need for code analyst. */
	}

	GL_Bind(0);

	if (rows <= 256) {
		hscale = 1;
		trows = rows;
	} else {
		hscale = rows / 256.0;
		trows = 256;
	}
	t = rows * hscale / 256;

	for (i = 0; i < trows; i++) {
		unsigned *dest;

		row = (int) (i * hscale);
		if (row > rows)
			break;

		source = data + cols * row;
		dest = &image32[i * 256];
		fracstep = cols * 0x10000 / 256;
		frac = fracstep >> 1;
		for (j = 0; j < 256; j++) {
			dest[j] = r_rawpalette[source[frac >> 16]];
			frac += fracstep;
		}
	}

	qglTexImage2D(GL_TEXTURE_2D, 0, gl_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image32);
	free(image32);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if ((gl_config.renderer == GL_RENDERER_MCD) || (gl_config.renderer & GL_RENDERER_RENDITION))
		qglDisable(GL_ALPHA_TEST);

	qglBegin(GL_QUADS);
	qglTexCoord2f(0, 0);
	qglVertex2f(x, y);
	qglTexCoord2f(1, 0);
	qglVertex2f(x + w, y);
	qglTexCoord2f(1, t);
	qglVertex2f(x + w, y + h);
	qglTexCoord2f(0, t);
	qglVertex2f(x, y + h);
	qglEnd();

	if ((gl_config.renderer == GL_RENDERER_MCD) || (gl_config.renderer & GL_RENDERER_RENDITION))
		qglEnable(GL_ALPHA_TEST);
}


static float lastQ;
/**
 * @brief
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

	/* init combiner */
	qglEnable(GL_BLEND);

	GL_SelectTexture(gl_texture0);
	gl = GL_FindImage(va("pics/menu/%s_night", map), it_wrappic);
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
 * @brief vertices of a unit icosahedron
 */
static globe_triangle_t icosahedron[MAX_ICOSAHEDRON] = {
	/* "north" pole */

	{{Ip1, Ip0, Ip2},},
	{{Ip5, Ip0, Ip1},},
	{{Ip4, Ip0, Ip5},},
	{{Ip3, Ip0, Ip4},},
	{{Ip2, Ip0, Ip3},},

	/* mid */
	{{Ip1, Im1, Im0},},
	{{Im0, Ip5, Ip1},},
	{{Ip5, Im0, Im4},},
	{{Im4, Ip4, Ip5},},
	{{Ip4, Im4, Im3},},
	{{Im3, Ip3, Ip4},},
	{{Ip3, Im3, Im2},},
	{{Im2, Ip2, Ip3},},
	{{Ip2, Im2, Im1},},
	{{Im1, Ip1, Ip2},},

	/* "south" pole */
	{{Im3, Im5, Im2},},
	{{Im4, Im5, Im3},},
	{{Im0, Im5, Im4},},
	{{Im1, Im5, Im0},},
	{{Im2, Im5, Im1},},
};

/**
 * @brief This function fills v with the x-, y-, and z-coordinate
 * for the specified point on the surface.
 */
void Globe_CoordToVec(double lat, double lon, vec3_t v)
{
	v[0] = (float) sin(lon * GLOBE_TORAD) * cos(lat * GLOBE_TORAD);
	v[1] = (float) sin(lat * GLOBE_TORAD);
	v[2] = (float) cos(lon * GLOBE_TORAD) * cos(lat * GLOBE_TORAD);
}

/**
 * @brief returns the longitude of a point on the unit sphere.
 * (Longitudes are the "x" axis on a mercator projection map.)
 */
double Globe_ToLong(vec3_t v)
{
	double s;

	if (v[1] == 1.0 || v[1] == -1.0)
		return (5000.0);		/* Problem! The texture will warp! Actually these */
	s = sqrt(1 - v[1] * v[1]);	/* points (the poles) are on ALL the longitudes! */
	return ((atan2(v[0] / s, v[2] / s) * GLOBE_TODEG));
}

/**
 * @brief returns the latitude of a point on the unit sphere.
 * (Latitudes are the "y" axis on a mercator projection map.)
 */
double Globe_ToLat(vec3_t v)
{
	return (atan2(v[1], sqrt(1 - v[1] * v[1])) * GLOBE_TODEG);
}

/**
 * @brief returns the actual theoretical minimum
 * distance (in meters) the signal has to travel.
 */
unsigned long Globe_Distance(int site)
{
	unsigned long distance_covered = 0;

#if 0
	int i;
	float angle;
	vec3_t v1, v2;

	for (i = 1; i <= site; i++) {
		/* This is not really the way to do this. This assumes that
		   earth is round (it isn't!). I'm sure there's a good way to
		   compute the actual distance between two locations on earth. */

		/*    Angle is arccos(Ax*Bx + Ay*By + Az*Bz)    */

		Globe_CoordToVec(sites[i].lat, sites[i].lon, v1);
		Globe_CoordToVec(sites[i - 1].lat, sites[i - 1].lon, v2);

		angle = acos((float) (v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2])) * GLOBE_TODEG;

		distance_covered += (int) rint((angle / 360.0) * PHYSICAL_EARTH_CIRC);
	}
#endif
	return distance_covered;
}

/**
 * @brief normalize vector r
 */
void Globe_Normalize(vec3_t r)
{
	float mag;

	mag = r[0] * r[0] + r[1] * r[1] + r[2] * r[2];
	if (mag != 0.0f) {
		mag = 1.0f / sqrt(mag);
		r[0] *= mag;
		r[1] *= mag;
		r[2] *= mag;
	}
}

/**
 * @brief linearly interpolate between a & b, by fraction f
 */
void Globe_Lerp(vec3_t a, vec3_t b, float f, vec3_t r)
{
	r[0] = a[0] + f * (b[0] - a[0]);
	r[1] = a[1] + f * (b[1] - a[1]);
	r[2] = a[2] + f * (b[2] - a[2]);
}

/**
 * @brief Convenience function used only in Draw_3DGlobe.
 * Adds a vertex to a tstrip.
 */
void Globe_AddVertex(vec3_t v, double *lastlon)
{
	double a, b, dist;
	vec3_t x;

	VectorCopy(v, x);
	Globe_Normalize(x);
	qglNormal3fv(x);
	a = Globe_ToLong(x);
	b = Globe_ToLat(x);

	/* 5000 is the slightly unintuitive value tolon() */
	if (a == 5000.0) {
		/* returns when it finds a value on a pole. */
		if (b > 0) {
			/* If it's the north pole */
			a = *lastlon - 36.0;
		} else {
			/* South pole. */
			a = *lastlon + 36.0;
		}
	} else if (*lastlon != 4000.0) {	/* Not the first vertex in a tstrip. */
		/* This stuff is to get the wraparoud at the backside of the
		   sphere to work. (Or you'd get a weird zigzag line down the
		   back of the sphere.) */

		dist = a - *lastlon;
		if (dist > 100.0) {
			a -= 360.0;
		} else if (dist < -100.0) {
			a += 360.0;
		}
	}
	*lastlon = a;
	qglTexCoord2f(a / 360.0 + 0.5, -(b / 180.0 + 0.5));
	qglVertex3fv(x);
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
	float vink;
	float linesweep;
	vec3_t v1, v2;

	qglPushMatrix();
/* 	qglMaterialfv(GL_FRONT, GL_DIFFUSE, lineColor); */

	/*    Angle is arccos(Ax*Bx + Ay*By + Az*Bz)    */
	Globe_CoordToVec(path[n][0], path[n][1], v1);
	Globe_CoordToVec(path[0][0], path[0][1], v2);
	linesweep = acos((float) (v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2]) * GLOBE_TODEG);

	/*   This mess calculates the angle to rotate "to" to a position
	   'south' of "from". It's a somewhat adapted version of a formula
	   that calculates an angle in a triangle on a sphere given all three
	   sides. (Found the original in Beta.)                           */
	vink = GLOBE_TODEG * acos((sin(path[n][0] * GLOBE_TORAD) - sin(path[0][0] * GLOBE_TORAD) * cos(linesweep * GLOBE_TORAD)) /
							  (cos(path[0][0] * GLOBE_TORAD) * sin(linesweep * GLOBE_TORAD)));

	/*   Compensate for the usual arccos problem. (It checks if "to" is
	   'to the right' or 'to the left' of "from")                 */

	if (path[0][1] > 0.0 && (path[n][1] > path[0][1] || path[n][1] < path[0][1] - 180.0))
		vink = -vink;
	else if (path[n][1] > path[0][1] && path[n][1] < path[0][1] + 180.0)
		vink = -vink;

	/*   Rotate "to" to a position 'south' of "from".   */
	qglRotatef(vink, v2[0], v2[1], v2[2]);

	/*   ...and rotate them onto the z=0 plane.   */
	qglRotatef(90.0 + path[0][1], 0.0, 1.0, 0.0);

	/*   Fix incorrect names on the lines, identifying them as the
	   sitemarker we just drew.             */
/* 	qglLoadName(NOT_SELECTABLE); */

/* 	partialtorus((GLfloat)path[0]->[1]-180.0, (GLfloat)linesweep, zoom); */
	qglPopMatrix();
}

/**
 * @brief responsible for drawing the 3d globe on geoscape
 */
void Draw_3DGlobe(int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, char *map)
{
	int nrows = 1 << 3;
	int s, i, j;
	double last_lon;
	vec3_t v0, v1, v2, v3, veca, vecb;
	globe_triangle_t *t = NULL;
	image_t *gl;

	qglPushMatrix();
	qglEnable(GL_TEXTURE_2D);
	gl = GL_FindImage(va("pics/menu/%s_day", map), it_wrappic);
	GL_Bind(gl->texnum);

	qglTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	qglTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	/* iterate over the 20 sides of the icosahedron */
	for (s = 0; s < MAX_ICOSAHEDRON; s++) {
		t = &icosahedron[s];
		for (i = 0; i < nrows; i++) {
			/* create a tstrip for each row */
			/* number of triangles in this row is number in previous +2 */
			/* strip the ith trapezoid block */
			Globe_Lerp(t->vec[1], t->vec[0], (float) (i + 1) / nrows, v0);
			Globe_Lerp(t->vec[1], t->vec[0], (float) i / nrows, v1);
			Globe_Lerp(t->vec[1], t->vec[2], (float) (i + 1) / nrows, v2);
			Globe_Lerp(t->vec[1], t->vec[2], (float) i / nrows, v3);

			/* Hack to get the texturing right. */
			last_lon = 4000.0;

			qglBegin(GL_TRIANGLE_STRIP);
			Globe_AddVertex(v0, &last_lon);
			Globe_AddVertex(v1, &last_lon);
			for (j = 0; j < i; j++) {
				/* calculate 2 more vertices at a time */
				Globe_Lerp(v0, v2, (float) (j + 1) / (i + 1), veca);
				Globe_Lerp(v1, v3, (float) (j + 1) / i, vecb);
				Globe_AddVertex(veca, &last_lon);
				Globe_AddVertex(vecb, &last_lon);
			}
			Globe_AddVertex(v2, &last_lon);
			qglEnd();			/* TRIANGLE_STRIP */
		}
	}

	/* test for multitexture and env_combine support */
	if (!qglSelectTextureSGIS && !qglActiveTextureARB) {
		qglPopMatrix();
		return;
	}

	/* init combiner */
	qglEnable(GL_BLEND);

	GL_SelectTexture(gl_texture0);
	gl = GL_FindImage(va("pics/menu/%s_night", map), it_wrappic);
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

	/* TODO: draw night image */

	/* reset mode */
	qglDisable(GL_TEXTURE_2D);
	GL_SelectTexture(gl_texture0);

	qglDisable(GL_BLEND);
	qglPopMatrix();
}
