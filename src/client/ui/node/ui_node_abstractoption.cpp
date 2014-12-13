/**
 * @file
 * @todo manage disabled option
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

#include "../ui_main.h"
#include "../ui_internal.h"
#include "../ui_parse.h"
#include "../ui_draw.h"
#include "../ui_data.h"
#include "../ui_lua.h"

#include "ui_node_abstractoption.h"
#include "ui_node_abstractnode.h"

#include "../../../common/scripts_lua.h"

#define EXTRADATA_TYPE abstractOptionExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

/**
 * @brief Sort options by alphabet
 */
void UI_AbstractOption_SortOptions (uiNode_t* node)
{
	uiNode_t* option;
	assert(UI_Node_IsOptionContainer(node));
	UI_SortOptions(&node->firstChild);

	/** update lastChild */
	/** @todo the sort option should do it itself */
	option = node->firstChild;
	while (option->next)
		option = option->next;
	node->lastChild = option;
}

const char* UI_AbstractOption_GetCurrentValue (uiNode_t* node)
{
	/* no cvar given? */
	if (!EXTRADATA(node).cvar || !*EXTRADATA(node).cvar) {
		Com_Printf("UI_AbstractOptionGetCurrentValue: node [%s] doesn't have a valid cvar assigned\n", UI_GetPath(node));
		return nullptr;
	}

	/* not a cvar? */
	if (!Q_strstart(EXTRADATA(node).cvar, "*cvar:"))
		Com_Printf("UI_AbstractOptionGetCurrentValue: in node [%s], the name [%s] is not a value cvar\n", UI_GetPath(node), EXTRADATA(node).cvar);
		return nullptr;

	return UI_GetReferenceString(node, EXTRADATA(node).cvar);
}

void UI_AbstractOption_SetCurrentValue(uiNode_t* node, const char* value)
{
	const char* cvarName = &EXTRADATA(node).cvar[6];
	Cvar_Set(cvarName, "%s", value);
	if (node->onChange) {
		UI_ExecuteEventActions(node, node->onChange);
	}
	if (node->lua_onChange != LUA_NOREF) {
		UI_ExecuteLuaEventScript(node, node->lua_onChange);
	}
}

void uiAbstractOptionNode::initNode(uiNode_t* node) {
	uiLocatedNode::initNode(node);
	EXTRADATA(node).lua_onViewChange = LUA_NOREF;
}

void uiAbstractOptionNode::doLayout (uiNode_t* node)
{
	uiNode_t* option = node->firstChild;

	if (EXTRADATA(node).dataId == 0) {
		int count = 0;
		while (option && option->behaviour == ui_optionBehaviour) {
			UI_Validate(option);
			if (!option->invis)
				count++;
			option = option->next;
		}

		EXTRADATA(node).count = count;
	}

	node->invalidated = false;
}

/**
 * @brief Return the first option of the node
 * @todo check versionId and update cached data, and fire events
 */
uiNode_t* UI_AbstractOption_GetFirstOption (uiNode_t* node)
{
	if (node->firstChild && node->firstChild->behaviour == ui_optionBehaviour) {
		return node->firstChild;
	} else {
		const int v = UI_GetDataVersion(EXTRADATA(node).dataId);
		if (v != EXTRADATA(node).versionId) {
			int count = 0;
			uiNode_t* option = UI_GetOption(EXTRADATA(node).dataId);
			while (option) {
				if (!option->invis)
					count++;
				option = option->next;
			}
			EXTRADATA(node).count = count;
			EXTRADATA(node).versionId = v;
		}
		return UI_GetOption(EXTRADATA(node).dataId);
	}
}

/**
 * @brief Return size of the cell, which is the size (in virtual "pixel") which represents 1 in the scroll values.
 * Here expect the widget can scroll pixel per pixel.
 * @return Size in pixel.
 */
int uiAbstractOptionNode::getCellWidth (uiNode_t* node)
{
	return 1;
}

