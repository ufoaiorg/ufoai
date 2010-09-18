/**
 * @file test_generic.c
 * @brief Test cases for code below common/ and shared/
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "CUnit/Basic.h"
#include "../common/common.h"
#include "../shared/utf8.h"
#include "../shared/shared.h"
#include "test_generic.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteGeneric (void)
{
	com_aliasSysPool = Mem_CreatePool("Common: Alias system");
	com_cmdSysPool = Mem_CreatePool("Common: Command system");
	com_cmodelSysPool = Mem_CreatePool("Common: Collision model");
	com_cvarSysPool = Mem_CreatePool("Common: Cvar system");
	com_fileSysPool = Mem_CreatePool("Common: File system");
	com_genericPool = Mem_CreatePool("Generic");

	Mem_Init();
	Cmd_Init();
	Cvar_Init();
	FS_InitFilesystem(qtrue);
	Swap_Init();

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteGeneric (void)
{
	FS_Shutdown();
	Cmd_Shutdown();
	Cvar_Shutdown();
	Mem_Shutdown();
	return 0;
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
	CU_ASSERT_EQUAL(out, 2);

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
	returnedData = LIST_GetByIdx(list, 0);
	CU_ASSERT_TRUE(returnedData != NULL);
	entry2 = LIST_ContainsString(list, returnedData);
	CU_ASSERT_TRUE(entry2 != NULL);
	CU_ASSERT_EQUAL((const void*)entry2->data, (const void*)returnedData);
	CU_ASSERT_STRING_EQUAL(entry2->data, returnedData);
	LIST_Remove(&list, entry);
	CU_ASSERT_EQUAL(LIST_Count(list), 0);
}

static void testLinkedListStringSort (void)
{
	linkedList_t *list = NULL;

	LIST_AddStringSorted(&list, "test2");
	LIST_AddStringSorted(&list, "test1");
	LIST_AddStringSorted(&list, "test3");

	CU_ASSERT_STRING_EQUAL((const char *)LIST_GetByIdx(list, 0), "test1");
}

static void testStringCopiers (void)
{
	const char src[] = "Командующий, я чрезвычайно рад доложить, что наш проект ОПЭВ был завершён успешно. Я прикрепил к письму изображения и схемы прототипа лазерного оружия, в котором реализовано непрерывное волновое излучение достаточной мощности в портативном корпусе. Практическое решение лежало не в тщетных попытках увеличения ёмкости аккумуляторной батареи, а в использовании радикальных технологий миниатюризации с целью облегчения и уменьшения размеров существующих лазеров с химической накачкой. Они использовались довольно долгое время, будучи частью экспериментальных военных программ с конца XX века, но их применение всегда сталкивалось с множеством проблем. Модели химических лазеров с реальными военными перспективами были слишком большими и громоздкими для использования в бою пехотой. До сих пор их расположение было ограничено выбором между лазерными орудиями на сухопутном шасси и батареями морского базирования. Боевое применение заключалось в основном для целей противоракетной обороны (ПРО). Тем не менее, теперь мы в ФАЛАНКС создали компактное лазерное орудие. Вопреки своему малому размеру, оно полностью способно управлять фтор-дейтериевой реакцией и её токсическими продуктами распада без опасности поражения личного состава. Разрешите мне дать вам краткое описание принципа его работы. Внутри камеры сгорания этилен окисляется в пламени трифторида азота. В ходе этой реакции выделяются свободные радикалы фтора, которые затем приводятся в контакт с газообразной смесью гелия и дейтерия. Дейтерий вступает в реакцию с фтором, что приводит к образованию энергетически возбуждённых молекул фторида дейтерия. Они подвергаются стимулированной эмиссии в оптическом резонаторе оружия, генерируя лазерный луч. Серия интеллектуальных линз фокусирует и направляет луч в точку прицеливания, и даже вносит коррекцию, с учётом небольших перемещений стрелка и/или цели. Излишки газообразного дейтерия направляются через сконструированную нами систему фильтров высокого давления, которые задерживают все токсичные и радиоактивные молекулы перед выбросом отработанных паров в атмосферу. Фильтр требует замены после каждой боевой операции, но разработанная нами система делает этот процесс простым и безболезненным. Если система фильтров будет так или иначе повреждена в ходе операции, оружие немедленно заблокируется, чтобы избежать поражения бойца и окружающих. Лазер работает в среднем инфракрасном диапазоне, так что он генерирует невидимый глазом луч, который однако можно видеть в инфракрасных очках. К сожалению, прототип, который мы создали, ещё не готов к широкомасштабному производству в качестве оружия пехоты, так как требуется оптимизация и балансировка для его использования человеком, но инновационные разработки, сделанные нами, могут так же применяться в орудийных системах боевых машин, и, возможно, даже в авиационных орудиях. Я отправил вам на электронную почту несколько новых концепций с принципиальными идеями, предложенными моим отделом. Сообщите, когда вы сочтёте нужным, чтоб мы начали работу над ними, командующий. —ком. Наварре ";

	long time;
	const int copies = 10000;
	char dest[4096];
	int i;
	
	printf("\n");
	time = Sys_Milliseconds();
	for (i = 0; i < copies; ++i) {
		Q_strncpyz(dest, src, sizeof(dest));
	}
	time = Sys_Milliseconds() - time;
	printf("%d copies with Q_strncpyz: %ld ms\n", copies, time);

	time = Sys_Milliseconds();
	for (i = 0; i < copies; ++i) {
		UTF8_strncpyz(dest, src, sizeof(dest));
	}
	time = Sys_Milliseconds() - time;
	printf("%d copies with UTF8_strncpyz: %ld ms\n", copies, time);

	time = Sys_Milliseconds();
	for (i = 0; i < copies; ++i) {
		Com_sprintf(dest, sizeof(dest), "%s", src);
	}
	time = Sys_Milliseconds() - time;
	printf("%d copies with Com_sprintf: %ld ms\n", copies, time);

}

int UFO_AddGenericTests (void)
{
	/* add a suite to the registry */
	CU_pSuite GenericSuite = CU_add_suite("GenericTests", UFO_InitSuiteGeneric, UFO_CleanSuiteGeneric);

	if (GenericSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(GenericSuite, testConstInt) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedList) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedListStringSort) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testStringCopiers) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
