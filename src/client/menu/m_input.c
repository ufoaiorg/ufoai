/**
 * @file m_input.c
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

#include "../client.h"
#include "../cl_map.h"
#include "m_input.h"
#include "m_main.h"
#include "m_actions.h"
#include "m_parse.h"
#include "m_input.h"
#include "node/m_node_text.h"
#include "node/m_node_tab.h"
#include "m_nodes.h"
#include "m_draw.h"
#include "../cl_global.h"

/**
 * @sa MN_DisplayNotice
 * @todo move it into a better file
 */
static void MN_CheckCvar (const cvar_t *cvar)
{
	if (cvar->modified) {
		if (cvar->flags & CVAR_CONTEXT) {
			MN_DisplayNotice(_("This change requires a restart"), 2000);
		} else if (cvar->flags & CVAR_IMAGES) {
			MN_DisplayNotice(_("This change might require a restart"), 2000);
		}
	}
}

/**
 * @param[in] str Might be NULL if you want to set a float value
 * @todo move it into a better file
 */
void MN_SetCvar (const char *name, const char *str, float value)
{
	const cvar_t *cvar;
	cvar = Cvar_FindVar(name);
	if (!cvar) {
		Com_Printf("Could not find cvar '%s'\n", name);
		return;
	}
	/* strip '*cvar ' from data[0] - length is already checked above */
	if (str)
		Cvar_Set(cvar->name, str);
	else
		Cvar_SetValue(cvar->name, value);
	MN_CheckCvar(cvar);
}

/**
 * @brief save the captured node
 * @sa MN_SetMouseCapture
 * @sa MN_GetMouseCapture
 * @sa MN_MouseRelease
 * @todo think about replacing it by a boolean. When capturedNode != NULL => hoveredNode == capturedNode
 * it create unneed case
 */
static menuNode_t* capturedNode = NULL;

/**
 * @brief Return the captured node
 * @return Return a node, else NULL
 */
menuNode_t* MN_GetMouseCapture (void)
{
	return capturedNode;
}

/**
 * @brief Captured the mouse into a node
 */
void MN_SetMouseCapture (menuNode_t* node)
{
	assert(capturedNode == NULL);
	assert(node != NULL);
	capturedNode = node;
}

/**
 * @brief Release the captured node
 */
void MN_MouseRelease (void)
{
	capturedNode = NULL;
	MN_FocusRemove();
	MN_InvalidateMouse();
}

/**
 * @brief save the current hovered node (first node under the mouse)
 * @sa MN_GetHoveredNode
 * @sa MN_MouseMove
 * @sa MN_CheckMouseMove
 */
static menuNode_t* hoveredNode = NULL;

/**
 * @brief save the previous hovered node
 */
static menuNode_t *oldHoveredNode;

/**
 * @brief save old position of the mouse
 */
static int oldMousePosX = 0, oldMousePosY = 0;

/**
 * @brief Get the current hovered node
 * @return A node, else NULL if the mouse hover nothing
 */
menuNode_t *MN_GetHoveredNode (void)
{
	return hoveredNode;
}

/**
 * @brief Force to invalidate the mouse position and then to update hover nodes...
 */
void MN_InvalidateMouse (void)
{
	oldMousePosX = -1;
	oldMousePosY = -1;
}

/**
 * @brief Call mouse move only if the mouse position change
 */
