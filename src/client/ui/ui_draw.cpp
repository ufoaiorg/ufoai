/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "ui_main.h"
#include "ui_nodes.h"
#include "ui_internal.h"
#include "ui_draw.h"
#include "ui_actions.h"
#include "ui_input.h"
#include "ui_node.h"
#include "ui_timer.h" /* define UI_HandleTimers */
#include "ui_dragndrop.h"
#include "ui_tooltip.h"
#include "ui_render.h"
#include "node/ui_node_abstractnode.h"

#include "../client.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_misc.h"

static const int TOOLTIP_DELAY = 500; /* delay that msecs before showing tooltip */

static cvar_t* ui_show_tooltips;
static bool tooltipVisible = false;
static uiTimer_t* tooltipTimer;

static int noticeTime;
static char noticeText[256];
static uiNode_t* noticeWindow;

/**
 * @brief Node we will draw over
 * @sa UI_CaptureDrawOver
 * @sa uiBehaviour_t.drawOverWindow
 */
static uiNode_t* drawOverNode;

/**
 * @brief Capture a node we will draw over all nodes (per window)
 * @note The node must be captured every frames
 * @todo it can be better to capture the draw over only one time (need new event like mouseEnter, mouseLeave)
 */
void UI_CaptureDrawOver (uiNode_t* node)
{
	drawOverNode = node;
}

#ifdef DEBUG

static int debugTextPositionY = 0;
static int debugPositionX = 0;
#define DEBUG_PANEL_WIDTH 300

static void UI_HighlightNode (const uiNode_t* node, const vec4_t color)
{
	vec2_t pos;
	int width;
	int lineDefinition[4];
	const char* text;

	if (node->parent) {
		static const vec4_t grey = {0.7, 0.7, 0.7, 1.0};
		UI_HighlightNode(node->parent, grey);
	}

	UI_GetNodeAbsPos(node, pos);

	text = va("%s (%s)", node->name, UI_Node_GetWidgetName(node));
	R_FontTextSize("f_small_bold", text, DEBUG_PANEL_WIDTH, LONGLINES_PRETTYCHOP, &width, nullptr, nullptr, nullptr);

	R_Color(color);
	UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX + 20, debugTextPositionY, debugPositionX + 20, DEBUG_PANEL_WIDTH, 0, text, 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;

	if (debugPositionX != 0) {
		lineDefinition[0] = debugPositionX + 20;
		lineDefinition[2] = pos[0] + node->box.size[0];
	} else {
		lineDefinition[0] = debugPositionX + 20 + width;
		lineDefinition[2] = pos[0];
	}
	lineDefinition[1] = debugTextPositionY - 5;
	lineDefinition[3] = pos[1];
	R_DrawLine(lineDefinition, 1);
	R_Color(nullptr);

	/* exclude rect */
	if (node->firstExcludeRect) {
		vec4_t trans = {1, 1, 1, 1};
		Vector4Copy(color, trans);
		trans[3] = trans[3] / 2;

		for (uiExcludeRect_t* current = node->firstExcludeRect; current != nullptr; current = current->next) {
			const int x = pos[0] + current->pos[0];
			const int y = pos[1] + current->pos[1];
			UI_DrawFill(x, y, current->size[0], current->size[1], trans);
		}
	}

	/* bounded box */
	UI_DrawRect(pos[0] - 1, pos[1] - 1, node->box.size[0] + 2, node->box.size[1] + 2, color, 2.0, 0x3333);
}

/**
 * @brief Prints active node names for debugging
 */
