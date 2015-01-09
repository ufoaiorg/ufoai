/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

/* prototype */
struct uiNode_t;

#define UI_MAX_KEYBINDING	128

typedef struct uiKeyBinding_s {
	uiNode_t* node;						/**< Node target. */
	const struct value_s* property;		/**< Property target, else nullptr. */
	int key;							/**< Keynum to catch. */
	const char* description;			/**< Description of this binding */
	bool inherited;						/**< True if this binding is inherited from another binding. */
	struct uiKeyBinding_s* next;		/**< Next binding from the window list. */
} uiKeyBinding_t;

void UI_SetKeyBinding(const char* path, int key, const char* description);

/* mouse input */
void UI_MouseScroll(int deltaX, int deltaY);
void UI_MouseMove(int x, int y);
void UI_MouseDown(int x, int y, int button);
void UI_MouseUp(int x, int y, int button);
void UI_InvalidateMouse(void);
bool UI_CheckMouseMove(void);
uiNode_t* UI_GetHoveredNode(void);
void UI_ResetInput(void);

/* focus */
void UI_RequestFocus(uiNode_t* node);
bool UI_HasFocus(uiNode_t const* node);
void UI_RemoveFocus(void);
bool UI_KeyRelease(unsigned int key, unsigned short unicode);
bool UI_KeyPressed(unsigned int key, unsigned short unicode);
int UI_GetKeyBindingCount(void);
uiKeyBinding_t* UI_GetKeyBindingByIndex(int index);

/* mouse capture */
uiNode_t* UI_GetMouseCapture(void);
void UI_SetMouseCapture(uiNode_t* node);
void UI_MouseRelease(void);

/* all inputs */
void UI_ReleaseInput(void);
