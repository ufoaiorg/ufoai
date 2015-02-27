/**
 * @file
 * @brief Shared game type code
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

#include "cl_game.h"
#include "../client.h"
#include "../cl_language.h"
#include "cl_game_team.h"
#include "../battlescape/cl_localentity.h"
#include "../battlescape/cl_hud.h"
#include "../battlescape/cl_parse.h"
#include "../ui/ui_main.h"
#include "../ui/ui_draw.h"
#include "../ui/ui_nodes.h"
#include "../ui/ui_popup.h"
#include "../ui/ui_render.h"
#include "../ui/ui_windows.h"
#include "../ui/ui_sprite.h"
#include "../ui/ui_font.h"
#include "../ui/ui_tooltip.h"
#include "../ui/node/ui_node_container.h"
#include "../ui/node/ui_node_messagelist.h"
#include "../ui/node/ui_node_model.h"
#include "../cl_team.h"
#include "../web/web_cgame.h"
#include "../battlescape/events/e_main.h"
#include "../cl_inventory.h"
#include "../../shared/parse.h"
#include "../../common/filesys.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_framebuffer.h"
#include "../renderer/r_geoscape.h"

#include <set>
#include <string>

#define MAX_CGAMETYPES 16
static cgameType_t cgameTypes[MAX_CGAMETYPES];
static int numCGameTypes;

static inline const cgame_export_t* GAME_GetCurrentType (void)
{
	return cls.gametype;
}

class GAMECvarListener: public CvarListener
{
private:
	typedef std::set<const struct cvar_s* > GameCvars;
	GameCvars _cvars;
public:
	~GAMECvarListener ()
	{
		_cvars.clear();
	}

	void onCreate (const struct cvar_s* cvar)
	{
		const cgame_export_t* list = GAME_GetCurrentType();
		if (list)
			_cvars.insert(cvar);
	}

	void onDelete (const struct cvar_s* cvar)
	{
		_cvars.erase(cvar);
	}

	void onGameModeChange ()
	{
		GameCvars copy = _cvars;
		for (GameCvars::const_iterator i = copy.begin(); i != copy.end(); ++i) {
			const struct cvar_s* cvar = *i;
			if (cvar->flags == 0) {
				const cgame_export_t* list = GAME_GetCurrentType();
				Com_DPrintf(DEBUG_CLIENT, "Delete cvar %s because it was created in the context of the cgame %s\n",
						cvar->name, list ? list->name : "none");
				Cvar_Delete(cvar->name);
			}
		}
	}
};

class GAMECmdListener: public CmdListener
{
private:
	typedef std::set<const char* > GameCmds;
	GameCmds _cmds;
public:
	~GAMECmdListener ()
	{
		_cmds.clear();
	}

	void onAdd (const char* cmdName)
	{
		const cgame_export_t* list = GAME_GetCurrentType();
		if (list) {
			_cmds.insert(cmdName);
		}
	}

	void onRemove (const char* cmdName)
	{
		_cmds.erase(cmdName);
	}

	void onGameModeChange ()
	{
		GameCmds copy = _cmds;
		for (GameCmds::const_iterator i = copy.begin(); i != copy.end(); ++i) {
			const char* cmdName = *i;
			const cgame_export_t* list = GAME_GetCurrentType();
			Com_DPrintf(DEBUG_COMMANDS, "Remove command %s because it was created in the context of the cgame %s\n",
					cmdName, list ? list->name : "none");
			Cmd_RemoveCommand(cmdName);
		}
	}
};

static SharedPtr<GAMECvarListener> cvarListener(new GAMECvarListener());
static SharedPtr<GAMECmdListener> cmdListener(new GAMECmdListener());

/* @todo: remove me - this should be per geoscape node data */
geoscapeData_t geoscapeData;

#ifdef HARD_LINKED_CGAME
#include "campaign/cl_game_campaign.h"
#include "multiplayer/cl_game_multiplayer.h"
#include "skirmish/cl_game_skirmish.h"

static const cgame_api_t gameTypeList[] = {
	GetCGameMultiplayerAPI,
	GetCGameCampaignAPI,
	GetCGameSkirmishAPI
};

static const char* cgameMenu;

const cgame_export_t* GetCGameAPI (const cgame_import_t* import)
{
	const size_t len = lengthof(gameTypeList);

	if (cgameMenu == nullptr)
		return nullptr;

	for (int i = 0; i < len; i++) {
		const cgame_api_t list = gameTypeList[i];
		const cgame_export_t* cgame = list(import);
		if (Q_streq(cgame->menu, cgameMenu)) {
			return cgame;
		}
	}

	return nullptr;
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
 * @brief Returns a character that can be used to store the game type specific character values
 * @note The returned pointer is a reference to static memory
 * @param ucn The unique character number
 * @return @c null if no character with the specified ucn was found.
 */
character_t* GAME_GetCharacterByUCN (int ucn)
{
	const int size = lengthof(characters);
	for (int i = 0; i < size; i++) {
		character_t* chr = &characters[i];
		if (chr->ucn == ucn)
			return chr;
	}
	return nullptr;
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

const char* GAME_GetCurrentName (void)
{
	const cgame_export_t* cgame = GAME_GetCurrentType();
	if (cgame == nullptr)
		return nullptr;
	return cgame->menu;
}
/**
 * @brief Reset all characters in the static character array
 * @sa GAME_GetCharacterArraySize
 * @sa GAME_GetCharacter
 */
void GAME_ResetCharacters (void)
{
	for (int i = 0; i < MAX_ACTIVETEAM; i++)
		characters[i].init();
	LIST_Delete(&chrDisplayList);
	UI_ExecuteConfunc("team_membersclear");
}

void GAME_AppendTeamMember (int memberIndex, const char* teamDefID, const equipDef_t* ed)
{
	if (ed == nullptr)
		Com_Error(ERR_DROP, "No equipment definition given");

	character_t* chr = GAME_GetCharacter(memberIndex);

	CL_GenerateCharacter(chr, teamDefID);

	const objDef_t* weapon = chr->teamDef->onlyWeapon;
	if (chr->teamDef->robot && !weapon) {
		/* This is likely an UGV, try to guess which one and get the weapon */
		const char* ugvId = strstr(chr->teamDef->id, "ugv");
		const ugv_t* ugv = Com_GetUGVByIDSilent(ugvId);
		if (ugv)
			weapon = INVSH_GetItemByID(ugv->weapon);
	}
	/* pack equipment */
	cls.i.EquipActor(chr, ed, weapon, GAME_GetChrMaxLoad(chr));

	LIST_AddPointer(&chrDisplayList, (void*)chr);

	UI_ExecuteConfunc("team_memberadd %i \"%s\" \"%s\" %i", memberIndex, chr->name, chr->head, chr->headSkin);
}

void GAME_GenerateTeam (const char* teamDefID, const equipDef_t* ed, int teamMembers)
{
	if (teamMembers > GAME_GetCharacterArraySize())
		Com_Error(ERR_DROP, "More than the allowed amount of team members");

	if (ed == nullptr)
		Com_Error(ERR_DROP, "No equipment definition given");

	GAME_ResetCharacters();

	for (int i = 0; i < teamMembers; i++)
		GAME_AppendTeamMember(i, teamDefID, ed);
}

void GAME_ReloadMode (void)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list != nullptr) {
		GAME_SetMode(nullptr);
		GAME_SetMode(list);
	}
}

bool GAME_IsMultiplayer (void)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list != nullptr) {
		const bool isMultiplayer = list->isMultiplayer == 1;
		return isMultiplayer;
	}

	return false;
}

/**
 * @brief This is called when a client quits the battlescape
 * @sa GAME_StartBattlescape
 */
