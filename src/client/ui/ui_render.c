/**
 * @file ui_render.c
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "ui_main.h"
#include "ui_font.h"
#include "ui_render.h"
#include "../cl_video.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_misc.h"

/**
 * @brief Fills a box of pixels with a single color
 */
void UI_DrawFill (int x, int y, int w, int h, const vec4_t color)
{
	R_DrawFill(x, y, w, h, color);
}

/**
 * @brief Pushes a new matrix, normalize to current resolution and move, rotate and
 * scale the matrix to the given values.
 * @note Will pop the matrix if @c transform is @c NULL
 * @param transform Translation (if @c NULL the matrix is removed from stack)
 * @param rotate Rotation
 * @param scale Scale
 * @sa R_Transform
 * @sa R_PopMatrix
 * @sa R_PushMatrix
 */
void UI_Transform (const vec3_t transform, const vec3_t rotate, const vec3_t scale)
{
	vec3_t pos;

	if (transform != NULL) {
		R_PushMatrix();
		VectorCopy(transform, pos);
		pos[0] *= viddef.rx;
		pos[1] *= viddef.ry;

		R_Transform(pos, rotate, scale);
	} else {
		R_PopMatrix();
	}
}

/**
 * @brief Searches for an image in the image array
 * @param[in] name The name of the image relative to pics/
 * @note name may not be null and has to be longer than 4 chars
 * @return NULL on error or image_t pointer on success
 * @sa R_FindImage
 */
const struct image_s *UI_LoadImage (const char *name)
{
	const struct image_s *image = R_FindImage(va("pics/%s", name), it_pic);
	if (image == r_noTexture)
		return NULL;
	return image;
}

/**
 * @brief Searches for a wrapped image in the image array
 * @param[in] name The name of the image relative to pics/
 * @note name may not be null and has to be longer than 4 chars
 * @return NULL on error or image_t pointer on success
 * @sa R_FindImage
 */
const struct image_s *UI_LoadWrappedImage (const char *name)
{
	const struct image_s *image = R_FindImage(va("pics/%s", name), it_wrappic);
	if (image == r_noTexture)
		return NULL;
	return image;
}

/**
 * @brief Draw a normalized (to the screen) image
 * @param[in] x The x coordinate (normalized)
 * @param[in] y The x coordinate (normalized)
 * @param[in] w The width (normalized)
 * @param[in] h The height (normalized)
 * @param[in] sh The s part of the texture coordinate (horizontal)
 * @param[in] th The t part of the texture coordinate (horizontal)
 * @param[in] sl The s part of the texture coordinate (vertical)
 * @param[in] tl The t part of the texture coordinate (vertical)
 * @param[in] image The image to draw
 */
