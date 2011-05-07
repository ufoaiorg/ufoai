/**
 * @file cl_game.c
 * @brief Shared game type code
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "cl_game.h"
#include "../client.h"
#include "cl_game_team.h"
#include "../battlescape/cl_localentity.h"
#include "../battlescape/cl_hud.h"
#include "../battlescape/cl_parse.h"
#include "../ui/ui_main.h"
#include "../ui/ui_nodes.h"
#include "../ui/ui_popup.h"
#include "../cl_team.h"
#include "../battlescape/events/e_main.h"
#include "../cl_inventory.h"
#include "../ui/node/ui_node_model.h"
#include "../../shared/parse.h"
#include "../../common/filesys.h"

#ifdef HARD_LINKED_CGAME
#include "cl_game_campaign.h"
#include "cl_game_multiplayer.h"
#include "cl_game_skirmish.h"

static const cgame_api_t gameTypeList[] = {
	GetCGameMultiplayerAPI,
	GetCGameCampaignAPI,
	GetCGameSkirmishAPI
};

static const char *cgameMenu;

const cgame_export_t *GetCGameAPI (const cgame_import_t *import)
{
	const size_t len = lengthof(gameTypeList);
	int i;

	if (cgameMenu == NULL)
		return NULL;

	for (i = 0; i < len; i++) {
		const cgame_api_t list = gameTypeList[i];
		const cgame_export_t *cgame = list(import);
		if (Q_streq(cgame->menu, cgameMenu)) {
			return cgame;
		}
	}

	return NULL;
}
#endif

static equipDef_t equipDefStandard;

/**
 * @brief static character array that can be used by a game mode to store the needed character values.
 */
static character_t characters[MAX_ACTIVETEAM];

/**
 * @brief Returns a character that can be used to store the game type specific character values
 * @note The returned pointer is a reference to static memory
 * @param index The index of the character array. This value must be greater than 0 and not bigger than the
 * value @c GAME_GetCharacterArraySize returned
 * @sa GAME_GetCharacterArraySize
 * @sa GAME_ResetCharacters
 * @return A character slot
 */
character_t* GAME_GetCharacter (int index)
{
	if (index < 0 || index >= lengthof(characters))
		Com_Error(ERR_DROP, "Out of bounds character access");

	return &characters[index];
}

/**
 * @return The size of the static character array
 * @sa GAME_GetCharacter
 * @sa GAME_ResetCharacters
 */
size_t GAME_GetCharacterArraySize (void)
{
	return lengthof(characters);
}

/**
 * @brief Reset all characters in the static character array
 * @sa GAME_GetCharacterArraySize
 * @sa GAME_GetCharacter
 */
void GAME_ResetCharacters (void)
{
	OBJZERO(characters);
	chrDisplayList.num = 0;
}

void GAME_AppendTeamMember (int memberIndex, const char *teamDefID, const equipDef_t *ed)
{
	character_t *chr;

	if (ed == NULL)
		Com_Error(ERR_DROP, "No equipment definition given");

	chr = GAME_GetCharacter(memberIndex);

	CL_GenerateCharacter(chr, teamDefID);
	/* pack equipment */
	cls.i.EquipActor(&cls.i, &chr->i, ed, chr->teamDef);

	chrDisplayList.chr[memberIndex] = chr;
	chrDisplayList.num++;
}

void GAME_GenerateTeam (const char *teamDefID, const equipDef_t *ed, int teamMembers)
{
	int i;

	if (teamMembers > GAME_GetCharacterArraySize())
		Com_Error(ERR_DROP, "More than the allowed amount of team members");

	if (ed == NULL)
		Com_Error(ERR_DROP, "No equipment definition given");

	GAME_ResetCharacters();

	for (i = 0; i < teamMembers; i++)
		GAME_AppendTeamMember(i, teamDefID, ed);
}

static const cgame_export_t *GAME_GetCurrentType (void)
{
	return cls.gametype;
}

void GAME_ReloadMode (void)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list != NULL) {
		GAME_SetMode(NULL);
		GAME_SetMode(list);
	}
}

qboolean GAME_IsMultiplayer (void)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list != NULL) {
		const qboolean isMultiplayer = list->isMultiplayer == 1;
		return isMultiplayer;
	}

	return qfalse;
}

/**
 * @brief This is called when a client quits the battlescape
 * @sa GAME_StartBattlescape
 */
void GAME_EndBattlescape (void)
{
	Cvar_SetValue("cl_onbattlescape", 0.0);
	Com_Printf("Used inventory slots after battle: %i\n", cls.i.GetUsedSlots(&cls.i));
}

/**
 * @brief Send end round announcements
 * @param playerNum The server player number of the player that has ended the round
 * @param team The team the player is in
 */
void GAME_EndRoundAnnounce (int playerNum, int team)
{
	/** @todo do we need the team number here? isn't the playernum enough to get the team? */
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list != NULL && list->EndRoundAnnounce)
		list->EndRoundAnnounce(playerNum, team);
}

/**
 * @brief Shows game type specific item information (if it's not resolvable via @c objDef_t).
 * @param[in] node The menu node to show the information in.
 * @param[in] string The id of the 'thing' to show information for.
 */
void GAME_DisplayItemInfo (uiNode_t *node, const char *string)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list != NULL && list->GetModelForItem) {
		const char *model = list->GetModelForItem(string);
		UI_DrawModelNode(node, model);
	}
}