static void UI_DrawDebugNodeNames (void)
{
	static const vec4_t white = {1, 1.0, 1.0, 1.0};
	static const vec4_t background = {0.0, 0.0, 0.0, 0.5};

	debugTextPositionY = 100;

	/* x panel position with hysteresis */
	if (mousePosX < viddef.virtualWidth / 3)
		debugPositionX = viddef.virtualWidth - DEBUG_PANEL_WIDTH;
	if (mousePosX > 2 * viddef.virtualWidth / 3)
		debugPositionX = 0;

	/* mouse position */
	UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, 200, 0, va("Mouse X: %i Y: %i", mousePosX, mousePosY), 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	/* global */
	UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, 200, 0, "main active window:", 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX+20, debugTextPositionY, debugPositionX + 20, 200, 0, Cvar_GetString("ui_sys_active"), 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, 200, 0, "main option window:", 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX+20, debugTextPositionY, debugPositionX + 20, 200, 0, Cvar_GetString("ui_sys_main"), 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, 200, 0, "-----------------------", 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;

	/* background */
	UI_DrawFill(debugPositionX, debugTextPositionY, DEBUG_PANEL_WIDTH, VID_NORM_HEIGHT - debugTextPositionY - 100, background);

	/* window stack */
	R_Color(white);
	UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, 200, 0, "window stack:", 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	for (int stackPosition = 0; stackPosition < ui_global.windowStackPos; stackPosition++) {
		uiNode_t* window = ui_global.windowStack[stackPosition];
		UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX+20, debugTextPositionY, debugPositionX + 20, 200, 0, window->name, 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;
	}

	/* hovered node */
	uiNode_t* hoveredNode = UI_GetHoveredNode();
	if (hoveredNode) {
		static const vec4_t red = {1.0, 0.0, 0.0, 1.0};
		UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, 200, 0, "-----------------------", 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;

		UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, 200, 0, "hovered node:", 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;
		UI_HighlightNode(hoveredNode, red);
		R_Color(white);
	}

	/* target node */
	if (UI_DNDIsDragging()) {
		uiNode_t* targetNode = UI_DNDGetTargetNode();
		if (targetNode) {
			static const vec4_t green = {0.0, 0.5, 0.0, 1.0};
			UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, 200, 0, "-----------------------", 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
			debugTextPositionY += 15;

			R_Color(green);
			UI_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, 200, 0, "drag and drop target node:", 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
			debugTextPositionY += 15;
			UI_HighlightNode(targetNode, green);
		}
	}
	R_Color(nullptr);
}
#endif


static void UI_CheckTooltipDelay (uiNode_t* node, uiTimer_t* timer)
{
	tooltipVisible = true;
	UI_TimerStop(timer);
}

static void UI_DrawNode (uiNode_t* node)
{
	/* update the layout */
	UI_Validate(node);

	/* skip invisible, virtual, and undrawable nodes */
	if (node->invis || UI_Node_IsVirtual(node))
		return;
	/* if construct */
	if (!UI_CheckVisibility(node))
		return;

	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);

	/** @todo remove it when its possible:
	 * we can create a 'box' node with this properties,
	 * but we often don't need it */
	/* check node size x and y value to check whether they are zero */
	if (Vector2NotEmpty(node->box.size)) {
		if (node->bgcolor[3] != 0)
			UI_DrawFill(pos[0], pos[1], node->box.size[0], node->box.size[1], node->bgcolor);

		if (node->border && node->bordercolor[3] != 0) {
			UI_DrawRect(pos[0], pos[1], node->box.size[0], node->box.size[1],
				node->bordercolor, node->border, 0xFFFF);
		}
	}

	/* draw the node */
	if (UI_Node_IsDrawable(node)) {
		UI_Node_Draw(node);
	}

	/* draw all child */
	if (!UI_Node_IsDrawItselfChild(node) && node->firstChild) {
		static int globalTransX = 0;
		static int globalTransY = 0;
		bool hasClient = false;
		vec2_t clientPosition;
		if (UI_Node_IsScrollableContainer(node)) {
			UI_Node_GetClientPosition(node, clientPosition);
			hasClient = true;
		}

		UI_PushClipRect(pos[0] + globalTransX, pos[1] + globalTransY, node->box.size[0], node->box.size[1]);

		/** @todo move it at the right position */
		if (hasClient) {
			vec3_t trans;
			globalTransX += clientPosition[0];
			globalTransY += clientPosition[1];
			trans[0] = clientPosition[0] * viddef.rx;
			trans[1] = clientPosition[1] * viddef.ry;
			trans[2] = 0;
			R_Transform(trans, nullptr, nullptr);
		}

		/** @todo we can skip nodes outside visible rect */
		for (uiNode_t* child = node->firstChild; child; child = child->next)
			UI_DrawNode(child);

		/** @todo move it at the right position */
		if (hasClient) {
			vec3_t trans;
			globalTransX -= clientPosition[0];
			globalTransY -= clientPosition[1];
			trans[0] = -clientPosition[0] * viddef.rx;
			trans[1] = -clientPosition[1] * viddef.ry;
			trans[2] = 0;
			R_Transform(trans, nullptr, nullptr);
		}

		UI_PopClipRect();
	}

	for (uiNode_t* iter = node->firstChild; iter; ) {
		uiNode_t* child = iter;
		iter = iter->next;
		if (child->deleteTime > 0 && child->deleteTime < CL_Milliseconds()) {
			UI_DeleteNode(child);
		}
	}
}

/**
 * @brief Generic notice function that renders a message to the screen
 * @todo Move it into Window node, and/or convert it in a reserved named string node and update the protocol
 */
