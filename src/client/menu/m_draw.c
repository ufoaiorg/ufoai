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
#include "m_timer.h" /* define MN_HandleTimers */
#include "m_dragndrop.h"
#include "m_tooltip.h"
#include "m_nodes.h"
#include "node/m_node_container.h"
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
 * @brief Prints active node names for debugging
 */
static void MN_DrawDebugMenuNodeNames (void)
{
	static vec4_t red = {1, 0.0, 0.0, 1.0};
	/*static vec4_t redalpha = {1, 0.0, 0.0, 0.2};*/
	static vec4_t white = {1, 1.0, 1.0, 1.0};
	menuNode_t *node = MN_GetHoveredNode();
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
	qboolean mouseOver = qfalse;
	qboolean mouseMoved = qfalse;
	vec2_t nodepos;


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

	/* Reset info for preview rendering of dragged items. */
	dragInfo.toNode = NULL;

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
		for (node = menu->firstChild; node; node = node->next) {
			/* skip invisible, virtual, and undrawable nodes */
			if (node->invis || node->behaviour->isVirtual || !node->behaviour->draw)
				continue;

			/* if construct */
			if (!MN_CheckCondition(node))
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

			/** @todo remove it when its possible */
			/* mouse darken effect */
			if (node->mousefx && node->behaviour->id == MN_PIC && mouseOver && sp > pp) {
				vec4_t color;
				VectorScale(node->color, 0.8, color);
				color[3] = node->color[3];
				R_ColorBlend(color);
			}

			/* draw the node */
			if (node->behaviour->draw) {
				node->behaviour->draw(node);
			}

			/** @todo remove it when its possible */
			R_ColorBlend(NULL);
		}	/* for */
	}

	/* draw tooltip */
	if (hoveredNode && tooltipVisible) {
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
	const vec3_t scale = { 3.5, 3.5, 3.5 };
	vec3_t org;
	vec4_t color = { 1, 1, 1, 1 };
	int checkedTo = INV_DOES_NOT_FIT;

	/** Revert the rotation info for the cursor-item in case it
	 * has been changed (can happen in rare conditions).
	 * @todo Maybe we can later change this to reflect "manual" rotation?
	 * @todo Check if this causes problems when letting the item snap back to its original location. */
	dragInfo.item.rotated = qfalse;

	/* Draw "preview" of placed (&rotated) item. */
	if (dragInfo.toNode && !dragInfo.to->scroll) {
		const int oldRotated = dragInfo.item.rotated;

		checkedTo = Com_CheckToInventory(menuInventory, dragInfo.item.t, dragInfo.to, dragInfo.toX, dragInfo.toY, dragInfo.ic);

		if (checkedTo == INV_FITS_ONLY_ROTATED)
			dragInfo.item.rotated = qtrue;

		if (checkedTo && Q_strncmp(dragInfo.item.t->type, "armour", MAX_VAR)) {	/* If the item fits somehow and it's not armour */
			vec2_t nodepos;

			MN_GetNodeAbsPos(dragInfo.toNode, nodepos);
			if (dragInfo.to->single) { /* Get center of single container for placement of preview item */
				VectorSet(org,
					nodepos[0] + dragInfo.toNode->size[0] / 2.0,
					nodepos[1] + dragInfo.toNode->size[1] / 2.0,
					-40);
			} else {
				/* This is a "grid" container - we need to calculate the item-position
				 * (on the screen) from stored placement in the container and the
				 * calculated rotation info. */
				if (dragInfo.item.rotated)
					VectorSet(org,
						nodepos[0] + (dragInfo.toX + dragInfo.item.t->sy / 2.0) * C_UNIT,
						nodepos[1] + (dragInfo.toY + dragInfo.item.t->sx / 2.0) * C_UNIT,
						-40);
				else
					VectorSet(org,
						nodepos[0] + (dragInfo.toX + dragInfo.item.t->sx / 2.0) * C_UNIT,
						nodepos[1] + (dragInfo.toY + dragInfo.item.t->sy / 2.0) * C_UNIT,
						-40);
			}
			Vector4Set(color, 0.5, 0.5, 1, 1);	/**< Make the preview item look blueish */
			MN_DrawItem(NULL, org, &dragInfo.item, -1, -1, scale, color);	/**< Draw preview item. */
		}

		dragInfo.item.rotated = oldRotated ;
	}

	/* Draw the (2.) item at the cursor. */
	VectorSet(org, mousePosX, mousePosY, -50);
	if (dragInfo.toNode && checkedTo)
		Vector4Set(color, 1, 1, 1, 0.2);		/**< Tune down the opacity of the cursor-item if the preview item is drawn. */
	MN_DrawItem(NULL, org, &dragInfo.item, -1, -1, scale, color);


#ifdef PARANOID
	/** Debugging only - Will draw a marker in the upper left corner of the
	 * dragged item (which is the one used for calculating the resulting grid-coordinates).
	 * @todo Maybe we could make this a feature in some way. i.e. we could draw
	 * a special cursor at this place when dragging as a hint*/

	Vector4Set(color, 1, 0, 0, 1);
	if (dragInfo.item.t)
		VectorSet(org,
			mousePosX - (C_UNIT * dragInfo.item.t->sx - C_UNIT) / 2,
			mousePosY - (C_UNIT * dragInfo.item.t->sy - C_UNIT) / 2,
			-50);
	R_DrawCircle2D(org[0] * viddef.rx, org[1] * viddef.ry, 2.0, qtrue, color, 1.0);
#endif
}

void MN_DrawMenusInit (void)
{
	mn_debugmenu = Cvar_Get("mn_debugmenu", "0", 0, "Prints node names for debugging purposes - valid values are 1 and 2");
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
