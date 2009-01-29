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
#include "../renderer/r_draw.h"
#include "m_main.h"
#include "m_draw.h"
#include "m_font.h"
#include "m_input.h"
#include "m_timer.h" /* define MN_HandleTimers */
#include "m_dragndrop.h"
#include "m_tooltip.h"
#include "m_nodes.h"
#include "node/m_node_abstractnode.h"

cvar_t *mn_debugmenu;
cvar_t *mn_show_tooltips;

static const int TOOLTIP_DELAY = 500; /* delay that msecs before showing tooltip */
static qboolean tooltipVisible = qfalse;
static menuTimer_t *tooltipTimer;

static void MN_DrawBorder (const menuNode_t *node)
{
	vec2_t nodepos;

	MN_GetNodeAbsPos(node, nodepos);
	/** @todo use GL_LINE_LOOP + array here */
	/* left */
	R_DrawFill(nodepos[0] - node->padding - node->border, nodepos[1] - node->padding - node->border,
		node->border, node->size[1] + (node->padding*2) + (node->border*2), ALIGN_UL, node->bordercolor);
	/* right */
	R_DrawFill(nodepos[0] + node->size[0] + node->padding, nodepos[1] - node->padding - node->border,
		node->border, node->size[1] + (node->padding*2) + (node->border*2), ALIGN_UL, node->bordercolor);
	/* top */
	R_DrawFill(nodepos[0] - node->padding, nodepos[1] - node->padding - node->border,
		node->size[0] + (node->padding*2), node->border, ALIGN_UL, node->bordercolor);
	/* down */
	R_DrawFill(nodepos[0] - node->padding, nodepos[1] + node->size[1] + node->padding,
		node->size[0] + (node->padding*2), node->border, ALIGN_UL, node->bordercolor);
}

