/**
 * @file m_node_window.c
 * @note this file is about menu function. Its not yet a real node,
 * but it may become one. Think the code like that will help to merge menu and node.
 * @note It used 'window' instead of 'menu', because a menu is not this king of widget
 * @todo merge here 'windowpanel' when it will be a real node
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

#include "../../client.h"
#include "../m_main.h"
#include "../m_parse.h"
#include "../m_nodes.h"
#include "m_node_window.h"
#include "m_node_abstractnode.h"

/**
 * @brief Check if a window is fullscreen or not
 */
qboolean MN_WindowIsFullScreen (menuNode_t* const menu)
{
	return menu->pos[0] == 0 && menu->size[0] == VID_NORM_WIDTH
		&& menu->pos[1] == 0 && menu->size[1] == VID_NORM_HEIGHT;
}

/**
 * @brief Called at the begin of the load from script
 */
static void MN_WindowNodeLoading (menuNode_t *menu)
{
	menu->pos[0] = 0;
	menu->pos[1] = 0;
	menu->size[0] = VID_NORM_WIDTH;
	menu->size[1] = VID_NORM_HEIGHT;
}

static const int CONTROLS_IMAGE_DIMENSIONS = 17;
static const int CONTROLS_PADDING = 22;
static const int CONTROLS_SPACING = 5;

#define TOP_HEIGHT 46

/**
 * @brief Called at the end of the load from script
 */
static void MN_WindowNodeLoaded (menuNode_t *menu)
{
	/* if it need, construct the drag button */
	if (menu->u.window.dragButton) {
		menuNode_t *control = MN_AllocNode("controls");
		Q_strncpyz(control->name, "move_window_button", sizeof(control->name));
		control->menu = menu;
		control->image = NULL;
		/** @todo Once @c image_t is known on the client, use @c image->width resp. @c image->height here */
		control->size[0] = menu->size[0];
		control->size[1] = TOP_HEIGHT;
		control->pos[0] = 0;
		control->pos[1] = 0;
		control->tooltip = _("Drag to move window");
		MN_AppendNode(menu, control);
	}

	/* if the menu should have a close button, add it here */
	if (menu->u.window.closeButton) {
		menuNode_t *button = MN_AllocNode("pic");
		const int positionFromRight = CONTROLS_PADDING;
		const char* command = MN_AllocString(va("mn_close %s;", menu->name), 0);
		Q_strncpyz(button->name, "close_window_button", sizeof(button->name));
		button->menu = menu;
		button->image = "menu/close";
		/** @todo Once @c image_t is known on the client, use @c image->width resp. @c image->height here */
		button->size[0] = CONTROLS_IMAGE_DIMENSIONS;
		button->size[1] = CONTROLS_IMAGE_DIMENSIONS;
		button->pos[0] = menu->size[0] - positionFromRight - button->size[0];
		button->pos[1] = CONTROLS_PADDING;
		button->tooltip = _("Close the window");
		MN_SetMenuAction(&button->onClick, EA_CMD, command);
		MN_AppendNode(menu, button);
	}
}

/**
 * @brief Valid properties for a window node (called yet 'menu')
 */
static const value_t windowNodeProperties[] = {
	{"pos", V_POS, offsetof(menuNode_t, pos), MEMBER_SIZEOF(menuNode_t, pos)},
	{"size", V_POS, offsetof(menuNode_t, size), MEMBER_SIZEOF(menuNode_t, size)},

	{"noticepos", V_POS, offsetof(menuNode_t, u.window.noticePos), MEMBER_SIZEOF(menuNode_t, u.window.noticePos)},
	{"dragbutton", V_BOOL, offsetof(menuNode_t, u.window.dragButton), MEMBER_SIZEOF(menuNode_t, u.window.dragButton)},
	{"closebutton", V_BOOL, offsetof(menuNode_t, u.window.closeButton), MEMBER_SIZEOF(menuNode_t, u.window.closeButton)},
	{"modal", V_BOOL, offsetof(menuNode_t, u.window.modal), MEMBER_SIZEOF(menuNode_t, u.window.modal)},
	{"preventtypingescape", V_BOOL, offsetof(menuNode_t, u.window.preventTypingEscape), MEMBER_SIZEOF(menuNode_t, u.window.preventTypingEscape)},

	{"init", V_SPECIAL_ACTION, offsetof(menuNode_t, u.window.onInit), MEMBER_SIZEOF(menuNode_t, u.window.onInit)},
	{"close", V_SPECIAL_ACTION, offsetof(menuNode_t, u.window.onClose), MEMBER_SIZEOF(menuNode_t, u.window.onClose)},
	{"leave", V_SPECIAL_ACTION, offsetof(menuNode_t, u.window.onLeave), MEMBER_SIZEOF(menuNode_t, u.window.onLeave)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterWindowNode (nodeBehaviour_t *behaviour)
{
	/** @todo rename it according to the function name when its possible */
	behaviour->name = "menu";
	behaviour->loading = MN_WindowNodeLoading;
	behaviour->loaded = MN_WindowNodeLoaded;
	behaviour->properties = windowNodeProperties;
}