void GAME_SetServerInfo (const char *server, const char *serverport)
{
	Q_strncpyz(cls.servername, server, sizeof(cls.servername));
	Q_strncpyz(cls.serverport, serverport, sizeof(cls.serverport));
}

/**
 * @sa CL_PingServers_f
 */
static void CL_QueryMasterServer (const char *action, http_callback_t callback)
{
	HTTP_GetURL(va("%s/ufo/masterserver.php?%s", masterserver_url->string, action), callback);
}

qboolean GAME_HandleServerCommand (const char *command, struct dbuffer *msg)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (!list || list->HandleServerCommand == NULL)
		return qfalse;

	return list->HandleServerCommand(command, msg);
}

void GAME_AddChatMessage (const char *format, ...)
{
	va_list argptr;
	char string[4096];

	const cgame_export_t *list = GAME_GetCurrentType();
	if (!list || list->AddChatMessage == NULL)
		return;

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	list->AddChatMessage(string);
}

qboolean GAME_IsTeamEmpty (void)
{
	return chrDisplayList.num == 0;
}

static void GAME_NET_OOB_Printf (struct net_stream *s, const char *format, ...)
{
	va_list argptr;
	char string[4096];

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	NET_OOB_Printf(s, "%s", string);
}

static void GAME_NET_OOB_Printf2 (const char *format, ...)
{
	va_list argptr;
	char string[4096];

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	NET_OOB_Printf(cls.netStream, "%s", string);
}

static void GAME_UI_Popup (const char *title, const char *format, ...)
{
	va_list argptr;

	va_start(argptr, format);
	Q_vsnprintf(popupText, sizeof(popupText), format, argptr);
	va_end(argptr);

	UI_Popup(title, popupText);
}

static void* GAME_StrDup (const char *string)
{
	return Mem_PoolStrDup(string, cl_genericPool, 0);
}

static void GAME_Free (void *ptr)
{
	Mem_Free(ptr);
}

