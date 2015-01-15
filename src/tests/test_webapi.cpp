/**
 * @file
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

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "test_shared.h"
#include "../client/web/web_main.h"
#include "../client/web/web_cgame.h"

#define FILEPATH "save/campaign/"
#define FILE "unittest1.savx"
#define FILENAME FILEPATH FILE
#define CGAME "test"
#define CATEGORY 0

static const char* user;
static const char* password;

class WebApiTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();

		Com_ParseScripts(true);

		WEB_InitStartup();

		user = TEST_GetStringProperty("webapi-user");
		if (user == nullptr)
			user = "";

		password = TEST_GetStringProperty("webapi-password");
		if (password == nullptr)
			password = "";
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}
};

TEST_F(WebApiTest, Auth)
{
	if (Q_strnull(user) || Q_strnull(password))
		return;
	ASSERT_TRUE(WEB_Auth(user, password));
}

TEST_F(WebApiTest, FileManagement)
{
	if (Q_strnull(user) || Q_strnull(password))
		return;

	/* we can only upload files from within our user directory - so let's copy it there */
	if (!FS_FileExists("%s/" FILENAME, FS_Gamedir())) {
		byte* buf;
		const int size = FS_LoadFile(FILENAME, &buf);
		ASSERT_TRUE(size > 0);
		ASSERT_TRUE(nullptr != buf);
		FS_WriteFile(buf, size, FILENAME);
		FS_FreeFile(buf);
	}
	ASSERT_TRUE(WEB_CGameUpload(CGAME, CATEGORY, FILENAME));
	ASSERT_TRUE(WEB_CGameDownloadFromUser(CGAME, CATEGORY, FILE, web_userid->integer));
	const int countAfterUpload = WEB_CGameListForUser(CGAME, CATEGORY, web_userid->integer);
	ASSERT_TRUE(countAfterUpload != -1);
	ASSERT_TRUE(countAfterUpload >= 1);
	ASSERT_TRUE(WEB_CGameDelete(CGAME, CATEGORY, FILE));
	const int countAfterDelete = WEB_CGameListForUser(CGAME, CATEGORY, web_userid->integer);
	ASSERT_TRUE(countAfterDelete != -1);
	ASSERT_EQ(countAfterDelete, countAfterUpload - 1);
}
