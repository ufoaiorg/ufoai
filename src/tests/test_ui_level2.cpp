/**
 * @file
 * @brief Test cases for code below client/ui/ when the global UI "engine" is started
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
#include "../client/cl_video.h" /* vid_imagePool */
#include "../client/ui/ui_internal.h"
#include "../client/renderer/r_state.h" /* r_state */

class UILevel2Test: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
		UI_Init();

		vid_imagePool = Mem_CreatePool("Vid: Image system");

		r_state.active_texunit = &texunit_diffuse;
	}

	static void TearDownTestCase() {
		UI_Shutdown();
		TEST_Shutdown();
	}
};

/** @todo move it somewhere */
static void TEST_ParseScript (const char* scriptName)
{
	const char* type, *name, *text;
	int nbWrongScripts = 0;

	/* parse ui node script */
	Com_Printf("Load \"%s\": %i script file(s)\n", scriptName, FS_BuildFileList(scriptName));
	FS_NextScriptHeader(nullptr, nullptr, nullptr);
	text = nullptr;
	while ((type = FS_NextScriptHeader(scriptName, &name, &text)) != nullptr) {
		try {
			CL_ParseClientData(type, name, &text);
		} catch (const comDrop_t& e) {
			ASSERT_TRUE(false) << "Failed to parse needed scripts for type: " << type << " and id " << name;
			nbWrongScripts++;
		}
	}
	ASSERT_TRUE(nbWrongScripts == 0);
}

/**
 * @brief after execution of a unittest window, it analyse color of
 * normalized indicator, and create asserts. Both Fail/Pass asserts is
 * useful to count number.
 */
static void UFO_AnalyseTestWindow (const char* windowName)
{
	const uiBehaviour_t* stringBehaviour = UI_GetNodeBehaviour("string");
	const uiNode_t* node;
	const uiNode_t* window = UI_GetWindow(windowName);
	ASSERT_TRUE(window != nullptr);
	ASSERT_TRUE(stringBehaviour != nullptr);

	for (node = window->firstChild; node != nullptr; node = node->next) {
		bool isGreen;
		bool isRed;
		/* skip non "string" nodes */
		if (node->behaviour != stringBehaviour)
			continue;
		if (node->invis)
			continue;
		/* skip nodes without "test" prefix */
		if (!Q_strstart(node->name, "test"))
			continue;

		isGreen = node->color[0] < 0.1 && node->color[1] > 0.9 && node->color[2] < 0.1;
		isRed = node->color[0] > 0.9 && node->color[1] < 0.1 && node->color[2] < 0.1;

		if (isGreen) {
			/** @note Useful to count number of tests */
			ASSERT_TRUE(true);
		} else if (isRed) {
			const char* message = va("%s.%s failed.", windowName, node->name);
			ASSERT_TRUE(false) << message << " base/ufos/uitest/" << windowName << ".ufo";
		} else {
			const char* message = va("%s.%s have an unknown status.", windowName, node->name);
			ASSERT_TRUE(false) << message << " base/ufos/uitest/" << windowName << ".ufo";
		}
	}
}

/**
 * @brief Parse and execute a test windows
 */
static void UFO_ExecuteTestWindow (const char* windowName)
{
	/* look and feed */
	Com_Printf("\n");

	TEST_ParseScript(va("ufos/uitest/%s.ufo", windowName));

	Cmd_ExecuteString("ui_push %s", windowName);

	/* while the execution buffer is not empty */
	for (int i = 0; i < 20; i++) {
		Cbuf_Execute();
	}

	UFO_AnalyseTestWindow(windowName);

	/* look and feed */
	Com_Printf("  ---> ");
}

/**
 * @brief test behaviour name
 */
TEST_F(UILevel2Test, KnownBehaviours)
{
	uiBehaviour_t* behaviour;

	behaviour = UI_GetNodeBehaviour("button");
	ASSERT_TRUE(behaviour != nullptr);
	ASSERT_STREQ(behaviour->name, "button");

	/* first one */
	behaviour = UI_GetNodeBehaviour("");
	ASSERT_TRUE(behaviour != nullptr);

	/* last one */
	behaviour = UI_GetNodeBehaviour("zone");
	ASSERT_TRUE(behaviour != nullptr);
	ASSERT_STREQ(behaviour->name, "zone");

	/* unknown */
	behaviour = UI_GetNodeBehaviour("dahu");
	ASSERT_TRUE(behaviour == nullptr);
}

/**
 * @brief test actions
 */
TEST_F(UILevel2Test, Actions)
{
	UFO_ExecuteTestWindow("test_action");
}

/**
 * @brief test actions
 */
TEST_F(UILevel2Test, Actions2)
{
	UFO_ExecuteTestWindow("test_action2");
}

/**
 * @brief test conditions
 */
TEST_F(UILevel2Test, Conditions)
{
	UFO_ExecuteTestWindow("test_condition");
}

/**
 * @brief test function
 */
TEST_F(UILevel2Test, Functions)
{
	UFO_ExecuteTestWindow("test_function");
}

/**
 * @brief test setters
 */
TEST_F(UILevel2Test, Setters)
{
	UFO_ExecuteTestWindow("test_setter");
}

/**
 * @brief test cvars
 */
TEST_F(UILevel2Test, Cvars)
{
	UFO_ExecuteTestWindow("test_cvar");
}

/**
 * @brief test components
 */
TEST_F(UILevel2Test, Components)
{
	UFO_ExecuteTestWindow("test_component");
}

/**
 * @brief test inherited confunc
 * @todo extend the text with inherited confunc from window (and not only from component)
 */
TEST_F(UILevel2Test, InheritedConfunc)
{
	UFO_ExecuteTestWindow("test_inheritedconfunc");
}

/**
 * @brief test some runtime errors
 * @todo when it is possible, we should check error message
 * but ATM we only check it dont crash
 */
TEST_F(UILevel2Test, RuntimeError)
{
	UFO_ExecuteTestWindow("test_runtimeerror");
}

/**
 * @brief test video nodes
 * but ATM we only check it dont crash
 */
TEST_F(UILevel2Test, Video)
{
	UFO_ExecuteTestWindow("test_video");
}

/**
 * @brief test cvarlistener nodes
 * but ATM we only check it dont crash
 */
TEST_F(UILevel2Test, CvarListener)
{
	UFO_ExecuteTestWindow("test_cvarlistener");
}

/**
 * @brief test key binding
 * @todo unfortunately we can't use "bindui" or "press" commands in tests.
 */
TEST_F(UILevel2Test, Binding)
{
	//UFO_ExecuteTestWindow("test_keybinding");
}

/**
 * @brief test if we can parse all samples
 */
TEST_F(UILevel2Test, Samples)
{
	TEST_ParseScript("ufos/uisample/*.ufo");
}
