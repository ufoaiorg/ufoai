/**
 * @file r_draw.c
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
#include "r_sphere.h"
#include "r_error.h"
#include "r_draw.h"
#include "r_mesh.h"
#include "r_overlay.h"
#include "r_framebuffer.h"
#include "r_program.h"
#include "../cl_console.h"

extern const float STANDARD_3D_ZOOM;

/** the last q value for the 2d geoscape night overlay */
static float lastQ = 0.0f;

image_t *shadow;

/* console font */
static image_t *draw_chars;

/**
 * @brief Loads some textures and init the 3d globe
 * @sa R_Init
 */
void R_DrawInitLocal (void)
{
	shadow = R_FindImage("pics/sfx/shadow", it_effect);
	if (shadow == r_noTexture)
		Com_Printf("Could not find shadow image in game pics/sfx directory!\n");

	draw_chars = R_FindImage("pics/conchars", it_chars);
	if (draw_chars == r_noTexture)
		Com_Error(ERR_FATAL, "Could not find conchars image in game pics directory!");

	lastQ = -1;
}

#define MAX_CHARS 8192

/* chars are batched per frame so that they are drawn in one shot */
static float char_texcoords[MAX_CHARS * 4 * 2];
static short char_verts[MAX_CHARS * 4 * 2];
static int char_index = 0;

/**
 * @brief Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void R_DrawChar (int x, int y, int num)
{
	int row, col;
	float frow, fcol;

	num &= 255;

	if ((num & 127) == ' ')		/* space */
		return;

	if (y <= -con_fontHeight)
		return;					/* totally off screen */

	if (char_index >= MAX_CHARS * 8)
		return;

	row = (int) num >> 4;
	col = (int) num & 15;

	/* 0.0625 => 16 cols (conchars images) */
	frow = row * 0.0625;
	fcol = col * 0.0625;

	char_texcoords[char_index + 0] = fcol;
	char_texcoords[char_index + 1] = frow;
	char_texcoords[char_index + 2] = fcol + 0.0625;
	char_texcoords[char_index + 3] = frow;
	char_texcoords[char_index + 4] = fcol + 0.0625;
	char_texcoords[char_index + 5] = frow + 0.0625;
	char_texcoords[char_index + 6] = fcol;
	char_texcoords[char_index + 7] = frow + 0.0625;

	char_verts[char_index + 0] = x;
	char_verts[char_index + 1] = y;
	char_verts[char_index + 2] = x + con_fontWidth;
	char_verts[char_index + 3] = y;
	char_verts[char_index + 4] = x + con_fontWidth;
	char_verts[char_index + 5] = y + con_fontHeight;
	char_verts[char_index + 6] = x;
	char_verts[char_index + 7] = y + con_fontHeight;

	char_index += 8;
}

void R_DrawChars (void)
{
	R_BindTexture(draw_chars->texnum);

	/* alter the array pointers */
	glVertexPointer(2, GL_SHORT, 0, char_verts);
	glTexCoordPointer(2, GL_FLOAT, 0, char_texcoords);

	glDrawArrays(GL_QUADS, 0, char_index / 2);

	char_index = 0;

	/* and restore them */
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
}

/**
 * @brief Uploads image data
 * @param[in] name The name of the texture to use for this data
 * @param[in] frame The frame data that is uploaded
 * @param[in] width The width of the texture
 * @param[in] height The height of the texture
 * @return the texture number of the uploaded images
 */
int R_UploadData (const char *name, byte *frame, int width, int height)
{
	image_t *img;
	unsigned *scaled;
	int scaledWidth, scaledHeight;

	R_GetScaledTextureSize(width, height, &scaledWidth, &scaledHeight);

	img = R_FindImage(name, it_pic);
	if (img == r_noTexture)
		Com_Error(ERR_FATAL, "Could not find the searched image: %s", name);

	if (scaledWidth != width || scaledHeight != height) {  /* whereas others need to be scaled */
		scaled = (unsigned *)Mem_PoolAllocExt(scaledWidth * scaledHeight * sizeof(unsigned), qfalse, vid_imagePool, 0);
		R_ScaleTexture((unsigned *)frame, width, height, scaled, scaledWidth, scaledHeight);
	} else {
		scaled = (unsigned *)frame;
	}

	R_BindTexture(img->texnum);
	if (img->upload_width == scaledWidth && img->upload_height == scaledHeight) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, scaledWidth, scaledHeight, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	} else {
		/* Reallocate the texture */
		img->width = width;
		img->height = height;
		img->upload_width = scaledWidth;
		img->upload_height = scaledHeight;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	R_CheckError();

	if (scaled != (unsigned *)frame)
		Mem_Free(scaled);

	return img->texnum;
}

/**
 * @brief Searches for an image in the image array
 * @param[in] name The name of the image relative to pics/
 * @note name may not be null and has to be longer than 4 chars
 * @return NULL on error or image_t pointer on success
 * @sa R_FindImage
 */
const image_t *R_RegisterImage (const char *name)
{
	const image_t *image = R_FindImage(va("pics/%s", name), it_pic);
	if (image == r_noTexture)
		return NULL;
	return image;
}

/**
 * @brief Bind and draw a texture
 * @param[in] texnum The texture id (already uploaded of course)
 * @param[in] x normalized x value on the screen
 * @param[in] y normalized y value on the screen
 * @param[in] w normalized width value
 * @param[in] h normalized height value
 */
void R_DrawTexture (int texnum, int x, int y, int w, int h)
{
	R_BindTexture(texnum);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(x, y);
	glTexCoord2f(1, 0);
	glVertex2f(x + w, y);
	glTexCoord2f(1, 1);
	glVertex2f(x + w, y + h);
	glTexCoord2f(0, 1);
	glVertex2f(x, y + h);
	glEnd();
}

/**
 * @brief Draws an image or parts of it
 * @param[in] x X position to draw the image to
 * @param[in] y Y position to draw the image to
 */
