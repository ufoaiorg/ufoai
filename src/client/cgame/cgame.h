/**
 * @file cgame.h
 * @brief Client game mode interface
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

#ifndef CL_CGAME_H
#define CL_CGAME_H

#include "../../common/xml.h"
#include "../../common/http.h"

struct cgame_import_s;

typedef struct cgame_export_s {
	const char *name;
	const char *menu;
	int isMultiplayer;
	void (EXPORT *Init) (void);
	void (EXPORT *Shutdown) (void);
	/** soldier spawn functions may differ between the different gametypes */
	qboolean (EXPORT *Spawn) (void);
	/** some gametypes only support special maps */
	const mapDef_t* (EXPORT *MapInfo) (int step);
	/** some gametypes require extra data in the results parsing (like e.g. campaign mode) */
	void (EXPORT *Results) (struct dbuffer *msg, int, int*, int*, int[][MAX_TEAMS], int[][MAX_TEAMS]);
	/** check whether the given item is usable in the current game mode */
	qboolean (EXPORT *IsItemUseable) (const objDef_t *od);
	/** if you want to display a different model for the given object in your game mode, implement this function */
	const char* (EXPORT *GetModelForItem) (const char *string);
	/** returns the equipment definition the game mode is using */
	equipDef_t* (EXPORT *GetEquipmentDefinition) (void);
	/** update character display values for game type dependent stuff */
	void (EXPORT *UpdateCharacterValues) (const character_t *chr);
	/** checks whether the given team is known in the particular gamemode */
	qboolean (EXPORT *IsTeamKnown) (const teamDef_t *teamDef);
	/** called on errors */
	void (EXPORT *Drop) (void);
	/** called after the team spawn messages where send, can e.g. be used to set initial actor states */
	void (EXPORT *InitializeBattlescape) (const chrList_t *team);
	/** callback that is executed every frame */
	void (EXPORT *RunFrame) (void);
	void (EXPORT *EndRoundAnnounce) (int playerNum, int team);
	void (EXPORT *StartBattlescape) (qboolean isTeamPlay);
	const char* (EXPORT *GetTeamDef) (void);
	void (EXPORT *NotifyEvent) (event_t eventType);
	void (EXPORT *AddChatMessage) (const char *message);
	qboolean (EXPORT *HandleServerCommand) (const char *command, struct dbuffer *msg);
} cgame_export_t;