void GAME_EndBattlescape (void)
{
	Cvar_Set("cl_onbattlescape", "0");
	Com_Printf("Used inventory slots after battle: %i\n", cls.i.GetUsedSlots());
}

/**
 * @brief Send end round announcements
 * @param playerNum The server player number of the player that has ended the round
 * @param team The team the player is in
 */
void GAME_EndRoundAnnounce (int playerNum, int team)
{
	/** @todo do we need the team number here? isn't the playernum enough to get the team? */
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list != nullptr && list->EndRoundAnnounce)
		list->EndRoundAnnounce(playerNum, team);
}

/**
 * @brief Shows game type specific item information (if it's not resolvable via @c objDef_t).
 * @param[in] node The menu node to show the information in.
 * @param[in] string The id of the 'thing' to show information for.
 */
void GAME_DisplayItemInfo (uiNode_t* node, const char* string)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list != nullptr && list->GetModelForItem) {
		const char* model = list->GetModelForItem(string);
		UI_DrawModelNode(node, model);
	}
}

void GAME_SetServerInfo (const char* server, const char* serverport)
{
	Q_strncpyz(cls.servername, server, sizeof(cls.servername));
	Q_strncpyz(cls.serverport, serverport, sizeof(cls.serverport));
}

/**
 * @sa CL_PingServers_f
 */
static void CL_QueryMasterServer (const char* action, http_callback_t callback)
{
	HTTP_GetURL(va("%s/ufo/masterserver.php?%s", masterserver_url->string, action), callback);
}

bool GAME_HandleServerCommand (const char* command, dbuffer* msg)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (!list || list->HandleServerCommand == nullptr)
		return false;

	return list->HandleServerCommand(command, msg);
}

void GAME_AddChatMessage (const char* format, ...)
{
	va_list argptr;
	char string[4096];

	const cgame_export_t* list = GAME_GetCurrentType();
	if (!list || list->AddChatMessage == nullptr)
		return;

	S_StartLocalSample("misc/talk", SND_VOLUME_DEFAULT);

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	list->AddChatMessage(string);
}

bool GAME_IsTeamEmpty (void)
{
	return LIST_IsEmpty(chrDisplayList);
}

static void GAME_NET_OOB_Printf (struct net_stream* s, const char* format, ...)
{
	va_list argptr;
	char string[4096];

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	NET_OOB_Printf(s, "%s", string);
}

static void GAME_NET_OOB_Printf2 (const char* format, ...)
{
	va_list argptr;
	char string[4096];

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	NET_OOB_Printf(cls.netStream, "%s", string);
}

static void GAME_UI_Popup (const char* title, const char* format, ...)
{
	va_list argptr;

	va_start(argptr, format);
	Q_vsnprintf(popupText, sizeof(popupText), format, argptr);
	va_end(argptr);

	UI_Popup(title, popupText);
}

static char* GAME_StrDup (const char* string)
{
	return Mem_PoolStrDup(string, cl_genericPool, 0);
}

static void GAME_Free (void* ptr)
{
	Mem_Free(ptr);
}

static memPool_t* GAME_CreatePool (const char* name)
{
	return Mem_CreatePool(name);
}

static void GAME_FreePool (memPool_t* pool)
{
	Mem_FreePool(pool);
}

static char* GAME_PoolStrDup (const char* in, memPool_t* pool, const int tagNum)
{
	return Mem_PoolStrDup(in, pool, tagNum);
}

static void GAME_DestroyInventory (Inventory* const inv)
{
	cls.i.destroyInventory(inv);
}

static void GAME_EquipActor (character_t* const chr, const equipDef_t* ed, const objDef_t* weapon, int maxWeight)
{
	cls.i.EquipActor(chr, ed, weapon, maxWeight);
}

static bool GAME_RemoveFromInventory (Inventory* const i, const invDef_t* container, Item* fItem)
{
	return cls.i.removeFromInventory(i, container, fItem);
}

static void GAME_WebUpload (int category, const char* filename)
{
	WEB_CGameUpload(GAME_GetCurrentName(), category, filename);
}

static void GAME_WebDelete (int category, const char* filename)
{
	WEB_CGameDelete(GAME_GetCurrentName(), category, filename);
}

static void GAME_WebDownloadFromUser (int category, const char* filename, int userId)
{
	WEB_CGameDownloadFromUser(GAME_GetCurrentName(), category, filename, userId);
}

static void GAME_WebListForUser (int category, int userId)
{
	WEB_CGameListForUser(GAME_GetCurrentName(), category, userId);
}

static void GAME_SetNextUniqueCharacterNumber (int ucn)
{
	cls.nextUniqueCharacterNumber = ucn;
}

static int GAME_GetNextUniqueCharacterNumber (void)
{
	return cls.nextUniqueCharacterNumber;
}

static void GAME_CollectItems (void* data, int won, void (*collectItem)(void*, const objDef_t*, int), void (*collectAmmo) (void* , const Item*), void (*ownitems) (const Inventory*))
{
	le_t* le = nullptr;
	while ((le = LE_GetNextInUse(le))) {
		/* Winner collects everything on the floor, and everything carried
		 * by surviving actors. Loser only gets what their living team
		 * members carry. */
		if (LE_IsItem(le)) {
			if (won) {
				Item* i = le->getFloorContainer();
				for ( ; i; i = i->getNext()) {
					collectItem(data, i->def(), 1);
					if (i->isReloadable() && i->getAmmoLeft() > 0)
						collectAmmo(data, i);
				}
			}
		} else if (LE_IsActor(le)) {
			/* The items are already dropped to floor and are available
			 * as ET_ITEM if the actor is dead; or the actor is not ours. */
			/* First of all collect armour of dead or stunned actors (if won). */
			if (won && LE_IsDead(le)) {
				Item* item = le->inv.getArmour();
				if (item)
					collectItem(data, item->def(), 1);
			} else if (le->team == cls.team && !LE_IsDead(le)) {
				/* Finally, the living actor from our team. */
				ownitems(&le->inv);
			}
		}
	}
}

/**
 * @brief Collecting stunned and dead alien bodies after the mission.
 */
static void GAME_CollectAliens (void* data, void (*collect)(void*, const teamDef_t*, int, bool))
{
	le_t* le = nullptr;

	while ((le = LE_GetNextInUse(le))) {
		if (LE_IsActor(le) && LE_IsAlien(le)) {
			assert(le->teamDef);

			if (LE_IsStunned(le))
				collect(data, le->teamDef, 1, false);
			else if (LE_IsDead(le))
				collect(data, le->teamDef, 1, true);
		}
	}
}

static int UI_DrawString_ (const char* fontID, align_t align, int x, int y, const char* c)
{
	return UI_DrawString(fontID, align, x, y, 0, 0, 0, c);
}

static void UI_PushWindow_ (const char* name)
{
	UI_PushWindow(name);
}

static void UI_DrawNormImageByName_ (bool flip, float x, float y, float w, float h, float sh, float th, float sl, float tl, const char* name)
{
	UI_DrawNormImageByName(flip, x, y, w, h, sh, th, sl, tl, name);
}

static void R_UploadAlpha_ (const char* name, const byte* alphaData)
{
	image_t* image = R_GetImage(name);
	if (!image) {
		Com_Printf("Could not find image '%s'\n", name);
		return;
	}
	R_UploadAlpha(image, alphaData);
}

static void R_DrawImageCentered (int x, int y, const char* name)
{
	image_t* image = R_FindImage(name, it_pic);
	if (image)
		R_DrawImage(x - image->width / 2, y - image->height / 2, image);
}

static const char* Com_EParse_ (const char** text, const char* errhead, const char* errinfo)
{
	return Com_EParse(text, errhead, errinfo);
}

