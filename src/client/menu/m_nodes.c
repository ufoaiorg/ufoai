/**
 * @file m_nodes.c
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

#include "m_main.h"
#include "m_nodes.h"
#include "m_parse.h"
#include "m_input.h"
#include "m_dragndrop.h"
#include "../cl_input.h"
#include "../../game/q_shared.h"

#include "m_node_bar.h"
#include "m_node_base.h"
#include "m_node_button.h"
#include "m_node_checkbox.h"
#include "m_node_controls.h"
#include "m_node_cinematic.h"
#include "m_node_container.h"
#include "m_node_custombutton.h"
#include "m_node_image.h"
#include "m_node_item.h"
#include "m_node_linestrip.h"
#include "m_node_map.h"
#include "m_node_model.h"
#include "m_node_radar.h"
#include "m_node_selectbox.h"
#include "m_node_string.h"
#include "m_node_tab.h"
#include "m_node_tbar.h"
#include "m_node_text.h"
#include "m_node_windowpanel.h"

extern menuNode_t *focusNode;

/**
 * @brief Returns the absolute position of a menunode
 * @param[in] menunode
 * @param[out] pos
 */
void MN_GetNodeAbsPos (const menuNode_t* node, vec2_t pos)
{
	if (!node)
		Sys_Error("MN_GetNodeAbsPos: No node given");
	if (!node->menu)
		Sys_Error("MN_GetNodeAbsPos: Node '%s' has no menu", node->name);

	/* if we request the position of an undrawable node, there is a problem */
	if (nodeBehaviourList[node->type].isVirtual)
		Sys_Error("MN_GetNodeAbsPos: Node '%s' dont have position", node->name);

	Vector2Set(pos, node->menu->pos[0] + node->pos[0], node->menu->pos[1] + node->pos[1]);
}

/**
 * @brief Update an absolute position to a relative one
 * @param[in] menunode
 * @param[inout] x an absolute x position
 * @param[inout] y an absolute y position
 */
void MN_NodeAbsoluteToRelativePos (const menuNode_t* node, int *x, int *y)
{
	assert(node != NULL);
	assert(x != NULL);
	assert(y != NULL);

	/* if we request the position of an undrawable node, there is a problem */
	if (nodeBehaviourList[node->type].isVirtual)
		Sys_Error("MN_NodeAbsoluteToRelativePos: Node '%s' dont have position", node->name);

	*x -= node->menu->pos[0] + node->pos[0];
	*y -= node->menu->pos[1] + node->pos[1];
}

#if 0
/** @todo (menu) to be integrated into MN_CheckNodeZone */
/**
 * @brief Check if the node is an image and if it is transparent on the given (global) position.
 * @param[in] node A menunode pointer to be checked.
 * @param[in] x X position on screen.
 * @param[in] y Y position on screen.
 * @return qtrue if an image is used and it is on transparent on the current position.
 */
static qboolean MN_NodeWithVisibleImage (menuNode_t* const node, int x, int y)
{
	byte *picture = NULL;	/**< Pointer to image (4 bytes == 1 pixel) */
	int width, height;	/**< Width and height for the pic. */
	int pic_x, pic_y;	/**< Position inside image */
	byte *color = NULL;	/**< Pointer to specific pixel in image. */
	vec2_t nodepos;

	if (!node || node->type != MN_PIC || !node->dataImageOrModel)
		return qfalse;

	MN_GetNodeAbsPos(node, nodepos);
	R_LoadImage(va("pics/menu/%s", path), &picture, &width, &height);

	if (!picture || !width || !height) {
		Com_DPrintf(DEBUG_CLIENT, "Couldn't load image %s in pics/menu\n", path);
		/* We return true here because we do not know if there is another image (withouth transparency) or another reason it might still be valid. */
		return qtrue;
	}

	/** @todo (menu) Get current location _inside_ image from global position. CHECKME */
	pic_x = x - nodepos[0];
	pic_y = y - nodepos[1];

	if (pic_x < 0 || pic_y < 0)
		return qfalse;

	/* Get pixel at current location. */
	color = picture + (4 * height * pic_y + 4 * pic_x); /* 4 means 4 values for each point */

	/* Return qtrue if pixel is visible (we check the alpha value here). */
	if (color[3] != 0)
		return qtrue;

	/* Image is transparent at this position. */
	return qfalse;
}
#endif

