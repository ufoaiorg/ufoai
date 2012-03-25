/**
 * @file test_dbuffer.c
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


#include "test_dbuffer.h"
#include "test_shared.h"
#include "../common/common.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteDBuffer (void)
{
	TEST_Init();
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteDBuffer (void)
{
	TEST_Shutdown();
	return 0;
}

static void testDBuffer (void)
{
	int i;
	const int size = 10000000;
	struct dbuffer* buf = new_dbuffer();
	char data[128];
	size_t dataSize = sizeof(data);
	for (i = 0; i < size; i++)
		dbuffer_add(buf, "b", 1);
	CU_ASSERT_EQUAL(size, dbuffer_len(buf));

	CU_ASSERT_EQUAL(dataSize, dbuffer_get(buf, data, dataSize));
	CU_ASSERT_EQUAL(size, dbuffer_len(buf));

	CU_ASSERT_EQUAL(dataSize, dbuffer_extract(buf, data, dataSize));
	CU_ASSERT_EQUAL(size - dataSize, dbuffer_len(buf));

	free_dbuffer(buf);

	buf = new_dbuffer();
	dbuffer_add(buf, "b", 1);
	CU_ASSERT_EQUAL(1, dbuffer_len(buf));

	CU_ASSERT_EQUAL(1, dbuffer_get(buf, data, dataSize));
	CU_ASSERT_EQUAL(1, dbuffer_len(buf));

	CU_ASSERT_EQUAL(1, dbuffer_extract(buf, data, dataSize));
	CU_ASSERT_EQUAL(0, dbuffer_len(buf));

	buf = dbuffer_dup(buf);
	CU_ASSERT_EQUAL(0, dbuffer_len(buf));

	for (i = 0; i <= dataSize; i++)
		dbuffer_add(buf, "b", 1);
	CU_ASSERT_EQUAL(dataSize + 1, dbuffer_len(buf));

	CU_ASSERT_EQUAL(dataSize, dbuffer_extract(buf, data, dataSize));
	CU_ASSERT_EQUAL(1, dbuffer_len(buf));

	CU_ASSERT_EQUAL(1, dbuffer_remove(buf, 1));
	CU_ASSERT_EQUAL(0, dbuffer_len(buf));

	CU_ASSERT_EQUAL(0, dbuffer_remove(buf, 1));

	CU_ASSERT_EQUAL(0, dbuffer_get_at(buf, 1, data, dataSize));

	for (i = 0; i <= dataSize; i++)
		dbuffer_add(buf, "b", 1);
	CU_ASSERT_EQUAL(dataSize + 1, dbuffer_len(buf));

	CU_ASSERT_EQUAL(dataSize, dbuffer_get_at(buf, 1, data, dataSize));
	CU_ASSERT_EQUAL(dataSize + 1, dbuffer_len(buf));
}

static void testDBufferBigData (void)
{
	int i;
	int count = 100;
	byte *data;
	/* this entity string may not contain any inline models, we don't have the bsp tree loaded here */
	const int size = FS_LoadFile("game/entity.txt", &data);
	struct dbuffer* buf = new_dbuffer();

	for (i = 0; i < count; i++) {
		dbuffer_add(buf, (char *)data, size);
	}

	CU_ASSERT_EQUAL(size * count, dbuffer_len(buf));
	free_dbuffer(buf);
	FS_FreeFile(data);
}

static void testDBufferNetHandling (void)
{
	struct dbuffer* buf = new_dbuffer();
	NET_WriteByte(buf, 'b');
	CU_ASSERT_EQUAL(1, dbuffer_len(buf));
	NET_WriteShort(buf, 128);
	CU_ASSERT_EQUAL(3, dbuffer_len(buf));
	NET_WriteLong(buf, 128);
	CU_ASSERT_EQUAL(7, dbuffer_len(buf));
	free_dbuffer(buf);
}

int UFO_AddDBufferTests (void)
{
	/* add a suite to the registry */
	CU_pSuite DBufferSuite = CU_add_suite("DBufferTests", UFO_InitSuiteDBuffer, UFO_CleanSuiteDBuffer);
	if (DBufferSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(DBufferSuite, testDBuffer) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(DBufferSuite, testDBufferBigData) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(DBufferSuite, testDBufferNetHandling) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