static const cgame_import_t* GAME_GetImportData (const cgameType_t* t)
{
	static cgame_import_t gameImport;
	static cgame_import_t* cgi = nullptr;

	if (cgi == nullptr) {
		cgi = &gameImport;

		cgi->ui_inventory = &ui_inventory;
		cgi->csi = &csi;

		cgi->r_xviAlpha = geoscapeData.r_xviAlpha;
		cgi->r_radarPic = geoscapeData.r_radarPic;
		cgi->r_radarSourcePic = geoscapeData.r_radarSourcePic;

		/** @todo add a wrapper here that stores the cgame command and removes them on shutdown automatically */
		cgi->Cmd_AddCommand = Cmd_AddCommand;
		cgi->Cmd_Argc = Cmd_Argc;
		cgi->Cmd_Args = Cmd_Args;
		cgi->Cmd_Argv = Cmd_Argv;
		cgi->Cmd_ExecuteString = Cmd_ExecuteString;
		cgi->Cmd_RemoveCommand = Cmd_RemoveCommand;
		cgi->Cmd_TableAddList = Cmd_TableAddList;
		cgi->Cmd_TableRemoveList = Cmd_TableRemoveList;
		cgi->Cmd_AddParamCompleteFunction = Cmd_AddParamCompleteFunction;
		cgi->Cmd_GenericCompleteFunction = Cmd_GenericCompleteFunction;
		cgi->Com_GetMapDefinitionByID = Com_GetMapDefinitionByID;

		cgi->Cbuf_AddText = Cbuf_AddText;
		cgi->Cbuf_Execute = Cbuf_Execute;

		cgi->LIST_PrependString = LIST_PrependString;
		cgi->LIST_AddString = LIST_AddString;
		cgi->LIST_AddStringSorted = LIST_AddStringSorted;
		cgi->LIST_AddPointer = LIST_AddPointer;
		cgi->LIST_Add = LIST_Add;
		cgi->LIST_ContainsString = LIST_ContainsString;
		cgi->LIST_GetPointer = LIST_GetPointer;
		cgi->LIST_Delete = LIST_Delete;
		cgi->LIST_RemoveEntry = LIST_RemoveEntry;
		cgi->LIST_IsEmpty = LIST_IsEmpty;
		cgi->LIST_Count = LIST_Count;
		cgi->LIST_CopyStructure = LIST_CopyStructure;
		cgi->LIST_GetByIdx = LIST_GetByIdx;
		cgi->LIST_Remove = LIST_Remove;
		cgi->LIST_Sort = LIST_Sort;
		cgi->LIST_GetRandom = LIST_GetRandom;

		cgi->Cvar_Delete = Cvar_Delete;
		/** @todo add a wrapper here that stores the cgame cvars and removes them on shutdown automatically */
		cgi->Cvar_Get = Cvar_Get;
		cgi->Cvar_GetInteger = Cvar_GetInteger;
		cgi->Cvar_GetValue = Cvar_GetValue;
		cgi->Cvar_VariableStringOld = Cvar_VariableStringOld;
		cgi->Cvar_Set = Cvar_Set;
		cgi->Cvar_SetValue = Cvar_SetValue;
		cgi->Cvar_GetString = Cvar_GetString;
		cgi->Cvar_ForceSet = Cvar_ForceSet;

		cgi->FS_CloseFile = FS_CloseFile;
		cgi->FS_FreeFile = FS_FreeFile;
		cgi->FS_OpenFile = FS_OpenFile;
		cgi->FS_LoadFile = FS_LoadFile;
		cgi->FS_CheckFile = FS_CheckFile;
		cgi->FS_WriteFile = FS_WriteFile;
		cgi->FS_RemoveFile = FS_RemoveFile;
		cgi->FS_Read = FS_Read;
		cgi->FS_BuildFileList = FS_BuildFileList;
		cgi->FS_NextFileFromFileList = FS_NextFileFromFileList;
		cgi->FS_NextScriptHeader = FS_NextScriptHeader;

		cgi->UI_AddOption = UI_AddOption;
		cgi->UI_ExecuteConfunc = UI_ExecuteConfunc;
		cgi->UI_InitStack = UI_InitStack;
		cgi->UI_Popup = GAME_UI_Popup;
		cgi->UI_PopupList = UI_PopupList;
		cgi->UI_PopWindow = UI_PopWindow;
		cgi->UI_PushWindow = UI_PushWindow_;
		cgi->UI_RegisterLinkedListText = UI_RegisterLinkedListText;
		cgi->UI_MessageGetStack = UI_MessageGetStack;
		cgi->UI_MessageAddStack = UI_MessageAddStack;
		cgi->UI_MessageResetStack = UI_MessageResetStack;
		cgi->UI_TextScrollEnd = UI_TextScrollEnd;
		cgi->UI_RegisterOption = UI_RegisterOption;
		cgi->UI_RegisterText = UI_RegisterText;
		cgi->UI_ResetData = UI_ResetData;
		cgi->UI_UpdateInvisOptions = UI_UpdateInvisOptions;
		cgi->UI_GetOption = UI_GetOption;
		cgi->UI_SortOptions = UI_SortOptions;
		cgi->UI_InitOptionIteratorAtIndex = UI_InitOptionIteratorAtIndex;
		cgi->UI_OptionIteratorNextOption = UI_OptionIteratorNextOption;
		cgi->UI_DrawString = UI_DrawString_;
		cgi->UI_GetFontFromNode = UI_GetFontFromNode;
		cgi->UI_DrawNormImageByName = UI_DrawNormImageByName_;
		cgi->UI_DrawRect = UI_DrawRect;
		cgi->UI_DrawFill = UI_DrawFill;
		cgi->UI_DrawTooltip = UI_DrawTooltip;
		cgi->UI_GetNodeAbsPos = UI_GetNodeAbsPos;
		cgi->UI_PopupButton = UI_PopupButton;
		cgi->UI_GetSpriteByName = UI_GetSpriteByName;
		cgi->UI_ContainerNodeUpdateEquipment = UI_ContainerNodeUpdateEquipment;
		cgi->UI_RegisterLineStrip = UI_RegisterLineStrip;
		cgi->UI_GetNodeByPath = UI_GetNodeByPath;
		cgi->UI_DisplayNotice = UI_DisplayNotice;
		cgi->UI_GetActiveWindowName = UI_GetActiveWindowName;
		cgi->UI_TextNodeSelectLine = UI_TextNodeSelectLine;

		cgi->CL_Translate = CL_Translate;

		cgi->NET_StreamSetCallback = NET_StreamSetCallback;
		cgi->NET_Connect = NET_Connect;
		cgi->NET_ReadString = NET_ReadString;
		cgi->NET_ReadStringLine = NET_ReadStringLine;
		cgi->NET_ReadByte = NET_ReadByte;
		cgi->NET_ReadShort = NET_ReadShort;
		cgi->NET_ReadLong = NET_ReadLong;
		cgi->NET_WriteByte = NET_WriteByte;
		cgi->NET_WriteShort = NET_WriteShort;
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
		cgi->Com_Error = Com_Error;
		cgi->Com_DropShipTypeToShortName = Com_DropShipTypeToShortName;
		cgi->Com_UFOCrashedTypeToShortName = Com_UFOCrashedTypeToShortName;
		cgi->Com_UFOTypeToShortName = Com_UFOTypeToShortName;
		cgi->Com_GetRandomMapAssemblyNameForCraft = Com_GetRandomMapAssemblyNameForCraft;
		cgi->Com_RegisterConstList = Com_RegisterConstList;
		cgi->Com_UnregisterConstList = Com_UnregisterConstList;
		cgi->Com_GetConstVariable = Com_GetConstVariable;
		cgi->Com_GetConstIntFromNamespace = Com_GetConstIntFromNamespace;
		cgi->Com_GetConstInt = Com_GetConstInt;
		cgi->Com_EParse = Com_EParse_;
		cgi->Com_EParseValue = Com_EParseValue;
		cgi->Com_ParseBoolean = Com_ParseBoolean;
		cgi->Com_ParseList = Com_ParseList;
		cgi->Com_ParseBlock = Com_ParseBlock;
		cgi->Com_ParseBlockToken = Com_ParseBlockToken;
		cgi->Com_ValueToStr = Com_ValueToStr;
		cgi->Com_GetTeamDefinitionByID = Com_GetTeamDefinitionByID;
		cgi->Com_UFOShortNameToID = Com_UFOShortNameToID;
		cgi->Com_GetRandomMapAssemblyNameForCrashedCraft = Com_GetRandomMapAssemblyNameForCrashedCraft;
		cgi->Com_SetGameType = Com_SetGameType;
		cgi->Com_RegisterConstInt = Com_RegisterConstInt;
		cgi->Com_UnregisterConstVariable = Com_UnregisterConstVariable;
		cgi->Com_Drop = Com_Drop;
		cgi->Com_GetUGVByID = Com_GetUGVByID;
		cgi->Com_GetUGVByIDSilent = Com_GetUGVByIDSilent;
		cgi->Com_DropShipShortNameToID = Com_DropShipShortNameToID;

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
		cgi->GAME_ChangeEquip = GAME_ChangeEquip;
		cgi->GAME_GetCharacterArraySize = GAME_GetCharacterArraySize;
		cgi->GAME_IsTeamEmpty = GAME_IsTeamEmpty;
		cgi->GAME_LoadDefaultTeam = GAME_LoadDefaultTeam;
		cgi->GAME_AppendTeamMember = GAME_AppendTeamMember;
		cgi->GAME_ReloadMode = GAME_ReloadMode;
		cgi->GAME_SetServerInfo = GAME_SetServerInfo;
		cgi->GAME_GetChrMaxLoad = GAME_GetChrMaxLoad;
		cgi->GAME_LoadCharacter = GAME_LoadCharacter;
		cgi->GAME_SaveCharacter = GAME_SaveCharacter;

		cgi->Alloc = _Mem_Alloc;
		cgi->Free = GAME_Free;
		cgi->CreatePool = GAME_CreatePool;
		cgi->FreePool = GAME_FreePool;
		cgi->PoolStrDup = GAME_PoolStrDup;

		cgi->R_LoadImage = R_LoadImage;
		cgi->R_SoftenTexture = R_SoftenTexture;
		cgi->R_ImageExists = R_ImageExists;
		cgi->R_Color = R_Color;
		cgi->R_DrawLineStrip = R_DrawLineStrip;
		cgi->R_DrawLine = R_DrawLine;
		cgi->R_DrawFill = R_DrawFill;
		cgi->R_DrawRect = R_DrawRect;
		cgi->R_DrawBloom = R_DrawBloom;
		cgi->R_Draw2DMapMarkers = R_Draw2DMapMarkers;
		cgi->R_Draw3DMapMarkers = R_Draw3DMapMarkers;
		cgi->R_UploadAlpha = R_UploadAlpha_;
		cgi->R_DrawImageCentered = R_DrawImageCentered;

		cgi->S_SetSampleRepeatRate = S_SetSampleRepeatRate;
		cgi->S_StartLocalSample = S_StartLocalSample;

		cgi->CL_GenerateCharacter = CL_GenerateCharacter;
		cgi->CL_OnBattlescape = CL_OnBattlescape;

		cgi->CL_ActorGetSkillString = CL_ActorGetSkillString;
		cgi->CL_UpdateCharacterValues = CL_UpdateCharacterValues;

		cgi->SetNextUniqueCharacterNumber = GAME_SetNextUniqueCharacterNumber;
		cgi->GetNextUniqueCharacterNumber = GAME_GetNextUniqueCharacterNumber;

		cgi->CollectItems = GAME_CollectItems;
		cgi->CollectAliens = GAME_CollectAliens;

		cgi->INV_GetEquipmentDefinitionByID = INV_GetEquipmentDefinitionByID;
		cgi->INV_DestroyInventory = GAME_DestroyInventory;
		cgi->INV_EquipActor = GAME_EquipActor;
		cgi->INV_RemoveFromInventory = GAME_RemoveFromInventory;

		cgi->INV_ItemDescription = INV_ItemDescription;
		cgi->INV_ItemMatchesFilter = INV_ItemMatchesFilter;
		cgi->INV_GetFilterType = INV_GetFilterType;
		cgi->INV_GetFilterTypeID = INV_GetFilterTypeID;

		cgi->WEB_Upload = GAME_WebUpload;
		cgi->WEB_Delete = GAME_WebDelete;
		cgi->WEB_DownloadFromUser = GAME_WebDownloadFromUser;
		cgi->WEB_ListForUser = GAME_WebListForUser;

		cgi->GetRelativeSavePath = GAME_GetRelativeSavePath;
		cgi->GetAbsoluteSavePath = GAME_GetAbsoluteSavePath;

		cgi->BEP_Evaluate = BEP_Evaluate;

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

		cgi->XML_GetBool = XML_GetBool;
		cgi->XML_GetInt = XML_GetInt;
		cgi->XML_GetShort = XML_GetShort;
		cgi->XML_GetLong = XML_GetLong;
		cgi->XML_GetString = XML_GetString;
		cgi->XML_GetFloat = XML_GetFloat;
		cgi->XML_GetDouble = XML_GetDouble;
		cgi->XML_Parse = XML_Parse;
		cgi->XML_GetPos2 = XML_GetPos2;
		cgi->XML_GetNextPos2 = XML_GetNextPos2;
		cgi->XML_GetPos3 = XML_GetPos3;
		cgi->XML_GetNextPos3 = XML_GetNextPos3;
		cgi->XML_GetDate = XML_GetDate;
		cgi->XML_GetNode = XML_GetNode;
		cgi->XML_GetNextNode = XML_GetNextNode;
	}

	cgi->cgameType = t;

	return cgi;
}

