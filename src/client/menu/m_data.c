/**
 * @file m_data.c
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

#include "../client.h"
#include "m_main.h"
#include "m_internal.h"
#include "m_nodes.h"
#include "node/m_node_linechart.h"
#include "node/m_node_option.h"

/**
 * @brief
 */
static const char *const menutextid_names[] = {
	"", /**< NULL value */
	"TEXT_STANDARD",
	"TEXT_LIST",
	"TEXT_LIST2",
	"TEXT_UFOPEDIA",
	"TEXT_UFOPEDIA_REQUIREMENT",
	"TEXT_BUILDINGS",
	"TEXT_BUILDING_INFO",
	"TEXT_RESEARCH",
	"TEXT_POPUP",
	"TEXT_POPUP_INFO",
	"TEXT_AIRCRAFT_LIST",
	"TEXT_AIRCRAFT_INFO",
	"TEXT_CAMPAIGN_LIST",
	"TEXT_MULTISELECTION",
	"TEXT_PRODUCTION_LIST",
	"TEXT_PRODUCTION_AMOUNT",
	"TEXT_PRODUCTION_INFO",
	"TEXT_EMPLOYEE",
	"TEXT_MOUSECURSOR_RIGHT",
	"TEXT_PRODUCTION_QUEUED",
	"TEXT_STATS_BASESUMMARY",
	"TEXT_STATS_MISSION",
	"TEXT_STATS_BASES",
	"TEXT_STATS_NATIONS",
	"TEXT_STATS_EMPLOYEES",
	"TEXT_STATS_COSTS",
	"TEXT_STATS_INSTALLATIONS",
	"TEXT_STATS_7",
	"TEXT_BASE_LIST",
	"TEXT_BASE_INFO",
	"TEXT_TRANSFER_LIST",
	"TEXT_TRANSFER_LIST_AMOUNT",
	"TEXT_TRANSFER_LIST_TRANSFERED",
	"TEXT_MOUSECURSOR_PLAYERNAMES",
	"TEXT_CARGO_LIST",
	"TEXT_CARGO_LIST_AMOUNT",
	"TEXT_UFOPEDIA_MAILHEADER",
	"TEXT_UFOPEDIA_MAIL",
	"TEXT_MARKET_NAMES",
	"TEXT_MARKET_STORAGE",
	"TEXT_MARKET_MARKET",
	"TEXT_MARKET_PRICES",
	"TEXT_CHAT_WINDOW",
	"TEXT_AIREQUIP_1",
	"TEXT_AIREQUIP_2",
	"TEXT_BASEDEFENCE_LIST",
	"TEXT_TIPOFTHEDAY",
	"TEXT_GENERIC",
	"TEXT_XVI",
	"TEXT_MOUSECURSOR_TOP",
	"TEXT_MOUSECURSOR_BOTTOM",
	"TEXT_MOUSECURSOR_LEFT",
	"TEXT_MESSAGEOPTIONS",
	"TEXT_UFORECOVERY_NATIONS",
	"TEXT_UFORECOVERY_UFOYARDS",
	"TEXT_UFORECOVERY_CAPACITIES",
	"TEXT_MATERIAL_STAGES",
	"TEXT_IRCCONTENT",
	"TEXT_IRCUSERS",
	"TEXT_MULTIPLAYER_USERLIST",
	"TEXT_MULTIPLAYER_USERTEAM",
	"TEXT_ITEMDESCRIPTION",

	"OPTION_LANGUAGES",
	"OPTION_JOYSTICKS",
	"OPTION_VIDEO_RESOLUTIONS",
	"OPTION_SINGLEPLAYER_SKINS",
	"OPTION_MULTIPLAYER_SKINS",
	"OPTION_UFOPEDIA",
	"OPTION_UFOS",
	"OPTION_DROPSHIPS",
	"OPTION_BASELIST",

	"LINESTRIP_FUNDING",
	"LINESTRIP_COLOR"
};
CASSERT(lengthof(menutextid_names) == MAX_MENUTEXTS);

/**
 * @brief Return a dataId by name
 * @return A dataId if data found, else -1
 */
