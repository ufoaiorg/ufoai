/**
 * @file m_draw.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "m_internal.h"
#include "m_draw.h"
#include "m_actions.h"
#include "m_input.h"
#include "m_timer.h" /* define MN_HandleTimers */
#include "m_dragndrop.h"
#include "m_tooltip.h"
#include "m_render.h"
#include "node/m_node_abstractnode.h"

#include "../client.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_misc.h"

static cvar_t *mn_show_tooltips;
static const int TOOLTIP_DELAY = 500; /* delay that msecs before showing tooltip */
static qboolean tooltipVisible = qfalse;
static menuTimer_t *tooltipTimer;

static int noticeTime;
static char noticeText[256];
static menuNode_t *noticeMenu;

/**
 * @brief Node we will draw over
 * @sa MN_CaptureDrawOver
 * @sa nodeBehaviour_t.drawOverMenu
 */
static menuNode_t *drawOverNode;

/**
 * @brief Capture a node we will draw over all nodes per menu
 * @note The node must be captured every frames
 * @todo it can be better to capture the draw over only one time (need new event like mouseEnter, mouseLeave)
 */
void MN_CaptureDrawOver (menuNode_t *node)
{
	drawOverNode = node;
}

#ifdef DEBUG

static int debugTextPositionY = 0;
static int debugPositionX = 0;
#define DEBUG_PANEL_WIDTH 300

static void MN_HighlightNode (const menuNode_t *node, const vec4_t color)
{
	static const vec4_t grey = {0.7, 0.7, 0.7, 1.0};
	vec2_t pos;
	int width;
	int lineDefinition[4];
	const char* text;

	if (node->parent)
		MN_HighlightNode(node->parent, grey);

	MN_GetNodeAbsPos(node, pos);

	text = va("%s (%s)", node->name, node->behaviour->name);
	R_FontTextSize("f_small_bold", text, DEBUG_PANEL_WIDTH, LONGLINES_PRETTYCHOP, &width, NULL, NULL, NULL);

	R_Color(color);
	MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX + 20, debugTextPositionY, debugPositionX + 20, debugTextPositionY, DEBUG_PANEL_WIDTH, DEBUG_PANEL_WIDTH, 0, text, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;

	if (debugPositionX != 0) {
		lineDefinition[0] = debugPositionX + 20;
		lineDefinition[2] = pos[0] + node->size[0];
	} else {
		lineDefinition[0] = debugPositionX + 20 + width;
		lineDefinition[2] = pos[0];
	}
	lineDefinition[1] = debugTextPositionY - 5;
	lineDefinition[3] = pos[1];
	R_DrawLine(lineDefinition, 1);
	R_Color(NULL);

	/* exclude rect */
	if (node->excludeRectNum) {
		int i;
		vec4_t trans = {1, 1, 1, 1};
		Vector4Copy(color, trans);
		trans[3] = trans[3] / 2;
		for (i = 0; i < node->excludeRectNum; i++) {
			const int x = pos[0] + node->excludeRect[i].pos[0];
			const int y = pos[1] + node->excludeRect[i].pos[1];
			MN_DrawFill(x, y, node->excludeRect[i].size[0], node->excludeRect[i].size[1], trans);
		}
	}

	/* bounded box */
	R_DrawRect(pos[0] - 1, pos[1] - 1, node->size[0] + 2, node->size[1] + 2, color, 2.0, 0x3333);
}

/**
 * @brief Prints active node names for debugging
 */
