/**
 * @file
 * @brief UFOAI web interface management. c(lient)game related stuff.
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

#include "web_cgame.h"
#include "web_main.h"
#include "../cl_shared.h"
#include "../cgame/cl_game.h"
#include "../ui/ui_main.h"
#include "../../shared/parse.h"

static cvar_t *web_cgamedownloadurl;
static cvar_t *web_cgamedeleteurl;
static cvar_t *web_cgameuploadurl;
static cvar_t *web_cgamelisturl;

#define URL_SIZE 576

static const char* WEB_CGameGetURL (char *out, size_t outSize, const char *url, const char *cgameId, int category, const char *filename, int userId = -1)
{
	char categoryStr[MAX_VAR];
	Com_sprintf(categoryStr, sizeof(categoryStr), "%i", category);
	char userIdStr[MAX_VAR];
	Com_sprintf(userIdStr, sizeof(userIdStr), "%i", userId);

	const struct urlIds_s {
		const char *id;
		const char *replace;
		bool mandatory;
	} urlIds[] = {
		{"$cgame$", cgameId, true},
		{"$category$", categoryStr, category >= 0},
		{"$file$", filename, filename != nullptr},
		{"$userid$", userIdStr, userId > 0}
	};

	Q_strncpyz(out, url, outSize);

	const size_t entries = lengthof(urlIds);
	for (int i = 0; i < entries; i++) {
		const urlIds_s &id = urlIds[i];
		if (strstr(out, id.id) != nullptr) {
			char encoded[256] = { '\0' };
			const char *replace = encoded;
			if (id.replace != nullptr) {
				if (!HTTP_Encode(id.replace, encoded, sizeof(encoded))) {
					Com_Printf("failed to encode '%s'\n", id.replace);
					return nullptr;
				}
			}
			char buf1[URL_SIZE];
			if (!Q_strreplace(out, id.id, replace, buf1, sizeof(buf1))) {
				Com_Printf("failed to replace '%s'\n", id.id);
				return nullptr;
			}
			Q_strncpyz(out, buf1, outSize);
		} else if (id.mandatory) {
			Com_Printf("'%s' is missing in the url\n", id.id);
			return nullptr;
		}
	}

	return out;
}

void WEB_CGameUpload (const char *cgameId, int category, const char *filename)
{
	if (!WEB_CheckAuth())
		return;

	const char *fullPath = va("%s/%s", FS_Gamedir(), filename);
	/* we ignore this, because this file is not in the users save path,
	 * but part of the game data. Don't upload this. */
	if (!FS_FileExists("%s", fullPath)) {
		Com_Printf("no user data: '%s'\n", fullPath);
		UI_ExecuteConfunc("cgame_uploadfailed");
		return;
	}

	char url[URL_SIZE];
	const char *encodedURL = WEB_CGameGetURL(url, sizeof(url), web_cgameuploadurl->string, cgameId, category, nullptr);
	if (encodedURL == nullptr) {
		UI_ExecuteConfunc("cgame_uploadfailed");
		return;
	}

	if (WEB_PutFile("cgame", fullPath, encodedURL)) {
		UI_ExecuteConfunc("cgame_uploadsuccessful");
		Com_Printf("uploaded the team '%s'\n", filename);
	} else {
		UI_ExecuteConfunc("cgame_uploadfailed");
		Com_Printf("failed to upload the team from file '%s'\n", filename);
	}
}

void WEB_CGameDelete (const char *cgameId, int category, const char *filename)
{
	if (!WEB_CheckAuth())
		return;

	char url[URL_SIZE];
	const char *encodedURL = WEB_CGameGetURL(url, sizeof(url), web_cgamedeleteurl->string, cgameId, category, filename);
	if (encodedURL == nullptr)
		return;

	if (WEB_GetURL(encodedURL, nullptr)) {
		Com_Printf("deleted the cgame file '%s'\n", filename);
		UI_ExecuteConfunc("cgame_deletesuccessful");
	} else {
		Com_Printf("failed to delete the cgame file '%s' from the server\n", filename);
		UI_ExecuteConfunc("cgame_deletefailed");
	}
}

void WEB_CGameDownloadFromUser (const char *cgameId, int category, const char *filename, int userId)
{
	char url[URL_SIZE];
	const char *encodedURL = WEB_CGameGetURL(url, sizeof(url), web_cgamedownloadurl->string, cgameId, category, filename, userId);
	if (encodedURL == nullptr)
		return;

	qFILE f;
	FS_OpenFile(filename, &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("Could not open the target file\n");
		FS_CloseFile(&f);
		return;
	}

	/* no login is needed here */
	if (!HTTP_GetToFile(encodedURL, f.f)) {
		Com_Printf("cgame file download failed from '%s'\n", url);
		FS_CloseFile(&f);
		return;
	}
	FS_CloseFile(&f);
}