int MN_GetDataIDByName (const char* name)
{
	int num;
	for (num = 0; num < MAX_MENUTEXTS; num++)
		if (!strcmp(name, menutextid_names[num]))
			return num;

	return -1;
}

/**
 * @brief link a text to a menu text id
 * @note The menu doesn't manage the text memory, only save a pointer
 */
void MN_RegisterText (int textId, const char *text)
{
	MN_ResetData(textId);

	if (!text)
		return;

	mn.sharedData[textId].type = MN_SHARED_TEXT;
	mn.sharedData[textId].data.text = text;
	mn.sharedData[textId].versionId++;
}

/**
 * @brief link a text to a menu text id
 * @note The menu doesn't manage the text memory, only save a pointer
 */
void MN_RegisterLinkedListText (int textId, linkedList_t *text)
{
	/** Hack to disable release memory, if we only want to update the same list */
	if (mn.sharedData[textId].type == MN_SHARED_LINKEDLISTTEXT && mn.sharedData[textId].data.linkedListText == text) {
		mn.sharedData[textId].versionId++;
		return;
	}
	MN_ResetData(textId);
	mn.sharedData[textId].type = MN_SHARED_LINKEDLISTTEXT;
	mn.sharedData[textId].data.linkedListText = text;
	mn.sharedData[textId].versionId++;
}

const char *MN_GetText (int textId)
{
	if (mn.sharedData[textId].type != MN_SHARED_TEXT)
		return NULL;
	return mn.sharedData[textId].data.text;
}

int MN_GetDataVersion (int textId)
{
	return mn.sharedData[textId].versionId;
}

/**
 * @brief Initializes an option with a very little set of values.
 * @param[in] option Context option
 * @param[in] nameID name of the option (should be unique in the option list)
 * @param[in] label label displayed
 * @param[in] value value used when this option is selected
 */
static void MN_InitOption (menuNode_t* option, const char* label, const char* value)
{
	assert(option);
	assert(option->behaviour == optionBehaviour);
	Q_strncpyz(OPTIONEXTRADATA(option).label, label, sizeof(OPTIONEXTRADATA(option).label));
	Q_strncpyz(OPTIONEXTRADATA(option).value, value, sizeof(OPTIONEXTRADATA(option).value));
}

/**
 * @brief Append an option to an option list.
 * @param[in,out] tree first option of the list/tree of options
 * @param[in] name name of the option (should be unique in the option list)
 * @param[in] label label displayed
 * @param[in] value value used when this option is selected
 * @return The new option
 */
menuNode_t* MN_AddOption (menuNode_t**tree, const char* name, const char* label, const char* value)
{
	menuNode_t *last;
	menuNode_t *option;
	assert(tree != NULL);

	option = MN_AllocNode(name, "option", qtrue);
	MN_InitOption(option, label, value);

	/* append the option */
	last = *tree;
	if (last != NULL) {
		while (last->next)
			last = last->next;
	}

	if (last)
		last->next = option;
	else
		*tree = option;

	return option;
}

/**
 * @warning If we use it with real option node, i will crash the code cause
 * relation with parent node are not updated
 * @param tree
 */
static void MN_DeleteOption (menuNode_t* tree)
{
	while (tree) {
		menuNode_t* del = tree;
		tree = tree->next;
		MN_DeleteNode(del);
	}
}

/**
 * @brief Reset a shared data. Type became NONE and value became NULL
 */
void MN_ResetData (int dataId)
{
	assert(dataId < MAX_MENUTEXTS);
	assert(dataId >= 0);

	switch (mn.sharedData[dataId].type) {
	case MN_SHARED_LINKEDLISTTEXT:
		LIST_Delete(&mn.sharedData[dataId].data.linkedListText);
		break;
	case MN_SHARED_OPTION:
		if (_Mem_AllocatedInPool(com_genericPool, mn.sharedData[dataId].data.option)) {
			MN_DeleteOption(mn.sharedData[dataId].data.option);
		}
		break;
	default:
		break;
	}

	mn.sharedData[dataId].type = MN_SHARED_NONE;
	mn.sharedData[dataId].data.text = NULL;
	mn.sharedData[dataId].versionId++;
}

/**
 * @brief Remove the higher element (in alphabet) from a list
 * @todo option should start with '_' if we need to translate it
 * @warning update parent
 */
