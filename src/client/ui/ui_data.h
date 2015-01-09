/**
 * @file
 * @brief Data and interface to share data
 * @todo clean up the interface
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

#pragma once

#include "../../shared/ufotypes.h"
#include "../../shared/shared.h"
#include "ui_nodes.h"
#include "node/ui_node_option.h"
#include "ui_dataids.h"

struct linkedList_t;

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
		const char* text;
		/** @brief Holds a linked list for displaying in the UI */
		linkedList_t* linkedListText;
		/** @brief Holds a linked list for option (label, action, icon...) */
		uiNode_t* option;
		/** @brief Holds a line strip, a list of point */
		struct lineStrip_s	*lineStrip;
	} data;						/**< The data */
	int versionId;				/**< Id identify the value, to check changes */
} uiSharedData_t;

#define MAX_DEPTH_OPTIONITERATORCACHE 8

typedef struct {
	uiNode_t* option;		/**< current option */
	uiNode_t* depthCache[MAX_DEPTH_OPTIONITERATORCACHE];	/**< parent link */
	int depthPos;			/**< current cache position */
	bool skipInvisible;		/**< skip invisible options when we iterate */
	bool skipCollapsed;		/**< skip collapsed options when we iterate */
} uiOptionIterator_t;

/* common */
int UI_GetDataVersion(int textId) __attribute__ ((warn_unused_result));
void UI_ResetData(int dataId);
int UI_GetDataIDByName(const char* name) __attribute__ ((warn_unused_result));
void UI_InitData(void);

/* text */
void UI_RegisterText(int textId, const char* text);
const char* UI_GetText(int textId) __attribute__ ((warn_unused_result));
const char* UI_GetTextFromList(int textId, int line) __attribute__ ((warn_unused_result));

/* linked list */
void UI_RegisterLinkedListText(int textId, linkedList_t* text);

/* option */
void UI_RegisterOption(int dataId, uiNode_t* option);
uiNode_t* UI_GetOption(int dataId) __attribute__ ((warn_unused_result));
void UI_SortOptions(uiNode_t** option);
uiNode_t* UI_InitOptionIteratorAtIndex(int index, uiNode_t* option, uiOptionIterator_t* iterator);
uiNode_t* UI_OptionIteratorNextOption(uiOptionIterator_t* iterator);
void UI_UpdateInvisOptions(uiNode_t* option, const linkedList_t* stringList);
uiNode_t* UI_FindOptionByValue(uiOptionIterator_t* iterator, const char* value);
int UI_FindOptionPosition(uiOptionIterator_t* iterator, uiNode_t const* option);
uiNode_t* UI_AddOption(uiNode_t** tree, const char* name, const char* label, const char* value);

/* line strip */
void UI_RegisterLineStrip(int dataId, struct lineStrip_s* text);
