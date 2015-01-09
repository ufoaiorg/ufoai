/**
 * @file
 * @brief The spinner node is a vertical widget used to change a value.
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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_main.h"
#include "../ui_input.h"
#include "../ui_timer.h"
#include "../ui_actions.h"
#include "../ui_render.h"
#include "../ui_sprite.h"
#include "ui_node_spinner.h"
#include "ui_node_abstractnode.h"

#include "../../input/cl_input.h"
#include "../../input/cl_keys.h"

#define EXTRADATA_TYPE spinnerExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

enum spinnerMode_t {
	/**
	 * Normal mode. The upper side of the node increase the value
	 * and the lower side of the node decrease the value
	 */
	NORMAL,
	/**
	 * Only increase mode. The whole node increase the value.
	 */
	ONLY_INCREASE,
	/**
	 * Only decrease mode. The whole node decrease the value.
	 */
	ONLY_DECREASE
};

static bool capturedDownButton;
static uiTimer_t* capturedTimer = nullptr;

/**
 * @brief change the value of the spinner of one step
 * @param[in] node Spinner to change
 * @param[in] down Direction of the step (if down is true, decrease the value)
 */
bool uiSpinnerNode::step (uiNode_t* node, bool down)
{
	if (!down)
		return incValue(node);
	return decValue(node);
}

void uiSpinnerNode::repeat (uiNode_t* node, uiTimer_t* timer)
{
	step(node, capturedDownButton);
	switch (timer->calledTime) {
	case 1:
		timer->delay = 50;
		break;
	}
}

static void UI_SpinnerNodeRepeat (uiNode_t* node, uiTimer_t* timer)
{
	uiSpinnerNode* b = dynamic_cast<uiSpinnerNode*>(node->behaviour->manager.get());
	b->repeat(node, timer);
}

/**
 * @brief Check a position relative to the node to check action
 * is can produce.
 * @param node This node
 * @param x,y Relative location to the node
 * @return True if the current location can increase the value
 */
bool uiSpinnerNode::isPositionIncrease(uiNode_t* node, int x, int y)
{
	switch ((spinnerMode_t)EXTRADATA(node).mode) {
	case ONLY_INCREASE:
		return true;
	case ONLY_DECREASE:
		return false;
	case NORMAL:
		if (EXTRADATA(node).horizontal)
			return x > node->box.size[0] * 0.5;
		return y < node->box.size[1] * 0.5;
	default:
		return false;
	}
}

void uiSpinnerNode::onMouseDown (uiNode_t* node, int x, int y, int button)
{
	const bool disabled = node->disabled || node->parent->disabled;
	if (disabled)
		return;

	if (button == K_MOUSE1) {
		UI_SetMouseCapture(node);
		UI_NodeAbsoluteToRelativePos(node, &x, &y);
		capturedDownButton = !isPositionIncrease(node, x, y);
		if (EXTRADATA(node).inverted)
			capturedDownButton = !capturedDownButton;
		step(node, capturedDownButton);
		capturedTimer = UI_AllocTimer(node, 500, UI_SpinnerNodeRepeat);
		UI_TimerStart(capturedTimer);
	}
}

