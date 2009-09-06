/**
 * @file m_node_abstractscrollbar.c
 * @brief The <code>abstractscrollbar</code> is an abstract node (we can't instantiate it).
 * It exists to share same properties for vertical and horizontal scrollbar.
 * At the moment only the concrete <code>vscrollbar</code>.
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
#include "m_node_abstractscrollbar.h"

static const value_t properties[] = {
	/* Current position of the scroll. Image of the <code>viewpos</code> from <code>abstractscrollable</code> node. */
	{"current", V_INT, MN_EXTRADATA_OFFSETOF(abstractScrollbarExtraData_t, pos),  MEMBER_SIZEOF(abstractScrollbarExtraData_t, pos)},
	/* Image of the <code>viewsize</code> from <code>abstractscrollable</code> node. */
	{"viewsize", V_INT, MN_EXTRADATA_OFFSETOF(abstractScrollbarExtraData_t, viewsize),  MEMBER_SIZEOF(abstractScrollbarExtraData_t, viewsize)},
	/* Image of the <code>fullsize</code> from <code>abstractscrollable</code> node. */
	{"fullsize", V_INT, MN_EXTRADATA_OFFSETOF(abstractScrollbarExtraData_t, fullsize),  MEMBER_SIZEOF(abstractScrollbarExtraData_t, fullsize)},

	/* If true, hide the scroll when the position is 0 and can't change (when <code>viewsize</code> >= <code>fullsize</code>). */
	{"hidewhenunused", V_BOOL, MN_EXTRADATA_OFFSETOF(abstractScrollbarExtraData_t, hideWhenUnused),  MEMBER_SIZEOF(abstractScrollbarExtraData_t, hideWhenUnused)},

	/* Callback value set when before calling onChange. It is used to know the change apply by the user
	 * @Deprecated
	 */
	{"lastdiff", V_INT, MN_EXTRADATA_OFFSETOF(abstractScrollbarExtraData_t, lastdiff),  MEMBER_SIZEOF(abstractScrollbarExtraData_t, lastdiff)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractScrollbarNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractscrollbar";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(abstractScrollbarExtraData_t);
}
