/**
 * @file test_parser.c
 * @brief Test cases for code about shared parser
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
#include "test_parser.h"
#include "../shared/ufotypes.h"
#include "../shared/parse.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteParser (void)
{
	TEST_Init();
	/* default state */
	Com_EnableFunctionScriptToken(qfalse);
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteParser (void)
{
	TEST_Shutdown();
	Com_EnableFunctionScriptToken(qfalse);
	return 0;
}

/**
 * @brief unittest around default use of parser
 */
static void testParser (void)
{
	const char* string = "aa \t\n aa,({\"bbb(bbb bbb)bbb {\"{a}\n/* foooo { } \n { } */\n// fooooo\naaaa";
	const char* cursor = string;
	const char *token;

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "aa");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "aa,(");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "{");

	token = Com_Parse(&cursor);
	CU_ASSERT_TRUE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "bbb(bbb bbb)bbb {");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "{");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "a");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "}");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "aaaa");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "\0");
}

/**
 * @brief unittest to check back slash entity conversion
 */
static void testParserWithEntity (void)
{
	const char* string = "\n\taaaa \"  \\n  \\t  \\\"  \"";
	const char* cursor = string;
	const char *token;

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "aaaa");

	token = Com_Parse(&cursor);
	CU_ASSERT_TRUE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "  \n  \t  \"  ");
}

/**
 * @brief unittest around default use of parser
 */
static void testParserWithUnParse (void)
{
	const char* string = "aaaaa\n\tbbbbb \"ccccc\"";
	const char* cursor = string;
	const char *token;

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "aaaaa");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "aaaaa");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "aaaaa");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "bbbbb");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "bbbbb");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "bbbbb");

	token = Com_Parse(&cursor);
	CU_ASSERT_TRUE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "ccccc");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	CU_ASSERT_TRUE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "ccccc");

	Com_UnParseLastToken();

	token = Com_Parse(&cursor);
	CU_ASSERT_TRUE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "ccccc");
}

/**
 * @brief unittest around default use of parser
 */
static void testParserWithFunctionScriptToken (void)
{
	const char* string = "aa \t\n aa,({\"bbb(bbb bbb)bbb {\"{a}\n/* foooo { } \n { } */\n// fooooo\naaaa)";
	const char* cursor = string;
	const char *token;

	Com_EnableFunctionScriptToken(qtrue);

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "aa");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "aa");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, ",");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "(");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "{");

	token = Com_Parse(&cursor);
	CU_ASSERT_TRUE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "bbb(bbb bbb)bbb {");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "{");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "a");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "}");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "aaaa");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, ")");

	token = Com_Parse(&cursor);
	CU_ASSERT_FALSE(Com_ParsedTokenIsQuoted());
	CU_ASSERT_STRING_EQUAL(token, "\0");
}

/**
 * @brief unittest around common type
 */
