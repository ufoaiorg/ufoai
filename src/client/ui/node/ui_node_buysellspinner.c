/**
 * @file ui_node_spinner.c
 * @brief The spinner node is a vertical widget used to change a value.
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_main.h"
#include "../ui_input.h"
#include "../ui_timer.h"
#include "../ui_actions.h"
#include "../ui_render.h"
#include "ui_node_spinner.h"
#include "ui_node_abstractvalue.h"
#include "ui_node_abstractnode.h"

#include "../../input/cl_input.h"
#include "../../input/cl_keys.h"

#define EXTRADATA(node) UI_EXTRADATA(node, abstractValueExtraData_t)

static const uiBehaviour_t const *localBehaviour;

static const int TILE_SIZE = 32;
static const int SPINNER_WIDTH = 15;
static const int SPINNER_HEIGHT = 19;
static const int BUTTON_TOP_SIZE = 9;
static const int BUTTON_BOTTOM_SIZE = 10;

/**
 * @brief change the value of the spinner of one step
 * @param[in] node Spinner to change
 * @param[in] down Direction of the step (if down is true, decrease the value)
 */
static void UI_SpinnerNodeStep (uiNode_t *node, qboolean down)
{
	float value = UI_GetReferenceFloat(node, EXTRADATA(node).value);
	const float last = value;
	const float delta = UI_GetReferenceFloat(node, EXTRADATA(node).delta);
	const float max = UI_GetReferenceFloat(node, EXTRADATA(node).max);
	const float min = UI_GetReferenceFloat(node, EXTRADATA(node).min);

	if (!down) {
		if (value + delta <= max) {
			value += delta;
		} else {
			value = max;
		}
	} else {
		if (value - delta >= min) {
			value -= delta;
		} else {
			value = min;
		}
	}

	/* ensure sane values */
	if (value < min)
		value = min;
	else if (value > max)
		value = max;

	/* nothing change? */
	if (last == value)
		return;

	/* save result */
	EXTRADATA(node).lastdiff = value - last;
	if (Q_strstart((const char *)EXTRADATA(node).value, "*cvar:"))
		Cvar_SetValue(&((const char*)EXTRADATA(node).value)[6], value);
	else
		*(float*) EXTRADATA(node).value = value;

	/* fire change event */
	if (node->onChange)
		UI_ExecuteEventActions(node, node->onChange);
}

static qboolean capturedDownButton;
static uiTimer_t *capturedTimer = NULL;

static void UI_SpinnerNodeRepeat (uiNode_t *node, uiTimer_t *timer)
{
	UI_SpinnerNodeStep(node, capturedDownButton);
	switch (timer->calledTime) {
	case 1:
		timer->delay = 50;
		break;
	}
}

static void UI_SpinnerNodeDown (uiNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1) {
		UI_SetMouseCapture(node);
		UI_NodeAbsoluteToRelativePos(node, &x, &y);
		capturedDownButton = y > BUTTON_TOP_SIZE;
		if( x >= node->size[0] - SPINNER_WIDTH ) {
			UI_SpinnerNodeStep(node, capturedDownButton);
		}
		capturedTimer = UI_AllocTimer(node, 500, UI_SpinnerNodeRepeat);
		UI_TimerStart (capturedTimer);
	}
}