void R_DrawImage (float x, float y, const image_t *image)
{
	float x1, x2, x3, x4, y1, y2, y3, y4;
	short image_verts[8];

	if (!image)
		return;

	/* normalize to the screen resolution */
	x1 = x * viddef.rx;
	y1 = y * viddef.ry;

	/* fill the rest of the coordinates to make a rectangle */
	x4 = x1;
	x3 = x2 = x1 + image->width * viddef.rx;
	y2 = y1;
	y4 = y3 = y1 + image->height * viddef.ry;

	image_verts[0] = x1;
	image_verts[1] = y1;
	image_verts[2] = x2;
	image_verts[3] = y2;
	image_verts[4] = x3;
	image_verts[5] = y3;
	image_verts[6] = x4;
	image_verts[7] = y4;

	R_DrawImageArray(default_texcoords, image_verts, image);
}

void R_DrawStretchImage (float x, float y, int w, int h, const image_t *image)
{
	float x1, x2, x3, x4, y1, y2, y3, y4;
	short image_verts[8];

	if (!image)
		return;

	/* normalize to the screen resolution */
	x1 = x * viddef.rx;
	y1 = y * viddef.ry;

	/* fill the rest of the coordinates to make a rectangle */
	x4 = x1;
	x3 = x2 = x1 + w * viddef.rx;
	y2 = y1;
	y4 = y3 = y1 + h * viddef.ry;

	image_verts[0] = x1;
	image_verts[1] = y1;
	image_verts[2] = x2;
	image_verts[3] = y2;
	image_verts[4] = x3;
	image_verts[5] = y3;
	image_verts[6] = x4;
	image_verts[7] = y4;

	R_DrawImageArray(default_texcoords, image_verts, image);
}

const image_t *R_DrawImageArray (const float texcoords[8], const short verts[8], const image_t *image)
{
	/* alter the array pointers */
	glVertexPointer(2, GL_SHORT, 0, verts);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, texcoords);

	R_BindTexture(image->texnum);

	glDrawArrays(GL_QUADS, 0, 4);

	/* and restore them */
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	return image;
}

/**
 * @brief Fills a box of pixels with a single color
 */
void R_DrawFill (int x, int y, int w, int h, const vec4_t color)
{
	const float nx = x * viddef.rx;
	const float ny = y * viddef.ry;
	const float nw = w * viddef.rx;
	const float nh = h * viddef.ry;

	R_Color(color);

	glDisable(GL_TEXTURE_2D);

	glBegin(GL_QUADS);
	glVertex2f(nx, ny);
	glVertex2f(nx + nw, ny);
	glVertex2f(nx + nw, ny + nh);
	glVertex2f(nx, ny + nh);
	glEnd();

	glEnable(GL_TEXTURE_2D);

	R_Color(NULL);
}

/**
 * @brief Draws a rect to the screen. Also has support for stippled rendering of the rect.
 *
 * @param[in] x X-position of the rect
 * @param[in] y Y-position of the rect
 * @param[in] w Width of the rect
 * @param[in] h Height of the rect
 * @param[in] color RGBA color of the rect
 * @param[in] lineWidth Line strength in pixel of the rect
 * @param[in] pattern Specifies a 16-bit integer whose bit pattern determines
 * which fragments of a line will be drawn when the line is rasterized.
 * Bit zero is used first; the default pattern is all 1's
 * @note The stipple factor is @c 2 for this function
 */
void R_DrawRect (int x, int y, int w, int h, const vec4_t color, float lineWidth, int pattern)
{
	const float nx = x * viddef.rx;
	const float ny = y * viddef.ry;
	const float nw = w * viddef.rx;
	const float nh = h * viddef.ry;

	R_Color(color);

	glDisable(GL_TEXTURE_2D);
	glLineWidth(lineWidth);
	glLineStipple(2, pattern);
	glEnable(GL_LINE_STIPPLE);

	glBegin(GL_LINE_LOOP);
	glVertex2f(nx, ny);
	glVertex2f(nx + nw, ny);
	glVertex2f(nx + nw, ny + nh);
	glVertex2f(nx, ny + nh);
	glEnd();

	glEnable(GL_TEXTURE_2D);
	glLineWidth(1.0f);
	glDisable(GL_LINE_STIPPLE);

	R_Color(NULL);
}

/**
 * @brief Draw the day and night images of a flat geoscape
 * multitexture feature is used to blend the images
 * @sa R_Draw3DGlobe
 * @param[in] map The geoscape map to draw (can be changed in the campaign definition)
 * @param[in] iz The zoomlevel of the geoscape - see ccs.zoom
 * @param[in] cx The x texture coordinate
 * @param[in] cy The y texture coordinate
 * @param[in] p The horizontal shift of the night map
 * @param[in] q The angle the sun is standing against the equator on earth
 * @param[in] x The x position of the geoscape node
 * @param[in] y The y position of the geoscape node
 * @param[in] w The width of the geoscape node
 * @param[in] h The height of the geoscape node
 */