static const int TAG_INVENTORY = 17462;

static void GAME_FreeInventory (void* data)
{
	Mem_Free(data);
}

static void* GAME_AllocInventoryMemory (size_t size)
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
		GAME_SetMode(nullptr);
		Com_Printf("Unload the cgame library\n");
		SDL_UnloadObject(cls.cgameLibrary);
		cls.cgameLibrary = nullptr;
	}
#endif
}

void GAME_SwitchCurrentSelectedMap (int step)
{
	cls.currentSelectedMap += step;

	if (cls.currentSelectedMap < 0)
		cls.currentSelectedMap = csi.numMDs - 1;
	cls.currentSelectedMap %= csi.numMDs;
}

const mapDef_t* GAME_GetCurrentSelectedMap (void)
{
	return Com_GetMapDefByIDX(cls.currentSelectedMap);
}

int GAME_GetCurrentTeam (void)
{
	return cls.team;
}

void GAME_DrawMap (geoscapeData_t* data)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->MapDraw)
		list->MapDraw(data);
}

void GAME_DrawMapMarkers (uiNode_t* node)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->MapDrawMarkers)
		list->MapDrawMarkers(node);
}

void GAME_MapClick (uiNode_t* node, int x, int y, const vec2_t pos)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->MapClick)
		list->MapClick(node, x, y, pos);
}

