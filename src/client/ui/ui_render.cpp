/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../client.h"
#include "../cl_video.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_misc.h"
#include "../renderer/r_state.h"

/**
 * @brief Fills a box of pixels with a single color
 */
void UI_DrawFill (int x, int y, int w, int h, const vec4_t color)
{
	R_DrawFill(x, y, w, h, color);
}

void UI_DrawRect (int x, int y, int w, int h, const vec4_t color, float lineWidth, int pattern)
{
	R_DrawRect(x, y, w, h, color, lineWidth, pattern);
}

void UI_PushClipRect (int x, int y, int width, int height)
{
	R_PushClipRect(x, y, width, height);
}

void UI_PopClipRect (void)
{
	R_PopClipRect();
}

/**
 * @brief Pushes a new matrix, normalize to current resolution and move, rotate and
 * scale the matrix to the given values.
 * @note Will pop the matrix if @c transform is @c nullptr
 * @param transform Translation (if @c nullptr the matrix is removed from stack)
 * @param rotate Rotation
 * @param scale Scale
 * @sa R_Transform
 * @sa R_PopMatrix
 * @sa R_PushMatrix
 */
void UI_Transform (const vec3_t transform, const vec3_t rotate, const vec3_t scale)
{
	vec3_t pos;

	if (transform != nullptr) {
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
 * @return nullptr on error or image_t pointer on success
 * @sa R_FindImage
 */
const struct image_s* UI_LoadImage (const char* name)
{
	const struct image_s* image = R_FindImage(va("pics/%s", name), it_pic);
	if (image == r_noTexture)
		return nullptr;
	return image;
}

/**
 * @brief Searches for a wrapped image in the image array
 * @param[in] name The name of the image relative to pics/
 * @note name may not be null and has to be longer than 4 chars
 * @return nullptr on error or image_t pointer on success
 * @sa R_FindImage
 */
const struct image_s* UI_LoadWrappedImage (const char* name)
{
	const struct image_s* image = R_FindImage(va("pics/%s", name), it_wrappic);
	if (image == r_noTexture)
		return nullptr;
	return image;
}

/**
 * @brief Draw a normalized (to the screen) image
 * @param[in] flip Flip the icon rendering (horizontal)
 * @param[in] x,y The coordinates (normalized)
 * @param[in] w The width (normalized)
 * @param[in] h The height (normalized)
 * @param[in] sh The s part of the texture coordinate (horizontal)
 * @param[in] th The t part of the texture coordinate (horizontal)
 * @param[in] sl The s part of the texture coordinate (vertical)
 * @param[in] tl The t part of the texture coordinate (vertical)
 * @param[in] image The image to draw
 */
void UI_DrawNormImage (bool flip, float x, float y, float w, float h, float sh, float th, float sl, float tl, const image_t* image)
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

	if (flip) {
		const float tmp = sl;
		sl = sh;
		sh = tmp;
	}
	const vec2_t imageTexcoords[4] = {{sl, tl}, {sh, tl}, {sh, th}, {sl, th}};
	const vec2_t imageVerts[4] = {{x1, y1}, {x2, y2}, {x3, y3}, {x4, y4}};
	R_DrawImageArray(imageTexcoords, imageVerts, image);
}

/**
 * @brief Draws an image or parts of it
 * @param[in] flip Flip the icon rendering (horizontal)
 * @param[in] x,y position to draw the image to
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
const image_t* UI_DrawNormImageByName (bool flip, float x, float y, float w, float h, float sh, float th, float sl, float tl, const char* name)
{
	const struct image_s* image;

	image = UI_LoadImage(name);
	if (!image) {
		Com_Printf("Can't find pic: %s\n", name);
		return nullptr;
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
void UI_DrawPanel (const vec2_t pos, const vec2_t size, const char* texture, int texX, int texY, const int panelDef[7])
{
	const image_t* image = UI_LoadImage(texture);
	if (!image)
		return;

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

	/* draw top (from left to right) */
	UI_DrawNormImage(false, pos[0], pos[1], leftWidth, topHeight, texX + firstPos + leftWidth, texY + firstPosY + topHeight,
		texX + firstPos, texY + firstPosY, image);
	UI_DrawNormImage(false, pos[0] + leftWidth, pos[1], size[0] - leftWidth - rightWidth, topHeight, texX + secondPos + midWidth, texY + firstPosY + topHeight,
		texX + secondPos, texY + firstPosY, image);
	UI_DrawNormImage(false, pos[0] + size[0] - rightWidth, pos[1], rightWidth, topHeight, texX + thirdPos + rightWidth, texY + firstPosY + topHeight,
		texX + thirdPos, texY + firstPosY, image);

	/* draw middle (from left to right) */
	const int yMiddle = pos[1] + topHeight;
	const int hMiddle = size[1] - topHeight - bottomHeight; /* height of middle */
	UI_DrawNormImage(false, pos[0], yMiddle, leftWidth, hMiddle, texX + firstPos + leftWidth, texY + secondPosY + midHeight,
		texX + firstPos, texY + secondPosY, image);
	UI_DrawNormImage(false, pos[0] + leftWidth, yMiddle, size[0] - leftWidth - rightWidth, hMiddle, texX + secondPos + midWidth, texY + secondPosY + midHeight,
		texX + secondPos, texY + secondPosY, image);
	UI_DrawNormImage(false, pos[0] + size[0] - rightWidth, yMiddle, rightWidth, hMiddle, texX + thirdPos + rightWidth, texY + secondPosY + midHeight,
		texX + thirdPos, texY + secondPosY, image);

	/* draw bottom (from left to right) */
	const int yBottom = pos[1] + size[1] - bottomHeight;
	UI_DrawNormImage(false, pos[0], yBottom, leftWidth, bottomHeight, texX + firstPos + leftWidth, texY + thirdPosY + bottomHeight,
		texX + firstPos, texY + thirdPosY, image);
	UI_DrawNormImage(false, pos[0] + leftWidth, yBottom, size[0] - leftWidth - rightWidth, bottomHeight, texX + secondPos + midWidth, texY + thirdPosY + bottomHeight,
		texX + secondPos, texY + thirdPosY, image);
	UI_DrawNormImage(false, pos[0] + size[0] - bottomHeight, yBottom, rightWidth, bottomHeight, texX + thirdPos + rightWidth, texY + thirdPosY + bottomHeight,
		texX + thirdPos, texY + thirdPosY, image);
}

