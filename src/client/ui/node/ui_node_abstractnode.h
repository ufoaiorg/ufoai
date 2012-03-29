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
	/* system allocation */

	/** Called before script initialization, initialized default values */
	virtual void loading(struct uiNode_s *node) {}
	/** only called one time, when node parsing was finished */
	virtual void loaded(struct uiNode_s *node) {}
	/** call to initialize a cloned node */
	virtual void clone(const struct uiNode_s *source, struct uiNode_s *clone) {}
	/** call to initialize a dynamic node */
	virtual void newNode(struct uiNode_s *node) {}
	/** call to delete a dynamic node */
	virtual void deleteNode(struct uiNode_s *node) {}

	/* system callback */

	/** Invoked when the window is added to the rendering stack */
	virtual void windowOpened(struct uiNode_s *node, linkedList_t *params);
	/** Invoked when the window is removed from the rendering stack */
	virtual void windowClosed(struct uiNode_s *node);
	/** Activate the node. Can be used without the mouse (ie. a button will execute onClick) */
	virtual void activate(struct uiNode_s *node);
	/** Called when a property change */
	virtual void propertyChanged(struct uiNode_s *node, const value_t *property);

	virtual ~uiNode() {}
};

class uiLocatedNode : public uiNode {
public:
	/** How to draw a node */
	virtual void draw(struct uiNode_s *node) {}
	/** Allow to draw a custom tooltip */
	virtual void drawTooltip(struct uiNode_s *node, int x, int y);
	/** Callback to draw content over the window @sa UI_CaptureDrawOver */
	virtual void drawOverWindow(struct uiNode_s *node) {}

	/** Called to update node layout */
	virtual void doLayout(struct uiNode_s *node);
	/** Called when the node size change */
	virtual void sizeChanged(struct uiNode_s *node);

	/* mouse events */

	/** Left mouse click event in the node */
	virtual void leftClick(struct uiNode_s *node, int x, int y);
	/** Right mouse button click event in the node */
	virtual void rightClick(struct uiNode_s *node, int x, int y);
	/** Middle mouse button click event in the node */
	virtual void middleClick(struct uiNode_s *node, int x, int y);
	/** Mouse wheel event in the node */
	virtual bool scroll(struct uiNode_s *node, int deltaX, int deltaY);
	/* Planned */
#if 0
	/* mouse move event */
	virtual void mouseEnter(struct uiNode_s *node);
	virtual void mouseLeave(struct uiNode_s *node);
#endif

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

	/* drag and drop callback */

	/** Send to the target when we enter first, return true if we can drop the DND somewhere on the node */
	virtual bool dndEnter(struct uiNode_s *node);
	/** Send to the target when we enter first, return true if we can drop the DND here */
	virtual bool dndMove(struct uiNode_s *node, int x, int y);
	/** Send to the target when the DND is canceled */
	virtual void dndLeave(struct uiNode_s *node);
	/** Send to the target to finalize the drop */
	virtual bool dndDrop(struct uiNode_s *node, int x, int y);
	/** Sent to the source to finalize the drop */
	virtual bool dndFinished(struct uiNode_s *node, bool isDroped);

	/* focus and keyboard events */
	virtual void focusGained(struct uiNode_s *node) {}
	virtual void focusLost(struct uiNode_s *node) {}
	virtual bool keyPressed(struct uiNode_s *node, unsigned int key, unsigned short unicode) {return false;}
	virtual bool keyReleased(struct uiNode_s *node, unsigned int key, unsigned short unicode) {return false;}

	/** Return the position of the client zone into the node */
	virtual void getClientPosition(const struct uiNode_s *node, vec2_t position) {}
	/** cell size */
	virtual int getCellWidth (uiNode_t *node) {return 1;}
	/** cell size */
	virtual int getCellHeight (uiNode_t *node) {return 1;}

};

void UI_RegisterAbstractNode(struct uiBehaviour_s *);

#endif