void UI_DrawNormImage (qboolean flip, float x, float y, float w, float h, float sh, float th, float sl, float tl, const image_t *image)
{
	float nw, nh, x1, x2, x3, x4, y1, y2, y3, y4;

	if (!image)
		return;

	/* normalize to the screen resolution */
	x1 = x * viddef.rx;
	y1 = y * viddef.ry;

	/* provided width and height (if any) take precedence */
	if (w)
		nw = w * viddef.rx;
	else
		nw = 0;

	if (h)
		nh = h * viddef.ry;
	else
		nh = 0;

	/* horizontal texture mapping */
	if (sh) {
		if (!w)
			nw = (sh - sl) * viddef.rx;
		sh /= image->width;
	} else {
		if (!w)
			nw = ((float)image->width - sl) * viddef.rx;
		sh = 1.0f;
	}
	sl /= image->width;

	/* vertical texture mapping */
	if (th) {
		if (!h)
			nh = (th - tl) * viddef.ry;
		th /= image->height;
	} else {
		if (!h)
			nh = ((float)image->height - tl) * viddef.ry;
		th = 1.0f;
	}
	tl /= image->height;

	/* fill the rest of the coordinates to make a rectangle */
	x4 = x1;
	x3 = x2 = x1 + nw;
	y2 = y1;
	y4 = y3 = y1 + nh;

	{
		if (flip) {
			const float tmp = sl;
			sl = sh;
			sh = tmp;
		}
		const vec2_t imageTexcoords[4] = {{sl, tl}, {sh, tl}, {sh, th}, {sl, th}};
		const vec2_t imageVerts[4] = {{x1, y1}, {x2, y2}, {x3, y3}, {x4, y4}};
		R_DrawImageArray(imageTexcoords, imageVerts, image);
	}
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
 * @param[in] name The name of the image - relative to base/pics
 * @sa R_RegisterImage
 * @note All these parameter are normalized to VID_NORM_WIDTH and VID_NORM_HEIGHT
 * they are adjusted in this function
 */
const image_t *UI_DrawNormImageByName (qboolean flip, float x, float y, float w, float h, float sh, float th, float sl, float tl, const char *name)
{
	const struct image_s *image;

	image = UI_LoadImage(name);
	if (!image) {
		Com_Printf("Can't find pic: %s\n", name);
		return NULL;
	}

	UI_DrawNormImage(flip, x, y, w, h, sh, th, sl, tl, image);
	return image;
}

/**
 * @brief draw a panel from a texture as we can see on the image
 * @image html http://ufoai.org/wiki/images/Inline_draw_panel.png
 * @param[in] pos Position of the output panel
 * @param[in] size Size of the output panel
 * @param[in] texture Texture contain the template of the panel
 * @param[in] texX Position x of the panel template into the texture
 * @param[in] texY Position y of the panel template into the texture
 * @param[in] panelDef Array of seven elements define the panel template used in the texture.
 * From the first to the last: left width, mid width, right width,
 * top height, mid height, bottom height, and margin
 * @todo can we improve the code? is it need?
 */
void UI_DrawPanel (const vec2_t pos, const vec2_t size, const char *texture, int texX, int texY, const int panelDef[7])
{
	const int leftWidth = panelDef[0];
	const int midWidth = panelDef[1];
	const int rightWidth = panelDef[2];
	const int topHeight = panelDef[3];
	const int midHeight = panelDef[4];
	const int bottomHeight = panelDef[5];
	const int marge = panelDef[6];

	/** @todo merge texX and texY here */
	const int firstPos = 0;
	const int secondPos = firstPos + leftWidth + marge;
	const int thirdPos = secondPos + midWidth + marge;
	const int firstPosY = 0;
	const int secondPosY = firstPosY + topHeight + marge;
	const int thirdPosY = secondPosY + midHeight + marge;

	int y, h;

	const image_t *image = UI_LoadImage(texture);
	if (!image)
		return;

	/* draw top (from left to right) */
	UI_DrawNormImage(qfalse, pos[0], pos[1], leftWidth, topHeight, texX + firstPos + leftWidth, texY + firstPosY + topHeight,
		texX + firstPos, texY + firstPosY, image);
	UI_DrawNormImage(qfalse, pos[0] + leftWidth, pos[1], size[0] - leftWidth - rightWidth, topHeight, texX + secondPos + midWidth, texY + firstPosY + topHeight,
		texX + secondPos, texY + firstPosY, image);
	UI_DrawNormImage(qfalse, pos[0] + size[0] - rightWidth, pos[1], rightWidth, topHeight, texX + thirdPos + rightWidth, texY + firstPosY + topHeight,
		texX + thirdPos, texY + firstPosY, image);

	/* draw middle (from left to right) */
	y = pos[1] + topHeight;
	h = size[1] - topHeight - bottomHeight; /* height of middle */
	UI_DrawNormImage(qfalse, pos[0], y, leftWidth, h, texX + firstPos + leftWidth, texY + secondPosY + midHeight,
		texX + firstPos, texY + secondPosY, image);
	UI_DrawNormImage(qfalse, pos[0] + leftWidth, y, size[0] - leftWidth - rightWidth, h, texX + secondPos + midWidth, texY + secondPosY + midHeight,
		texX + secondPos, texY + secondPosY, image);
	UI_DrawNormImage(qfalse, pos[0] + size[0] - rightWidth, y, rightWidth, h, texX + thirdPos + rightWidth, texY + secondPosY + midHeight,
		texX + thirdPos, texY + secondPosY, image);

	/* draw bottom (from left to right) */
	y = pos[1] + size[1] - bottomHeight;
	UI_DrawNormImage(qfalse, pos[0], y, leftWidth, bottomHeight, texX + firstPos + leftWidth, texY + thirdPosY + bottomHeight,
		texX + firstPos, texY + thirdPosY, image);
	UI_DrawNormImage(qfalse, pos[0] + leftWidth, y, size[0] - leftWidth - rightWidth, bottomHeight, texX + secondPos + midWidth, texY + thirdPosY + bottomHeight,
		texX + secondPos, texY + thirdPosY, image);
	UI_DrawNormImage(qfalse, pos[0] + size[0] - bottomHeight, y, rightWidth, bottomHeight, texX + thirdPos + rightWidth, texY + thirdPosY + bottomHeight,
		texX + thirdPos, texY + thirdPosY, image);
}

/**
 * @brief draw a line into a bounding box
 * @param[in] fontID the font id (defined in ufos/fonts.ufo)
 * @param[in] align Align of the text into the bounding box
 * @param[in] x Current x position of the bounded box
 * @param[in] y Current y position of the bounded box
 * @param[in] width Current width of the bounded box
 * @param[in] height Current height of the bounded box
 * @param[in] text The string to draw
 * @param[in] method Truncation method
 * @image html http://ufoai.org/wiki/images/Text_position.png
 * @note the x, y, width and height values are all normalized here - don't use the
 * viddef settings for drawstring calls - make them all relative to VID_NORM_WIDTH
 * and VID_NORM_HEIGHT
 * @todo remove the use of UI_DrawString
 * @todo test the code for multiline?
 * @todo fix problem with truncation (maybe problem into UI_DrawString)
 */
int UI_DrawStringInBox (const char *fontID, align_t align, int x, int y, int width, int height, const char *text, longlines_t method)
{
	const align_t horizontalAlign = align % 3; /* left, center, right */
	const align_t verticalAlign = align / 3;  /* top, center, bottom */

	/* position of the text for UI_DrawString */
	const int xx = x + ((width * horizontalAlign) >> 1);
	const int yy = y + ((height * verticalAlign) >> 1);

	return UI_DrawString(fontID, align, xx, yy, xx, width, 0, text, 0, 0, NULL, qfalse, method);
}

int UI_DrawString (const char *fontID, align_t align, int x, int y, int absX, int maxWidth,
		int lineHeight, const char *c, int boxHeight, int scrollPos, int *curLine, qboolean increaseLine, longlines_t method)
{
	const uiFont_t *font = UI_GetFontByID(fontID);
	const align_t verticalAlign = align / 3;  /* top, center, bottom */
	int lines;

	if (!font)
		Com_Error(ERR_FATAL, "Could not find font with id: '%s'", fontID);

	if (lineHeight <= 0)
		lineHeight = UI_FontGetHeight(font->name);

	/* vertical alignment makes only a single-line adjustment in this
	 * function. That means that ALIGN_Lx values will not show more than
	 * one line in any case. */
	if (verticalAlign == 1)
		y += -(lineHeight / 2);
	else if (verticalAlign == 2)
		y += -lineHeight;

	lines = R_FontDrawString(fontID, align, x, y, absX, maxWidth, lineHeight,
			c, boxHeight, scrollPos, curLine, method);

	if (curLine && increaseLine)
		*curLine += lines;

	return lines * lineHeight;
}
