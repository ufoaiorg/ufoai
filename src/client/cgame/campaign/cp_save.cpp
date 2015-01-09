/**
 * @file
 * @brief Implements savegames
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

#include "../../cl_shared.h"
#include "../cl_game.h" /* GAME_ReloadMode */
#include "cp_campaign.h"
#include "cp_save.h"
#include "cp_time.h"
#include "cp_popup.h"
#include "save/save.h"
#include "missions/cp_mission_baseattack.h"

#define SAVEGAME_EXTENSION "savx"

typedef struct saveFileHeader_s {
	uint32_t version;			/**< which savegame version */
	uint32_t compressed;		/**< is this file compressed via zlib */
	uint32_t subsystems;		/**< amount of subsystems that were saved in this savegame */
	uint32_t dummy[13];			/**< maybe we have to extend this later */
	char gameVersion[16];		/**< game version that was used to save this file */
	char name[32];				/**< savefile name */
	char gameDate[32];			/**< internal game date */
	char realDate[32];			/**< real datestring when the user saved this game */
	uint32_t xmlSize;
} saveFileHeader_t;

static saveSubsystems_t saveSubsystems[MAX_SAVESUBSYSTEMS];
static int saveSubsystemsAmount;
static cvar_t* save_compressed;
static cvar_t* cl_lastsave;

/**
 * @brief Perform actions after loading a game for single player campaign
 * @sa SAV_GameLoad
 */
static bool SAV_GameActionsAfterLoad (void)
{
	bool result = true;

	result = result && AIR_PostLoadInit();
	result = result && B_PostLoadInit();
	result = result && PR_PostLoadInit();

	/* Make sure the date&time is displayed when loading. */
	CP_UpdateTime();

	/* Update number of UFO detected by radar */
	RADAR_SetRadarAfterLoading();

	return result;
}

/**
 * @brief Tries to verify the Header of the savegame
 * @param[in] header a pointer to the header to verify
 */
static bool SAV_VerifyHeader (saveFileHeader_t const * const header)
{
	size_t len;
	/*check the length of the string*/
	len = strlen(header->name);
	if (len > sizeof(header->name)) {
		Com_Printf("Name is " UFO_SIZE_T " Bytes long, max is " UFO_SIZE_T "\n", len, sizeof(header->name));
		return false;
	}
	len = strlen(header->gameVersion);
	if (len > sizeof(header->gameVersion)) {
		Com_Printf("gameVersion is " UFO_SIZE_T " Bytes long, max is " UFO_SIZE_T "\n", len, sizeof(header->gameVersion));
		return false;
	}
	len = strlen(header->gameDate);
	if (len > sizeof(header->gameDate)) {
		Com_Printf("gameDate is " UFO_SIZE_T " Bytes long, max is " UFO_SIZE_T "\n", len, sizeof(header->gameDate));
		return false;
	}
	len = strlen(header->realDate);
	if (len > sizeof(header->realDate)) {
		Com_Printf("realDate is " UFO_SIZE_T " Bytes long, max is " UFO_SIZE_T "\n", len, sizeof(header->realDate));
		return false;
	}
	if (header->subsystems != 0 && header->subsystems != saveSubsystemsAmount) {
		Com_DPrintf(DEBUG_CLIENT, "Savefile has incompatible amount of subsystems\n");
	}

	/* saved games should not be bigger than 15MB */
	if (header->xmlSize > 15 * 1024 * 1024) {
		Com_Printf("Save size seems to be to large (over 15 MB) %i.\n", header->xmlSize);
		return false;
	}
	if (header->version == 0) {
		Com_Printf("Version is invalid - must be greater than zero\n");
		return false;
	}
	if (header->version > SAVE_FILE_VERSION) {
		Com_Printf("Savefile is newer than the game!\n");
	}
	return true;
}

/**
 * @brief Loads the given savegame from an xml File.
 * @return true on load success false on failures
 * @param[in] file The Filename to load from (without extension)
 * @param[out] error On failure an errormessage may be set.
 */
