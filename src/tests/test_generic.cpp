/**
 * @file
 * @brief Test cases for code below common/ and shared/
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

class GenericTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
		NET_Shutdown();
	}
};

static void STRHUNK_VisitorTestEntry (const char* string)
{
	ASSERT_STREQ(string, "Test");
}

static void STRHUNK_VisitorTestEntry2 (const char* string)
{
	ASSERT_STREQ(string, "T");
}

TEST_F(GenericTest, StringHunks)
{
	stringHunk_t* hunk = STRHUNK_Create(20);
	ASSERT_TRUE(STRHUNK_Add(hunk, "Test"));
	ASSERT_EQ(STRHUNK_Size(hunk), 1);
	ASSERT_EQ(STRHUNK_GetFreeSpace(hunk), 15);
	STRHUNK_Visit(hunk, STRHUNK_VisitorTestEntry);
	STRHUNK_Delete(&hunk);
	ASSERT_TRUE(nullptr == hunk);

	hunk = STRHUNK_Create(23);
	ASSERT_TRUE(STRHUNK_Add(hunk, "Test"));
	ASSERT_TRUE(STRHUNK_Add(hunk, "Test"));
	ASSERT_TRUE(STRHUNK_Add(hunk, "Test"));
	ASSERT_TRUE(STRHUNK_Add(hunk, "Test"));
	ASSERT_EQ(STRHUNK_Size(hunk), 4);
	ASSERT_EQ(STRHUNK_GetFreeSpace(hunk), 0);
	STRHUNK_Visit(hunk, STRHUNK_VisitorTestEntry);

	STRHUNK_Reset(hunk);
	ASSERT_EQ(STRHUNK_Size(hunk), 0);

	STRHUNK_Delete(&hunk);
	ASSERT_TRUE(nullptr == hunk);

	hunk = STRHUNK_Create(5);
	ASSERT_TRUE(STRHUNK_Add(hunk, "T"));
	ASSERT_FALSE(STRHUNK_Add(hunk, "Test"));
	/* the second string is ignored */
	ASSERT_FALSE(STRHUNK_Add(hunk, "Test"));
	ASSERT_EQ(STRHUNK_Size(hunk), 2);
	STRHUNK_Visit(hunk, STRHUNK_VisitorTestEntry2);
	STRHUNK_Delete(&hunk);
}

TEST_F(GenericTest, ConstInt)
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
	ASSERT_TRUE(Com_UnregisterConstVariable("namespace::variable"));

	Com_RegisterConstInt("namespace::variable", 1);
	ASSERT_TRUE(Com_UnregisterConstVariable("namespace::variable"));

	Com_RegisterConstInt("namespace::variable2", 2);
	Com_RegisterConstInt("namespace::variable3", 3);
	Com_RegisterConstInt("namespace::variable4", 4);
	Com_RegisterConstInt("namespace::variable5", 5);
	Com_RegisterConstInt("namespace::variable6", 6);

	Com_RegisterConstInt("namespace2::variable2", 10);

	out = 0;
	ASSERT_TRUE(Com_GetConstInt("namespace2::variable2", &out));
	ASSERT_EQ(out, 10);
	out = 0;
	ASSERT_TRUE(Com_GetConstInt("namespace::variable2", &out));
	ASSERT_EQ(out, 2);
	out = 0;
	ASSERT_TRUE(Com_GetConstInt("variable2", &out));
	ASSERT_EQ(out, 10);

	ASSERT_STREQ(Com_GetConstVariable("namespace", 2), "variable2");

	ASSERT_TRUE(Com_UnregisterConstVariable("namespace2::variable2"));
	ASSERT_TRUE(Com_UnregisterConstVariable("namespace::variable2"));
	ASSERT_TRUE(Com_UnregisterConstVariable("namespace::variable3"));
	ASSERT_TRUE(Com_UnregisterConstVariable("namespace::variable4"));
	ASSERT_TRUE(Com_UnregisterConstVariable("namespace::variable5"));
	ASSERT_TRUE(Com_UnregisterConstVariable("namespace::variable6"));

	ASSERT_TRUE(!Com_UnregisterConstVariable("namespace::variable"));
	ASSERT_TRUE(!Com_UnregisterConstVariable("namespace::variable2"));
	ASSERT_TRUE(!Com_UnregisterConstVariable("namespace::variable3"));
	ASSERT_TRUE(!Com_UnregisterConstVariable("namespace::variable4"));
	ASSERT_TRUE(!Com_UnregisterConstVariable("namespace::variable5"));
	ASSERT_TRUE(!Com_UnregisterConstVariable("namespace::variable6"));

	Com_RegisterConstList(list);
	out = 0;
	ASSERT_TRUE(Com_GetConstInt("sniper", &out));
	ASSERT_EQ(out, 8);

	ASSERT_TRUE(Com_UnregisterConstList(list));
	out = 0;
	ASSERT_FALSE(Com_GetConstInt("sniper", &out));

	Com_RegisterConstList(list2);

	Com_RegisterConstList(list);
	ASSERT_TRUE(Com_UnregisterConstList(list));

	out = 0;
	ASSERT_TRUE(Com_GetConstInt("pilot", &out));
	ASSERT_EQ(out, 3);
	Com_UnregisterConstList(list2);
}