static const cgame_import_t* GAME_GetImportData (void)
{
	static cgame_import_t gameImport;
	static cgame_import_t *cgi = NULL;

	if (cgi == NULL) {
		cgi = &gameImport;

		cgi->csi = &csi;

		cgi->Cmd_AddCommand = Cmd_AddCommand;
		cgi->Cmd_Argc = Cmd_Argc;
		cgi->Cmd_Args = Cmd_Args;
		cgi->Cmd_Argv = Cmd_Argv;
		cgi->Cmd_ExecuteString = Cmd_ExecuteString;
		cgi->Cmd_RemoveCommand = Cmd_RemoveCommand;
		cgi->Cmd_AddParamCompleteFunction = Cmd_AddParamCompleteFunction;
		cgi->Cmd_GenericCompleteFunction = Cmd_GenericCompleteFunction;

		cgi->Cbuf_AddText = Cbuf_AddText;

		cgi->LIST_AddString = LIST_AddString;
		cgi->LIST_ContainsString = LIST_ContainsString;

		cgi->Cvar_Delete = Cvar_Delete;
		cgi->Cvar_Get = Cvar_Get;
		cgi->Cvar_GetInteger = Cvar_GetInteger;
		cgi->Cvar_GetValue = Cvar_GetValue;
		cgi->Cvar_VariableStringOld = Cvar_VariableStringOld;
		cgi->Cvar_Set = Cvar_Set;
		cgi->Cvar_SetValue = Cvar_SetValue;
		cgi->Cvar_GetString = Cvar_GetString;
		cgi->Cvar_ForceSet = Cvar_ForceSet;

		cgi->FS_FreeFile = FS_FreeFile;
		cgi->FS_LoadFile = FS_LoadFile;
		cgi->FS_CheckFile = FS_CheckFile;

		cgi->UI_AddOption = UI_AddOption;
		cgi->UI_ExecuteConfunc = UI_ExecuteConfunc;
		cgi->UI_InitStack = UI_InitStack;
		cgi->UI_Popup = GAME_UI_Popup;
		/*cgi->UI_PopupList = UI_PopupList;*/
		cgi->UI_PopWindow = UI_PopWindow;
		cgi->UI_PushWindow = UI_PushWindow;
		cgi->UI_RegisterLinkedListText = UI_RegisterLinkedListText;
		cgi->UI_RegisterOption = UI_RegisterOption;
		cgi->UI_RegisterText = UI_RegisterText;
		cgi->UI_ResetData = UI_ResetData;
		cgi->UI_UpdateInvisOptions = UI_UpdateInvisOptions;
		cgi->UI_GetOption = UI_GetOption;
		/*gi->UI_TextNodeSelectLine = UI_TextNodeSelectLine;*/

		cgi->NET_StreamSetCallback = NET_StreamSetCallback;
		cgi->NET_Connect = NET_Connect;
		cgi->NET_ReadString = NET_ReadString;
		cgi->NET_ReadStringLine = NET_ReadStringLine;
		cgi->NET_ReadByte = NET_ReadByte;
		cgi->NET_ReadMsg = NET_ReadMsg;
		cgi->NET_StreamGetData = NET_StreamGetData;
		cgi->NET_StreamSetData = NET_StreamSetData;
		cgi->NET_StreamFree = NET_StreamFree;
		cgi->NET_StreamPeerToName = NET_StreamPeerToName;
		cgi->NET_SockaddrToStrings = NET_SockaddrToStrings;
		cgi->NET_DatagramSocketNew = NET_DatagramSocketNew;
		cgi->NET_DatagramBroadcast = NET_DatagramBroadcast;
		cgi->NET_DatagramSocketClose = NET_DatagramSocketClose;
		cgi->NET_OOB_Printf = GAME_NET_OOB_Printf;
		cgi->NET_OOB_Printf2 = GAME_NET_OOB_Printf2;

		cgi->Com_ServerState = Com_ServerState;
		cgi->Com_Printf = Com_Printf;
		cgi->Com_DPrintf = Com_DPrintf;
		cgi->Com_Parse = Com_Parse;
		cgi->Com_Error = Com_Error;
		cgi->Com_DropShipTypeToShortName = Com_DropShipTypeToShortName;
		cgi->Com_UFOCrashedTypeToShortName = Com_UFOCrashedTypeToShortName;
		cgi->Com_UFOTypeToShortName = Com_UFOTypeToShortName;
		cgi->Com_GetRandomMapAssemblyNameForCraft = Com_GetRandomMapAssemblyNameForCraft;
		cgi->Com_SetGameType = Com_SetGameType;

		cgi->SV_ShutdownWhenEmpty = SV_ShutdownWhenEmpty;
		cgi->SV_Shutdown = SV_Shutdown;

		cgi->CL_Drop = CL_Drop;
		cgi->CL_Milliseconds = CL_Milliseconds;
		cgi->CL_PlayerGetName = CL_PlayerGetName;
		cgi->CL_GetPlayerNum = CL_GetPlayerNum;
		cgi->CL_SetClientState = CL_SetClientState;
		cgi->CL_GetClientState = CL_GetClientState;
		cgi->CL_Disconnect = CL_Disconnect;
		cgi->CL_QueryMasterServer = CL_QueryMasterServer;

		cgi->GAME_SwitchCurrentSelectedMap = GAME_SwitchCurrentSelectedMap;
		cgi->GAME_GetCurrentSelectedMap = GAME_GetCurrentSelectedMap;
		cgi->GAME_GetCurrentTeam = GAME_GetCurrentTeam;
		cgi->GAME_StrDup = GAME_StrDup;
		cgi->GAME_AutoTeam = GAME_AutoTeam;
		cgi->GAME_GetCharacterArraySize = GAME_GetCharacterArraySize;
		cgi->GAME_IsTeamEmpty = GAME_IsTeamEmpty;
		cgi->GAME_LoadDefaultTeam = GAME_LoadDefaultTeam;
		cgi->GAME_AppendTeamMember = GAME_AppendTeamMember;
		cgi->GAME_ReloadMode = GAME_ReloadMode;
		cgi->GAME_SetServerInfo = GAME_SetServerInfo;

		cgi->Free = GAME_Free;

		cgi->R_LoadImage = R_LoadImage;
		cgi->R_SoftenTexture = R_SoftenTexture;

		/*cgi->S_SetSampleRepeatRate = S_SetSampleRepeatRate;*/
		cgi->S_StartLocalSample = S_StartLocalSample;

		cgi->INV_GetEquipmentDefinitionByID = INV_GetEquipmentDefinitionByID;

		cgi->Sys_Error = Sys_Error;

		cgi->HUD_InitUI = HUD_InitUI;
		cgi->HUD_DisplayMessage = HUD_DisplayMessage;

		cgi->XML_AddBool = XML_AddBool;
		cgi->XML_AddBoolValue = XML_AddBoolValue;
		cgi->XML_AddByte = XML_AddByte;
		cgi->XML_AddByteValue = XML_AddByteValue;
		cgi->XML_AddDate = XML_AddDate;
		cgi->XML_AddDouble = XML_AddDouble;
		cgi->XML_AddDoubleValue = XML_AddDoubleValue;
		cgi->XML_AddFloat = XML_AddFloat;
		cgi->XML_AddFloatValue = XML_AddFloatValue;
		cgi->XML_AddInt = XML_AddInt;
		cgi->XML_AddIntValue = XML_AddIntValue;
		cgi->XML_AddLong = XML_AddLong;
		cgi->XML_AddLongValue = XML_AddLongValue;
		cgi->XML_AddNode = XML_AddNode;
		cgi->XML_AddPos2 = XML_AddPos2;
		cgi->XML_AddPos3 = XML_AddPos3;
		cgi->XML_AddShort = XML_AddShort;
		cgi->XML_AddShortValue = XML_AddShortValue;
		cgi->XML_AddString = XML_AddString;
		cgi->XML_AddStringValue = XML_AddStringValue;
	}

	return cgi;
}

static const int TAG_INVENTORY = 17462;

static void GAME_FreeInventory (void *data)
{
	Mem_Free(data);
}

static void *GAME_AllocInventoryMemory (size_t size)
{
	return Mem_PoolAlloc(size, cl_genericPool, TAG_INVENTORY);
}


static void GAME_FreeAllInventory (void)
{
	Mem_FreeTag(cl_genericPool, TAG_INVENTORY);
}

static const inventoryImport_t inventoryImport = { GAME_FreeInventory, GAME_FreeAllInventory, GAME_AllocInventoryMemory };

void GAME_UnloadGame (void)
{
#ifndef HARD_LINKED_CGAME
	if (cls.cgameLibrary) {
		Com_Printf("Unload the cgame library\n");
		SDL_UnloadObject(cls.cgameLibrary);
		cls.cgameLibrary = NULL;
	}
#endif
}

