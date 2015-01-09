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

#pragma once

#include "../../common/common.h"
#include "../../common/http.h"

#define WEB_API_SERVER "http://ufoai.org/"

extern cvar_t* web_username;
extern cvar_t* web_password;
extern cvar_t* web_userid;

bool WEB_CheckAuth(void);
bool WEB_Auth(const char* username, const char* password);
void WEB_InitStartup(void);
bool WEB_GetURL(const char* url, http_callback_t callback, void* userdata = nullptr);
bool WEB_GetToFile(const char* url, FILE* file);
bool WEB_PutFile(const char* formName, const char* fileName, const char* url, upparam_t* params = nullptr);