static void MN_DrawDebugMenuNodeNames (void)
{
	static const vec4_t red = {1.0, 0.0, 0.0, 1.0};
	static const vec4_t green = {0.0, 0.5, 0.0, 1.0};
	static const vec4_t white = {1, 1.0, 1.0, 1.0};
	static const vec4_t background = {0.0, 0.0, 0.0, 0.5};
	menuNode_t *hoveredNode;
	int stackPosition;

	debugTextPositionY = 100;

	/* x panel position with hysteresis */
	if (mousePosX < viddef.virtualWidth / 3)
		debugPositionX = viddef.virtualWidth - DEBUG_PANEL_WIDTH;
	if (mousePosX > 2 * viddef.virtualWidth / 3)
		debugPositionX = 0;

	/* mouse position */
	MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, va("Mouse X: %i Y: %i", mousePosX, mousePosY), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	/* main menus */
	MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "main active menu:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX+20, debugTextPositionY, debugPositionX+20, debugTextPositionY, 200, 200, 0, Cvar_GetString("mn_sys_active"), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "main option menu:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX+20, debugTextPositionY, debugPositionX+20, debugTextPositionY, 200, 200, 0, Cvar_GetString("mn_sys_main"), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "-----------------------", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;

	/* background */
	MN_DrawFill(debugPositionX, debugTextPositionY, DEBUG_PANEL_WIDTH, VID_NORM_HEIGHT - debugTextPositionY - 100, background);

	/* menu stack */
	R_Color(white);
	MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "menu stack:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	for (stackPosition = 0; stackPosition < mn.windowStackPos; stackPosition++) {
		menuNode_t *menu = mn.windowStack[stackPosition];
		MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX+20, debugTextPositionY, debugPositionX+20, debugTextPositionY, 200, 200, 0, menu->name, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;
	}

	/* hovered node */
	hoveredNode = MN_GetHoveredNode();
	if (hoveredNode) {
		MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "-----------------------", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;

		MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "hovered node:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;
		MN_HighlightNode(hoveredNode, red);
		R_Color(white);
	}

	/* target node */
	if (MN_DNDIsDragging()) {
		menuNode_t *targetNode = MN_DNDGetTargetNode();
		if (targetNode) {
			MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "-----------------------", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
			debugTextPositionY += 15;

			R_Color(green);
			MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "drag and drop target node:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
			debugTextPositionY += 15;
			MN_HighlightNode(targetNode, green);
		}
	}
	R_Color(NULL);
}
#endif


static void MN_CheckTooltipDelay (menuNode_t *node, menuTimer_t *timer)
{
	tooltipVisible = qtrue;
	MN_TimerStop(timer);
}

static void MN_DrawNode (menuNode_t *node)
{
	static int globalTransX = 0;
	static int globalTransY = 0;
	menuNode_t *child;
	vec2_t pos;

	/* update the layout */
	node->behaviour->doLayout(node);

	/* skip invisible, virtual, and undrawable nodes */
	if (node->invis || node->behaviour->isVirtual)
		return;
	/* if construct */
	if (!MN_CheckVisibility(node))
		return;

	MN_GetNodeAbsPos(node, pos);

	/** @todo remove it when its possible:
	 * we can create a 'box' node with this properties,
	 * but we often don't need it */
	/* check node size x and y value to check whether they are zero */
	if (node->size[0] && node->size[1]) {
		if (node->bgcolor[3] != 0)
			MN_DrawFill(pos[0], pos[1], node->size[0], node->size[1], node->bgcolor);

		if (node->border && node->bordercolor[3] != 0) {
			R_DrawRect(pos[0], pos[1], node->size[0], node->size[1],
				node->bordercolor, node->border, 0xFFFF);
		}
	}

	/* draw the node */
	if (node->behaviour->draw) {
		node->behaviour->draw(node);
	}

	/* draw all child */
	if (!node->behaviour->drawItselfChild && node->firstChild) {
		qboolean hasClient = qfalse;
		vec2_t clientPosition;
		if (node->behaviour->getClientPosition) {
			node->behaviour->getClientPosition(node, clientPosition);
			hasClient = qtrue;
		}

		R_PushClipRect(pos[0] + globalTransX, pos[1] + globalTransY, node->size[0], node->size[1]);

		/** @todo move it at the right position */
		if (hasClient) {
			vec3_t trans;
			globalTransX += clientPosition[0];
			globalTransY += clientPosition[1];
			trans[0] = clientPosition[0];
			trans[1] = clientPosition[1];
			trans[2] = 0;
			R_Transform(trans, NULL, NULL);
		}

		for (child = node->firstChild; child; child = child->next)
			MN_DrawNode(child);

		/** @todo move it at the right position */
		if (hasClient) {
			vec3_t trans;
			globalTransX -= clientPosition[0];
			globalTransY -= clientPosition[1];
			trans[0] = -clientPosition[0];
			trans[1] = -clientPosition[1];
			trans[2] = 0;
			R_Transform(trans, NULL, NULL);
		}

		R_PopClipRect();
	}
}

/**
 * @brief Generic notice function that renders a message to the screen
 * @todo Move it into Window node, and/or convert it in a reserved named string node and update the protocol
 */