qboolean MN_CheckMouseMove (void)
{
	/* is hovered node no more draw */
	if (hoveredNode && (hoveredNode->invis || !MN_CheckCondition(hoveredNode)))
		MN_InvalidateMouse();

	if (mousePosX != oldMousePosX || mousePosY != oldMousePosY) {
		oldMousePosX = mousePosX;
		oldMousePosY = mousePosY;
		MN_MouseMove(mousePosX, mousePosY);
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Check if a position is inner a node
 * @param[in] node The node to test
 * @param[in] x absolute x position on the screen
 * @param[in] y absolute y position on the screen
 * @todo Move it on a better file.c
 */
static qboolean MN_IsInnerNode (const menuNode_t* const node, int x, int y)
{
	int i;
	/* relative position */
	x -= node->menu->pos[0];
	y -= node->menu->pos[1];

	/* check bounding box */
	if (x < node->pos[0] || x > node->pos[0] + node->size[0]
	 || y < node->pos[1] || y > node->pos[1] + node->size[1]) {
		return qfalse;
	}

	/* check excluded box */
	for (i = 0; i < node->excludeRectNum; i++) {
		if (x >= node->excludeRect[i].pos[0]
		 && x <= node->excludeRect[i].pos[0] + node->excludeRect[i].size[0]
		 && y >= node->excludeRect[i].pos[1]
		 && y <= node->excludeRect[i].pos[1] + node->excludeRect[i].size[1])
			return qfalse;
	}
	return qtrue;
}

/**
 * @brief Return the first visible node at a possition
 */
menuNode_t *MN_GetNodeByPosition (int x, int y)
{
	menu_t *menu;
	int sp;
	menuNode_t *node;
	menuNode_t *find;

	/* find the first menu under the mouse */
	menu = NULL;
	for (sp = mn.menuStackPos-1; sp >= 0; sp--) {
		menu_t *m = mn.menuStack[sp];
		/* check mouse vs menu boundedbox */
		if (x >= m->pos[0] && x <= m->pos[0] + m->size[0]
		 && y >= m->pos[1] && y <= m->pos[1] + m->size[1]) {
			menu = m;
			break;
		}

		if (m->modal) {
			/* we must not search anymore */
			break;
		}
	}

	/* find the first node under the mouse (last of the node list) */
	find = NULL;
	if (menu) {
		/* check mouse vs node boundedbox */
		for (node = menu->firstChild; node; node = node->next) {
			if (node->invis || node->behaviour->isVirtual || !MN_CheckCondition(node))
				continue;
			if (MN_IsInnerNode(node, x, y)) {
				find = node;
			}
		}
	}

	return find;
}

/**
 * @brief Is called everytime the mouse position change
 */
void MN_MouseMove (int x, int y)
{
	/** @todo very bad code */
	if (mouseSpace == MS_DRAGITEM)
		return;

	/* send the captured move mouse event */
	if (capturedNode) {
		if (capturedNode->behaviour->capturedMouseMove)
			capturedNode->behaviour->capturedMouseMove(capturedNode, x, y);
		return;
	}

	MN_FocusRemove();

	hoveredNode = MN_GetNodeByPosition(x, y);

	/* update nodes: send 'in' and 'out' event */
	if (oldHoveredNode != hoveredNode) {
		if (oldHoveredNode) {
			MN_ExecuteActions(oldHoveredNode->menu, oldHoveredNode->onMouseOut);
			oldHoveredNode->state = qfalse;
		}
		if (hoveredNode) {
			hoveredNode->state = qtrue;
			MN_ExecuteActions(hoveredNode->menu, hoveredNode->onMouseIn);
		}
	}
	oldHoveredNode = hoveredNode;

	/* send the move event */
	if (hoveredNode && hoveredNode->behaviour->mouseMove) {
		hoveredNode->behaviour->mouseMove(hoveredNode, x, y);
	}
}

/**
 * @brief Is called everytime one clickes on a menu/screen. Then checks if anything needs to be executed in the earea of the click (e.g. button-commands, inventory-handling, geoscape-stuff, etc...)
 * @sa MN_ModelClick
 * @sa MN_TextRightClick
 * @sa MN_TextClick
 * @sa MN_Drag
 * @sa MN_BarClick
 * @sa MN_CheckboxClick
 * @sa MN_BaseMapClick
 * @sa MAP_3DMapClick
 * @sa MAP_MapClick
 * @sa MN_ExecuteActions
 * @sa MN_RightClick
 * @sa Key_Message
 * @sa CL_MessageMenu_f
 * @note inline editing of cvars (e.g. save dialog) is done in Key_Message
 */
void MN_LeftClick (int x, int y)
{
	menuNode_t *node;

	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->leftClick)
			capturedNode->behaviour->leftClick(capturedNode, x, y);
		return;
	}

	if (hoveredNode) {
#if 0 /* this code do nothing? */
		qboolean mouseOver;
#endif
		node = hoveredNode;
#if 0 /* this code do nothing? */
		if (mouseSpace == MS_DRAGITEM && node->behaviour->id == MN_CONTAINER && dragInfo.item.t) {
			int itemX = 0;
			int itemY = 0;

			/** We calculate the position of the top-left corner of the dragged
			 * item in oder to compensate for the centered-drawn cursor-item.
			 * Or to be more exact, we calculate the relative offset from the cursor
			 * location to the middle of the top-left square of the item.
			 * @sa MN_DrawMenus:MN_CONTAINER */
			if (dragInfo.item.t) {
				itemX = C_UNIT * dragInfo.item.t->sx / 2;	/* Half item-width. */
				itemY = C_UNIT * dragInfo.item.t->sy / 2;	/* Half item-height. */

				/* Place relative center in the middle of the square. */
				itemX -= C_UNIT / 2;
				itemY -= C_UNIT / 2;
			}
			mouseOver = MN_CheckNodeZone(node, x, y) || MN_CheckNodeZone(node, x - itemX, y - itemY);
		}
#endif

		if (node->behaviour->leftClick) {
			node->behaviour->leftClick(node, x, y);
		} else {
			MN_ExecuteEventActions(node, node->onClick);
		}
		return;
	}
}