/**
 * @brief The http callback for the cgame list command
 * @param[in] responseBuf The cgame list in ufo script format
 * @param[in] userdata This is null in this case
 */
static void WEB_ListCGameFilesCallback (const char *responseBuf, void *userdata)
{
	if (!responseBuf) {
		Com_Printf("Could not load the cgame list\n");
		return;
	}

	struct entry_s {
		int userId;
		char name[MAX_QPATH];
	};

	const value_t values[] = {
		{"userid", V_INT, offsetof(entry_s, userId), MEMBER_SIZEOF(entry_s, userId)},
		{"file", V_STRING, offsetof(entry_s, name), 0},
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
			UI_ExecuteConfunc("cgamefiles_add \"%s\" \"%s\" %i %i", entry.name, entry.name, entry.userId, (entry.userId == web_userid->integer) ? 1 : 0);
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
	Com_Printf("found %i cgame file entries\n", num);
}

void WEB_CGameListForUser (const char *cgameId, int category, int userId)
{
	if (userId == -1 && !WEB_CheckAuth())
		return;

	char url[URL_SIZE];
	const char* encodedURL = WEB_CGameGetURL(url, sizeof(url), web_cgamelisturl->string, cgameId, category, nullptr, userId);
	if (encodedURL == nullptr)
		return;

	if (!WEB_GetURL(encodedURL, WEB_ListCGameFilesCallback))
		Com_Printf("failed to query the cgame list for '%s' in category %i and for user %i\n", cgameId, category, userId);
}

static void WEB_UploadCGame_f (void)
{
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <category> <filename>\n", Cmd_Argv(0));
		return;
	}
	const char* name = GAME_GetCurrentName();
	if (name == nullptr) {
		Com_Printf("No active cgame type\n");
		return;
	}
	const int category = atoi(Cmd_Argv(1));
	const char* filename = Cmd_Argv(2);
	WEB_CGameUpload(name, category, filename);
}

static void WEB_DeleteCGame_f (void)
{
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <category> <filename>\n", Cmd_Argv(0));
		return;
	}
	const char* name = GAME_GetCurrentName();
	if (name == nullptr) {
		Com_Printf("No active cgame type\n");
		return;
	}
	const int category = atoi(Cmd_Argv(1));
	const char* filename = Cmd_Argv(2);
	WEB_CGameDelete(name, category, filename);
}

static void WEB_DownloadCGame_f (void)
{
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <category> <filename> [<userid>]\n", Cmd_Argv(0));
		return;
	}
	const char* name = GAME_GetCurrentName();
	if (name == nullptr) {
		Com_Printf("No active cgame type\n");
		return;
	}
	const int category = atoi(Cmd_Argv(1));
	const char* filename = Cmd_Argv(2);
	const int userId = Cmd_Argc() == 4 ? atoi(Cmd_Argv(3)) : -1;
	WEB_CGameDownloadFromUser(name, category, filename, userId);
}

static void WEB_ListCGame_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category> [<userid>]\n", Cmd_Argv(0));
		return;
	}
	const char* name = GAME_GetCurrentName();
	if (name == nullptr) {
		Com_Printf("No active cgame type\n");
		return;
	}
	const int category = atoi(Cmd_Argv(1));
	const int userId = Cmd_Argc() == 3 ? atoi(Cmd_Argv(2)) : -1;
	WEB_CGameListForUser(name, category, userId);
}

void WEB_CGameCvars (void)
{
	web_cgamedownloadurl = Cvar_Get("web_cgamedownloadurl", WEB_API_SERVER "cgame/$cgame$/$userid$/$category$/$file$", 0, "The url to download a shared cgame file from. Use $userid$, $category$, $cgame$ and $file$ as placeholders.");
	web_cgamelisturl = Cvar_Get("web_cgamelisturl", WEB_API_SERVER "api/cgamelist.php?cgame=$cgame$&category=$category$&userid=$userid$", 0, "The url to get the cgame file list from.");
	web_cgamedeleteurl = Cvar_Get("web_cgamedeleteurl", WEB_API_SERVER "api/cgamedelete.php?cgame=$cgame$&category=$category$&file=$file$", 0, "The url to call if you want to delete one of your own cgame files again.");
	web_cgameuploadurl = Cvar_Get("web_cgameuploadurl", WEB_API_SERVER "api/cgameupload.php?cgame=$cgame$&category=$category$", 0, "The url to upload a cgame file to.");
}

void WEB_CGameCommands (void)
{
	Cmd_AddCommand("web_uploadcgame", WEB_UploadCGame_f, "Upload a file for a cgame to the UFOAI server");
	Cmd_AddCommand("web_deletecgame", WEB_DeleteCGame_f, "Delete one of your own cgame files from the server");
	Cmd_AddCommand("web_downloadcgame", WEB_DownloadCGame_f, "Download a cgame file from the UFOAI server");
	Cmd_AddCommand("web_listcgame", WEB_ListCGame_f, "Show all files for a cgame on the UFOAI server");
}
