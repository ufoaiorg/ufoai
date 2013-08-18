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

#include "web_main.h"
#include "../cl_shared.h"
#include "web_team.h"
#define SERVER "http://ufoai.org/"

static cvar_t *web_authurl;

bool WEB_GetURL (const char *url, http_callback_t callback, void *userdata)
{
	return HTTP_GetURL(url, callback, userdata);
}

bool WEB_GetToFile (const char *url, FILE* file)
{
	return HTTP_GetToFile(url, file);
}

bool WEB_PutFile (const char *formName, const char *fileName, const char *url, const upparam_t *params)
{
	return HTTP_PutFile(formName, fileName, url, params);
}

bool WEB_Auth (const char *username, const char *password)
{
	return false;
}

static void WEB_Auth_f (void)
{
	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <username> <password>\n", Cmd_Argv(0));
		return;
	}
	if (WEB_Auth(Cmd_Argv(1), Cmd_Argv(2))) {
		Com_Printf("authentification successful\n");
	} else {
		Com_Printf("authentification failed\n");
	}
}

void WEB_InitStartup (void)
{
	Cmd_AddCommand("web_uploadteam", WEB_UploadTeam_f, "Upload a team to the UFOAI server");
	Cmd_AddCommand("web_downloadteam", WEB_DownloadTeam_f, "Download a team from the UFOAI server");
	Cmd_AddCommand("web_listteams", WEB_ListTeams_f, "Show all teams on the UFOAI server");
	Cmd_AddCommand("web_auth", WEB_Auth_f, "Perform the authentification against the UFOAI server");

	web_teamdownloadurl = Cvar_Get("web_teamdownloadurl", SERVER "teams/team$id$.mpt", CVAR_ARCHIVE,
			"The url to download a shared team from. Use $id$ as a placeholder for the team id.");
	web_teamlisturl = Cvar_Get("web_teamlisturl", SERVER "/api/teamlist.php", CVAR_ARCHIVE,
			"The url to get the team list from.");
	web_teamuploadurl = Cvar_Get("web_teamuploadurl", SERVER "/api/teamupload.php", CVAR_ARCHIVE,
			"The url to upload a team to.");
	web_authurl = Cvar_Get("web_authurl", SERVER "/api/auth.php", CVAR_ARCHIVE,
			"The url to perform the authentification against.");
}
