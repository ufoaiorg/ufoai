/**
 * @file cl_messageoptions_callbacks.c
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

#include "../client.h"
#include "../cl_global.h"
#include "cl_messageoptions.h"
#include "cl_messageoptions_callbacks.h"


#include "../menu/m_nodes.h"

static msoMenuState_t msoMenuState = MSO_MSTATE_REINIT;
int messageList_scroll = 0; /**< actual messageSettings list begin index due to scrolling */
int visibleMSOEntries = 0; /**< actual visible entry count */
static menuNode_t *msoTextNode; /**< reference to text node for easier references */
messageSettings_t backupMessageSettings[NT_NUM_NOTIFYTYPE]; /**< array holding backup message settings (used for restore function in options popup) */


/**
 * @brief Returns the category entry that is shown at given selection index.
 * @param selection index in visible list as returned by text list
 * @param visibleIndex flag indicating that index is from visible lines, not from text row
 * @return category entry from gd.msgCategoryEntries
 * @note this method takes into account scroll index and folding state of categories.
 * @sa MSO_Toggle_f
 * @sa MSO_OptionsClick_f
 */
static const msgCategoryEntry_t *MSO_GetEntryFromSelectionIndex (const int selection, const qboolean visibleIndex)
{
	int entriesToCheck = visibleIndex ? selection + messageList_scroll : selection;
	int realIndex = 0;
	for (; entriesToCheck > 0; realIndex++) {
		const msgCategoryEntry_t *entry = &gd.msgCategoryEntries[realIndex];
		entriesToCheck--;
		if (entry->isCategory && entry->category->isFolded) {
			/* first entry of category is category itself, count other entries */
			const msgCategoryEntry_t *invisibleEntry = entry->next;
			while (invisibleEntry) {
				realIndex++;
				invisibleEntry = invisibleEntry->next;
			}
		}
	}
	if (realIndex >= gd.numMsgCategoryEntries)
		return NULL;
	return &gd.msgCategoryEntries[realIndex];
}

/**
 * @brief Initializes menu texts for scrollable area
 * @sa MSO_Init_f
 */
static void MSO_InitTextList (void)
{
	char lineprefix[64], categoryLine[128];
	int idx;
	const int oldVisibleEntries = visibleMSOEntries;

	linkedList_t *messageSettingsList = NULL;

	MN_MenuTextReset(TEXT_MESSAGEOPTIONS);
	visibleMSOEntries = 0;

	for (idx = 0; idx < gd.numMsgCategoryEntries; idx++) {
		const msgCategoryEntry_t *entry = &gd.msgCategoryEntries[idx];
		if (!entry->isCategory && entry->category->isFolded)
			continue;
		if (entry->isCategory) {
			Com_sprintf(lineprefix, sizeof(lineprefix), TEXT_IMAGETAG"menu/icon_%s", entry->category->isFolded ? "open" : "close");
		} else
			Com_sprintf(lineprefix, sizeof(lineprefix), "   ");
		Com_sprintf(categoryLine, sizeof(categoryLine), "%s %s", lineprefix, _(entry->notifyType));
		LIST_AddString(&messageSettingsList, categoryLine);
		visibleMSOEntries++;
	}
	MN_RegisterLinkedListText(TEXT_MESSAGEOPTIONS, messageSettingsList);
	MSO_SetMenuState(MSO_MSTATE_PREPARED,qfalse,qtrue);
	if (oldVisibleEntries > visibleMSOEntries && messageList_scroll > visibleMSOEntries - msoTextNode->u.text.rows) {
		messageList_scroll = visibleMSOEntries - msoTextNode->u.text.rows;
		if (messageList_scroll < 0)
			messageList_scroll = 0;
		msoTextNode->u.text.textScroll = messageList_scroll;
	}
}

/**
 * @brief Executes confuncs to update visible message options lines.
 * @sa MSO_Init_f
 */
static void MSO_UpdateVisibleButtons (void)
{
	int idx;
	int visible = 0;/* visible lines*/

	/* update visible button lines based on current displayed values */
	for (idx = 0; visible < msoTextNode->u.text.rows && idx < gd.numMsgCategoryEntries; idx++) {
		const msgCategoryEntry_t *entry = MSO_GetEntryFromSelectionIndex(idx,qtrue);
		if (!entry)
			break;
		if (entry->isCategory) {
			/* category is visible anyway*/
			MN_ExecuteConfunc("ms_disable %i", visible);
			visible++;
		} else {
			assert(entry->category);
			if (!entry->category->isFolded) {
				MN_ExecuteConfunc("ms_enable %i", visible);
				MN_ExecuteConfunc("ms_btnstate %i %i %i %i", visible, entry->settings->doPause,
						entry->settings->doNotify, entry->settings->doSound);
				visible++;
			}
		}
	}

	for (; visible < msoTextNode->u.text.rows && idx < lengthof(gd.msgCategoryEntries); idx++) {
		MN_ExecuteConfunc("ms_disable %i", visible);
		visible++;
	}
}

/**
 * @brief initializes message options menu by showing as much button lines as needed.
 * @note This must only be done once as long as settings are changed in menu, so
 * messageOptionsInitialized is checked whether this is done yet. This function will be
 * reenabled if settings are changed via MSO_Set_f. If text list is not initialized
 * after parsing, MSO_InitTextList will be called first.
 */
