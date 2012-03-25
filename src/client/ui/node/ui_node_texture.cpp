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
#include "../ui_behaviour.h"
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

	const char* imageName = UI_GetReferenceString(node, node->image);
	if (Q_strnull(imageName))
		return;

	image = UI_LoadWrappedImage(imageName);
	if (!image)
		return;

	/* avoid potential infinit loop */
	if (image->height == 0 || image->width == 0)
		return;

	UI_GetNodeAbsPos(node, nodepos);

	UI_DrawNormImage(qfalse, nodepos[0], nodepos[1], node->size[0], node->size[1], node->size[0], node->size[1], 0, 0, image);
}

void UI_RegisterTextureNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "texture";
	behaviour->draw = UI_TextureNodeDraw;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Source of the texture */
	UI_RegisterNodeProperty(behaviour, "src", V_CVAR_OR_STRING, uiNode_t, image);
}
