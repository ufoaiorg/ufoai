/**
 * @file test_generic.c
 * @brief Test cases for code below common/ and shared/
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "../common/common.h"
#include "../common/http.h"
#include "../shared/utf8.h"
#include "../shared/shared.h"
#include "../shared/infostring.h"
#include "../shared/stringhunk.h"
#include "../ports/system.h"
#include "test_generic.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteGeneric (void)
{
	TEST_Init();
	NET_Init();
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteGeneric (void)
{
	TEST_Shutdown();
	NET_Shutdown();
	return 0;
}

static void STRHUNK_VisitorTestEntry (const char *string)
{
	CU_ASSERT_STRING_EQUAL(string, "Test");
}

static void testStringHunks (void)
{
	stringHunk_t *hunk = STRHUNK_Create(20);
	STRHUNK_Add(hunk, "Test");
	CU_ASSERT_EQUAL(STRHUNK_Size(hunk), 1);
	CU_ASSERT_EQUAL(STRHUNK_Free(hunk), 15);
	STRHUNK_Visit(hunk, STRHUNK_VisitorTestEntry);
	STRHUNK_Delete(&hunk);
	CU_ASSERT_PTR_EQUAL(hunk, NULL);

	hunk = STRHUNK_Create(23);
	STRHUNK_Add(hunk, "Test");
	STRHUNK_Add(hunk, "Test");
	STRHUNK_Add(hunk, "Test");
	STRHUNK_Add(hunk, "Test");
	CU_ASSERT_EQUAL(STRHUNK_Size(hunk), 4);
	CU_ASSERT_EQUAL(STRHUNK_Free(hunk), 0);
	STRHUNK_Visit(hunk, STRHUNK_VisitorTestEntry);

	STRHUNK_Reset(hunk);
	CU_ASSERT_EQUAL(STRHUNK_Size(hunk), 0);

	STRHUNK_Delete(&hunk);
	CU_ASSERT_PTR_EQUAL(hunk, NULL);
}

static void testConstInt (void)
{
	const constListEntry_t list[] = {
		{"namespace::power", 1},
		{"namespace::speed", 2},
		{"namespace::accuracy", 3},
		{"namespace::mind", 4},
		{"namespace::close", 5},
		{"namespace::heavy", 6},
		{"namespace::assault", 7},
		{"namespace::sniper", 8},
		{"namespace::explosive", 9},
		{"namespace::hp", 10},

		{NULL, -1}
	};
	const constListEntry_t list2[] = {
		{"namespace2::soldier", 0},
		{"namespace2::scientist", 1},
		{"namespace2::worker", 2},
		{"namespace2::pilot", 3},
		{NULL, -1}
	};
	int out;

	Com_RegisterConstInt("namespace::variable", 1);
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable"));

	Com_RegisterConstInt("namespace::variable", 1);
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable"));

	Com_RegisterConstInt("namespace::variable2", 2);
	Com_RegisterConstInt("namespace::variable3", 3);
	Com_RegisterConstInt("namespace::variable4", 4);
	Com_RegisterConstInt("namespace::variable5", 5);
	Com_RegisterConstInt("namespace::variable6", 6);

	Com_RegisterConstInt("namespace2::variable2", 10);

	out = 0;
	CU_ASSERT_TRUE(Com_GetConstInt("namespace2::variable2", &out));
	CU_ASSERT_EQUAL(out, 10);
	out = 0;
	CU_ASSERT_TRUE(Com_GetConstInt("namespace::variable2", &out));
	CU_ASSERT_EQUAL(out, 2);
	out = 0;
	CU_ASSERT_TRUE(Com_GetConstInt("variable2", &out));
	CU_ASSERT_EQUAL(out, 10);

	CU_ASSERT_STRING_EQUAL(Com_GetConstVariable("namespace", 2), "variable2");

	CU_ASSERT(Com_UnregisterConstVariable("namespace2::variable2"));
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable2"));
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable3"));
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable4"));
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable5"));
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable6"));

	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable"));
	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable2"));
	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable3"));
	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable4"));
	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable5"));
	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable6"));

	Com_RegisterConstList(list);
	out = 0;
	CU_ASSERT_TRUE(Com_GetConstInt("sniper", &out));
	CU_ASSERT_EQUAL(out, 8);

	CU_ASSERT_TRUE(Com_UnregisterConstList(list));
	out = 0;
	CU_ASSERT_FALSE(Com_GetConstInt("sniper", &out));

	Com_RegisterConstList(list2);

	Com_RegisterConstList(list);
	CU_ASSERT_TRUE(Com_UnregisterConstList(list));

	out = 0;
	CU_ASSERT(Com_GetConstInt("pilot", &out));
	CU_ASSERT_EQUAL(out, 3);
	Com_UnregisterConstList(list2);
}


static void testLinkedList (void)
{
	linkedList_t *list = NULL;
	const char* data = "SomeDataForTheLinkedList";
	const size_t length = strlen(data);
	linkedList_t *entry;
	const linkedList_t *entry2;
	const char *returnedData;

	entry = LIST_Add(&list, (const byte*)data, length);
	CU_ASSERT_EQUAL(LIST_Count(list), 1);
	CU_ASSERT_TRUE(entry != NULL);
	returnedData = (const char *)LIST_GetByIdx(list, 0);
	CU_ASSERT_TRUE(returnedData != NULL);
	entry2 = LIST_ContainsString(list, returnedData);
	CU_ASSERT_TRUE(entry2 != NULL);
	CU_ASSERT_EQUAL((const void*)entry2->data, (const void*)returnedData);
	CU_ASSERT_STRING_EQUAL(entry2->data, returnedData);
	LIST_RemoveEntry(&list, entry);
	CU_ASSERT_EQUAL(LIST_Count(list), 0);
}

static void testLinkedListIterator (void)
{
	linkedList_t *list = NULL;
	char *string;
	int cnt;

	LIST_AddString(&list, "test1");
	LIST_AddString(&list, "test2");
	LIST_AddString(&list, "test3");

	cnt = 0;
	LIST_Foreach(list, char, string) {
		CU_ASSERT_PTR_NOT_NULL(string);
		cnt++;
	}

	LIST_Delete(&list);

	CU_ASSERT_EQUAL(cnt, 3);

	list = NULL;
	LIST_Foreach(list, char, string) {
		/* we should not be here, because the list is empty */
		CU_ASSERT_TRUE(qfalse);
	}
}