static void testParserCommonType (void)
{
	int ivalue;
	qboolean bvalue;
	float fvalue;
	align_t align;
	blend_t blend;
	style_t style;
	fade_t fade;
	size_t bytes;
	int result;

	/* boolean */

	bytes = 0;
	result = Com_ParseValue (&bvalue, "true", V_BOOL, 0, sizeof(qboolean), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_OK);
	CU_ASSERT_EQUAL(bvalue, 1);
	CU_ASSERT_EQUAL(bytes, sizeof(qboolean));

	bytes = 0;
	result = Com_ParseValue (&bvalue, "false", V_BOOL, 0, sizeof(qboolean), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_OK);
	CU_ASSERT_EQUAL(bvalue, 0);
	CU_ASSERT_EQUAL(bytes, sizeof(qboolean));

	bytes = 0;
	result = Com_ParseValue (&bvalue, "foo", V_INT, 0, sizeof(int), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_ERROR);
	CU_ASSERT_EQUAL(bytes, 0);

	/* int */

	bytes = 0;
	result = Com_ParseValue (&ivalue, "10", V_INT, 0, sizeof(int), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_OK);
	CU_ASSERT_EQUAL(ivalue, 10);
	CU_ASSERT_EQUAL(bytes, sizeof(int));

	bytes = 0;
	result = Com_ParseValue (&ivalue, "abc", V_INT, 0, sizeof(int), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_ERROR);
	CU_ASSERT_EQUAL(bytes, 0);

	/* float */

	bytes = 0;
	result = Com_ParseValue (&fvalue, "1.1", V_FLOAT, 0, sizeof(float), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_OK);
	CU_ASSERT_EQUAL(fvalue, 1.1f);
	CU_ASSERT_EQUAL(bytes, sizeof(float));

	bytes = 0;
	result = Com_ParseValue (&fvalue, "9.8", V_FLOAT, 0, sizeof(float), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_OK);
	CU_ASSERT_EQUAL(fvalue, 9.8f);
	CU_ASSERT_EQUAL(bytes, sizeof(float));

	bytes = 0;
	result = Com_ParseValue (&fvalue, "abc", V_FLOAT, 0, sizeof(float), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_ERROR);
	CU_ASSERT_EQUAL(bytes, 0);

	/** @todo V_POS */
	/** @todo V_VECTOR */
	/** @todo V_COLOR */
	/** @todo V_RGBA */
	/** @todo V_STRING */
	/** @todo V_TRANSLATION_STRING */
	/** @todo V_LONGSTRING */

	/* align */

	bytes = 0;
	result = Com_ParseValue (&align, "cc", V_ALIGN, 0, sizeof(align_t), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_OK);
	CU_ASSERT_EQUAL(align, ALIGN_CC);
	CU_ASSERT_EQUAL(bytes, sizeof(align_t));

	bytes = 0;
	result = Com_ParseValue (&align, "abc", V_ALIGN, 0, sizeof(align_t), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_ERROR);
	CU_ASSERT_EQUAL(bytes, 0);

	/* blend */

	bytes = 0;
	result = Com_ParseValue (&blend, "blend", V_BLEND, 0, sizeof(blend_t), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_OK);
	CU_ASSERT_EQUAL(blend, BLEND_BLEND);
	CU_ASSERT_EQUAL(bytes, sizeof(blend_t));

	bytes = 0;
	result = Com_ParseValue (&blend, "abc", V_BLEND, 0, sizeof(blend_t), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_ERROR);
	CU_ASSERT_EQUAL(bytes, 0);

	/* style */

	bytes = 0;
	result = Com_ParseValue (&style, "rotated", V_STYLE, 0, sizeof(style_t), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_OK);
	CU_ASSERT_EQUAL(style, STYLE_ROTATED);
	CU_ASSERT_EQUAL(bytes, sizeof(style_t));

	bytes = 0;
	result = Com_ParseValue (&style, "abc", V_STYLE, 0, sizeof(style_t), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_ERROR);
	CU_ASSERT_EQUAL(bytes, 0);

	/* fade */

	bytes = 0;
	result = Com_ParseValue (&fade, "sin", V_FADE, 0, sizeof(fade_t), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_OK);
	CU_ASSERT_EQUAL(fade, FADE_SIN);
	CU_ASSERT_EQUAL(bytes, sizeof(fade_t));

	bytes = 0;
	result = Com_ParseValue (&fade, "abc", V_FADE, 0, sizeof(fade_t), &bytes);
	CU_ASSERT_EQUAL(result, RESULT_ERROR);
	CU_ASSERT_EQUAL(bytes, 0);

}

int UFO_AddParserTests (void)
{
	/* add a suite to the registry */
	CU_pSuite ParserSuite = CU_add_suite("ParserTests", UFO_InitSuiteParser, UFO_CleanSuiteParser);

	if (ParserSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(ParserSuite, testParser) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(ParserSuite, testParserWithFunctionScriptToken) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(ParserSuite, testParserWithUnParse) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(ParserSuite, testParserWithEntity) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(ParserSuite, testParserCommonType) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
