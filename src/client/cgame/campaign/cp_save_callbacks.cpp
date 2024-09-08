/**
 * @file
 * @brief UI Callbacks for savegame management
 */

/*
Copyright (C) 2002-2024 UFO: Alien Invasion.

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

#include "../../../common/filesys.h"
#include "../../cl_shared.h"
#include "cp_save_callbacks.h"
#include "cp_campaign.h"
#include "cp_save.h"
#include "cp_popup.h"
#include "missions/cp_mission_baseattack.h"

static cvar_t* cl_lastsave;

/**
 * @brief Console command to list savegames
 */
static void SAV_ListSaveGames_f (void)
{
	if (cgi->Cmd_Argc() < 1) {
		cgi->Com_Printf("usage: %s\n", cgi->Cmd_Argv(0));
		return;
	}

	char pathMask[MAX_OSPATH];
	cgi->GetRelativeSavePath(pathMask, sizeof(pathMask));
	Q_strcat(pathMask, sizeof(pathMask), "/*.savx");

	cgi->FS_BuildFileList(pathMask);
	const char* path;
	int idx = 0;
	while ((path = cgi->FS_NextFileFromFileList(pathMask)) != nullptr) {
		char fileName[MAX_OSPATH];
		saveFileHeader_t header;
		Com_StripExtension(path, fileName, sizeof(fileName));
		const char *basename = Com_SkipPath(fileName);

		if (SAV_LoadHeader(basename, &header))
			cgi->UI_ExecuteConfunc("ui_add_savegame %i \"%s\" \"%s\" \"%s\" \"%s\"", idx++,
				header.name, header.gameDate, header.realDate, basename);
	}
	cgi->FS_NextFileFromFileList(nullptr);
}


/**
 * @brief Console command to load a savegame
 * @sa SAV_GameLoad
 */
static void SAV_GameLoad_f (void)
{
	const char* error = nullptr;

	/* get argument */
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <filename>\n", cgi->Cmd_Argv(0));
		return;
	}

	/* Check if savegame exists */
	char buf[MAX_OSPATH];
	cgi->GetRelativeSavePath(buf, sizeof(buf));
	if (cgi->FS_CheckFile("%s%s.%s", buf, cgi->Cmd_Argv(1), SAVEGAME_EXTENSION) <= 0) {
		cgi->Com_Printf("savegame file '%s' doesn't exist or an empty file\n", cgi->Cmd_Argv(1));
		return;
	}

	cgi->Com_DPrintf(DEBUG_CLIENT, "load file '%s'\n", cgi->Cmd_Argv(1));

	/* load and go to map */
	if (!SAV_GameLoad(cgi->Cmd_Argv(1), &error)) {
		cgi->Cbuf_Execute(); /* wipe outstanding campaign commands */
		cgi->Cmd_ExecuteString("game_exit");
		cgi->Cmd_ExecuteString("game_setmode campaign");
		cgi->UI_Popup(_("Error"), "%s\n%s", _("Error loading game."), error ? error : "");
	}
}

/**
 * @brief Loads the last saved game
 * @note At saving the archive cvar cl_lastsave was set to the latest savegame
 * @sa SAV_GameLoad
 */
static void SAV_GameContinue_f (void)
{
	const char* error = nullptr;

	if (cgi->CL_OnBattlescape()) {
		cgi->UI_PopWindow(false);
		return;
	}

	if (!CP_IsRunning()) {
		/* try to load the last saved campaign */
		if (!SAV_GameLoad(cl_lastsave->string, &error)) {
			cgi->Cbuf_Execute(); /* wipe outstanding campaign commands */
			cgi->UI_Popup(_("Error"), "%s\n%s", _("Error loading game."), error ? error : "");
			cgi->Cmd_ExecuteString("game_exit");
		}
	} else {
		/* just continue the current running game */
		cgi->UI_PopWindow(false);
	}
}

/**
 * @brief Console command binding for save function
 * @sa SAV_GameSave
 * @note called via 'game_save' command
 */
static void SAV_GameSave_f (void)
{
	/* get argument */
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <filename> <comment|*cvar>\n", cgi->Cmd_Argv(0));
		return;
	}

	/* get comment */
	char comment[MAX_VAR] = "";
	if (cgi->Cmd_Argc() > 2) {
		const char* arg = cgi->Cmd_Argv(2);
		Q_strncpyz(comment, arg, sizeof(comment));
	}

	char* error = nullptr;
	const bool result = SAV_GameSave(cgi->Cmd_Argv(1), comment, &error);
	if (result)
		return;

	if (error)
		CP_Popup(_("Note"), "%s\n%s", _("Error saving game."), error);
	else
		CP_Popup(_("Note"), "%s\n%s", "%s\n", _("Error saving game."));
}