/**
 * @brief draw a panel from a texture as we can see on the image
 * @image html http://ufoai.org/wiki/images/Inline_draw_panel.png
 * @param[in] pos Position of the output panel
 * @param[in] size Size of the output panel
 * @param[in] texture Texture contain the template of the panel
 * @param[in] texX,texY Position of the panel template into the texture
 * @param[in] texW,texH Width/height of the panel template into the texture
 * @param[in] border Size of unscalable border
 * From the first to the last: left width, mid width, right width,
 * top height, mid height, bottom height, and margin
 * @todo can we improve the code? is it need?
 */
void UI_DrawBorderedPanel (const vec2_t pos, const vec2_t size, const char* texture, int texX, int texY, int texW, int texH, int border)
{
	const image_t* image = UI_LoadImage(texture);
	if (!image)
		return;

	const int leftWidth = border;
	const int midWidth = texW - 2 * border;
	const int rightWidth = border;
	const int topHeight = border;
	const int midHeight = texH - 2 * border;
	const int bottomHeight = border;
	const int marge = 0;

	const int firstPos = texX;
	const int secondPos = firstPos + leftWidth + marge;
	const int thirdPos = secondPos + midWidth + marge;
	const int firstPosY = texY;
	const int secondPosY = firstPosY + topHeight + marge;
	const int thirdPosY = secondPosY + midHeight + marge;

	/* draw top (from left to right) */
	UI_DrawNormImage(false, pos[0], pos[1], leftWidth, topHeight, firstPos + leftWidth, firstPosY + topHeight,
		firstPos, firstPosY, image);
	UI_DrawNormImage(false, pos[0] + leftWidth, pos[1], size[0] - leftWidth - rightWidth, topHeight, secondPos + midWidth, firstPosY + topHeight,
		secondPos, firstPosY, image);
	UI_DrawNormImage(false, pos[0] + size[0] - rightWidth, pos[1], rightWidth, topHeight, thirdPos + rightWidth, firstPosY + topHeight,
		thirdPos, firstPosY, image);

	/* draw middle (from left to right) */
	const int yMiddle = pos[1] + topHeight;
	const int hMiddle = size[1] - topHeight - bottomHeight; /* height of middle */
	UI_DrawNormImage(false, pos[0], yMiddle, leftWidth, hMiddle, firstPos + leftWidth, secondPosY + midHeight,
		firstPos, secondPosY, image);
	UI_DrawNormImage(false, pos[0] + leftWidth, yMiddle, size[0] - leftWidth - rightWidth, hMiddle, secondPos + midWidth, secondPosY + midHeight,
		secondPos, secondPosY, image);
	UI_DrawNormImage(false, pos[0] + size[0] - rightWidth, yMiddle, rightWidth, hMiddle, thirdPos + rightWidth, secondPosY + midHeight,
		thirdPos, secondPosY, image);

	/* draw bottom (from left to right) */
	const int yBottom = pos[1] + size[1] - bottomHeight;
	UI_DrawNormImage(false, pos[0], yBottom, leftWidth, bottomHeight, firstPos + leftWidth, thirdPosY + bottomHeight,
		firstPos, thirdPosY, image);
	UI_DrawNormImage(false, pos[0] + leftWidth, yBottom, size[0] - leftWidth - rightWidth, bottomHeight, secondPos + midWidth, thirdPosY + bottomHeight,
		secondPos, thirdPosY, image);
	UI_DrawNormImage(false, pos[0] + size[0] - bottomHeight, yBottom, rightWidth, bottomHeight, thirdPos + rightWidth, thirdPosY + bottomHeight,
		thirdPos, thirdPosY, image);
}

