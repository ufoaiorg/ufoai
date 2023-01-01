/**
 * @file
 * @brief Implements savegames
 */

/*
Copyright (C) 2002-2023 UFO: Alien Invasion.

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
#include "cp_xvi.h" /* CP_UpdateXVIMapButton */
#include "save/save.h"

static saveSubsystems_t saveSubsystems[MAX_SAVESUBSYSTEMS];
static int saveSubsystemsAmount;
static cvar_t* save_compressed;

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

	/* Update the XVI button -- needs to be done after research was loaded */
	CP_UpdateXVIMapButton();

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
		cgi->Com_Printf("Name is " UFO_SIZE_T " Bytes long, max is " UFO_SIZE_T "\n", len, sizeof(header->name));
		return false;
	}
	len = strlen(header->gameVersion);
	if (len > sizeof(header->gameVersion)) {
		cgi->Com_Printf("gameVersion is " UFO_SIZE_T " Bytes long, max is " UFO_SIZE_T "\n", len, sizeof(header->gameVersion));
		return false;
	}
	len = strlen(header->gameDate);
	if (len > sizeof(header->gameDate)) {
		cgi->Com_Printf("gameDate is " UFO_SIZE_T " Bytes long, max is " UFO_SIZE_T "\n", len, sizeof(header->gameDate));
		return false;
	}
	len = strlen(header->realDate);
	if (len > sizeof(header->realDate)) {
		cgi->Com_Printf("realDate is " UFO_SIZE_T " Bytes long, max is " UFO_SIZE_T "\n", len, sizeof(header->realDate));
		return false;
	}
	if (header->subsystems != 0 && header->subsystems != saveSubsystemsAmount) {
		cgi->Com_DPrintf(DEBUG_CLIENT, "Savefile has incompatible amount of subsystems\n");
	}

	/* saved games should not be bigger than 15MB */
	if (header->xmlSize > 15 * 1024 * 1024) {
		cgi->Com_Printf("Save size seems to be to large (over 15 MB) %i.\n", header->xmlSize);
		return false;
	}
	if (header->version == 0) {
		cgi->Com_Printf("Version is invalid - must be greater than zero\n");
		return false;
	}
	if (header->version > SAVE_FILE_VERSION) {
		cgi->Com_Printf("Savefile is newer than the game!\n");
	}
	return true;
}

/**
 * @brief Loads and verifies a savegame header
 * @param[in] filename Name of the file to load header from (without path and extension)
 * @param[out] header Pointer to the header structure to fill
 * @return @c true on success @c false on failure
 */
bool SAV_LoadHeader (const char* filename, saveFileHeader_t* header)
{
	assert(filename);
	assert(header);

	char path[MAX_OSPATH];
	cgi->GetRelativeSavePath(path, sizeof(path));
	Q_strcat(path, sizeof(path), "%s.%s", filename, SAVEGAME_EXTENSION);
	ScopedFile f;
	cgi->FS_OpenFile(path, &f, FILE_READ);
	if (!f) {
		cgi->Com_Printf("Couldn't open file '%s'\n", filename);
		return false;
	}

	if (cgi->FS_Read(header, sizeof(saveFileHeader_t), &f) != sizeof(saveFileHeader_t)) {
		cgi->Com_Printf("Warning: Could not read %lu bytes from savefile %s.%s\n", sizeof(saveFileHeader_t), filename, SAVEGAME_EXTENSION);
		return false;
	}

	header->compressed = LittleLong(header->compressed);
	header->version = LittleLong(header->version);
	header->xmlSize = LittleLong(header->xmlSize);

	if (!SAV_VerifyHeader(header)) {
		cgi->Com_Printf("The Header of the savegame '%s.%s' is corrupted.\n", filename, SAVEGAME_EXTENSION);
		return false;
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

	cgi->GetRelativeSavePath(filename, sizeof(filename));
	Q_strcat(filename, sizeof(filename), "%s.%s", file, SAVEGAME_EXTENSION);
	ScopedFile f;
	const int clen = cgi->FS_OpenFile(filename, &f, FILE_READ);
	if (!f) {
		cgi->Com_Printf("Couldn't open file '%s'\n", filename);
		*error = "File not found";
		return false;
	}

	byte* const cbuf = Mem_PoolAllocTypeN(byte, clen + 1 /* for '\0' if not compressed */, cp_campaignPool);
	if (cgi->FS_Read(cbuf, clen, &f) != clen)
		cgi->Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
	cgi->Com_Printf("Loading savegame xml (size %d)\n", clen);

	saveFileHeader_t header;
	memcpy(&header, cbuf, sizeof(header));
	/* swap all int values if needed */
	header.compressed = LittleLong(header.compressed);
	header.version = LittleLong(header.version);
	header.xmlSize = LittleLong(header.xmlSize);
	/* doing some header verification */
	if (!SAV_VerifyHeader(&header)) {
		/* our header is not valid, we MUST abort loading the game! */
		cgi->Com_Printf("The Header of the savegame '%s.%s' is corrupted. Loading aborted\n", filename, SAVEGAME_EXTENSION);
		cgi->Free(cbuf);
		*error = "Corrupted header";
		return false;
	}

	cgi->Com_Printf("Loading savegame\n"
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
			cgi->Com_Printf("Error decompressing data in '%s'.\n", filename);
			return false;
		}
		topNode = cgi->XML_Parse((const char*)buf);
		if (!topNode) {
			cgi->Free(buf);
			cgi->Com_Printf("Error: Failure in loading the xml data!\n");
			*error = "Corrupted xml data";
			return false;
		}
		cgi->Free(buf);
	} else {
		topNode = cgi->XML_Parse((const char*)(cbuf + sizeof(header)));
		cgi->Free(cbuf);
		if (!topNode) {
			cgi->Com_Printf("Error: Failure in loading the xml data!\n");
			*error = "Corrupted xml data";
			return false;
		}
	}

	/* doing a subsystem run */
	cgi->GAME_ReloadMode();
	xmlNode_t* node = cgi->XML_GetNode(topNode, SAVE_ROOTNODE);
	if (!node) {
		cgi->Com_Printf("Error: Failure in loading the xml data! (savegame node not found)\n");
		mxmlDelete(topNode);
		*error = "Invalid xml data";
		return false;
	}

	cgi->Com_Printf("Load '%s' %d subsystems\n", filename, saveSubsystemsAmount);
	for (int i = 0; i < saveSubsystemsAmount; i++) {
		if (!saveSubsystems[i].load)
			continue;
		cgi->Com_Printf("...Running subsystem '%s'\n", saveSubsystems[i].name);
		if (!saveSubsystems[i].load(node)) {
			cgi->Com_Printf("...subsystem '%s' returned false - savegame could not be loaded\n",
					saveSubsystems[i].name);
			*error = va("Could not load subsystem %s", saveSubsystems[i].name);
			return false;
		} else
			cgi->Com_Printf("...subsystem '%s' - loaded.\n", saveSubsystems[i].name);
	}
	mxmlDelete(node);
	mxmlDelete(topNode);

	if (!SAV_GameActionsAfterLoad()) {
		cgi->Com_Printf("Savegame postprocessing returned false - savegame could not be loaded\n");
		*error = "Postprocessing failed";
		return false;
	}

	cgi->Com_Printf("File '%s' successfully loaded from %s xml savegame.\n",
			filename, header.compressed ? "compressed" : "");

	cgi->UI_InitStack("geoscape", "campaign_main");
	return true;
}