static void testLinkedListIteratorRemove (void)
{
	linkedList_t *list = NULL;
	char *string;

	LIST_AddString(&list, "test1");
	LIST_AddString(&list, "test2");
	LIST_AddString(&list, "test3");

	LIST_Foreach(list, char, string) {
		Com_Printf("Found string: %s\n", string);
		LIST_Remove(&list, string);
	}

	CU_ASSERT_TRUE(LIST_IsEmpty(list));
}

static void testLinkedListStringSort (void)
{
	linkedList_t *list = NULL;

	LIST_AddStringSorted(&list, "test2");
	LIST_AddStringSorted(&list, "test1");
	LIST_AddStringSorted(&list, "test3");

	CU_ASSERT_STRING_EQUAL((const char *)LIST_GetByIdx(list, 0), "test1");

	LIST_Delete(&list);
}

static void testFileSystemBuildLists (void)
{
	const char *filename, *prev;
	const char *wildcard = "ufos/**.ufo";
	const int ufosCnt = FS_BuildFileList(wildcard);

	CU_ASSERT_TRUE(ufosCnt > 1);

	prev = NULL;
	while ((filename = FS_NextFileFromFileList(wildcard)) != NULL) {
		if (prev != NULL)
			CU_ASSERT_EQUAL(Q_StringSort(prev, filename), -1);
		prev = filename;
	}

	FS_NextFileFromFileList(NULL);
}

static void testInfoStrings (void)
{
	char info[MAX_INFO_STRING] = "";

	Info_SetValueForKey(info, sizeof(info), "name", "test");

	CU_ASSERT_STRING_EQUAL(Info_ValueForKey(info, "name"), "test");
	CU_ASSERT_STRING_EQUAL(Info_ValueForKey(info, "name2"), "");
	Info_RemoveKey(info, "name");
	CU_ASSERT_STRING_EQUAL(Info_ValueForKey(info, "name"), "");

	Info_SetValueForKey(info, sizeof(info), "name", "\\invalid\\value");
	CU_ASSERT_STRING_EQUAL(Info_ValueForKey(info, "name"), "");
}

static void testCvars (void)
{
	Cvar_Get("testGeneric_cvar", "testGeneric_cvarValue", CVAR_NOSET, "No set");
	Cvar_Set("testGeneric_cvar", "test");
	CU_ASSERT_STRING_EQUAL(Cvar_GetString("testGeneric_cvar"), "testGeneric_cvarValue");
}

