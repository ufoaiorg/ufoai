/**
 * @file
 * @brief Test cases for code below common/ and shared/
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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
#include "../common/sha1.h"
#include "../common/sha2.h"
#include "../common/http.h"
#include "../common/binaryexpressionparser.h"
#include "../shared/utf8.h"
#include "../shared/shared.h"
#include "../shared/parse.h"
#include "../shared/infostring.h"
#include "../shared/stringhunk.h"
#include "../shared/entitiesdef.h"
#include "../ports/system.h"
#include "test_generic.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteGeneric (void)
{
	TEST_Init();
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

static void STRHUNK_VisitorTestEntry (const char* string)
{
	CU_ASSERT_STRING_EQUAL(string, "Test");
}

static void STRHUNK_VisitorTestEntry2 (const char* string)
{
	CU_ASSERT_STRING_EQUAL(string, "T");
}

static void testStringHunks (void)
{
	stringHunk_t* hunk = STRHUNK_Create(20);
	CU_ASSERT_TRUE(STRHUNK_Add(hunk, "Test"));
	CU_ASSERT_EQUAL(STRHUNK_Size(hunk), 1);
	CU_ASSERT_EQUAL(STRHUNK_GetFreeSpace(hunk), 15);
	STRHUNK_Visit(hunk, STRHUNK_VisitorTestEntry);
	STRHUNK_Delete(&hunk);
	CU_ASSERT_PTR_EQUAL(hunk, nullptr);

	hunk = STRHUNK_Create(23);
	CU_ASSERT_TRUE(STRHUNK_Add(hunk, "Test"));
	CU_ASSERT_TRUE(STRHUNK_Add(hunk, "Test"));
	CU_ASSERT_TRUE(STRHUNK_Add(hunk, "Test"));
	CU_ASSERT_TRUE(STRHUNK_Add(hunk, "Test"));
	CU_ASSERT_EQUAL(STRHUNK_Size(hunk), 4);
	CU_ASSERT_EQUAL(STRHUNK_GetFreeSpace(hunk), 0);
	STRHUNK_Visit(hunk, STRHUNK_VisitorTestEntry);

	STRHUNK_Reset(hunk);
	CU_ASSERT_EQUAL(STRHUNK_Size(hunk), 0);

	STRHUNK_Delete(&hunk);
	CU_ASSERT_PTR_EQUAL(hunk, nullptr);

	hunk = STRHUNK_Create(5);
	CU_ASSERT_TRUE(STRHUNK_Add(hunk, "T"));
	CU_ASSERT_FALSE(STRHUNK_Add(hunk, "Test"));
	/* the second string is ignored */
	CU_ASSERT_FALSE(STRHUNK_Add(hunk, "Test"));
	CU_ASSERT_EQUAL(STRHUNK_Size(hunk), 2);
	STRHUNK_Visit(hunk, STRHUNK_VisitorTestEntry2);
	STRHUNK_Delete(&hunk);
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

		{nullptr, -1}
	};
	const constListEntry_t list2[] = {
		{"namespace2::soldier", 0},
		{"namespace2::scientist", 1},
		{"namespace2::worker", 2},
		{"namespace2::pilot", 3},
		{nullptr, -1}
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

static int testListSorter (linkedList_t* entry1, linkedList_t* entry2, const void* userData)
{
	return strcmp((const char*)entry1->data, (const char*)entry2->data);
}

static void testLinkedList (void)
{
	linkedList_t* list = nullptr, *list2;
	const char* data = "SomeDataForTheLinkedList";
	const size_t length = strlen(data);
	linkedList_t* entry;
	const linkedList_t* entry2;
	const char* returnedData;

	entry = LIST_Add(&list, data, length);
	CU_ASSERT_EQUAL(LIST_Count(list), 1);
	CU_ASSERT_TRUE(entry != nullptr);
	returnedData = (const char*)LIST_GetByIdx(list, 0);
	CU_ASSERT_TRUE(returnedData != nullptr);
	entry2 = LIST_ContainsString(list, returnedData);
	CU_ASSERT_TRUE(entry2 != nullptr);
	CU_ASSERT_EQUAL((const void*)entry2->data, (const void*)returnedData);
	CU_ASSERT_STRING_EQUAL(entry2->data, returnedData);
	LIST_RemoveEntry(&list, entry);
	CU_ASSERT_EQUAL(LIST_Count(list), 0);
	CU_ASSERT_EQUAL(LIST_Count(LIST_CopyStructure(list)), 0);
	LIST_Add(&list, data, length);
	list2 = LIST_CopyStructure(list);
	CU_ASSERT_EQUAL(LIST_Count(list2), 1);
	LIST_Delete(&list2);
	CU_ASSERT_EQUAL(LIST_Count(list2), 0);
	CU_ASSERT_EQUAL(LIST_Count(list), 1);
	LIST_Delete(&list);
	CU_ASSERT_PTR_NULL(LIST_GetRandom(list2));

	LIST_AddString(&list, "test6");
	LIST_AddString(&list, "test2");
	LIST_AddString(&list, "test1");
	LIST_AddString(&list, "test3");
	LIST_AddString(&list, "test5");
	LIST_AddString(&list, "test4");
	LIST_AddString(&list, "test7");
	CU_ASSERT_EQUAL(LIST_Count(list), 7);
	CU_ASSERT_PTR_NOT_NULL(LIST_GetRandom(list));

	LIST_Sort(&list, testListSorter, nullptr);
	CU_ASSERT_STRING_EQUAL(LIST_GetByIdx(list, 0), "test1");
	CU_ASSERT_STRING_EQUAL(LIST_GetByIdx(list, 1), "test2");
	CU_ASSERT_STRING_EQUAL(LIST_GetByIdx(list, 2), "test3");
	CU_ASSERT_STRING_EQUAL(LIST_GetByIdx(list, 3), "test4");
	CU_ASSERT_STRING_EQUAL(LIST_GetByIdx(list, 4), "test5");
	CU_ASSERT_STRING_EQUAL(LIST_GetByIdx(list, 5), "test6");
	CU_ASSERT_STRING_EQUAL(LIST_GetByIdx(list, 6), "test7");
}

static void testLinkedListIterator (void)
{
	linkedList_t* list = nullptr;
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

	list = nullptr;
	LIST_Foreach(list, char, string) {
		(void)string;
		/* we should not be here, because the list is empty */
		CU_ASSERT_TRUE(false);
	}
}

static void testLinkedListIteratorRemove (void)
{
	linkedList_t* list = nullptr;

	LIST_AddString(&list, "test1");
	LIST_AddString(&list, "test2");
	LIST_AddString(&list, "test3");

	LIST_Foreach(list, char, string) {
		Com_Printf("Found string: %s\n", string);
		LIST_Remove(&list, string);
	}

	CU_ASSERT_TRUE(LIST_IsEmpty(list));
}

static void testPrependStringList (void)
{
	linkedList_t* list = nullptr;

	LIST_PrependString(&list, "test2");
	LIST_PrependString(&list, "test1");

	CU_ASSERT_STRING_EQUAL((const char*)LIST_GetByIdx(list, 0), "test1");
	CU_ASSERT_STRING_EQUAL((const char*)LIST_GetByIdx(list, 1), "test2");

	LIST_Delete(&list);
}

static void testLinkedListStringSort (void)
{
	linkedList_t* list = nullptr;

	LIST_AddStringSorted(&list, "test2");
	LIST_AddStringSorted(&list, "test1");
	LIST_AddStringSorted(&list, "test3");

	CU_ASSERT_STRING_EQUAL((const char*)LIST_GetByIdx(list, 0), "test1");

	LIST_Delete(&list);
}

static void testFileSystemBuildLists (void)
{
	const char* filename, *prev;
	const char* wildcard = "ufos/**.ufo";
	const int ufosCnt = FS_BuildFileList(wildcard);

	CU_ASSERT_TRUE(ufosCnt > 1);

	prev = nullptr;
	while ((filename = FS_NextFileFromFileList(wildcard)) != nullptr) {
		if (prev != nullptr)
			CU_ASSERT_EQUAL(Q_StringSort(prev, filename), -1);
		prev = filename;
	}

	FS_NextFileFromFileList(nullptr);
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

static void testTokenizeInfoStrings (void)
{
	Cvar_Get("password", "test", CVAR_USERINFO, nullptr);
	char info[MAX_INFO_STRING];
	const char* s = va(SV_CMD_CONNECT " %i \"%s\"\n", PROTOCOL_VERSION, Cvar_Userinfo(info, sizeof(info)));
	Cmd_TokenizeString(s, false, false);
	CU_ASSERT_STRING_EQUAL(Cmd_Argv(0), SV_CMD_CONNECT);
	CU_ASSERT_STRING_EQUAL(Cmd_Argv(1), DOUBLEQUOTE(PROTOCOL_VERSION));
	CU_ASSERT_STRING_EQUAL(Cmd_Argv(2), info);
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

	const char* s = src;
	int cnt = 0;
	while (UTF8_next(&s) != -1) {
		cnt++;
	}
	CU_ASSERT_EQUAL(cnt, UTF8_strlen(src));

	/* Com_sprintf */

	/* empty string */
	Com_sprintf(dest, 1, "aab%c%c", 0xd0, 0x80);
	CU_ASSERT_EQUAL(dest[0], '\0');

	/* trimmed non utf8 */
	Com_sprintf(dest, 4, "aab%c%c", 0xd0, 0x80);
	CU_ASSERT_EQUAL(dest[2], 'b');
	CU_ASSERT_EQUAL(dest[3], '\0');

	/* trimmed utf8 char. */
	Com_sprintf(dest, 5, "aab%c%c", 0xd0, 0x80);
	CU_ASSERT_EQUAL(dest[2], 'b');
	CU_ASSERT_EQUAL(dest[3], '\0');

	/* untrimmed utf8 char. */
	Com_sprintf(dest, 6, "aab%c%c", 0xd0, 0x80);
	CU_ASSERT_EQUAL((unsigned char) dest[3], 0xd0);
	CU_ASSERT_EQUAL((unsigned char) dest[4], 0x80);
	CU_ASSERT_EQUAL(dest[5], '\0');

	/* 2 consecutive utf8 char. */
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

	/* trimmed utf8 char. */
	UTF8_strncpyz(dest, "aab\xD0\x80", 5);
	CU_ASSERT_EQUAL(dest[2], 'b');
	CU_ASSERT_EQUAL(dest[3], '\0');

	/* untrimmed utf8 char. */
	UTF8_strncpyz(dest, "aab\xD0\x80", 6);
	CU_ASSERT_EQUAL((unsigned char) dest[3], 0xd0);
	CU_ASSERT_EQUAL((unsigned char) dest[4], 0x80);
	CU_ASSERT_EQUAL(dest[5], '\0');

	/* 2 consecutive utf8 char. */
	UTF8_strncpyz(dest, "aab\xD0\x80\xD0\x80", 7);
	CU_ASSERT_NOT_EQUAL(dest[3], '\0');
	CU_ASSERT_EQUAL(dest[5], '\0');
}

static void testStringFunctions (void)
{
	char targetBuf[256];
	char buf[16];
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

	Q_strncpyz(buf, "foobar", sizeof(buf));
	CU_ASSERT_STRING_EQUAL(Com_ConvertToASCII7(buf), "foobar");

	buf[0] = '\177';
	CU_ASSERT_STRING_EQUAL(Com_ConvertToASCII7(buf), ".oobar");

	buf[5] = '\177';
	CU_ASSERT_STRING_EQUAL(Com_ConvertToASCII7(buf), ".ooba.");


	/* UTF8_char_offset_to_byte_offset */

	Q_strncpyz(buf, "mn\xD0\x80opq\xD0\x81r", sizeof(buf));

	CU_ASSERT_EQUAL(UTF8_char_offset_to_byte_offset(buf, 4), 5);
	CU_ASSERT_EQUAL(UTF8_char_offset_to_byte_offset(buf, 0), 0);
	CU_ASSERT_EQUAL(UTF8_char_offset_to_byte_offset(buf, -1), 0);
	CU_ASSERT_EQUAL(UTF8_char_offset_to_byte_offset(buf, 999), 10);

	/* UTF8_delete_char_at */

	Q_strncpyz(buf, "mn\xD0\x80opq\xD0\x81r", sizeof(buf));

	/* single-byte char before any multi-byte chars */
	CU_ASSERT_EQUAL(UTF8_delete_char_at(buf, 4), 1);
	CU_ASSERT_STRING_EQUAL(buf, "mn\xD0\x80oq\xD0\x81r")

	/* multi-byte char after a multi-byte char */
	CU_ASSERT_EQUAL(UTF8_delete_char_at(buf, 5), 2);
	CU_ASSERT_STRING_EQUAL(buf, "mn\xD0\x80oqr");

	/* negative index deletes first char */
	CU_ASSERT_EQUAL(UTF8_delete_char_at(buf, -1), 1);
	CU_ASSERT_STRING_EQUAL(buf, "n\xD0\x80oqr");

	/* too-high index deletes nothing */
	CU_ASSERT_EQUAL(UTF8_delete_char_at(buf, 999), 0);
	CU_ASSERT_STRING_EQUAL(buf, "n\xD0\x80oqr");

	/* UTF8_insert_char_at */

	Q_strncpyz(buf, "m\xD0\x82opqr", sizeof(buf));

	/* single-byte char before any multi-byte chars */
	CU_ASSERT_EQUAL(UTF8_insert_char_at(buf, sizeof(buf), 1, (int)'n'), 1);
	CU_ASSERT_STRING_EQUAL(buf, "mn\xD0\x82opqr");

	/* multi-byte char after a multi-byte char */
	CU_ASSERT_EQUAL(UTF8_insert_char_at(buf, sizeof(buf), 5, 0x0403), 2);
	CU_ASSERT_STRING_EQUAL(buf, "mn\xD0\x82op\xD0\x83qr");

	/* negative index inserts as first char */
	CU_ASSERT_EQUAL(UTF8_insert_char_at(buf, sizeof(buf), -1, 0x0404), 2);
	CU_ASSERT_STRING_EQUAL(buf, "\xD0\x84mn\xD0\x82op\xD0\x83qr");

	/* too-high index inserts as last char */
	CU_ASSERT_EQUAL(UTF8_insert_char_at(buf, sizeof(buf), 999, 0x0405), 2);
	CU_ASSERT_STRING_EQUAL(buf, "\xD0\x84mn\xD0\x82op\xD0\x83qr\xD0\x85");

	Q_strncpyz(buf, "mnopqr", sizeof(buf));

	/* trigger buffer overrun protection using multi-byte char */
	CU_ASSERT_EQUAL(UTF8_insert_char_at(buf, 8, 0, 0x0405), 0);
	CU_ASSERT_STRING_EQUAL(buf, "mnopqr");

	/* trigger buffer overrun protection using single-byte char */
	CU_ASSERT_EQUAL(UTF8_insert_char_at(buf, 7, 0, (int)'o'), 0);
	CU_ASSERT_STRING_EQUAL(buf, "mnopqr");

	/* exactly fill the buffer using multi-byte char */
	CU_ASSERT_EQUAL(UTF8_insert_char_at(buf, 9, 1, 0x0406), 2);
	CU_ASSERT_STRING_EQUAL(buf, "m\xD0\x86nopqr");

	/* exactly fill the buffer using single-byte char */
	CU_ASSERT_EQUAL(UTF8_insert_char_at(buf, 10, 1, (int)'s'), 1);
	CU_ASSERT_STRING_EQUAL(buf, "ms\xD0\x86nopqr");

	char catBuf[32] = "";
	Q_strcat(catBuf, sizeof(catBuf), "foo");
	Q_strcat(catBuf, sizeof(catBuf), "bar");
	CU_ASSERT_STRING_EQUAL(catBuf, "foobar");
}

static void testHttpHelperFunctions (void)
{
	char server[512];
	char uriPath[512];
	int port;
	const char* url;

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

	url = "http://ufoai.org/ufo/masterserver.php?query";
	CU_ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	CU_ASSERT_STRING_EQUAL(server, "ufoai.org")
	CU_ASSERT_EQUAL(port, 80);
	CU_ASSERT_STRING_EQUAL(uriPath, "/ufo/masterserver.php?query");
}

static void testNetResolv (void)
{
	char ipServer[MAX_VAR];
	NET_ResolvNode("localhost", ipServer, sizeof(ipServer));
	CU_ASSERT_STRING_EQUAL(ipServer, "127.0.0.1");
}

static void testUnsignedIntToBinary (void)
{
	const char* buf = Com_UnsignedIntToBinary(3);
	CU_ASSERT_STRING_EQUAL(buf, "00000000 00000000 00000000 00000011");

	buf = Com_UnsignedIntToBinary(255);
	CU_ASSERT_STRING_EQUAL(buf, "00000000 00000000 00000000 11111111");

	buf = Com_UnsignedIntToBinary(65536);
	CU_ASSERT_STRING_EQUAL(buf, "00000000 00000001 00000000 00000000");

	buf = Com_UnsignedIntToBinary(65535);
	CU_ASSERT_STRING_EQUAL(buf, "00000000 00000000 11111111 11111111");

	buf = Com_ByteToBinary(2);
	CU_ASSERT_STRING_EQUAL(buf, "00000010");

	buf = Com_ByteToBinary(255);
	CU_ASSERT_STRING_EQUAL(buf, "11111111");
}

static void testStringCheckFunctions (void)
{
	const char* strNull = nullptr;
	const char* strEmpty = "";
	const char* strValid = "someString";
	CU_ASSERT_TRUE(Q_strnull(strNull));
	CU_ASSERT_TRUE(Q_strnull(strEmpty));
	CU_ASSERT_FALSE(Q_strnull(strValid));
	CU_ASSERT_TRUE(Q_strvalid(strValid));
	CU_ASSERT_FALSE(Q_strvalid(strEmpty));
	CU_ASSERT_FALSE(Q_strvalid(strNull));
}

static void testEntitiesDef (void)
{
	byte* fileBuffer;

	FS_LoadFile("ufos/entities.ufo", &fileBuffer);

	const char* buf = (const char*) fileBuffer;
	CU_ASSERT_EQUAL(ED_Parse(buf), ED_OK);

	CU_ASSERT_TRUE(numEntityDefs > 0);

	bool worldSpawnFound = false;
	for (int i = 0; i < numEntityDefs; i++) {
		const entityDef_t* e = &entityDefs[i];

		CU_ASSERT_PTR_NOT_NULL(e);
		if (Q_streq(e->classname, "worldspawn")) {
			worldSpawnFound = true;

			CU_ASSERT_TRUE(e->numKeyDefs > 10);
			for (int j = 0; j < e->numKeyDefs; j++) {
				const entityKeyDef_t* keyDef = &e->keyDefs[j];
				CU_ASSERT_PTR_NOT_NULL(keyDef);
			}
		}
	}

	CU_ASSERT_TRUE(worldSpawnFound);

	FS_FreeFile(fileBuffer);
	ED_Free();
}

static void testGetBlock (void)
{
	{
		const char* test = "invalid block";
		const int length = Com_GetBlock(&test, nullptr);
		UFO_CU_ASSERT_EQUAL_INT_MSG(length, -1, nullptr);
	}
	{
		const char* test = "{the block length  }";
		const char* buf = test;
		const size_t expected = strlen(test) - 2;
		const char* start = nullptr;
		const int length = Com_GetBlock(&buf, &start);
		CU_ASSERT_EQUAL(length, expected);
		UFO_CU_ASSERT_EQUAL_INT_MSG(strncmp(start, test + 1, length), 0,
				va("%s and %s should match on the first %i characters", start, test + 1, length));
	}
}

static void testComFilePath (void)
{
	char buf[32];
	Com_FilePath("/foo/bar/file.txt", buf, sizeof(buf));
	CU_ASSERT_STRING_EQUAL(buf, "/foo/bar");

	Com_FilePath("/foo/bar/a/little/bit/too/long/for/the/buffer/file.txt", buf, sizeof(buf));
	CU_ASSERT_STRING_EQUAL(buf, "");
}

static void testMD5File (void)
{
	const char* md5 = Com_MD5File("media/DejaVuSans.ttf");
	const char* expected = "c4adcbdd6ec636e0b19cd6aabe85e8fb";
	Com_Printf("got: '%s', expected '%s'\n", md5, expected);
	CU_ASSERT_STRING_EQUAL(md5, expected);
}

static void testMD5Buffer (void)
{
	const char* in = "Test";
	const char* expected = "0cbc6611f5540bd0809a388dc95a615b";
	const char* md5 = Com_MD5Buffer((const byte*)in, strlen(in));
	Com_Printf("got: '%s', expected '%s'\n", md5, expected);
	CU_ASSERT_STRING_EQUAL(md5, expected);
}

static void testSHA1Buffer (void)
{
	const char* in = "Test";
	const char* expected = "640ab2bae07bedc4c163f679a746f7ab7fb5d1fa";
	char digest[41];
	Com_SHA1Buffer((const byte*)in, strlen(in), digest);
	Com_Printf("got: '%s', expected '%s'\n", digest, expected);
	CU_ASSERT_STRING_EQUAL(digest, expected);
}

static void testSHA2Buffer (void)
{
	const char* in = "Test";
	const char* expected = "532eaabd9574880dbf76b9b8cc00832c20a6ec113d682299550d7a6e0f345e25";
	byte digest[32];
	Com_SHA2Csum((const byte*)in, strlen(in), digest);
	char buf[65];
	Com_SHA2ToHex(digest, buf);
	Com_Printf("got: '%s', expected '%s'\n", buf, expected);
	CU_ASSERT_STRING_EQUAL(buf, expected);
}

/**
 * @brief The string 'a' and 'c' evaluates to true - everything else to false
 */
static int TEST_BEP (const char* id, const void* userdata)
{
	return Q_streq(id, "a") || Q_streq(id, "c");
}

static void testBinaryExpressionParser (void)
{
	CU_ASSERT_TRUE(BEP_Evaluate("", TEST_BEP));
	CU_ASSERT_TRUE(BEP_Evaluate("a", TEST_BEP));
	CU_ASSERT_TRUE(BEP_Evaluate("c", TEST_BEP));
	CU_ASSERT_FALSE(BEP_Evaluate("d", TEST_BEP));
	CU_ASSERT_TRUE(BEP_Evaluate("a|b", TEST_BEP));
	CU_ASSERT_FALSE(BEP_Evaluate("a&b", TEST_BEP));
	CU_ASSERT_TRUE(BEP_Evaluate("a&(b|c)", TEST_BEP));
	CU_ASSERT_TRUE(BEP_Evaluate("a|(b&c)", TEST_BEP));
	CU_ASSERT_FALSE(BEP_Evaluate("b|(d&c)", TEST_BEP));
	CU_ASSERT_TRUE(BEP_Evaluate("b|(a&c)", TEST_BEP));
	CU_ASSERT_FALSE(BEP_Evaluate("a^c", TEST_BEP));
	CU_ASSERT_TRUE(BEP_Evaluate("a^d", TEST_BEP));
	CU_ASSERT_TRUE(BEP_Evaluate("!z", TEST_BEP));
	CU_ASSERT_FALSE(BEP_Evaluate("!!z", TEST_BEP));
	CU_ASSERT_FALSE(BEP_Evaluate("!a", TEST_BEP));
}

int UFO_AddGenericTests (void)
{
	/* add a suite to the registry */
	CU_pSuite GenericSuite = CU_add_suite("GenericTests", UFO_InitSuiteGeneric, UFO_CleanSuiteGeneric);

	if (GenericSuite == nullptr)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(GenericSuite, testMD5File) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testMD5Buffer) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testSHA1Buffer) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testSHA2Buffer) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testStringHunks) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testConstInt) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedList) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedListIterator) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedListIteratorRemove) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testPrependStringList) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedListStringSort) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testFileSystemBuildLists) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testInfoStrings) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testTokenizeInfoStrings) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testCvars) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testStringFunctions) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testStringCopiers) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testHttpHelperFunctions) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testNetResolv) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testUnsignedIntToBinary) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testStringCheckFunctions) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testEntitiesDef) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testGetBlock) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testComFilePath) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testBinaryExpressionParser) == nullptr)
		return CU_get_error();

	return CUE_SUCCESS;
}
