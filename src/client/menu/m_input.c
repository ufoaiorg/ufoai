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
#include "m_main.h"
#include "m_internal.h"
#include "m_actions.h"
#include "m_input.h"
#include "m_nodes.h"
#include "m_draw.h"
#include "m_dragndrop.h"

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
	if (hoveredNode && (hoveredNode->invis || !MN_CheckVisibility(hoveredNode)))
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
 * @brief Return the first visible node at a position
 * @param[in] node Node where we must search
 * @param[in] rx Relative x position to the parent of the node
 * @param[in] ry Relative y position to the parent of the node
 * @return The first visible node at position, else NULL
 */
static menuNode_t *MN_GetNodeInTreeAtPosition(menuNode_t *node, int rx, int ry)
{
	menuNode_t *find;
	menuNode_t *child;
	int i;

	if (node->invis || node->behaviour->isVirtual || !MN_CheckVisibility(node))
		return NULL;

	/* relative to the node */
	rx -= node->pos[0];
	ry -= node->pos[1];

	/* check bounding box */
	if (rx < 0 || ry < 0 || rx >= node->size[0] || ry >= node->size[1])
		return NULL;

	/** @todo we should improve the loop to search the right in first */
	find = NULL;
	for (child = node->firstChild; child; child = child->next) {
		menuNode_t *tmp;
		tmp = MN_GetNodeInTreeAtPosition(child, rx, ry);
		if (tmp)
			find = tmp;
	}
	if (find)
		return find;

	/* is the node tangible */
	if (node->ghost)
		return NULL;

	/* check excluded box */
	for (i = 0; i < node->excludeRectNum; i++) {
		if (rx >= node->excludeRect[i].pos[0]
		 && rx < node->excludeRect[i].pos[0] + node->excludeRect[i].size[0]
		 && ry >= node->excludeRect[i].pos[1]
		 && ry < node->excludeRect[i].pos[1] + node->excludeRect[i].size[1])
			return NULL;
	}

	/* we are over the node */
	return node;
}

/**
 * @brief Return the first visible node at a position
 */
menuNode_t *MN_GetNodeAtPosition (int x, int y)
{
	int menuId;

	/* find the first menu under the mouse */
	for (menuId = mn.menuStackPos - 1; menuId >= 0; menuId--) {
		menuNode_t *menu = mn.menuStack[menuId];
		menuNode_t *find;

		find = MN_GetNodeInTreeAtPosition(menu, x, y);
		if (find)
			return find;

		/* we must not search anymore */
		if (menu->u.window.dropdown)
			break;
		if (menu->u.window.modal)
			break;
	}

	return NULL;
}

/**
 * @brief Is called everytime the mouse position change
 */
void MN_MouseMove (int x, int y)
{
	if (MN_DNDIsDragging())
		return;

	/* send the captured move mouse event */
	if (capturedNode) {
		if (capturedNode->behaviour->capturedMouseMove)
			capturedNode->behaviour->capturedMouseMove(capturedNode, x, y);
		return;
	}

	MN_FocusRemove();

	hoveredNode = MN_GetNodeAtPosition(x, y);

	/* update nodes: send 'in' and 'out' event */
	if (oldHoveredNode != hoveredNode) {
		if (oldHoveredNode) {
			MN_ExecuteEventActions(oldHoveredNode, oldHoveredNode->onMouseOut);
			oldHoveredNode->state = qfalse;
		}
		if (hoveredNode) {
			hoveredNode->state = qtrue;
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onMouseIn);
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
 * @sa MN_ExecuteEventActions
 * @sa MN_RightClick
 * @sa Key_Message
 * @sa CL_MessageMenu_f
 * @note inline editing of cvars (e.g. save dialog) is done in Key_Message
 */
void MN_LeftClick (int x, int y)
{
	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->leftClick)
			capturedNode->behaviour->leftClick(capturedNode, x, y);
		return;
	}

	/* if we click out side a dropdown menu, we close it */
	/** @todo need to refactoring it with a the focus code (cleaner) */
	if (!hoveredNode && mn.menuStackPos != 0) {
		if (mn.menuStack[mn.menuStackPos - 1]->u.window.dropdown) {
			MN_PopMenu(qfalse);
		}
	}

	if (hoveredNode && !hoveredNode->disabled) {
		if (hoveredNode->behaviour->leftClick) {
			hoveredNode->behaviour->leftClick(hoveredNode, x, y);
		} else {
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onClick);
		}
	}
}

/**
 * @sa MAP_ResetAction
 * @sa MN_ExecuteEventActions
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

	if (hoveredNode && !hoveredNode->disabled) {
		if (hoveredNode->behaviour->rightClick) {
			hoveredNode->behaviour->rightClick(hoveredNode, x, y);
		} else {
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onRightClick);
		}
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

	if (hoveredNode && !hoveredNode->disabled) {
		if (hoveredNode->behaviour->middleClick) {
			hoveredNode->behaviour->middleClick(hoveredNode, x, y);
		} else {
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onMiddleClick);
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
				MN_ExecuteEventActions(hoveredNode, (down ? hoveredNode->onWheelDown : hoveredNode->onWheelUp));
			else
				MN_ExecuteEventActions(hoveredNode, hoveredNode->onWheel);
		}
	}
}

/**
 * @brief Called when we are in menu mode and down a mouse button
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
