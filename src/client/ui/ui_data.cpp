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

#include "../cl_shared.h"
#include "../cl_language.h"
#include "ui_main.h"
#include "ui_internal.h"
#include "ui_nodes.h"
#include "node/ui_node_linechart.h"
#include "node/ui_node_option.h"

#include "../cl_language.h"

/**
 * @brief
 */
static const char* const ui_sharedDataIDNames[] = {
	"", /**< nullptr value */
	STRINGIFY(TEXT_STANDARD),
	STRINGIFY(TEXT_LIST),
	STRINGIFY(TEXT_LIST2),
	STRINGIFY(TEXT_UFOPEDIA),
	STRINGIFY(TEXT_UFOPEDIA_REQUIREMENT),
	STRINGIFY(TEXT_BUILDINGS),
	STRINGIFY(TEXT_BUILDING_INFO),
	STRINGIFY(TEXT_RESEARCH),
	STRINGIFY(TEXT_POPUP),
	STRINGIFY(TEXT_POPUP_INFO),
	STRINGIFY(TEXT_AIRCRAFT_LIST),
	STRINGIFY(TEXT_AIRCRAFT_INFO),
	STRINGIFY(TEXT_MULTISELECTION),
	STRINGIFY(TEXT_PRODUCTION_LIST),
	STRINGIFY(TEXT_PRODUCTION_AMOUNT),
	STRINGIFY(TEXT_PRODUCTION_INFO),
	STRINGIFY(TEXT_EMPLOYEE),
	STRINGIFY(TEXT_MOUSECURSOR_RIGHT),
	STRINGIFY(TEXT_PRODUCTION_QUEUED),
	STRINGIFY(TEXT_STATS_MISSION),
	STRINGIFY(TEXT_STATS_BASES),
	STRINGIFY(TEXT_STATS_NATIONS),
	STRINGIFY(TEXT_STATS_EMPLOYEES),
	STRINGIFY(TEXT_STATS_COSTS),
	STRINGIFY(TEXT_STATS_INSTALLATIONS),
	STRINGIFY(TEXT_STATS_7),
	STRINGIFY(TEXT_BASE_LIST),
	STRINGIFY(TEXT_BASE_INFO),
	STRINGIFY(TEXT_TRANSFER_LIST),
	STRINGIFY(TEXT_TRANSFER_LIST_AMOUNT),
	STRINGIFY(TEXT_TRANSFER_LIST_TRANSFERED),
	STRINGIFY(TEXT_MOUSECURSOR_PLAYERNAMES),
	STRINGIFY(TEXT_CARGO_LIST),
	STRINGIFY(TEXT_CARGO_LIST_AMOUNT),
	STRINGIFY(TEXT_UFOPEDIA_MAILHEADER),
	STRINGIFY(TEXT_UFOPEDIA_MAIL),
	STRINGIFY(TEXT_MARKET_NAMES),
	STRINGIFY(TEXT_MARKET_STORAGE),
	STRINGIFY(TEXT_MARKET_MARKET),
	STRINGIFY(TEXT_MARKET_PRICES),
	STRINGIFY(TEXT_CHAT_WINDOW),
	STRINGIFY(TEXT_AIREQUIP_1),
	STRINGIFY(TEXT_AIREQUIP_2),
	STRINGIFY(TEXT_BASEDEFENCE_LIST),
	STRINGIFY(TEXT_TIPOFTHEDAY),
	STRINGIFY(TEXT_GENERIC),
	STRINGIFY(TEXT_XVI),
	STRINGIFY(TEXT_MOUSECURSOR_TOP),
	STRINGIFY(TEXT_MOUSECURSOR_BOTTOM),
	STRINGIFY(TEXT_MOUSECURSOR_LEFT),
	STRINGIFY(TEXT_MESSAGEOPTIONS),
	STRINGIFY(TEXT_UFORECOVERY_NATIONS),
	STRINGIFY(TEXT_UFORECOVERY_UFOYARDS),
	STRINGIFY(TEXT_UFORECOVERY_CAPACITIES),
	STRINGIFY(TEXT_MATERIAL_STAGES),
	STRINGIFY(TEXT_IRCCONTENT),
	STRINGIFY(TEXT_IRCUSERS),
	STRINGIFY(TEXT_MULTIPLAYER_USERLIST),
	STRINGIFY(TEXT_MULTIPLAYER_USERTEAM),
	STRINGIFY(TEXT_ITEMDESCRIPTION),
	STRINGIFY(TEXT_MISSIONBRIEFING),
	STRINGIFY(TEXT_MISSIONBRIEFING_TITLE),
	STRINGIFY(TEXT_MISSIONBRIEFING_VICTORY_CONDITIONS),

	STRINGIFY(OPTION_LANGUAGES),
	STRINGIFY(OPTION_JOYSTICKS),
	STRINGIFY(OPTION_VIDEO_RESOLUTIONS),
	STRINGIFY(OPTION_SINGLEPLAYER_SKINS),
	STRINGIFY(OPTION_MULTIPLAYER_SKINS),
	STRINGIFY(OPTION_UFOPEDIA),
	STRINGIFY(OPTION_UFOS),
	STRINGIFY(OPTION_DROPSHIPS),
	STRINGIFY(OPTION_BASELIST),
	STRINGIFY(OPTION_TEAMDEFS),
	STRINGIFY(OPTION_PRODUCTION_REQUIREMENTS),
	STRINGIFY(OPTION_CAMPAIGN_LIST),

	STRINGIFY(LINESTRIP_FUNDING),
	STRINGIFY(LINESTRIP_COLOR)
};
CASSERT(lengthof(ui_sharedDataIDNames) == UI_MAX_DATAID);

