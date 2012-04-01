/**
 * @file
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
#include "../ui_behaviour.h"
#include "../ui_main.h"
#include "../ui_input.h"
#include "../ui_timer.h"
#include "../ui_actions.h"
#include "../ui_render.h"
#include "ui_node_spinner.h"
#include "ui_node_abstractnode.h"

#include "../../input/cl_input.h"
#include "../../input/cl_keys.h"

#define EXTRADATA_TYPE abstractValueExtraData_t;
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static const uiBehaviour_t *localBehaviour;

static const int TILE_SIZE = 32;
static const int SPINNER_WIDTH = 15;
static const int SPINNER_HEIGHT = 19;
static const int BUTTON_TOP_SIZE = 9;
static const int BUTTON_BOTTOM_SIZE = 10;

static qboolean capturedDownButton;
static uiTimer_t *capturedTimer = NULL;

/**
 * @brief change the value of the spinner of one step
 * @param[in] node Spinner to change
 * @param[in] down Direction of the step (if down is true, decrease the value)
 */
bool uiSpinnerNode::step (uiNode_t *node, bool down)
{
	if (!down) {
		return incValue(node);
	} else {
		return decValue(node);
	}
}

void uiSpinnerNode::repeat (uiNode_t *node, uiTimer_t *timer)
{
	step(node, capturedDownButton);
	switch (timer->calledTime) {
	case 1:
		timer->delay = 50;
		break;
	}
}

static void UI_SpinnerNodeRepeat (uiNode_t *node, uiTimer_t *timer)
{
	uiSpinnerNode* b = dynamic_cast<uiSpinnerNode*>(node->behaviour->manager);
	b->repeat(node, timer);
}

void uiSpinnerNode::mouseDown (uiNode_t *node, int x, int y, int button)
{
	const qboolean disabled = node->disabled || node->parent->disabled;
	if (disabled)
		return;

	if (button == K_MOUSE1) {
		UI_SetMouseCapture(node);
		UI_NodeAbsoluteToRelativePos(node, &x, &y);
		capturedDownButton = y > (node->size[1] * 0.5);
		step(node, capturedDownButton);
		capturedTimer = UI_AllocTimer(node, 500, UI_SpinnerNodeRepeat);
		UI_TimerStart(capturedTimer);
	}
}

void uiSpinnerNode::mouseUp (uiNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1 && UI_GetMouseCapture() == node) {
		UI_MouseRelease();
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
void uiSpinnerNode::capturedMouseLost (uiNode_t *node)
{
	if (capturedTimer) {
		UI_TimerRelease(capturedTimer);
		capturedTimer = NULL;
	}
}

/**
 * @note Mouse wheel is not inhibited when node is disabled
 */
bool uiSpinnerNode::scroll (uiNode_t *node, int deltaX, int deltaY)
{
	bool down = deltaY > 0;
	const bool disabled = node->disabled || node->parent->disabled;
	if (deltaY == 0)
		return false;
	if (disabled)
		return false;
	return step(node, down);
}

void uiSpinnerNode::draw (uiNode_t *node)
{
	vec2_t pos;
	int topTexX, topTexY;
	int bottomTexX, bottomTexY;
	const char* image = UI_GetReferenceString(node, node->image);
	const float delta = getDelta(node);
	const qboolean disabled = node->disabled || node->parent->disabled;

	if (!image)
		return;

	UI_GetNodeAbsPos(node, pos);

	if (disabled || delta == 0) {
		topTexX = TILE_SIZE;
		topTexY = TILE_SIZE;
		bottomTexX = TILE_SIZE;
		bottomTexY = TILE_SIZE;
	} else {
		const float value = getValue(node);
		const float min = getMin(node);
		const float max = getMax(node);

		/* top button status */
		if (value >= max) {
			topTexX = TILE_SIZE;
			topTexY = TILE_SIZE;
		} else if (node->state && mousePosY < pos[1] + node->size[1] * 0.5) {
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
		} else if (node->state && mousePosY > pos[1] + node->size[1] * 0.5) {
			bottomTexX = TILE_SIZE;
			bottomTexY = 0;
		} else {
			bottomTexX = 0;
			bottomTexY = 0;
		}
	}

	/* center the spinner */
	pos[0] += (node->size[0] - SPINNER_WIDTH) * 0.5;
	pos[1] += (node->size[1] - SPINNER_HEIGHT) * 0.5;

	/* draw top button */
	UI_DrawNormImageByName(qfalse, pos[0], pos[1], SPINNER_WIDTH, BUTTON_TOP_SIZE,
		topTexX + SPINNER_WIDTH, topTexY + BUTTON_TOP_SIZE, topTexX, topTexY, image);
	/* draw bottom button */
	UI_DrawNormImageByName(qfalse, pos[0], pos[1] + BUTTON_TOP_SIZE, SPINNER_WIDTH, BUTTON_BOTTOM_SIZE,
		bottomTexX + SPINNER_WIDTH, bottomTexY + SPINNER_HEIGHT, bottomTexX, bottomTexY + SPINNER_HEIGHT - BUTTON_BOTTOM_SIZE, image);
}

void uiSpinnerNode::loading (uiNode_t *node)
{
	uiAbstractValueNode::loading(node);
	node->size[0] = SPINNER_WIDTH;
	node->size[1] = SPINNER_HEIGHT;
}

void UI_RegisterSpinnerNode (uiBehaviour_t *behaviour)
{
	localBehaviour = behaviour;
	behaviour->name = "spinner";
	behaviour->extends = "abstractvalue";
	behaviour->manager = new uiSpinnerNode();

	/* The size of the widget is uneditable. Fixed to 15x19.
	 */
	UI_RegisterOveridedNodeProperty(behaviour, "size");

	/* Texture used for the widget. Its a 64x64 template image with all four
	 * status. The top button take the first vertical 9 pixels, the bottom
	 * button use the last 10 pixels. See the sample image.
	 * @image html http://ufoai.ninex.info/wiki/images/Spinner_blue.png
	 */
	UI_RegisterOveridedNodeProperty(behaviour, "image");
}