/** @todo define the import interface */
typedef struct cgame_import_s {
	csi_t *csi;

	/* UI functions */
	void (IMPORT *UI_ExecuteConfunc) (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	void (IMPORT *UI_PopWindow) (qboolean all);
	uiNode_t* (IMPORT *UI_PushWindow) (const char *name, const char *parentName, linkedList_t *params);
	void (IMPORT *UI_InitStack) (const char* activeMenu, const char* mainMenu, qboolean popAll, qboolean pushActive);
	void (IMPORT *UI_Popup) (const char *title, const char *format, ...);
	uiNode_t* (IMPORT *UI_AddOption) (uiNode_t** tree, const char* name, const char* label, const char* value);
	void (IMPORT *UI_RegisterOption) (int dataId, uiNode_t *option);
	void (IMPORT *UI_RegisterText) (int textId, const char *text);
	void (IMPORT *UI_ResetData) (int dataId);
	void (IMPORT *UI_RegisterLinkedListText) (int textId, linkedList_t *text);
	void (IMPORT *UI_TextNodeSelectLine) (uiNode_t *node, int num);
	uiNode_t *(IMPORT *UI_PopupList) (const char *title, const char *headline, linkedList_t* entries, const char *clickAction);
	void (IMPORT *UI_UpdateInvisOptions) (uiNode_t *option, const linkedList_t *stringList);
	void (IMPORT *HUD_InitUI) (const char *optionWindowName, qboolean popAll);
	void (IMPORT *HUD_DisplayMessage) (const char *text);
	uiNode_t *(IMPORT *UI_GetOption) (int dataId);

	void (IMPORT *LIST_AddString) (linkedList_t** listDest, const char* data);
	const linkedList_t* (IMPORT *LIST_ContainsString) (const linkedList_t* list, const char* string);

	void (IMPORT *SV_ShutdownWhenEmpty) (void);
	void (IMPORT *SV_Shutdown) (const char *finalmsg, qboolean reconnect);

	int (IMPORT *CL_GetPlayerNum) (void);
	const char *(IMPORT *CL_PlayerGetName) (unsigned int player);
	int (IMPORT *CL_Milliseconds) (void);
	void (IMPORT *CL_Drop) (void);
	void (IMPORT *CL_QueryMasterServer) (const char *action, http_callback_t callback);
	void (IMPORT *CL_SetClientState) (int state);
	int (IMPORT *CL_GetClientState) (void);
	void (IMPORT *CL_Disconnect) (void);

	void (IMPORT *GAME_SwitchCurrentSelectedMap) (int step);
	const mapDef_t* (IMPORT *GAME_GetCurrentSelectedMap) (void);
	int (IMPORT *GAME_GetCurrentTeam) (void);
	void* (IMPORT *GAME_StrDup) (const char *string);
	void (IMPORT *GAME_AutoTeam) (const char *equipmentDefinitionID, int teamMembers);
	size_t (IMPORT *GAME_GetCharacterArraySize) (void);
	qboolean (IMPORT *GAME_IsTeamEmpty) (void);
	qboolean (IMPORT *GAME_LoadDefaultTeam) (void);
	void (IMPORT *GAME_SetServerInfo) (const char *server, const char *serverport);
	void (IMPORT *GAME_AppendTeamMember) (int memberIndex, const char *teamDefID, const equipDef_t *ed);
	void (IMPORT *GAME_ReloadMode) (void);
	void (IMPORT *Free) (void *ptr);

	/* sound functions */
	void (IMPORT *S_StartLocalSample) (const char *s, float volume);
	void (IMPORT *S_SetSampleRepeatRate) (int sampleRepeatRate);

	/* renderer functions */
	void (IMPORT *R_SoftenTexture) (byte *in, int width, int height, int bpp);
	void (IMPORT *R_LoadImage) (const char *name, byte **pic, int *width, int *height);

	struct dbuffer *(IMPORT *NET_ReadMsg)  (struct net_stream *s);
	int (IMPORT *NET_ReadByte)  (struct dbuffer *buf);
	int (IMPORT *NET_ReadStringLine)  (struct dbuffer *buf, char *string, size_t length);
	int (IMPORT *NET_ReadString)  (struct dbuffer *buf, char *string, size_t length);
	struct net_stream *(IMPORT *NET_Connect)  (const char *node, const char *service);
	void (IMPORT *NET_StreamSetCallback)  (struct net_stream *s, stream_callback_func *func);
	void (IMPORT *NET_OOB_Printf) (struct net_stream *s, const char *format, ...) __attribute__((format(printf,2,3)));
	void (IMPORT *NET_OOB_Printf2) (const char *format, ...) __attribute__((format(printf,1,2)));
	void *(IMPORT *NET_StreamGetData) (struct net_stream *s);
	void (IMPORT *NET_StreamSetData) (struct net_stream *s, void *data);
	void (IMPORT *NET_StreamFree) (struct net_stream *s);
	const char *(IMPORT *NET_StreamPeerToName) (struct net_stream *s, char *dst, int len, qboolean appendPort);
	void (IMPORT *NET_SockaddrToStrings) (struct datagram_socket *s, struct sockaddr *addr, char *node, size_t nodelen, char *service, size_t servicelen);
	struct datagram_socket *(IMPORT *NET_DatagramSocketNew) (const char *node, const char *service, datagram_callback_func *func);
	void (IMPORT *NET_DatagramBroadcast) (struct datagram_socket *s, const char *buf, int len, int port);
	void (IMPORT *NET_DatagramSocketClose) (struct datagram_socket *s);

	/* xml functions */
	xmlNode_t * (IMPORT *XML_AddNode) (xmlNode_t *parent, const char *name);
	void (IMPORT *XML_AddString) (xmlNode_t *parent, const char *name, const char *value);
	void (IMPORT *XML_AddBool) (xmlNode_t *parent, const char *name, qboolean value);
	void (IMPORT *XML_AddFloat) (xmlNode_t *parent, const char *name, float value);
	void (IMPORT *XML_AddDouble) (xmlNode_t *parent, const char *name, double value);
	void (IMPORT *XML_AddByte) (xmlNode_t *parent, const char *name, byte value);
	void (IMPORT *XML_AddShort) (xmlNode_t *parent, const char *name, short value);
	void (IMPORT *XML_AddInt) (xmlNode_t *parent, const char *name, int value);
	void (IMPORT *XML_AddLong) (xmlNode_t *parent, const char *name, long value);
	void (IMPORT *XML_AddPos3) (xmlNode_t *parent, const char *name, const vec3_t pos);
	void (IMPORT *XML_AddPos2) (xmlNode_t *parent, const char *name, const vec2_t pos);
	void (IMPORT *XML_AddDate) (xmlNode_t *parent, const char *name, const int day, const int sec);
	void (IMPORT *XML_AddStringValue) (xmlNode_t *parent, const char *name, const char *value);
	void (IMPORT *XML_AddBoolValue) (xmlNode_t *parent, const char *name, qboolean value);
	void (IMPORT *XML_AddFloatValue) (xmlNode_t *parent, const char *name, float value);
	void (IMPORT *XML_AddDoubleValue) (xmlNode_t *parent, const char *name, double value);
	void (IMPORT *XML_AddByteValue) (xmlNode_t *parent, const char *name, byte value);
	void (IMPORT *XML_AddShortValue) (xmlNode_t *parent, const char *name, short value);
	void (IMPORT *XML_AddIntValue) (xmlNode_t *parent, const char *name, int value);
	void (IMPORT *XML_AddLongValue) (xmlNode_t *parent, const char *name, long value);

	/* filesystem functions */
	int (IMPORT *FS_LoadFile) (const char *path, byte **buffer);
	void (IMPORT *FS_FreeFile) (void *buffer);
	int (IMPORT *FS_CheckFile) (const char *fmt, ...) __attribute__((format(printf, 1, 2)));

	/* console variable interaction */
	cvar_t *(IMPORT *Cvar_Get) (const char *varName, const char *value, int flags, const char* desc);
	cvar_t *(IMPORT *Cvar_Set) (const char *varName, const char *value);
	void (IMPORT *Cvar_SetValue) (const char *varName, float value);
	const char *(IMPORT *Cvar_GetString) (const char *varName);
	int (IMPORT *Cvar_GetInteger) (const char *varName);
	const char *(IMPORT *Cvar_VariableStringOld) (const char *varName);
	float (IMPORT *Cvar_GetValue) (const char *varName);
	qboolean (IMPORT *Cvar_Delete) (const char *varName);
	cvar_t * (IMPORT *Cvar_ForceSet) (const char *varName, const char *value);

	/* ClientCommand and ServerCommand parameter access */
	int (IMPORT *Cmd_Argc) (void);
	const char *(IMPORT *Cmd_Argv) (int n);
	const char *(IMPORT *Cmd_Args) (void);		/**< concatenation of all argv >= 1 */
	void (IMPORT *Cmd_AddCommand) (const char *cmdName, xcommand_t function, const char *desc);
	void (IMPORT *Cmd_RemoveCommand) (const char *cmdName);
	void (IMPORT *Cmd_ExecuteString) (const char *text);
	void (IMPORT *Cmd_AddParamCompleteFunction) (const char *cmd_name, int (*function)(const char *partial, const char **match));
	int (IMPORT *Cmd_GenericCompleteFunction) (size_t len, const char **match, int matches, const char **list);

	void (IMPORT *Cbuf_AddText) (const char *text);

	void (IMPORT *Sys_Error) (const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));
	int (IMPORT *Com_ServerState) (void);
	void (IMPORT *Com_SetGameType) (void);
	const char *(IMPORT *Com_Parse) (const char *data_p[]);
	void (IMPORT *Com_Error) (int code, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
	void (IMPORT *Com_Printf) (const char *msg, ...) __attribute__((format(printf, 1, 2)));
	void (IMPORT *Com_DPrintf) (int level, const char *msg, ...) __attribute__((format(printf, 2, 3)));
	const char* (IMPORT *Com_DropShipTypeToShortName) (humanAircraftType_t type);
	const char* (IMPORT *Com_UFOCrashedTypeToShortName) (ufoType_t type);
	const char* (IMPORT *Com_UFOTypeToShortName) (ufoType_t type);
	const char* (IMPORT *Com_GetRandomMapAssemblyNameForCraft) (const char *craftID);

	const equipDef_t *(IMPORT *INV_GetEquipmentDefinitionByID) (const char *name);
} cgame_import_t;

const cgame_export_t *GetCGameAPI(const cgame_import_t *import);

typedef const cgame_export_t *(*cgame_api_t) (const cgame_import_t *);

#endif