void GAME_SetMode (const cgame_export_t* gametype)
{
	const cgame_export_t* list;

	if (cls.gametype == gametype)
		return;

	GAME_ResetCharacters();
	LIST_Delete(&cl.chrList);
	Cvar_FullSet("gamemode", "", CVAR_NOSET);

	list = GAME_GetCurrentType();
	if (list) {
		Com_Printf("Shutdown gametype '%s'\n", list->name);
		list->Shutdown();
		cvarListener->onGameModeChange();
		cmdListener->onGameModeChange();

		/* we dont need to go back to "main" stack if we are already on this stack */
		if (!UI_IsWindowOnStack("main"))
			UI_InitStack("main", "");
	}

	cls.gametype = gametype;

	CL_Disconnect();

	list = GAME_GetCurrentType();
	if (list) {
		Com_Printf("Change gametype to '%s'\n", list->name);
		/* inventory structure switched/initialized */
		cls.i.destroyInventoryInterface();
		cls.i.initInventory(list->name, &csi, &inventoryImport);
		list->Init();
		Cvar_FullSet("gamemode", list->menu, CVAR_NOSET);
	}
}

static void UI_MapInfoGetNext (int step)
{
	int ref = cls.currentSelectedMap;

	for (;;) {
		cls.currentSelectedMap += step;
		if (cls.currentSelectedMap < 0)
			cls.currentSelectedMap = csi.numMDs - 1;
		cls.currentSelectedMap %= csi.numMDs;

		const mapDef_t* md = Com_GetMapDefByIDX(cls.currentSelectedMap);

		/* avoid infinite loop */
		if (ref == cls.currentSelectedMap)
			break;
		/* special purpose maps are not startable without the specific context */
		if (md->mapTheme[0] == '.')
			continue;

		if (md->mapTheme[0] != '+' && FS_CheckFile("maps/%s.bsp", md->mapTheme) != -1)
			break;
		if (md->mapTheme[0] == '+' && FS_CheckFile("maps/%s.ump", md->mapTheme + 1) != -1)
			break;
	}
}

/**
 * @brief Prints the map info for the server creation dialogue
 */
static void UI_MapInfo (int step)
{
	const cgame_export_t* list = GAME_GetCurrentType();

	if (!list)
		return;

	if (!csi.numMDs)
		return;

	UI_MapInfoGetNext(step);

	const mapDef_t* md = list->MapInfo(step);
	if (!md)
		return;

	const char* mapname = md->mapTheme;
	/* skip random map char. */
	Cvar_Set("mn_svmapid", "%s", md->id);
	if (mapname[0] == '+') {
		Cvar_Set("mn_svmapname", "%s %s", md->mapTheme, md->params ? (const char*)LIST_GetRandom(md->params) : "");
		mapname++;
	} else {
		Cvar_Set("mn_svmapname", "%s", md->mapTheme);
	}

	if (R_ImageExists("pics/maps/shots/%s", mapname))
		Cvar_Set("mn_mappic", "maps/shots/%s", mapname);
	else
		Cvar_Set("mn_mappic", "maps/shots/default");

	if (R_ImageExists("pics/maps/shots/%s_2", mapname))
		Cvar_Set("mn_mappic2", "maps/shots/%s_2", mapname);
	else
		Cvar_Set("mn_mappic2", "maps/shots/default");

	if (R_ImageExists("pics/maps/shots/%s_3", mapname))
		Cvar_Set("mn_mappic3", "maps/shots/%s_3", mapname);
	else
		Cvar_Set("mn_mappic3", "maps/shots/default");
}

static void UI_RequestMapList_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <callback>\n", Cmd_Argv(0));
		return;
	}

	if (!csi.numMDs)
		return;

	const char* callbackCmd = Cmd_Argv(1);

	Cbuf_AddText("%s begin\n", callbackCmd);

	const mapDef_t* md;
	const bool multiplayer = GAME_IsMultiplayer();
	MapDef_ForeachCondition(md, multiplayer ? md->multiplayer : md->singleplayer) {
		const char* preview;

		/* special purpose maps are not startable without the specific context */
		if (md->mapTheme[0] == '.')
			continue;

		/* do we have the map file? */
		if (md->mapTheme[0] != '+' && FS_CheckFile("maps/%s.bsp", md->mapTheme) == -1)
			continue;
		if (md->mapTheme[0] == '+' && FS_CheckFile("maps/%s.ump", md->mapTheme + 1) == -1)
			continue;

		preview = md->mapTheme;
		if (preview[0] == '+')
			preview++;
		if (!R_ImageExists("pics/maps/shots/%s", preview))
			preview = "default";

		Cbuf_AddText("%s add \"%s\" \"%s\"\n", callbackCmd, md->id, preview);
	}
	Cbuf_AddText("%s end\n", callbackCmd);
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
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <mapname>\n", Cmd_Argv(0));
		return;
	}

	if (!csi.numMDs)
		return;

	const char* mapname = Cmd_Argv(1);
	int i = 0;
	const mapDef_t* md;

	MapDef_Foreach(md) {
		i++;
		if (!Q_streq(md->mapTheme, mapname))
			continue;
		cls.currentSelectedMap = i - 1;
		UI_MapInfo(0);
		return;
	}

	i = 0;
	MapDef_Foreach(md) {
		i++;
		if (!Q_streq(md->id, mapname))
			continue;
		cls.currentSelectedMap = i - 1;
		UI_MapInfo(0);
		return;
	}

	Com_Printf("Could not find map %s\n", mapname);
}

/** @brief Valid equipment definition values from script files. */
static const value_t cgame_vals[] = {
	{"window", V_STRING, offsetof(cgameType_t, window), 0},
	{"name", V_STRING, offsetof(cgameType_t, name), 0},
	{"equipments", V_LIST, offsetof(cgameType_t, equipmentList), 0},

	{nullptr, V_NULL, 0, 0}
};

void GAME_ParseModes (const char* name, const char** text)
{
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
		Sys_Error("GAME_ParseModes: MAX_CGAMETYPES exceeded");

	/* initialize the equipment definition */
	cgameType_t* cgame = &cgameTypes[numCGameTypes++];
	OBJZERO(*cgame);

	Q_strncpyz(cgame->id, name, sizeof(cgame->id));

	Com_ParseBlock(name, text, cgame, cgame_vals, nullptr);
}

#ifndef HARD_LINKED_CGAME
static bool GAME_LoadGame (const char* path, const char* name)
{
	char fullPath[MAX_OSPATH];

	Com_sprintf(fullPath, sizeof(fullPath), "%s/cgame-%s_" CPUSTRING ".%s", path, name, SO_EXT);
	cls.cgameLibrary = SDL_LoadObject(fullPath);
	if (!cls.cgameLibrary) {
		Com_sprintf(fullPath, sizeof(fullPath), "%s/cgame-%s.%s", path, name, SO_EXT);
		cls.cgameLibrary = SDL_LoadObject(fullPath);
	}

	if (cls.cgameLibrary) {
		Com_Printf("found at '%s'\n", path);
		return true;
	} else {
		Com_Printf("not found at '%s'\n", path);
		Com_DPrintf(DEBUG_SYSTEM, "%s\n", SDL_GetError());
		return false;
	}
}
#endif