/**
 * @sa MN_LeftClick
 */
qboolean MN_CheckNodeZone (menuNode_t* const node, int x, int y)
{
	int sx, sy, tx, ty, i;
	vec2_t nodepos;

	/* skip all unactive nodes */
	if (nodeBehaviourList[node->type].isVirtual)
		return qfalse;

	/* don't hover nodes if we are executing an action on geoscape like rotating or moving */
	if (mouseSpace >= MS_ROTATE && mouseSpace <= MS_SHIFT3DMAP)
		return qfalse;

	if (focusNode && mouseSpace != MS_NULL)
		return qfalse;

	if (MN_GetMouseCapture())
		return qfalse;

	MN_GetNodeAbsPos(node, nodepos);
	switch (node->type) {
	case MN_CONTAINER:
#if 0
		MN_FindContainer(node);
		if (!node->container)
			return qfalse;

		/* check bounding box */
		if (x < nodepos[0] || x > nodepos[0] + node->size[0] || y < nodepos[1] || y > nodepos[1] + node->size[1])
			return qfalse;

		/* found a container */
		return qtrue;
#endif
	/* the following nodes don't need action nodes */
	case MN_CHECKBOX:
	case MN_TAB:
	case MN_SELECTBOX:
	case MN_CONTROLS:
		break;
	default:
		/* check for click action */
		if (node->invis || (!node->click && !node->rclick && !node->mclick && !node->wheel && !node->mouseIn && !node->mouseOut && !node->wheelUp && !node->wheelDown)
		 || !MN_CheckCondition(node))
			return qfalse;
	}

	if (node->size[0] == 0 || node->size[1] == 0)
		return qfalse;

	sx = node->size[0];
	sy = node->size[1];
	tx = x - nodepos[0];
	ty = y - nodepos[1];

	if (node->align > 0 && node->align < ALIGN_LAST) {
		switch (node->align % 3) {
		/* center */
		case 1:
			tx = x - nodepos[0] + sx / 2;
			break;
		/* right */
		case 2:
			tx = x - nodepos[0] + sx;
			break;
		}
	}

	if (tx < 0 || ty < 0 || tx > sx || ty > sy)
		return qfalse;

	for (i = 0; i < node->excludeNum; i++) {
		if (x >= node->menu->pos[0] + node->exclude[i].pos[0] && x <= node->menu->pos[0] + node->exclude[i].pos[0] + node->exclude[i].size[0]
		 && y >= node->menu->pos[1] + node->exclude[i].pos[1] && y <= node->menu->pos[1] + node->exclude[i].pos[1] + node->exclude[i].size[1])
			return qfalse;
	}

	return qtrue;
}

/**
 * @brief Searches a given node in the current menu
 * @sa MN_GetNode
 */
menuNode_t* MN_GetNodeFromCurrentMenu (const char *name)
{
	return MN_GetNode(MN_GetActiveMenu(), name);
}

/**
 * @brief Sets new x and y coordinates for a given node
 */
void MN_SetNewNodePos (menuNode_t* node, int x, int y)
{
	if (node) {
		node->pos[0] = x;
		node->pos[1] = y;
	}
}

/**
 * @brief Hides a given menu node
 * @note Sanity check whether node is null included
 */
void MN_HideNode (menuNode_t* node)
{
	if (node)
		node->invis = qtrue;
	else
		Com_Printf("MN_HideNode: No node given\n");
}

/**
 * @brief Script command to hide a given menu node
 */
static void MN_HideNode_f (void)
{
	if (Cmd_Argc() == 2)
		MN_HideNode(MN_GetNodeFromCurrentMenu(Cmd_Argv(1)));
	else
		Com_Printf("Usage: %s <node>\n", Cmd_Argv(0));
}

/**
 * @todo Change the @c type to @c char* (behaviourName)
 */
menuNode_t* MN_AllocNode (int type)
{
	menuNode_t* node = &mn.menuNodes[mn.numNodes++];
	if (mn.numNodes >= MAX_MENUNODES)
		Sys_Error("MAX_MENUNODES hit");
	node->type = type;
	return node;
}

/**
 * @brief Return a node behaviour by name
 * @todo when its possible use a dicotomic search
 */
