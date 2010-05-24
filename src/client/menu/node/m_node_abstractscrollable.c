/**
 * @file m_node_abstractscrollable.c
 * @todo use this interface into every scrollable node
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "../m_main.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_render.h"
#include "../m_actions.h"
#include "m_node_abstractnode.h"
#include "m_node_abstractscrollable.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA_TYPE abstractScrollableExtraData_t
#define EXTRADATA(node) MN_EXTRADATA(node, EXTRADATA_TYPE)

/**
 * @brief return true if the node size change and update the cache
 */
qboolean MN_AbstractScrollableNodeIsSizeChange (menuNode_t *node)
{
	assert(MN_NodeInstanceOf(node, "abstractscrollable"));

	if (node->size[0] != EXTRADATA(node).cacheSize[0]
		|| node->size[1] != EXTRADATA(node).cacheSize[1])
	{
		EXTRADATA(node).cacheSize[0] = node->size[0];
		EXTRADATA(node).cacheSize[1] = node->size[1];
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
qboolean MN_SetScroll (menuScroll_t *scroll, int viewPos, int viewSize, int fullSize)
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
qboolean MN_AbstractScrollableNodeSetY (menuNode_t *node, int viewPos, int viewSize, int fullSize)
{
	qboolean updated;
	assert(MN_NodeInstanceOf(node, "abstractscrollable"));

	updated = MN_SetScroll(&EXTRADATA(node).scrollY, viewPos, viewSize, fullSize);

	if (updated && EXTRADATA(node).onViewChange)
		MN_ExecuteEventActions(node, EXTRADATA(node).onViewChange);

	return updated;
}

/**
 * @note pos == -1 is a reserved value, that why we clamp the value
 */
static void MN_AbstractScrollableNodePageUp (menuNode_t *node, const menuCallContext_t *context) {
	const int pos = EXTRADATA(node).scrollY.viewPos - 10;
	MN_AbstractScrollableNodeSetY(node, (pos >= 0)?pos:0, -1, -1);
}

static void MN_AbstractScrollableNodePageDown (menuNode_t *node, const menuCallContext_t *context) {
	MN_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.viewPos + 10, -1, -1);
}

static void MN_AbstractScrollableNodeMoveUp (menuNode_t *node, const menuCallContext_t *context) {
	MN_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.viewPos - 1, -1, -1);
}

static void MN_AbstractScrollableNodeMoveDown (menuNode_t *node, const menuCallContext_t *context) {
	MN_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.viewPos + 1, -1, -1);
}

static void MN_AbstractScrollableNodeMoveHome (menuNode_t *node, const menuCallContext_t *context) {
	MN_AbstractScrollableNodeSetY(node, 0, -1, -1);
}

/**
 * @note fullSize is bigger than the "end" position. But the function will clamp it right
 */
static void MN_AbstractScrollableNodeMoveEnd (menuNode_t *node, const menuCallContext_t *context) {
	MN_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.fullSize, -1, -1);
}

/**
 * @brief Scroll the Y scroll with a relative position, and call event if need
 * @return True, if something have change
 */
qboolean MN_AbstractScrollableNodeScrollY (menuNode_t *node, int offset)
{
	assert(MN_NodeInstanceOf(node, "abstractscrollable"));
	return MN_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.viewPos + offset, -1, -1);
}

static const value_t properties[] = {
	/* position of the vertical view (into the full number of elements the node contain) */
	{"viewpos", V_INT, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, scrollY.viewPos),  MEMBER_SIZEOF(EXTRADATA_TYPE, scrollY.viewPos)},
	/* size of the vertical view (proportional to the number of elements the node can display without moving) */
	{"viewsize", V_INT, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, scrollY.viewSize),  MEMBER_SIZEOF(EXTRADATA_TYPE, scrollY.viewSize)},
	/* full vertical size (proportional to the number of elements the node contain) */
	{"fullsize", V_INT, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, scrollY.fullSize),  MEMBER_SIZEOF(EXTRADATA_TYPE, scrollY.fullSize)},
	/* Called when one of the properties viewpos/viewsize/fullsize change */
	{"onviewchange", V_UI_ACTION, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, onViewChange), MEMBER_SIZEOF(EXTRADATA_TYPE, onViewChange)},

	/* Call it to vertically scroll the document up */
	{"pageup", V_UI_NODEMETHOD, ((size_t) MN_AbstractScrollableNodePageUp), 0},
	/* Call it to vertically scroll the document down */
	{"pagedown", V_UI_NODEMETHOD, ((size_t) MN_AbstractScrollableNodePageDown), 0},
	/* Call it to vertically scroll the document up */
	{"moveup", V_UI_NODEMETHOD, ((size_t) MN_AbstractScrollableNodeMoveUp), 0},
	/* Call it to vertically scroll the document down */
	{"movedown", V_UI_NODEMETHOD, ((size_t) MN_AbstractScrollableNodeMoveDown), 0},
	/* Call it to vertically reset the scroll position to 0 */
	{"movehome", V_UI_NODEMETHOD, ((size_t) MN_AbstractScrollableNodeMoveHome), 0},
	/* Call it to vertically move the scroll to the end of the document */
	{"moveend", V_UI_NODEMETHOD, ((size_t) MN_AbstractScrollableNodeMoveEnd), 0},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractScrollableNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractscrollable";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