void GAME_SwitchCurrentSelectedMap (int step)
{
	cls.currentSelectedMap += step;

	if (cls.currentSelectedMap < 0)
		cls.currentSelectedMap = cls.numMDs - 1;
	cls.currentSelectedMap %= cls.numMDs;
}

const mapDef_t* GAME_GetCurrentSelectedMap (void)
{
	return Com_GetMapDefByIDX(cls.currentSelectedMap);
}

int GAME_GetCurrentTeam (void)
{
	return cls.team;
}

void GAME_SetMode (const cgame_export_t *gametype)
{
	const cgame_export_t *list;

	if (cls.gametype == gametype)
		return;

	GAME_ResetCharacters();
	OBJZERO(cl.chrList);

	list = GAME_GetCurrentType();
	if (list) {
		Com_Printf("Shutdown gametype '%s'\n", list->name);
		list->Shutdown();

		/* we dont need to go back to "main" stack if we are already on this stack */
		if (!UI_IsWindowOnStack("main"))
			UI_InitStack("main", "", qtrue, qtrue);
	}

	cls.gametype = gametype;

	CL_Disconnect();

	list = GAME_GetCurrentType();
	if (list) {
		Com_Printf("Change gametype to '%s'\n", list->name);
		/* inventory structure switched/initialized */
		INV_DestroyInventory(&cls.i);
		INV_InitInventory(list->name, &cls.i, &csi, &inventoryImport);
		list->Init();
	}
}

static void UI_MapInfoGetNext (int step)
{
	const mapDef_t *md;
	int ref = cls.currentSelectedMap;

	while (qtrue) {
		cls.currentSelectedMap += step;
		if (cls.currentSelectedMap < 0)
			cls.currentSelectedMap = cls.numMDs - 1;
		cls.currentSelectedMap %= cls.numMDs;

		md = Com_GetMapDefByIDX(cls.currentSelectedMap);

		/* avoid infinit loop */
		if (ref == cls.currentSelectedMap)
			break;
		/* special purpose maps are not startable without the specific context */
		if (md->map[0] == '.')
			continue;

		if (md->map[0] != '+' && FS_CheckFile("maps/%s.bsp", md->map) != -1)
			break;
		if (md->map[0] == '+' && FS_CheckFile("maps/%s.ump", md->map + 1) != -1)
			break;
	}
}

/**
 * @brief Prints the map info for the server creation dialogue
 * @todo Skip special map that start with a '.' (e.g. .baseattack)
 */
static void UI_MapInfo (int step)
{
	const char *mapname;
	const mapDef_t *md;
	const cgame_export_t *list = GAME_GetCurrentType();

	if (!list)
		return;

	if (!cls.numMDs)
		return;

	UI_MapInfoGetNext(step);

	md = list->MapInfo(step);
	if (!md)
		return;

	mapname = md->map;
	/* skip random map char */
	Cvar_Set("mn_svmapid", md->id);
	if (mapname[0] == '+') {
		Cvar_Set("mn_svmapname", va("%s %s", md->map, md->param ? md->param : ""));
		mapname++;
	} else {
		Cvar_Set("mn_svmapname", md->map);
	}

	if (FS_CheckFile("pics/maps/shots/%s.jpg", mapname) != -1)
		Cvar_Set("mn_mappic", va("maps/shots/%s", mapname));
	else
		Cvar_Set("mn_mappic", "maps/shots/default");

	if (FS_CheckFile("pics/maps/shots/%s_2.jpg", mapname) != -1)
		Cvar_Set("mn_mappic2", va("maps/shots/%s_2", mapname));
	else
		Cvar_Set("mn_mappic2", "maps/shots/default");

	if (FS_CheckFile("pics/maps/shots/%s_3.jpg", mapname) != -1)
		Cvar_Set("mn_mappic3", va("maps/shots/%s_3", mapname));
	else
		Cvar_Set("mn_mappic3", "maps/shots/default");
}

static void UI_RequestMapList_f (void)
{
	const char *callbackCmd;
	int i;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <callback>\n", Cmd_Argv(0));
		return;
	}

	if (!cls.numMDs)
		return;

	callbackCmd = Cmd_Argv(1);

	Cbuf_AddText(va("%s begin;", callbackCmd));

	for (i = 0; i < cls.numMDs; i++) {
		const mapDef_t *md = Com_GetMapDefByIDX(i);
		const char *preview;

		/* special purpose maps are not startable without the specific context */
		if (md->map[0] == '.')
			continue;

		/* do we have the map file? */
		if (md->map[0] != '+' && FS_CheckFile("maps/%s.bsp", md->map) == -1)
			continue;
		if (md->map[0] == '+' && FS_CheckFile("maps/%s.ump", md->map + 1) == -1)
			continue;

		preview = md->map;
		if (preview[0] == '+')
			preview++;
		if (FS_CheckFile("pics/maps/shots/%s.jpg", preview) == -1)
			preview = "default";

		Cbuf_AddText(va("%s add \"%s\" \"%s\";", callbackCmd, md->id, preview));
	}
	Cbuf_AddText(va("%s end;", callbackCmd));
}

static void UI_GetMaps_f (void)
{
	UI_MapInfo(0);
}

/**
 * @brief Select the next available map.
 */
static void UI_NextMap_f (void)
{
	UI_MapInfo(1);
}

/**
 * @brief Select the previous available map.
 */