static void MN_HilightNode (menuNode_t *node, vec4_t color, int textPositionY)
{
	const int size = 20;
	const int weigth = 5;
	vec2_t pos;
	MN_GetNodeAbsPos(node, pos);

	R_ColorBlend(color);
	R_FontDrawString("f_small_bold", ALIGN_UL, 20, textPositionY, 20, textPositionY, 400, 400, 0, va("%s (%s)", node->name, node->behaviour->name), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	R_ColorBlend(NULL);

	R_DrawFill(pos[0], pos[1], size, weigth, ALIGN_UL, color);
	R_DrawFill(pos[0], pos[1], weigth, size, ALIGN_UL, color);
	R_DrawFill(pos[0] + node->size[0] - size, pos[1]+node->size[1] - weigth, size, weigth, ALIGN_UL, color);
	R_DrawFill(pos[0] + node->size[0] - weigth, pos[1]+node->size[1] - size, weigth, size, ALIGN_UL, color);
}

/**
 * @brief Prints active node names for debugging
 */
static void MN_DrawDebugMenuNodeNames (void)
{
	static vec4_t red = {1.0, 0.0, 0.0, 1.0};
	static vec4_t green = {0.0, 0.5, 0.0, 1.0};
	static vec4_t redalpha = {1, 0.0, 0.0, 0.4};
	static vec4_t white = {1, 1.0, 1.0, 1.0};
	menuNode_t *hoveredNode = MN_GetHoveredNode();
	menuNode_t *targetNode = NULL;
	int sp;
	int posy = 15;
	if (MN_DNDIsDragging())
		targetNode = MN_DNDGetTargetNode();


	/* menu stack */
	R_ColorBlend(white);
	R_FontDrawString("f_small_bold", ALIGN_UL, 0, posy, 0, posy, 200, 200, 0, "menu stack:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	posy += 15;
	for (sp = 0; sp < mn.menuStackPos; sp++) {
		menu_t *menu = mn.menuStack[sp];
		if (hoveredNode && menu == hoveredNode->menu) {
			R_ColorBlend(red);
		} else if (targetNode && menu == targetNode->menu) {
			R_ColorBlend(green);
		} else {
			R_ColorBlend(white);
		}
		R_FontDrawString("f_small_bold", ALIGN_UL, 20, posy, 20, posy, 200, 200, 0, menu->name, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		posy += 15;
	}
	posy += 15;

	/* nodes */
	R_ColorBlend(white);
	R_FontDrawString("f_small_bold", ALIGN_UL, 0, posy, 0, posy, 200, 200, 0, "nodes:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	posy += 15;
	R_ColorBlend(NULL);

	if (targetNode) {
		MN_HilightNode(MN_DNDGetTargetNode(), green, posy);
		posy += 15;
	}
	if (hoveredNode) {
		int i;
		for (i = 0; i < hoveredNode->excludeRectNum; i++) {
			int x = hoveredNode->menu->pos[0] + hoveredNode->pos[0] + hoveredNode->excludeRect[i].pos[0];
			int y = hoveredNode->menu->pos[1] + hoveredNode->pos[1] + hoveredNode->excludeRect[i].pos[1];
			R_DrawFill(x, y, hoveredNode->excludeRect[i].size[0], hoveredNode->excludeRect[i].size[1], ALIGN_UL, redalpha);
		}
		MN_HilightNode(hoveredNode, red, posy);
		posy += 15;
	}
	R_ColorBlend(NULL);
}


static void MN_CheckTooltipDelay (menuNode_t *node, menuTimer_t *timer)
{
	tooltipVisible = qtrue;
}

/**
 * @brief Draws the menu stack
 * @todo move DrawMenusTest here
 * @sa SCR_UpdateScreen
 */
void MN_DrawMenus (void)
{
	menuNode_t *hoveredNode;
	menu_t *menu;
	int sp, pp;
	qboolean mouseMoved = qfalse;
	vec2_t nodepos;
	static const vec4_t modalBackground = {0, 0, 0, 0.6};

	mouseMoved = MN_CheckMouseMove();
	hoveredNode = MN_GetHoveredNode();

	/* handle delay time for tooltips */
	if (mouseMoved && tooltipTimer != NULL) {
		MN_TimerRelease(tooltipTimer);
		tooltipTimer = NULL;
		tooltipVisible = qfalse;
	} else if (!mouseMoved && tooltipTimer == NULL && mn_show_tooltips->integer && hoveredNode) {
		tooltipTimer = MN_AllocTimer(NULL, TOOLTIP_DELAY, MN_CheckTooltipDelay);
		MN_TimerStart(tooltipTimer);
	}

	MN_HandleTimers();

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

	while (sp < mn.menuStackPos) {
		menuNode_t *node;
		menu = mn.menuStack[sp++];
		/* event node */
		if (menu->onTimeOut) {
			if (menu->eventNode->timeOut > 0 && (menu->eventNode->timeOut == 1 || (!menu->eventTime || (menu->eventTime + menu->eventNode->timeOut < cls.realtime)))) {
				menu->eventTime = cls.realtime;
				MN_ExecuteActions(menu, menu->onTimeOut);
#ifdef DEBUG
				Com_DPrintf(DEBUG_CLIENT, "Event node '%s' '%i\n", menu->eventNode->name, menu->eventNode->timeOut);
#endif
			}
		}

		/** draker background for the last modal */
		if (menu->modal && sp == mn.menuStackPos) {
			R_DrawFill(0, 0, VID_NORM_WIDTH, VID_NORM_HEIGHT, ALIGN_UL, modalBackground);
		}

		for (node = menu->firstChild; node; node = node->next) {
			/* skip invisible, virtual, and undrawable nodes */
			if (node->invis || node->behaviour->isVirtual || !node->behaviour->draw)
				continue;

			/* if construct */
			if (!MN_CheckVisibility(node))
				continue;

			/** @todo remove it when its possible */
			MN_GetNodeAbsPos(node, nodepos);

			/* check node size x and y value to check whether they are zero */
			if (node->size[0] && node->size[1]) {
				if (node->bgcolor) {
					/** @todo remove it when its possible */
					R_DrawFill(nodepos[0], nodepos[1], node->size[0], node->size[1], 0, node->bgcolor);
				}
				if (node->border && node->bordercolor)
					/** @todo remove it when its possible */
					MN_DrawBorder(node);
			}

			/** @todo remove it when its possible */
			if (node->border && node->bordercolor && node->size[0] && node->size[1] && node->pos)
				MN_DrawBorder(node);

			/* draw the node */
			if (node->behaviour->draw) {
				node->behaviour->draw(node);
			}

			/** @todo remove it when its possible */
			R_ColorBlend(NULL);
		}	/* for */
	}

	/* draw tooltip */
	if (hoveredNode && tooltipVisible && !MN_DNDIsDragging()) {
		if (hoveredNode->behaviour->drawTooltip) {
			hoveredNode->behaviour->drawTooltip(hoveredNode, mousePosX, mousePosY);
		} else {
			MN_Tooltip(hoveredNode->menu, hoveredNode, mousePosX, mousePosY);
		}
	}

	/** @todo remove it (or clean up) when it possible. timeout? */
	if (hoveredNode && hoveredNode->timePushed) {
		if (hoveredNode->timePushed + hoveredNode->timeOut < cls.realtime) {
			hoveredNode->timePushed = 0;
			hoveredNode->invis = qtrue;
			/* only timeout this once, otherwise there is a new timeout after every new stack push */
			if (hoveredNode->timeOutOnce)
				hoveredNode->timeOut = 0;
			Com_DPrintf(DEBUG_CLIENT, "MN_DrawMenus: timeout for node '%s'\n", hoveredNode->name);
		}
	}

	/* draw a special notice */
	menu = MN_GetActiveMenu();
	if (cl.time < cl.msgTime) {
		if (menu && (menu->noticePos[0] || menu->noticePos[1]))
			MN_DrawNotice(menu->noticePos[0], menu->noticePos[1]);
		else
			MN_DrawNotice(500, 110);
	}

	/* debug information */
	if (mn_debugmenu->integer == 2) {
		MN_DrawDebugMenuNodeNames();
	}

	/** @todo better to use an assert(blend==NULL) to cleanup the code */
	R_ColorBlend(NULL);
}

void MN_DrawCursor (void)
{
	MN_DrawDragAndDrop(mousePosX, mousePosY);
}

void MN_DrawMenusInit (void)
{
	mn_debugmenu = Cvar_Get("mn_debugmenu", "0", 0, "Prints node names for debugging purposes - valid values are 1 and 2");
	mn_show_tooltips = Cvar_Get("mn_show_tooltips", "1", CVAR_ARCHIVE, "Show tooltips in menus and hud");
}

/**
 * @brief Displays a message over all menus.
 * @sa HUD_DisplayMessage
 * @param[in] time is a ms values
 * @param[in] text text is already translated here
 */
void MN_DisplayNotice (const char *text, int time)
{
	cl.msgTime = cl.time + time;
	Q_strncpyz(cl.msgText, text, sizeof(cl.msgText));
}
