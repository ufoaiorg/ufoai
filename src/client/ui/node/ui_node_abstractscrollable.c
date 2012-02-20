/**
 * @file ui_node_abstractscrollable.c
 * @todo use this interface into every scrollable node
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

#include "../ui_main.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_font.h"
#include "../ui_render.h"
#include "../ui_actions.h"
#include "ui_node_abstractnode.h"
#include "ui_node_abstractscrollable.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA_TYPE abstractScrollableExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

/**
 * @brief return true if the node size change and update the cache
 */
qboolean UI_AbstractScrollableNodeIsSizeChange (uiNode_t *node)
{
	assert(UI_NodeInstanceOf(node, "abstractscrollable"));

	if (!Vector2Equal(node->size, EXTRADATA(node).cacheSize)) {
		Vector2Copy(node->size, EXTRADATA(node).cacheSize);
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief Set the scroll to a position
 * @param[in] scroll scroll to edit
 * @param[in] viewPos New position to set, else -1 if no change
 * @param[in] viewSize New view size to set, else -1 if no change
 * @param[in] fullSize New full size to set, else -1 if no change
 * @return True, if something have change
 */
qboolean UI_SetScroll (uiScroll_t *scroll, int viewPos, int viewSize, int fullSize)
{
	qboolean updated = qfalse;

	/* default values */
	if (viewPos == -1)
		viewPos = scroll->viewPos;
	if (viewSize == -1)
		viewSize = scroll->viewSize;
	if (fullSize == -1)
		fullSize = scroll->fullSize;

	/* fix limits */
	if (viewSize < 0)
		viewSize = 0;
	if (fullSize < 0)
		fullSize = 0;
	if (viewPos < 0)
		viewPos = 0;

	/* clap position */
	if (viewSize >= fullSize)
		viewPos = 0;
	else if (viewPos > fullSize - viewSize)
		viewPos = fullSize - viewSize;

	/* update */
	if (scroll->viewPos != viewPos) {
		scroll->viewPos = viewPos;
		updated = qtrue;
	}
	if (scroll->viewSize != viewSize) {
		scroll->viewSize = viewSize;
		updated = qtrue;
	}
	if (scroll->fullSize != fullSize) {
		scroll->fullSize = fullSize;
		updated = qtrue;
	}

	return updated;
}

/**
 * @brief Set the Y scroll to a position, and call event if need
 * @param[in] node Context node
 * @param[in] viewPos New position to set, else -1 if no change
 * @param[in] viewSize New view size to set, else -1 if no change
 * @param[in] fullSize New full size to set, else -1 if no change
 * @return True, if something have change
 */
qboolean UI_AbstractScrollableNodeSetY (uiNode_t *node, int viewPos, int viewSize, int fullSize)
{
	qboolean updated;
	assert(UI_NodeInstanceOf(node, "abstractscrollable"));

	updated = UI_SetScroll(&EXTRADATA(node).scrollY, viewPos, viewSize, fullSize);

	if (updated && EXTRADATA(node).onViewChange)
		UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);

	return updated;
}

/**
 * @note pos == -1 is a reserved value, that why we clamp the value
 */
static void UI_AbstractScrollableNodePageUp (uiNode_t *node, const uiCallContext_t *context) {
	const int pos = EXTRADATA(node).scrollY.viewPos - 10;
	UI_AbstractScrollableNodeSetY(node, (pos >= 0)?pos:0, -1, -1);
}

static void UI_AbstractScrollableNodePageDown (uiNode_t *node, const uiCallContext_t *context) {
	UI_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.viewPos + 10, -1, -1);
}

static void UI_AbstractScrollableNodeMoveUp (uiNode_t *node, const uiCallContext_t *context) {
	UI_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.viewPos - 1, -1, -1);
}

static void UI_AbstractScrollableNodeMoveDown (uiNode_t *node, const uiCallContext_t *context) {
	UI_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.viewPos + 1, -1, -1);
}

static void UI_AbstractScrollableNodeMoveHome (uiNode_t *node, const uiCallContext_t *context) {
	UI_AbstractScrollableNodeSetY(node, 0, -1, -1);
}

/**
 * @note fullSize is bigger than the "end" position. But the function will clamp it right
 */
static void UI_AbstractScrollableNodeMoveEnd (uiNode_t *node, const uiCallContext_t *context) {
	UI_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.fullSize, -1, -1);
}

/**
 * @brief Scroll the Y scroll with a relative position, and call event if need
 * @return True, if something have change
 */
qboolean UI_AbstractScrollableNodeScrollY (uiNode_t *node, int offset)
{
	assert(UI_NodeInstanceOf(node, "abstractscrollable"));
	return UI_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.viewPos + offset, -1, -1);
}

/**
 * @brief Return size of the cell, which is the size (in virtual "pixel") which represente 1 in the scroll values.
 * Here expect the widget can scroll pixel per pixel.
 * @return Size in pixel.
 */
static int UI_AbstractScrollableGetCellSize (uiNode_t *node)
{
	return 1;
}

void UI_RegisterAbstractScrollableNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "abstractscrollable";
	behaviour->isAbstract = qtrue;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->getCellWidth = UI_AbstractScrollableGetCellSize;
	behaviour->getCellHeight = UI_AbstractScrollableGetCellSize;


	/* position of the vertical view (into the full number of elements the node contain) */
	UI_RegisterExtradataNodeProperty(behaviour, "viewpos", V_INT, EXTRADATA_TYPE, scrollY.viewPos);
	/* size of the vertical view (proportional to the number of elements the node can display without moving) */
	UI_RegisterExtradataNodeProperty(behaviour, "viewsize", V_INT, EXTRADATA_TYPE, scrollY.viewSize);
	/* full vertical size (proportional to the number of elements the node contain) */
	UI_RegisterExtradataNodeProperty(behaviour, "fullsize", V_INT, EXTRADATA_TYPE, scrollY.fullSize);
	/* Called when one of the properties viewpos/viewsize/fullsize change */
	UI_RegisterExtradataNodeProperty(behaviour, "onviewchange", V_UI_ACTION, EXTRADATA_TYPE, onViewChange);

	/* Call it to vertically scroll the document up */
	UI_RegisterNodeMethod(behaviour, "pageup", UI_AbstractScrollableNodePageUp);
	/* Call it to vertically scroll the document down */
	UI_RegisterNodeMethod(behaviour, "pagedown", UI_AbstractScrollableNodePageDown);
	/* Call it to vertically scroll the document up */
	UI_RegisterNodeMethod(behaviour, "moveup", UI_AbstractScrollableNodeMoveUp);
	/* Call it to vertically scroll the document down */
	UI_RegisterNodeMethod(behaviour, "movedown", UI_AbstractScrollableNodeMoveDown);
	/* Call it to vertically reset the scroll position to 0 */
	UI_RegisterNodeMethod(behaviour, "movehome", UI_AbstractScrollableNodeMoveHome);
	/* Call it to vertically move the scroll to the end of the document */
	UI_RegisterNodeMethod(behaviour, "moveend", UI_AbstractScrollableNodeMoveEnd);
}
