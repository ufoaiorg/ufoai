/**
 * @file ui_data.h
 * @brief Data and interface to share data
 * @todo clean up the interface
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

#ifndef CLIENT_UI_UI_DATA_H
#define CLIENT_UI_UI_DATA_H

#include "../../shared/ufotypes.h"
#include "../../shared/shared.h"
#include "ui_nodes.h"
#include "node/ui_node_option.h"

/* prototype */
struct linkedList_s;

/** @brief linked into ui_global.sharedData - defined in UI scripts via dataID property */
typedef enum {
	TEXT_NULL,		/**< default value, should not be used */
	TEXT_STANDARD,
	TEXT_LIST,
	TEXT_LIST2,
	TEXT_UFOPEDIA,
	TEXT_UFOPEDIA_REQUIREMENT,
	TEXT_BUILDINGS,
	TEXT_BUILDING_INFO,
	TEXT_RESEARCH,
	TEXT_POPUP,
	TEXT_POPUP_INFO,
	TEXT_AIRCRAFT_LIST,
	TEXT_AIRCRAFT_INFO,
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
	TEXT_TRANSFER_LIST_AMOUNT,
	TEXT_TRANSFER_LIST_TRANSFERED,
	TEXT_MOUSECURSOR_PLAYERNAMES,
	TEXT_CARGO_LIST,
	TEXT_CARGO_LIST_AMOUNT,
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
	TEXT_UFORECOVERY_UFOYARDS,
	TEXT_UFORECOVERY_CAPACITIES,
	TEXT_MATERIAL_STAGES,
	TEXT_IRCCONTENT,
	TEXT_IRCUSERS,
	TEXT_MULTIPLAYER_USERLIST,
	TEXT_MULTIPLAYER_USERTEAM,
	TEXT_ITEMDESCRIPTION,

	OPTION_LANGUAGES,
	OPTION_JOYSTICKS,
	OPTION_VIDEO_RESOLUTIONS,
	OPTION_SINGLEPLAYER_SKINS,
	OPTION_MULTIPLAYER_SKINS,
	OPTION_UFOPEDIA,
	OPTION_UFOS,
	OPTION_DROPSHIPS,
	OPTION_BASELIST,
	OPTION_TEAMDEFS,
	OPTION_PRODUCTION_REQUIREMENTS,
	OPTION_CAMPAIGN_LIST,

	LINESTRIP_FUNDING,
	LINESTRIP_COLOR,

	UI_MAX_DATAID
} uiTextIDs_t;

typedef enum {
	UI_SHARED_NONE = 0,
	UI_SHARED_TEXT,
	UI_SHARED_LINKEDLISTTEXT,
	UI_SHARED_OPTION,
	UI_SHARED_LINESTRIP
} uiSharedType_t;

typedef struct uiSharedData_s {
	uiSharedType_t type;		/**< Type of the shared data */
	union {
		/** @brief Holds static array of characters to display */
		const char *text;
		/** @brief Holds a linked list for displaying in the UI */
		struct linkedList_s *linkedListText;
		/** @brief Holds a linked list for option (label, action, icon...) */
		struct uiNode_s *option;
		/** @brief Holds a line strip, a list of point */
		struct lineStrip_s	*lineStrip;
	} data;						/**< The data */
	int versionId;				/**< Id identify the value, to check changes */
} uiSharedData_t;

#define MAX_DEPTH_OPTIONITERATORCACHE 8

typedef struct {
	uiNode_t* option;		/**< current option */
	uiNode_t* depthCache[MAX_DEPTH_OPTIONITERATORCACHE];	/**< parent link */
	int depthPos;				/**< current cache position */
	qboolean skipInvisible;		/**< skip invisible options when we iterate */
	qboolean skipCollapsed;		/**< skip collapsed options when we iterate */
} uiOptionIterator_t;

/* common */
int UI_GetDataVersion(int textId) __attribute__ ((warn_unused_result));
void UI_ResetData(int dataId);
int UI_GetDataIDByName(const char* name) __attribute__ ((warn_unused_result));
void UI_InitData(void);

/* text */
void UI_RegisterText(int textId, const char *text);
const char *UI_GetText(int textId) __attribute__ ((warn_unused_result));

/* linked list */
void UI_RegisterLinkedListText(int textId, struct linkedList_s *text);

/* option */
void UI_RegisterOption(int dataId, struct uiNode_s *option);
struct uiNode_s *UI_GetOption(int dataId) __attribute__ ((warn_unused_result));
void UI_SortOptions(struct uiNode_s **option);
struct uiNode_s* UI_InitOptionIteratorAtIndex(int index, struct uiNode_s* option, uiOptionIterator_t* iterator);
struct uiNode_s* UI_OptionIteratorNextOption(uiOptionIterator_t* iterator);
void UI_UpdateInvisOptions(struct uiNode_s *option, const struct linkedList_s *stringList);
struct uiNode_s* UI_FindOptionByValue(uiOptionIterator_t* iterator, const char* value);
int UI_FindOptionPosition(uiOptionIterator_t* iterator, const struct uiNode_s* option);
struct uiNode_s* UI_AddOption(struct uiNode_s**tree, const char* name, const char* label, const char* value);

/* line strip */
void UI_RegisterLineStrip(int dataId, struct lineStrip_s *text);

#endif