static void UI_SpinnerNodeUp (uiNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1) {
		UI_MouseRelease();
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
static void UI_SpinnerNodeCapturedMouseLost (uiNode_t *node)
{
	if (capturedTimer) {
		UI_TimerRelease(capturedTimer);
		capturedTimer = NULL;
	}
}

/**
 * @note Mouse wheel is not inhibited when node is disabled
 */
static void UI_SpinnerNodeWheel (uiNode_t *node, qboolean down, int x, int y)
{
	const qboolean disabled = node->disabled || node->parent->disabled;
	if (disabled)
		return;
	UI_SpinnerNodeStep(node, down);
}

static void UI_SpinnerNodeDraw (uiNode_t *node)
{
	vec2_t pos;
	int topTexX, topTexY;
	int bottomTexX, bottomTexY;
	const char* image = UI_GetReferenceString(node, node->image);
	const float delta = UI_GetReferenceFloat(node, EXTRADATA(node).delta);
	const qboolean disabled = node->disabled || node->parent->disabled;
	int offset = node->size[0] - SPINNER_WIDTH;

	if (!image)
		return;

	UI_GetNodeAbsPos(node, pos);

	if (disabled || delta == 0) {
		topTexX = TILE_SIZE;
		topTexY = TILE_SIZE;
		bottomTexX = TILE_SIZE;
		bottomTexY = TILE_SIZE;
	} else {
		const float value = UI_GetReferenceFloat(node, EXTRADATA(node).value);
		const float min = UI_GetReferenceFloat(node, EXTRADATA(node).min);
		const float max = UI_GetReferenceFloat(node, EXTRADATA(node).max);

		/* top button status */
		if (value >= max) {
			topTexX = TILE_SIZE;
			topTexY = TILE_SIZE;
		} else if (node->state && mousePosY < pos[1] + BUTTON_TOP_SIZE) {
			topTexX = TILE_SIZE;
			topTexY = 0;
		} else {
			topTexX = 0;
			topTexY = 0;
		}
		/* bottom button status */
		if (value <= min) {
			bottomTexX = TILE_SIZE;
			bottomTexY = TILE_SIZE;
		} else if (node->state && mousePosY > pos[1] + BUTTON_TOP_SIZE) {
			bottomTexX = TILE_SIZE;
			bottomTexY = 0;
		} else {
			bottomTexX = 0;
			bottomTexY = 0;
		}
	}

	/* draw top button */
	UI_DrawNormImageByName(pos[0] + offset, pos[1], SPINNER_WIDTH, BUTTON_TOP_SIZE,
		topTexX + SPINNER_WIDTH, topTexY + BUTTON_TOP_SIZE, topTexX, topTexY, image);
	/* draw bottom button */
	UI_DrawNormImageByName(pos[0] + offset, pos[1] + BUTTON_TOP_SIZE, SPINNER_WIDTH, BUTTON_BOTTOM_SIZE,
		bottomTexX + SPINNER_WIDTH, bottomTexY + SPINNER_HEIGHT, bottomTexX, bottomTexY + SPINNER_HEIGHT - BUTTON_BOTTOM_SIZE, image);
}

static void UI_SpinnerNodeLoaded (uiNode_t *node)
{
	localBehaviour->super->loaded(node);
	/* we can't choose a size */
	/*
	node->size[0] = SPINNER_WIDTH;
	node->size[1] = SPINNER_HEIGHT;
	*/
}

/**
 * @note Required for Android, to buy/sell with volume up/down buttons
 */
static qboolean UI_SpinnerNodeKeyPressed (uiNode_t *node, unsigned int key, unsigned short unicode)
{
	const qboolean disabled = node->disabled || node->parent->disabled;
	if (disabled)
		return qfalse;
	if( key == K_UPARROW || key == K_DOWNARROW || key == K_LEFTARROW || key == K_RIGHTARROW ) {
		UI_SpinnerNodeStep(node, (key == K_UPARROW || key == K_RIGHTARROW));
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief force focus on a spinner
 */
static void UI_SpinnerMouseMove(uiNode_t *node, int x, int y)
{
	if (!UI_HasFocus(node)) {
		UI_RequestFocus(node);
	}
}

static const value_t properties[] = {
	/* @override size
	 * The size of the widget is uneditable. Fixed to 15x19.
	 */
	/* @override image
	 * Texture used for the widget. Its a 64x64 template image with all four
	 * status. The top button take the first vertical 9 pixels, the bottom
	 * button use the last 10 pixels. See the sample image.
	 * @image html http://ufoai.ninex.info/wiki/images/Spinner_blue.png
	 */

	/* Call it to force node edition */

	{NULL, V_NULL, 0, 0}
};

void UI_RegisterBuySellSpinnerNode (uiBehaviour_t *behaviour)
{
	localBehaviour = behaviour;
	behaviour->name = "buysellspinner";
	behaviour->extends = "abstractvalue";
	behaviour->mouseWheel = UI_SpinnerNodeWheel;
	behaviour->mouseDown = UI_SpinnerNodeDown;
	behaviour->mouseUp = UI_SpinnerNodeUp;
	behaviour->capturedMouseLost = UI_SpinnerNodeCapturedMouseLost;
	behaviour->draw = UI_SpinnerNodeDraw;
	behaviour->loaded = UI_SpinnerNodeLoaded;
	behaviour->properties = properties;
	behaviour->keyPressed = UI_SpinnerNodeKeyPressed;
	behaviour->mouseMove = UI_SpinnerMouseMove;
}