static void UI_PreviousMap_f (void)
{
	UI_MapInfo(-1);
}

static void UI_SelectMap_f (void)
{
	const char *mapname;
	int i;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <mapname>\n", Cmd_Argv(0));
		return;
	}

	if (!cls.numMDs)
		return;

	mapname = Cmd_Argv(1);

	for (i = 0; i < cls.numMDs; i++) {
		const mapDef_t *md = Com_GetMapDefByIDX(i);
		if (!Q_streq(md->map, mapname))
			continue;
		cls.currentSelectedMap = i;
		UI_MapInfo(0);
		return;
	}

	for (i = 0; i < cls.numMDs; i++) {
		const mapDef_t *md = Com_GetMapDefByIDX(i);
		if (!Q_streq(md->id, mapname))
			continue;
		cls.currentSelectedMap = i;
		UI_MapInfo(0);
		return;
	}

	Com_Printf("Could not find map %s\n", mapname);
}

typedef struct cgameType_s {
	char id[MAX_VAR];		/**< the id is also the file basename */
	char window[MAX_VAR];	/**< the ui window id where this game type should become active for */
	char name[MAX_VAR];		/**< translatable ui name */
} cgameType_t;

#define MAX_CGAMETYPES 16
static cgameType_t cgameTypes[MAX_CGAMETYPES];
static int numCGameTypes;

/** @brief Valid equipment definition values from script files. */
static const value_t cgame_vals[] = {
	{"window", V_STRING, offsetof(cgameType_t, window), 0},
	{"name", V_STRING, offsetof(cgameType_t, name), 0},

	{NULL, 0, 0, 0}
};

void GAME_ParseModes (const char *name, const char **text)
{
	const char *errhead = "GAME_ParseModes: unexpected end of file (cgame ";
	cgameType_t *ed;
	const char *token;
	const value_t *vp;
	int i;

	/* search for equipments with same name */
	for (i = 0; i < numCGameTypes; i++)
		if (Q_streq(name, cgameTypes[i].id))
			break;

	if (i < numCGameTypes) {
		Com_Printf("GAME_ParseModes: cgame def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (i >= MAX_CGAMETYPES)
		Sys_Error("GAME_ParseModes: MAX_CGAMETYPES exceeded\n");

	/* initialize the equipment definition */
	ed = &cgameTypes[numCGameTypes++];
	OBJZERO(*ed);

	Q_strncpyz(ed->id, name, sizeof(ed->id));

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("GAME_ParseModes: cgame def \"%s\" without body ignored\n", name);
		numCGameTypes--;
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text || *token == '}')
			return;

		for (vp = cgame_vals; vp->string; vp++)
			if (Q_streq(token, vp->string)) {
				/* found a definition */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;
				Com_EParseValue(ed, token, vp->type, vp->ofs, vp->size);
				break;
			}

		if (!vp->string) {
			Sys_Error("unknown token in cgame definition %s: '%s'", ed->id, token);
		}
	} while (*text);
}

#ifndef HARD_LINKED_CGAME
static qboolean GAME_LoadGame (const char *path, const char *name)
{
	char fullPath[MAX_OSPATH];

	Com_sprintf(fullPath, sizeof(fullPath), "%s/cgame-%s_"CPUSTRING".%s", path, name, SO_EXT);
	cls.cgameLibrary = SDL_LoadObject(fullPath);
	if (!cls.cgameLibrary) {
		Com_sprintf(fullPath, sizeof(fullPath), "%s/cgame-%s.%s", path, name, SO_EXT);
		cls.cgameLibrary = SDL_LoadObject(fullPath);
	}

	if (cls.cgameLibrary) {
		Com_Printf("found at '%s'\n", path);
		return qtrue;
	} else {
		Com_Printf("not found at '%s'\n", path);
		Com_DPrintf(DEBUG_SYSTEM, "%s\n", SDL_GetError());
		return qfalse;
	}
}
#endif

static const cgame_export_t *GAME_GetCGameAPI (const char *name)
{
#ifndef HARD_LINKED_CGAME
	cgame_api_t GetCGameAPI;
	const char *path;

	if (cls.cgameLibrary)
		Com_Error(ERR_FATAL, "GAME_GetCGameAPI without GAME_UnloadGame");

	Com_Printf("------- Loading cgame-%s.%s -------\n", name, SO_EXT);

#ifdef PKGLIBDIR
	GAME_LoadGame(PKGLIBDIR, name);
#endif

	/* now run through the search paths */
	path = NULL;
	while (!cls.cgameLibrary) {
		path = FS_NextPath(path);
		if (!path)
			/* couldn't find one anywhere */
			return NULL;
		else if (GAME_LoadGame(path, name))
			break;
	}

	GetCGameAPI = (cgame_api_t)(uintptr_t)SDL_LoadFunction(cls.cgameLibrary, "GetCGameAPI");
	if (!GetCGameAPI) {
		GAME_UnloadGame();
		return NULL;
	}
#endif

	return GetCGameAPI(GAME_GetImportData());
}

static const cgame_export_t *GAME_GetCGameAPI_ (const cgameType_t *t)
{
#ifdef HARD_LINKED_CGAME
	cgameMenu = t->window;
#endif
	return GAME_GetCGameAPI(t->id);
}

/**
 * @brief Decides with game mode should be set - takes the menu as reference
 */