void R_DrawFlatGeoscape (int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, const char *map)
{
	image_t *gl;
	float geoscape_texcoords[4 * 2];
	short geoscape_verts[4 * 2];

	/* normalize */
	const float nx = x * viddef.rx;
	const float ny = y * viddef.ry;
	const float nw = w * viddef.rx;
	const float nh = h * viddef.ry;

	/* load day image */
	gl = R_FindImage(va("pics/geoscape/%s_day", map), it_wrappic);
	if (gl == r_noTexture)
		Com_Error(ERR_FATAL, "Could not load geoscape day image");

	/* alter the array pointers */
	glVertexPointer(2, GL_SHORT, 0, geoscape_verts);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, geoscape_texcoords);

	geoscape_texcoords[0] = cx - iz;
	geoscape_texcoords[1] = cy - iz;
	geoscape_texcoords[2] = cx + iz;
	geoscape_texcoords[3] = cy - iz;
	geoscape_texcoords[4] = cx + iz;
	geoscape_texcoords[5] = cy + iz;
	geoscape_texcoords[6] = cx - iz;
	geoscape_texcoords[7] = cy + iz;

	geoscape_verts[0] = nx;
	geoscape_verts[1] = ny;
	geoscape_verts[2] = nx + nw;
	geoscape_verts[3] = ny;
	geoscape_verts[4] = nx + nw;
	geoscape_verts[5] = ny + nh;
	geoscape_verts[6] = nx;
	geoscape_verts[7] = ny + nh;

	/* draw day image */
	R_BindTexture(gl->texnum);
	glDrawArrays(GL_QUADS, 0, 4);

	/* draw night map */
	gl = R_FindImage(va("pics/geoscape/%s_night", map), it_wrappic);
	/* maybe the campaign map doesn't have a night image */
	if (gl != r_noTexture) {
		float geoscape_nighttexcoords[4 * 2];

		R_BindTexture(gl->texnum);
		R_EnableTexture(&texunit_lightmap, qtrue);
		R_SelectTexture(&texunit_lightmap);

		geoscape_nighttexcoords[0] = geoscape_texcoords[0] + p;
		geoscape_nighttexcoords[1] = geoscape_texcoords[1];
		geoscape_nighttexcoords[2] = geoscape_texcoords[2] + p;
		geoscape_nighttexcoords[3] = geoscape_texcoords[3];
		geoscape_nighttexcoords[4] = geoscape_texcoords[4] + p;
		geoscape_nighttexcoords[5] = geoscape_texcoords[5];
		geoscape_nighttexcoords[6] = geoscape_texcoords[6] + p;
		geoscape_nighttexcoords[7] = geoscape_texcoords[7];

		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, geoscape_nighttexcoords);

		R_BindTexture(r_dayandnightTexture->texnum);
		if (lastQ != q) {
			R_CalcAndUploadDayAndNightTexture(q);
			lastQ = q;
		}

		R_SelectTexture(&texunit_diffuse);
		glDrawArrays(GL_QUADS, 0, 4);

		R_SelectTexture(&texunit_lightmap);
		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, geoscape_texcoords);

		R_EnableTexture(&texunit_lightmap, qfalse);
	}

	/* draw nation overlay */
	if (r_geoscape_overlay->integer & OVERLAY_NATION) {
		gl = R_FindImage(va("pics/geoscape/%s_nations_overlay", map), it_wrappic);
		if (gl == r_noTexture)
			Com_Error(ERR_FATAL, "Could not load geoscape nation overlay image");

		/* draw day image */
		R_BindTexture(gl->texnum);
		glDrawArrays(GL_QUADS, 0, 4);
	}

	/* draw XVI image */
	if (r_geoscape_overlay->integer & OVERLAY_XVI) {
		gl = R_FindImage(va("pics/geoscape/%s_xvi_overlay", map), it_wrappic);
		if (gl == r_noTexture)
			Com_Error(ERR_FATAL, "Could not load xvi overlay image");

		R_BindTexture(gl->texnum);

		R_EnableTexture(&texunit_lightmap, qtrue);
		R_BindLightmapTexture(r_xviTexture->texnum);

		glDrawArrays(GL_QUADS, 0, 4);

		R_EnableTexture(&texunit_lightmap, qfalse);
	}

	/* draw radar image */
	if (r_geoscape_overlay->integer & OVERLAY_RADAR) {
		R_BindTexture(r_radarTexture->texnum);
		glDrawArrays(GL_QUADS, 0, 4);
	}

	/* and restore them */
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
}

/**
 * @brief Draws a circle out of lines
 * @param[in] mid Center of the circle
 * @param[in] radius Radius of the circle
 * @param[in] color The color of the circle lines
 * @sa R_DrawPtlCircle
 * @sa R_DrawLineStrip
 */
void R_DrawCircle (vec3_t mid, float radius, const vec4_t color, int thickness)
{
	float theta;
	const float accuracy = 5.0;
	const float step = M_PI / radius * accuracy;

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LINE_SMOOTH);

	R_Color(color);

	assert(radius > thickness);

	/* scale it */
	radius *= viddef.rx;
	thickness *= viddef.rx;

	/* store the matrix - we are using glTranslate */
	glPushMatrix();

	/* translate the position */
	glTranslated(mid[0], mid[1], mid[2]);

	if (thickness <= 1) {
		glBegin(GL_LINE_STRIP);
		for (theta = 0.0; theta <= 2.0 * M_PI; theta += step) {
			glVertex3f(radius * cos(theta), radius * sin(theta), 0.0);
		}
		glEnd();
	} else {
		glBegin(GL_TRIANGLE_STRIP);
		for (theta = 0.0; theta <= 2.0 * M_PI; theta += step) {
			glVertex3f(radius * cos(theta), radius * sin(theta), 0.0);
			glVertex3f(radius * cos(theta - step), radius * sin(theta - step), 0.0);
			glVertex3f((radius - thickness) * cos(theta - step), (radius - thickness) * sin(theta - step), 0.0);
			glVertex3f((radius - thickness) * cos(theta), (radius - thickness) * sin(theta), 0.0);
		}
		glEnd();
	}

	glPopMatrix();

	R_Color(NULL);

	glDisable(GL_LINE_SMOOTH);
	glEnable(GL_TEXTURE_2D);
}

#define CIRCLE_LINE_COUNT	40

/**
 * @brief Draws a circle out of lines
 * @param[in] x X screen coordinate
 * @param[in] y Y screen coordinate
 * @param[in] radius Radius of the circle
 * @param[in] color The color of the circle lines
 * @param[in] fill Fill the circle with the given color
 * @param[in] thickness The thickness of the lines
 * @sa R_DrawPtlCircle
 * @sa R_DrawLineStrip
 */
void R_DrawCircle2D (int x, int y, float radius, qboolean fill, const vec4_t color, float thickness)
{
	int i;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_TEXTURE_2D);
	R_Color(color);

	if (thickness > 0.0)
		glLineWidth(thickness);

	if (fill)
		glBegin(GL_TRIANGLE_STRIP);
	else
		glBegin(GL_LINE_LOOP);

	/* Create a vertex at the exact position specified by the start angle. */
	glVertex2f(x + radius, y);

	for (i = 0; i < CIRCLE_LINE_COUNT; i++) {
		const float angle = (2.0 * M_PI / CIRCLE_LINE_COUNT) * i;
		glVertex2f(x + radius * cos(angle), y - radius * sin(angle));

		/* When filling we're drawing triangles so we need to
		 * create a vertex in the middle of the vertex to fill
		 * the entire pie slice/circle. */
		if (fill)
			glVertex2f(x, y);
	}

	glVertex2f(x + radius, y);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	R_Color(NULL);

	glPopAttrib();
}

#define MAX_LINEVERTS 256

