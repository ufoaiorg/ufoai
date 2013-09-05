/**
 * @file
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

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "test_webapi.h"
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

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int InitSuite (void)
{
	TEST_Init();

	Com_ParseScripts(true);

	WEB_InitStartup();

	user = TEST_GetStringProperty("webapi-user");
	if (user == nullptr)
		user = "";

	password = TEST_GetStringProperty("webapi-password");
	if (password == nullptr)
		password = "";

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int CleanSuite (void)
{
	TEST_Shutdown();
	return 0;
}

static void testAuth (void)
{
	if (Q_strnull(user) || Q_strnull(password))
		return;
	CU_ASSERT_TRUE(WEB_Auth(user, password));
}

static void testFileManagement (void)
{
	if (Q_strnull(user) || Q_strnull(password))
		return;

	/* we can only upload files from within our user directory - so let's copy it there */
	if (!FS_FileExists("%s/" FILENAME, FS_Gamedir())) {
		byte *buf;
		const int size = FS_LoadFile(FILENAME, &buf);
		CU_ASSERT_TRUE_FATAL(size > 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
		FS_WriteFile(buf, size, FILENAME);
		FS_FreeFile(buf);
	}
	CU_ASSERT_TRUE_FATAL(WEB_CGameUpload(CGAME, CATEGORY, FILENAME));
	CU_ASSERT_TRUE_FATAL(WEB_CGameDownloadFromUser(CGAME, CATEGORY, FILE, web_userid->integer));
	const int countAfterUpload = WEB_CGameListForUser(CGAME, CATEGORY, web_userid->integer);
	CU_ASSERT_TRUE_FATAL(countAfterUpload != -1);
	CU_ASSERT_TRUE_FATAL(countAfterUpload >= 1);
	CU_ASSERT_TRUE(WEB_CGameDelete(CGAME, CATEGORY, FILE));
	const int countAfterDelete = WEB_CGameListForUser(CGAME, CATEGORY, web_userid->integer);
	CU_ASSERT_TRUE_FATAL(countAfterDelete != -1);
	CU_ASSERT_EQUAL_FATAL(countAfterDelete, countAfterUpload - 1);
}

int UFO_AddWebAPITests (void)
{
	/* add a suite to the registry */
	CU_pSuite DBufferSuite = CU_add_suite("WebAPITests", InitSuite, CleanSuite);
	if (DBufferSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(DBufferSuite, testAuth) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(DBufferSuite, testFileManagement) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
