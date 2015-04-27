/**
 * @file
 * @brief UFOAI web interface management. c(lient)game related stuff.
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

static cvar_t* web_cgamedownloadurl;
static cvar_t* web_cgamedeleteurl;
static cvar_t* web_cgameuploadurl;
static cvar_t* web_cgamelisturl;

#define URL_SIZE 576

/**
 * @brief Replaces placeholders in the urls with given values.
 * @param[out] out The final url
 * @param[in] outSize The size of the @c out buffer
 * @param[in] url The url with all the placeholders included
 * @param[in] cgameId The cgame id. Mandatory. May not be @c null.
 * @param[in] category The category of the cgame. If @c -1 then skip it.
 * @param[in] filename The filename to replace. If @c null, then skip it.
 * @param[in] userId The user id to replace. If @c -1 then skip it.
 * @return The @c buf pointer, or @c null in case of an error.
 */
static const char* WEB_CGameGetURL (char* out, size_t outSize, const char* url, const char* cgameId, int category, const char* filename, int userId = -1)
{
	char categoryStr[MAX_VAR];
	Com_sprintf(categoryStr, sizeof(categoryStr), "%i", category);
	char userIdStr[MAX_VAR];
	Com_sprintf(userIdStr, sizeof(userIdStr), "%i", userId);

	const struct urlIds_s {
		const char* id;
		const char* replace;
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
		const urlIds_s& id = urlIds[i];
		if (strstr(out, id.id) != nullptr) {
			char encoded[256] = { '\0' };
			const char* replace = encoded;
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

/**
 * @brief Uploads a file to the server.
 * @param[in] cgameId The cgame id that is used to upload the file.
 * @param[in] category The category of the cgame.
 * @param[in] filename The filename part below the gamedir
 * @note The current authenticated user will be taken.
 * @note Files can only get uploaded from within the user directory. You can't upload game provided content.
 * @return @c true if the upload of the file was successful, @c false otherwise.
 */
bool WEB_CGameUpload (const char* cgameId, int category, const char* filename)
{
	if (Q_strnull(cgameId))
		return false;

	if (Q_strnull(filename))
		return false;

	if (!WEB_CheckAuth())
		return false;

	const char* fullPath = va("%s/%s", FS_Gamedir(), filename);
	/* we ignore this, because this file is not in the users save path,
	 * but part of the game data. Don't upload this. */
	if (!FS_FileExists("%s", fullPath)) {
		Com_Printf("no user data: '%s'\n", fullPath);
		UI_ExecuteConfunc("cgame_uploadfailed");
		return false;
	}

	char url[URL_SIZE];
	const char* encodedURL = WEB_CGameGetURL(url, sizeof(url), web_cgameuploadurl->string, cgameId, category, nullptr);
	if (encodedURL == nullptr) {
		UI_ExecuteConfunc("cgame_uploadfailed");
		return false;
	}

	if (!WEB_PutFile("cgame", fullPath, encodedURL)) {
		Com_Printf("failed to upload the team from file '%s'\n", filename);
		UI_ExecuteConfunc("cgame_uploadfailed");
		return false;
	}

	Com_Printf("uploaded the team '%s'\n", filename);
	UI_ExecuteConfunc("cgame_uploadsuccessful");
	return true;
}

/**
 * @brief Deletes a user owned file on the server.
 * @param[in] cgameId The cgame id that is used to get the file.
 * @param[in] category The category of the cgame.
 * @param[in] filename The filename to replace in the URL.
 * @note The current authenticated user will be taken.
 * @return @c true if the deletion of the file was successful, @c false otherwise.
 */
bool WEB_CGameDelete (const char* cgameId, int category, const char* filename)
{
	if (!WEB_CheckAuth())
		return false;

	char url[URL_SIZE];
	const char* encodedURL = WEB_CGameGetURL(url, sizeof(url), web_cgamedeleteurl->string, cgameId, category, filename);
	if (encodedURL == nullptr)
		return false;

	if (!WEB_GetURL(encodedURL, nullptr)) {
		Com_Printf("failed to delete the cgame file '%s' from the server\n", filename);
		UI_ExecuteConfunc("cgame_deletefailed");
		return false;
	}

	Com_Printf("deleted the cgame file '%s'\n", filename);

	char idBuf[MAX_VAR];
	const char* id = Com_SkipPath(filename);
	Com_StripExtension(id, idBuf, sizeof(idBuf));

	UI_ExecuteConfunc("cgame_deletesuccessful \"%s\" %i %i", idBuf, category, web_userid->integer);
	return true;
}

/**
 * @brief Downloads a file from the server and store it in the user directory.
 * @param[in] cgameId The cgame id that is used to get the file.
 * @param[in] category The category of the cgame.
 * @param[in] filename The filename to replace in the URL.
 * @param[in] userId The user id to get the file for. If this is @c -1 (the default), the current
 * authenticated user will be taken.
 * @return @c true if the download of the file was successful, @c false otherwise.
 */
bool WEB_CGameDownloadFromUser (const char* cgameId, int category, const char* filename, int userId)
{
	char url[URL_SIZE];
	const char* encodedURL = WEB_CGameGetURL(url, sizeof(url), web_cgamedownloadurl->string, cgameId, category, filename, userId);
	if (encodedURL == nullptr)
		return false;

	char buf[MAX_OSPATH];
	GAME_GetRelativeSavePath(buf, sizeof(buf));
	Q_strcat(buf, sizeof(buf), "%s", filename);

	ScopedFile f;
	FS_OpenFile(buf, &f, FILE_WRITE);
	if (!f) {
		Com_Printf("Could not open the target file\n");
		return false;
	}

	/* no login is needed here */
	if (!HTTP_GetToFile(encodedURL, f.getFile())) {
		Com_Printf("cgame file download failed from '%s'\n", url);
		return false;
	}
	return true;
}

/**
 * @brief The http callback for the cgame list command
 * @param[in] responseBuf The cgame list in ufo script format
 * @param[in] userdata This can be used to return the amount of files that were listed
 */
static void WEB_ListCGameFilesCallback (const char* responseBuf, void* userdata)
{
	int* count = (int*)userdata;

	if (count != nullptr)
		*count = 0;

	if (!responseBuf) {
		Com_Printf("Could not load the cgame list\n");
		return;
	}

	struct entry_s {
		int userId;
		int category;
		char file[MAX_OSPATH];
		char name[MAX_VAR];
	};

	const value_t values[] = {
		{"userid", V_INT, offsetof(entry_s, userId), MEMBER_SIZEOF(entry_s, userId)},
		{"category", V_INT, offsetof(entry_s, category), MEMBER_SIZEOF(entry_s, category)},
		{"file", V_STRING, offsetof(entry_s, file), 0},
		{"name", V_STRING, offsetof(entry_s, name), 0},
		{nullptr, V_NULL, 0, 0}
	};

	entry_s entry;

	const char* token = Com_Parse(&responseBuf);
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
			char idBuf[MAX_VAR];
			const char* id = Com_SkipPath(entry.file);
			Com_StripExtension(id, idBuf, sizeof(idBuf));
			const bool ownEntry = entry.userId == web_userid->integer;
			UI_ExecuteConfunc("cgamefiles_add \"%s\" %i %i \"%s\" \"%s\" %i", idBuf, entry.category, entry.userId, id, entry.name, ownEntry ? 1 : 0);
			num++;
			continue;
		}

		const value_t* value;
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
	if (num == 0) {
		UI_ExecuteConfunc("cgamefiles_nofilesfound");
	}
	Com_Printf("found %i cgame file entries\n", num);
	if (count != nullptr)
		*count = num;
}

/**
 * @brief Shows the uploaded files for the particular cgame category and the given userid.
 * @param[in] cgameId The cgame id that is used to get a filelist for.
 * @param[in] category The category of the cgame.
 * @param[in] userId The user id to get the file list for. If this is @c -1 (the default), the current
 * authenticated user will be taken.
 * @return The amount of files that were found on the server. Or @c -1 on error.
 */
int WEB_CGameListForUser (const char* cgameId, int category, int userId)
{
	if (userId == -1) {
		if (!WEB_CheckAuth())
			return -1;

		userId = web_userid->integer;
	}

	char url[URL_SIZE];
	const char* encodedURL = WEB_CGameGetURL(url, sizeof(url), web_cgamelisturl->string, cgameId, category, nullptr, userId);
	if (encodedURL == nullptr)
		return -1;

	UI_ExecuteConfunc("cgamefiles_clear");
	int count = 0;
	if (!WEB_GetURL(encodedURL, WEB_ListCGameFilesCallback, &count)) {
		Com_Printf("failed to query the cgame list for '%s' in category %i and for user %i\n", cgameId, category, userId);
		return -1;
	}

	return count;
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
	char filename[MAX_OSPATH];
	GAME_GetRelativeSavePath(filename, sizeof(filename));
	Q_strcat(filename, sizeof(filename), "%s", Cmd_Argv(2));
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
	if (WEB_CGameListForUser(name, category, userId) == -1) {
		Com_Printf("failed to list the cgame files for '%s' in category %i for userid %i\n", name, category, userId);
	}
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