static int testListSorter (linkedList_t* entry1, linkedList_t* entry2, const void* userData)
{
	return strcmp((const char*)entry1->data, (const char*)entry2->data);
}

TEST_F(GenericTest, LinkedList)
{
	linkedList_t* list = nullptr, *list2;
	const char* data = "SomeDataForTheLinkedList";
	const size_t length = strlen(data);
	linkedList_t* entry;
	const linkedList_t* entry2;
	const char* returnedData;

	entry = LIST_Add(&list, data, length);
	ASSERT_EQ(LIST_Count(list), 1);
	ASSERT_TRUE(entry != nullptr);
	returnedData = (const char*)LIST_GetByIdx(list, 0);
	ASSERT_TRUE(returnedData != nullptr);
	entry2 = LIST_ContainsString(list, returnedData);
	ASSERT_TRUE(entry2 != nullptr);
	ASSERT_EQ((const void*)entry2->data, (const void*)returnedData);
	ASSERT_STREQ(static_cast<const char*>(entry2->data), returnedData);
	LIST_RemoveEntry(&list, entry);
	ASSERT_EQ(LIST_Count(list), 0);
	ASSERT_EQ(LIST_Count(LIST_CopyStructure(list)), 0);
	LIST_Add(&list, data, length);
	list2 = LIST_CopyStructure(list);
	ASSERT_EQ(LIST_Count(list2), 1);
	LIST_Delete(&list2);
	ASSERT_EQ(LIST_Count(list2), 0);
	ASSERT_EQ(LIST_Count(list), 1);
	LIST_Delete(&list);
	ASSERT_TRUE(nullptr == LIST_GetRandom(list2));

	LIST_AddString(&list, "test6");
	LIST_AddString(&list, "test2");
	LIST_AddString(&list, "test1");
	LIST_AddString(&list, "test3");
	LIST_AddString(&list, "test5");
	LIST_AddString(&list, "test4");
	LIST_AddString(&list, "test7");
	ASSERT_EQ(LIST_Count(list), 7);
	ASSERT_TRUE(nullptr != LIST_GetRandom(list));

	LIST_Sort(&list, testListSorter, nullptr);
	ASSERT_STREQ(static_cast<const char*>(LIST_GetByIdx(list, 0)), "test1");
	ASSERT_STREQ(static_cast<const char*>(LIST_GetByIdx(list, 1)), "test2");
	ASSERT_STREQ(static_cast<const char*>(LIST_GetByIdx(list, 2)), "test3");
	ASSERT_STREQ(static_cast<const char*>(LIST_GetByIdx(list, 3)), "test4");
	ASSERT_STREQ(static_cast<const char*>(LIST_GetByIdx(list, 4)), "test5");
	ASSERT_STREQ(static_cast<const char*>(LIST_GetByIdx(list, 5)), "test6");
	ASSERT_STREQ(static_cast<const char*>(LIST_GetByIdx(list, 6)), "test7");
}

