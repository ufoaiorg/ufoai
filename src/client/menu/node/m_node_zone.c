/**
 * @file m_node_special.c
 * @brief The <code>zone</code> node allow to create an hidden active node.
 * Currently we only use it to support repeat mouse actions without merging
 * the code which managing this feature.
 * @brief The very special zone called "render" is used to identify a rendering rectangle
 * for cinematics and map (battlescape). This part of the code should be removed.
 * @todo Find a way to remove the zone called "render". Create a node for the cinematics
 * and for the battlescape?
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
#include "m_node_zone.h"
#include "m_node_window.h"

#include "../../cl_keys.h"

#define EXTRADATA(node) node->u.zone

static menuTimer_t *capturedTimer;

static void MN_ZoneNodeRepeat (menuNode_t *node, menuTimer_t *timer)
{
	if (node->onClick) {
		MN_ExecuteEventActions(node, node->onClick);
	}
}

static void MN_ZoneNodeDown (menuNode_t *node, int x, int y, int button)
{
	if (!EXTRADATA(node).repeat)
		return;
	if (button == K_MOUSE1) {
		MN_SetMouseCapture(node);
		capturedTimer = MN_AllocTimer(node, EXTRADATA(node).clickDelay, MN_ZoneNodeRepeat);
		MN_TimerStart(capturedTimer);
	}
}

static void MN_ZoneNodeUp (menuNode_t *node, int x, int y, int button)
{
	if (!EXTRADATA(node).repeat)
		return;
	if (button == K_MOUSE1) {
		MN_MouseRelease();
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
static void MN_ZoneNodeCapturedMouseLost (menuNode_t *node)
{
	if (capturedTimer) {
		MN_TimerRelease(capturedTimer);
		capturedTimer = NULL;
	}
}

/**
 * @brief Call before the script initialized the node
 */
static void MN_ZoneNodeLoading (menuNode_t *node)
{
	EXTRADATA(node).clickDelay = 1000;
}

/**
 * @brief Call after the script initialized the node
 */
static void MN_ZoneNodeLoaded (menuNode_t *node)
{
	if (!strcmp(node->name, "render"))
		MN_WindowNodeSetRenderNode(node->root, node);
}

static const value_t properties[] = {
	/* If true, the <code>onclick</code> call back is called more than one time if the user do not release the button. */
	{"repeat", V_BOOL, MN_EXTRADATA_OFFSETOF(zoneExtraData_t, repeat), MEMBER_SIZEOF(zoneExtraData_t, repeat)},
	/* Delay it is used between 2 calls of <code>onclick</code>. */
	{"clickdelay", V_INT, MN_EXTRADATA_OFFSETOF(zoneExtraData_t, clickDelay), MEMBER_SIZEOF(zoneExtraData_t, clickDelay)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterZoneNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "zone";
	behaviour->loading = MN_ZoneNodeLoading;
	behaviour->loaded = MN_ZoneNodeLoaded;
	behaviour->mouseDown = MN_ZoneNodeDown;
	behaviour->mouseUp = MN_ZoneNodeUp;
	behaviour->capturedMouseLost = MN_ZoneNodeCapturedMouseLost;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(zoneExtraData_t);
}