/**
 * @brief Return a dataId by name
 * @return A dataId if data found, else -1
 */
int UI_GetDataIDByName (const char* name)
{
	for (int num = 0; num < UI_MAX_DATAID; num++)
		if (Q_streq(name, ui_sharedDataIDNames[num]))
			return num;

	return -1;
}

/**
 * @brief share a text with a data id
 * @note The UI code doesn't manage the text memory, it only save a pointer
 */
void UI_RegisterText (int dataId, const char* text)
{
	UI_ResetData(dataId);

	if (!text)
		return;

	ui_global.sharedData[dataId].type = UI_SHARED_TEXT;
	ui_global.sharedData[dataId].data.text = text;
	ui_global.sharedData[dataId].versionId++;
}

/**
 * @brief share a linked list of text with a data id
 * @note The UI code manage the linked list memory (linked list is freed by the UI code)
 */
void UI_RegisterLinkedListText (int dataId, linkedList_t* text)
{
	/** @todo FIXME It is a hack to disable release memory, if we only want to update the same list */
	if (ui_global.sharedData[dataId].type == UI_SHARED_LINKEDLISTTEXT && ui_global.sharedData[dataId].data.linkedListText == text) {
		ui_global.sharedData[dataId].versionId++;
		return;
	}
	UI_ResetData(dataId);
	ui_global.sharedData[dataId].type = UI_SHARED_LINKEDLISTTEXT;
	ui_global.sharedData[dataId].data.linkedListText = text;
	ui_global.sharedData[dataId].versionId++;
}

const char* UI_GetText (int textId)
{
	if (ui_global.sharedData[textId].type != UI_SHARED_TEXT)
		return nullptr;
	return CL_Translate(ui_global.sharedData[textId].data.text);
}

const char* UI_GetTextFromList (int textId, int line)
{
	if (ui_global.sharedData[textId].type != UI_SHARED_LINKEDLISTTEXT)
		return nullptr;
	linkedList_t* list = ui_global.sharedData[textId].data.linkedListText;
	return static_cast<const char*>(LIST_GetByIdx(list, line));
}

int UI_GetDataVersion (int textId)
{
	return ui_global.sharedData[textId].versionId;
}

/**
 * @brief Append an option to an option list.
 * @param[in,out] tree first option of the list/tree of options
 * @param[in] name name of the option (should be unique in the option list)
 * @param[in] label label displayed
 * @param[in] value value used when this option is selected
 * @return The new option
 */