TEST_F(GenericTest, LinkedListIterator)
{
	linkedList_t* list = nullptr;
	int cnt;

	LIST_AddString(&list, "test1");
	LIST_AddString(&list, "test2");
	LIST_AddString(&list, "test3");

	cnt = 0;
	LIST_Foreach(list, char, string) {
		ASSERT_TRUE(nullptr != string);
		cnt++;
	}

	LIST_Delete(&list);

	ASSERT_EQ(cnt, 3);

	list = nullptr;
	LIST_Foreach(list, char, string) {
		(void)string;
		/* we should not be here, because the list is empty */
		ASSERT_TRUE(false);
	}
}

TEST_F(GenericTest, LinkedListIteratorRemove)
{
	linkedList_t* list = nullptr;

	LIST_AddString(&list, "test1");
	LIST_AddString(&list, "test2");
	LIST_AddString(&list, "test3");

	LIST_Foreach(list, char, string) {
		Com_Printf("Found string: %s\n", string);
		LIST_Remove(&list, string);
	}

	ASSERT_TRUE(LIST_IsEmpty(list));
}

TEST_F(GenericTest, PrependStringList)
{
	linkedList_t* list = nullptr;

	LIST_PrependString(&list, "test2");
	LIST_PrependString(&list, "test1");

	ASSERT_STREQ((const char*)LIST_GetByIdx(list, 0), "test1");
	ASSERT_STREQ((const char*)LIST_GetByIdx(list, 1), "test2");

	LIST_Delete(&list);
}

TEST_F(GenericTest, LinkedListStringSort)
{
	linkedList_t* list = nullptr;

	LIST_AddStringSorted(&list, "test2");
	LIST_AddStringSorted(&list, "test1");
	LIST_AddStringSorted(&list, "test3");

	ASSERT_STREQ((const char*)LIST_GetByIdx(list, 0), "test1");

	LIST_Delete(&list);
}

TEST_F(GenericTest, FileSystemBuildLists)
{
	const char* filename, *prev;
	const char* wildcard = "ufos/**.ufo";
	const int ufosCnt = FS_BuildFileList(wildcard);

	ASSERT_TRUE(ufosCnt > 1);

	prev = nullptr;
	while ((filename = FS_NextFileFromFileList(wildcard)) != nullptr) {
		if (prev != nullptr)
			ASSERT_EQ(Q_StringSort(prev, filename), -1);
		prev = filename;
	}

	FS_NextFileFromFileList(nullptr);
}

TEST_F(GenericTest, InfoStrings)
{
	char info[MAX_INFO_STRING] = "";

	Info_SetValueForKey(info, sizeof(info), "name", "test");

	ASSERT_STREQ(Info_ValueForKey(info, "name"), "test");
	ASSERT_STREQ(Info_ValueForKey(info, "name2"), "");
	Info_RemoveKey(info, "name");
	ASSERT_STREQ(Info_ValueForKey(info, "name"), "");

	Info_SetValueForKey(info, sizeof(info), "name", "\\invalid\\value");
	ASSERT_STREQ(Info_ValueForKey(info, "name"), "");
}

TEST_F(GenericTest, TokenizeInfoStrings)
{
	Cvar_Get("password", "test", CVAR_USERINFO, nullptr);
	char info[MAX_INFO_STRING];
	const char* s = va(SV_CMD_CONNECT " %i \"%s\"\n", PROTOCOL_VERSION, Cvar_Userinfo(info, sizeof(info)));
	Cmd_TokenizeString(s, false, false);
	ASSERT_STREQ(Cmd_Argv(0), SV_CMD_CONNECT);
	ASSERT_STREQ(Cmd_Argv(1), DOUBLEQUOTE(PROTOCOL_VERSION));
	ASSERT_STREQ(Cmd_Argv(2), info);
}

TEST_F(GenericTest, Cvars)
{
	Cvar_Get("testGeneric_cvar", "testGeneric_cvarValue", CVAR_NOSET, "No set");
	Cvar_Set("testGeneric_cvar", "test");
	ASSERT_STREQ(Cvar_GetString("testGeneric_cvar"), "testGeneric_cvarValue");
}