static inline void R_Draw2DArray (int points, int *verts, GLenum mode)
{
	int i;

	/* fit it on screen */
	if (points > MAX_LINEVERTS * 2)
		points = MAX_LINEVERTS * 2;

	/* set vertex array pointer */
	glVertexPointer(2, GL_SHORT, 0, r_state.vertex_array_2d);

	for (i = 0; i < points * 2; i += 2) {
		r_state.vertex_array_2d[i] = verts[i] * viddef.rx;
		r_state.vertex_array_2d[i + 1] = verts[i + 1] * viddef.ry;
	}

	glDisable(GL_TEXTURE_2D);
	glDrawArrays(mode, 0, points);
	glEnable(GL_TEXTURE_2D);
	glVertexPointer(3, GL_FLOAT, 0, r_state.vertex_array_3d);
}

/**
 * @brief 2 dimensional line strip
 * @sa R_DrawCircle
 * @sa R_DrawLineLoop
 */
void R_DrawLineStrip (int points, int *verts)
{
	R_Draw2DArray(points, verts, GL_LINE_STRIP);
}

/**
 * @sa R_DrawCircle
 * @sa R_DrawLineStrip
 */
void R_DrawLineLoop (int points, int *verts)
{
	R_Draw2DArray(points, verts, GL_LINE_LOOP);
}

/**
 * @brief Draws one line with only one start and one end point
 * @sa R_DrawCircle
 * @sa R_DrawLineStrip
 */
void R_DrawLine (int *verts, float thickness)
{
	if (thickness > 0.0)
		glLineWidth(thickness);

	R_Draw2DArray(2, verts, GL_LINES);

	if (thickness > 0.0)
		glLineWidth(1.0);
}

/**
 * @sa R_DrawCircle
 * @sa R_DrawLineStrip
 * @sa R_DrawLineLoop
 */
void R_DrawPolygon (int points, int *verts)
{
	R_Draw2DArray(points, verts, GL_POLYGON);
}

#define MARKER_SIZE 60.0

/**
 * @brief Draw 3D Marker on the 2D geoscape.
 * @param[in] screenPos Position on screenlongitude and latitude of the model to draw.
 * @param[in] direction angle giving the direction the model is heading toward.
 */
void R_Draw2DMapMarkers (const vec2_t screenPos, float direction, const char *model, int skin)
{
	modelInfo_t mi;
	vec2_t size;
	vec3_t scale, center, position, angles;
	float zoom = 0.4f;

	memset(&mi, 0, sizeof(mi));
	VectorCopy(vec3_origin, position);
	VectorCopy(vec3_origin, angles);

	mi.model = R_RegisterModelShort(model);
	if (!mi.model) {
		Com_Printf("Could not find model '%s'\n", model);
		return;
	}

	mi.name = model;
	mi.origin = position;
	mi.angles = angles;
	mi.skin = skin;

	size[0] = size[1] = MARKER_SIZE * zoom;
	R_ModelAutoScale(size, &mi, scale, center);
	/* reset the center, as we want to place the models onto the surface of the earth */
	mi.center = NULL;

	/* go to a new matrix */
	glPushMatrix();

	/* Apply all transformation to model. Note that the transformations are applied starting
	 * from the last one and ending with the first one */

	/* move model to its location */
	glTranslatef(screenPos[0]* viddef.rx, screenPos[1]* viddef.ry, 0);
	/* scale model to proper resolution */
	glScalef(viddef.rx, viddef.ry, 1.0f);
	/* rotate model to proper direction. */
	glRotatef(180.0f, 0, 1, 0);
	glRotatef(90.f - direction, 0, 0, 1);

	R_DrawModelDirect(&mi, NULL, NULL);

	/* restore previous matrix */
	glPopMatrix();
}

/**
 * @brief Draw 3D Marker on the 3D geoscape.
 * @param[in] x menu node x position
 * @param[in] y menu node y position
 * @param[in] w menu node widht
 * @param[in] h menu node height
 * @param[in] rotate vector giving the angles of earth rotation due to player view.
 * @param[in] pos longitude and latitude of the model to draw.
 * @param[in] direction angle giving the direction the model is heading toward.
 * @param[in] earthRadius Radius of earth on screen (this include zoom).
 */
void R_Draw3DMapMarkers (int x, int y, int w, int h, const vec3_t rotate, const vec2_t pos, float direction, float earthRadius, const char *model, int skin)
{
	/* normalize */
	const float nx = x * viddef.rx;
	const float ny = y * viddef.ry;
	const float nw = w * viddef.rx;
	const float nh = h * viddef.ry;

	/* Earth center is in the middle of node.
	 * Due to Orthographic view, this is also camera position */
	const vec3_t earthPos = {nx + nw / 2.0, ny + nh / 2.0, 0.0};

	modelInfo_t mi;
	vec2_t size;
	vec3_t scale, center, position, angles;
	float zoom = 0.4f;

	memset(&mi, 0, sizeof(mi));
	VectorCopy(vec3_origin, position);
	VectorCopy(vec3_origin, angles);

	mi.model = R_RegisterModelShort(model);
	if (!mi.model) {
		Com_Printf("Could not find model '%s'\n", model);
		return;
	}

	mi.name = model;
	mi.origin = position;
	mi.angles = angles;
	mi.skin = skin;

	size[0] = size[1] = MARKER_SIZE * zoom;
	R_ModelAutoScale(size, &mi, scale, center);
	/* reset the center, as we want to place the models onto the surface of the earth */
	mi.center = NULL;

	/* go to a new matrix */
	glPushMatrix();

	/* Apply all transformation to model. Note that the transformations are applied starting
	 * from the last one and ending with the first one */

	/* center model on earth. Translate also along z to avoid seeing
	 * bottom part of the model through earth (only half of earth is drawn) */
	glTranslatef(earthPos[0], earthPos[1], 10.0f);
	/* scale model to proper resolution */
	glScalef(viddef.rx, viddef.ry, 1.0f);
	/* place model on earth: make it tangent to earth surface, heading toward it if direction is used. */
	glRotatef(-rotate[1], 1, 0, 0);
	glRotatef(rotate[2], 0, 1, 0);
	glRotatef(rotate[0] - pos[0], 0, 0, 1);
	glRotatef(90.0f - pos[1], 1, 0, 0);
	glTranslatef(0, 0, earthRadius);
	glRotatef(-90.0f + direction, 0, 0, 1);

	R_DrawModelDirect(&mi, NULL, NULL);

	/* restore previous matrix */
	glPopMatrix();
}