static void MN_DrawNotice (void)
{
	const vec4_t noticeBG = { 1.0f, 0.0f, 0.0f, 0.2f };
	const vec4_t noticeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	int height = 0, width = 0;
	const int maxWidth = 320;
	const int maxHeight = 100;
	const char *font = "f_normal";
	int lines = 5;
	int dx; /**< Delta-x position. Relative to original x position. */
	int x, y;

	if (noticeMenu->u.window.noticePos[0] || noticeMenu->u.window.noticePos[1]) {
		x = noticeMenu->u.window.noticePos[0];
		y = noticeMenu->u.window.noticePos[1];
	} else {
		x = 500;
		y = 110;
	}

	/* relative to the menu */
	x += noticeMenu->pos[0];
	y += noticeMenu->pos[1];

	R_FontTextSize(font, noticeText, maxWidth, LONGLINES_WRAP, &width, &height, NULL, NULL);

	if (!width)
		return;

	if (x + width + 3 > viddef.virtualWidth)
		dx = -(width + 10);
	else
		dx = 0;

	MN_DrawFill(x - 1 + dx, y - 1, width + 4, height + 4, noticeBG);
	R_Color(noticeColor);
	MN_DrawString(font, 0, x + 1 + dx, y + 1, x + 1, y + 1, maxWidth, maxHeight, 0, noticeText, lines, 0, NULL, qfalse, LONGLINES_WRAP);
	R_Color(NULL);
}

/**
 * @brief Draws the menu stack
 * @sa SCR_UpdateScreen
 */
void MN_Draw (void)
{
	menuNode_t *hoveredNode;
	menuNode_t *menu;
	int pos;
	qboolean mouseMoved = qfalse;

	MN_HandleTimers();

	assert(mn.windowStackPos >= 0);

	mouseMoved = MN_CheckMouseMove();
	hoveredNode = MN_GetHoveredNode();

	/* handle delay time for tooltips */
	if (mouseMoved && tooltipVisible) {
		MN_TimerStop(tooltipTimer);
		tooltipVisible = qfalse;
	} else if (!tooltipVisible && !mouseMoved && !tooltipTimer->isRunning && mn_show_tooltips->integer && hoveredNode) {
		MN_TimerStart(tooltipTimer);
	}

	/* under a fullscreen, menu should not be visible */
	pos = MN_GetLastFullScreenWindow();
	if (pos < 0)
		return;

	/* draw all visible menus */
	for (; pos < mn.windowStackPos; pos++) {
		menu = mn.windowStack[pos];

		drawOverNode = NULL;

		MN_DrawNode(menu);

		/* draw a special notice */
		if (menu == noticeMenu && cls.realtime < noticeTime)
			MN_DrawNotice();

		/* draw a node over the menu */
		if (drawOverNode && drawOverNode->behaviour->drawOverMenu) {
			drawOverNode->behaviour->drawOverMenu(drawOverNode);
		}
	}

	/* unactive notice */
	if (cls.realtime >= noticeTime && noticeMenu != NULL)
		noticeMenu = NULL;

	/* draw tooltip */
	if (hoveredNode && tooltipVisible && !MN_DNDIsDragging()) {
		if (hoveredNode->behaviour->drawTooltip) {
			hoveredNode->behaviour->drawTooltip(hoveredNode, mousePosX, mousePosY);
		} else {
			MN_Tooltip(hoveredNode, mousePosX, mousePosY);
		}
	}

#ifdef DEBUG
	/* debug information */
	if (MN_DebugMode() >= 1) {
		MN_DrawDebugMenuNodeNames();
	}
#endif
}

void MN_DrawCursor (void)
{
	MN_DrawDragAndDrop(mousePosX, mousePosY);
}

/**
 * @brief Displays a message over all menus.
 * @sa HUD_DisplayMessage
 * @param[in] time is a ms values
 * @param[in] text text is already translated here
 * @param[in] windowName Window name where we must display the notice; else NULL to use the current active window
 */
void MN_DisplayNotice (const char *text, int time, const char* windowName)
{
	noticeTime = cls.realtime + time;
	Q_strncpyz(noticeText, text, sizeof(noticeText));

	if (windowName == NULL) {
		noticeMenu = MN_GetActiveWindow();
		if (noticeMenu == NULL)
			Com_Printf("MN_DisplayNotice: No active menu\n");
	} else {
		noticeMenu = MN_GetWindow(windowName);
		if (noticeMenu == NULL)
			Com_Printf("MN_DisplayNotice: '%s' not found\n", windowName);
	}
}

void MN_InitDraw (void)
{
	mn_show_tooltips = Cvar_Get("mn_show_tooltips", "1", CVAR_ARCHIVE, "Show tooltips in menus and hud");
	tooltipTimer = MN_AllocTimer(NULL, TOOLTIP_DELAY, MN_CheckTooltipDelay);
}