/**
 * @brief Determines if saving is allowed
 */
bool SAV_GameSaveAllowed (char** error = nullptr)
{
	if (!CP_IsRunning()) {
		if (error)
			*error = _("Saving is not possible: Campaign is not active.");
		return false;
	}
	if (cgi->CL_OnBattlescape()) {
		if (error)
			*error = _("Saving is not possible: Cannot save the Battlescape.");
		return false;
	}
	if (!B_AtLeastOneExists()) {
		if (error)
			*error = _("Saving is not possible: No base is built.");
		return false;
	}
	return true;
}

/**
 * @brief This is a savegame function which stores the game in xml-Format.
 * @param[in] filename The Filename to save to (without extension)
 * @param[in] comment Description of the savegame
 * @param[out] error On failure an errormessage may be set.
 */
bool SAV_GameSave (const char* filename, const char* comment, char** error)
{
	if (!SAV_GameSaveAllowed(error)) {
		cgi->Com_Printf("%s", *error);
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
	CP_DateConvertLong(ccs.date, &date);
	Com_sprintf(message, sizeof(message), _("%i %s %02i"),
		date.year, Date_GetMonthName(date.month - 1), date.day);
	cgi->XML_AddString(node, SAVE_GAMEDATE, message);
	/* working through all subsystems. perhaps we should redesign it, order is not important anymore */
	cgi->Com_Printf("Calling subsystems\n");
	for (int i = 0; i < saveSubsystemsAmount; i++) {
		if (!saveSubsystems[i].save)
			continue;
		if (!saveSubsystems[i].save(node))
			cgi->Com_Printf("...subsystem '%s' failed to save the data\n", saveSubsystems[i].name);
		else
			cgi->Com_Printf("...subsystem '%s' - saved\n", saveSubsystems[i].name);
	}

	/* calculate the needed buffer size */
	saveFileHeader_t header;
	OBJZERO(header);
	header.compressed = LittleLong(save_compressed->integer);
	header.version = LittleLong(SAVE_FILE_VERSION);
	header.subsystems = LittleLong(saveSubsystemsAmount);
	Q_strncpyz(header.name, comment, sizeof(header.name));
	Q_strncpyz(header.gameVersion, UFO_VERSION, sizeof(header.gameVersion));
	CP_DateConvertLong(ccs.date, &date);
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
		cgi->Com_Printf("Error: Could not allocate enough memory to save this game\n");
		return false;
	}
	int res = mxmlSaveString(topNode, (char*)buf, requiredBufferLength + 1, MXML_NO_CALLBACK);
	mxmlDelete(topNode);
	cgi->Com_Printf("XML Written to buffer (%d Bytes)\n", res);

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
			cgi->Com_Printf("Memory error compressing save-game data (%s) (Error: %i)!\n", comment, res);
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

	cgi->Com_Printf("added %s subsystem\n", subsystem->name);
	return true;
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

	cgi->Com_Printf("\n--- save subsystem initialization --\n");

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

	save_compressed = cgi->Cvar_Get("save_compressed", "1", CVAR_ARCHIVE, "Save the savefiles compressed if set to 1");
}