/**
 * @brief Center position of skybox along z-axis. This is used to make sure we see only the inside of Skybox.
 * @sa R_DrawStarfield
 * @sa R_Setup2D
 */
const float SKYBOX_DEPTH = -9999.0f;

/**
 * @brief Half size of Skybox.
 * @note The bigger, the less perspective default you'll have, but the more you'll
 * zoom on the texture (and see it's default).
 * @sa R_DrawStarfield
 * @sa R_Setup2D
 */
#define SKYBOX_HALFSIZE 800.0f

static const float starFieldVerts[] = {
	/* face 1 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,

	/* face 2 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,

	/* face 3 */
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,

	/* face 4 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,

	/* face 5 */
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,

	/* face 6 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE
};

static const float starFieldTexCoords[] = {
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
};

/**
 * @brief Bind and draw starfield.
 * @param[in] texnum The texture id (already uploaded of course)
 * @note We draw a skybox: the camera is inside a cube rotating at earth rotation speed
 * (stars seems to rotate because we see earth as idle, but in reality stars are statics
 * and earth rotate around itself)
 * @sa R_Setup2D
 * @sa R_Draw3DGlobe
 */
static void R_DrawStarfield (int texnum, const vec3_t pos, const vec3_t rotate, float p)
{
	vec3_t angle;		/**< Angle of rotation of starfield */

	/* go to a new matrix */
	glPushMatrix();

	/* we must center the skybox on the camera border of view, and not on the earth, in order
	 * to see only the inside of the cube */
	glTranslatef(pos[0], pos[1], -SKYBOX_DEPTH);

	/* rotates starfield: only time and rotation of earth around itself causes starfield to rotate. */
	VectorSet(angle, rotate[0] - p * todeg, rotate[1], rotate[2]);
	glRotatef(angle[YAW], 1, 0, 0);
	glRotatef(angle[ROLL], 0, 1, 0);
	glRotatef(angle[PITCH], 0, 0, 1);

	R_BindTexture(texnum);

	/* alter the array pointers */
	glVertexPointer(3, GL_FLOAT, 0, starFieldVerts);
	glTexCoordPointer(2, GL_FLOAT, 0, starFieldTexCoords);

	/* draw the cube */
	glDrawArrays(GL_QUADS, 0, 24);

	/* restore previous matrix */
	glPopMatrix();
}

/**
 * @brief rotate a planet (sun or moon) with respect to the earth
 * @param[in] v
 * @param[out] r
 * @param[int] rotate
 * @param[int] earthPos
 * @param[int] celestialDist
 */
static inline void RotateCelestialBody (const vec4_t v, vec4_t * r, const vec3_t rotate, const vec3_t earthPos, const float celestialDist)
{
	vec4_t v1;
	vec4_t v2;
	vec3_t rotationAxis;

	VectorSet(v2, v[1], v[0], v[2]);
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v1, rotationAxis, v2, -rotate[PITCH]);
	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v2, rotationAxis, v1, -rotate[YAW]);

	Vector4Set((*r), earthPos[0] + celestialDist * v2[1], earthPos[1] + celestialDist * v2[0], -celestialDist * v2[2], 0);
}


/**
 * @brief responsible for drawing the 3d globe on geoscape
 * @param[in] x menu node x position
 * @param[in] y menu node y position
 * @param[in] w menu node widht
 * @param[in] h menu node height
 * @param[in] rotate the rotate angle of the globe
 * @param[in] zoom the current globe zoon
 * @param[in] map the prefix of the map to use (image must be at base/pics/menu/\<map\>_[day|night])
 * @sa R_DrawFlatGeoscape
 * @sa R_SphereGenerate
 */
