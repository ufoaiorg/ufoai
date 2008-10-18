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
#include "m_main.h"
#include "m_parse.h"
#include "m_input.h"
#include "m_inventory.h"
#include "m_node_text.h"
#include "m_node_tab.h"
#include "m_nodes.h"
#include "m_draw.h"
#include "../cl_global.h"

cvar_t *mn_inputlength;

/**
 * @sa MN_DisplayNotice
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
 * @sa IN_Parse
 */
qboolean MN_CursorOnMenu (int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next)
			if (MN_CheckNodeZone(node, x, y)) {
				/* found an element */
				/*MN_FocusSetNode(node);*/
				return qtrue;
			}

		if (menu->renderNode) {
			/* don't care about non-rendered windows */
			if (menu->renderNode->invis)
				return qtrue;
			else
				return qfalse;
		}
	}

	return qfalse;
}

/**
 * @brief Is called everytime the mouse position change
 */
void MN_MouseMove (int x, int y)
{

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
void MN_Click (int x, int y)
{
	menuNode_t *node;
	int sp, mouseOver;
	qboolean clickedInside = qfalse;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu_t *menu = mn.menuStack[--sp];
		menuNode_t *execute_node = NULL;
		for (node = menu->firstNode; node; node = node->next) {
			if (!nodeBehaviourList[node->type].leftClick && !node->click)
				continue;

			/* check whether mouse is over this node */
			if (mouseSpace == MS_DRAGITEM && node->type == MN_CONTAINER && dragInfo.item.t) {
				int itemX = 0;
				int itemY = 0;

				/** We calculate the position of the top-left corner of the dragged
				 * item in oder to compensate for the centered-drawn cursor-item.
				 * Or to be more exact, we calculate the relative offset from the cursor
				 * location to the middle of the top-left square of the item.
				 * @sa m_draw.c:MN_DrawMenus:MN_CONTAINER */
				if (dragInfo.item.t) {
					itemX = C_UNIT * dragInfo.item.t->sx / 2;	/* Half item-width. */
					itemY = C_UNIT * dragInfo.item.t->sy / 2;	/* Half item-height. */

					/* Place relative center in the middle of the square. */
					itemX -= C_UNIT / 2;
					itemY -= C_UNIT / 2;
				}
				mouseOver = MN_CheckNodeZone(node, x, y) || MN_CheckNodeZone(node, x - itemX, y - itemY);
			} else {
				mouseOver = MN_CheckNodeZone(node, x, y);
			}

			if (!mouseOver)
				continue;

			/* check whether we clicked at least on one menu node */
			clickedInside = qtrue;

			/* found a node -> do actions */
			if (nodeBehaviourList[node->type].leftClick) {
				nodeBehaviourList[node->type].leftClick(node, x, y);
			} else {
				/* Save the action for later execution. */
				if (node->click && (node->click->type != EA_NULL))
					execute_node = node;
			}
			if (node->type == MN_TEXT) {
				execute_node = node;
				mn.mouseRepeat.textLine = mouseOver;
			}
		}

		/* Only execute the last-found (i.e. from the top-most displayed node) action found.
		 * Especially needed for button-nodes that are (partially) overlayed and all have actions defined.
		 * e.g. the firemode checkboxes.
		 */
		if (execute_node) {
			mn.mouseRepeat.node = execute_node;
			mn.mouseRepeat.lastclicked = cls.realtime;
			if (execute_node->repeat) {
				mouseSpace = MS_LHOLD;
				/* Reset number of click */
				mn.mouseRepeat.numClick = 1;
				if (execute_node->clickDelay)
					mn.mouseRepeat.clickDelay = execute_node->clickDelay;
				else
					mn.mouseRepeat.clickDelay = 300;
				/** Delay between the 2 first actions is longer than the delay between other actions (see IN_Parse()) */
				mn.mouseRepeat.nexttime = cls.realtime + mn.mouseRepeat.clickDelay;
				mn.mouseRepeat.menu = menu;
				mn.mouseRepeat.action = execute_node->click;
			}
			if (execute_node->type != MN_TEXT)
				MN_ExecuteActions(menu, execute_node->click);
		}

		/** @todo maybe we should also check sp == mn.menuStackPos here */
		if (!clickedInside && menu->leaveNode)
			MN_ExecuteActions(menu, menu->leaveNode->click);

		/* don't care about non-rendered windows */
		if (menu->renderNode || menu->popupNode)
			return;
	}
}

/**
 * @sa MAP_ResetAction
 * @sa MN_TextRightClick
 * @sa MN_ExecuteActions
 * @sa MN_Click
 * @sa MN_MiddleClick
 * @sa MN_MouseWheel
 */
void MN_RightClick (int x, int y)
{
	int mouseOver;
	int sp = mn.menuStackPos;

	while (sp > 0) {
		menu_t *menu = mn.menuStack[--sp];
		menuNode_t *node;

		for (node = menu->firstNode; node; node = node->next) {
			/* no right click for this node defined */
			if (!nodeBehaviourList[node->type].rightClick && !node->rclick)
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */

			if (nodeBehaviourList[node->type].rightClick) {
				nodeBehaviourList[node->type].rightClick(node, x, y);
			} else {
				MN_ExecuteActions(menu, node->rclick);
			}
		}

		if (menu->renderNode || menu->popupNode)
			/* don't care about non-rendered windows */
			return;
	}
}

/**
 * @sa MN_Click
 * @sa MN_RightClick
 * @sa MN_MouseWheel
 */
void MN_MiddleClick (int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, mouseOver;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next) {
			/* no middle click for this node defined */
			if (!nodeBehaviourList[node->type].middleClick && !node->mclick)
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			if (nodeBehaviourList[node->type].middleClick) {
				nodeBehaviourList[node->type].middleClick(node, x, y);
			} else {
				MN_ExecuteActions(menu, node->mclick);
			}
		}

		if (menu->renderNode || menu->popupNode)
			/* don't care about non-rendered windows */
			return;
	}
}

/**
 * @brief Called when we are in menu mode and scroll via mousewheel
 * @note The geoscape zooming code is here, too (also in CL_ParseInput)
 * @sa MN_Click
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 * @sa MN_Click
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseWheel (qboolean down, int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, mouseOver;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next) {
			/* no middle click for this node defined */
			/* both wheelUp & wheelDown required */
			if (!nodeBehaviourList[node->type].mouseWheel && !node->wheel && !(node->wheelUp && node->wheelDown))
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			if (nodeBehaviourList[node->type].mouseWheel) {
				nodeBehaviourList[node->type].mouseWheel(node, down, x, y);
			} else {
				if (node->wheelUp && node->wheelDown)
					MN_ExecuteActions(menu, (down ? node->wheelDown : node->wheelUp));
				else
					MN_ExecuteActions(menu, node->wheel);
			}
		}

		if (menu->renderNode || menu->popupNode)
			/* don't care about non-rendered windows */
			return;
	}
}
