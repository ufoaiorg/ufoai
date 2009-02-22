/**
 * @file m_node_spinner.c
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

#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_input.h"
#include "../m_timer.h"
#include "../m_actions.h"
#include "m_node_spinner.h"
#include "m_node_abstractvalue.h"
#include "m_node_abstractnode.h"

#include "../../renderer/r_draw.h"
#include "../../cl_input.h"
#include "../../cl_keys.h"

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
static void MN_SpinnerNodeStep (menuNode_t *node, qboolean down)
{
	float value = MN_GetReferenceFloat(node, node->u.abstractvalue.value);
	const float last = value;
	const float delta = MN_GetReferenceFloat(node, node->u.abstractvalue.delta);

	if (!down) {
		const float max = MN_GetReferenceFloat(node, node->u.abstractvalue.max);
		if (value + delta <= max) {
			value += delta;
		} else {
			value = max;
		}
	} else {
		const float min = MN_GetReferenceFloat(node, node->u.abstractvalue.min);
		if (value - delta >= min) {
			value -= delta;
		} else {
			value = min;
		}
	}

	/* nothing change? */
	if (last == value)
		return;

	/* save result */
	node->u.abstractvalue.lastdiff = value - last;
	if (!Q_strncmp(node->u.abstractvalue.value, "*cvar", 5)) {
		MN_SetCvar(&((char*)node->u.abstractvalue.value)[6], NULL, value);
	} else {
		*(float*) node->u.abstractvalue.value = value;
	}

	/* fire change event */
	if (node->onChange) {
		MN_ExecuteEventActions(node, node->onChange);
	}
}

static qboolean capturedDownButton;
static menuTimer_t *capturedTimer = NULL;

static void MN_SpinnerNodeRepeat (menuNode_t *node, menuTimer_t *timer)
{
	MN_SpinnerNodeStep(node, capturedDownButton);
	switch (timer->calledTime) {
	case 1:
		timer->delay = 50;
		break;
	}
}

static void MN_SpinnerNodeDown (menuNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1) {
		MN_SetMouseCapture(node);
		MN_NodeAbsoluteToRelativePos(node, &x, &y);
		capturedDownButton = y > BUTTON_TOP_SIZE;
		MN_SpinnerNodeStep(node, capturedDownButton);
		capturedTimer = MN_AllocTimer(node, 500, MN_SpinnerNodeRepeat);
		MN_TimerStart (capturedTimer);
	}
}

static void MN_SpinnerNodeUp (menuNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1) {
		MN_MouseRelease();
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
static void MN_SpinnerNodeCapturedMouseLost (menuNode_t *node)
{
	if (capturedTimer) {
		MN_TimerRelease(capturedTimer);
		capturedTimer = NULL;
	}
}

static void MN_SpinnerNodeWheel (menuNode_t *node, qboolean down, int x, int y)
{
	MN_SpinnerNodeStep(node, down);
}

static void MN_SpinnerNodeDraw (menuNode_t *node)
{
	vec2_t pos;
	int topTexX, topTexY;
	int bottomTexX, bottomTexY;
	const char* image = MN_GetReferenceString(node, node->image);
	const float delta = MN_GetReferenceFloat(node, node->u.abstractvalue.delta);

	if (!image)
		return;

	MN_GetNodeAbsPos(node, pos);

	if (node->disabled || delta == 0) {
		topTexX = TILE_SIZE;
		topTexY = TILE_SIZE;
		bottomTexX = TILE_SIZE;
		bottomTexY = TILE_SIZE;
	} else {
		const float value = MN_GetReferenceFloat(node, node->u.abstractvalue.value);
		const float min = MN_GetReferenceFloat(node, node->u.abstractvalue.min);
		const float max = MN_GetReferenceFloat(node, node->u.abstractvalue.max);

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
	R_DrawNormPic(pos[0], pos[1], SPINNER_WIDTH, BUTTON_TOP_SIZE,
		topTexX + SPINNER_WIDTH, topTexY + BUTTON_TOP_SIZE, topTexX, topTexY, ALIGN_UL, node->blend, image);
	/* draw bottom button */
	R_DrawNormPic(pos[0], pos[1] + BUTTON_TOP_SIZE, SPINNER_WIDTH, BUTTON_BOTTOM_SIZE,
		bottomTexX + SPINNER_WIDTH, bottomTexY + SPINNER_HEIGHT, bottomTexX, bottomTexY + SPINNER_HEIGHT - BUTTON_BOTTOM_SIZE, ALIGN_UL, node->blend, image);
}

static void MN_SpinnerNodeLoaded (menuNode_t *node)
{
	node->behaviour->super->loaded(node);
	/* we can't choose a size */
	node->size[0] = SPINNER_WIDTH;
	node->size[1] = SPINNER_HEIGHT;
}

void MN_RegisterSpinnerNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "spinner";
	behaviour->extends = "abstractvalue";
	behaviour->mouseWheel = MN_SpinnerNodeWheel;
	behaviour->mouseDown = MN_SpinnerNodeDown;
	behaviour->mouseUp = MN_SpinnerNodeUp;
	behaviour->capturedMouseLost = MN_SpinnerNodeCapturedMouseLost;
	behaviour->draw = MN_SpinnerNodeDraw;
	behaviour->loaded = MN_SpinnerNodeLoaded;
}