static void GAME_SetMode_f (void)
{
	const char *modeName;
	int i;

	if (Cmd_Argc() == 2)
		modeName = Cmd_Argv(1);
	else
		modeName = UI_GetActiveWindowName();

	if (modeName[0] == '\0')
		return;

	for (i = 0; i < numCGameTypes; i++) {
		cgameType_t *t = &cgameTypes[i];
		if (Q_streq(t->window, modeName)) {
			const cgame_export_t *gametype;
			GAME_UnloadGame();

			gametype = GAME_GetCGameAPI_(t);
			GAME_SetMode(gametype);

			return;
		}
	}
	Com_Printf("GAME_SetMode_f: Mode '%s' not found\n", modeName);
}

static qboolean GAME_IsArmourUseableForTeam (const objDef_t *od, const teamDef_t *teamDef)
{
	if (teamDef != NULL && teamDef->armour && INV_IsArmour(od)) {
		if (CHRSH_IsTeamDefAlien(teamDef))
			return od->useable == TEAM_ALIEN;
		else if (teamDef->race == RACE_PHALANX_HUMAN)
			return od->useable == TEAM_PHALANX;
		else if (teamDef->race == RACE_CIVILIAN)
			return od->useable == TEAM_CIVILIAN;
		else
			return qfalse;
	}

	return qtrue;
}

qboolean GAME_ItemIsUseable (const objDef_t *od)
{
	const cgame_export_t *list = GAME_GetCurrentType();

	if (INV_IsArmour(od)) {
		const char *teamDefID = GAME_GetTeamDef();
		const teamDef_t *teamDef = Com_GetTeamDefinitionByID((teamDefID));

		/* Don't allow armour for other teams */
		if (!GAME_IsArmourUseableForTeam(od, teamDef))
			return qfalse;
	}

	if (list && list->IsItemUseable)
		return list->IsItemUseable(od);

	return qtrue;
}

/**
 * @brief After a mission was finished this function is called
 * @param msg The network message buffer
 * @param winner The winning team
 * @param numSpawned The amounts of all spawned actors per team
 * @param numAlive The amount of survivors per team
 * @param numKilled The amount of killed actors for all teams. The first dimension contains
 * the attacker team, the second the victim team
 * @param numStunned The amount of stunned actors for all teams. The first dimension contains
 * the attacker team, the second the victim team
 */
void GAME_HandleResults (struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS])
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list)
		list->Results(msg, winner, numSpawned, numAlive, numKilled, numStunned);
	else
		CL_Drop();
}

/**
 * @sa G_WriteItem
 * @sa G_ReadItem
 * @note The amount of the item_t struct should not be needed here - because
 * the amount is only valid for idFloor and idEquip
 */
static void GAME_NetSendItem (struct dbuffer *buf, item_t item, containerIndex_t container, int x, int y)
{
	const int ammoIdx = item.m ? item.m->idx : NONE;
	const eventRegister_t *eventData = CL_GetEvent(EV_INV_TRANSFER);
	assert(item.t);
	Com_DPrintf(DEBUG_CLIENT, "GAME_NetSendItem: Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n",
		item.t->id, container, item.t->idx, item.a, ammoIdx, x, y);
	NET_WriteFormat(buf, eventData->formatString, item.t->idx, item.a, ammoIdx, container, x, y, item.rotated, item.amount);
}

/**
 * @sa G_SendInventory
 */
static void GAME_NetSendInventory (struct dbuffer *buf, const inventory_t *i)
{
	containerIndex_t container;
	int nr = 0;
	const invList_t *ic;

	for (container = 0; container < csi.numIDs; container++) {
		for (ic = i->c[container]; ic; ic = ic->next)
			nr++;
	}

	NET_WriteShort(buf, nr * INV_INVENTORY_BYTES);
	for (container = 0; container < csi.numIDs; container++) {
		for (ic = i->c[container]; ic; ic = ic->next)
			GAME_NetSendItem(buf, ic->item, container, ic->x, ic->y);
	}
}

/**
 * @brief Send the character information to the server that is needed to spawn the soldiers of the player.
 * @param[out] buf The net channel buffer to write the character data into.
 * @param[in] chr The character to get the data from.
 */
static void GAME_NetSendCharacter (struct dbuffer * buf, const character_t *chr)
{
	int j;

	if (!chr)
		Com_Error(ERR_DROP, "No character given");
	if (chr->fieldSize != ACTOR_SIZE_2x2 && chr->fieldSize != ACTOR_SIZE_NORMAL)
		Com_Error(ERR_DROP, "Invalid character size given for character '%s': %i",
				chr->name, chr->fieldSize);
	if (chr->teamDef == NULL)
		Com_Error(ERR_DROP, "Character with no teamdef set (%s)", chr->name);

	NET_WriteByte(buf, chr->fieldSize);
	NET_WriteShort(buf, chr->ucn);
	NET_WriteString(buf, chr->name);

	/* model */
	NET_WriteString(buf, chr->path);
	NET_WriteString(buf, chr->body);
	NET_WriteString(buf, chr->head);
	NET_WriteByte(buf, chr->skin);

	NET_WriteShort(buf, chr->HP);
	NET_WriteShort(buf, chr->maxHP);
	NET_WriteByte(buf, chr->teamDef->idx);
	NET_WriteByte(buf, chr->gender);
	NET_WriteByte(buf, chr->STUN);
	NET_WriteByte(buf, chr->morale);

	for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
		NET_WriteLong(buf, chr->score.experience[j]);
	for (j = 0; j < SKILL_NUM_TYPES; j++)
		NET_WriteByte(buf, chr->score.skills[j]);
	for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
		NET_WriteByte(buf, chr->score.initialSkills[j]);
	for (j = 0; j < KILLED_NUM_TYPES; j++)
		NET_WriteShort(buf, chr->score.kills[j]);
	for (j = 0; j < KILLED_NUM_TYPES; j++)
		NET_WriteShort(buf, chr->score.stuns[j]);
	NET_WriteShort(buf, chr->score.assignedMissions);
}

