/**
 * @file ui_node_abstractscrollbar.h
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


#ifndef CLIENT_UI_UI_NODE_ABSTRACTSCROLLBAR_H
#define CLIENT_UI_UI_NODE_ABSTRACTSCROLLBAR_H

/**
 * @brief extradata for scrollbar widget
 * @todo think about switching to percent when its possible (lowPosition, hightPosition)
 * @todo think about adding a "direction" property and merging v and h scrollbar
 */
typedef struct abstractScrollbarExtraData_s {
	int pos;		/**< Position of the visible size */
	int viewsize;	/**< Visible size */
	int fullsize;	/**< Full size allowed */
	int lastdiff;	/**< Different of the pos from the last update. Its more an event property than a node property */
	qboolean hideWhenUnused; /** Hide the scrollbar when we can't scroll anything */
} abstractScrollbarExtraData_t;

struct uiBehaviour_s; /* prototype */

void UI_RegisterAbstractScrollbarNode(struct uiBehaviour_s *behaviour);

#endif
