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
#include "m_parse.h"
#include "m_input.h"
#include "m_dragndrop.h"
#include "m_node_text.h"
#include "m_node_tab.h"
#include "m_nodes.h"
#include "m_draw.h"
#include "../cl_global.h"

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
 * @brief save the captured node
 * @sa MN_SetMouseCapture
 * @sa MN_GetMouseCapture
 * @sa MN_MouseRelease
 * @todo think about replacing it by a boolean. When capturedNode != NULL => mouseOverTest == capturedNode
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
 * @todo update mouse events (in, out)
 */
void MN_MouseRelease (void)
{
	capturedNode = NULL;
	MN_FocusRemove();
}

/**
 * @brief save the node under the mouse
 * @todo rename it into hoveredNode when the code is stable
 */
menuNode_t *mouseOverTest;

/**
 * @brief save the previous node under the mouse
 */
static menuNode_t *oldMouseOverTest;

/**
 * @brief save old position of the mouse
 */
static int oldX = 0, oldY = 0;

void MN_InvalidateMouse (void) {
	oldX = -1;
	oldY = -1;
}

/**
 * @brief hack to catch a mouse move event when we redraw the GUI
 */
void MN_CheckMouseMove (void)
{
	if (mousePosX != oldX || mousePosY != oldY) {
		oldX = mousePosX;
		oldY = mousePosY;
		MN_MouseMove(mousePosX, mousePosY);
	}
}

/**
 * @brief Is called everytime the mouse position change
 */
void MN_MouseMove (int x, int y)
{
	menu_t *menu;
	int sp;
	menuNode_t *node;

	/* send the captured move mouse event */
	if (capturedNode) {
		if (capturedNode->behaviour->capturedMouseMove)
			capturedNode->behaviour->capturedMouseMove(capturedNode, x, y);
		return;
	}

	MN_FocusRemove();


	/* find the first menu under the mouse */
	menu = NULL;
	for (sp = mn.menuStackPos-1; sp >= 0; sp--) {
		menu_t *m = mn.menuStack[sp];
		/* full screen menu */
		if (m->size[0] == 0 && m->size[1] == 0) {
			menu = m;
			break;
		}
		/* check mouse vs menu boundedbox */
		if (x >= m->pos[0] && x <= m->pos[0] + m->size[0]
			&& y >= m->pos[1] && y <= m->pos[1] + m->size[1]) {
			menu = m;
			break;
		}
	}

	/* find the first node under the mouse (last of the node list) */
	mouseOverTest = NULL;
	if (menu) {
		/* relative position */
		const int xx = x - menu->pos[0];
		const int yy = y - menu->pos[1];

		/* check mouse vs node boundedbox */
		for (node = menu->firstChild; node; node = node->next) {
			if (node->invis || !MN_CheckCondition(node))
				continue;
			if (xx >= node->pos[0] && xx <= node->pos[0] + node->size[0]
				&& yy >= node->pos[1] && yy <= node->pos[1] + node->size[1])
				mouseOverTest = node;
		}
	}

	/* update nodes: send 'in' and 'out' event */
	if (oldMouseOverTest != mouseOverTest) {
		if (oldMouseOverTest) {
			MN_ExecuteActions(oldMouseOverTest->menu, oldMouseOverTest->mouseOut);
			oldMouseOverTest->menu->hoverNode = NULL;
			oldMouseOverTest->state = qfalse;
		}
		if (mouseOverTest) {
			mouseOverTest->state = qtrue;
			mouseOverTest->menu->hoverNode = mouseOverTest;
			MN_ExecuteActions(mouseOverTest->menu, mouseOverTest->mouseIn);
		}
	}
	oldMouseOverTest = mouseOverTest;

	/* send the move event */
	if (mouseOverTest && mouseOverTest->behaviour->mouseMove) {
		mouseOverTest->behaviour->mouseMove(mouseOverTest, x, y);
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
#if 0 /* new handler */
	int sp;
	qboolean insideNode = qfalse;
#endif
	menuNode_t *node;
	qboolean mouseOver;

	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->leftClick)
			capturedNode->behaviour->leftClick(capturedNode, x, y);
		return;
	}

	if (mouseOverTest) {
		node = mouseOverTest;
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

		if (node->behaviour->leftClick) {
			node->behaviour->leftClick(node, x, y);
		} else {
			MN_ExecuteActions(node->menu, node->click);
		}
		return;
	}
