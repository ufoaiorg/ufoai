/**
 * @file
 * @brief UFOAI web interface management. Authentification as well as
 * uploading/downloading stuff to and from your account is done here.
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "web_team.h"
#include "web_main.h"
#include "../cl_shared.h"
#include "../cgame/cl_game_team.h"
#include "../cl_http.h"
#include "../ui/ui_main.h"
#include "../../shared/parse.h"

cvar_t *web_username;
cvar_t *web_password;
cvar_t *web_teamdownloadurl;
cvar_t *web_teamuploadurl;
cvar_t *web_teamlisturl;

void WEB_UploadTeam_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <slotindex>\n", Cmd_Argv(0));
		return;
	}

	/** @todo: check that we are authenticated */

	int index = atoi(Cmd_Argv(1));
	const char *filename;
	/* we will loop the whole team save list, just because i don't want
	 * to specify the filename in the script api of this command. Otherwise
	 * one could upload everything with this command */
	while ((filename = FS_NextFileFromFileList("save/*.mpt")) != nullptr) {
		if (index-- != 0)
			continue;

		const char *fullPath = va("%s/save/%s", FS_Gamedir(), filename);
		if (!FS_FileExists("%s", fullPath)) {
			/* we silently ignore this, because this file is not in the users save path,
			 * but part of the game data. Don't upload this. */
			break;
		}

		upparam_t paramUser;
		paramUser.name = "user";
		paramUser.value = Sys_GetCurrentUser();
		paramUser.next = nullptr;

		if (WEB_PutFile("team", fullPath, web_teamuploadurl->string, &paramUser))
			Com_Printf("uploaded the team '%s'\n", filename);
		else
			Com_Printf("failed to upload the team from file '%s'\n", filename);
		break;
	}
	FS_NextFileFromFileList(nullptr);
}

void WEB_DownloadTeam_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <id>\n", Cmd_Argv(0));
		return;
	}
	const int id = atoi(Cmd_Argv(1));
	char filename[MAX_QPATH];
	if (!GAME_TeamGetFreeFilename(filename, sizeof(filename)))
		return;
	qFILE f;
	FS_OpenFile(filename, &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("could not open the target file\n");
		return;
	}
	char url[256];
	Q_strncpyz(url, web_teamdownloadurl->string, sizeof(url));
	if (!Q_strreplace(web_teamdownloadurl->string, "$id$", va("%08d", id), url, sizeof(url)))
		return;

	/* no login is needed here */
	if (!HTTP_GetToFile(url, f.f))
		return;

	if (Com_CheckDuplicateFile(filename, "save/*.mpt")) {
		FS_RemoveFile(va("%s/%s", FS_Gamedir(), filename));
		UI_ExecuteConfunc("teamlist_downloadduplicate");
		return;
	}

	UI_ExecuteConfunc("teamlist_downloadsuccessful %s", filename);
}

static void WEB_ListTeamsCallback (const char *responseBuf, void *userdata)
{
	if (!responseBuf) {
		Com_Printf("Could not load the teamlist\n");
		return;
	}

	struct entry_s {
		int id;
		char name[MAX_VAR];
	};

	const value_t values[] = {
		{"id", V_INT, offsetof(entry_s, id), MEMBER_SIZEOF(entry_s, id)},
		{"name", V_STRING, offsetof(entry_s, name), 0},
		{nullptr, V_NULL, 0, 0}
	};

	entry_s entry;

	const char *token = Com_Parse(&responseBuf);
	if (token[0] != '{') {
		Com_Printf("invalid token: '%s' - expected {\n", token);
		return;
	}
	int num = 0;
	int level = 1;
	for (;;) {
		token = Com_Parse(&responseBuf);
		if (!responseBuf)
			break;
		if (token[0] == '{') {
			level++;
			OBJZERO(entry);
			continue;
		}
		if (token[0] == '}') {
			level--;
			if (level == 0) {
				break;
			}
			UI_ExecuteConfunc("teamlist_add %i \"%s\"", entry.id, entry.name);
			num++;
			continue;
		}

		const value_t *value;
		for (value = values; value->string; value++) {
			if (!Q_streq(token, value->string))
				continue;
			token = Com_Parse(&responseBuf);
			if (!responseBuf)
				break;
			/* found a normal particle value */
			Com_EParseValue(&entry, token, value->type, value->ofs, value->size);
			break;
		}
		if (value->string == nullptr) {
			Com_DPrintf(DEBUG_CLIENT, "invalid value found: '%s'\n", token);
			// skip the invalid value for this and try to go on
			token = Com_Parse(&responseBuf);
		}
	}
	Com_Printf("found %i teams\n", num);
}

void WEB_ListTeams_f (void)
{
	static const char *url = web_teamlisturl->string;
	if (!WEB_GetURL(url, WEB_ListTeamsCallback))
		Com_Printf("failed to query the team list\n");
}