TEST_F(GenericTest, StringCopiers)
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
	ASSERT_EQ(cnt, UTF8_strlen(src));

	/* Com_sprintf */

	/* empty string */
	Com_sprintf(dest, 1, "aab%c%c", 0xd0, 0x80);
	ASSERT_EQ(dest[0], '\0');

	/* trimmed non utf8 */
	Com_sprintf(dest, 4, "aab%c%c", 0xd0, 0x80);
	ASSERT_EQ(dest[2], 'b');
	ASSERT_EQ(dest[3], '\0');

	/* trimmed utf8 char. */
	Com_sprintf(dest, 5, "aab%c%c", 0xd0, 0x80);
	ASSERT_EQ(dest[2], 'b');
	ASSERT_EQ(dest[3], '\0');

	/* untrimmed utf8 char. */
	Com_sprintf(dest, 6, "aab%c%c", 0xd0, 0x80);
	ASSERT_EQ((unsigned char) dest[3], 0xd0);
	ASSERT_EQ((unsigned char) dest[4], 0x80);
	ASSERT_EQ(dest[5], '\0');

	/* 2 consecutive utf8 char. */
	Com_sprintf(dest, 7, "aab\xD0\x80\xD0\x80");
	ASSERT_NE(dest[3], '\0');
	ASSERT_EQ(dest[5], '\0');

	/* UTF8_strncpyz */

	/* empty string */
	UTF8_strncpyz(dest, "aab\xD0\x80", 1);
	ASSERT_EQ(dest[0], '\0');

	/* trimmed non utf8 */
	UTF8_strncpyz(dest, "aab\xD0\x80", 4);
	ASSERT_EQ(dest[2], 'b');
	ASSERT_EQ(dest[3], '\0');

	/* trimmed utf8 char. */
	UTF8_strncpyz(dest, "aab\xD0\x80", 5);
	ASSERT_EQ(dest[2], 'b');
	ASSERT_EQ(dest[3], '\0');

	/* untrimmed utf8 char. */
	UTF8_strncpyz(dest, "aab\xD0\x80", 6);
	ASSERT_EQ((unsigned char) dest[3], 0xd0);
	ASSERT_EQ((unsigned char) dest[4], 0x80);
	ASSERT_EQ(dest[5], '\0');

	/* 2 consecutive utf8 char. */
	UTF8_strncpyz(dest, "aab\xD0\x80\xD0\x80", 7);
	ASSERT_NE(dest[3], '\0');
	ASSERT_EQ(dest[5], '\0');
}

