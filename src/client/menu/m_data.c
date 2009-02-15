/**
 * @file m_data.c
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

#include "m_main.h"
#include "m_internal.h"
#include "m_draw.h"
#include "m_input.h"
#include "m_timer.h"
#include "m_condition.h"
#include "../client.h"

/**
 * @brief
 */
static const char *const menutextid_names[] = {
	"", /**< NULL value */
	"TEXT_STANDARD", "TEXT_LIST", "TEXT_UFOPEDIA", "TEXT_BUILDINGS", "TEXT_BUILDING_INFO",
	"TEXT_RESEARCH", "TEXT_POPUP", "TEXT_POPUP_INFO", "TEXT_AIRCRAFT_LIST",
	"TEXT_AIRCRAFT_INFO", "TEXT_MESSAGESYSTEM", "TEXT_CAMPAIGN_LIST",
	"TEXT_MULTISELECTION", "TEXT_PRODUCTION_LIST", "TEXT_PRODUCTION_AMOUNT", "TEXT_PRODUCTION_INFO",
	"TEXT_EMPLOYEE", "TEXT_MOUSECURSOR_RIGHT", "TEXT_PRODUCTION_QUEUED", "TEXT_STATS_BASESUMMARY",
	"TEXT_STATS_MISSION", "TEXT_STATS_BASES", "TEXT_STATS_NATIONS", "TEXT_STATS_EMPLOYEES", "TEXT_STATS_COSTS",
	"TEXT_STATS_INSTALLATIONS", "TEXT_STATS_7", "TEXT_BASE_LIST", "TEXT_BASE_INFO",
	"TEXT_TRANSFER_LIST", "TEXT_MOUSECURSOR_PLAYERNAMES",
	"TEXT_CARGO_LIST", "TEXT_UFOPEDIA_MAILHEADER", "TEXT_UFOPEDIA_MAIL", "TEXT_MARKET_NAMES",
	"TEXT_MARKET_STORAGE", "TEXT_MARKET_MARKET", "TEXT_MARKET_PRICES", "TEXT_CHAT_WINDOW",
	"TEXT_AIREQUIP_1", "TEXT_AIREQUIP_2", "TEXT_BASEDEFENCE_LIST", "TEXT_TIPOFTHEDAY",
	"TEXT_GENERIC", "TEXT_XVI", "TEXT_MOUSECURSOR_TOP", "TEXT_MOUSECURSOR_BOTTOM", "TEXT_MOUSECURSOR_LEFT", "TEXT_MESSAGEOPTIONS",
	"OPTION_LANGUAGES", "OPTION_JOYSTICKS", "OPTION_VIDEO_RESOLUTIONS"
};
CASSERT(lengthof(menutextid_names) == MAX_MENUTEXTS);

/**
 * @brief Return a dataId by name
 * @return A dataId if data found, else -1
 */
int MN_GetIdByName (const char* name)
{
	int num;
	for (num = 0; num < MAX_MENUTEXTS; num++)
		if (!Q_strcmp(name, menutextid_names[num]))
			return num;

	return -1;
}

/**
 * @brief link a text to a menu text id
 * @note The menu dont manage the text memory, only save a pointer
 */
void MN_RegisterText(int textId, const char *text)
{
	mn.sharedData[textId].type = MN_SHARED_TEXT;
	mn.sharedData[textId].data.text = text;
	mn.sharedData[textId].versionId++;
}

/**
 * @brief link a text to a menu text id
 * @note The menu dont manage the text memory, only save a pointer
 */
void MN_RegisterLinkedListText(int textId, linkedList_t *text)
{
	mn.sharedData[textId].type = MN_SHARED_LINKEDLISTTEXT;
	mn.sharedData[textId].data.linkedListText = text;
	mn.sharedData[textId].versionId++;
}

const char *MN_GetText(int textId)
{
	if (mn.sharedData[textId].type != MN_SHARED_TEXT)
		return NULL;
	return mn.sharedData[textId].data.text;
}

int MN_GetDataVersion(int textId) {
	return mn.sharedData[textId].versionId;
}

/**
 * @brief Resets the mn.menuText pointers and the mn.menuTextLinkedList lists
 * @todo Move it out of this node
 */
void MN_MenuTextReset (int menuTextID)
{
	assert(menuTextID < MAX_MENUTEXTS);
	assert(menuTextID >= 0);

	if (mn.sharedData[menuTextID].type == MN_SHARED_LINKEDLISTTEXT)
		LIST_Delete(&mn.sharedData[menuTextID].data.linkedListText);

	mn.sharedData[menuTextID].type = MN_SHARED_NONE;
	mn.sharedData[menuTextID].data.text = NULL;
	mn.sharedData[menuTextID].versionId++;
}

/**
 * @brief Remove the highter element (in alphabet) from a list
 */
static menuOption_t *MN_OptionNodeRemoveHighterOption (menuOption_t **option)
{
	menuOption_t *prev = NULL;
	menuOption_t *prevfind = NULL;
	menuOption_t *search = (*option)->next;
	char *label = (*option)->label;

	/* search the smaller element */
	while (search) {
		if (Q_strcmp(label, search->label) < 0) {
			prevfind = prev;
			label = search->label;
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
		element = MN_OptionNodeRemoveHighterOption(&option);
		element->next = *first;
		*first = element;
	}
}

void MN_RegisterOption (int dataId, menuOption_t *option)
{
	mn.sharedData[dataId].type = MN_SHARED_OPTION;
	mn.sharedData[dataId].data.option = option;
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
 * @brief Alloc an array of option
 */
menuOption_t* MN_AllocOption (int count)
{
	menuOption_t* newOptions;
	assert(count > 0);

	if (mn.numOptions + count >= MAX_MENUOPTIONS) {
		Com_Printf("MN_AllocOption: numOptions exceeded - increase MAX_MENUOPTIONS\n");
		return NULL;
	}
	newOptions = &mn.menuOptions[mn.numOptions];
	mn.numOptions += count;
	return newOptions;
}