static void testStringCopiers (void)
{
	const char src[] = "Командующий, я чрезвычайно рад доложить, что наш проект ОПЭВ был завершён успешно. Я прикрепил к письму "
			"изображения и схемы прототипа лазерного оружия, в котором реализовано непрерывное волновое излучение достаточной "
			"мощности в портативном корпусе. Практическое решение лежало не в тщетных попытках увеличения ёмкости аккумуляторной "
			"батареи, а в использовании радикальных технологий миниатюризации с целью облегчения и уменьшения размеров существующих "
			"лазеров с химической накачкой. Они использовались довольно долгое время, будучи частью экспериментальных военных "
			"программ с конца XX века, но их применение всегда сталкивалось с множеством проблем. Модели химических лазеров с "
			"реальными военными перспективами были слишком большими и громоздкими для использования в бою пехотой. До сих пор их "
			"расположение было ограничено выбором между лазерными орудиями на сухопутном шасси и батареями морского базирования. "
			"Боевое применение заключалось в основном для целей противоракетной обороны (ПРО). Тем не менее, теперь мы в ФАЛАНКС "
			"создали компактное лазерное орудие. Вопреки своему малому размеру, оно полностью способно управлять фтор-дейтериевой "
			"реакцией и её токсическими продуктами распада без опасности поражения личного состава. Разрешите мне дать вам краткое "
			"описание принципа его работы. Внутри камеры сгорания этилен окисляется в пламени трифторида азота. В ходе этой реакции "
			"выделяются свободные радикалы фтора, которые затем приводятся в контакт с газообразной смесью гелия и дейтерия. Дейтерий "
			"вступает в реакцию с фтором, что приводит к образованию энергетически возбуждённых молекул фторида дейтерия. Они "
			"подвергаются стимулированной эмиссии в оптическом резонаторе оружия, генерируя лазерный луч. Серия интеллектуальных "
			"линз фокусирует и направляет луч в точку прицеливания, и даже вносит коррекцию, с учётом небольших перемещений стрелка "
			"и/или цели. Излишки газообразного дейтерия направляются через сконструированную нами систему фильтров высокого давления, "
			"которые задерживают все токсичные и радиоактивные молекулы перед выбросом отработанных паров в атмосферу. Фильтр требует "
			"замены после каждой боевой операции, но разработанная нами система делает этот процесс простым и безболезненным. Если ";

	long time;
	const int copies = 10000;
	char dest[8192];
	int i;

	Com_Printf("\n");
	time = Sys_Milliseconds();
	for (i = 0; i < copies; ++i) {
		Q_strncpyz(dest, src, sizeof(dest));
	}
	time = Sys_Milliseconds() - time;
	Com_Printf("%d copies with Q_strncpyz: %ld ms\n", copies, time);

	time = Sys_Milliseconds();
	for (i = 0; i < copies; ++i) {
		UTF8_strncpyz(dest, src, sizeof(dest));
	}
	time = Sys_Milliseconds() - time;
	Com_Printf("%d copies with UTF8_strncpyz: %ld ms\n", copies, time);

	time = Sys_Milliseconds();
	for (i = 0; i < copies; ++i) {
		Com_sprintf(dest, sizeof(dest), "%s", src);
	}
	time = Sys_Milliseconds() - time;
	Com_Printf("%d copies with Com_sprintf: %ld ms\n", copies, time);

	/* Com_sprintf */

	/* empty string */
	Com_sprintf(dest, 1, "aab%c%c", 0xd0, 0x80);
	CU_ASSERT_EQUAL(dest[0], '\0');

	/* trimmed non utf8 */
	Com_sprintf(dest, 4, "aab%c%c", 0xd0, 0x80);
	CU_ASSERT_EQUAL(dest[2], 'b');
	CU_ASSERT_EQUAL(dest[3], '\0');

	/* trimmed utf8 char */
	Com_sprintf(dest, 5, "aab%c%c", 0xd0, 0x80);
	CU_ASSERT_EQUAL(dest[2], 'b');
	CU_ASSERT_EQUAL(dest[3], '\0');

	/* untrimmed utf8 char */
	Com_sprintf(dest, 6, "aab%c%c", 0xd0, 0x80);
	CU_ASSERT_EQUAL((unsigned char) dest[3], 0xd0);
	CU_ASSERT_EQUAL((unsigned char) dest[4], 0x80);
	CU_ASSERT_EQUAL(dest[5], '\0');

	/* 2 consecutive utf8 char */
	Com_sprintf(dest, 7, "aab\xD0\x80\xD0\x80");
	CU_ASSERT_NOT_EQUAL(dest[3], '\0');
	CU_ASSERT_EQUAL(dest[5], '\0');

	/* UTF8_strncpyz */

	/* empty string */
	UTF8_strncpyz(dest, "aab\xD0\x80", 1);
	CU_ASSERT_EQUAL(dest[0], '\0');

	/* trimmed non utf8 */
	UTF8_strncpyz(dest, "aab\xD0\x80", 4);
	CU_ASSERT_EQUAL(dest[2], 'b');
	CU_ASSERT_EQUAL(dest[3], '\0');

	/* trimmed utf8 char */
	UTF8_strncpyz(dest, "aab\xD0\x80", 5);
	CU_ASSERT_EQUAL(dest[2], 'b');
	CU_ASSERT_EQUAL(dest[3], '\0');

	/* untrimmed utf8 char */
	UTF8_strncpyz(dest, "aab\xD0\x80", 6);
	CU_ASSERT_EQUAL((unsigned char) dest[3], 0xd0);
	CU_ASSERT_EQUAL((unsigned char) dest[4], 0x80);
	CU_ASSERT_EQUAL(dest[5], '\0');

	/* 2 consecutive utf8 char */
	UTF8_strncpyz(dest, "aab\xD0\x80\xD0\x80", 7);
	CU_ASSERT_NOT_EQUAL(dest[3], '\0');
	CU_ASSERT_EQUAL(dest[5], '\0');
}