static void MSO_Init_f (void)
{
	if (!msoTextNode)
		msoTextNode = MN_GetNodeFromCurrentMenu("messagetypes");
	if (msoMenuState < MSO_MSTATE_PREPARED) {
		MSO_InitTextList();
		msoMenuState = MSO_MSTATE_PREPARED;
	}

	if (msoMenuState < MSO_MSTATE_INITIALIZED) {
		MSO_UpdateVisibleButtons();
		msoMenuState = MSO_MSTATE_INITIALIZED;
	}
}

/**
 * @brief Function for menu buttons to update message settings.
 * @sa MSO_Set
 */
static void MSO_Toggle_f (void)
{
	if (Cmd_Argc() != 3)
		Com_Printf("Usage: %s <listId> <pause|notify|sound>\n", Cmd_Argv(0));
	else {
		const int listIndex = atoi(Cmd_Argv(1));
		const msgCategoryEntry_t *selectedEntry = MSO_GetEntryFromSelectionIndex(listIndex,qtrue);
		int optionType;
		qboolean activate;
		notify_t type;

		if (!selectedEntry)
			return;
		if (selectedEntry->isCategory) {
			Com_Printf("Toggle command with selected category entry ignored.\n");
			return;
		}
		for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
			if (!Q_strcmp(nt_strings[type], selectedEntry->notifyType))
				break;
		}
		if (type == NT_NUM_NOTIFYTYPE) {
			Com_Printf("Unrecognized messagetype during toggle '%s' ignored\n", selectedEntry->notifyType);
			return;
		}

		if (!Q_strcmp(Cmd_Argv(2), "pause")) {
			optionType = MSO_PAUSE;
			activate = !selectedEntry->settings->doPause;
		} else if (!Q_strcmp(Cmd_Argv(2), "notify")) {
			optionType = MSO_NOTIFY;
			activate = !selectedEntry->settings->doNotify;
		} else {
			optionType = MSO_SOUND;
			activate = !selectedEntry->settings->doSound;
		}
		MSO_Set(listIndex, type, optionType, activate, qtrue);
	}
}

/**
 * @brief Function to update message options menu after scrolling.
 * Updates all visible button lines based on configuration.
 */
static void MSO_Scroll_f (void)
{
	const int oldScrollIdx = messageList_scroll;

	if (!msoTextNode)
		return;

	/* no scrolling if visible entry count is less than max on page (due to folding) */
	if (visibleMSOEntries < msoTextNode->u.text.rows)
		return;

	messageList_scroll = msoTextNode->u.text.textScroll;

	if (messageList_scroll >= visibleMSOEntries - msoTextNode->u.text.rows) {
		messageList_scroll = visibleMSOEntries - msoTextNode->u.text.rows;
		msoTextNode->u.text.textScroll = messageList_scroll;
	}

	if (messageList_scroll != oldScrollIdx)
		MSO_UpdateVisibleButtons();
}

/**
 * @brief Function toggles visibility of a message category to show or hide entries associated to this category.
 */
static void MSO_OptionsClick_f(void)
{
	int num;
	const msgCategoryEntry_t *category;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= gd.numMsgCategoryEntries || num < 0)
		return;

	category = MSO_GetEntryFromSelectionIndex(num, qfalse);
	assert(msoTextNode);
	/* don't highlight selection */
	MN_TextNodeSelectLine(msoTextNode, -1);
	if (!category || !category->isCategory)
		return;
	category->category->isFolded = !category->category->isFolded;
	MSO_SetMenuState(MSO_MSTATE_REINIT, qtrue, qtrue);
}


/**
 * @brief Saves actual settings into backup settings variable.
 */
static void MSO_BackupSettings_f (void)
{
	memcpy(backupMessageSettings, messageSettings, sizeof(backupMessageSettings));
}

/**
 * @brief Restores actual settings from backup settings variable.
 * @note message options are caused to be re-initialized (for menu display)
 * @sa MSO_Init_f
 */
static void MSO_RestoreSettings_f (void)
{
	memcpy(messageSettings, backupMessageSettings, sizeof(messageSettings));
	MSO_SetMenuState(MSO_MSTATE_REINIT,qfalse,qtrue);
}

void MSO_SetMenuState (const msoMenuState_t newState, const qboolean callInit, const qboolean preserveIndex)
{
	msoMenuState = newState;
	if (newState == MSO_MSTATE_REINIT && !preserveIndex) {
		visibleMSOEntries = 0;
		messageList_scroll = 0;
	}
	if (callInit)
		MSO_Init_f();

}

void MSO_InitCallbacks(void)
{
	memset(backupMessageSettings, 1, sizeof(backupMessageSettings));
	Cmd_AddCommand("msgoptions_toggle", MSO_Toggle_f, "Toggles pause, notification or sound setting for a message category");
	Cmd_AddCommand("msgoptions_scroll", MSO_Scroll_f, "Scroll callback function for message options menu text");
	Cmd_AddCommand("messagetypes_click", MSO_OptionsClick_f, "Callback function to (un-)fold visible categories");
	Cmd_AddCommand("msgoptions_init", MSO_Init_f, "Initializes message options menu");
	Cmd_AddCommand("msgoptions_backup", MSO_BackupSettings_f, "Backup message settings");
	Cmd_AddCommand("msgoptions_restore",MSO_RestoreSettings_f, "Restore message settings from backup");

}

void MSO_ShutdownCallbacks(void)
{
	Cmd_RemoveCommand("msgoptions_toggle");
	Cmd_RemoveCommand("msgoptions_scroll");
	Cmd_RemoveCommand("messagetypes_click");
	Cmd_RemoveCommand("msgoptions_init");
	Cmd_RemoveCommand("msgoptions_backup");
	Cmd_RemoveCommand("msgoptions_restore");

}
