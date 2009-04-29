/**
 * @file m_node_material_editor.c
 * @brief Material editor related code
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

#include "../../client.h"
#include "../m_menus.h"
#include "../m_nodes.h"
#include "../m_actions.h"
#include "../m_render.h"
#include "../m_parse.h"
#include "m_node_abstractnode.h"
#include "m_node_abstractscrollable.h"
#include "../../cl_video.h"
#include "../../renderer/r_image.h"
#include "../../renderer/r_draw.h"
#include "../../renderer/r_model.h"
#include "m_node_material_editor.h"

/*#define ANYIMAGES*/
/** @todo Replace magic number 64 by some script definition */
#define IMAGE_WIDTH 64

/**
 * @brief return the number of images we can display
 */
static int MN_MaterialEditorNodeGetImageCount (menuNode_t *node)
{
	int i;
	int cnt = 0;

	for (i = 0; i < r_numImages; i++) {
#ifndef ANYIMAGES
		const image_t *image = &r_images[i];
		/* filter */
		if (image->type != it_world)
			continue;

		if (strstr(image->name, "tex_common"))
			continue;
#endif
		cnt++;
	}
	return cnt;
}

/**
 * @brief Update the scrollable view
 */
static void MN_MaterialEditorNodeUpdateView (menuNode_t *node)
{
	const int imageCount = MN_MaterialEditorNodeGetImageCount(node);
	const int imagesPerLine = (node->size[0] - node->padding) / (IMAGE_WIDTH + node->padding);
	const int imagesPerColumn = (node->size[1] - node->padding) / (IMAGE_WIDTH + node->padding);

	/* update view */
	if (imagesPerLine > 0 && imagesPerColumn > 0) {
		node->u.abstractscrollable.viewSizeX = imagesPerColumn;
		node->u.abstractscrollable.fullSizeX = imageCount / imagesPerLine;
	} else {
		node->u.abstractscrollable.viewSizeX = 0;
		node->u.abstractscrollable.viewPosX = 0;
		node->u.abstractscrollable.fullSizeX = 0;
	}

	/* clap position */
	if (node->u.abstractscrollable.viewPosX < 0)
		node->u.abstractscrollable.viewPosX = 0;
	if (node->u.abstractscrollable.viewPosX > node->u.abstractscrollable.fullSizeX - node->u.abstractscrollable.viewSizeX)
		node->u.abstractscrollable.viewPosX = node->u.abstractscrollable.fullSizeX - node->u.abstractscrollable.viewSizeX;

	/* callback the event */
	if (node->u.abstractscrollable.onViewChange)
		MN_ExecuteEventActions(node, node->u.abstractscrollable.onViewChange);
}

/**
 * @param node The node to draw
 */
static void MN_MaterialEditorNodeDraw (menuNode_t *node)
{
	int i;
	vec2_t pos;
	int cnt = 0;
	int cntView = 0;
	const int imagesPerLine = (node->size[0] - node->padding) / (IMAGE_WIDTH + node->padding);

	if (MN_AbstractScrollableNodeIsSizeChange(node))
		MN_MaterialEditorNodeUpdateView(node);

	/* width too small to display anything */
	if (imagesPerLine <= 0)
		return;

	MN_GetNodeAbsPos(node, pos);

	/* display images */
	for (i = 0; i < r_numImages; i++) {
		image_t *image = &r_images[i];
		vec2_t imagepos;

#ifndef ANYIMAGES
		/* filter */
		if (image->type != it_world)
			continue;

		if (strstr(image->name, "tex_common"))
			continue;
#endif

		/* skip images before the scroll position */
		if (cnt / imagesPerLine < node->u.abstractscrollable.viewPosX) {
			cnt++;
			continue;
		}

		/** @todo do it incremental. Don't need all this math */
		imagepos[0] = pos[0] + node->padding + (cntView % imagesPerLine) * (IMAGE_WIDTH + node->padding);
		imagepos[1] = pos[1] + node->padding + (cntView / imagesPerLine) * (IMAGE_WIDTH + node->padding);

		/* vertical overflow */
		if (imagepos[1] + IMAGE_WIDTH + node->padding >= pos[1] + node->size[1])
			break;

		if (i == node->num) {
#define MARGIN 3
			R_DrawRect(imagepos[0] - MARGIN, imagepos[1] - MARGIN, IMAGE_WIDTH + MARGIN * 2, IMAGE_WIDTH + MARGIN * 2, node->selectedColor, 2, 0xFFFF);
#undef MARGIN
		}

		MN_DrawNormImage(imagepos[0], imagepos[1], IMAGE_WIDTH, IMAGE_WIDTH, 0, 0, 0, 0, ALIGN_UL, image);

		cnt++;
		cntView++;
	}
}

/**
 * @brief Return index of the image (r_images[i]) else NULL
 */
