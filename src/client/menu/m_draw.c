/**
 * @file m_draw.c
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
#include "m_draw.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_input.h"
#include "m_dragndrop.h"
#include "m_tooltip.h"
#include "m_nodes.h"

static cvar_t *mn_debugmenu;
static cvar_t *mn_debugmenu2;
cvar_t *mn_show_tooltips;

static const invList_t *itemHover;

/**
 * @brief assign an hover item for the draw pipeline
 * @todo think better about this mecanism
 * @sa itemHover
 */
void MN_SetItemHover(const invList_t *item) {
	itemHover = item;
}

static void MN_DrawBorder (const menuNode_t *node)
{
	vec2_t nodepos;

	MN_GetNodeAbsPos(node, nodepos);
	/** @todo use GL_LINE_LOOP + array here */
	/* left */
	R_DrawFill(nodepos[0] - node->padding - node->border, nodepos[1] - node->padding - node->border,
		node->border, node->size[1] + (node->padding*2) + (node->border*2), node->align, node->bordercolor);
	/* right */
	R_DrawFill(nodepos[0] + node->size[0] + node->padding, nodepos[1] - node->padding - node->border,
		node->border, node->size[1] + (node->padding*2) + (node->border*2), node->align, node->bordercolor);
	/* top */
	R_DrawFill(nodepos[0] - node->padding, nodepos[1] - node->padding - node->border,
		node->size[0] + (node->padding*2), node->border, node->align, node->bordercolor);
	/* down */
	R_DrawFill(nodepos[0] - node->padding, nodepos[1] + node->size[1] + node->padding,
		node->size[0] + (node->padding*2), node->border, node->align, node->bordercolor);
}

/**
 * @brief hack to catch a mouse move event when we redraw the GUI
 */
static void MN_CheckMouseMove (void)
{
	static int oldX = 0, oldY = 0;
	if (mousePosX != oldX || mousePosY != oldY) {
		oldX = mousePosX;
		oldY = mousePosY;
		MN_MouseMove(mousePosX, mousePosY);
	}
}

static void MN_DrawDebugMenu2 (void)
{
	static vec4_t red = {1, 0.0, 0.0, 1.0};
	/*static vec4_t redalpha = {1, 0.0, 0.0, 0.2};*/
	static vec4_t white = {1, 1.0, 1.0, 1.0};
	menuNode_t *node = mouseOverTest;
	vec2_t pos;
	int size = 20;
	int weigth = 5;
	int sp;
	int posy = 0;

	for (sp = 0; sp < mn.menuStackPos; sp++) {
		menu_t *menu = mn.menuStack[sp];
		if (node && node->menu == menu)
			R_ColorBlend(red);
		else
			R_ColorBlend(white);
		R_FontDrawString("f_small_bold", ALIGN_UL, 0, posy, 0, posy, 200, 200, 0, menu->name, 0, 0, NULL, qfalse, LONGLINES_WRAP);
		posy += 15;
	}
	R_ColorBlend(NULL);

	if (node == NULL)
		return;

	MN_GetNodeAbsPos(node, pos);

	R_ColorBlend(red);
	R_FontDrawString("f_small_bold", ALIGN_LR, pos[0], pos[1], pos[0], pos[1], 200, 200, 0, node->name, 0, 0, NULL, qfalse, LONGLINES_WRAP);
	R_ColorBlend(NULL);

	/*R_DrawFill(pos[0], pos[1], node->size[0], node->size[1], ALIGN_UL, redalpha);*/
	R_DrawFill(pos[0], pos[1], size, weigth, ALIGN_UL, red);
	R_DrawFill(pos[0], pos[1], weigth, size, ALIGN_UL, red);
	R_DrawFill(pos[0] + node->size[0] - size, pos[1]+node->size[1] - weigth, size, weigth, ALIGN_UL, red);
	R_DrawFill(pos[0] + node->size[0] - weigth, pos[1]+node->size[1] - size, weigth, size, ALIGN_UL, red);
}

/**
 * @brief Draws the menu stack
 * @sa SCR_UpdateScreen
 */
