/**
 * @file m_node_abstractoption.h
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

#ifndef CLIENT_MENU_M_NODE_ABSTRACTOPTION_H
#define CLIENT_MENU_M_NODE_ABSTRACTOPTION_H

#include "../../../shared/shared.h"
#include "m_node_abstractscrollable.h"

struct uiAction_s;
struct menuOption_s;

typedef struct {
	/* link to shared data (can be used if internal data is null) */
	int dataId;							/**< Shared data id where we can find option */
	int versionId;						/**< Cached version of the shared data, to check update */
	void* cvar;							/**< Cvar containing current value */

	/* information */
	struct menuOption_s *selected;		/**< current selected option */
	struct menuOption_s *hovered;		/**< current hovered option */
	int count;							/**< number of elements */
	int lineHeight;

	menuScroll_t scrollY;				/**< Scroll position, if need */

	struct uiAction_s *onViewChange;	/**< called when view change (number of elements...) */
} abstractOptionExtraData_t;

struct uiNode_s;
struct uiBehaviour_s;

void UI_RegisterAbstractOptionNode(struct uiBehaviour_s *behaviour);
void UI_OptionNodeSortOptions(struct uiNode_s *node);
struct uiNode_s* UI_AbstractOptionGetFirstOption(uiNode_t * node);
const char *UI_AbstractOptionGetCurrentValue(uiNode_t * node);
void UI_AbstractOptionSetCurrentValue(uiNode_t * node, const char *value);

#endif
