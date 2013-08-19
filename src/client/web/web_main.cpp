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
#include "../ui/ui_main.h"
#include "web_team.h"

#define SERVER "http://ufoai.org/"

static cvar_t *web_authurl;

/**
 * @brief Downloads the given url and notify the callback. The login credentials are automatically added as GET parameters
 * @param[in] url The url to download
 * @param[in] callback The callback to given the downloaded data to
 * @param[in] userdata Userdata that is given to the @c callback
 * @return @c true if the download was successful, @c false otherwise
 */
bool WEB_GetURL (const char *url, http_callback_t callback, void *userdata)
{
	char buf[512];
	char passwordEncoded[MAX_VAR];
	HTTP_Encode(web_password->string, passwordEncoded, sizeof(passwordEncoded));
	char usernameEncoded[MAX_VAR];
	HTTP_Encode(web_username->string, usernameEncoded, sizeof(usernameEncoded));
	Com_sprintf(buf, sizeof(buf), "%s?request=user_authentication&username=%s&password=%s", url, usernameEncoded, passwordEncoded);
	return HTTP_GetURL(buf, callback, userdata);
}

/**
 * @brief Downloads the given url directly into the given file. The login credentials are automatically added as GET parameters
 * @param[in] url The url to download
 * @param[in] file The file to write into
 * @return @c true if the download was successful, @c false otherwise
 */
bool WEB_GetToFile (const char *url, FILE* file)
{
	char buf[512];
	char passwordEncoded[MAX_VAR];
	HTTP_Encode(web_password->string, passwordEncoded, sizeof(passwordEncoded));
	char usernameEncoded[MAX_VAR];
	HTTP_Encode(web_username->string, usernameEncoded, sizeof(usernameEncoded));
	Com_sprintf(buf, sizeof(buf), "%s?request=user_authentication&username=%s&password=%s", url, usernameEncoded, passwordEncoded);
	return HTTP_GetToFile(buf, file);
}

/**
 * @brief Uploads a file to the server with the login credentials
 * @param[in] formName The name of the form to submit with the file upload
 * @param]in] fileName The filename to upload - this must be a full path, not a virtual filesystem path
 * @param[in] url The url to open
 * @param[in] params Additional parameters for the form. The username and password values are automatically
 * filled in and don't have to be specified here.
 */
bool WEB_PutFile (const char *formName, const char *fileName, const char *url, upparam_t *params)
{
	upparam_t paramUser;
	upparam_t paramPassword;
	upparam_t paramRequest;
	paramUser.name = "username";
	paramUser.value = web_username->string;
	paramUser.next = &paramPassword;
	paramPassword.name = "password";
	paramPassword.value = web_password->string;
	paramPassword.next = &paramRequest;
	paramRequest.name = "request";
	paramRequest.value = "user_authentication";
	paramRequest.next = nullptr;
	if (params != nullptr) {
		params->next = &paramUser;
	} else {
		params = &paramUser;
	}
	return HTTP_PutFile(formName, fileName, url, params);
}

/**
 * @brief The callback for the web auth request
 * @param[in] response The web auth response
 * @param[in] userdata The userdata. @c NULL in this case.
 */
static void WEB_AuthResponse (const char *response, void *userdata)
{
	if (response == nullptr) {
		Cvar_Set("web_password", "");
		return;
	}
	Com_DPrintf(DEBUG_CLIENT, "response: '%s'\n", response);
	/* success */
	if (Q_streq(response, "1"))
		return;

	/* failed */
	Cvar_Set("web_password", "");
}

/**
 * @brief Performs a web auth request
 * @param[in] username The (unencoded) username
 * @param[in] password The (ununcoded) password
 * @note the cvars @c web_username and @c web_password are going to become overridden here.
 * If the auth failed, the cvar @c web_password is set to an empty string again.
 * @return @c true if the auth was successful, @c false otherwise.
 */
bool WEB_Auth (const char *username, const char *password)
{
	const char *md5sum = Com_MD5Buffer((const byte*)password, strlen(password));
	Cvar_Set("web_username", username);
	Cvar_Set("web_password", md5sum);
	if (!WEB_GetURL(web_authurl->string, WEB_AuthResponse)) {
		Cvar_Set("web_password", "");
		return false;
	}
	/* if the password is still set, the auth was successful */
	return Q_strvalid(web_password->string);
}

static void WEB_Auth_f (void)
{
	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <username> <password>\n", Cmd_Argv(0));
		return;
	}
	if (WEB_Auth(Cmd_Argv(1), Cmd_Argv(2))) {
		UI_ExecuteConfunc("web_authsuccessful");
	} else {
		UI_ExecuteConfunc("web_authfailed");
	}
}

void WEB_InitStartup (void)
{
	Cmd_AddCommand("web_uploadteam", WEB_UploadTeam_f, "Upload a team to the UFOAI server");
	Cmd_AddCommand("web_downloadteam", WEB_DownloadTeam_f, "Download a team from the UFOAI server");
	Cmd_AddCommand("web_listteams", WEB_ListTeams_f, "Show all teams on the UFOAI server");
	Cmd_AddCommand("web_auth", WEB_Auth_f, "Perform the authentification against the UFOAI server");

	web_username = Cvar_Get("web_username", Sys_GetCurrentUser(), CVAR_ARCHIVE, "The username for the UFOAI server.");
	web_password = Cvar_Get("web_password", "", CVAR_ARCHIVE, "The encrypted password for the UFOAI server.");

	web_teamdownloadurl = Cvar_Get("web_teamdownloadurl", SERVER "teams/team$id$.mpt", CVAR_ARCHIVE,
			"The url to download a shared team from. Use $id$ as a placeholder for the team id.");
	web_teamlisturl = Cvar_Get("web_teamlisturl", SERVER "api/teamlist.php", CVAR_ARCHIVE,
			"The url to get the team list from.");
	web_teamuploadurl = Cvar_Get("web_teamuploadurl", SERVER "api/teamupload.php", CVAR_ARCHIVE,
			"The url to upload a team to.");
	web_authurl = Cvar_Get("web_authurl", SERVER "api/auth.php", CVAR_ARCHIVE,
			"The url to perform the authentification against.");

	Com_Printf("\n------- web initialization ---------\n");
	if (Q_strvalid(web_password->string)) {
		Com_Printf("... using username '%s'\n", web_username->string);
		if (!WEB_GetURL(web_authurl->string, WEB_AuthResponse)) {
			Cvar_Set("web_password", "");
		}
		if (Q_strvalid(web_password->string)) {
			Com_Printf("... login successful\n");
		} else {
			Com_Printf("... login failed\n");
		}
	} else {
		Com_Printf("... web access not yet configured\n");
	}
}
