/**
 * @file m_node_spinner.c
 * @todo manage autorepeate somewhere here
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

#include "m_nodes.h"
#include "m_node_spinner.h"
#include "m_node_abstractvalue.h"
#include "m_parse.h"
#include "m_input.h"
#include "../cl_input.h"

static const int TILE_SIZE = 32;
static const int SPINNER_WIDTH = 15;
static const int SPINNER_HEIGHT = 19;
static const int BUTTON_TOP_SIZE = 9;
static const int BUTTON_BOTTOM_SIZE = 10;

static void MN_SpinnerNodeClick (menuNode_t *node, int x, int y)
{
	float value = MN_GetReferenceFloat(node->menu, node->u.abstractvalue.value);
	const float last = value;
	const float delta = MN_GetReferenceFloat(node->menu, node->u.abstractvalue.delta);

	if (node->disabled)
		return;

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	if (y < BUTTON_TOP_SIZE) {
		const float max = MN_GetReferenceFloat(node->menu, node->u.abstractvalue.max);
		if (value + delta <= max) {
			value += delta;
		} else {
			value = max;
		}
	} else {
		const float min = MN_GetReferenceFloat(node->menu, node->u.abstractvalue.min);
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
	if (!Q_strncmp(node->u.abstractvalue.value, "*cvar", 5)) {
		MN_SetCvar(&((char*)node->u.abstractvalue.value)[6], NULL, value);
	} else {
		*(float*) node->u.abstractvalue.value = value;
	}
}

static void MN_SpinnerNodeDraw (menuNode_t *node)
{
	vec2_t pos;
	int topTexX, topTexY;
	int bottomTexX, bottomTexY;
	const char* image = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	const float delta = MN_GetReferenceFloat(node->menu, node->u.abstractvalue.delta);

	if (!image)
		return;

	MN_GetNodeAbsPos(node, pos);

	if (node->disabled || delta == 0) {
		topTexX = TILE_SIZE;
		topTexY = TILE_SIZE;
		bottomTexX = TILE_SIZE;
		bottomTexY = TILE_SIZE;
	} else {
		const float value = MN_GetReferenceFloat(node->menu, node->u.abstractvalue.value);
		const float min = MN_GetReferenceFloat(node->menu, node->u.abstractvalue.min);
		const float max = MN_GetReferenceFloat(node->menu, node->u.abstractvalue.max);

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
		} else if (node->state && mousePosY >= pos[1] + BUTTON_TOP_SIZE) {
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
	/* we can't choose a size */
	node->size[0] = SPINNER_WIDTH;
	node->size[1] = SPINNER_HEIGHT;
}

void MN_RegisterSpinnerNode (nodeBehaviour_t *behaviour)
{
	/* inheritance */
	MN_RegisterAbstractValueNode(behaviour);
	/* overwrite */
	behaviour->name = "spinner";
	behaviour->id = MN_SPINNER;
	behaviour->leftClick = MN_SpinnerNodeClick;
	behaviour->draw = MN_SpinnerNodeDraw;
	behaviour->loaded = MN_SpinnerNodeLoaded;
}