/**
 * @sa MAP_ResetAction
 * @sa MN_TextRightClick
 * @sa MN_ExecuteActions
 * @sa MN_LeftClick
 * @sa MN_MiddleClick
 * @sa MN_MouseWheel
 */
void MN_RightClick (int x, int y)
{
	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->rightClick)
			capturedNode->behaviour->rightClick(capturedNode, x, y);
		return;
	}

	if (hoveredNode) {
		if (hoveredNode->behaviour->rightClick) {
			hoveredNode->behaviour->rightClick(hoveredNode, x, y);
		} else {
			MN_ExecuteActions(hoveredNode->menu, hoveredNode->onRightClick);
		}
		return;
	}
}

/**
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MouseWheel
 */
void MN_MiddleClick (int x, int y)
{
	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->middleClick)
			capturedNode->behaviour->middleClick(capturedNode, x, y);
		return;
	}

	if (hoveredNode) {
		if (hoveredNode->behaviour->middleClick) {
			hoveredNode->behaviour->middleClick(hoveredNode, x, y);
		} else {
			MN_ExecuteActions(hoveredNode->menu, hoveredNode->onMiddleClick);
		}
		return;
	}
}

/**
 * @brief Called when we are in menu mode and scroll via mousewheel
 * @note The geoscape zooming code is here, too (also in @see CL_ParseInput)
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseWheel (qboolean down, int x, int y)
{
	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->mouseWheel)
			capturedNode->behaviour->mouseWheel(capturedNode, down, x, y);
		return;
	}

	if (hoveredNode) {
		if (hoveredNode->behaviour->mouseWheel) {
			hoveredNode->behaviour->mouseWheel(hoveredNode, down, x, y);
		} else {
			if (hoveredNode->onWheelUp && hoveredNode->onWheelDown)
				MN_ExecuteActions(hoveredNode->menu, (down ? hoveredNode->onWheelDown : hoveredNode->onWheelUp));
			else
				MN_ExecuteActions(hoveredNode->menu, hoveredNode->onWheel);
		}
		return;
	}
}

/**
 * @brief Called when we are in menu mode and down a mouse button
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseDown (int x, int y, int button)
{
	menuNode_t *node;

	/* captured or hover node */
	node = capturedNode ? capturedNode : hoveredNode;

	if (node == NULL)
		return;

	if (node->behaviour->mouseDown)
		node->behaviour->mouseDown(node, x, y, button);
}

/**
 * @brief Called when we are in menu mode and up a mouse button
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseUp (int x, int y, int button)
{
	menuNode_t *node;

	/* captured or hover node */
	node = capturedNode ? capturedNode : hoveredNode;

	if (node == NULL)
		return;

	if (node->behaviour->mouseUp)
		node->behaviour->mouseUp(node, x, y, button);
}
