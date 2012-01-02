/**
 * @file ui_node_abstractscrollbar.c
 * @brief The <code>abstractscrollbar</code> is an abstract node (we can't instantiate it).
 * It exists to share same properties for vertical and horizontal scrollbar.
 * At the moment only the concrete <code>vscrollbar</code>.
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

#include "../ui_property.h"
#include "ui_node_abstractscrollbar.h"

#define EXTRADATA_TYPE abstractScrollbarExtraData_t

static const value_t properties[] = {
	/* Current position of the scroll. Image of the <code>viewpos</code> from <code>abstractscrollable</code> node. */
	UI_INIT_EXTRADATA_PROPERTY("current", V_INT, EXTRADATA_TYPE, pos),
	/* Image of the <code>viewsize</code> from <code>abstractscrollable</code> node. */
	UI_INIT_EXTRADATA_PROPERTY("viewsize", V_INT, EXTRADATA_TYPE, viewsize),
	/* Image of the <code>fullsize</code> from <code>abstractscrollable</code> node. */
	UI_INIT_EXTRADATA_PROPERTY("fullsize", V_INT, EXTRADATA_TYPE, fullsize),

	/* If true, hide the scroll when the position is 0 and can't change (when <code>viewsize</code> >= <code>fullsize</code>). */
	UI_INIT_EXTRADATA_PROPERTY("hidewhenunused", V_BOOL, EXTRADATA_TYPE, hideWhenUnused),

	/* Callback value set when before calling onChange. It is used to know the change apply by the user
	 * @Deprecated
	 */
	UI_INIT_EXTRADATA_PROPERTY("lastdiff", V_INT, EXTRADATA_TYPE, lastdiff),

	UI_INIT_EMPTY_PROPERTY
};

void UI_RegisterAbstractScrollbarNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "abstractscrollbar";
	behaviour->isAbstract = qtrue;
	behaviour->oldProperties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