void R_Draw3DGlobe (int x, int y, int w, int h, int day, int second, const vec3_t rotate, float zoom, const char *map, qboolean disableSolarRender, float ambient)
{
	/* globe scaling */
	const float fullscale = zoom / STANDARD_3D_ZOOM;

	/* lighting colors */
	static const vec4_t diffuseLightColor = { 1.75f, 1.75f, 1.75f, 1.0f };
	static const vec4_t specularLightColor = { 2.0f, 1.9f, 1.7f, 1.0f };
	static const vec4_t darknessLightColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	static const vec4_t brightDiffuseLightColor = { 5.0f, 5.0f, 5.0f, 1.0f };
	const vec4_t ambientLightColor = { ambient + 0.2f, ambient + 0.2f, ambient + 0.2f, ambient + 0.2f };
	/* billboard textures */
	image_t *starfield;
	image_t *halo;
	image_t *sun;
	image_t *sunOverlay;

	/* set distance of the sun and moon to make them static on starfield when
	 * time is stoped.  this distance should be used for any celestial body
	 * considered at infinite location (sun, moon) */
	static const float celestialDist = 1.37f * SKYBOX_HALFSIZE;
	static const float moonSize = 0.025f;
	vec4_t sunPos;
	vec4_t antiSunPos;
	vec4_t moonLoc;
	vec4_t sunLoc;

	/* normalize */
	const float nx = x * viddef.rx;
	const float ny = y * viddef.ry;
	const float nw = w * viddef.rx;
	const float nh = h * viddef.ry;

	/* Earth center is in the middle of node.
	 * Due to Orthographic view, this is also camera position */
	const vec3_t earthPos = { nx + nw / 2.0, ny + nh / 2.0, 0.0 };

	/* estimate the progress through the current season so we can do
	 * smooth transitions between textures.  Currently there are 12
	 * "seasons", because we have one image per Earth-month. */
	const float season = (float) (day % DAYS_PER_YEAR) / ((float) (DAYS_PER_YEAR) / (float) (SEASONS_PER_YEAR));
	const int currSeason = (int) floorf(season) % SEASONS_PER_YEAR;
	const int nextSeason = (int) ceilf(season) % SEASONS_PER_YEAR;
	const float seasonProgress = season - (float) currSeason;

	/* Compute sun position in absolute frame */
	const float q = (day % DAYS_PER_YEAR * SECONDS_PER_DAY + second) * (2 * M_PI / (SECONDS_PER_DAY * DAYS_PER_YEAR));	/* sun rotation (year) */
	const float a = cos(q) * SIN_ALPHA;	/* due to earth obliquity */
	const float sqrta = sqrt(0.5f * (1 - a * a));

	/* earth rotation (day) */
	const float p = (second - SECONDS_PER_DAY / 4) * (2.0 * M_PI / SECONDS_PER_DAY);
	/* lunar orbit */
	const float m = p + (((double)((10 * day % 249) / 10.0) + ((double)second / (double)SECONDS_PER_DAY)) / 24.9) * (2.0 * M_PI);

	glPushMatrix();
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_LIGHTING);
	/* draw the starfield, rotating with the planet */
	starfield = R_FindImage(va("pics/geoscape/%s_stars", map), it_wrappic);
	if (starfield != r_noTexture)
		R_DrawStarfield(starfield->texnum, earthPos, rotate, p);

	glPopMatrix();

	/* set up position vectors for celestial bodies */
	Vector4Set(sunPos, cos(p) * sqrta, -sin(p) * sqrta, a, 0);
	Vector4Set(antiSunPos, -cos(p) * sqrta, sin(p) * sqrta, -a, 0);

	/* Rotate the sun in the relative frame of player view, to get sun location */
	RotateCelestialBody(sunPos, &sunLoc, rotate, earthPos, 1.0);
	/* load sun texture image */
	sun = R_FindImage(va("pics/geoscape/%s_sun", map), it_wrappic);
	sunOverlay = R_FindImage(va("pics/geoscape/%s_sun_overlay", map), it_pic);
	if (sun != r_noTexture && sunOverlay != r_noTexture && sunLoc[2] > 0 && !disableSolarRender) {
		const int sunx = earthPos[0] + viddef.rx * (-128.0 + celestialDist * (sunLoc[0] - earthPos[0]));
		const int suny = earthPos[1] + viddef.ry * (-128.0 + celestialDist * (sunLoc[1] - earthPos[1]));

		R_DrawTexture(sunOverlay->texnum, sunx, suny, 256.0 * viddef.rx, 256.0 * viddef.ry);
		R_DrawBuffers(2);
		R_DrawTexture(sun->texnum, sunx, suny, 256.0 * viddef.rx, 256.0 * viddef.ry);
		R_DrawBuffers(1);
	}

	/* calculate position of the moon (it rotates around earth with a period of
	 * about 24.9 h, and we must take day into account to avoid moon to "jump"
	 * every time the day is changing) */
	VectorSet(moonLoc, cos(m) * sqrta, -sin(m) * sqrta, a);
	RotateCelestialBody(moonLoc, &moonLoc, rotate, earthPos, celestialDist);

	/* free last month's texture image */
	if (r_globeEarth.season != currSeason) {
		r_globeEarth.season = currSeason;
		R_FreeImage(r_globeEarth.texture);
	}

	/* load diffuse texture map (with embedded night-glow map as alpha channel) */
	r_globeEarth.texture = R_FindImage(va("pics/geoscape/%s/%s_season_%02d", r_config.lodDir, map, currSeason), it_wrappic);
	if (r_globeEarth.texture == r_noTexture)
		Com_Error(ERR_FATAL, "Could not find pics/geoscape/%s/%s_season_%02d\n", r_config.lodDir, map, currSeason);

	/* set up for advanced GLSL rendering if we have the capability */
	if (r_programs->integer) {
		r_globeEarth.glslProgram = r_state.geoscape_program;
		/* load earth image for the next month so we can blend them */
		r_globeEarth.blendTexture = R_FindImage(va("pics/geoscape/%s/%s_season_%02d", r_config.lodDir, map, nextSeason), it_wrappic);
		if (r_globeEarth.blendTexture == r_noTexture)
			Com_Error(ERR_FATAL, "Could not find pics/geoscape/%s/%s_season_%02d\n", r_config.lodDir, map, nextSeason);

		/* load normal map (with embedded gloss map as alpha channel) */
		r_globeEarth.normalMap = R_FindImage(va("pics/geoscape/%s/%s_bump", r_config.lodDir, map), it_wrappic);
		if (r_globeEarth.normalMap == r_noTexture)
			r_globeEarth.normalMap = NULL;

		/* weight the blending based on how much of the month has elapsed */
		r_globeEarth.blendScale = seasonProgress;
		/* set up lights for nighttime city glow */
		VectorCopy(antiSunPos, r_globeEarth.nightLightPos);
		glLightfv(GL_LIGHT1, GL_AMBIENT, darknessLightColor);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, brightDiffuseLightColor);
		glLightfv(GL_LIGHT1, GL_SPECULAR, darknessLightColor);

		r_globeEarth.glowScale = 0.7;
	}

	/* load moon texture image */
	r_globeMoon.texture = R_FindImage(va("pics/geoscape/%s_moon", map), it_wrappic);

	/* globe texture scaling */
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(2, 1, 1);
	glMatrixMode(GL_MODELVIEW);

	/* enable the lighting */
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLightColor);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLightColor);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specularLightColor);

	/* draw the moon */
	if (r_globeMoon.texture != r_noTexture && moonLoc[2] > 0 && !disableSolarRender)
		R_SphereRender(&r_globeMoon, moonLoc, rotate, moonSize, sunPos);

	/* activate depth to hide 3D models behind earth */
	glEnable(GL_DEPTH_TEST);

	/* draw the earth */
	R_DrawBuffers(2);
	if (r_programs->integer == 0) /* ignore alpha channel, since the city-light map is stored there */
		glBlendFunc(GL_ONE, GL_ZERO);

	R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);

	if (r_programs->integer == 0) /* restore default blend function */
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	r_globeEarthAtmosphere.texture = R_FindImage(va("pics/geoscape/%s_atmosphere", map), it_wrappic);

	/* Draw earth atmosphere */
	if (r_programs->integer && r_postprocess->integer) {
		r_globeEarthAtmosphere.normalMap = r_globeEarth.normalMap;
		r_globeEarthAtmosphere.glowScale = 1.0;
		r_globeEarthAtmosphere.blendScale = -1.0;
		r_globeEarthAtmosphere.glslProgram = r_state.atmosphere_program;
		R_SphereRender(&r_globeEarthAtmosphere, earthPos, rotate, fullscale, sunPos);
	} else {
		halo = R_FindImage("pics/geoscape/map_earth_halo", it_pic);
		if (halo != r_noTexture) {
			/** @todo Replace this magic number with some speaking constant */
			const float earthSizeX = fullscale * 20500.0 * viddef.rx;
			const float earthSizeY = fullscale * 20500.0 * viddef.ry;
			glMatrixMode(GL_TEXTURE);
			glPushMatrix();
			glLoadIdentity();
			glDisable(GL_LIGHTING);

			R_DrawTexture(halo->texnum, earthPos[0] - earthSizeX * 0.5, earthPos[1] - earthSizeY * 0.5, earthSizeX, earthSizeY);
			glEnable(GL_LIGHTING);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
		}
	}

	R_DrawBuffers(1);
	glDisable(GL_DEPTH_TEST);

	/* draw nation overlay */
	if (r_geoscape_overlay->integer & OVERLAY_NATION) {
		r_globeEarth.overlay = R_FindImage(va("pics/geoscape/%s_nations_overlay", map), it_wrappic);
		if (r_globeEarth.overlay == r_noTexture)
			Com_Error(ERR_FATAL, "Could not load geoscape nation overlay image");

		R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);

		/* draw glowing borders */
		r_globeEarth.overlay = R_FindImage(va("pics/geoscape/%s_nations_overlay_glow", map), it_wrappic);
		if (r_globeEarth.overlay == r_noTexture)
			Com_Error(ERR_FATAL, "Could not load geoscape nation overlay glow image");

		R_DrawBuffers(2);
		glDisable(GL_LIGHTING);
		R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);
		glEnable(GL_LIGHTING);
		R_DrawBuffers(1);

		r_globeEarth.overlay = NULL;
	}
	/* draw XVI overlay */
	if (r_geoscape_overlay->integer & OVERLAY_XVI) {
		assert(r_xviTexture);
		r_globeEarth.overlay = R_FindImage(va("pics/geoscape/%s_xvi_overlay", map), it_wrappic);
		r_globeEarth.overlayAlphaMask = r_xviTexture;
		R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);
		r_globeEarth.overlayAlphaMask = NULL;
		r_globeEarth.overlay = NULL;
	}
	/* draw radar overlay */
	if (r_geoscape_overlay->integer & OVERLAY_RADAR) {
		assert(r_radarTexture);
		r_globeEarth.overlay = r_radarTexture;
		R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);
		r_globeEarth.overlay = NULL;
	}


	/* disable 3d geoscape lighting */
	glDisable(GL_LIGHTING);

	/* restore the previous matrix */
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}

