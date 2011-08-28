/**
 * @file ui_node_texture.c
 * @brief The <code>texture</code> behaviour allow to draw a motif (an image) in all the surface of the a node.
 * The image is not stretched but looped.
 * @code
 * texture background
 * {
 *	image	ui/wood
 *	[..]
 * }
 * @endcode
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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_render.h"
#include "../../renderer/r_draw.h"
#include "ui_node_texture.h"
#include "ui_node_abstractnode.h"

#define EXTRADATA_TYPE 0

/**
 * @brief Draws the texture node
 * @param[in] node The UI node to draw
 */
static void UI_TextureNodeDraw (uiNode_t *node)
{
	vec2_t nodepos;
	const image_t *image;
	int x, y;

	const char* imageName = UI_GetReferenceString(node, node->image);
	if (Q_strnull(imageName))
		return;

	image = UI_LoadImage(imageName);
	if (!image)
		return;

	/* avoid potential infinit loop */
	if (image->height == 0 || image->width == 0)
		return;

	UI_GetNodeAbsPos(node, nodepos);
	R_PushClipRect(nodepos[0], nodepos[1], node->size[0], node->size[1]);

	/** @todo use opengl feature instead of loop to display a texture */
	for (y = nodepos[1]; y < nodepos[1] + node->size[1]; y += image->height) {
		for (x = nodepos[0]; x < nodepos[0] + node->size[0]; x += image->width) {
			UI_DrawNormImage(x, y, image->width, image->height,
					image->width, image->height, 0, 0, image);
		}
	}

	R_PopClipRect();
}

static const value_t properties[] = {
	/* Source of the texture */
	{"src", V_CVAR_OR_STRING, offsetof(uiNode_t, image), 0},

	{NULL, V_NULL, 0, 0}
};

void UI_RegisterTextureNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "texture";
	behaviour->draw = UI_TextureNodeDraw;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
