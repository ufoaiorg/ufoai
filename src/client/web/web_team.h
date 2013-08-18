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

#pragma once

#include "../../common/cvar.h"

extern cvar_t *web_teamdownloadurl;
extern cvar_t *web_teamuploadurl;
extern cvar_t *web_teamlisturl;

void WEB_UploadTeam_f(void);
void WEB_DownloadTeam_f(void);
void WEB_ListTeams_f(void);
