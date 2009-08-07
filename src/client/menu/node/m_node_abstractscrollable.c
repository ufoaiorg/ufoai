/**
 * @file m_node_abstractscrollable.c
 * @todo use this interface into every scrollable node
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

#include "../m_main.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_render.h"
#include "../m_actions.h"
#include "m_node_abstractnode.h"
#include "m_node_abstractscrollable.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA(node) (node->u.abstractscrollable)

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
 * @brief Scroll the Y scroll with a relative position, and call event if need
 * @return True, if something have change
 */
qboolean MN_AbstractScrollableNodeScrollY (menuNode_t *node, int offset)
{
	assert(MN_NodeInstanceOf(node, "abstractscrollable"));
	return MN_AbstractScrollableNodeSetY(node, EXTRADATA(node).scrollY.viewPos + offset, -1, -1);
}

static const value_t properties[] = {
	{"viewpos", V_INT, MN_EXTRADATA_OFFSETOF(abstractScrollableExtraData_t, scrollY.viewPos),  MEMBER_SIZEOF(abstractScrollableExtraData_t, scrollY.viewPos)},
	{"viewsize", V_INT, MN_EXTRADATA_OFFSETOF(abstractScrollableExtraData_t, scrollY.viewSize),  MEMBER_SIZEOF(abstractScrollableExtraData_t, scrollY.viewSize)},
	{"fullsize", V_INT, MN_EXTRADATA_OFFSETOF(abstractScrollableExtraData_t, scrollY.fullSize),  MEMBER_SIZEOF(abstractScrollableExtraData_t, scrollY.fullSize)},

	{"onviewchange", V_UI_ACTION, MN_EXTRADATA_OFFSETOF(abstractScrollableExtraData_t, onViewChange), MEMBER_SIZEOF(abstractScrollableExtraData_t, onViewChange)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractScrollableNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractscrollable";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(abstractScrollableExtraData_t);
}