#if 0 /* new handler */
	sp = mn.menuStackPos;

	while (sp > 0 && !insideNode) {
		menu_t *menu = mn.menuStack[--sp];
		menuNode_t *execute_node = NULL;

		/* check mouse vs menu boundedbox */
		if (menu->size[0] != 0 && menu->size[1] != 0) {
			if (x < menu->pos[0] || x > menu->pos[0] + menu->size[0]
			 || y < menu->pos[1] || y > menu->pos[1] + menu->size[1])
				continue;
		}

		for (node = menu->firstChild; node; node = node->next) {
			if (!node->behaviour->leftClick && !node->click)
				continue;

			/* check whether mouse is over this node */
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
			} else {
				mouseOver = MN_CheckNodeZone(node, x, y);
			}

			if (!mouseOver)
				continue;

			/* check whether we clicked at least on one menu node */
			insideNode = qtrue;

			/* found a node -> do actions */
			if (node->behaviour->leftClick) {
				node->behaviour->leftClick(node, x, y);
			} else {
				/* Save the action for later execution. */
				if (node->click && (node->click->type != EA_NULL))
					execute_node = node;
			}
			if (node->behaviour->id == MN_TEXT) {
				execute_node = node;
				mn.mouseRepeat.textLine = MN_TextNodeGetLine(node, x, y);
			}
		}

		/* Only execute the last-found (i.e. from the top-most displayed node) action found.
		 * Especially needed for button-nodes that are (partially) overlayed and all have actions defined.
		 * e.g. the firemode checkboxes. */
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
				/* Delay between the 2 first actions is longer than the delay between other actions (see IN_Parse()) */
				mn.mouseRepeat.nexttime = cls.realtime + mn.mouseRepeat.clickDelay;
				mn.mouseRepeat.menu = menu;
				mn.mouseRepeat.action = execute_node->click;
			}
			if (execute_node->behaviour->id != MN_TEXT)
				MN_ExecuteActions(menu, execute_node->click);
		}

		/** @todo maybe we should also check sp == mn.menuStackPos here */
		if (!insideNode && menu->leaveNode)
			MN_ExecuteActions(menu, menu->leaveNode->click);

		break;
	}
#endif
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
#if 0 /* new handler */
	qboolean mouseOver;
	int sp;
	qboolean insideNode = qfalse;
#endif

	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->rightClick)
			capturedNode->behaviour->rightClick(capturedNode, x, y);
		return;
	}

	if (mouseOverTest) {
		if (mouseOverTest->behaviour->rightClick) {
			mouseOverTest->behaviour->rightClick(mouseOverTest, x, y);
		} else {
			MN_ExecuteActions(mouseOverTest->menu, mouseOverTest->rclick);
		}
		return;
	}
#if 0 /* new handler */
	sp = mn.menuStackPos;

	while (sp > 0 && !insideNode) {
		menu_t *menu = mn.menuStack[--sp];
		menuNode_t *node;

		/* check mouse vs menu boundedbox */
		if (menu->size[0] != 0 && menu->size[1] != 0) {
			if (x < menu->pos[0] || x > menu->pos[0] + menu->size[0]
			 || y < menu->pos[1] || y > menu->pos[1] + menu->size[1])
				continue;
		}

		for (node = menu->firstChild; node; node = node->next) {
			/* no right click for this node defined */
			if (!node->behaviour->rightClick && !node->rclick)
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			insideNode = qtrue;

			/* found a node -> do actions */
			if (node->behaviour->rightClick) {
				node->behaviour->rightClick(node, x, y);
			} else {
				MN_ExecuteActions(menu, node->rclick);
			}
		}

		break;
	}