void uiSpinnerNode::onMouseUp (uiNode_t* node, int x, int y, int button)
{
	if (button == K_MOUSE1 && UI_GetMouseCapture() == node) {
		UI_MouseRelease();
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
void uiSpinnerNode::onCapturedMouseLost (uiNode_t* node)
{
	if (capturedTimer) {
		UI_TimerRelease(capturedTimer);
		capturedTimer = nullptr;
	}
}

/**
 * @note Mouse wheel is not inhibited when node is disabled
 */
bool uiSpinnerNode::onScroll (uiNode_t* node, int deltaX, int deltaY)
{
	bool down = deltaY > 0;
	const bool disabled = node->disabled || node->parent->disabled;
	if (deltaY == 0)
		return false;
	if (disabled)
		return false;
	return step(node, down);
}

void uiSpinnerNode::draw (uiNode_t* node)
{
	vec2_t pos;
	const float delta = getDelta(node);
	const bool disabled = node->disabled || node->parent->disabled;

	UI_GetNodeAbsPos(node, pos);

	uiSpriteStatus_t status;
	uiSpriteStatus_t topStatus;
	uiSpriteStatus_t bottomStatus;

	if (disabled || delta == 0) {
		status = SPRITE_STATUS_DISABLED;
		topStatus = SPRITE_STATUS_DISABLED;
		bottomStatus = SPRITE_STATUS_DISABLED;
	} else {
		const float value = getValue(node);
		const float min = getMin(node);
		const float max = getMax(node);

		status = SPRITE_STATUS_NORMAL;

		bool increaseLocation = isPositionIncrease(node, mousePosX - pos[0], mousePosY - pos[1]);

		/* top button status */
		if (node->state && increaseLocation) {
			topStatus = SPRITE_STATUS_HOVER;
		} else {
			topStatus = SPRITE_STATUS_NORMAL;
		}
		/* bottom button status */
		if (node->state && !increaseLocation) {
			bottomStatus = SPRITE_STATUS_HOVER;
		} else {
			bottomStatus = SPRITE_STATUS_NORMAL;
		}

		if (value >= max) {
			if (EXTRADATA(node).inverted)
				bottomStatus = SPRITE_STATUS_DISABLED;
			else
				topStatus = SPRITE_STATUS_DISABLED;
		}
		if (value <= min) {
			if (EXTRADATA(node).inverted)
				topStatus = SPRITE_STATUS_DISABLED;
			else
				bottomStatus = SPRITE_STATUS_DISABLED;
		}
	}

	if (EXTRADATA(node).background)
		UI_DrawSpriteInBox(false, EXTRADATA(node).background, status, pos[0], pos[1], node->box.size[0], node->box.size[1]);
	if (!EXTRADATA(node).horizontal) {
		if (EXTRADATA(node).topIcon)
			UI_DrawSpriteInBox(false, EXTRADATA(node).topIcon, topStatus, pos[0], pos[1], node->box.size[0], node->box.size[1]);
		if (EXTRADATA(node).bottomIcon)
			UI_DrawSpriteInBox(false, EXTRADATA(node).bottomIcon, bottomStatus, pos[0], pos[1], node->box.size[0], node->box.size[1]);
	} else {
		if (EXTRADATA(node).topIcon) /* Top becomes right */
			UI_DrawSpriteInBox(false, EXTRADATA(node).topIcon, topStatus, pos[0] + node->box.size[0] / 2, pos[1], node->box.size[0] / 2, node->box.size[1]);
		if (EXTRADATA(node).bottomIcon) /* Bottom becomes left */
			UI_DrawSpriteInBox(false, EXTRADATA(node).bottomIcon, bottomStatus, pos[0], pos[1], node->box.size[0] / 2, node->box.size[1]);
	}
}

void uiSpinnerNode::onLoading (uiNode_t* node)
{
	uiAbstractValueNode::onLoading(node);
}

void UI_RegisterSpinnerNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "spinner";
	behaviour->extends = "abstractvalue";
	behaviour->manager = UINodePtr(new uiSpinnerNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/**
	 * @brief Background used to display the spinner. It is displayed in the center of the node.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);

	/**
	 * @brief Top icon used to decorate the top button of the spinner. It is displayed in the center of the node.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "topIcon", V_UI_SPRITEREF, EXTRADATA_TYPE, topIcon);

	/**
	 * @brief Sprite used to decorate the bottom button of the spinner. It is displayed in the center of the node.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "bottomIcon", V_UI_SPRITEREF, EXTRADATA_TYPE, bottomIcon);

	/**
	 * @brief Spinner mode allow to change the input action of the spinner.
	 * SPINNER_NORMAL is the default mode. With SPINNER_ONLY_INC anywhere it click on the node
	 * it will increase the value. With SPINNER_ONLY_DEC anywhere it click on the node
	 * it will decrease the value.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "mode", V_INT, EXTRADATA_TYPE, mode);

	/**
	 * @brief Draw images in horizontal orientation, also change active touch area for images.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "horizontal", V_BOOL, EXTRADATA_TYPE, horizontal);

	/**
	 * @brief Invert spinner directions, so that down/left will increase value, up/right will decrease it.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "inverted", V_BOOL, EXTRADATA_TYPE, inverted);

	Com_RegisterConstInt("SPINNER_NORMAL", NORMAL);
	Com_RegisterConstInt("SPINNER_ONLY_INC", ONLY_INCREASE);
	Com_RegisterConstInt("SPINNER_ONLY_DEC", ONLY_DECREASE);
}
