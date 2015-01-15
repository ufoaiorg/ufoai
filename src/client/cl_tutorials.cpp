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

#include "cl_tutorials.h"
#include "client.h"
#include "ui/ui_main.h"
#include "../shared/parse.h"

typedef struct tutorial_s {
	char name[MAX_VAR];
	char sequence[MAX_VAR];
} tutorial_t;

#define MAX_TUTORIALS 16
static tutorial_t tutorials[MAX_TUTORIALS];
static int numTutorials;

static void TUT_GetTutorials_f (void)
{
	UI_ExecuteConfunc("tutorialsListClear");

	for (int i = 0; i < numTutorials; i++) {
		const tutorial_t* t = &tutorials[i];
		UI_ExecuteConfunc("tutorialsListAdd %i \"%s\"",i, _(t->name));
	}
}

static void TUT_List_f (void)
{
	Com_Printf("Tutorials: %i\n", numTutorials);
	for (int i = 0; i < numTutorials; i++) {
		Com_Printf("tutorial: %s\n", tutorials[i].name);
		Com_Printf("..sequence: %s\n", tutorials[i].sequence);
	}
}

/**
 * @brief click function for text tutoriallist in menu_tutorials.ufo
 */
static void TUT_ListClick_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	const int num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= numTutorials)
		return;

	Cmd_ExecuteString("seq_start %s", tutorials[num].sequence);
}

void TUT_InitStartup (void)
{
	/* tutorial stuff */
	Cmd_AddCommand("listtutorials", TUT_List_f, "Show all tutorials");
	Cmd_AddCommand("gettutorials", TUT_GetTutorials_f);
	Cmd_AddCommand("tutoriallist_click", TUT_ListClick_f);

	numTutorials = 0;
}

static const value_t tutValues[] = {
	{"name", V_TRANSLATION_STRING, offsetof(tutorial_t, name), 0},
	{"sequence", V_STRING, offsetof(tutorial_t, sequence), 0},
	{nullptr, V_NULL, 0, 0}
};

/**
 * @sa CL_ParseClientData
 */
void TUT_ParseTutorials (const char* name, const char** text)
{
	/* parse tutorials */
	if (numTutorials >= MAX_TUTORIALS) {
		Com_Printf("Too many tutorials, '%s' ignored.\n", name);
		return;
	}
	tutorial_t* t = &tutorials[numTutorials++];
	OBJZERO(*t);

	Com_ParseBlock(name, text, t, tutValues, nullptr);
}