TEST_F(GenericTest, StringFunctions)
{
	char targetBuf[256];
	char buf[16];
	const size_t length = lengthof(targetBuf);

	ASSERT_FALSE(Q_strreplace("ReplaceNothing", "###", "foobar", targetBuf, length));
	ASSERT_TRUE(Q_strreplace("Replace###Something", "###", "foobar", targetBuf, length));
	ASSERT_STREQ(targetBuf, "ReplacefoobarSomething");

	ASSERT_TRUE(Q_strreplace("Replace#", "#", "foobar", targetBuf, length));
	ASSERT_STREQ(targetBuf, "Replacefoobar");

	ASSERT_TRUE(Q_strreplace("#Replace", "#", "foobar", targetBuf, length));
	ASSERT_STREQ(targetBuf, "foobarReplace");

	ASSERT_TRUE(Q_strreplace("#Replace#", "#", "foobar", targetBuf, length));
	ASSERT_STREQ(targetBuf, "foobarReplace#");

	ASSERT_FALSE(Q_strreplace("#ReplaceNothing#", "##", "foobar", targetBuf, length));

	Q_strncpyz(buf, "foobar", sizeof(buf));
	ASSERT_STREQ(Com_ConvertToASCII7(buf), "foobar");

	buf[0] = '\177';
	ASSERT_STREQ(Com_ConvertToASCII7(buf), ".oobar");

	buf[5] = '\177';
	ASSERT_STREQ(Com_ConvertToASCII7(buf), ".ooba.");


	/* UTF8_char_offset_to_byte_offset */

	Q_strncpyz(buf, "mn\xD0\x80opq\xD0\x81r", sizeof(buf));

	ASSERT_EQ(UTF8_char_offset_to_byte_offset(buf, 4), 5);
	ASSERT_EQ(UTF8_char_offset_to_byte_offset(buf, 0), 0);
	ASSERT_EQ(UTF8_char_offset_to_byte_offset(buf, -1), 0);
	ASSERT_EQ(UTF8_char_offset_to_byte_offset(buf, 999), 10);

	/* UTF8_delete_char_at */

	Q_strncpyz(buf, "mn\xD0\x80opq\xD0\x81r", sizeof(buf));

	/* single-byte char before any multi-byte chars */
	ASSERT_EQ(UTF8_delete_char_at(buf, 4), 1);
	ASSERT_STREQ(buf, "mn\xD0\x80oq\xD0\x81r");

	/* multi-byte char after a multi-byte char */
	ASSERT_EQ(UTF8_delete_char_at(buf, 5), 2);
	ASSERT_STREQ(buf, "mn\xD0\x80oqr");

	/* negative index deletes first char */
	ASSERT_EQ(UTF8_delete_char_at(buf, -1), 1);
	ASSERT_STREQ(buf, "n\xD0\x80oqr");

	/* too-high index deletes nothing */
	ASSERT_EQ(UTF8_delete_char_at(buf, 999), 0);
	ASSERT_STREQ(buf, "n\xD0\x80oqr");

	/* UTF8_insert_char_at */

	Q_strncpyz(buf, "m\xD0\x82opqr", sizeof(buf));

	/* single-byte char before any multi-byte chars */
	ASSERT_EQ(UTF8_insert_char_at(buf, sizeof(buf), 1, (int)'n'), 1);
	ASSERT_STREQ(buf, "mn\xD0\x82opqr");

	/* multi-byte char after a multi-byte char */
	ASSERT_EQ(UTF8_insert_char_at(buf, sizeof(buf), 5, 0x0403), 2);
	ASSERT_STREQ(buf, "mn\xD0\x82op\xD0\x83qr");

	/* negative index inserts as first char */
	ASSERT_EQ(UTF8_insert_char_at(buf, sizeof(buf), -1, 0x0404), 2);
	ASSERT_STREQ(buf, "\xD0\x84mn\xD0\x82op\xD0\x83qr");

	/* too-high index inserts as last char */
	ASSERT_EQ(UTF8_insert_char_at(buf, sizeof(buf), 999, 0x0405), 2);
	ASSERT_STREQ(buf, "\xD0\x84mn\xD0\x82op\xD0\x83qr\xD0\x85");

	Q_strncpyz(buf, "mnopqr", sizeof(buf));

	/* trigger buffer overrun protection using multi-byte char */
	ASSERT_EQ(UTF8_insert_char_at(buf, 8, 0, 0x0405), 0);
	ASSERT_STREQ(buf, "mnopqr");

	/* trigger buffer overrun protection using single-byte char */
	ASSERT_EQ(UTF8_insert_char_at(buf, 7, 0, (int)'o'), 0);
	ASSERT_STREQ(buf, "mnopqr");

	/* exactly fill the buffer using multi-byte char */
	ASSERT_EQ(UTF8_insert_char_at(buf, 9, 1, 0x0406), 2);
	ASSERT_STREQ(buf, "m\xD0\x86nopqr");

	/* exactly fill the buffer using single-byte char */
	ASSERT_EQ(UTF8_insert_char_at(buf, 10, 1, (int)'s'), 1);
	ASSERT_STREQ(buf, "ms\xD0\x86nopqr");

	char catBuf[32] = "";
	Q_strcat(catBuf, sizeof(catBuf), "foo");
	Q_strcat(catBuf, sizeof(catBuf), "bar");
	ASSERT_STREQ(catBuf, "foobar");
}