void MN_DrawMenus (void)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, pp;
	qboolean mouseOver = qfalse;
	vec2_t nodepos;

	MN_CheckMouseMove();

	MN_SetItemHover(NULL);

	/* render every menu on top of a menu with a render node */
	pp = 0;
	sp = mn.menuStackPos;
	while (sp > 0) {
		if (mn.menuStack[--sp]->renderNode)
			break;
		if (mn.menuStack[sp]->popupNode)
			pp = sp;
	}
	if (pp < sp)
		pp = sp;

	/* Reset info for preview rendering of dragged items. */
	dragInfo.toNode = NULL;

	while (sp < mn.menuStackPos) {
		menu = mn.menuStack[sp++];
		/* event node */
		if (menu->eventNode) {
			if (menu->eventNode->timeOut > 0 && (menu->eventNode->timeOut == 1 || (!menu->eventTime || (menu->eventTime + menu->eventNode->timeOut < cls.realtime)))) {
				menu->eventTime = cls.realtime;
				MN_ExecuteActions(menu, menu->eventNode->click);
#ifdef DEBUG
				Com_DPrintf(DEBUG_CLIENT, "Event node '%s' '%i\n", menu->eventNode->name, menu->eventNode->timeOut);
#endif
			}
		}
		for (node = menu->firstNode; node; node = node->next) {
			if (node->invis || !nodeBehaviourList[node->type].draw)
				continue;

			/* if construct */
			if (!MN_CheckCondition(node))
				continue;

			MN_GetNodeAbsPos(node, nodepos);

			/* timeout? */
			if (node->timePushed) {
				if (node->timePushed + node->timeOut < cls.realtime) {
					node->timePushed = 0;
					node->invis = qtrue;
					/* only timeout this once, otherwise there is a new timeout after every new stack push */
					if (node->timeOutOnce)
						node->timeOut = 0;
					Com_DPrintf(DEBUG_CLIENT, "MN_DrawMenus: timeout for node '%s'\n", node->name);
					continue;
				}
			}

			/* mouse effects */
			/* don't allow to open more than one selectbox */
			if (sp > pp) {
				/* in and out events */
				mouseOver = MN_CheckNodeZone(node, mousePosX, mousePosY);
				/** @todo This won't work anymore - as the return value was an int before and
				 * was the line you clicked in the textnode */
				if (mouseOver != node->state) {
					/* maybe we are leaving to another menu */
					menu->hoverNode = NULL;
					if (mouseOver)
						MN_ExecuteActions(menu, node->mouseIn);
					else
						MN_ExecuteActions(menu, node->mouseOut);
					/** @todo This won't work anymore - as the return value was an int before and
					 * was the line you clicked in the textnode */
					node->state = mouseOver;
				}
			}

			/* check node size x and y value to check whether they are zero */
			if (node->size[0] && node->size[1]) {
				if (node->bgcolor) {
					R_DrawFill(nodepos[0], nodepos[1], node->size[0], node->size[1], 0, node->bgcolor);
				}
				if (node->border && node->bordercolor)
					MN_DrawBorder(node);
			}

			if (node->border && node->bordercolor && node->size[0] && node->size[1] && node->pos)
				MN_DrawBorder(node);

			/* mouse darken effect */
			if (node->mousefx && node->type == MN_PIC && mouseOver && sp > pp) {
				vec4_t color;
				VectorScale(node->color, 0.8, color);
				color[3] = node->color[3];
				R_ColorBlend(color);
			}
#if 0 /* if it need move this function into some nodes */
			else if (node->type != MN_SELECTBOX)
				R_ColorBlend(node->color);
#endif

			/* draw the node */
			if (nodeBehaviourList[node->type].draw) {
				nodeBehaviourList[node->type].draw(node);
			}

			/* mouseover? */
			if (node->state)
				menu->hoverNode = node;

			if (mn_debugmenu->integer)
				MN_DrawTooltip("f_small", node->name, nodepos[0], nodepos[1], node->size[1], 0);

			R_ColorBlend(NULL);

		}	/* for */
		if (sp == mn.menuStackPos && menu->hoverNode && mn_show_tooltips->integer) {
			/* We are hovering over an item and also want to display it.
			 * Make sure that we draw this on top of every other node. */

			if (itemHover) {
				char tooltiptext[MAX_VAR * 2];
				const int itemToolTipWidth = 250;

				/* Get name and info about item */
				MN_GetItemTooltip(itemHover->item, tooltiptext, sizeof(tooltiptext));
#ifdef DEBUG
				/* Display stored container-coordinates of the item. */
				Q_strcat(tooltiptext, va("\n%i/%i", itemHover->x, itemHover->y), sizeof(tooltiptext));
#endif
				MN_DrawTooltip("f_small", tooltiptext, mousePosX, mousePosY, itemToolTipWidth, 0);
			} else {
				MN_Tooltip(menu, menu->hoverNode, mousePosX, mousePosY);
				menu->hoverNode = NULL;
			}
		}
	}

	/* draw a special notice */
	if (cl.time < cl.msgTime) {
		menu = MN_GetActiveMenu();
		if (menu->noticePos[0] || menu->noticePos[1])
			MN_DrawNotice(menu->noticePos[0], menu->noticePos[1]);
		else
			MN_DrawNotice(500, 110);
	}

	/* debug help for comming architecture */
	if (mn_debugmenu2->integer) {
		MN_DrawDebugMenu2();
	}

	R_ColorBlend(NULL);
}

void MN_DrawMenusInit (void)
{
	mn_debugmenu = Cvar_Get("mn_debugmenu", "0", 0, "Prints node names for debugging purposes");
	mn_debugmenu2 = Cvar_Get("mn_debugmenu2", "0", 0, "Prints active node names for debugging the coming architecture");
	mn_show_tooltips = Cvar_Get("mn_show_tooltips", "1", CVAR_ARCHIVE, "Show tooltips in menus and hud");
}

/**
 * @brief Displays a message over all menus.
 * @sa SCR_DisplayHudMessage
 * @param[in] time is a ms values
 * @param[in] text text is already translated here
 */
void MN_DisplayNotice (const char *text, int time)
{
	cl.msgTime = cl.time + time;
	Q_strncpyz(cl.msgText, text, sizeof(cl.msgText));
}