uiNode_t* UI_AddOption (uiNode_t** tree, const char* name, const char* label, const char* value)
{
	uiNode_t* last;
	uiNode_t* option;
	assert(tree != nullptr);

	option = UI_AllocOptionNode(name, label, value);

	/* append the option */
	last = *tree;
	if (last != nullptr) {
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
 * @param tree Root of nodes we want to delete
 */
static void UI_DeleteOption (uiNode_t* tree)
{
	while (tree) {
		uiNode_t* del = tree;
		tree = tree->next;
		UI_DeleteNode(del);
	}
}

/**
 * @brief Reset a shared data. Type became NONE and value became nullptr
 */
void UI_ResetData (int dataId)
{
	assert(dataId < UI_MAX_DATAID);
	assert(dataId >= 0);

	switch (ui_global.sharedData[dataId].type) {
	case UI_SHARED_LINKEDLISTTEXT:
		LIST_Delete(&ui_global.sharedData[dataId].data.linkedListText);
		break;
	case UI_SHARED_OPTION:
		if (_Mem_AllocatedInPool(com_genericPool, ui_global.sharedData[dataId].data.option)) {
			UI_DeleteOption(ui_global.sharedData[dataId].data.option);
		}
		break;
	default:
		break;
	}

	ui_global.sharedData[dataId].type = UI_SHARED_NONE;
	ui_global.sharedData[dataId].data.text = nullptr;
	ui_global.sharedData[dataId].versionId++;
}

/**
 * @brief Remove the higher element (in alphabet) from a list
 * @todo option should start with '_' if we need to translate it
 * @warning update parent
 */
static uiNode_t* UI_OptionNodeRemoveHigherOption (uiNode_t** option)
{
	uiNode_t* prev = *option;
	uiNode_t* prevfind = nullptr;
	uiNode_t* search = (*option)->next;
	const char* label = CL_Translate(OPTIONEXTRADATA(*option).label);

	/* search the smaller element */
	while (search) {
		const char* searchlabel = CL_Translate(OPTIONEXTRADATA(search).label);
		if (strcmp(label, searchlabel) < 0) {
			prevfind = prev;
			label = searchlabel;
		}
		prev = search;
		search = search->next;
	}

	/* remove the first element */
	if (prevfind == nullptr) {
		uiNode_t* tmp = *option;
		*option = (*option)->next;
		return tmp;
	} else {
		uiNode_t* tmp = prevfind->next;
		prevfind->next = tmp->next;
		return tmp;
	}
}

/**
 * @brief Sort options by alphabet
 */
void UI_SortOptions (uiNode_t** first)
{
	uiNode_t* option;

	/* unlink the unsorted list */
	option = *first;
	if (option == nullptr)
		return;
	*first = nullptr;

	/* construct a sorted list */
	while (option) {
		uiNode_t* element;
		element = UI_OptionNodeRemoveHigherOption(&option);
		element->next = *first;
		*first = element;
	}
}

/**
 * @brief Unhide those options that are stored in the linked list and hide the others
 * @param[in,out] option Option list we want to update
 * @param[in] stringList List of option name (ID) we want to display
 */
void UI_UpdateInvisOptions (uiNode_t* option, const linkedList_t* stringList)
{
	if (option == nullptr || stringList == nullptr)
		return;

	while (option) {
		if (LIST_ContainsString(stringList, option->name))
			option->invis = false;
		else
			option->invis = true;
		option = option->next;
	}
}

void UI_RegisterOption (int dataId, uiNode_t* option)
{
	/** Hack to disable release option memory, if we only want to update the same option */
	if (ui_global.sharedData[dataId].type == UI_SHARED_OPTION && ui_global.sharedData[dataId].data.option == option) {
		ui_global.sharedData[dataId].versionId++;
		return;
	}
	UI_ResetData(dataId);
	ui_global.sharedData[dataId].type = UI_SHARED_OPTION;
	ui_global.sharedData[dataId].data.option = option;
	ui_global.sharedData[dataId].versionId++;
}

void UI_RegisterLineStrip (int dataId, lineStrip_t* lineStrip)
{
	UI_ResetData(dataId);
	ui_global.sharedData[dataId].type = UI_SHARED_LINESTRIP;
	ui_global.sharedData[dataId].data.lineStrip = lineStrip;
	ui_global.sharedData[dataId].versionId++;
}

uiNode_t* UI_GetOption (int dataId)
{
	if (ui_global.sharedData[dataId].type == UI_SHARED_OPTION) {
		return ui_global.sharedData[dataId].data.option;
	}
	return nullptr;
}

/**
 * @brief find an option why index (0 is the first option)
 * @param[in] index Requested index (0 is the first option)
 * @param[in] option First element of options (it can be a tree)
 * @param[in,out] iterator need an initialised iterator, and update it into the write index
 */
static uiNode_t* UI_FindOptionAtIndex (int index, uiNode_t* option, uiOptionIterator_t* iterator)
{
	while (option) {
		assert(option->behaviour == ui_optionBehaviour);
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
				assert(false);
			iterator->depthCache[iterator->depthPos] = option;
			iterator->depthPos++;
			return UI_FindOptionAtIndex(index, option->firstChild, iterator);
		}
		index -= OPTIONEXTRADATA(option).childCount;
		option = option->next;
	}

	iterator->option = nullptr;
	return nullptr;
}

/**
 * @brief Init an option iterator at an index
 * @note invis option are skipped, and child are counted
 * @param[in] index Requested index (0 is the first option)
 * @param[in] option First element of options (it can be a tree)
 * @param[out] iterator Initialised iterator
 * @return the first option element found (current position of the iterator)
 * @code
 * uiOptionIterator_t iterator;
 * UI_InitOptionIteratorAtIndex(index, firstOption, &iterator);	// also return the option
 * while (iterator.option) {
 *     ...
 *     UI_OptionIteratorNextOption(&iterator);	// also return the option
 * }
 * @endcode
 * @todo Rework that code, we should split "Init" and "AtIndex"
 */
uiNode_t* UI_InitOptionIteratorAtIndex (int index, uiNode_t* option, uiOptionIterator_t* iterator)
{
	assert(option == nullptr || option->behaviour == ui_optionBehaviour);
	OBJZERO(*iterator);
	iterator->skipCollapsed = true;
	iterator->skipInvisible = true;
	return UI_FindOptionAtIndex(index, option, iterator);
}

/**
 * @brief Find the next element from the iterator
 * Iterator skipCollapsed and skipInvisible attribute can control the option flow
 */
uiNode_t* UI_OptionIteratorNextOption (uiOptionIterator_t* iterator)
{
	uiNode_t* option;

	option = iterator->option;
	assert(iterator->depthPos < MAX_DEPTH_OPTIONITERATORCACHE);
	iterator->depthCache[iterator->depthPos] = option;
	iterator->depthPos++;

	if (OPTIONEXTRADATA(option).collapsed && iterator->skipCollapsed)
		option = nullptr;
	else
		option = option->firstChild;

	while (true) {
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

	iterator->option = nullptr;
	return nullptr;
}

/**
 * @brief Find an option (and all his parents) by is value.
 * @param[in,out] iterator If it found an option, the iterator contain all option parent
 * @param[in] value The value we search
 * @return The right option, else nullptr
 */
uiNode_t* UI_FindOptionByValue (uiOptionIterator_t* iterator, const char* value)
{
	while (iterator->option) {
		assert(iterator->option->behaviour == ui_optionBehaviour);
		if (Q_streq(OPTIONEXTRADATA(iterator->option).value, value))
			return iterator->option;
		UI_OptionIteratorNextOption(iterator);
	}
	return nullptr;
}

/**
 * @brief Find an option position from an option iterator
 * @param[in,out] iterator Context of the iteration. If it found an option, the iterator contain all option parent
 * @param[in] option The value we search
 * @return The option index, else -1
 */
int UI_FindOptionPosition (uiOptionIterator_t* iterator, const uiNode_t* option)
{
	int i = 0;
	while (iterator->option) {
		if (iterator->option == option)
			return i;
		i++;
		UI_OptionIteratorNextOption(iterator);
	}
	return -1;
}

/**
 * @brief Resets the ui_global.sharedData pointers from a func node
 * @note You can give this function a parameter to only delete a specific data
 * @sa ui_sharedDataIDNames
 */
static void UI_ResetData_f (void)
{
	if (Cmd_Argc() == 2) {
		const char* dataId = Cmd_Argv(1);
		const int id = UI_GetDataIDByName(dataId);
		if (id == -1)
			Com_Printf("%s: invalid data ID: %s\n", Cmd_Argv(0), dataId);
		else
			UI_ResetData(id);
	} else {
		for (int i = 0; i < UI_MAX_DATAID; i++)
			UI_ResetData(i);
	}
}

/**
 * @brief Initialize console command about UI shared data
 * @note called by UI_Init
 */
void UI_InitData (void)
{
	Cmd_AddCommand("ui_resetdata", UI_ResetData_f, "Resets memory and data used by a UI data id");
}