TEST_F(GenericTest, HttpHelperFunctions)
{
	char server[512];
	char uriPath[512];
	int port;
	const char* url;

	url = "http://www.test.domain.com:123/someScript.cgi?parameter";
	ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	ASSERT_STREQ("www.test.domain.com", server);
	ASSERT_EQ(port, 123);
	ASSERT_STREQ("/someScript.cgi?parameter", uriPath);

	url = "http://www.test.domain.com/someScript.cgi?parameter";
	ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	ASSERT_STREQ("www.test.domain.com", server);
	ASSERT_EQ(80, port);
	ASSERT_STREQ("/someScript.cgi?parameter", uriPath);

	url = "http://www.test.domain.com";
	ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	ASSERT_STREQ("www.test.domain.com", server);
	ASSERT_EQ(80, port);
	ASSERT_STREQ("", uriPath);

	url = "http://www.test.domain.com/";
	ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	ASSERT_STREQ("www.test.domain.com", server);
	ASSERT_EQ(80, port);
	ASSERT_STREQ("/", uriPath);

	url = "http://ufoai.org/ufo/masterserver.php?query";
	ASSERT_TRUE(HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port));
	ASSERT_STREQ("ufoai.org", server);
	ASSERT_EQ(80, port);
	ASSERT_STREQ("/ufo/masterserver.php?query", uriPath);
}

TEST_F(GenericTest, NetResolv)
{
	char ipServer[MAX_VAR];
	ASSERT_TRUE(NET_ResolvNode("localhost", ipServer, sizeof(ipServer))) << "failed to resolve localhost";
	ASSERT_STREQ("127.0.0.1", ipServer);
}

TEST_F(GenericTest, UnsignedIntToBinary)
{
	const char* buf = Com_UnsignedIntToBinary(3);
	ASSERT_STREQ(buf, "00000000 00000000 00000000 00000011");

	buf = Com_UnsignedIntToBinary(255);
	ASSERT_STREQ(buf, "00000000 00000000 00000000 11111111");

	buf = Com_UnsignedIntToBinary(65536);
	ASSERT_STREQ(buf, "00000000 00000001 00000000 00000000");

	buf = Com_UnsignedIntToBinary(65535);
	ASSERT_STREQ(buf, "00000000 00000000 11111111 11111111");

	buf = Com_ByteToBinary(2);
	ASSERT_STREQ(buf, "00000010");

	buf = Com_ByteToBinary(255);
	ASSERT_STREQ(buf, "11111111");
}

TEST_F(GenericTest, StringCheckFunctions)
{
	const char* strNull = nullptr;
	const char* strEmpty = "";
	const char* strValid = "someString";
	ASSERT_TRUE(Q_strnull(strNull));
	ASSERT_TRUE(Q_strnull(strEmpty));
	ASSERT_FALSE(Q_strnull(strValid));
	ASSERT_TRUE(Q_strvalid(strValid));
	ASSERT_FALSE(Q_strvalid(strEmpty));
	ASSERT_FALSE(Q_strvalid(strNull));
}

TEST_F(GenericTest, EntitiesDef)
{
	byte* fileBuffer;

	ASSERT_NE(-1, FS_LoadFile("ufos/entities.ufo", &fileBuffer)) << "Could not load ufos/entities.ufo.";
	ASSERT_TRUE(fileBuffer != nullptr);

	const char* buf = (const char*) fileBuffer;
	ASSERT_EQ(ED_Parse(buf), ED_OK);

	ASSERT_TRUE(numEntityDefs > 0);

	bool worldSpawnFound = false;
	for (int i = 0; i < numEntityDefs; i++) {
		const entityDef_t* e = &entityDefs[i];

		ASSERT_TRUE(nullptr != e);
		if (Q_streq(e->classname, "worldspawn")) {
			worldSpawnFound = true;

			ASSERT_TRUE(e->numKeyDefs > 10);
			for (int j = 0; j < e->numKeyDefs; j++) {
				const entityKeyDef_t* keyDef = &e->keyDefs[j];
				ASSERT_TRUE(nullptr != keyDef);
			}
		}
	}

	ASSERT_TRUE(worldSpawnFound);

	FS_FreeFile(fileBuffer);
	ED_Free();
}