bool SAV_GameLoad (const char* file, const char** error)
{
	char filename[MAX_OSPATH];

	/* open file */
	cgi->GetRelativeSavePath(filename, sizeof(filename));
	Q_strcat(filename, sizeof(filename), "%s.%s", file, SAVEGAME_EXTENSION);
	ScopedFile f;
	const int clen = cgi->FS_OpenFile(filename, &f, FILE_READ);
	if (!f) {
		Com_Printf("Couldn't open file '%s'\n", filename);
		*error = "File not found";
		return false;
	}

	byte* const cbuf = Mem_PoolAllocTypeN(byte, clen + 1 /* for '\0' if not compressed */, cp_campaignPool);
	if (cgi->FS_Read(cbuf, clen, &f) != clen)
		Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
	Com_Printf("Loading savegame xml (size %d)\n", clen);

	saveFileHeader_t header;
	memcpy(&header, cbuf, sizeof(header));
	/* swap all int values if needed */
	header.compressed = LittleLong(header.compressed);
	header.version = LittleLong(header.version);
	header.xmlSize = LittleLong(header.xmlSize);
	/* doing some header verification */
	if (!SAV_VerifyHeader(&header)) {
		/* our header is not valid, we MUST abort loading the game! */
		Com_Printf("The Header of the savegame '%s.%s' is corrupted. Loading aborted\n", filename, SAVEGAME_EXTENSION);
		cgi->Free(cbuf);
		*error = "Corrupted header";
		return false;
	}

	Com_Printf("Loading savegame\n"
			"...version: %i\n"
			"...game version: %s\n"
			"...xml Size: %i, compressed? %c\n",
			header.version, header.gameVersion, header.xmlSize, header.compressed ? 'y' : 'n');

	xmlNode_t* topNode;
	if (header.compressed) {
		uLongf      len = header.xmlSize + 1 /* for '\0' */;
		byte* const buf = Mem_PoolAllocTypeN(byte, len /* sic, old savegames contain one (garbage) byte more than the header says. */, cp_campaignPool);
		/* uncompress data, skipping comment header */
		const int res = uncompress(buf, &len, cbuf + sizeof(header), clen - sizeof(header));
		buf[header.xmlSize] = '\0'; /* Ensure '\0' termination. */
		cgi->Free(cbuf);

		if (res != Z_OK) {
			cgi->Free(buf);
			*error = _("Error decompressing data");
			Com_Printf("Error decompressing data in '%s'.\n", filename);
			return false;
		}
		topNode = cgi->XML_Parse((const char*)buf);
		if (!topNode) {
			cgi->Free(buf);
			Com_Printf("Error: Failure in loading the xml data!\n");
			*error = "Corrupted xml data";
			return false;
		}
		cgi->Free(buf);
	} else {
		topNode = cgi->XML_Parse((const char*)(cbuf + sizeof(header)));
		cgi->Free(cbuf);
		if (!topNode) {
			Com_Printf("Error: Failure in loading the xml data!\n");
			*error = "Corrupted xml data";
			return false;
		}
	}

	/* doing a subsystem run */
	cgi->GAME_ReloadMode();
	xmlNode_t* node = cgi->XML_GetNode(topNode, SAVE_ROOTNODE);
	if (!node) {
		Com_Printf("Error: Failure in loading the xml data! (savegame node not found)\n");
		mxmlDelete(topNode);
		*error = "Invalid xml data";
		return false;
	}

	Com_Printf("Load '%s' %d subsystems\n", filename, saveSubsystemsAmount);
	for (int i = 0; i < saveSubsystemsAmount; i++) {
		if (!saveSubsystems[i].load)
			continue;
		Com_Printf("...Running subsystem '%s'\n", saveSubsystems[i].name);
		if (!saveSubsystems[i].load(node)) {
			Com_Printf("...subsystem '%s' returned false - savegame could not be loaded\n",
					saveSubsystems[i].name);
			*error = va("Could not load subsystem %s", saveSubsystems[i].name);
			return false;
		} else
			Com_Printf("...subsystem '%s' - loaded.\n", saveSubsystems[i].name);
	}
	mxmlDelete(node);
	mxmlDelete(topNode);

	if (!SAV_GameActionsAfterLoad()) {
		Com_Printf("Savegame postprocessing returned false - savegame could not be loaded\n");
		*error = "Postprocessing failed";
		return false;
	}

	Com_Printf("File '%s' successfully loaded from %s xml savegame.\n",
			filename, header.compressed ? "compressed" : "");

	cgi->UI_InitStack("geoscape", nullptr);
	return true;
}