/**
 * @brief Return size of the cell, which is the size (in virtual "pixel") which represents 1 in the scroll values.
 * Here we guess the widget can scroll pixel per pixel.
 * @return Size in pixel.
 */
int uiAbstractOptionNode::getCellHeight (uiNode_t* node)
{
	return 1;
}

int UI_AbstractOption_GetDataId (uiNode_t* node) {
	return EXTRADATA(node).dataId;
}

int UI_AbstractOption_GetCount (uiNode_t* node) {
	return EXTRADATA(node).count;
}

const char* UI_AbstractOption_GetCvar (uiNode_t* node) {
	return EXTRADATA(node).cvar;
}

void UI_AbstractOption_SetDataIdByName (uiNode_t* node, const char* name) {
	EXTRADATA(node).dataId = UI_GetDataIDByName(name);
}

void UI_AbstractOption_SetCvar (uiNode_t* node, const char* name) {
	cvar_t* var = Cvar_Get(name);
	EXTRADATA(node).cvar = var->name;
}

void UI_AbstractOption_SetBackgroundByName(uiNode_t* node, const char* name) {
	uiSprite_t* sprite = UI_GetSpriteByName(name);
	UI_EXTRADATA(node, abstractOptionExtraData_t).background = sprite;
}

int UI_AbstractOption_Scroll_Current (uiNode_t* node) {
	return EXTRADATA(node).scrollY.viewPos;
}

void UI_AbstractOption_Scroll_SetCurrent (uiNode_t* node, int pos) {
	EXTRADATA(node).scrollY.move(pos);
}

int UI_AbstractOption_Scroll_ViewSize (uiNode_t* node) {
	return EXTRADATA(node).scrollY.viewSize;
}

int UI_AbstractOption_Scroll_FullSize (uiNode_t* node) {
	return EXTRADATA(node).scrollY.fullSize;
}


void UI_RegisterAbstractOptionNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "abstractoption";
	behaviour->isAbstract = true;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->drawItselfChild = true;
	behaviour->manager = UINodePtr(new uiAbstractOptionNode());
	behaviour->lua_SWIG_typeinfo = UI_SWIG_TypeQuery("uiAbstractOptionNode_t *");

	/** Optional. Data ID we want to use. It must be an option list. It substitute to the inline options */
	UI_RegisterExtradataNodeProperty(behaviour, "dataid", V_UI_DATAID, EXTRADATA_TYPE, dataId);
	/** Optional. We can define the height of the block containing an option. */
	UI_RegisterExtradataNodeProperty(behaviour, "lineheight", V_INT, EXTRADATA_TYPE, lineHeight);

	/* position of the vertical view (into the full number of elements the node contain) */
	UI_RegisterExtradataNodeProperty(behaviour, "viewpos", V_INT, EXTRADATA_TYPE, scrollY.viewPos);
	/* size of the vertical view (proportional to the number of elements the node can display without moving) */
	UI_RegisterExtradataNodeProperty(behaviour, "viewsize", V_INT, EXTRADATA_TYPE, scrollY.viewSize);
	/* full vertical size (proportional to the number of elements the node contain) */
	UI_RegisterExtradataNodeProperty(behaviour, "fullsize", V_INT, EXTRADATA_TYPE, scrollY.fullSize);

	/* number of elements contain the node */
	UI_RegisterExtradataNodeProperty(behaviour, "count", V_INT, EXTRADATA_TYPE, count);

	/* Define the cvar containing the value of the current selected option */
	UI_RegisterExtradataNodeProperty(behaviour, "cvar", V_UI_CVAR, EXTRADATA_TYPE, cvar);

	/* Called when one of the properties viewpos/viewsize/fullsize change */
	UI_RegisterExtradataNodeProperty(behaviour, "onviewchange", V_UI_ACTION, EXTRADATA_TYPE, onViewChange);
}