static menuNode_t *MN_OptionNodeRemoveHigherOption (menuNode_t **option)
{
	menuNode_t *prev = *option;
	menuNode_t *prevfind = NULL;
	menuNode_t *search = (*option)->next;
	const char *label = OPTIONEXTRADATA(*option).label;
#if 0
	if (label[0] == '_')
#endif
		label = _(label);

	/* search the smaller element */
	while (search) {
		const char *searchlabel = OPTIONEXTRADATA(search).label;
#if 0
		if (searchlabel[0] == '_')
#endif
			searchlabel = _(searchlabel);
		if (strcmp(label, searchlabel) < 0) {
			prevfind = prev;
			label = searchlabel;
		}
		prev = search;
		search = search->next;
	}

	/* remove the first element */
	if (prevfind == NULL) {
		menuNode_t *tmp = *option;
		*option = (*option)->next;
		return tmp;
	} else {
		menuNode_t *tmp = prevfind->next;
		prevfind->next = tmp->next;
		return tmp;
	}
}

/**
 * @brief Sort options by alphabet
 */
void MN_SortOptions (menuNode_t **first)
{
	menuNode_t *option;

	/* unlink the unsorted list */
	option = *first;
	if (option == NULL)
		return;
	*first = NULL;

	/* construct a sorted list */
	while (option) {
		menuNode_t *element;
		element = MN_OptionNodeRemoveHigherOption(&option);
		element->next = *first;
		*first = element;
	}
}

/**
 * @brief Unhide those options that are stored in the linked list and hide the others
 * @param[in,out] option Option list we want to update
 * @param[in] stringList List of option name (ID) we want to display
 */
void MN_UpdateInvisOptions (menuNode_t *option, const linkedList_t *stringList)
{
	if (option == NULL || stringList == NULL)
		return;

	while (option) {
		if (LIST_ContainsString(stringList, option->name))
			option->invis = qfalse;
		else
			option->invis = qtrue;
		option = option->next;
	}
}

void MN_RegisterOption (int dataId, menuNode_t *option)
{
	/** Hack to disable release option memory, if we only want to update the same option */
	if (mn.sharedData[dataId].type == MN_SHARED_OPTION && mn.sharedData[dataId].data.option == option) {
		mn.sharedData[dataId].versionId++;
		return;
	}
	MN_ResetData(dataId);
	mn.sharedData[dataId].type = MN_SHARED_OPTION;
	mn.sharedData[dataId].data.option = option;
	mn.sharedData[dataId].versionId++;
}

void MN_RegisterLineStrip (int dataId, lineStrip_t *lineStrip)
{
	MN_ResetData(dataId);
	mn.sharedData[dataId].type = MN_SHARED_LINESTRIP;
	mn.sharedData[dataId].data.lineStrip = lineStrip;
	mn.sharedData[dataId].versionId++;
}

menuNode_t *MN_GetOption (int dataId)
{
	if (mn.sharedData[dataId].type == MN_SHARED_OPTION) {
		return mn.sharedData[dataId].data.option;
	}
	return NULL;
}

/**
 * @brief find an option why index (0 is the first option)
 * @param[in] index Requested index (0 is the first option)
 * @param[in] option First element of options (it can be a tree)
 * @param[in,out] iterator need an initialised iterator, and update it into the write index
 */
static menuNode_t* MN_FindOptionAtIndex (int index, menuNode_t* option, menuOptionIterator_t* iterator)
{
	while (option) {
		assert(option->behaviour == optionBehaviour);
		if (option->invis) {
			option = option->next;
			continue;
		}

		/* we are on the right element */
		if (index == 0) {
			iterator->option = option;
			return option;
		}

		/* not the parent */
		index--;

		if (OPTIONEXTRADATA(option).collapsed) {
			option = option->next;
			continue;
		}

		/* its a child */
		if (index < OPTIONEXTRADATA(option).childCount) {
			if (iterator->depthPos >= MAX_DEPTH_OPTIONITERATORCACHE)
				assert(qfalse);
			iterator->depthCache[iterator->depthPos] = option;
			iterator->depthPos++;
			return MN_FindOptionAtIndex(index, option->firstChild, iterator);
		}
		index -= OPTIONEXTRADATA(option).childCount;
		option = option->next;
	}

	iterator->option = NULL;
	return NULL;
}