nodeBehaviour_t* MN_GetNodeBehaviour (const char* name)
{
	int i;
	for (i = 0; i < MN_NUM_NODETYPE; i++) {
		if (!Q_strcmp(name, nodeBehaviourList[i].name)) {
			return &nodeBehaviourList[i];
		}
	}
	Sys_Error("Node behaviour '%s' dont exists\n", name);
}

/**
 * @brief Unhides a given menu node
 * @note Sanity check whether node is null included
 */
void MN_UnHideNode (menuNode_t* node)
{
	if (node)
		node->invis = qfalse;
	else
		Com_Printf("MN_UnHideNode: No node given\n");
}

/**
 * @brief Script command to unhide a given menu node
 */
static void MN_UnHideNode_f (void)
{
	if (Cmd_Argc() == 2)
		MN_UnHideNode(MN_GetNodeFromCurrentMenu(Cmd_Argv(1)));
	else
		Com_Printf("Usage: %s <node>\n", Cmd_Argv(0));
}

/**
 * @brief Init a behaviour to null. A null node doesn't react
 */
static inline void MN_RegisterNullNode (nodeBehaviour_t* behaviour, const char* name, qboolean isVirtual)
{
	memset(behaviour, 0, sizeof(behaviour));
	behaviour->name = name;
	behaviour->isVirtual = isVirtual;
}

/**
 * @brief List of all node behaviours, indexes by nodetype num.
 */
nodeBehaviour_t nodeBehaviourList[MN_NUM_NODETYPE];

/**
 * @brief menu behaviour, not realy a node for the moment
 */
nodeBehaviour_t menuBehaviour;

void MN_InitNodes (void)
{
	Cmd_AddCommand("mn_hidenode", MN_HideNode_f, "Hides a given menu node");
	Cmd_AddCommand("mn_unhidenode", MN_UnHideNode_f, "Unhides a given menu node");

	/* menu node */
	MN_RegisterWindowNode(&menuBehaviour);

	/* all nodes */
	MN_RegisterNullNode(nodeBehaviourList + MN_NULL, "", qtrue);
	MN_RegisterNullNode(nodeBehaviourList + MN_CONFUNC, "confunc", qtrue);
	MN_RegisterNullNode(nodeBehaviourList + MN_CVARFUNC, "cvarfunc", qtrue);
	MN_RegisterNullNode(nodeBehaviourList + MN_FUNC, "func", qtrue);
	MN_RegisterNullNode(nodeBehaviourList + MN_ZONE, "zone", qfalse);
	MN_RegisterImageNode(nodeBehaviourList + MN_PIC);
	MN_RegisterStringNode(nodeBehaviourList + MN_STRING);
	MN_RegisterTextNode(nodeBehaviourList + MN_TEXT);
	MN_RegisterBarNode(nodeBehaviourList + MN_BAR);
	MN_RegisterTBarNode(nodeBehaviourList + MN_TBAR);
	MN_RegisterModelNode(nodeBehaviourList + MN_MODEL);
	MN_RegisterContainerNode(nodeBehaviourList + MN_CONTAINER);
	MN_RegisterItemNode(nodeBehaviourList + MN_ITEM);
	MN_RegisterMapNode(nodeBehaviourList + MN_MAP);
	MN_RegisterBaseMapNode(nodeBehaviourList + MN_BASEMAP);
	MN_RegisterBaseLayoutNode(nodeBehaviourList + MN_BASELAYOUT);
	MN_RegisterCheckBoxNode(nodeBehaviourList + MN_CHECKBOX);
	MN_RegisterSelectBoxNode(nodeBehaviourList + MN_SELECTBOX);
	MN_RegisterLineStripNode(nodeBehaviourList + MN_LINESTRIP);
	MN_RegisterCinematicNode(nodeBehaviourList + MN_CINEMATIC);
	MN_RegisterRadarNode(nodeBehaviourList + MN_RADAR);
	MN_RegisterTabNode(nodeBehaviourList + MN_TAB);
	MN_RegisterControlsNode(nodeBehaviourList + MN_CONTROLS);
	MN_RegisterCustomButtonNode(nodeBehaviourList + MN_CUSTOMBUTTON);
	MN_RegisterWindowPanelNode(nodeBehaviourList + MN_WINDOWPANEL);
	MN_RegisterButtonNode(nodeBehaviourList + MN_BUTTON);
}
