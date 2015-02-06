/**
 * @file
 * @brief Client game mode interface
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

#pragma once

#include "../../common/xml.h"
#include "../../common/http.h"

#include "../cl_inventory.h"
#include "../../common/binaryexpressionparser.h"

struct cgame_import_s;

typedef struct cgame_export_s {
	/** the name of this cgame mode - only for console output */
	const char* name;
	const char* menu;
	int isMultiplayer;
	void (EXPORT* Init) (void);
	void (EXPORT* Shutdown) (void);
	/** soldier spawn functions may differ between the different gametypes */
	bool (EXPORT* Spawn) (linkedList_t** chrList);
	/** some gametypes only support special maps */
	const mapDef_t* (EXPORT* MapInfo) (int step);
	/** some gametypes require extra data in the results parsing (like e.g. campaign mode) */
	void (EXPORT* Results) (dbuffer* msg, int, int*, int*, int[][MAX_TEAMS], int[][MAX_TEAMS], bool nextmap);
	/** check whether the given item is usable in the current game mode */
	bool (EXPORT* IsItemUseable) (const objDef_t* od);
	/** if you want to display a different model for the given object in your game mode, implement this function */
	const char* (EXPORT* GetModelForItem) (const char* string);
	/** returns the equipment definition the game mode is using */
	equipDef_t* (EXPORT* GetEquipmentDefinition) (void);
	/** update character display values for game type dependent stuff */
	void (EXPORT* UpdateCharacterValues) (const character_t* chr);
	/** checks whether the given team is known in the particular gamemode */
	bool (EXPORT* IsTeamKnown) (const teamDef_t* teamDef);
	/** returns the selected character */
	character_t* (EXPORT* GetSelectedChr) (void);
	/** if you want to have a different control on how much a soldier can carry implement this (but max is still limited to actor strength) */
	int (EXPORT* GetChrMaxLoad) (const character_t* chr);
	/** called on errors */
	void (EXPORT* Drop) (void);
	/** called after the team spawn messages where send, can e.g. be used to set initial actor states */
	void (EXPORT* InitializeBattlescape) (dbuffer* msg, const linkedList_t* team);
	/** callback that is executed every frame */
	void (EXPORT* RunFrame) (float secondsSinceLastFrame);
	void (EXPORT* HandleBaseClick) (int baseIdx, int key, int col, int row);
	void (EXPORT* DrawBase) (int baseIdx, int x, int y, int w, int h, int col, int row, bool hover, int overlap);
	void (EXPORT* DrawBaseLayout) (int baseIdx, int x, int y, int totalMarge, int w, int h, int padding, const vec4_t bgcolor, const vec4_t color);
	void (EXPORT* DrawBaseTooltip) (int baseIdx, int x, int y, int col, int row);
	void (EXPORT* EndRoundAnnounce) (int playerNum, int team);
	void (EXPORT* StartBattlescape) (bool isTeamPlay);
	void (EXPORT* InitMissionBriefing) (const char** title, linkedList_t** victoryConditionsMsgIDs, linkedList_t** missionBriefingMsgIDs);
	const char* (EXPORT* GetTeamDef) (void);
	void (EXPORT* NotifyEvent) (event_t eventType);
	void (EXPORT* AddChatMessage) (const char* message);
	bool (EXPORT* HandleServerCommand) (const char* command, dbuffer* msg);

	void (EXPORT* MapDraw) (geoscapeData_t* data);
	void (EXPORT* MapDrawMarkers) (const uiNode_t* node);
	void (EXPORT* MapClick) (const uiNode_t* node, int x, int y, const vec2_t pos);
} cgame_export_t;

typedef struct cgameType_s {
	char id[MAX_VAR];		/**< the id is also the file basename */
	char window[MAX_VAR];	/**< the ui window id where this game type should become active for */
	char name[MAX_VAR];		/**< translatable ui name */
	linkedList_t* equipmentList; /**< the list of valid equipment definitions for this gametype - if this
								 * is @c nullptr, every equipment may be used */
} cgameType_t;