static int MN_MaterialEditorNodeGetImageAtPosition (menuNode_t *node, int x, int y)
{
	int i;
	vec2_t pos;
	int cnt = 0;
	int cntView = 0;
	const int imagesPerLine = (node->size[0] - node->padding) / (IMAGE_WIDTH + node->padding);
	const int imagesPerColumn = (node->size[1] - node->padding) / (IMAGE_WIDTH + node->padding);
	int columnRequested;
	int lineRequested;

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	/* have we click between 2 images? */
	if (((x % (IMAGE_WIDTH + node->padding)) < node->padding)
		|| ((y % (IMAGE_WIDTH + node->padding)) < node->padding))
		return -1;

	/* get column and line of the image */
	columnRequested = x / (IMAGE_WIDTH + node->padding);
	lineRequested = y / (IMAGE_WIDTH + node->padding);

	/* have we click outside? */
	if (columnRequested >= imagesPerLine || lineRequested >= imagesPerColumn)
		return -1;

	MN_GetNodeAbsPos(node, pos);

	/* check images */
	for (i = 0; i < r_numImages; i++) {
		image_t *image = &r_images[i];

#ifndef ANYIMAGES
		/* filter */
		if (image->type != it_world)
			continue;

		if (strstr(image->name, "tex_common"))
			continue;
#endif

		/* skip images before the scroll position */
		if (cnt / imagesPerLine < node->u.abstractscrollable.viewPosX) {
			cnt++;
			continue;
		}

		if (cntView % imagesPerLine == columnRequested && cntView / imagesPerLine == lineRequested)
			return i;

		/* vertical overflow */
		if (cntView / imagesPerLine > lineRequested)
			break;

		cnt++;
		cntView++;
	}

	return -1;
}

static void MN_MaterialEditorMouseDown (menuNode_t *node, int x, int y, int button)
{
	int id;
	if (button != K_MOUSE1)
		return;

	id = MN_MaterialEditorNodeGetImageAtPosition(node, x, y);
	if (id == -1)
		return;

	/** @note here we use "num" to cache the selected image id. We can reuse it on the script with "<num>" */
	/* have we selected a new image? */
	if (node->num != id) {
		const image_t *image = &r_images[id];
		node->num = id;
		if (image->normalmap == NULL)
			MN_ExecuteConfunc("hideshaders true 0 0 0 0");
		else {
			MN_ExecuteConfunc("hideshaders false %f %f %f %f", image->material.bump,
					image->material.hardness, image->material.parallax, image->material.specular);
		}
		if (node->onChange)
			MN_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief Called when we push a window with this node
 */
static void MN_MaterialEditorNodeInit (menuNode_t *node)
{
	node->num = -1;
	node->u.abstractscrollable.viewPosX = 0;
	MN_MaterialEditorNodeUpdateView(node);
}

/**
 * @brief Called when the user wheel the mouse over the node
 */
static void MN_MaterialEditorNodeWheel (menuNode_t *node, qboolean down, int x, int y)
{
	const int diff = (down) ? 1 : -1;
	int pos;

	if (node->u.abstractscrollable.fullSizeX == 0 || node->u.abstractscrollable.fullSizeX < node->u.abstractscrollable.viewSizeX)
		return;

	/* clap new position */
	pos = node->u.abstractscrollable.viewPosX + diff;
	if (pos < 0)
		pos = 0;
	if (pos > node->u.abstractscrollable.fullSizeX - node->u.abstractscrollable.viewSizeX)
		pos = node->u.abstractscrollable.fullSizeX - node->u.abstractscrollable.viewSizeX;

	/* update position */
	if (pos != node->u.abstractscrollable.viewPosX) {
		node->u.abstractscrollable.viewPosX = pos;
		/* callback the event */
		if (node->u.abstractscrollable.onViewChange)
			MN_ExecuteEventActions(node, node->u.abstractscrollable.onViewChange);
	}
}

static void MN_MaterialEditorStart_f (void)
{
	/* material editor only makes sense in battlescape mode */
#ifndef ANYIMAGES
	if (cls.state != ca_active) {
		MN_PopMenu(qfalse);
		return;
	}
#endif
}

/** @brief valid properties for options (selectbox, tab...) */
static const value_t materialValues[] = {
	{"bump", V_FLOAT, offsetof(material_t, bump), 0},
	{"parallax", V_FLOAT, offsetof(material_t, parallax), 0},
	{"specular", V_FLOAT, offsetof(material_t, specular), 0},
	{"hardness", V_FLOAT, offsetof(material_t, hardness), 0},

	{NULL, V_NULL, 0, 0},
};

static void MN_MaterialEditorChangeValue_f (void)
{
	image_t *image;
	int id;
	const char *var, *value;
	const value_t *val;
	size_t bytes;

	if (Cmd_Argc() < 4) {
		Com_Printf("Usage: %s <image index> <variable> <value>\n", Cmd_Argv(0));
		return;
	}

	id = atoi(Cmd_Argv(1));
	if (id < 0 || id >= r_numImages) {
		Com_Printf("Given image index (%i) is out of bounds\n", id);
		return;
	}

	var = Cmd_Argv(2);
	value = Cmd_Argv(3);

	val = MN_FindPropertyByName(materialValues, var);
	if (!val) {
		Com_Printf("Could not find material variable for '%s'\n", var);
		return;
	}

	image = &r_images[id];
	/* new material should be added */
	if (image->material.flags == 0)
		image->material.flags = STAGE_RENDER;

	Com_ParseValue(&image->material, value, val->type, val->ofs, val->size, &bytes);

	R_ModReloadSurfacesArrays();
}

void MN_RegisterMaterialEditorNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "material_editor";
	behaviour->extends = "abstractscrollable";
	behaviour->draw = MN_MaterialEditorNodeDraw;
	behaviour->init = MN_MaterialEditorNodeInit;
	behaviour->mouseDown = MN_MaterialEditorMouseDown;
	behaviour->mouseWheel = MN_MaterialEditorNodeWheel;

	Cmd_AddCommand("mn_materialeditor_changevalue", MN_MaterialEditorChangeValue_f, "Initializes the material editor menu");
	Cmd_AddCommand("mn_materialeditor", MN_MaterialEditorStart_f, "Initializes the material editor menu");
}
