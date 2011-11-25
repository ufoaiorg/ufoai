/**
 * @file cl_game.h
 * @brief Shared game type headers
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

#ifndef CL_GAME_H
#define CL_GAME_H

#include "../cl_shared.h"
#include "../ui/ui_nodes.h"

struct cgame_export_s;

#define GAME_IsSingleplayer()	(!GAME_IsMultiplayer())
qboolean GAME_IsMultiplayer(void);
void GAME_ParseModes(const char *name, const char **text);
void GAME_InitStartup(void);
void GAME_Shutdown(void);
void GAME_InitUIData(void);
void GAME_UnloadGame(void);
void GAME_SetMode(const struct cgame_export_s *gametype);
void GAME_ReloadMode(void);
void GAME_Init(qboolean load);
void GAME_DisplayItemInfo(uiNode_t *node, const char *string);
qboolean GAME_ItemIsUseable(const objDef_t *od);
void GAME_HandleResults(struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS], qboolean nextmap);
void GAME_SpawnSoldiers(void);
void GAME_StartMatch(void);
equipDef_t *GAME_GetEquipmentDefinition(void);
qboolean GAME_HandleServerCommand(const char *command, struct dbuffer *msg);
void GAME_AddChatMessage(const char *format, ...);
void GAME_CharacterCvars(const character_t *chr);
character_t* GAME_GetCharacter(int index);
size_t GAME_GetCharacterArraySize(void);
void GAME_ResetCharacters(void);
void GAME_GenerateTeam(const char *teamDefID, const equipDef_t *ed, int teamMembers);
void GAME_AppendTeamMember(int memberIndex, const char *teamDefID, const equipDef_t *ed);
void GAME_StartBattlescape(qboolean isTeamPlay);
void GAME_EndBattlescape(void);
void GAME_EndRoundAnnounce(int playerNum, int team);
qboolean GAME_TeamIsKnown(const teamDef_t *teamDef);
void GAME_NofityEvent(event_t eventType);
const char* GAME_GetTeamDef(void);
void GAME_Drop(void);
void GAME_Frame(void);
const char* GAME_GetModelForItem(const objDef_t *od, struct uiModel_s** menuModel);
const mapDef_t* GAME_GetCurrentSelectedMap(void);
void GAME_SwitchCurrentSelectedMap(int step);
qboolean GAME_IsTeamEmpty(void);
int GAME_GetCurrentTeam(void);
void GAME_SetServerInfo(const char *server, const char *serverport);

#include "cgame.h"

const equipDef_t *GAME_ChangeEquip(const linkedList_t *equipmentList, changeEquipType_t changeType, const char *equipID);

#ifndef HARD_LINKED_CGAME
/* this is only here so the functions in the shared code can link */
#define CGAME_HARD_LINKED_FUNCTIONS \
void Sys_Error (const char *error, ...) \
{ \
	va_list argptr; \
	char text[1024]; \
	va_start(argptr, error); \
	Q_vsnprintf(text, sizeof(text), error, argptr); \
	va_end(argptr); \
	cgi->Sys_Error("%s", text); \
} \
\
void Com_Printf (const char *msg, ...) \
{ \
	va_list argptr; \
	char text[1024]; \
	va_start(argptr, msg); \
	Q_vsnprintf(text, sizeof(text), msg, argptr); \
	va_end(argptr); \
	cgi->Com_Printf("%s", text); \
} \
\
void Com_DPrintf (int level, const char *msg, ...) \
{ \
	va_list argptr; \
	char text[1024]; \
	va_start(argptr, msg); \
	Q_vsnprintf(text, sizeof(text), msg, argptr); \
	va_end(argptr); \
	cgi->Com_DPrintf(level, "%s", text); \
}
#else
#define CGAME_HARD_LINKED_FUNCTIONS
#endif

#endif