static void UI_DrawNotice (void)
{
	const vec4_t noticeBG = { 1.0f, 0.0f, 0.0f, 0.2f };
	const vec4_t noticeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	int height = 0, width = 0;
	const int maxWidth = 500;
	const char* font = "f_normal";
	int lines = 5;
	int dx; /**< Delta-x position. Relative to original x position. */
	int x, y;
	vec_t* noticePosition;

	noticePosition = UI_WindowNodeGetNoticePosition(noticeWindow);
	if (noticePosition) {
		x = noticePosition[0];
		y = noticePosition[1];
	} else {
		x = VID_NORM_WIDTH / 2;
		y = 110;
	}

	/* relative to the window */
	x += noticeWindow->box.pos[0];
	y += noticeWindow->box.pos[1];

	R_FontTextSize(font, noticeText, maxWidth, LONGLINES_WRAP, &width, &height, nullptr, nullptr);

	if (!width)
		return;

	if (x + width + 3 > viddef.virtualWidth)
		dx = -(width + 10);
	else
		dx = 0;

	UI_DrawFill((x - 2 + dx) - ((width + 2) / 2), (y - 2) - ((height + 2) / 2), width + 4, height + 4, noticeBG);
	R_Color(noticeColor);
	UI_DrawString(font, ALIGN_CC, x + 1 + dx, y + 1, x + 1, maxWidth, 0, noticeText, lines);
	R_Color(nullptr);
}

/**
 * @brief Draws the window stack
 * @sa SCR_UpdateScreen
 */
void UI_Draw (void)
{
	UI_HandleTimers();

	assert(ui_global.windowStackPos >= 0);

	const bool mouseMoved = UI_CheckMouseMove();
	uiNode_t* hoveredNode = UI_GetHoveredNode();

	/* handle delay time for tooltips */
	if (mouseMoved && tooltipVisible) {
		UI_TimerStop(tooltipTimer);
		tooltipVisible = false;
	} else if (!tooltipVisible && !mouseMoved && !tooltipTimer->isRunning && ui_show_tooltips->integer && hoveredNode) {
		UI_TimerStart(tooltipTimer);
	}

	/* under a fullscreen, windows should not be visible */
	int pos = UI_GetLastFullScreenWindow();
	if (pos < 0)
		return;

	/* draw all visible windows */
	for (; pos < ui_global.windowStackPos; pos++) {
		uiNode_t* window;
		window = ui_global.windowStack[pos];

		drawOverNode = nullptr;

		UI_DrawNode(window);

		/* draw a node over the window */
		if (drawOverNode) {
			UI_Node_DrawOverWindow(drawOverNode);
		}
	}

	/* draw a special notice */
	if (noticeWindow != nullptr && CL_Milliseconds() < noticeTime)
		UI_DrawNotice();

	/* unactive notice */
	if (noticeWindow != nullptr && CL_Milliseconds() >= noticeTime)
		noticeWindow = nullptr;

	/* draw tooltip */
	if (hoveredNode && tooltipVisible && !UI_DNDIsDragging()) {
		UI_Node_DrawTooltip(hoveredNode, mousePosX, mousePosY);
	}

#ifdef DEBUG
	/* debug information */
	if (UI_DebugMode() >= 1) {
		UI_DrawDebugNodeNames();
	}
#endif
}

void UI_DrawCursor (void)
{
	UI_DrawDragAndDrop(mousePosX, mousePosY);
}

/**
 * @brief Displays a message over all windows.
 * @sa HUD_DisplayMessage
 * @param[in] time is a ms values
 * @param[in] text text is already translated here
 * @param[in] windowName Window name where we must display the notice; else nullptr to use the current active window
 */
void UI_DisplayNotice (const char* text, int time, const char* windowName)
{
	noticeTime = CL_Milliseconds() + time;
	Q_strncpyz(noticeText, text, sizeof(noticeText));

	if (windowName == nullptr) {
		noticeWindow = UI_GetActiveWindow();
		if (noticeWindow == nullptr)
			Com_Printf("UI_DisplayNotice: No active window\n");
	} else {
		noticeWindow = UI_GetWindow(windowName);
		if (noticeWindow == nullptr)
			Com_Printf("UI_DisplayNotice: '%s' not found\n", windowName);
	}
}

void UI_InitDraw (void)
{
	ui_show_tooltips = Cvar_Get("ui_show_tooltips", "1", CVAR_ARCHIVE, "Show tooltips in the UI");
	tooltipTimer = UI_AllocTimer(nullptr, TOOLTIP_DELAY, UI_CheckTooltipDelay);
}
