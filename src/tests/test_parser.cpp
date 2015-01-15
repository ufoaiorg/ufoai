/**
 * @file
 * @brief Test cases for code about shared parser
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
#include "../shared/ufotypes.h"
#include "../shared/parse.h"

class ParserTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}
};

/**
 * @brief unittest around default use of parser
 */
TEST_F(ParserTest, Parser)
{
	const char* string = "aa \t\n {\"bbb(bbb bbb)bbb {\"{a}\n/* foooo { } \n { } */\n// fooooo\naaaa";
	const char* cursor = string;
	const char* token;

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "aa");

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "{");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "bbb(bbb bbb)bbb {");

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "{");

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "a");

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "}");

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "aaaa");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_EOF);
	ASSERT_STREQ(token, "\0");
}

/**
 * @brief unittest to check back slash entity conversion
 */
TEST_F(ParserTest, ParserWithEntity)
{
	const char* string = "\n\taaaa \"  \\n  \\t  \\\"  \"";
	const char* cursor = string;
	const char* token;

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "aaaa");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "  \n  \t  \"  ");
}

/**
 * @brief unittest around default use of parser
 */
TEST_F(ParserTest, ParserWithUnParse)
{
	const char* string = "aaaaa\n\tbbbbb \"ccccc\"";
	const char* cursor = string;
	const char* token;

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "aaaaa");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "aaaaa");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "aaaaa");

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "bbbbb");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "bbbbb");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	ASSERT_NE(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "bbbbb");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "ccccc");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "ccccc");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "ccccc");
}

/**
 * @brief unittest around default use of parser
 */
TEST_F(ParserTest, ParserWithFunctionScriptToken)
{
	const char* string = "aa \t\n aa,({\"bbb(bbb bbb)bbb {\"{a}\n/* foooo { } \n { } */\n// fooooo\naaaa)";
	const char* cursor = string;
	const char* token;

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_WORD);
	ASSERT_STREQ(token, "aa");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_WORD);
	ASSERT_STREQ(token, "aa");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_COMMA);
	ASSERT_STREQ(token, ",");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_BEGIN_LIST);
	ASSERT_STREQ(token, "(");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_BEGIN_BLOCK);
	ASSERT_STREQ(token, "{");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_QUOTED_WORD);
	ASSERT_STREQ(token, "bbb(bbb bbb)bbb {");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_BEGIN_BLOCK);
	ASSERT_STREQ(token, "{");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_WORD);
	ASSERT_STREQ(token, "a");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_END_BLOCK);
	ASSERT_STREQ(token, "}");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_WORD);
	ASSERT_STREQ(token, "aaaa");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_END_LIST);
	ASSERT_STREQ(token, ")");

	token = Com_Parse(&cursor);
	ASSERT_EQ(Com_GetType(&cursor), TT_EOF);
	ASSERT_STREQ(token, "\0");
}

/**
 * @brief unittest around common type
 */