/**
 * @brief This is a savegame function which stores the game in xml-Format.
 * @param[in] filename The Filename to save to (without extension)
 * @param[in] comment Description of the savegame
 * @param[out] error On failure an errormessage may be set.
 */
static bool SAV_GameSave (const char* filename, const char* comment, char** error)
{
	if (!CP_IsRunning()) {
		*error = _("No campaign active.");
		Com_Printf("Error: No campaign active.\n");
		return false;
	}

	if (!B_AtLeastOneExists()) {
		*error = _("Nothing to save yet.");
		Com_Printf("Error: Nothing to save yet.\n");
		return false;
	}

	char savegame[MAX_OSPATH];
	dateLong_t date;
	char message[30];
	char timeStampBuffer[32];

	Com_MakeTimestamp(timeStampBuffer, sizeof(timeStampBuffer));
	cgi->GetRelativeSavePath(savegame, sizeof(savegame));
	Q_strcat(savegame, sizeof(savegame), "%s.%s", filename, SAVEGAME_EXTENSION);
	xmlNode_t* topNode = mxmlNewXML("1.0");
	xmlNode_t* node = cgi->XML_AddNode(topNode, SAVE_ROOTNODE);
	/* writing  Header */
	cgi->XML_AddInt(node, SAVE_SAVEVERSION, SAVE_FILE_VERSION);
	cgi->XML_AddString(node, SAVE_COMMENT, comment);
	cgi->XML_AddString(node, SAVE_UFOVERSION, UFO_VERSION);
	cgi->XML_AddString(node, SAVE_REALDATE, timeStampBuffer);
	CP_DateConvertLong(&ccs.date, &date);
	Com_sprintf(message, sizeof(message), _("%i %s %02i"),
		date.year, Date_GetMonthName(date.month - 1), date.day);
	cgi->XML_AddString(node, SAVE_GAMEDATE, message);
	/* working through all subsystems. perhaps we should redesign it, order is not important anymore */
	Com_Printf("Calling subsystems\n");
	for (int i = 0; i < saveSubsystemsAmount; i++) {
		if (!saveSubsystems[i].save)
			continue;
		if (!saveSubsystems[i].save(node))
			Com_Printf("...subsystem '%s' failed to save the data\n", saveSubsystems[i].name);
		else
			Com_Printf("...subsystem '%s' - saved\n", saveSubsystems[i].name);
	}

	/* calculate the needed buffer size */
	saveFileHeader_t header;
	OBJZERO(header);
	header.compressed = LittleLong(save_compressed->integer);
	header.version = LittleLong(SAVE_FILE_VERSION);
	header.subsystems = LittleLong(saveSubsystemsAmount);
	Q_strncpyz(header.name, comment, sizeof(header.name));
	Q_strncpyz(header.gameVersion, UFO_VERSION, sizeof(header.gameVersion));
	CP_DateConvertLong(&ccs.date, &date);
	Com_sprintf(header.gameDate, sizeof(header.gameDate), _("%i %s %02i"),
		date.year, Date_GetMonthName(date.month - 1), date.day);
	Q_strncpyz(header.realDate, timeStampBuffer, sizeof(header.realDate));

	char dummy[2];
	int requiredBufferLength = mxmlSaveString(topNode, dummy, 2, MXML_NO_CALLBACK);

	header.xmlSize = LittleLong(requiredBufferLength);
	byte* const buf = Mem_PoolAllocTypeN(byte, requiredBufferLength + 1, cp_campaignPool);
	if (!buf) {
		mxmlDelete(topNode);
		*error = _("Could not allocate enough memory to save this game");
		Com_Printf("Error: Could not allocate enough memory to save this game\n");
		return false;
	}
	int res = mxmlSaveString(topNode, (char*)buf, requiredBufferLength + 1, MXML_NO_CALLBACK);
	mxmlDelete(topNode);
	Com_Printf("XML Written to buffer (%d Bytes)\n", res);

	uLongf bufLen;
	if (header.compressed)
		bufLen = compressBound(requiredBufferLength);
	else
		bufLen = requiredBufferLength;

	byte* const fbuf = Mem_PoolAllocTypeN(byte, bufLen + sizeof(header), cp_campaignPool);
	memcpy(fbuf, &header, sizeof(header));

	if (header.compressed) {
		res = compress(fbuf + sizeof(header), &bufLen, buf, requiredBufferLength);
		cgi->Free(buf);

		if (res != Z_OK) {
			cgi->Free(fbuf);
			*error = _("Memory error compressing save-game data - set save_compressed cvar to 0");
			Com_Printf("Memory error compressing save-game data (%s) (Error: %i)!\n", comment, res);
			return false;
		}
	} else {
		memcpy(fbuf + sizeof(header), buf, requiredBufferLength);
		cgi->Free(buf);
	}

	/* last step - write data */
	cgi->FS_WriteFile(fbuf, bufLen + sizeof(header), savegame);
	cgi->Free(fbuf);

	return true;
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
		Com_Printf("Usage: %s <filename> <comment|*cvar>\n", cgi->Cmd_Argv(0));
		return;
	}

	if (!CP_IsRunning()) {
		Com_Printf("No running game - no saving...\n");
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
void SAV_GameDelete_f (void)
{
	if (cgi->Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <filename>\n", cgi->Cmd_Argv(0));
		return;
	}
	const char* savegame = cgi->Cmd_Argv(1);
	char buf[MAX_OSPATH];

	cgi->GetAbsoluteSavePath(buf, sizeof(buf));
	Q_strcat(buf, sizeof(buf), "%s.%s", savegame, SAVEGAME_EXTENSION);

	cgi->FS_RemoveFile(buf);
}

/**
 * @brief Init menu cvar for one savegame slot given by actual index.
 * @param[in] idx the savegame slot to retrieve gamecomment for
 * @sa SAV_GameReadGameComments_f
 */
static void SAV_GameReadGameComment (const int idx)
{
	char filename[MAX_OSPATH];
	cgi->GetRelativeSavePath(filename, sizeof(filename));
	Q_strcat(filename, sizeof(filename), "slot%i.%s", idx, SAVEGAME_EXTENSION);

	ScopedFile f;
	cgi->FS_OpenFile(filename, &f, FILE_READ);
	if (!f) {
		cgi->UI_ExecuteConfunc("update_game_info %i \"\" \"\" \"\" \"\"", idx);
		return;
	}

	saveFileHeader_t header;
	if (cgi->FS_Read(&header, sizeof(header), &f) != sizeof(header))
		Com_Printf("Warning: Savefile header may be corrupted\n");

	header.compressed = LittleLong(header.compressed);
	header.version = LittleLong(header.version);
	header.xmlSize = LittleLong(header.xmlSize);
	header.subsystems = LittleLong(header.subsystems);

	const char* basename = Com_SkipPath(filename);
	if (!SAV_VerifyHeader(&header)) {
		Com_Printf("Savegame header for slot%d is corrupted!\n", idx);
		return;
	}

	cgi->UI_ExecuteConfunc("update_game_info %i \"%s\" \"%s\" \"%s\" \"%s\"", idx, header.name, header.gameDate, header.realDate, basename);
}

/**
 * @brief Console commands to read the comments from savegames
 * @note The comment is the part of the savegame header that you type in at saving
 * for reidentifying the savegame
 * @sa SAV_GameLoad_f
 * @sa SAV_GameLoad
 * @sa SAV_GameSaveNameCleanup_f
 * @sa SAV_GameReadGameComment
 */
static void SAV_GameReadGameComments_f (void)
{
	if (cgi->Cmd_Argc() < 1) {
		Com_Printf("usage: %s {id}\n", cgi->Cmd_Argv(0));
		return;
	}

	if (cgi->Cmd_Argc() == 2) {
		int idx = atoi(cgi->Cmd_Argv(1));
		SAV_GameReadGameComment(idx);
	} else {
		/* read all game comments */
		for (int i = 0; i < 8; i++) {
			SAV_GameReadGameComment(i);
		}
	}
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
		Com_Printf("Usage: %s <filename>\n", cgi->Cmd_Argv(0));
		return;
	}

	/* Check if savegame exists */
	char buf[MAX_OSPATH];
	cgi->GetRelativeSavePath(buf, sizeof(buf));
	if (cgi->FS_CheckFile("%s%s.%s", buf, cgi->Cmd_Argv(1), SAVEGAME_EXTENSION) <= 0) {
		Com_Printf("savegame file '%s' doesn't exist or an empty file\n", cgi->Cmd_Argv(1));
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "load file '%s'\n", cgi->Cmd_Argv(1));

	/* load and go to map */
	if (!SAV_GameLoad(cgi->Cmd_Argv(1), &error)) {
		cgi->Cbuf_Execute(); /* wipe outstanding campaign commands */
		cgi->UI_Popup(_("Error"), "%s\n%s", _("Error loading game."), error ? error : "");
		cgi->Cmd_ExecuteString("game_exit");
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
 * @brief Adds a subsystem to the saveSubsystems array
 * @note The order is _not_ important
 * @sa SAV_Init
 */
bool SAV_AddSubsystem (saveSubsystems_t* subsystem)
{
	if (saveSubsystemsAmount >= MAX_SAVESUBSYSTEMS)
		return false;

	saveSubsystems[saveSubsystemsAmount].name = subsystem->name;
	saveSubsystems[saveSubsystemsAmount].load = subsystem->load;
	saveSubsystems[saveSubsystemsAmount].save = subsystem->save;
	saveSubsystemsAmount++;

	Com_Printf("added %s subsystem\n", subsystem->name);
	return true;
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
		Com_Printf("Error saving the xml game: %s\n", error ? error : "");
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
		Com_Printf("Could not load the campaign while you are on the battlefield\n");
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
 * @brief Register all save-subsystems and init some cvars and commands
 * @sa SAV_GameSave
 * @sa SAV_GameLoad
 */
void SAV_Init (void)
{
	static saveSubsystems_t cp_subsystemXML = {"campaign", CP_SaveXML, CP_LoadXML};
	static saveSubsystems_t rs_subsystemXML = {"research", RS_SaveXML, RS_LoadXML};
	static saveSubsystems_t b_subsystemXML = {"base", B_SaveXML, B_LoadXML};
	static saveSubsystems_t hos_subsystemXML = {"hospital", HOS_SaveXML, HOS_LoadXML};
	static saveSubsystems_t bs_subsystemXML = {"market", BS_SaveXML, BS_LoadXML};
	static saveSubsystems_t e_subsystemXML = {"employee", E_SaveXML, E_LoadXML};
	static saveSubsystems_t ac_subsystemXML = {"aliencont", nullptr, AC_LoadXML};
	static saveSubsystems_t pr_subsystemXML = {"production", PR_SaveXML, PR_LoadXML};
	static saveSubsystems_t air_subsystemXML = {"aircraft", AIR_SaveXML, AIR_LoadXML};
	static saveSubsystems_t ab_subsystemXML = {"alien base", AB_SaveXML, AB_LoadXML};
	static saveSubsystems_t int_subsystemXML = {"interest", INT_SaveXML, INT_LoadXML};
	static saveSubsystems_t mis_subsystemXML = {"mission", MIS_SaveXML, MIS_LoadXML};
	static saveSubsystems_t ms_subsystemXML = {"messagesystem", MS_SaveXML, MS_LoadXML};
	static saveSubsystems_t stats_subsystemXML = {"stats", STATS_SaveXML, STATS_LoadXML};
	static saveSubsystems_t na_subsystemXML = {"nations", NAT_SaveXML, NAT_LoadXML};
	static saveSubsystems_t trans_subsystemXML = {"transfer", TR_SaveXML, TR_LoadXML};
	static saveSubsystems_t xvi_subsystemXML = {"xvirate", XVI_SaveXML, XVI_LoadXML};
	static saveSubsystems_t ins_subsystemXML = {"installation", INS_SaveXML, INS_LoadXML};
	static saveSubsystems_t mso_subsystemXML = {"messageoptions", MSO_SaveXML, MSO_LoadXML};
	static saveSubsystems_t event_subsystemXML = {"triggerevents", CP_TriggerEventSaveXML, CP_TriggerEventLoadXML};
	static saveSubsystems_t us_subsystemXML = {"ufostores", US_SaveXML, US_LoadXML};

	saveSubsystemsAmount = 0;
	OBJZERO(saveSubsystems);

	Com_Printf("\n--- save subsystem initialization --\n");

	/* don't mess with the order */
	SAV_AddSubsystem(&cp_subsystemXML);
	SAV_AddSubsystem(&rs_subsystemXML);
	SAV_AddSubsystem(&b_subsystemXML);
	SAV_AddSubsystem(&hos_subsystemXML);
	SAV_AddSubsystem(&bs_subsystemXML);
	SAV_AddSubsystem(&e_subsystemXML);
	SAV_AddSubsystem(&ac_subsystemXML);
	SAV_AddSubsystem(&air_subsystemXML);
	SAV_AddSubsystem(&ab_subsystemXML);
	SAV_AddSubsystem(&int_subsystemXML);
	SAV_AddSubsystem(&ins_subsystemXML);
	SAV_AddSubsystem(&mis_subsystemXML);
	SAV_AddSubsystem(&us_subsystemXML);
	SAV_AddSubsystem(&pr_subsystemXML);
	SAV_AddSubsystem(&ms_subsystemXML);
	SAV_AddSubsystem(&stats_subsystemXML);
	SAV_AddSubsystem(&na_subsystemXML);
	SAV_AddSubsystem(&trans_subsystemXML);
	SAV_AddSubsystem(&xvi_subsystemXML);
	SAV_AddSubsystem(&mso_subsystemXML);
	SAV_AddSubsystem(&event_subsystemXML);

	/* Check whether there is a quicksave at all */
	cgi->Cmd_AddCommand("game_quickloadinit", SAV_GameQuickLoadInit_f, "Load the game from the quick save slot.");
	cgi->Cmd_AddCommand("game_quicksave", SAV_GameQuickSave_f, "Save to the quick save slot.");
	cgi->Cmd_AddCommand("game_quickload", SAV_GameQuickLoad_f, "Load the game from the quick save slot.");
	cgi->Cmd_AddCommand("game_save", SAV_GameSave_f, "Saves to a given filename");
	cgi->Cmd_AddCommand("game_load", SAV_GameLoad_f, "Loads a given filename");
	cgi->Cmd_AddCommand("game_delete", SAV_GameDelete_f, "Deletes a given filename");
	cgi->Cmd_AddCommand("game_comments", SAV_GameReadGameComments_f, "Loads the savegame names");
	cgi->Cmd_AddCommand("game_continue", SAV_GameContinue_f, "Continue with the last saved game");
	save_compressed = cgi->Cvar_Get("save_compressed", "1", CVAR_ARCHIVE, "Save the savefiles compressed if set to 1");
	cl_lastsave = cgi->Cvar_Get("cl_lastsave", "", CVAR_ARCHIVE, "Last saved slot - use for the continue-campaign function");
}