/**
 * @brief Stores a team-list (chr-list) info to buffer (which might be a network buffer, too).
 * @sa G_ClientTeamInfo
 * @sa MP_SaveTeamMultiplayerInfo
 * @note Called in CL_Precache_f to send the team info to server
 */
static void GAME_SendCurrentTeamSpawningInfo (struct dbuffer * buf, chrList_t *team)
{
	int i;

	/* header */
	NET_WriteByte(buf, clc_teaminfo);
	NET_WriteByte(buf, team->num);

	Com_DPrintf(DEBUG_CLIENT, "GAME_SendCurrentTeamSpawningInfo: Upload information about %i soldiers to server\n", team->num);
	for (i = 0; i < team->num; i++) {
		character_t *chr = team->chr[i];

		GAME_NetSendCharacter(buf, chr);

		GAME_NetSendInventory(buf, &chr->i);
	}
}

const char* GAME_GetTeamDef (void)
{
	const char *teamDefID;
	const cgame_export_t *list = GAME_GetCurrentType();

	if (list && list->GetTeamDef)
		return list->GetTeamDef();

	teamDefID = Cvar_GetString("cl_teamdef");
	if (teamDefID[0] == '\0')
		teamDefID = "phalanx";
	return teamDefID;
}

static qboolean GAME_Spawn (void)
{
	const size_t size = GAME_GetCharacterArraySize();
	int i;

	/* If there is no active gametype we create a team with default values.
	 * This is e.g. the case when someone starts a map with the map command */
	if (GAME_GetCurrentType() == NULL || chrDisplayList.num == 0) {
		const char *teamDefID = GAME_GetTeamDef();
		const equipDef_t *ed = INV_GetEquipmentDefinitionByID("multiplayer_initial");

		/* inventory structure switched/initialized */
		INV_DestroyInventory(&cls.i);
		INV_InitInventory("client", &cls.i, &csi, &inventoryImport);
		GAME_GenerateTeam(teamDefID, ed, size);
	}

	for (i = 0; i < size; i++)
		cl.chrList.chr[i] = chrDisplayList.chr[i];
	cl.chrList.num = chrDisplayList.num;

	return qtrue;
}

/**
 * @brief Called when the server sends the @c EV_START event.
 * @param isTeamPlay @c true if the game is a teamplay round. This can be interesting for
 * multiplayer based game types
 * @sa GAME_EndBattlescape
 */
void GAME_StartBattlescape (qboolean isTeamPlay)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	qboolean spawnStatus;

	Cvar_SetValue("cl_onbattlescape", 1.0);

	Cvar_Set("cl_maxworldlevel", va("%i", cl.mapMaxLevel - 1));
	if (list != NULL && list->StartBattlescape) {
		list->StartBattlescape(isTeamPlay);
	} else {
		HUD_InitUI(NULL, qtrue);
	}

	/* this callback is responsible to set up the cl.chrList */
	if (list && list->Spawn)
		spawnStatus = list->Spawn();
	else
		spawnStatus = GAME_Spawn();

	Com_Printf("Used inventory slots: %i\n", cls.i.GetUsedSlots(&cls.i));

	if (spawnStatus && cl.chrList.num > 0) {
		struct dbuffer *msg;

		/* send team info */
		msg = new_dbuffer();
		GAME_SendCurrentTeamSpawningInfo(msg, &cl.chrList);
		NET_WriteMsg(cls.netStream, msg);
	}
}

/**
 * @brief This is called if actors are spawned (or at least the spawning commands were send to
 * the server). This callback can e.g. be used to set initial actor states. E.g. request crouch and so on.
 * These events are executed without consuming time
 */
static void GAME_InitializeBattlescape (chrList_t *team)
{
	int i;
	const cgame_export_t *list = GAME_GetCurrentType();

	for (i = 0; i < lengthof(cl.teamList); i++) {
		UI_ExecuteConfunc("huddisable %i", i);
	}

	if (list && list->InitializeBattlescape)
		list->InitializeBattlescape(team);
}

/**
 * @brief Called during startup of mission to send team info
 */
void GAME_SpawnSoldiers (void)
{
	if (cl.chrList.num > 0) {
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, clc_stringcmd);
		NET_WriteString(msg, "spawn\n");
		NET_WriteMsg(cls.netStream, msg);

		GAME_InitializeBattlescape(&cl.chrList);
	}
}

equipDef_t *GAME_GetEquipmentDefinition (void)
{
	const cgame_export_t *list = GAME_GetCurrentType();

	if (list && list->GetEquipmentDefinition != NULL)
		return list->GetEquipmentDefinition();
	return &equipDefStandard;
}

void GAME_NofityEvent (event_t eventType)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list && list->NotifyEvent)
		list->NotifyEvent(eventType);
}