typedef struct {
	int x;
	int y;
	int width;
	int height;
} rect_t;

#define MAX_CLIPRECT 16

static rect_t clipRect[MAX_CLIPRECT];

static int currentClipRect = 0;

/**
 * @brief Compute the intersection of 2 rect
 * @param[in] a A rect
 * @param[in] b A rect
 * @param[out] out The intersection rect
 */
static void R_RectIntersection (const rect_t* a, const rect_t* b, rect_t* out)
{
	out->x = (a->x > b->x) ? a->x : b->x;
	out->y = (a->y > b->y) ? a->y : b->y;
	out->width = ((a->x + a->width < b->x + b->width) ? a->x + a->width : b->x + b->width) - out->x;
	out->height = ((a->y + a->height < b->y + b->height) ? a->y + a->height : b->y + b->height) - out->y;
	if (out->width < 0)
		out->width = 0;
	if (out->height < 0)
		out->height = 0;
}

/**
 * @brief Force to draw only on a rect
 * @note Don't forget to call @c R_EndClipRect
 * @sa R_PopClipRect
 */
void R_PushClipRect (int x, int y, int width, int height)
{
	const int depth = currentClipRect;
	assert(depth < MAX_CLIPRECT);

	if (depth == 0) {
		clipRect[depth].x = x * viddef.rx;
		clipRect[depth].y = (viddef.virtualHeight - (y + height)) * viddef.ry;
		clipRect[depth].width = width * viddef.rx;
		clipRect[depth].height = height * viddef.ry;
	} else {
		rect_t rect;
		rect.x = x * viddef.rx;
		rect.y = (viddef.virtualHeight - (y + height)) * viddef.ry;
		rect.width = width * viddef.rx;
		rect.height = height * viddef.ry;
		R_RectIntersection(&clipRect[depth - 1], &rect, &clipRect[depth]);
	}

	glScissor(clipRect[depth].x, clipRect[depth].y, clipRect[depth].width, clipRect[depth].height);

	if (currentClipRect == 0)
		glEnable(GL_SCISSOR_TEST);
	currentClipRect++;
}

/**
 * @sa R_PushClipRect
 */
void R_PopClipRect (void)
{
	assert(currentClipRect > 0);
	currentClipRect--;
	if (currentClipRect == 0)
		glDisable(GL_SCISSOR_TEST);
	else {
		const int depth = currentClipRect - 1;
		glScissor(clipRect[depth].x, clipRect[depth].y, clipRect[depth].width, clipRect[depth].height);
	}
}

/**
 * @brief "Clean up" the depth buffer into a rect
 * @note we use a big value (but not too big) to set the depth buffer, then it is not really a clean up
 * @todo can we fix bigZ with a value come from glGet?
 */
void R_CleanupDepthBuffer (int x, int y, int width, int height)
{
	const int nx = x * viddef.rx;
	const int ny = y * viddef.ry;
	const int nwidth = width * viddef.rx;
	const int nheight = height * viddef.ry;
	const GLboolean hasDepthTest = glIsEnabled(GL_DEPTH_TEST);
	const GLdouble bigZ = 2000;
	GLint depthFunc;
	glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);

	/* we want to overwrite depth buffer not to have his constraints */
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_ALWAYS);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	glBegin(GL_QUADS);
	glVertex3d(nx, ny, bigZ);
	glVertex3d(nx + nwidth, ny, bigZ);
	glVertex3d(nx + nwidth, ny + nheight, bigZ);
	glVertex3d(nx, ny + nheight, bigZ);
	glEnd();

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	if (!hasDepthTest)
		glDisable(GL_DEPTH_TEST);
	glDepthFunc(depthFunc);
}

/**
 * @brief Compute the bounding box for an entity out of the mins, maxs
 * @sa R_DrawBoundingBox
 */
