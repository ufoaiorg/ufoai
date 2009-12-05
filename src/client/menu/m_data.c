/**
 * @file m_data.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
 * @brief Append an option to an option list.
 * @param[in,out] tree first option of the list/tree of options
 * @param[in] name name of the option (should be unique in the option list)
 * @param[in] label label displayed
 * @param[in] value value used when this option is selected
 * @return The new option
 */
menuOption_t* MN_AddOption (menuOption_t**tree, const char* name, const char* label, const char* value)
{
	menuOption_t *last;
	menuOption_t *option;
	assert(tree != NULL);

	option = (menuOption_t*)Mem_PoolAlloc(sizeof(*option), com_genericPool, 0);
	MN_InitOption(option, name, label, value);

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

void MN_DeleteOption (menuOption_t* tree)
{
	while (tree) {
		menuOption_t* del = tree;
		tree = tree->next;
		if (del->firstChild)
			MN_DeleteOption(del->firstChild);
		Mem_Free(del);
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
 */
static menuOption_t *MN_OptionNodeRemoveHigherOption (menuOption_t **option)
{
	menuOption_t *prev = *option;
	menuOption_t *prevfind = NULL;
	menuOption_t *search = (*option)->next;
	const char *label = (*option)->label;
#if 0
	if (label[0] == '_')
#endif
		label = _(label);

	/* search the smaller element */
	while (search) {
		const char *searchlabel = search->label;
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
		menuOption_t *tmp = *option;
		*option = (*option)->next;
		return tmp;
	} else {
		menuOption_t *tmp = prevfind->next;
		prevfind->next = tmp->next;
		return tmp;
	}
}

/**
 * @brief Sort options by alphabet
 */
void MN_SortOptions (menuOption_t **first)
{
	menuOption_t *option;

	/* unlink the unsorted list */
	option = *first;
	if (option == NULL)
		return;
	*first = NULL;

	/* construct a sorted list */
	while (option) {
		menuOption_t *element;
		element = MN_OptionNodeRemoveHigherOption(&option);
		element->next = *first;
		*first = element;
	}
}

/**
 * @brief Unhide those options that are stored in the linked list and hide the others
 */
void MN_UpdateInvisOptions (menuOption_t *option, const linkedList_t *stringList)
{
	if (option == NULL || stringList == NULL)
		return;

	while (option) {
		if (LIST_ContainsString(stringList, option->label))
			option->invis = qfalse;
		else
			option->invis = qtrue;
		option = option->next;
	}
}

/**
 * @brief Initializes an option with a very little set of values.
 * @param[in] option Context option
 * @param[in] nameID name of the option (should be unique in the option list)
 * @param[in] label label displayed
 * @param[in] value value used when this option is selected
 */
void MN_InitOption (menuOption_t* option, const char* nameID, const char* label, const char* value)
{
	assert(option);
	memset(option, 0, sizeof(*option));
	Q_strncpyz(option->id, nameID, sizeof(option->id));
	Q_strncpyz(option->label, label, sizeof(option->label));
	Q_strncpyz(option->value, value, sizeof(option->value));
}

void MN_RegisterOption (int dataId, menuOption_t *option)
{
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

menuOption_t *MN_GetOption (int dataId)
{
	if (mn.sharedData[dataId].type == MN_SHARED_OPTION) {
		return mn.sharedData[dataId].data.option;
	}
	return NULL;
}

/**
 * @brief Allocates an array of option
 */
menuOption_t* MN_AllocStaticOption (int count)
{
	menuOption_t* newOptions;
	assert(count > 0);

	if (mn.numOptions + count >= MAX_MENUOPTIONS)
		Com_Error(ERR_FATAL, "MN_AllocStaticOption: numOptions exceeded - increase MAX_MENUOPTIONS");

	newOptions = &mn.options[mn.numOptions];
	mn.numOptions += count;
	return newOptions;
}

/**
 * @brief update option cache about child, according to collapse and visible status
 * @note can be a common function for all option node
 * @return number of visible elements
 */
int MN_OptionUpdateCache (menuOption_t* option)
{
	int count = 0;
	while (option) {
		int localCount = 0;
		if (option->invis) {
			option = option->next;
			continue;
		}
		if (option->collapsed) {
			option->childCount = 0;
			option = option->next;
			count++;
			continue;
		}
		if (option->firstChild)
			localCount = MN_OptionUpdateCache(option->firstChild);
		option->childCount = localCount;
		count += 1 + localCount;
		option = option->next;
	}
	return count;
}


/**
 * @brief find an option why index (0 is the first option)
 * @param[in] index Requested index (0 is the first option)
 * @param[in] option First element of options (it can be a tree)
 * @param[in,out] iterator need an initialised iterator, and update it into the write index
 */
static menuOption_t* MN_FindOptionAtIndex (int index, menuOption_t* option, menuOptionIterator_t* iterator)
{
	while (option) {
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

		if (option->collapsed) {
			option = option->next;
			continue;
		}

		/* its a child */
		if (index < option->childCount) {
			if (iterator->depthPos >= MAX_DEPTH_OPTIONITERATORCACHE)
				assert(qfalse);
			iterator->depthCache[iterator->depthPos] = option;
			iterator->depthPos++;
			return MN_FindOptionAtIndex(index, option->firstChild, iterator);
		}
		index -= option->childCount;
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
 */
menuOption_t* MN_InitOptionIteratorAtIndex (int index, menuOption_t* option, menuOptionIterator_t* iterator)
{
	memset(iterator, 0, sizeof(*iterator));
	return MN_FindOptionAtIndex(index, option, iterator);
}

/**
 * @brief Find the next element from the iterator
 */
menuOption_t* MN_OptionIteratorNextOption (menuOptionIterator_t* iterator)
{
	menuOption_t* option;

	option = iterator->option;
	assert(iterator->depthPos < MAX_DEPTH_OPTIONITERATORCACHE);
	iterator->depthCache[iterator->depthPos] = option;
	iterator->depthPos++;

	if (option->collapsed)
		option = NULL;
	else
		option = option->firstChild;

	while (qtrue) {
		while (option) {
			if (!option->invis) {
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
 * @brief Link an option array together (first element to second, second to third...)
 */
void MN_OptionLinkArray (menuOption_t* array, int count)
{
	menuOption_t *prev = NULL;
	menuOption_t *current = array;
	while (count) {
		if (prev != NULL)
			prev->next = current;
		prev = current;
		current++;
		count--;
	}
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