qboolean GAME_TeamIsKnown (const teamDef_t *teamDef)
{
	const cgame_export_t *list = GAME_GetCurrentType();

	if (!teamDef)
		return qfalse;

	if (list && list->IsTeamKnown != NULL)
		return list->IsTeamKnown(teamDef);
	return qtrue;
}

void GAME_CharacterCvars (const character_t *chr)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list && list->UpdateCharacterValues != NULL)
		list->UpdateCharacterValues(chr);
}

/**
 * @brief Let the aliens win the match
 */
static void GAME_Abort_f (void)
{
	/* aborting means letting the aliens win */
	Cbuf_AddText(va("sv win %i\n", TEAM_ALIEN));
}

void GAME_Drop (void)
{
	const cgame_export_t *list = GAME_GetCurrentType();

	if (list && list->Drop) {
		list->Drop();
	} else {
		SV_Shutdown("Drop", qfalse);
		GAME_SetMode(NULL);

		GAME_UnloadGame();

		UI_InitStack("main", NULL, qfalse, qtrue);
	}
}

/**
 * @brief Quits the current running game by calling the @c shutdown callback
 */
static void GAME_Exit_f (void)
{
	GAME_SetMode(NULL);

	GAME_UnloadGame();
}

/**
 * @brief Called every frame and allows us to hook into the current running game mode
 */
void GAME_Frame (void)
{
	const cgame_export_t *list;

	list = GAME_GetCurrentType();
	if (list && list->RunFrame != NULL)
		list->RunFrame();
}

/**
 * @brief Get a model for an item.
 * @param[in] od The object definition to get the model from.
 * @param[out] uiModel The menu model pointer.
 * @return The model path for the item. Never @c NULL.
 */
const char* GAME_GetModelForItem (const objDef_t *od, uiModel_t** uiModel)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list && list->GetModelForItem != NULL) {
		const char *model = list->GetModelForItem(od->id);
		if (model != NULL) {
			if (uiModel != NULL)
				*uiModel = UI_GetUIModel(model);
			return model;
		}
	}

	if (uiModel != NULL)
		*uiModel = NULL;
	return od->model;
}

mapDef_t* Com_GetMapDefinitionByID (const char *mapDefID)
{
	int i;

	assert(mapDefID);

	for (i = 0; i < cls.numMDs; i++) {
		mapDef_t *md = Com_GetMapDefByIDX(i);
		if (Q_streq(md->id, mapDefID))
			return md;
	}

	Com_DPrintf(DEBUG_CLIENT, "Com_GetMapDefinition: Could not find mapdef with id: '%s'\n", mapDefID);
	return NULL;
}

mapDef_t* Com_GetMapDefByIDX (int index)
{
	return &cls.mds[index];
}

/**
 * @brief Fills the game mode list entries with the parsed values from the script
 */
void GAME_InitUIData (void)
{
	int i;

	Com_Printf("----------- game modes -------------\n");
	for (i = 0; i < numCGameTypes; i++) {
		const cgameType_t *t = &cgameTypes[i];
		const cgame_export_t *e = GAME_GetCGameAPI_(t);
		if (e == NULL)
			continue;

		if (e->isMultiplayer)
			UI_ExecuteConfunc("game_addmode_multiplayer \"%s\" \"%s\"", t->window, t->name);
		else
			UI_ExecuteConfunc("game_addmode_singleplayer \"%s\" \"%s\"", t->window, t->name);
		Com_Printf("added %s\n", t->name);
		GAME_UnloadGame();
	}

	Com_Printf("added %i game modes\n", numCGameTypes);
}

void GAME_InitStartup (void)
{
	Cmd_AddCommand("game_setmode", GAME_SetMode_f, "Decides with game mode should be set - takes the menu as reference");
	Cmd_AddCommand("game_exit", GAME_Exit_f, "Abort the game and let the aliens/opponents win");
	Cmd_AddCommand("game_abort", GAME_Abort_f, "Abort the game and let the aliens/opponents win");
	Cmd_AddCommand("game_autoteam", GAME_AutoTeam_f, "Assign initial equipment to soldiers");
	Cmd_AddCommand("game_toggleactor", GAME_ToggleActorForTeam_f, NULL);
	Cmd_AddCommand("game_saveteamstate", GAME_SaveTeamState_f, NULL);
	Cmd_AddCommand("game_saveteam", GAME_SaveTeam_f, "Save a team slot");
	Cmd_AddCommand("game_loadteam", GAME_LoadTeam_f, "Load a team slot");
	Cmd_AddCommand("game_teamcomments", GAME_TeamSlotComments_f, "Fills the team selection menu with the team names");
	Cmd_AddCommand("game_teamupdate", GAME_UpdateTeamMenuParameters_f, "");
	Cmd_AddCommand("game_actorselect", GAME_ActorSelect_f, "Select an actor in the equipment menu");
	Cmd_AddCommand("mn_getmaps", UI_GetMaps_f, "The initial map to show");
	Cmd_AddCommand("mn_nextmap", UI_NextMap_f, "Switch to the next valid map for the selected gametype");
	Cmd_AddCommand("mn_prevmap", UI_PreviousMap_f, "Switch to the previous valid map for the selected gametype");
	Cmd_AddCommand("mn_selectmap", UI_SelectMap_f, "Switch to the map given by the parameter - may be invalid for the current gametype");
	Cmd_AddCommand("mn_requestmaplist", UI_RequestMapList_f, "Request to send the list of available maps for the current gametype to a command.");
}

void GAME_Shutdown (void)
{
}