static const cgame_export_t* GAME_GetCGameAPI (const cgameType_t* t)
{
	const char* name = t->id;
#ifndef HARD_LINKED_CGAME
	if (cls.cgameLibrary)
		Com_Error(ERR_FATAL, "GAME_GetCGameAPI without GAME_UnloadGame");

	Com_Printf("------- Loading cgame-%s.%s -------\n", name, SO_EXT);

#ifdef PKGLIBDIR
	GAME_LoadGame(PKGLIBDIR, name);
#endif

	/* now run through the search paths */
	const char* path = nullptr;
	while (!cls.cgameLibrary) {
		path = FS_NextPath(path);
		if (!path)
			/* couldn't find one anywhere */
			return nullptr;
		else if (GAME_LoadGame(path, name))
			break;
	}

	cgame_api_t GetCGameAPI = (cgame_api_t)(uintptr_t)SDL_LoadFunction(cls.cgameLibrary, "GetCGameAPI");
	if (!GetCGameAPI) {
		GAME_UnloadGame();
		return nullptr;
	}
#endif

	/* sanity checks */
	if (!LIST_IsEmpty(t->equipmentList)) {
		LIST_Foreach(t->equipmentList, char const, equipID) {
			if (INV_GetEquipmentDefinitionByID(equipID) == nullptr)
				Sys_Error("Could not find the equipDef '%s' in the cgame mode: '%s'", equipID, name);
		}
	}

	return GetCGameAPI(GAME_GetImportData(t));
}

static const cgame_export_t* GAME_GetCGameAPI_ (const cgameType_t* t)
{
#ifdef HARD_LINKED_CGAME
	cgameMenu = t->window;
#endif
	return GAME_GetCGameAPI(t);
}

/**
 * @brief Decides which game mode should be set - takes the menu as reference
 */
static void GAME_SetMode_f (void)
{
	const char* modeName;
	if (Cmd_Argc() == 2)
		modeName = Cmd_Argv(1);
	else
		modeName = UI_GetActiveWindowName();

	if (modeName[0] == '\0')
		return;

	for (int i = 0; i < numCGameTypes; i++) {
		cgameType_t* t = &cgameTypes[i];
		if (Q_streq(t->window, modeName)) {
			const cgame_export_t* gametype;
			GAME_UnloadGame();

			gametype = GAME_GetCGameAPI_(t);
			GAME_SetMode(gametype);

			return;
		}
	}
	Com_Printf("GAME_SetMode_f: Mode '%s' not found\n", modeName);
}

bool GAME_ItemIsUseable (const objDef_t* od)
{
	const cgame_export_t* list = GAME_GetCurrentType();

	if (od->isArmour()) {
		const char* teamDefID = GAME_GetTeamDef();
		const teamDef_t* teamDef = Com_GetTeamDefinitionByID(teamDefID);

		/* Don't allow armour for other teams */
		if (teamDef != nullptr && !CHRSH_IsArmourUseableForTeam(od, teamDef))
			return false;
	}

	if (list && list->IsItemUseable)
		return list->IsItemUseable(od);

	return true;
}

/**
 * @brief After a mission was finished this function is called
 * @param msg The network message buffer
 * @param winner The winning team or -1 if it was a draw
 * @param numSpawned The amounts of all spawned actors per team
 * @param numAlive The amount of survivors per team
 * @param numKilled The amount of killed actors for all teams. The first dimension contains
 * the attacker team, the second the victim team
 * @param numStunned The amount of stunned actors for all teams. The first dimension contains
 * the attacker team, the second the victim team
 * @param nextmap Indicates if there is another map to follow within the same msission
 */
void GAME_HandleResults (dbuffer* msg, int winner, int* numSpawned, int* numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS], bool nextmap)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list)
		list->Results(msg, winner, numSpawned, numAlive, numKilled, numStunned, nextmap);
	else
		CL_Drop();
}

/**
 * @sa G_WriteItem
 * @sa G_ReadItem
 * @note The amount of the Item should not be needed here - because
 * the amount is only valid for CID_FLOOR and CID_EQUIP
 */
static void GAME_NetSendItem (dbuffer* buf, const Item* item, containerIndex_t container, int x, int y)
{
	const int ammoIdx = item->ammoDef() ? item->ammoDef()->idx : NONE;
	const eventRegister_t* eventData = CL_GetEvent(EV_INV_TRANSFER);
	assert(item->def());
	Com_DPrintf(DEBUG_CLIENT, "GAME_NetSendItem: Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n",
		item->def()->id, container, item->def()->idx, item->getAmmoLeft(), ammoIdx, x, y);
	NET_WriteFormat(buf, eventData->formatString, item->def()->idx, item->getAmmoLeft(), ammoIdx, container, x, y, item->rotated, item->getAmount());
}

/**
 * @sa G_SendInventory
 */
static void GAME_NetSendInventory (dbuffer* buf, const Inventory* inv)
{
	const int nr = inv->countItems();

	NET_WriteShort(buf, nr);

	for (containerIndex_t container = 0; container < CID_MAX; container++) {
		if (INVDEF(container)->temp)
			continue;
		const Item* ic;
		for (ic = inv->getContainer2(container); ic; ic = ic->getNext()) {
			GAME_NetSendItem(buf, ic, container, ic->getX(), ic->getY());
		}
	}
}

/**
 * @brief Send the character information to the server that is needed to spawn the soldiers of the player.
 * @param[out] buf The net channel buffer to write the character data into.
 * @param[in] chr The character to get the data from.
 */
static void GAME_NetSendCharacter (dbuffer* buf, const character_t* chr)
{
	if (!chr)
		Com_Error(ERR_DROP, "No character given");
	if (chr->fieldSize != ACTOR_SIZE_2x2 && chr->fieldSize != ACTOR_SIZE_NORMAL)
		Com_Error(ERR_DROP, "Invalid character size given for character '%s': %i",
				chr->name, chr->fieldSize);
	if (chr->teamDef == nullptr)
		Com_Error(ERR_DROP, "Character with no teamdef set (%s)", chr->name);

	NET_WriteByte(buf, chr->fieldSize);
	NET_WriteShort(buf, chr->ucn);
	NET_WriteString(buf, chr->name);

	/* model */
	NET_WriteString(buf, chr->path);
	NET_WriteString(buf, chr->body);
	NET_WriteString(buf, chr->head);
	NET_WriteByte(buf, chr->bodySkin);
	NET_WriteByte(buf, chr->headSkin);

	NET_WriteShort(buf, chr->HP);
	NET_WriteShort(buf, chr->maxHP);
	NET_WriteByte(buf, chr->teamDef->idx);
	NET_WriteByte(buf, chr->gender);
	NET_WriteByte(buf, chr->STUN);
	NET_WriteByte(buf, chr->morale);

	for (int j = 0; j < chr->teamDef->bodyTemplate->numBodyParts(); ++j)
		NET_WriteByte(buf, chr->wounds.treatmentLevel[j]);

	for (int j = 0; j < SKILL_NUM_TYPES + 1; j++)
		NET_WriteLong(buf, chr->score.experience[j]);
	for (int j = 0; j < SKILL_NUM_TYPES; j++)
		NET_WriteByte(buf, chr->score.skills[j]);
	for (int j = 0; j < KILLED_NUM_TYPES; j++)
		NET_WriteShort(buf, chr->score.kills[j]);
	for (int j = 0; j < KILLED_NUM_TYPES; j++)
		NET_WriteShort(buf, chr->score.stuns[j]);
	NET_WriteShort(buf, chr->score.assignedMissions);
}

/**
 * @brief Stores a team-list (chr-list) info to buffer (which might be a network buffer, too).
 * @sa G_ClientTeamInfo
 * @sa MP_SaveTeamMultiplayerInfo
 * @note Called in CL_Precache_f to send the team info to server
 */
static void GAME_SendCurrentTeamSpawningInfo (dbuffer* buf, linkedList_t* team)
{
	const int teamSize = LIST_Count(team);

	/* header */
	NET_WriteByte(buf, clc_teaminfo);
	NET_WriteByte(buf, teamSize);

	LIST_Foreach(team, character_t, chr) {
		Inventory* inv = &chr->inv;

		/* unlink all temp containers */
		inv->resetTempContainers();

		GAME_NetSendCharacter(buf, chr);
		GAME_NetSendInventory(buf, inv);
	}
}