/**
 * @brief Removes savegame file
 */
static void SAV_GameDelete_f (void)
{
	if (cgi->Cmd_Argc() != 2) {
		cgi->Com_Printf("Usage: %s <filename>\n", cgi->Cmd_Argv(0));
		return;
	}
	const char* savegame = cgi->Cmd_Argv(1);
	char buf[MAX_OSPATH];

	cgi->GetAbsoluteSavePath(buf, sizeof(buf));
	Q_strcat(buf, sizeof(buf), "%s.%s", savegame, SAVEGAME_EXTENSION);

	cgi->FS_RemoveFile(buf);
}

/**
 * @brief Checks whether there is a quicksave file and opens the quickload menu if there is one
 * @note This does not work while we are in the battlescape
 */
static void SAV_GameQuickLoadInit_f (void)
{
	if (cgi->CL_OnBattlescape()) {
		return;
	}

	char buf[MAX_OSPATH];
	cgi->GetRelativeSavePath(buf, sizeof(buf));
	if (cgi->FS_CheckFile("%sslotquick.%s", buf, SAVEGAME_EXTENSION) > 0) {
		cgi->UI_PushWindow("quickload");
	}
}

/**
 * @brief Saves to the quick save slot
 */
static void SAV_GameQuickSave_f (void)
{
	if (!CP_IsRunning())
		return;
	if (cgi->CL_OnBattlescape())
		return;

	char* error = nullptr;
	bool result = SAV_GameSave("slotquick", _("QuickSave"), &error);
	if (!result)
		cgi->Com_Printf("Error saving the xml game: %s\n", error ? error : "");
	else
		MS_AddNewMessage(_("Quicksave"), _("Campaign was successfully saved."), MSG_INFO);
}

/**
 * @brief Loads the quick save slot
 * @sa SAV_GameQuickSave_f
 */
static void SAV_GameQuickLoad_f (void)
{
	const char* error = nullptr;

	if (cgi->CL_OnBattlescape()) {
		cgi->Com_Printf("Could not load the campaign while you are on the battlefield\n");
		return;
	}

	if (!SAV_GameLoad("slotquick", &error)) {
		cgi->Cbuf_Execute(); /* wipe outstanding campaign commands */
		CP_Popup(_("Error"), "%s\n%s", _("Error loading game."), error ? error : "");
	} else {
		MS_AddNewMessage(_("Campaign loaded"), _("Quicksave campaign was successfully loaded."), MSG_INFO);
		CP_CheckBaseAttacks();
	}
}

/**
 * @brief Returns whether saving game is allowed
*/
static void SAV_GameSaveAllowed_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <confunc_callback>\n", cgi->Cmd_Argv(0));
		return;
	}
	const char* callback = cgi->Cmd_Argv(1);

	char* error = nullptr;
	bool allowed = SAV_GameSaveAllowed(&error);

	cgi->UI_ExecuteConfunc("%s %s \"%s\"", callback, allowed ? "true" : "false", error ? error : "");
}

static const cmdList_t saveCallbacks[] = {
	{"game_listsaves", SAV_ListSaveGames_f, "Lists available savegames"},
	{"game_load", SAV_GameLoad_f, "Loads a given filename"},
	{"game_saveallowed", SAV_GameSaveAllowed_f, "Checks if saving the game is allowed"},
	{"game_save", SAV_GameSave_f, "Saves to a given filename"},
	{"game_delete", SAV_GameDelete_f, "Deletes a given filename"},
	{"game_continue", SAV_GameContinue_f, "Continue with the last saved game"},
	{"game_quickloadinit", SAV_GameQuickLoadInit_f, "Load the game from the quick save slot."},
	{"game_quickload", SAV_GameQuickLoad_f, "Load the game from the quick save slot."},
	{"game_quicksave", SAV_GameQuickSave_f, "Save to the quick save slot."},
	{nullptr, nullptr, nullptr}
};

/**
 * @brief Register UI callbacks for the savegame-subsystem
 */
void SAV_InitCallbacks(void)
{
	cgi->Cmd_TableAddList(saveCallbacks);

	cl_lastsave = cgi->Cvar_Get("cl_lastsave", "", CVAR_ARCHIVE, "Last saved slot - use for the continue-campaign function");
}

/**
 * @brief UnregisterUI callbacks for the savegame-subsystem
 */
void SAV_ShutdownCallbacks(void)
{
	cgi->Cmd_TableRemoveList(saveCallbacks);
}
