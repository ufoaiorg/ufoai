/**
 * @file m_drawutil.h
 * @brief Some functions to help the draw of nodes
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#ifndef CLIENT_MENU_M_DRAWUTIL_H
#define CLIENT_MENU_M_DRAWUTIL_H

/**
 * @brief draw a panel from a texture as we can see on the texture
 * The function is inline because there are often 3 or 5 const param, with it a lot of var became const too
 * @texture html http://ufoai.ninex.info/wiki/textures/Inline_draw_panel.png
 * @param[in] pos Position of the output panel
 * @param[in] size Size of the output panel
 * @param[in] texture Texture contain the template of the panel
 * @param[in] blend True if the texture must use alpha chanel for per pixel transparency
 * @param[in] texX Position x of the panel template into the texture
 * @param[in] texY Position y of the panel template into the texture
 * @param[in] corner Size of the corner part of the template
 * @param[in] middle Size of the middle part of the template
 * @param[in] marge Size of the marge between corners and middle part of the template
 */
static inline void MN_DrawPanel (const vec2_t pos, const vec2_t size, const char *texture, qboolean blend, int texX, int texY, int corner, int middle, int marge)
{
	const int firstPos = 0;
	int secondPos = firstPos + corner + marge;
	int thirdPos = secondPos + middle + marge;
	int y, h;

	/* draw top (from left to right) */
	R_DrawNormPic(pos[0], pos[1], corner, corner, texX + firstPos + corner, texY + firstPos + corner,
		texX + firstPos, texY + firstPos, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + corner, pos[1], size[0] - corner - corner, corner, texX + secondPos + middle, texY + firstPos + corner,
		texX + secondPos, texY + firstPos, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + size[0] - corner, pos[1], corner, corner, texX + thirdPos + corner, texY + firstPos + corner,
		texX + thirdPos, texY + firstPos, ALIGN_UL, blend, texture);

	/* draw middle (from left to right) */
	y = pos[1] + corner;
	h = size[1] - corner - corner; /*< height of middle */
	R_DrawNormPic(pos[0], y, corner, h, texX + firstPos + corner, texY + secondPos + middle,
		texX + firstPos, texY + secondPos, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + corner, y, size[0] - corner - corner, h, texX + secondPos + middle, texY + secondPos + middle,
		texX + secondPos, texY + secondPos, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + size[0] - corner, y, corner, h, texX + thirdPos + corner, texY + secondPos + middle,
		texX + thirdPos, texY + secondPos, ALIGN_UL, blend, texture);

	/* draw bottom (from left to right) */
	y = pos[1] + size[1] - corner;
	R_DrawNormPic(pos[0], y, corner, corner, texX + firstPos + corner, texY + thirdPos + corner,
		texX + firstPos, texY + thirdPos, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + corner, y, size[0] - corner - corner, corner, texX + secondPos + middle, texY + thirdPos + corner,
		texX + secondPos, texY + thirdPos, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + size[0] - corner, y, corner, corner, texX + thirdPos + corner, texY + thirdPos + corner,
		texX + thirdPos, texY + thirdPos, ALIGN_UL, blend, texture);
}

#endif
