/**
 * @file
 * @brief UFOAI web interface management. Authentication as well as
 * uploading/downloading stuff to and from your account is done here.
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

#include "web_main.h"
#include "../cl_shared.h"
#include "../ui/ui_main.h"
#include "web_cgame.h"
#include "../../common/sha1.h"

static cvar_t* web_authurl;
cvar_t* web_username;
cvar_t* web_password;
cvar_t* web_userid;

/**
 * @brief Downloads the given url and notify the callback. The login credentials are automatically added as GET parameters
 * @param[in] url The url to download
 * @param[in] callback The callback to given the downloaded data to
 * @param[in] userdata Userdata that is given to the @c callback
 * @return @c true if the download was successful, @c false otherwise
 */
bool WEB_GetURL (const char* url, http_callback_t callback, void* userdata)
{
	char buf[576];
	char passwordEncoded[512];
	HTTP_Encode(web_password->string, passwordEncoded, sizeof(passwordEncoded));
	char usernameEncoded[128];
	HTTP_Encode(web_username->string, usernameEncoded, sizeof(usernameEncoded));
	const char sep = strchr(url, '?') ? '&' : '?';
	if (!Com_sprintf(buf, sizeof(buf), "%s%cusername=%s&password=%s", url, sep, usernameEncoded, passwordEncoded)) {
		Com_Printf("overflow in url length: '%s'\n", buf);
		return false;
	}
	return HTTP_GetURL(buf, callback, userdata);
}

/**
 * @brief Downloads the given url directly into the given file. The login credentials are automatically added as GET parameters
 * @param[in] url The url to download
 * @param[in] file The file to write into
 * @return @c true if the download was successful, @c false otherwise
 */
bool WEB_GetToFile (const char* url, FILE* file)
{
	char buf[576];
	char passwordEncoded[MAX_VAR];
	HTTP_Encode(web_password->string, passwordEncoded, sizeof(passwordEncoded));
	char usernameEncoded[MAX_VAR];
	HTTP_Encode(web_username->string, usernameEncoded, sizeof(usernameEncoded));
	if (!Com_sprintf(buf, sizeof(buf), "%s?username=%s&password=%s", url, usernameEncoded, passwordEncoded)) {
		Com_Printf("overflow in url length: '%s'\n", buf);
		return false;
	}
	return HTTP_GetToFile(buf, file);
}

/**
 * @brief Uploads a file to the server with the login credentials
 * @param[in] formName The name of the form to submit with the file upload
 * @param[in] fileName The filename to upload - this must be a full path, not a virtual filesystem path
 * @param[in] url The url to open
 * @param[in] params Additional parameters for the form. The username and password values are automatically
 * filled in and don't have to be specified here.
 */
bool WEB_PutFile (const char* formName, const char* fileName, const char* url, upparam_t* params)
{
	upparam_t paramUser;
	upparam_t paramPassword;
	paramUser.name = "username";
	paramUser.value = web_username->string;
	paramUser.next = &paramPassword;
	paramPassword.name = "password";
	paramPassword.value = web_password->string;
	paramPassword.next = nullptr;
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
static void WEB_AuthResponse (const char* response, void* userdata)
{
	if (response == nullptr) {
		Cvar_Set("web_password", "");
		Cvar_Set("web_userid", "0");
		return;
	}
	Com_DPrintf(DEBUG_CLIENT, "response: '%s'\n", response);
	if (Q_streq(response, "0")) {
		/* failed */
		Cvar_Set("web_password", "");
		Cvar_Set("web_userid", "0");
	} else {
		Cvar_Set("web_userid", "%i", atoi(response));
	}
}

/**
 * @brief Performs a web auth request
 * @param[in] username The (unencoded) username
 * @param[in] password The (ununcoded) password
 * @note the cvars @c web_username and @c web_password are going to become overridden here.
 * If the auth failed, the cvar @c web_password is set to an empty string again.
 * @return @c true if the auth was successful, @c false otherwise.
 */
bool WEB_Auth (const char* username, const char* password)
{
	char digest[41];
	char user[MAX_VAR];
	Q_strncpyz(user, username, sizeof(user));
	Q_strlwr(user);
	char combined[512];
	Com_sprintf(combined, sizeof(combined), "%s%s", user, password);
	Com_SHA1Buffer((const byte*)combined, strlen(combined), digest);
	Cvar_Set("web_username", "%s", username);
	Cvar_Set("web_password", "%s", digest);
	if (!WEB_GetURL(web_authurl->string, WEB_AuthResponse)) {
		Cvar_Set("web_password", "");
		Cvar_Set("web_userid", "0");
		return false;
	}
	/* if the password is still set, the auth was successful */
	return Q_strvalid(web_password->string);
}

/**
 * @brief Console callback for handling the web auth
 * @sa WEB_Auth
 */
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

/**
 * @brief Pushes the webauth window if the password is not yet set
 * @return @c true if the user is authenticated, @c false otherwise
 */
bool WEB_CheckAuth (void)
{
	if (Q_strnull(web_password->string)) {
		UI_PushWindow("webauth");
		return false;
	}
	return true;
}

void WEB_InitStartup (void)
{
	Cmd_AddCommand("web_auth", WEB_Auth_f, "Perform the authentication against the UFOAI server");

	web_username = Cvar_Get("web_username", Sys_GetCurrentUser(), CVAR_ARCHIVE, "The username for the UFOAI server.");
	/* if the password is a non-empty string, this means that username and password
	 * are valid, and the authentication was successful */
	web_password = Cvar_Get("web_password", "", CVAR_ARCHIVE, "The encrypted password for the UFOAI server.");
	web_userid = Cvar_Get("web_userid", "0", 0, "Your userid for the UFOAI server");
	web_authurl = Cvar_Get("web_authurl", WEB_API_SERVER "api/auth.php", CVAR_ARCHIVE,
				"The url to perform the authentication against.");

	WEB_CGameCvars();
	WEB_CGameCommands();

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