static void testStringFunctions (void)
{
	char targetBuf[256];
	const size_t length = lengthof(targetBuf);

	CU_ASSERT_FALSE(Q_strreplace("ReplaceNothing", "###", "foobar", targetBuf, length));
	CU_ASSERT_TRUE(Q_strreplace("Replace###Something", "###", "foobar", targetBuf, length));
	CU_ASSERT_STRING_EQUAL(targetBuf, "ReplacefoobarSomething");

	CU_ASSERT_TRUE(Q_strreplace("Replace#", "#", "foobar", targetBuf, length));
	CU_ASSERT_STRING_EQUAL(targetBuf, "Replacefoobar");

	CU_ASSERT_TRUE(Q_strreplace("#Replace", "#", "foobar", targetBuf, length));
	CU_ASSERT_STRING_EQUAL(targetBuf, "foobarReplace");

	CU_ASSERT_TRUE(Q_strreplace("#Replace#", "#", "foobar", targetBuf, length));
	CU_ASSERT_STRING_EQUAL(targetBuf, "foobarReplace#");

	CU_ASSERT_FALSE(Q_strreplace("#ReplaceNothing#", "##", "foobar", targetBuf, length));
}

static void testHttpHelperFunctions (void)
{
	char server[512];
	char uriPath[512];
	int port;
	const char *url;

	url = "http://www.test.domain.com:123/someScript.cgi?parameter";
	CU_ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	CU_ASSERT_STRING_EQUAL(server, "www.test.domain.com")
	CU_ASSERT_EQUAL(port, 123);
	CU_ASSERT_STRING_EQUAL(uriPath, "/someScript.cgi?parameter");

	url = "http://www.test.domain.com/someScript.cgi?parameter";
	CU_ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	CU_ASSERT_STRING_EQUAL(server, "www.test.domain.com")
	CU_ASSERT_EQUAL(port, 80);
	CU_ASSERT_STRING_EQUAL(uriPath, "/someScript.cgi?parameter");

	url = "http://www.test.domain.com";
	CU_ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	CU_ASSERT_STRING_EQUAL(server, "www.test.domain.com")
	CU_ASSERT_EQUAL(port, 80);
	CU_ASSERT_STRING_EQUAL(uriPath, "");

	url = "http://www.test.domain.com/";
	CU_ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	CU_ASSERT_STRING_EQUAL(server, "www.test.domain.com")
	CU_ASSERT_EQUAL(port, 80);
	CU_ASSERT_STRING_EQUAL(uriPath, "/");

	url = "http://ufoai.ninex.info/ufo/masterserver.php?query";
	CU_ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	CU_ASSERT_STRING_EQUAL(server, "ufoai.ninex.info")
	CU_ASSERT_EQUAL(port, 80);
	CU_ASSERT_STRING_EQUAL(uriPath, "/ufo/masterserver.php?query");
}

static void testNetResolv (void)
{
	char ipServer[MAX_VAR];
	NET_ResolvNode("localhost", ipServer, sizeof(ipServer));
	CU_ASSERT_STRING_EQUAL(ipServer, "127.0.0.1");
}

int UFO_AddGenericTests (void)
{
	/* add a suite to the registry */
	CU_pSuite GenericSuite = CU_add_suite("GenericTests", UFO_InitSuiteGeneric, UFO_CleanSuiteGeneric);

	if (GenericSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(GenericSuite, testStringHunks) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testConstInt) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedList) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedListIterator) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedListIteratorRemove) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedListStringSort) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testFileSystemBuildLists) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testInfoStrings) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testCvars) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testStringFunctions) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testStringCopiers) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testHttpHelperFunctions) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testNetResolv) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
