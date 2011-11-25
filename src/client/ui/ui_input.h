/**
 * @file ui_input.h
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

#ifndef CLIENT_UI_UI_INPUT_H
#define CLIENT_UI_UI_INPUT_H

/* prototype */
struct uiNode_s;

#define UI_MAX_KEYBINDING	128

typedef struct uiKeyBinding_s {
	struct uiNode_s *node;				/**< Node target. */
	const struct value_s *property;		/**< Property target, else NULL. */
	int key;							/**< Keynum to catch. */
	const char* description;			/**< Description of this binding */
	qboolean inherited;					/**< True if this binding is inherited from another binding. */
	struct uiKeyBinding_s *next;		/**< Next binding from the window list. */
} uiKeyBinding_t;

void UI_SetKeyBinding(const char* path, int key, const char* description);

/* mouse input */
void UI_MouseScroll(int deltaX, int deltaY);
void UI_MouseMove(int x, int y);
void UI_MouseDown(int x, int y, int button);
void UI_MouseUp(int x, int y, int button);
void UI_InvalidateMouse(void);
qboolean UI_CheckMouseMove(void);
struct uiNode_s *UI_GetHoveredNode(void);

/* focus */
void UI_RequestFocus(struct uiNode_s* node);
qboolean UI_HasFocus(const struct uiNode_s* node);
void UI_RemoveFocus(void);
qboolean UI_KeyRelease(unsigned int key, unsigned short unicode);
qboolean UI_KeyPressed(unsigned int key, unsigned short unicode);
int UI_GetKeyBindingCount(void);
uiKeyBinding_t* UI_GetKeyBindingByIndex(int index);

/* mouse capture */
struct uiNode_s* UI_GetMouseCapture(void);
void UI_SetMouseCapture(struct uiNode_s* node);
void UI_MouseRelease(void);

/* all inputs */
void UI_ReleaseInput(void);

#endif
