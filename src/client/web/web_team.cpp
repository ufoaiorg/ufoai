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
#include "../cl_shared.h"
#include "../cl_http.h"
#include "../ui/ui_main.h"
#include "../../shared/parse.h"

cvar_t *web_teamdownloadurl;
cvar_t *web_teamuploadurl;
cvar_t *web_teamlisturl;

void WEB_UploadTeam_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <slotindex>\n", Cmd_Argv(0));
		return;
	}

	const int index = atoi(Cmd_Argv(1));

	const char *filename = va("%s/save/team%i.mpt", FS_Gamedir(), index);
	if (!FS_FileExists(filename)) {
		Com_Printf("file '%s' doesn't exist\n", filename);
		return;
	}

	upparam_t paramUser;
	paramUser.name = "user";
	paramUser.value = Sys_GetCurrentUser();
	paramUser.next = nullptr;

	if (HTTP_PutFile("team", filename, web_teamuploadurl->string, &paramUser))
		Com_Printf("uploaded the team %i\n", index);
	else
		Com_Printf("failed to upload the team %i\n", index);
}

void WEB_DownloadTeam_f (void)
{
	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <id> <slotindex>\n", Cmd_Argv(0));
		return;
	}
	const int id = atoi(Cmd_Argv(1));
	const int index = atoi(Cmd_Argv(2));
	qFILE f;
	FS_OpenFile(va("save/team%i.mpt", index), &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("could not open the target file\n");
		return;
	}
	char url[256];
	Q_strncpyz(url, web_teamdownloadurl->string, sizeof(url));
	if (!Q_strreplace(web_teamdownloadurl->string, "$id$", va("%08d", id), url, sizeof(url)))
		return;

	if (!HTTP_GetToFile(url, f.f))
		return;

	Com_Printf("downloaded team %i into slot %i\n", id, index);
	UI_ExecuteConfunc("teamlist_downloadsuccessful %i", index);
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
	if (!HTTP_GetURL(url, WEB_ListTeamsCallback))
		Com_Printf("failed to query the team list\n");
}
