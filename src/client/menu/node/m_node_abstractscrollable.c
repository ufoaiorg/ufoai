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
#include "m_node_abstractnode.h"
#include "m_node_abstractscrollable.h"

#include "../../client.h" /* gettext _() */

/**
 * @brief return true if the node size change and update the cache
 */
qboolean MN_AbstractScrollableNodeIsSizeChange (menuNode_t *node)
{
	assert(MN_NodeInstanceOf(node, "abstractscrollable"));

	if (node->size[0] != node->u.abstractscrollable.cacheSize[0]
		|| node->size[1] != node->u.abstractscrollable.cacheSize[1])
	{
		node->u.abstractscrollable.cacheSize[0] = node->size[0];
		node->u.abstractscrollable.cacheSize[1] = node->size[1];
		return qtrue;
	}
	return qfalse;
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
	qboolean updated = qfalse;
	assert(MN_NodeInstanceOf(node, "abstractscrollable"));

	/* default values */
	if (viewPos == -1)
		viewPos = node->u.abstractscrollable.viewPosY;
	if (viewSize == -1)
		viewSize = node->u.abstractscrollable.viewSizeY;
	if (fullSize == -1)
		fullSize = node->u.abstractscrollable.fullSizeY;

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
	if (node->u.abstractscrollable.viewPosY != viewPos) {
		node->u.abstractscrollable.viewPosY = viewPos;
		updated = qtrue;
	}
	if (node->u.abstractscrollable.viewSizeY != viewSize) {
		node->u.abstractscrollable.viewSizeY = viewSize;
		updated = qtrue;
	}
	if (node->u.abstractscrollable.fullSizeY != fullSize) {
		node->u.abstractscrollable.fullSizeY = fullSize;
		updated = qtrue;
	}

	if (updated && node->u.abstractscrollable.onViewChange)
		MN_ExecuteEventActions(node, node->u.abstractscrollable.onViewChange);

	return updated;
}

/**
 * @brief Scroll the Y scroll with a relative position, and call event if need
 * @return True, if something have change
 */
qboolean MN_AbstractScrollableNodeScrollY (menuNode_t *node, int offset)
{
	assert(MN_NodeInstanceOf(node, "abstractscrollable"));
	return MN_AbstractScrollableNodeSetY(node, node->u.abstractscrollable.viewPosY + offset, -1, -1);
}

static const value_t properties[] = {
	{"viewpos", V_INT, offsetof(menuNode_t, u.abstractscrollable.viewPosX),  MEMBER_SIZEOF(menuNode_t, u.abstractscrollable.viewPosX)},
	{"viewsize", V_INT, offsetof(menuNode_t, u.abstractscrollable.viewSizeX),  MEMBER_SIZEOF(menuNode_t, u.abstractscrollable.viewSizeX)},
	{"fullsize", V_INT, offsetof(menuNode_t, u.abstractscrollable.fullSizeX),  MEMBER_SIZEOF(menuNode_t, u.abstractscrollable.fullSizeX)},

	{"onviewchange", V_SPECIAL_ACTION, offsetof(menuNode_t, u.abstractscrollable.onViewChange), MEMBER_SIZEOF(menuNode_t, u.abstractscrollable.onViewChange)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractScrollableNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractscrollable";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
}