TEST_F(GenericTest, GetBlock)
{
	{
		const char* test = "invalid block";
		const int length = Com_GetBlock(&test, nullptr);
		ASSERT_EQ(length, -1);
	}
	{
		const char* test = "{the block length  }";
		const char* buf = test;
		const size_t expected = strlen(test) - 2;
		const char* start = nullptr;
		const int length = Com_GetBlock(&buf, &start);
		ASSERT_EQ(length, expected);
		ASSERT_EQ(strncmp(start, test + 1, length), 0) << start << " and " << (test + 1) << " should match on the first " << length << " characters";
	}
}

TEST_F(GenericTest, ComFilePath)
{
	char buf[32];
	Com_FilePath("/foo/bar/file.txt", buf, sizeof(buf));
	ASSERT_STREQ(buf, "/foo/bar");

	Com_FilePath("/foo/bar/a/little/bit/too/long/for/the/buffer/file.txt", buf, sizeof(buf));
	ASSERT_STREQ(buf, "");
}

TEST_F(GenericTest, MD5File)
{
	const char* md5 = Com_MD5File("media/DejaVuSans.ttf");
	const char* expected = "c4adcbdd6ec636e0b19cd6aabe85e8fb";
	Com_Printf("got: '%s', expected '%s'\n", md5, expected);
	ASSERT_STREQ(md5, expected);
}

TEST_F(GenericTest, MD5Buffer)
{
	const char* in = "Test";
	const char* expected = "0cbc6611f5540bd0809a388dc95a615b";
	const char* md5 = Com_MD5Buffer((const byte*)in, strlen(in));
	Com_Printf("got: '%s', expected '%s'\n", md5, expected);
	ASSERT_STREQ(md5, expected);
}

TEST_F(GenericTest, SHA1Buffer)
{
	const char* in = "Test";
	const char* expected = "640ab2bae07bedc4c163f679a746f7ab7fb5d1fa";
	char digest[41];
	Com_SHA1Buffer((const byte*)in, strlen(in), digest);
	Com_Printf("got: '%s', expected '%s'\n", digest, expected);
	ASSERT_STREQ(digest, expected);
}

TEST_F(GenericTest, SHA2Buffer)
{
	const char* in = "Test";
	const char* expected = "532eaabd9574880dbf76b9b8cc00832c20a6ec113d682299550d7a6e0f345e25";
	byte digest[32];
	Com_SHA2Csum((const byte*)in, strlen(in), digest);
	char buf[65];
	Com_SHA2ToHex(digest, buf);
	Com_Printf("got: '%s', expected '%s'\n", buf, expected);
	ASSERT_STREQ(buf, expected);
}

/**
 * @brief The string 'a' and 'c' evaluates to true - everything else to false
 */
static int TEST_BEP (const char* id, const void* userdata)
{
	return Q_streq(id, "a") || Q_streq(id, "c");
}

TEST_F(GenericTest, BinaryExpressionParser)
{
	ASSERT_TRUE(BEP_Evaluate("", TEST_BEP));
	ASSERT_TRUE(BEP_Evaluate("a", TEST_BEP));
	ASSERT_TRUE(BEP_Evaluate("c", TEST_BEP));
	ASSERT_FALSE(BEP_Evaluate("d", TEST_BEP));
	ASSERT_TRUE(BEP_Evaluate("a|b", TEST_BEP));
	ASSERT_FALSE(BEP_Evaluate("a&b", TEST_BEP));
	ASSERT_TRUE(BEP_Evaluate("a&(b|c)", TEST_BEP));
	ASSERT_TRUE(BEP_Evaluate("a|(b&c)", TEST_BEP));
	ASSERT_FALSE(BEP_Evaluate("b|(d&c)", TEST_BEP));
	ASSERT_TRUE(BEP_Evaluate("b|(a&c)", TEST_BEP));
	ASSERT_FALSE(BEP_Evaluate("a^c", TEST_BEP));
	ASSERT_TRUE(BEP_Evaluate("a^d", TEST_BEP));
	ASSERT_TRUE(BEP_Evaluate("!z", TEST_BEP));
	ASSERT_FALSE(BEP_Evaluate("!!z", TEST_BEP));
	ASSERT_FALSE(BEP_Evaluate("!a", TEST_BEP));
}
