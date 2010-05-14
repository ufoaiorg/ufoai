/**
 * @file m_node_selectbox.c
 * @todo manage disabled option
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "../m_main.h"
#include "../m_internal.h"
#include "../m_parse.h"
#include "../m_draw.h"
#include "../m_data.h"
#include "m_node_abstractoption.h"
#include "m_node_abstractnode.h"

#define SELECTBOX_SIDE_WIDTH 7.0f
#define SELECTBOX_RIGHT_WIDTH 20.0f

#define SELECTBOX_SPACER 2.0f
#define SELECTBOX_BOTTOM_HEIGHT 4.0f

#define EXTRADATA(node) (node->u.option)

/**
 * @brief Sort options by alphabet
 */
void MN_OptionNodeSortOptions (menuNode_t *node)
{
	assert(MN_NodeInstanceOf(node, "abstractoption"));
	MN_SortOptions(&EXTRADATA(node).first);
}

/**
 * @brief Adds a new selectbox option to a selectbox node
 * @return NULL if menuSelectBoxes is 'full' - otherwise pointer to the selectBoxOption
 * @param[in] node Context node
 * @param[in] newOption The option to add
 * @note You have to add the values manually to the option pointer
 */
menuOption_t* MN_NodeAppendOption (menuNode_t *node, menuOption_t *newOption)
{
	assert(node);
	assert(newOption);
	assert(MN_NodeInstanceOf(node, "abstractoption"));

	if (!EXTRADATA(node).first)
		EXTRADATA(node).first = newOption;
	else {
		/* link it in */
		menuOption_t *option = EXTRADATA(node).first;
		while (option->next)
			option = option->next;
		option->next = newOption;
	}
	EXTRADATA(node).count++;
	return newOption;
}

static void MN_UpdateOption_f (void)
{
	menuNode_t *node;
	menuOption_t *option;

	if (Cmd_Argc() != 4) {
		Com_Printf("Usage: %s <nodepath> <optionname> <hide|display|enable|disable>\n", Cmd_Argv(0));
		return;
	}

	node = MN_GetNodeByPath(Cmd_Argv(1));
	if (node == NULL) {
		Com_Printf("MN_UpdateOption_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}

	if (!MN_NodeInstanceOf(node, "abstractoption")) {
		Com_Printf("MN_UpdateOption_f: '%s' node is not an 'abstractoption'.\n", Cmd_Argv(1));
		return;
	}

	option = EXTRADATA(node).first;
	while (option) {
		if (!strcmp(option->id, Cmd_Argv(2)))
			break;
		option = option->next;
	}
	if (option == NULL) {
		Com_Printf("MN_UpdateOption_f: option '%s' from '%s' node not found.\n", Cmd_Argv(2), Cmd_Argv(1));
		return;
	}

	if (!strcmp("disable", Cmd_Argv(3))) {
		option->disabled = qtrue;
	} else if (!strcmp("enable", Cmd_Argv(3))) {
		option->disabled = qfalse;
	} else if (!strcmp("hide", Cmd_Argv(3))) {
		option->invis = qtrue;
	} else if (!strcmp("display", Cmd_Argv(3))) {
		option->invis = qfalse;
	} else {
		Com_Printf("MN_AddListener_f: '%s' command do not exists.\n", Cmd_Argv(3));
		return;
	}
}

static const value_t properties[] = {
	/** Optional. Data ID we want to use. It must be an option list. It substitute to the inline options */
	{"dataid", V_UI_DATAID, MN_EXTRADATA_OFFSETOF(optionExtraData_t, dataId), MEMBER_SIZEOF(optionExtraData_t, dataId)},
	/** Special property only use into node description. Used for each element of the inline option list */
	{"option", V_UI_OPTIONNODE, MN_EXTRADATA_OFFSETOF(optionExtraData_t, first), 0},
	/** Optional. We can define the height of the block containing an option. */
	{"lineheight", V_INT, MN_EXTRADATA_OFFSETOF(optionExtraData_t, lineHeight),  MEMBER_SIZEOF(optionExtraData_t, lineHeight)},

	/* position of the vertical view (into the full number of elements the node contain) */
	{"viewpos", V_INT, MN_EXTRADATA_OFFSETOF(optionExtraData_t, scrollY.viewPos),  MEMBER_SIZEOF(optionExtraData_t, scrollY.viewPos)},
	/* size of the vertical view (proportional to the number of elements the node can display without moving) */
	{"viewsize", V_INT, MN_EXTRADATA_OFFSETOF(optionExtraData_t, scrollY.viewSize),  MEMBER_SIZEOF(optionExtraData_t, scrollY.viewSize)},
	/* full vertical size (proportional to the number of elements the node contain) */
	{"fullsize", V_INT, MN_EXTRADATA_OFFSETOF(optionExtraData_t, scrollY.fullSize),  MEMBER_SIZEOF(optionExtraData_t, scrollY.fullSize)},

	/* number of elements contain the node */
	{"count", V_INT, MN_EXTRADATA_OFFSETOF(optionExtraData_t, count),  MEMBER_SIZEOF(optionExtraData_t, count)},

	/* Define the cvar containing the value of the current selected option */
	{"cvar", V_UI_CVAR, offsetof(menuNode_t, cvar), 0},

	/* Called when one of the properties viewpos/viewsize/fullsize change */
	{"onviewchange", V_UI_ACTION, MN_EXTRADATA_OFFSETOF(optionExtraData_t, onViewChange), MEMBER_SIZEOF(optionExtraData_t, onViewChange)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractOptionNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractoption";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(optionExtraData_t);
	Cmd_AddCommand("mn_updateoption", MN_UpdateOption_f, "Update some option status");
}