typedef enum {
	FORWARD,
	BACKWARD,
	INIT
} changeEquipType_t;

/** @todo define the import interface */
typedef struct cgame_import_s {
	csi_t* csi;
	Inventory** ui_inventory;
	const cgameType_t* cgameType;

	/* UI functions */
	void (IMPORT* UI_ExecuteConfunc) (const char* fmt, ...) __attribute__((format(__printf__, 1, 2)));
	void (IMPORT* UI_PopWindow) (bool all);
	void (IMPORT* UI_PushWindow) (const char* name);
	void (IMPORT* UI_InitStack) (const char* activeMenu, const char* mainMenu);
	void (IMPORT* UI_Popup) (const char* title, const char* format, ...);
	uiNode_t* (IMPORT* UI_AddOption) (uiNode_t** tree, const char* name, const char* label, const char* value);
	void (IMPORT* UI_RegisterOption) (int dataId, uiNode_t* option);
	void (IMPORT* UI_RegisterText) (int textId, const char* text);
	void (IMPORT* UI_ResetData) (int dataId);
	void (IMPORT* UI_RegisterLinkedListText) (int textId, linkedList_t* text);
	struct uiMessageListNodeMessage_s* (IMPORT* UI_MessageGetStack) (void);
	void (IMPORT* UI_MessageAddStack) (struct uiMessageListNodeMessage_s*);
	void (IMPORT* UI_MessageResetStack) (void);
	void (IMPORT* UI_TextScrollEnd) (const char* nodePath);
	void (IMPORT* UI_TextNodeSelectLine) (uiNode_t* node, int num);
	uiNode_t* (IMPORT* UI_PopupList) (const char* title, const char* headline, linkedList_t* entries, const char* clickAction);
	void (IMPORT* UI_UpdateInvisOptions) (uiNode_t* option, const linkedList_t* stringList);
	void (IMPORT* HUD_InitUI) (const char* optionWindowName);
	void (IMPORT* HUD_DisplayMessage) (const char* text);
	uiNode_t* (IMPORT* UI_GetOption) (int dataId);
	void (IMPORT* UI_SortOptions) (uiNode_t** first);
	uiNode_t* (IMPORT* UI_InitOptionIteratorAtIndex) (int index, uiNode_t* option, uiOptionIterator_t* iterator);
	uiNode_t* (IMPORT* UI_OptionIteratorNextOption) (uiOptionIterator_t* iterator);
	int (IMPORT* UI_DrawString) (const char* fontID, align_t align, int x, int y, const char* c);
	const char*  (IMPORT* UI_GetFontFromNode) (const uiNode_t*  const node);
	void (IMPORT* UI_DrawNormImageByName) (bool flip, float x, float y, float w, float h, float sh, float th, float sl, float tl, const char* name);
	void (IMPORT* UI_DrawRect) (int x, int y, int w, int h, const vec4_t color, float lineWidth, int pattern);
	void (IMPORT* UI_DrawFill) (int x, int y, int w, int h, const vec4_t color);
	int (IMPORT* UI_DrawTooltip) (const char* string, int x, int y, int maxWidth);
	void (IMPORT* UI_GetNodeAbsPos) (const uiNode_t* node, vec2_t pos);
	void (IMPORT* UI_PopupButton) (const char* title, const char* text, const char* clickAction1, const char* clickText1, const char* tooltip1,
		const char* clickAction2, const char* clickText2, const char* tooltip2, const char* clickAction3, const char* clickText3, const char* tooltip3);
	uiSprite_t* (IMPORT* UI_GetSpriteByName) (const char* name);
	void (IMPORT* UI_ContainerNodeUpdateEquipment) (Inventory* inv, const equipDef_t* ed);
	void (IMPORT* UI_RegisterLineStrip) (int dataId, struct lineStrip_s* lineStrip);
	uiNode_t* (IMPORT* UI_GetNodeByPath) (const char* path);
	void (IMPORT* UI_DisplayNotice) (const char* text, int time, const char* windowName);
	const char* (IMPORT* UI_GetActiveWindowName) (void);

	const char* (IMPORT* CL_Translate) (const char* t);

	void (IMPORT* LIST_PrependString) (linkedList_t** listDest, const char* data);
	void (IMPORT* LIST_AddString) (linkedList_t** list, const char* data);
	void (IMPORT* LIST_AddStringSorted) (linkedList_t** listDest, const char* data);
	void (IMPORT* LIST_AddPointer) (linkedList_t** listDest, void* data);
	linkedList_t* (IMPORT* LIST_Add) (linkedList_t** list, void const* data, size_t length);
	const linkedList_t* (IMPORT* LIST_ContainsString) (const linkedList_t* list, const char* string);
	linkedList_t* (IMPORT* LIST_GetPointer) (linkedList_t* list, const void* data);
	void (IMPORT* LIST_Delete) (linkedList_t** list);
	bool (IMPORT* LIST_RemoveEntry) (linkedList_t** list, linkedList_t* entry);
	bool (IMPORT* LIST_IsEmpty) (const linkedList_t* list);
	int (IMPORT* LIST_Count) (const linkedList_t* list);
	linkedList_t* (IMPORT* LIST_CopyStructure) (linkedList_t* src);
	void* (IMPORT* LIST_GetByIdx) (linkedList_t* list, int index);
	bool (IMPORT* LIST_Remove) (linkedList_t** list, const void* data);
	void (IMPORT* LIST_Sort) (linkedList_t** list, linkedListSort_t sorter, const void* userData);
	void* (IMPORT* LIST_GetRandom) (linkedList_t* list);

	void (IMPORT* SV_ShutdownWhenEmpty) (void);
	void (IMPORT* SV_Shutdown) (const char* finalmsg, bool reconnect);

	int (IMPORT* CL_GetPlayerNum) (void);
	const char* (IMPORT* CL_PlayerGetName) (unsigned int player);
	int (IMPORT* CL_Milliseconds) (void);
	void (IMPORT* CL_Drop) (void);
	void (IMPORT* CL_QueryMasterServer) (const char* action, http_callback_t callback);
	void (IMPORT* CL_SetClientState) (connstate_t state);
	int (IMPORT* CL_GetClientState) (void);
	void (IMPORT* CL_Disconnect) (void);

	void (IMPORT* GAME_SwitchCurrentSelectedMap) (int step);
	const mapDef_t* (IMPORT* GAME_GetCurrentSelectedMap) (void);
	int (IMPORT* GAME_GetCurrentTeam) (void);
	char* (IMPORT* GAME_StrDup) (const char* string);
	void (IMPORT* GAME_AutoTeam) (const char* equipmentDefinitionID, int teamMembers);
	const equipDef_t* (IMPORT* GAME_ChangeEquip) (const linkedList_t* equipmentList, changeEquipType_t changeType, const char* equipID);
	size_t (IMPORT* GAME_GetCharacterArraySize) (void);
	bool (IMPORT* GAME_IsTeamEmpty) (void);
	bool (IMPORT* GAME_LoadDefaultTeam) (bool force);
	void (IMPORT* GAME_SetServerInfo) (const char* server, const char* serverport);
	void (IMPORT* GAME_AppendTeamMember) (int memberIndex, const char* teamDefID, const equipDef_t* ed);
	void (IMPORT* GAME_ReloadMode) (void);
	int (IMPORT* GAME_GetChrMaxLoad) (const character_t* chr);
	bool (IMPORT* GAME_LoadCharacter) (xmlNode_t* p, character_t* chr);
	bool (IMPORT* GAME_SaveCharacter) (xmlNode_t* p, const character_t* chr);

	/* Mem functions */
	void* (IMPORT* Alloc) (size_t size, bool zeroFill, memPool_t* pool, const int tagNum, const char* fileName, const int fileLine);
	void (IMPORT* Free) (void* ptr);
	memPool_t* (IMPORT* CreatePool) (const char* name);
	void (IMPORT* FreePool) (memPool_t* pool);
	char* (IMPORT* PoolStrDup) (const char* in, memPool_t* pool, const int tagNum);

	/* sound functions */
	void (IMPORT* S_StartLocalSample) (const char* s, float volume);
	void (IMPORT* S_SetSampleRepeatRate) (int sampleRepeatRate);

	/* renderer functions */
	void (IMPORT* R_SoftenTexture) (byte* in, int width, int height, int bpp);
	void (IMPORT* R_LoadImage) (const char* name, byte** pic, int* width, int* height);
	bool (IMPORT* R_ImageExists) (const char* pname, ...) __attribute__((format(__printf__, 1, 2)));
	void (IMPORT* R_Color) (const vec4_t rgba);
	void (IMPORT* R_DrawLineStrip) (int points, int* verts);
	void (IMPORT* R_DrawLine) (int* verts, float thickness);
	void (IMPORT* R_DrawRect) (int x, int y, int w, int h, const vec4_t color, float lineWidth, int pattern);
	void (IMPORT* R_DrawFill) (int x, int y, int w, int h, const vec4_t color);
	void (IMPORT* R_Draw2DMapMarkers) (const vec2_t screenPos, float direction, const char* model, int skin);
	void (IMPORT* R_Draw3DMapMarkers) (const vec2_t nodePos, const vec2_t nodeSize, const vec3_t rotate, const vec2_t pos, float direction, float earthRadius, const char* model, int skin);
	void (IMPORT* R_DrawBloom) (void);
	void (IMPORT* R_UploadAlpha) (const char* name, const byte* alphaData);
	void (IMPORT* R_DrawImageCentered) (int x, int y, const char* name);

	dbuffer* (IMPORT* NET_ReadMsg)  (struct net_stream* s);
	int (IMPORT* NET_ReadByte)  (dbuffer* buf);
	int (IMPORT* NET_ReadShort)  (dbuffer* buf);
	int (IMPORT* NET_ReadLong)  (dbuffer* buf);
	int (IMPORT* NET_ReadStringLine)  (dbuffer* buf, char* string, size_t length);
	int (IMPORT* NET_ReadString)  (dbuffer* buf, char* string, size_t length);
	void (IMPORT* NET_WriteByte)  (dbuffer* buf, byte c);
	void (IMPORT* NET_WriteShort)  (dbuffer* buf, int c);
	struct net_stream* (IMPORT* NET_Connect)  (const char* node, const char* service, stream_onclose_func* onclose);
	void (IMPORT* NET_StreamSetCallback)  (struct net_stream* s, stream_callback_func* func);
	void (IMPORT* NET_OOB_Printf) (struct net_stream* s, const char* format, ...) __attribute__((format(__printf__,2,3)));
	void (IMPORT* NET_OOB_Printf2) (const char* format, ...) __attribute__((format(__printf__,1,2)));
	void* (IMPORT* NET_StreamGetData) (struct net_stream* s);
	void (IMPORT* NET_StreamSetData) (struct net_stream* s, void* data);
	void (IMPORT* NET_StreamFree) (struct net_stream* s);
	const char* (IMPORT* NET_StreamPeerToName) (struct net_stream* s, char* dst, int len, bool appendPort);
	void (IMPORT* NET_SockaddrToStrings) (struct datagram_socket* s, struct sockaddr* addr, char* node, size_t nodelen, char* service, size_t servicelen);
	struct datagram_socket* (IMPORT* NET_DatagramSocketNew) (const char* node, const char* service, datagram_callback_func* func);
	void (IMPORT* NET_DatagramBroadcast) (struct datagram_socket* s, const char* buf, int len, int port);
	void (IMPORT* NET_DatagramSocketClose) (struct datagram_socket* s);

	/* xml functions */
	xmlNode_t*  (IMPORT* XML_AddNode) (xmlNode_t* parent, const char* name);
	void (IMPORT* XML_AddString) (xmlNode_t* parent, const char* name, const char* value);
	void (IMPORT* XML_AddBool) (xmlNode_t* parent, const char* name, bool value);
	void (IMPORT* XML_AddFloat) (xmlNode_t* parent, const char* name, float value);
	void (IMPORT* XML_AddDouble) (xmlNode_t* parent, const char* name, double value);
	void (IMPORT* XML_AddByte) (xmlNode_t* parent, const char* name, byte value);
	void (IMPORT* XML_AddShort) (xmlNode_t* parent, const char* name, short value);
	void (IMPORT* XML_AddInt) (xmlNode_t* parent, const char* name, int value);
	void (IMPORT* XML_AddLong) (xmlNode_t* parent, const char* name, long value);
	void (IMPORT* XML_AddPos3) (xmlNode_t* parent, const char* name, const vec3_t pos);
	void (IMPORT* XML_AddPos2) (xmlNode_t* parent, const char* name, const vec2_t pos);
	void (IMPORT* XML_AddDate) (xmlNode_t* parent, const char* name, const int day, const int sec);
	void (IMPORT* XML_AddStringValue) (xmlNode_t* parent, const char* name, const char* value);
	void (IMPORT* XML_AddBoolValue) (xmlNode_t* parent, const char* name, bool value);
	void (IMPORT* XML_AddFloatValue) (xmlNode_t* parent, const char* name, float value);
	void (IMPORT* XML_AddDoubleValue) (xmlNode_t* parent, const char* name, double value);
	void (IMPORT* XML_AddByteValue) (xmlNode_t* parent, const char* name, byte value);
	void (IMPORT* XML_AddShortValue) (xmlNode_t* parent, const char* name, short value);
	void (IMPORT* XML_AddIntValue) (xmlNode_t* parent, const char* name, int value);
	void (IMPORT* XML_AddLongValue) (xmlNode_t* parent, const char* name, long value);

	bool (IMPORT* XML_GetBool) (xmlNode_t* parent, const char* name, const bool defaultval);
	int (IMPORT* XML_GetInt) (xmlNode_t* parent, const char* name, const int defaultval);
	short (IMPORT* XML_GetShort) (xmlNode_t* parent, const char* name, const short defaultval);
	long (IMPORT* XML_GetLong) (xmlNode_t* parent, const char* name, const long defaultval);
	const char*  (IMPORT* XML_GetString) (xmlNode_t* parent, const char* name);
	float (IMPORT* XML_GetFloat) (xmlNode_t* parent, const char* name, const float defaultval);
	double (IMPORT* XML_GetDouble) (xmlNode_t* parent, const char* name, const double defaultval);
	xmlNode_t* (IMPORT* XML_Parse) (const char* buffer);
	xmlNode_t*  (IMPORT* XML_GetPos2) (xmlNode_t* parent, const char* name, vec2_t pos);
	xmlNode_t*  (IMPORT* XML_GetNextPos2) (xmlNode_t* actual, xmlNode_t* parent, const char* name, vec2_t pos);
	xmlNode_t*  (IMPORT* XML_GetPos3) (xmlNode_t* parent, const char* name, vec3_t pos);
	xmlNode_t*  (IMPORT* XML_GetNextPos3) (xmlNode_t* actual, xmlNode_t* parent, const char* name, vec3_t pos);
	xmlNode_t*  (IMPORT* XML_GetDate) (xmlNode_t* parent, const char* name, int* day, int* sec);
	xmlNode_t*  (IMPORT* XML_GetNode) (xmlNode_t* parent, const char* name);
	xmlNode_t*  (IMPORT* XML_GetNextNode) (xmlNode_t* current, xmlNode_t* parent, const char* name);

	/* filesystem functions */
	int (IMPORT* FS_OpenFile) (const char* filename, qFILE* file, filemode_t mode);
	int (IMPORT* FS_LoadFile) (const char* path, byte** buffer);
	void (IMPORT* FS_CloseFile) (qFILE*  f);
	void (IMPORT* FS_FreeFile) (void* buffer);
	int (IMPORT* FS_CheckFile) (const char* fmt, ...) __attribute__((format(__printf__, 1, 2)));
	int (IMPORT* FS_WriteFile) (const void* buffer, size_t len, const char* filename);
	void (IMPORT* FS_RemoveFile) (const char* osPath);
	int (IMPORT* FS_Read) (void* buffer, int len, qFILE*  f);
	int (IMPORT* FS_BuildFileList) (const char* files);
	const char* (IMPORT* FS_NextFileFromFileList) (const char* files);
	char* (IMPORT* FS_NextScriptHeader) (const char* files, const char** name, const char** text);

	/* console variable interaction */
	cvar_t* (IMPORT* Cvar_Get) (const char* varName, const char* value, int flags, const char* desc);
	cvar_t* (IMPORT* Cvar_Set) (const char* varName, const char* value, ...) __attribute__((format(__printf__, 2, 3)));
	void (IMPORT* Cvar_SetValue) (const char* varName, float value);
	const char* (IMPORT* Cvar_GetString) (const char* varName);
	int (IMPORT* Cvar_GetInteger) (const char* varName);
	const char* (IMPORT* Cvar_VariableStringOld) (const char* varName);
	float (IMPORT* Cvar_GetValue) (const char* varName);
	bool (IMPORT* Cvar_Delete) (const char* varName);
	cvar_t*  (IMPORT* Cvar_ForceSet) (const char* varName, const char* value);

	/* ClientCommand and ServerCommand parameter access */
	int (IMPORT* Cmd_Argc) (void);
	const char* (IMPORT* Cmd_Argv) (int n);
	const char* (IMPORT* Cmd_Args) (void);		/**< concatenation of all argv >= 1 */
	void (IMPORT* Cmd_AddCommand) (const char* cmdName, xcommand_t function, const char* desc);
	void (IMPORT* Cmd_RemoveCommand) (const char* cmdName);
	void (IMPORT* Cmd_TableAddList) (const cmdList_t* cmdList);
	void (IMPORT* Cmd_TableRemoveList) (const cmdList_t* cmdList);
	void (IMPORT* Cmd_ExecuteString) (const char* text, ...) __attribute__((format(__printf__, 1, 2)));
	void (IMPORT* Cmd_AddParamCompleteFunction) (const char* cmd_name, int (*function)(const char* partial, const char** match));
	bool (IMPORT* Cmd_GenericCompleteFunction) (char const* candidate, char const* partial, char const** match);
	mapDef_t* (IMPORT* Com_GetMapDefinitionByID) (const char* mapDefID);

	void (IMPORT* Cbuf_AddText) (const char* format, ...) __attribute__((format(__printf__, 1, 2)));
	void (IMPORT* Cbuf_Execute) (void);

	void (IMPORT* Sys_Error) (const char* error, ...) __attribute__((noreturn, format(__printf__, 1, 2)));
	int (IMPORT* Com_ServerState) (void);
	void (IMPORT* Com_SetGameType) (void);
	void (IMPORT* Com_Error) (int code, const char* fmt, ...) __attribute__((noreturn, format(__printf__, 2, 3)));
	void (IMPORT* Com_Printf) (const char* msg, ...) __attribute__((format(__printf__, 1, 2)));
	void (IMPORT* Com_DPrintf) (int level, const char* msg, ...) __attribute__((format(__printf__, 2, 3)));
	const char* (IMPORT* Com_DropShipTypeToShortName) (humanAircraftType_t type);
	const char* (IMPORT* Com_UFOCrashedTypeToShortName) (ufoType_t type);
	const char* (IMPORT* Com_UFOTypeToShortName) (ufoType_t type);
	const char* (IMPORT* Com_GetRandomMapAssemblyNameForCraft) (const char* craftID);
	void (IMPORT* Com_RegisterConstInt) (const char* name, int value);
	bool (IMPORT* Com_UnregisterConstVariable) (const char* name);
	void (IMPORT* Com_RegisterConstList) (const constListEntry_t constList[]);
	bool (IMPORT* Com_UnregisterConstList) (const constListEntry_t constList[]);
	const char* (IMPORT* Com_GetConstVariable) (const char* space, int value);
	bool (IMPORT* Com_GetConstIntFromNamespace) (const char* space, const char* variable, int* value);
	bool (IMPORT* Com_GetConstInt) (const char* name, int* value);
	const char* (IMPORT* Com_EParse) (const char** text, const char* errhead, const char* errinfo);
	int (IMPORT* Com_EParseValue) (void* base, const char* token, valueTypes_t type, int ofs, size_t size);
	bool (IMPORT* Com_ParseBoolean) (const char* token);
	bool (IMPORT* Com_ParseList) (const char** text, linkedList_t** list);
	bool (IMPORT* Com_ParseBlock) (const char* name, const char** text, void* base, const value_t* values, memPool_t* mempool);
	bool (IMPORT* Com_ParseBlockToken) (const char* name, const char** text, void* base, const value_t* values, memPool_t* mempool, const char* token);
	const char* (IMPORT* Com_ValueToStr) (const void* base, const valueTypes_t type, const int ofs);
	const teamDef_t* (IMPORT* Com_GetTeamDefinitionByID) (const char* team);
	ufoType_t (IMPORT* Com_UFOShortNameToID) (const char* token);
	const char* (IMPORT* Com_GetRandomMapAssemblyNameForCrashedCraft) (const char* craftID);
	void (IMPORT* Com_Drop) (void);
	const ugv_t* (IMPORT* Com_GetUGVByID) (const char* ugvID);
	const ugv_t* (IMPORT* Com_GetUGVByIDSilent) (const char* ugvID);
	humanAircraftType_t (IMPORT* Com_DropShipShortNameToID) (const char* token);

	void (IMPORT* CL_GenerateCharacter) (character_t* chr, const char* teamDefName);
	bool (IMPORT* CL_OnBattlescape) (void);

	const char* (IMPORT* CL_ActorGetSkillString) (const int skill);
	void (IMPORT* CL_UpdateCharacterValues) (const character_t* chr);

	void (IMPORT* SetNextUniqueCharacterNumber) (int ucn);
	int (IMPORT* GetNextUniqueCharacterNumber) (void);

	void (IMPORT* CollectItems) (void* target, int won, void (*item)(void*, const objDef_t*, int), void (*ammo) (void* , const Item* ), void (*ownitems) (const Inventory*));
	void (IMPORT* CollectAliens) (void* data, void (*collect)(void*, const teamDef_t*, int, bool));

	const equipDef_t* (IMPORT* INV_GetEquipmentDefinitionByID) (const char* name);
	void (IMPORT* INV_DestroyInventory) (Inventory* const i) __attribute__((nonnull(1)));
	void (IMPORT* INV_EquipActor) (character_t* const chr, const equipDef_t* ed, const objDef_t* weapon, int maxWeight);
	bool (IMPORT* INV_RemoveFromInventory) (Inventory* const i, const invDef_t*  container, Item* fItem);

	void (IMPORT* INV_ItemDescription) (const objDef_t* od);
	bool (IMPORT* INV_ItemMatchesFilter) (const objDef_t* obj, const itemFilterTypes_t filterType);
	const char* (IMPORT* INV_GetFilterType) (itemFilterTypes_t id);
	itemFilterTypes_t (IMPORT* INV_GetFilterTypeID) (const char*  filterTypeID);

	void (IMPORT* WEB_Upload) (int category, const char* filename);
	void (IMPORT* WEB_Delete) (int category, const char* filename);
	void (IMPORT* WEB_DownloadFromUser) (int category, const char* filename, int userId);
	void (IMPORT* WEB_ListForUser) (int category, int userId);

	const char* (IMPORT* GetRelativeSavePath) (char* buf, size_t bufSize);
	const char* (IMPORT* GetAbsoluteSavePath) (char* buf, size_t bufSize);

	bool (IMPORT* BEP_Evaluate) (const char* expr, BEPEvaluteCallback_t varFuncParam, const void* userdata);
	/** @todo: remove me */
	byte* r_xviAlpha;
	byte* r_radarPic;
	byte* r_radarSourcePic;
} cgame_import_t;

extern "C" const cgame_export_t* GetCGameAPI(const cgame_import_t* import);

typedef const cgame_export_t* (*cgame_api_t) (const cgame_import_t*);