/**
 * @brief draw a line into a bounding box
 * @param[in] fontID the font id (defined in ufos/fonts.ufo)
 * @param[in] align Align of the text into the bounding box
 * @param[in] x,y Current position of the bounded box
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
int UI_DrawStringInBox (const char* fontID, align_t align, int x, int y, int width, int height, const char* text, longlines_t method)
{
	const align_t horizontalAlign = (align_t)(align % 3); /* left, center, right */
	const align_t verticalAlign = (align_t)(align / 3);  /* top, center, bottom */

	/* position of the text for UI_DrawString */
	const int xx = x + ((width * horizontalAlign) >> 1);
	const int yy = y + ((height * verticalAlign) >> 1);

	return UI_DrawString(fontID, align, xx, yy, xx, width, 0, text, 0, 0, nullptr, false, method);
}

int UI_DrawString (const char* fontID, align_t align, int x, int y, int absX, int maxWidth,
		int lineHeight, const char* c, int boxHeight, int scrollPos, int* curLine, bool increaseLine, longlines_t method)
{
	const uiFont_t* font = UI_GetFontByID(fontID);
	const align_t verticalAlign = (align_t)(align / 3);  /* top, center, bottom */
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

/**
 * @brief Enables flashing effect for UI nodes
 * @param[in] flashingColor Color to fade to and back
 * @param[in] speed Speed of flashing effect
 */
void UI_EnableFlashing (const vec4_t flashingColor, float speed)
{
	vec4_t color;
	Vector4Copy(flashingColor, color);
	color[3] = sin(M_PI * cls.realtime * speed / 500.0f) * 0.5f + 0.5f;
	R_TexOverride(color);
}

/**
 * @brief Disables flashing effect for UI nodes
 */
void UI_DisableFlashing (void)
{
	R_TexOverride(nullptr);
}