#endif
}

/**
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MouseWheel
 */
void MN_MiddleClick (int x, int y)
{
#if 0 /* new handler */
	menuNode_t *node;
	int sp;
	qboolean mouseOver;
	qboolean insideNode = qfalse;
#endif

	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->middleClick)
			capturedNode->behaviour->middleClick(capturedNode, x, y);
		return;
	}

	if (mouseOverTest) {
		if (mouseOverTest->behaviour->middleClick) {
			mouseOverTest->behaviour->middleClick(mouseOverTest, x, y);
		} else {
			MN_ExecuteActions(mouseOverTest->menu, mouseOverTest->mclick);
		}
		return;
	}
#if 0 /* new handler */
	sp = mn.menuStackPos;

	while (sp > 0 && !insideNode) {
		menu_t *menu = mn.menuStack[--sp];

		/* check mouse vs menu boundedbox */
		if (menu->size[0] != 0 && menu->size[1] != 0) {
			if (x < menu->pos[0] || x > menu->pos[0] + menu->size[0]
			 || y < menu->pos[1] || y > menu->pos[1] + menu->size[1])
				continue;
		}

		for (node = menu->firstChild; node; node = node->next) {
			/* no middle click for this node defined */
			if (!node->behaviour->middleClick && !node->mclick)
				continue;

			insideNode = qtrue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			if (node->behaviour->middleClick) {
				node->behaviour->middleClick(node, x, y);
			} else {
				MN_ExecuteActions(menu, node->mclick);
			}
		}

		break;
	}
#endif
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
#if 0 /* new handler */
	menuNode_t *node;
	int sp;
	qboolean mouseOver;
	qboolean insideNode = qfalse;
#endif

	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->mouseWheel)
			capturedNode->behaviour->mouseWheel(capturedNode, down, x, y);
		return;
	}

	if (mouseOverTest) {
		if (mouseOverTest->behaviour->mouseWheel) {
			mouseOverTest->behaviour->mouseWheel(mouseOverTest, down, x, y);
		} else {
			if (mouseOverTest->wheelUp && mouseOverTest->wheelDown)
				MN_ExecuteActions(mouseOverTest->menu, (down ? mouseOverTest->wheelDown : mouseOverTest->wheelUp));
			else
				MN_ExecuteActions(mouseOverTest->menu, mouseOverTest->wheel);
		}
		return;
	}
#if 0 /* new handler */
	sp = mn.menuStackPos;

	while (sp > 0 && !insideNode) {
		menu_t *menu = mn.menuStack[--sp];

		/* check mouse vs menu boundedbox */
		if (menu->size[0] != 0 && menu->size[1] != 0) {
			if (x < menu->pos[0] || x > menu->pos[0] + menu->size[0]
			 || y < menu->pos[1] || y > menu->pos[1] + menu->size[1])
				continue;
		}

		for (node = menu->firstChild; node; node = node->next) {
			/* both wheelUp & wheelDown required */
			if (!node->behaviour->mouseWheel && !node->wheel && !(node->wheelUp && node->wheelDown))
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			insideNode = qtrue;

			/* found a node -> do actions */
			if (node->behaviour->mouseWheel) {
				node->behaviour->mouseWheel(node, down, x, y);
			} else {
				if (node->wheelUp && node->wheelDown)
					MN_ExecuteActions(menu, (down ? node->wheelDown : node->wheelUp));
				else
					MN_ExecuteActions(menu, node->wheel);
			}
		}

		break;
	}
#endif
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
	node = capturedNode ? capturedNode : mouseOverTest;

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
	node = capturedNode ? capturedNode : mouseOverTest;

	if (node == NULL)
		return;

	if (node->behaviour->mouseUp)
		node->behaviour->mouseUp(node, x, y, button);
}
