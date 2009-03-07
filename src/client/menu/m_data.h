/**
 * @file m_data.h
 * @brief Data and interface to share data
 * @todo clean up the interface
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

#ifndef CLIENT_MENU_M_DATA_H
#define CLIENT_MENU_M_DATA_H

/* prototype */
struct linkedList_s;
struct menuOption_s;

/** @brief linked into mn.menuText - defined in menu scripts via num */
typedef enum {
	TEXT_NULL,		/**< default value, should not be used */
	TEXT_STANDARD,
	TEXT_LIST,
	TEXT_UFOPEDIA,
	TEXT_BUILDINGS,
	TEXT_BUILDING_INFO,
	TEXT_RESEARCH,
	TEXT_POPUP,
	TEXT_POPUP_INFO,
	TEXT_AIRCRAFT_LIST,
	TEXT_AIRCRAFT_INFO,
	TEXT_MESSAGESYSTEM,			/**< just a dummy for messagesystem - we use the stack */
	TEXT_CAMPAIGN_LIST,
	TEXT_MULTISELECTION,
	TEXT_PRODUCTION_LIST,
	TEXT_PRODUCTION_AMOUNT,
	TEXT_PRODUCTION_INFO,
	TEXT_EMPLOYEE,
	TEXT_MOUSECURSOR_RIGHT,
	TEXT_PRODUCTION_QUEUED,
	TEXT_STATS_BASESUMMARY,
	TEXT_STATS_MISSION,
	TEXT_STATS_BASES,
	TEXT_STATS_NATIONS,
	TEXT_STATS_EMPLOYEES,
	TEXT_STATS_COSTS,
	TEXT_STATS_INSTALLATIONS,
	TEXT_STATS_7,
	TEXT_BASE_LIST,
	TEXT_BASE_INFO,
	TEXT_TRANSFER_LIST,
	TEXT_MOUSECURSOR_PLAYERNAMES,
	TEXT_CARGO_LIST, /* unused, why? */
	TEXT_UFOPEDIA_MAILHEADER,
	TEXT_UFOPEDIA_MAIL,
	TEXT_MARKET_NAMES,
	TEXT_MARKET_STORAGE,
	TEXT_MARKET_MARKET,
	TEXT_MARKET_PRICES,
	TEXT_CHAT_WINDOW,
	TEXT_AIREQUIP_1,
	TEXT_AIREQUIP_2,
	TEXT_BASEDEFENCE_LIST,
	TEXT_TIPOFTHEDAY,
	TEXT_GENERIC,
	TEXT_XVI,
	TEXT_MOUSECURSOR_TOP,
	TEXT_MOUSECURSOR_BOTTOM,
	TEXT_MOUSECURSOR_LEFT,
	TEXT_MESSAGEOPTIONS,
	TEXT_UFORECOVERY_NATIONS,
	TEXT_UFORECOVERY_BASESTORAGE,

	OPTION_LANGUAGES,
	OPTION_JOYSTICKS,
	OPTION_VIDEO_RESOLUTIONS,

	LINESTRIP_FUNDING,
	LINESTRIP_COLOR,

	MAX_MENUTEXTS
} menuTextIDs_t;

typedef enum {
	MN_SHARED_NONE = 0,
	MN_SHARED_TEXT,
	MN_SHARED_LINKEDLISTTEXT,
	MN_SHARED_OPTION,
	MN_SHARED_LINESTRIP
} menuSharedType_t;

typedef struct menuSharedData_s {
	menuSharedType_t type;		/**< Type of the shared data */
	union {
		/** @brief Holds static array of characters to display */
		const char *text;
		/** @brief Holds a linked list for displaying in the menu */
		struct linkedList_s *linkedListText;
		/** @brief Holds a linked list for option (label, action, icon...) */
		struct menuOption_s *option;
		/** @brief Holds a line strip, a list of point */
		struct lineStrip_s	*lineStrip;
	} data;						/**< The data */
	int versionId;				/**< Id identify the value, to check changes */
} menuSharedData_t;

/* common */
int MN_GetDataVersion(int textId) __attribute__ ((warn_unused_result));
void MN_ResetData(int dataId);
int MN_GetDataIDByName(const char* name) __attribute__ ((warn_unused_result));
void MN_InitData(void);

/* text */
void MN_RegisterText(int textId, const char *text);
const char *MN_GetText(int textId) __attribute__ ((warn_unused_result));

/* linked list */
void MN_RegisterLinkedListText(int textId, struct linkedList_s *text);

/* option */
struct menuOption_s* MN_AllocOption(int count) __attribute__ ((warn_unused_result));
void MN_RegisterOption(int dataId, struct menuOption_s *option);
struct menuOption_s *MN_GetOption(int dataId) __attribute__ ((warn_unused_result));
void MN_SortOptions(struct menuOption_s **option);

/* line strip */
void MN_RegisterLineStrip(int dataId, struct lineStrip_s *text);

#endif