TEST_F(ParserTest, ParserCommonType)
{
	int ivalue;
	bool bvalue;
	float fvalue;
	align_t align;
	blend_t blend;
	style_t style;
	fade_t fade;
	size_t bytes;
	int result;

	/* boolean */

	bytes = 0;
	result = Com_ParseValue (&bvalue, "true", V_BOOL, 0, sizeof(bool), &bytes);
	ASSERT_EQ(result, RESULT_OK);
	ASSERT_EQ(bvalue, 1);
	ASSERT_EQ(bytes, sizeof(bool));

	bytes = 0;
	result = Com_ParseValue (&bvalue, "false", V_BOOL, 0, sizeof(bool), &bytes);
	ASSERT_EQ(result, RESULT_OK);
	ASSERT_EQ(bvalue, 0);
	ASSERT_EQ(bytes, sizeof(bool));

	bytes = 0;
	result = Com_ParseValue (&bvalue, "foo", V_BOOL, 0, sizeof(int), &bytes);
	ASSERT_EQ(result, RESULT_ERROR);
	ASSERT_EQ(bytes, 0);

	/* int */

	bytes = 0;
	result = Com_ParseValue (&ivalue, "10", V_INT, 0, sizeof(int), &bytes);
	ASSERT_EQ(result, RESULT_OK);
	ASSERT_EQ(ivalue, 10);
	ASSERT_EQ(bytes, sizeof(int));

	bytes = 0;
	result = Com_ParseValue (&ivalue, "abc", V_INT, 0, sizeof(int), &bytes);
	ASSERT_EQ(result, RESULT_ERROR);
	ASSERT_EQ(bytes, 0);

	/* float */

	bytes = 0;
	result = Com_ParseValue (&fvalue, "1.1", V_FLOAT, 0, sizeof(float), &bytes);
	ASSERT_EQ(result, RESULT_OK);
	ASSERT_EQ(fvalue, 1.1f);
	ASSERT_EQ(bytes, sizeof(float));

	bytes = 0;
	result = Com_ParseValue (&fvalue, "9.8", V_FLOAT, 0, sizeof(float), &bytes);
	ASSERT_EQ(result, RESULT_OK);
	ASSERT_EQ(fvalue, 9.8f);
	ASSERT_EQ(bytes, sizeof(float));

	bytes = 0;
	result = Com_ParseValue (&fvalue, "abc", V_FLOAT, 0, sizeof(float), &bytes);
	ASSERT_EQ(result, RESULT_ERROR);
	ASSERT_EQ(bytes, 0);

	/** @todo V_POS */
	/** @todo V_VECTOR */
	/** @todo V_COLOR */
	/** @todo V_STRING */
	/** @todo V_TRANSLATION_STRING */
	/** @todo V_LONGSTRING */

	/* align */

	bytes = 0;
	result = Com_ParseValue (&align, "cc", V_ALIGN, 0, sizeof(align_t), &bytes);
	ASSERT_EQ(result, RESULT_OK);
	ASSERT_EQ(align, ALIGN_CC);
	ASSERT_EQ(bytes, sizeof(align_t));

	bytes = 0;
	result = Com_ParseValue (&align, "abc", V_ALIGN, 0, sizeof(align_t), &bytes);
	ASSERT_EQ(result, RESULT_ERROR);
	ASSERT_EQ(bytes, 0);

	/* blend */

	bytes = 0;
	result = Com_ParseValue (&blend, "blend", V_BLEND, 0, sizeof(blend_t), &bytes);
	ASSERT_EQ(result, RESULT_OK);
	ASSERT_EQ(blend, BLEND_BLEND);
	ASSERT_EQ(bytes, sizeof(blend_t));

	bytes = 0;
	result = Com_ParseValue (&blend, "abc", V_BLEND, 0, sizeof(blend_t), &bytes);
	ASSERT_EQ(result, RESULT_ERROR);
	ASSERT_EQ(bytes, 0);

	/* style */

	bytes = 0;
	result = Com_ParseValue (&style, "rotated", V_STYLE, 0, sizeof(style_t), &bytes);
	ASSERT_EQ(result, RESULT_OK);
	ASSERT_EQ(style, STYLE_ROTATED);
	ASSERT_EQ(bytes, sizeof(style_t));

	bytes = 0;
	result = Com_ParseValue (&style, "abc", V_STYLE, 0, sizeof(style_t), &bytes);
	ASSERT_EQ(result, RESULT_ERROR);
	ASSERT_EQ(bytes, 0);

	/* fade */

	bytes = 0;
	result = Com_ParseValue (&fade, "sin", V_FADE, 0, sizeof(fade_t), &bytes);
	ASSERT_EQ(result, RESULT_OK);
	ASSERT_EQ(fade, FADE_SIN);
	ASSERT_EQ(bytes, sizeof(fade_t));

	bytes = 0;
	result = Com_ParseValue (&fade, "abc", V_FADE, 0, sizeof(fade_t), &bytes);
	ASSERT_EQ(result, RESULT_ERROR);
	ASSERT_EQ(bytes, 0);
}

/**
 * @brief unittest to check well formed list
 */
TEST_F(ParserTest, ParserListOk)
{
	const char* string = " (  aaa \n \"bbb\" \t ccc \n \n ) ";
	const char* cursor = string;
	linkedList_t* list;

	ASSERT_TRUE(Com_ParseList(&cursor, &list)) << "List parsing failed";

	ASSERT_EQ(LIST_Count(list), 3);

	ASSERT_STREQ(static_cast<const char*>(list->data), "aaa");
	ASSERT_STREQ(static_cast<const char*>(list->next->data), "bbb");
	ASSERT_STREQ(static_cast<const char*>(list->next->next->data), "ccc");

	LIST_Delete(&list);
}


/**
 * @brief unittest to check well formed empty list
 */
TEST_F(ParserTest, ParserListOkEmpty)
{
	const char* string = " (  \n ) ()";
	const char* cursor = string;
	linkedList_t* list;

	ASSERT_TRUE(Com_ParseList(&cursor, &list)) << "List parsing failed";

	ASSERT_EQ(LIST_Count(list), 0);

	ASSERT_TRUE(Com_ParseList(&cursor, &list)) << "List parsing failed";

	ASSERT_EQ(LIST_Count(list), 0);
}

/**
 * @brief unittest to check wrong list with EOF
 */
TEST_F(ParserTest, ParserListKoEOF)
{
	const char* string = " (  aaa \n bbb \t ccc \n \n";
	const char* cursor = string;
	linkedList_t* list;

	ASSERT_FALSE(Com_ParseList(&cursor, &list)) << "List parsing succeed, which is wrong";
}

/**
 * @brief unittest to check wrong list with unexpected token
 */
TEST_F(ParserTest, ParserListKoWrongToken)
{
	const char* string = " (  aaa \n bbb \t ccc \n \n } ";
	const char* cursor = string;

	linkedList_t* list;
	ASSERT_FALSE(Com_ParseList(&cursor, &list)) << "List parsing succeed, which is wrong";
}

/**
 * @brief unittest to check wrong list which contains another sublist
 */
TEST_F(ParserTest, ParserListKoNewList)
{
	const char* string = " (  aaa \n bbb \t ccc \n \n ( ";
	const char* cursor = string;
	linkedList_t* list;

	ASSERT_FALSE(Com_ParseList(&cursor, &list)) << "List parsing succeed, which is wrong";
}