/**
 * @brief Init an option iterator at an index
 * @note invis option are skipped, and child are counted
 * @param[in] index Requested index (0 is the first option)
 * @param[in] option First element of options (it can be a tree)
 * @param[out] iterator Initialised iterator
 * @return the first option element found (current position of the iterator)
 * @code
 * menuOptionIterator_t iterator;
 * MN_InitOptionIteratorAtIndex(index, firstOption, &iterator);	// also return the option
 * while (iterator.option) {
 *     ...
 *     MN_OptionIteratorNextOption(&iterator);	// also return the option
 * }
 * @endcode
 * @todo Rework that code, we should split "Init" and "AtIndex"
 */
menuNode_t* MN_InitOptionIteratorAtIndex (int index, menuNode_t* option, menuOptionIterator_t* iterator)
{
	assert(option->behaviour == optionBehaviour);
	memset(iterator, 0, sizeof(*iterator));
	iterator->skipCollapsed = qtrue;
	iterator->skipInvisible = qtrue;
	return MN_FindOptionAtIndex(index, option, iterator);
}

/**
 * @brief Find the next element from the iterator
 * Iterator skipCollapsed and skipInvisible attribute can control the option flow
 */
menuNode_t* MN_OptionIteratorNextOption (menuOptionIterator_t* iterator)
{
	menuNode_t* option;

	option = iterator->option;
	assert(iterator->depthPos < MAX_DEPTH_OPTIONITERATORCACHE);
	iterator->depthCache[iterator->depthPos] = option;
	iterator->depthPos++;

	if (OPTIONEXTRADATA(option).collapsed && iterator->skipCollapsed)
		option = NULL;
	else
		option = option->firstChild;

	while (qtrue) {
		while (option) {
			if (!option->invis || !iterator->skipInvisible) {
				iterator->option = option;
				return option;
			}
			option = option->next;
		}
		if (iterator->depthPos == 0)
			break;
		iterator->depthPos--;
		option = iterator->depthCache[iterator->depthPos]->next;
	}

	iterator->option = NULL;
	return NULL;
}

/**
 * @brief Find an option (and all his parents) by is value.
 * @param[in,out] iterator If it found an option, the iterator contain all option parent
 * @param[in] value The value we search
 * @return The right option, else NULL
 */
menuNode_t* MN_FindOptionByValue (menuOptionIterator_t* iterator, const char* value)
{
	while (iterator->option) {
		assert(iterator->option->behaviour == optionBehaviour);
		if (!strcmp(OPTIONEXTRADATA(iterator->option).value, value))
			return iterator->option;
		MN_OptionIteratorNextOption(iterator);
	}
	return NULL;
}

/**
 * @brief Find an option position from an option iterator
 * @param[in,out] iterator Context of the iteration. If it found an option, the iterator contain all option parent
 * @param[in] option The value we search
 * @return The option index, else -1
 */
int MN_FindOptionPosition (menuOptionIterator_t* iterator, const menuNode_t* option)
{
	int i = 0;
	while (iterator->option) {
		if (iterator->option == option)
			return i;
		i++;
		MN_OptionIteratorNextOption(iterator);
	}
	return -1;
}

/**
 * @brief Resets the mn.menuText pointers from a func node
 * @note You can give this function a parameter to only delete a specific data
 * @sa menutextid_names
 */
static void MN_ResetData_f (void)
{
	if (Cmd_Argc() == 2) {
		const char *menuTextID = Cmd_Argv(1);
		const int id = MN_GetDataIDByName(menuTextID);
		if (id < 0)
			Com_Printf("%s: invalid mn.menuText ID: %s\n", Cmd_Argv(0), menuTextID);
		else
			MN_ResetData(id);
	} else {
		int i;
		for (i = 0; i < MAX_MENUTEXTS; i++)
			MN_ResetData(i);
	}
}

/**
 * @brief Initialize console command about shared menu data
 * @note called by MN_Init
 */
void MN_InitData (void)
{
	Cmd_AddCommand("mn_datareset", MN_ResetData_f, "Resets a menu data pointers");
}