const char* GAME_GetTeamDef (void)
{
	const cgame_export_t* list = GAME_GetCurrentType();

	if (list && list->GetTeamDef)
		return list->GetTeamDef();

	const char* teamDefID = Cvar_GetString("cl_teamdef");
	if (teamDefID[0] == '\0')
		teamDefID = "phalanx";
	return teamDefID;
}

static bool GAME_Spawn (linkedList_t** chrList)
{
	const size_t size = GAME_GetCharacterArraySize();

	/* If there is no active gametype we create a team with default values.
	 * This is e.g. the case when someone starts a map with the map command */
	if (GAME_GetCurrentType() == nullptr || LIST_IsEmpty(chrDisplayList)) {
		const char* teamDefID = GAME_GetTeamDef();
		const equipDef_t* ed = INV_GetEquipmentDefinitionByID("multiplayer_initial");

		/* inventory structure switched/initialized */
		cls.i.destroyInventoryInterface();
		cls.i.initInventory("client", &csi, &inventoryImport);
		GAME_GenerateTeam(teamDefID, ed, size);
	}

	int count = 0;
	LIST_Foreach(chrDisplayList, character_t, chr) {
		if (count > size)
			break;
		LIST_AddPointer(chrList, (void*)chr);
	}

	return true;
}

/**
 * @brief Called when the server sends the @c EV_START event.
 * @param isTeamPlay @c true if the game is a teamplay round. This can be interesting for
 * multiplayer based game types
 * @sa GAME_EndBattlescape
 */
void GAME_StartBattlescape (bool isTeamPlay)
{
	const cgame_export_t* list = GAME_GetCurrentType();

	Cvar_Set("cl_onbattlescape", "1");

	Cvar_Set("cl_maxworldlevel", "%i", cl.mapMaxLevel - 1);
	if (list != nullptr && list->StartBattlescape) {
		list->StartBattlescape(isTeamPlay);
	} else {
		HUD_InitUI("missionoptions");
		GAME_InitMissionBriefing(_(CL_GetConfigString(CS_MAPTITLE)));
	}
}

void GAME_InitMissionBriefing (const char* title)
{
	const cgame_export_t* list = GAME_GetCurrentType();

	linkedList_t* victoryConditionsMsgIDs = nullptr;
	linkedList_t* missionBriefingMsgIDs = nullptr;

	/* allow the server to add e.g. the misc_mission victory condition */
	const char* serverVictoryMsgID = CL_GetConfigString(CS_VICTORY_CONDITIONS);
	if (Q_strvalid(serverVictoryMsgID))
		LIST_AddString(&victoryConditionsMsgIDs, serverVictoryMsgID);

	UI_PushWindow("mission_briefing");

	if (list != nullptr && list->InitMissionBriefing)
		list->InitMissionBriefing(&title, &victoryConditionsMsgIDs, &missionBriefingMsgIDs);

	/* if the cgame has nothing to contribute here, we will add a default victory condition */
	if (LIST_IsEmpty(victoryConditionsMsgIDs))
		LIST_AddString(&victoryConditionsMsgIDs, "*msgid:victory_condition_default");

	/* if the cgame has nothing to contribute here, we will add a default mission briefing */
	if (LIST_IsEmpty(missionBriefingMsgIDs))
		LIST_AddString(&missionBriefingMsgIDs, "*msgid:mission_briefing_default");

	UI_RegisterLinkedListText(TEXT_MISSIONBRIEFING, missionBriefingMsgIDs);
	UI_RegisterLinkedListText(TEXT_MISSIONBRIEFING_VICTORY_CONDITIONS, victoryConditionsMsgIDs);
	UI_RegisterText(TEXT_MISSIONBRIEFING_TITLE, title);
}

/**
 * @brief This is called if actors are spawned (or at least the spawning commands were send to
 * the server). This callback can e.g. be used to set initial actor states. E.g. request crouch and so on.
 * These events are executed without consuming time
 */
static void GAME_InitializeBattlescape (linkedList_t* team)
{
	const size_t size = lengthof(cl.teamList);
	for (int i = 0; i < size; i++) {
		UI_ExecuteConfunc("huddisable %i", i);
	}

	dbuffer msg;
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->InitializeBattlescape) {
		list->InitializeBattlescape(&msg, team);
	} else {
		const int teamSize = LIST_Count(team);
		dbuffer* m = &msg;

		NET_WriteByte(m, clc_initactorstates);
		NET_WriteByte(m, teamSize);

		LIST_Foreach(team, character_t, chr) {
			NET_WriteShort(m, chr->ucn);
			NET_WriteShort(m, STATE_REACTION);
			NET_WriteShort(m, ACTOR_HAND_NOT_SET);
			NET_WriteShort(m, NONE);
			NET_WriteShort(m, NONE);
		}
	}
	NET_WriteMsg(cls.netStream, msg);
}

/**
 * @brief Called during startup of mission to send team info
 */
void GAME_SpawnSoldiers (void)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	bool spawnStatus;

	/* this callback is responsible to set up the teamlist */
	if (list && list->Spawn)
		spawnStatus = list->Spawn(&cl.chrList);
	else
		spawnStatus = GAME_Spawn(&cl.chrList);

	Com_Printf("Used inventory slots: %i\n", cls.i.GetUsedSlots());

	if (spawnStatus && !LIST_IsEmpty(cl.chrList)) {
		/* send team info */
		dbuffer msg;
		GAME_SendCurrentTeamSpawningInfo(&msg, cl.chrList);
		NET_WriteMsg(cls.netStream, msg);
	}
}

void GAME_StartMatch (void)
{
	if (!LIST_IsEmpty(cl.chrList)) {
		dbuffer msg(12);
		NET_WriteByte(&msg, clc_stringcmd);
		NET_WriteString(&msg, NET_STATE_STARTMATCH "\n");
		NET_WriteMsg(cls.netStream, msg);

		GAME_InitializeBattlescape(cl.chrList);
	}
}

const char* GAME_GetRelativeSavePath (char* buf, size_t bufSize)
{
	Com_sprintf(buf, bufSize, "save/%s/", GAME_GetCurrentName());
	return buf;
}

const char* GAME_GetAbsoluteSavePath (char* buf, size_t bufSize)
{
	Com_sprintf(buf, bufSize, "%s/save/%s/", FS_Gamedir(), GAME_GetCurrentName());
	return buf;
}

equipDef_t* GAME_GetEquipmentDefinition (void)
{
	const cgame_export_t* list = GAME_GetCurrentType();

	if (list && list->GetEquipmentDefinition != nullptr)
		return list->GetEquipmentDefinition();
	return &equipDefStandard;
}

void GAME_NotifyEvent (event_t eventType)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->NotifyEvent)
		list->NotifyEvent(eventType);
}

bool GAME_TeamIsKnown (const teamDef_t* teamDef)
{
	const cgame_export_t* list = GAME_GetCurrentType();

	if (!teamDef)
		return false;

	if (list && list->IsTeamKnown != nullptr)
		return list->IsTeamKnown(teamDef);
	return true;
}

void GAME_CharacterCvars (const character_t* chr)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->UpdateCharacterValues != nullptr)
		list->UpdateCharacterValues(chr);
}

/**
 * @brief Let the aliens win the match
 */
static void GAME_Abort_f (void)
{
	/* aborting means letting the aliens win */
	Cbuf_AddText("sv win %i\n", TEAM_ALIEN);
}

void GAME_Drop (void)
{
	const cgame_export_t* list = GAME_GetCurrentType();

	if (list && list->Drop) {
		list->Drop();
	} else {
		SV_Shutdown("Drop", false);
		GAME_SetMode(nullptr);

		GAME_UnloadGame();

		UI_InitStack("main", nullptr);
	}
}

