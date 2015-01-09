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
#include "../common/common.h"

class DBufferTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}
};

TEST_F(DBufferTest, testDBuffer)
{
	int i;
	const int size = 10000000;
	dbuffer* buf = new dbuffer();
	char data[128];
	size_t dataSize = sizeof(data);
	for (i = 0; i < size; i++)
		buf->add("b", 1);
	ASSERT_EQ(size, buf->length());

	ASSERT_EQ(dataSize, buf->get(data, dataSize));
	ASSERT_EQ(size, buf->length());

	ASSERT_EQ(dataSize, buf->extract(data, dataSize));
	ASSERT_EQ(size - dataSize, buf->length());

	delete buf;

	buf = new dbuffer();
	buf->add("b", 1);
	ASSERT_EQ(1, buf->length());

	ASSERT_EQ(1, buf->get(data, dataSize));
	ASSERT_EQ(1, buf->length());

	ASSERT_EQ(1, buf->extract(data, dataSize));
	ASSERT_EQ(0, buf->length());

	dbuffer* dup = new dbuffer(*buf);
	delete buf;
	buf = dup;
	ASSERT_EQ(0, buf->length());

	for (i = 0; i <= dataSize; i++)
		buf->add("b", 1);
	ASSERT_EQ(dataSize + 1, buf->length());

	ASSERT_EQ(dataSize, buf->extract(data, dataSize));
	ASSERT_EQ(1, buf->length());

	ASSERT_EQ(1, buf->remove(1));
	ASSERT_EQ(0, buf->length());

	ASSERT_EQ(0, buf->remove(1));

	ASSERT_EQ(0, buf->getAt(1, data, dataSize));

	for (i = 0; i <= dataSize; i++)
		buf->add("b", 1);
	ASSERT_EQ(dataSize + 1, buf->length());

	ASSERT_EQ(dataSize, buf->getAt(1, data, dataSize));
	ASSERT_EQ(dataSize + 1, buf->length());
}

TEST_F(DBufferTest, testDBufferBigData)
{
	int count = 100;
	byte* data;
	/* this entity string may not contain any inline models, we don't have the bsp tree loaded here */
	const int size = FS_LoadFile("game/entity.txt", &data);
	dbuffer buf;

	for (int i = 0; i < count; i++) {
		buf.add((char*)data, size);
	}

	ASSERT_EQ(size * count, buf.length());
	FS_FreeFile(data);
}

TEST_F(DBufferTest, testDBufferNetHandling)
{
	dbuffer buf;
	NET_WriteByte(&buf, 'b');
	ASSERT_EQ(1, buf.length());
	NET_WriteShort(&buf, 128);
	ASSERT_EQ(3, buf.length());
	NET_WriteLong(&buf, 128);
	ASSERT_EQ(7, buf.length());
}
