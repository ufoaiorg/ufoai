/**
 * @file ui_node_abstractnode.h
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

#ifndef CLIENT_UI_UI_NODE_ABSTRACTNODE_H
#define CLIENT_UI_UI_NODE_ABSTRACTNODE_H

#include "../ui_nodes.h"
#include "../ui_node.h"

struct uiNode_s;
struct uiBehaviour_s;

class uiNode {
public:
	/** Called before script initialization, initialized default values */
	virtual void loading(struct uiNode_s *node) {}
	/** only called one time, when node parsing was finished */
	virtual void loaded(struct uiNode_s *node) {}

	virtual ~uiNode() {}
};

class uiLocatedNode : public uiNode {
public:
	/** How to draw a node */
	virtual void draw(struct uiNode_s *node) {}

	/** Mouse move event in the node */
	virtual void mouseMove(struct uiNode_s *node, int x, int y) {}
	/** Mouse button down event in the node */
	virtual void mouseDown(struct uiNode_s *node, int x, int y, int button) {}
	/** Mouse button up event in the node */
	virtual void mouseUp(struct uiNode_s *node, int x, int y, int button) {}
	/** Mouse move event in the node when captured */
	virtual void capturedMouseMove(struct uiNode_s *node, int x, int y) {}
	/** Capture is finished */
	virtual void capturedMouseLost(struct uiNode_s *node) {}

	virtual ~uiLocatedNode() {}
};

void UI_RegisterAbstractNode(struct uiBehaviour_s *);

#endif
