/**
 * @file ui_node_radiobutton.c
 * @brief The radiobutton is a clickable widget. Commonly, with use it in
 * a group of radiobuttons; the user is allowed to choose only one button
 * from this set. The current implementation share the value of the group
 * with a cvar, and each button use is own value. When the cvar equals to
 * a button value, this button is selected.
 * @code
 * radiobutton foo {
 *   cvar "*cvar:foobar"
 *   value 4
 *   icon boo
 * }
 * @endcode
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

#include "../ui_main.h"
#include "../ui_actions.h"
#include "../ui_sprite.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_input.h"
#include "../ui_render.h"
#include "ui_node_radiobutton.h"
#include "ui_node_abstractnode.h"

#define EXTRADATA_TYPE radioButtonExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

#define EPSILON 0.001f

/** Height of a status in a 4 status 256*256 texture */
#define UI_4STATUS_TEX_HEIGHT 64

static qboolean UI_RadioButtonNodeIsSelected (uiNode_t *node)
{
	if (EXTRADATA(node).string == NULL) {
		const float current = UI_GetReferenceFloat(node, EXTRADATA(node).cvar);
		return current > EXTRADATA(node).value - EPSILON && current < EXTRADATA(node).value + EPSILON;
	} else {
		const char *current = UI_GetReferenceString(node, EXTRADATA(node).cvar);
		return Q_streq(current, EXTRADATA(node).string);
	}
}

/**
 * @brief Handles RadioButton draw
 * @todo need to implement image. We can't do everything with only one icon (or use another icon)
 */
static void UI_RadioButtonNodeDraw (uiNode_t *node)
{
	vec2_t pos;
	uiSpriteStatus_t iconStatus;
	const qboolean disabled = node->disabled || node->parent->disabled;
	int texY;
	const char *image;
	const qboolean isSelected = UI_RadioButtonNodeIsSelected(node);

	if (disabled) {
		iconStatus = SPRITE_STATUS_DISABLED;
		texY = UI_4STATUS_TEX_HEIGHT * 2;
	} else if (isSelected) {
		iconStatus = SPRITE_STATUS_CLICKED;
		texY = UI_4STATUS_TEX_HEIGHT * 3;
	} else if (node->state) {
		iconStatus = SPRITE_STATUS_HOVER;
		texY = UI_4STATUS_TEX_HEIGHT;
	} else {
		iconStatus = SPRITE_STATUS_NORMAL;
		texY = 0;
	}

	UI_GetNodeAbsPos(node, pos);

	image = UI_GetReferenceString(node, node->image);
	if (image) {
		const int texX = 0;
		UI_DrawNormImageByName(qfalse, pos[0], pos[1], node->size[0], node->size[1],
			texX + node->size[0], texY + node->size[1], texX, texY, image);
	}

	if (EXTRADATA(node).background) {
		UI_DrawSpriteInBox(qfalse, EXTRADATA(node).background, iconStatus, pos[0], pos[1], node->size[0], node->size[1]);
	}

	if (EXTRADATA(node).icon) {
		UI_DrawSpriteInBox(EXTRADATA(node).flipIcon, EXTRADATA(node).icon, iconStatus, pos[0], pos[1], node->size[0], node->size[1]);
	}
}

/**
 * @brief Activate the node. Can be used without the mouse (ie. a button will execute onClick)
 */
static void UI_RadioButtonNodeActivate (uiNode_t * node)
{
	/* no cvar given? */
	if (!EXTRADATA(node).cvar || !*(char*)(EXTRADATA(node).cvar)) {
		Com_Printf("UI_RadioButtonNodeClick: node '%s' doesn't have a valid cvar assigned\n", UI_GetPath(node));
		return;
	}

	/* its not a cvar! */
	/** @todo the parser should already check that the property value is a right cvar */
	if (!Q_strstart((const char *)(EXTRADATA(node).cvar), "*cvar"))
		return;

	UI_GetReferenceFloat(node, EXTRADATA(node).cvar);
	/* Is we click on the already selected button, we can continue */
	if (UI_RadioButtonNodeIsSelected(node))
		return;

	{
		const char *cvarName = &((const char *)(EXTRADATA(node).cvar))[6];

		if (EXTRADATA(node).string == NULL) {
			Cvar_SetValue(cvarName, EXTRADATA(node).value);
		} else {
			Cvar_Set(cvarName, EXTRADATA(node).string);
		}
		if (node->onChange)
			UI_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief Handles radio button clicks
 */
static void UI_RadioButtonNodeClick (uiNode_t * node, int x, int y)
{
	if (node->onClick)
		UI_ExecuteEventActions(node, node->onClick);

	UI_RadioButtonNodeActivate(node);
}

void UI_RegisterRadioButtonNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "radiobutton";
	behaviour->draw = UI_RadioButtonNodeDraw;
	behaviour->leftClick = UI_RadioButtonNodeClick;
	behaviour->activate = UI_RadioButtonNodeActivate;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Numerical value defining the radiobutton. Cvar is updated with this value when the radio button is selected. */
	UI_RegisterExtradataNodeProperty(behaviour, "value", V_FLOAT, EXTRADATA_TYPE, value);
	/* String Value defining the radiobutton. Cvar is updated with this value when the radio button is selected. */
	UI_RegisterExtradataNodeProperty(behaviour, "stringValue", V_CVAR_OR_STRING, EXTRADATA_TYPE, string);

	/* Cvar name shared with the radio button group to identify when a radio button is selected. */
	UI_RegisterExtradataNodeProperty(behaviour, "cvar", V_UI_CVAR, EXTRADATA_TYPE, cvar);
	/* Icon used to display the node */
	UI_RegisterExtradataNodeProperty(behaviour, "icon", V_UI_SPRITEREF, EXTRADATA_TYPE, icon);
	UI_RegisterExtradataNodeProperty(behaviour, "flipicon", V_BOOL, EXTRADATA_TYPE, flipIcon);
	/* Sprite used to display the background */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);
}
