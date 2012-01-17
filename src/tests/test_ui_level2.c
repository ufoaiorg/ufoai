/**
 * @file test_ui.c
 * @brief Test cases for code below client/ui/ when the global UI "engine" is started
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
#include "test_ui_level2.h"
#include "../client/cl_video.h" /* vid_imagePool */
#include "../client/ui/ui_internal.h"
#include "../client/renderer/r_state.h" /* r_state */

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteUILevel2 (void)
{
	TEST_Init();
	UI_Init();

	vid_imagePool = Mem_CreatePool("Vid: Image system");

	r_state.active_texunit = &texunit_diffuse;

	return 0;
}

/** @todo move it somewhere */
static void TEST_ParseScript (const char* scriptName)
{
	const char *type, *name, *text;
	int nbWrongScripts = 0;

	/* parse ui node script */
	Com_Printf("Load \"%s\": %i script file(s)\n", scriptName, FS_BuildFileList(scriptName));
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;
	while ((type = FS_NextScriptHeader(scriptName, &name, &text)) != NULL) {
		const qboolean result = CL_ParseClientData(type, name, &text);
		if (!result) {
			CU_assertImplementation(CU_FALSE, __LINE__, va("Failed to parse needed scripts for type: %s and id %s", type, name), __FILE__, "", CU_FALSE);
			nbWrongScripts++;
		}
	}
	CU_ASSERT_FATAL(nbWrongScripts == 0);
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteUILevel2 (void)
{
	UI_Shutdown();
	TEST_Shutdown();
	return 0;
}

/**
 * @brief after execution of a unittest window, it analyse color of
 * normalized indicator, and create asserts. Both Fail/Pass asserts is
 * useful to count number.
 */
static void UFO_AnalyseTestWindow (const char* windowName)
{
	const uiBehaviour_t *stringBehaviour = UI_GetNodeBehaviour("string");
	const uiNode_t *node;
	const uiNode_t *window = UI_GetWindow(windowName);
	CU_ASSERT_FATAL(window != NULL);
	CU_ASSERT_FATAL(stringBehaviour != NULL);

	for (node = window->firstChild; node != NULL; node = node->next) {
		qboolean isGreen;
		qboolean isRed;
		/* skip non "string" nodes */
		if (node->behaviour != stringBehaviour)
			continue;
		if (node->invis)
			continue;
		/* skip nodes without "test" prefix */
		if (!Q_strstart(node->name, "test") != 0)
			continue;

		isGreen = node->color[0] < 0.1 && node->color[1] > 0.9 && node->color[2] < 0.1;
		isRed = node->color[0] > 0.9 && node->color[1] < 0.1 && node->color[2] < 0.1;

		if (isGreen) {
			/** @note Useful to count number of tests */
			CU_ASSERT_TRUE(qtrue);
		} else if (isRed) {
			const char *message = va("%s.%s failed.", windowName, node->name);
			/*Com_Printf("Error: %s\n", message);*/
			/*CU_FAIL(message);*/
			CU_assertImplementation(CU_FALSE, 0, va("CU_FAIL(%s)", message), va("base/ufos/uitest/%s.ufo", windowName), "", CU_FALSE);
		} else {
			const char *message = va("%s.%s have an unknown status.", windowName, node->name);
			/*Com_Printf("Warning: %s\n", message);*/
			/*CU_FAIL(message);*/
			CU_assertImplementation(CU_FALSE, 0, va("CU_FAIL(%s)", message), va("base/ufos/uitest/%s.ufo", windowName), "", CU_FALSE);
		}
	}
}

/**
 * @brief Parse and execute a test windows
 */
static void UFO_ExecuteTestWindow (const char* windowName)
{
	int i;

	/* look and feed */
	Com_Printf("\n");

	TEST_ParseScript(va("ufos/uitest/%s.ufo", windowName));

	Cmd_ExecuteString(va("ui_push %s", windowName));

	/* while the execution buffer is not empty */
	for (i = 0; i < 20; i++) {
		Cbuf_Execute();
	}

	UFO_AnalyseTestWindow(windowName);

	/* look and feed */
	Com_Printf("  ---> ");
}

/**
 * @brief test behaviour name
 */
static void testKnownBehaviours (void)
{
	uiBehaviour_t *behaviour;

	behaviour = UI_GetNodeBehaviour("button");
	CU_ASSERT_FATAL(behaviour != NULL);
	CU_ASSERT_STRING_EQUAL(behaviour->name, "button");

	/* first one */
	behaviour = UI_GetNodeBehaviour("");
	CU_ASSERT(behaviour != NULL);

	/* last one */
	behaviour = UI_GetNodeBehaviour("zone");
	CU_ASSERT_FATAL(behaviour != NULL);
	CU_ASSERT_STRING_EQUAL(behaviour->name, "zone");

	/* unknown */
	behaviour = UI_GetNodeBehaviour("dahu");
	CU_ASSERT(behaviour == NULL);
}

/**
 * @brief test actions
 */
static void testActions (void)
{
	UFO_ExecuteTestWindow("test_action");
}

/**
 * @brief test actions
 */
static void testActions2 (void)
{
	UFO_ExecuteTestWindow("test_action2");
}

/**
 * @brief test conditions
 */
static void testConditions (void)
{
	UFO_ExecuteTestWindow("test_condition");
}

/**
 * @brief test function
 */
static void testFunctions (void)
{
	UFO_ExecuteTestWindow("test_function");
}

/**
 * @brief test setters
 */
static void testSetters (void)
{
	UFO_ExecuteTestWindow("test_setter");
}

/**
 * @brief test cvars
 */
static void testCvars(void)
{
	UFO_ExecuteTestWindow("test_cvar");
}

/**
 * @brief test components
 */
static void testComponents (void)
{
	UFO_ExecuteTestWindow("test_component");
}

/**
 * @brief test inherited confunc
 * @todo extend the text with inherited confunc from window (and not only from component)
 */
static void testInheritedConfunc (void)
{
	UFO_ExecuteTestWindow("test_inheritedconfunc");
}

/**
 * @brief test some runtime errors
 * @todo when it is possible, we should check error message
 * but ATM we only check it dont crash
 */
static void testRuntimeError (void)
{
	UFO_ExecuteTestWindow("test_runtimeerror");
}

/**
 * @brief test video nodes
 * but ATM we only check it dont crash
 */
static void testVideo (void)
{
	UFO_ExecuteTestWindow("test_video");
}

/**
 * @brief test cvarlistener nodes
 * but ATM we only check it dont crash
 */
static void testCvarListener (void)
{
	UFO_ExecuteTestWindow("test_cvarlistener");
}

/**
 * @brief test key binding
 * @todo unfortunately we can't use "bindui" or "press" commands in tests.
 */
static void testBinding (void)
{
	//UFO_ExecuteTestWindow("test_keybinding");
}

/**
 * @brief test if we can parse all samples
 */
static void testSamples (void)
{
	TEST_ParseScript("ufos/uisample/*.ufo");
}

int UFO_AddUILevel2Tests (void)
{
	/* add a suite to the registry */
	CU_pSuite UISuite = CU_add_suite("UILevel2Tests", UFO_InitSuiteUILevel2, UFO_CleanSuiteUILevel2);

	if (UISuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(UISuite, testKnownBehaviours) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testActions) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testActions2) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testConditions) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testFunctions) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testSetters) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testCvars) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testComponents) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testInheritedConfunc) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testRuntimeError) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testVideo) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testBinding) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testCvarListener) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(UISuite, testSamples) == NULL)
		return CU_get_error();
	return CUE_SUCCESS;
}