static void R_ComputeBoundingBox (const vec3_t mins, const vec3_t maxs, vec3_t bbox[8])
{
	int i;

	/* compute a full bounding box */
	for (i = 0; i < 8; i++) {
		bbox[i][0] = (i & 1) ? mins[0] : maxs[0];
		bbox[i][1] = (i & 2) ? mins[1] : maxs[1];
		bbox[i][2] = (i & 4) ? mins[2] : maxs[2];
	}
}

/**
 * @brief Draws the model bounding box
 * @sa R_EntityComputeBoundingBox
 */
void R_DrawBoundingBox (const vec3_t mins, const vec3_t maxs)
{
	vec3_t bbox[8];

	R_ComputeBoundingBox(mins, maxs, bbox);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	/* Draw top and sides */
	glBegin(GL_TRIANGLE_STRIP);
	glVertex3fv(bbox[2]);
	glVertex3fv(bbox[1]);
	glVertex3fv(bbox[0]);
	glVertex3fv(bbox[1]);
	glVertex3fv(bbox[4]);
	glVertex3fv(bbox[5]);
	glVertex3fv(bbox[1]);
	glVertex3fv(bbox[7]);
	glVertex3fv(bbox[3]);
	glVertex3fv(bbox[2]);
	glVertex3fv(bbox[7]);
	glVertex3fv(bbox[6]);
	glVertex3fv(bbox[2]);
	glVertex3fv(bbox[4]);
	glVertex3fv(bbox[0]);
	glEnd();

	/* Draw bottom */
	glBegin(GL_TRIANGLE_STRIP);
	glVertex3fv(bbox[4]);
	glVertex3fv(bbox[6]);
	glVertex3fv(bbox[7]);
	glEnd();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

/**
 * @brief Draw the current texture on a quad the size of the renderbuffer
 */
static inline void R_DrawQuad (void)
{
	glBegin(GL_QUADS);
	glTexCoord2i(0, 1);
	glVertex2i(0, 0);
	glTexCoord2i(1, 1);
	glVertex2i(fbo_render->width, 0);
	glTexCoord2i(1, 0);
	glVertex2i(fbo_render->width, fbo_render->height);
	glTexCoord2i(0, 0);
	glVertex2i(0, fbo_render->height);
	glEnd();
}

/**
 * @brief does 1D filter convolution to blur a framebuffer texture.
 * dir=0 for horizontal, dir=1 for vertical
 */
static void R_Blur (r_framebuffer_t * source, r_framebuffer_t * dest, int tex, int dir)
{
	R_EnableBlur(r_state.convolve_program, qtrue, source, dest, dir);

	/* draw new texture onto a flat surface */
	R_BindTextureForTexUnit(source->textures[tex], &texunit_0);
	R_UseViewport(source);
	R_DrawQuad();

	R_EnableBlur(r_state.convolve_program, qfalse, NULL, NULL, 0);
}

/**
 * @brief blur from the source image pyramid into the dest image pyramid
 */
static void R_BlurStack (int levels, r_framebuffer_t ** sources, r_framebuffer_t ** dests)
{
	int i;

	for (i = 0; i < levels; i++) {
		const int l = levels - i - 1;

		R_UseProgram(i == 0 ? default_program : r_state.combine2_program);
		R_UseFramebuffer(dests[l]);
		R_BindTextureForTexUnit(sources[l]->textures[0], &texunit_0);
		if (i != 0)
			R_BindTextureForTexUnit(dests[l + 1]->textures[0], &texunit_1);

		R_UseViewport(sources[l]);
		R_DrawQuad();

		R_Blur(dests[l], sources[l], 0, 1);
		R_Blur(sources[l], dests[l], 0, 0);
	}
}

/**
 * @brief handle post-processing bloom
 */
void R_DrawBloom (void)
{
	int i;
	qboolean rb_enable;

	if (!r_config.frameBufferObject || !r_postprocess->integer || !r_programs->integer)
		return;

	/* save state, then set up for blit-style rendering to quads */
	rb_enable = R_RenderbufferEnabled();
	glPushAttrib(GL_ENABLE_BIT | GL_VIEWPORT_BIT);
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, viddef.width, viddef.height, 0, 9999.0f, SKYBOX_DEPTH);

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	/* downsample into image pyramid */
	R_BindTexture(fbo_render->textures[1]);
	qglGenerateMipmapEXT(GL_TEXTURE_2D);

	R_Blur(fbo_render, fbo_bloom0, 1, 0);
	R_Blur(fbo_bloom0, fbo_bloom1, 0, 1);

	R_UseFramebuffer(r_state.buffers0[0]);
	R_BindTexture(fbo_bloom1->textures[0]);
	qglGenerateMipmapEXT(GL_TEXTURE_2D);
	R_UseViewport(r_state.buffers0[0]);
	R_DrawQuad();

	for (i = 1; i < DOWNSAMPLE_PASSES; i++) {
		R_Blur(r_state.buffers0[i - 1], r_state.buffers1[i - 1], 0, 0);
		R_Blur(r_state.buffers1[i - 1], r_state.buffers2[i - 1], 0, 1);
		R_UseFramebuffer(r_state.buffers0[i]);
		R_BindTexture(r_state.buffers2[i - 1]->textures[0]);
		R_UseViewport(r_state.buffers0[i]);
		R_DrawQuad();
	}

	/* blur and combine downsampled images */
	R_BlurStack(DOWNSAMPLE_PASSES, r_state.buffers0, r_state.buffers1);

	/* re-combine the blurred version with the original "glow" image */
	R_UseProgram(r_state.combine2_program);
	R_UseFramebuffer(fbo_bloom0);
	R_BindTextureForTexUnit(fbo_render->textures[1], &texunit_0);
	R_BindTextureForTexUnit(r_state.buffers1[0]->textures[0], &texunit_1);

	R_UseViewport(fbo_screen);
	R_DrawQuad();

	/* draw final result to the screenbuffer */
	R_UseFramebuffer(fbo_screen);
	R_UseProgram(r_state.combine2_program);
	R_BindTextureForTexUnit(fbo_render->textures[0], &texunit_0);
	R_BindTextureForTexUnit(fbo_bloom0->textures[0], &texunit_1);

	R_DrawQuad();

	/* cleanup before returning */
	R_UseProgram(default_program);

	R_CheckError();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glPopAttrib();
	R_CheckError();

	/* reset renderbuffer state to what it was before */
	R_EnableRenderbuffer(rb_enable);
}