/**
 * @brief Quits the current running game by calling the @c shutdown callback
 */
static void GAME_Exit_f (void)
{
	GAME_SetMode(nullptr);

	GAME_UnloadGame();
}

/**
 * @brief Called every frame and allows us to hook into the current running game mode
 */
void GAME_Frame (void)
{
	/* don't run the cgame in console mode */
	if (cls.keyDest == key_console)
		return;

	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->RunFrame != nullptr)
		list->RunFrame(cls.frametime);
}

void GAME_DrawBase (int baseIdx, int x, int y, int w, int h, int col, int row, bool hover, int overlap)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->DrawBase != nullptr)
		list->DrawBase(baseIdx, x, y, w, h, col, row, hover, overlap);
}

void GAME_DrawBaseTooltip (int baseIdx, int x, int y, int col, int row)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->DrawBaseTooltip != nullptr)
		list->DrawBaseTooltip(baseIdx, x, y, col, row);
}

void GAME_DrawBaseLayout (int baseIdx, int x, int y, int totalMarge, int w, int h, int padding, const vec4_t bgcolor, const vec4_t color)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->DrawBaseLayout != nullptr)
		list->DrawBaseLayout(baseIdx, x, y, totalMarge, w, h, padding, bgcolor, color);
}

void GAME_HandleBaseClick (int baseIdx, int key, int col, int row)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->HandleBaseClick != nullptr)
		list->HandleBaseClick(baseIdx, key, col, row);
}

/**
 * @brief Get a model for an item.
 * @param[in] od The object definition to get the model from.
 * @param[out] uiModel The menu model pointer.
 * @return The model path for the item. Never @c nullptr.
 */
const char* GAME_GetModelForItem (const objDef_t* od, uiModel_t** uiModel)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->GetModelForItem != nullptr) {
		const char* model = list->GetModelForItem(od->id);
		if (model != nullptr) {
			if (uiModel != nullptr)
				*uiModel = UI_GetUIModel(model);
			return model;
		}
	}

	if (uiModel != nullptr)
		*uiModel = nullptr;
	return od->model;
}

/**
 * @brief Returns the currently selected character.
 * @return The selected character or @c nullptr.
 */
character_t* GAME_GetSelectedChr (void)
{
	const cgame_export_t* list = GAME_GetCurrentType();
	if (list && list->GetSelectedChr != nullptr)
		return list->GetSelectedChr();

	const int ucn = Cvar_GetInteger("mn_ucn");
	character_t* chr = nullptr;
	LIST_Foreach(chrDisplayList, character_t, chrTmp) {
		if (ucn == chrTmp->ucn) {
			chr = chrTmp;
			break;
		}
	}
	return chr;
}

/**
 * @brief Returns the max weight the given character can carry.
 * @param[in] chr The character to find the max load for.
 * @return The max load the character can carry or @c NONE.
 */
int GAME_GetChrMaxLoad (const character_t* chr)
{
	if (chr == nullptr)
		return NONE;

	const cgame_export_t* list = GAME_GetCurrentType();
	const int strength = chr->score.skills[ABILITY_POWER];
	if (list && list->GetChrMaxLoad != nullptr)
		return std::min(strength, list->GetChrMaxLoad(chr));
	return strength;
}

/**
 * @brief Changed the given cvar to the next/prev equipment definition
 */
const equipDef_t* GAME_ChangeEquip (const linkedList_t* equipmentList, changeEquipType_t changeType, const char* equipID)
{
	const equipDef_t* ed;

	if (LIST_IsEmpty(equipmentList)) {
		ed = INV_GetEquipmentDefinitionByID(equipID);
		if (ed == nullptr)
			Com_Error(ERR_DROP, "Could not find the equipment definition for '%s'", equipID);
		int index = ed - csi.eds;

		switch (changeType) {
		case BACKWARD:
			index--;
			if (index < 0)
				index = csi.numEDs - 1;
			break;
		case FORWARD:
			index++;
			if (index >= csi.numEDs)
				index = 0;
			break;
		default:
			break;
		}
		ed = &csi.eds[index];
	} else {
		const linkedList_t* entry = LIST_ContainsString(equipmentList, equipID);
		if (entry == nullptr) {
			equipID = (const char*)equipmentList->data;
		} else if (changeType == FORWARD) {
			equipID = (const char*)(entry->next != nullptr ? entry->next->data : equipmentList->data);
		} else if (changeType == BACKWARD) {
			const char* newEntry = nullptr;
			const char* prevEntry = nullptr;
			LIST_Foreach(equipmentList, char const, tmp) {
				if (Q_streq(tmp, equipID)) {
					if (prevEntry != nullptr) {
						newEntry = prevEntry;
						break;
					}
				}
				prevEntry = tmp;
				newEntry = tmp;
			}
			equipID = newEntry;
		}
		ed = INV_GetEquipmentDefinitionByID(equipID);
		if (ed == nullptr)
			Com_Error(ERR_DROP, "Could not find the equipment definition for '%s'", equipID);
	}

	return ed;
}

/**
 * @brief Fills the game mode list entries with the parsed values from the script
 */
void GAME_InitUIData (void)
{
	Com_Printf("----------- game modes -------------\n");
	for (int i = 0; i < numCGameTypes; i++) {
		const cgameType_t* t = &cgameTypes[i];
		const cgame_export_t* e = GAME_GetCGameAPI_(t);
		if (e == nullptr)
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
	Cmd_AddCommand("game_exit", GAME_Exit_f, "Quits the current running game and shutdown the game mode");
	Cmd_AddCommand("game_abort", GAME_Abort_f, "Abort the game and let the aliens/opponents win");
	Cmd_AddCommand("game_autoteam", GAME_AutoTeam_f, "Assign initial equipment to soldiers");
	Cmd_AddCommand("game_toggleactor", GAME_ToggleActorForTeam_f);
	Cmd_AddCommand("game_saveteamstate", GAME_SaveTeamState_f);
	Cmd_AddCommand("game_saveteam", GAME_SaveTeam_f, "Save a team slot");
	Cmd_AddCommand("game_loadteam", GAME_LoadTeam_f, "Load a team slot");
	Cmd_AddCommand("game_teamcomments", GAME_TeamSlotComments_f, "Fills the team selection menu with the team names");
	Cmd_AddCommand("game_teamupdate", GAME_UpdateTeamMenuParameters_f, "");
	Cmd_AddCommand("game_teamdelete", GAME_TeamDelete_f, "Removes a team");
	Cmd_AddCommand("game_actorselect", GAME_ActorSelect_f, "Select an actor in the equipment menu");
	Cmd_AddCommand("mn_getmaps", UI_GetMaps_f, "The initial map to show");
	Cmd_AddCommand("mn_nextmap", UI_NextMap_f, "Switch to the next valid map for the selected gametype");
	Cmd_AddCommand("mn_prevmap", UI_PreviousMap_f, "Switch to the previous valid map for the selected gametype");
	Cmd_AddCommand("mn_selectmap", UI_SelectMap_f, "Switch to the map given by the parameter - may be invalid for the current gametype");
	Cmd_AddCommand("mn_requestmaplist", UI_RequestMapList_f, "Request to send the list of available maps for the current gametype to a command.");

	Cvar_RegisterCvarListener(cvarListener);
	Cmd_RegisterCmdListener(cmdListener);
}

void GAME_Shutdown (void)
{
	OBJZERO(cgameTypes);
	numCGameTypes = 0;
	OBJZERO(equipDefStandard);
	OBJZERO(characters);

	Cvar_UnRegisterCvarListener(cvarListener);
	Cmd_UnRegisterCmdListener(cmdListener);
}
