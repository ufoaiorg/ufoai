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

static cvar_t *web_cgamedownloadurl;
static cvar_t *web_cgamedeleteurl;
static cvar_t *web_cgameuploadurl;
static cvar_t *web_cgamelisturl;

void WEB_CGameUpload (const char *cgameId, int category, const char *filename)
{
}

void WEB_CGameDelete (const char *cgameId, int category, const char *filename)
{
}

void WEB_CGameDownloadFromUser (const char *cgameId, int category, const char *filename, int userId)
{
	char buf1[256];
	char buf2[256];
	if (!Q_strreplace(web_cgamedownloadurl->string, "$cgame$", cgameId, buf1, sizeof(buf1))) {
		Com_Printf("$cgame$ is missing in the url\n");
		return;
	}
	if (!Q_strreplace(buf1, "$userid$", va("%i", userId), buf2, sizeof(buf2))) {
		Com_Printf("$userid$ is missing in the url\n");
		return;
	}
	if (!Q_strreplace(buf2, "$category$", va("%i", category), buf1, sizeof(buf1))) {
		Com_Printf("$category$ is missing in the url\n");
		return;
	}
	if (!Q_strreplace(buf1, "$file$", filename, buf2, sizeof(buf2))) {
		Com_Printf("$file$ is missing in the url\n");
		return;
	}
	const char *url = buf2;
	qFILE f;
	FS_OpenFile(filename, &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("Could not open the target file\n");
		FS_CloseFile(&f);
		return;
	}

	/* no login is needed here */
	if (!HTTP_GetToFile(url, f.f)) {
		Com_Printf("team download failed from '%s'\n", url);
		FS_CloseFile(&f);
		return;
	}
	FS_CloseFile(&f);
}

void WEB_CGameListForUser (const char *cgameId, int category, int userId)
{
}

void WEB_CGameDownload (const char *cgameId, int category, const char *filename)
{
}

void WEB_CGameList (const char *cgameId, int category)
{
}

void WEB_CGameCvars (void)
{
	web_cgamedownloadurl = Cvar_Get("web_cgamedownloadurl", WEB_API_SERVER "cgame/$cgame$/$userid$/$category$/$file$", CVAR_ARCHIVE, "The url to download a shared cgame file from. Use $userid$, $category$, $cgame$ and $file$ as placeholders.");
	web_cgamelisturl = Cvar_Get("web_cgamelisturl", WEB_API_SERVER "api/cgamelist.php", CVAR_ARCHIVE, "The url to get the team list from.");
	web_cgamedeleteurl = Cvar_Get("web_cgamedeleteurl", WEB_API_SERVER "api/cgamedelete.php", CVAR_ARCHIVE, "The url to call if you want to delete one of your own teams again.");
	web_cgameuploadurl = Cvar_Get("web_cgameuploadurl", WEB_API_SERVER "api/cgameupload.php", CVAR_ARCHIVE, "The url to upload a team to.");
}
