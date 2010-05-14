/**
 * @file m_main.c
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

#include "m_main.h"
#include "m_internal.h"
#include "m_draw.h"
#include "m_timer.h"

#include "../client.h"

/** @todo client code should manage itself this vars */
cvar_t *mn_sequence;
cvar_t *mn_hud;

menuGlobal_t mn;
struct memPool_s *mn_dynStringPool;
struct memPool_s *mn_dynPool;
struct memPool_s *mn_sysPool;

#ifdef DEBUG
static cvar_t *mn_debug;
#endif

/**
 * @sa MN_DisplayNotice
 * @todo move it into a better file
 */
static void MN_CheckCvar (const cvar_t *cvar)
{
	if (cvar->modified) {
		if (cvar->flags & CVAR_R_CONTEXT) {
			MN_DisplayNotice(_("This change requires a restart"), 2000, NULL);
		} else if (cvar->flags & CVAR_R_IMAGES) {
			MN_DisplayNotice(_("This change might require a restart"), 2000, NULL);
		}
	}
}

/**
 * @sa MN_DisplayNotice
 * @todo move it into a better file
 */
int MN_DebugMode (void)
{
#ifdef DEBUG
	return mn_debug->integer;
#else
	return 0;
#endif
}

/**
 * @param[in] name Name of the cvar
 * @param[in] str Might be NULL if you want to set a float value
 * @param[in] value Float value to set
 * @todo move it into a better file
 */
void MN_SetCvar (const char *name, const char *str, float value)
{
	const cvar_t *cvar;
	cvar = Cvar_FindVar(name);
	if (!cvar) {
		Com_Printf("Could not find cvar '%s'\n", name);
		return;
	}
	if (str)
		Cvar_Set(cvar->name, str);
	else
		Cvar_SetValue(cvar->name, value);
	MN_CheckCvar(cvar);
}

/**
 * @brief Adds a given value to the numeric representation of a cvar. Also
 * performs min and max check for that value.
 * @sa MN_ModifyWrap_f
 */
static void MN_Modify_f (void)
{
	float value;

	if (Cmd_Argc() < 5)
		Com_Printf("Usage: %s <name> <amount> <min> <max>\n", Cmd_Argv(0));

	value = Cvar_GetValue(Cmd_Argv(1));
	value += atof(Cmd_Argv(2));
	if (value < atof(Cmd_Argv(3)))
		value = atof(Cmd_Argv(3));
	else if (value > atof(Cmd_Argv(4)))
		value = atof(Cmd_Argv(4));

	Cvar_SetValue(Cmd_Argv(1), value);
}

/**
 * @brief Adds a given value to the numeric representation of a cvar. Also
 * performs min and max check for that value. If there would be an overflow
 * we use the min value - and if there would be an underrun, we use the max
 * value.
 * @sa MN_Modify_f
 */
static void MN_ModifyWrap_f (void)
{
	float value;

	if (Cmd_Argc() < 5) {
		Com_Printf("Usage: %s <name> <amount> <min> <max>\n", Cmd_Argv(0));
		return;
	}

	value = Cvar_GetValue(Cmd_Argv(1));
	value += atof(Cmd_Argv(2));
	if (value < atof(Cmd_Argv(3)))
		value = atof(Cmd_Argv(4));
	else if (value > atof(Cmd_Argv(4)))
		value = atof(Cmd_Argv(3));

	Cvar_SetValue(Cmd_Argv(1), value);
}

/**
 * @brief Shows the corresponding strings in menu
 * Example: Optionsmenu - fullscreen: yes
 */
static void MN_Translate_f (void)
{
	qboolean next;
	const char *current, *list;
	char *orig, *trans;
	char original[MAX_VAR], translation[MAX_VAR];

	if (Cmd_Argc() < 4) {
		Com_Printf("Usage: %s <source> <dest> <list>\n", Cmd_Argv(0));
		return;
	}

	current = Cvar_GetString(Cmd_Argv(1));
	list = Cmd_Argv(3);
	next = qfalse;

	while (list[0] != '\0') {
		orig = original;
		while (list[0] != '\0' && list[0] != ':') {
			/** @todo overflow check */
			*orig++ = *list++;
		}
		*orig = '\0';
		list++;

		trans = translation;
		while (list[0] != '\0' && list[0] != ',') {
			/** @todo overflow check */
			*trans++ = *list++;
		}
		*trans = '\0';
		list++;

		if (!strcmp(current, original)) {
			Cvar_Set(Cmd_Argv(2), _(translation));
			return;
		}
	}

	if (current[0] != '\0') {
		/* nothing found, copy value */
		Cvar_Set(Cmd_Argv(2), _(current));
	} else {
		Cvar_Set(Cmd_Argv(2), "");
	}
}

#ifdef DEBUG
/**
 * @brief display info about menu memory
 * @todo it can be nice to have number of nodes per behaviours
 */
static void MN_Memory_f (void)
{
	int i;
	const size_t nodeSize = sizeof(menuNode_t) - MEMBER_SIZEOF(menuNode_t, u);
	size_t size;
	Com_Printf("^BAllocation:\n");
	Com_Printf("\t-Option allocation: %i/%i\n", mn.numOptions, MAX_MENUOPTIONS);
	Com_Printf("\t-Node allocation: %i/%i\n", mn.numNodes, MAX_MENUNODES);
	Com_Printf("\t-Window allocation: %i/%i\n", mn.numWindows, MAX_WINDOWS);
	Com_Printf("\t-Rendering menu stack slot: %i\n", MAX_MENUSTACK);
	Com_Printf("\t-Action allocation: %i/%i\n", mn.numActions, MAX_MENUACTIONS);
	Com_Printf("\t-Model allocation: %i/%i\n", mn.numMenuModels, MAX_MENUMODELS);
	Com_Printf("\t-Exclude rect allocation: %i/%i\n", mn.numExcludeRect, MAX_EXLUDERECTS);
	Com_Printf("\t-AData allocation: "UFO_SIZE_T"/%i B\n", (ptrdiff_t)(mn.curadata - mn.adata), mn.adataize);
	Com_Printf("\t-Node per behaviour: ");
	for (i = 0; i < MN_GetNodeBehaviourCount(); i++) {
		nodeBehaviour_t *b = MN_GetNodeBehaviourByIndex(i);
		if (b->count == 0)
			continue;
		Com_Printf("%s (%d), ", b->name, b->count);
	}
	Com_Printf("\n");

	Com_Printf("\tMemory:\n");
	Com_Printf("\t-Option structure size: "UFO_SIZE_T" B\n", sizeof(menuOption_t));
	Com_Printf("\t-Action structure size: "UFO_SIZE_T" B\n", sizeof(menuAction_t));
	Com_Printf("\t-Model structure size: "UFO_SIZE_T" B\n", sizeof(menuModel_t));
	Com_Printf("\t-Node structure size: "UFO_SIZE_T" B\n", sizeof(menuNode_t));
	Com_Printf("\t-Node structure size (without extradata): "UFO_SIZE_T" B\n", nodeSize);
	Com_Printf("\t-Node extradata size: "UFO_SIZE_T" B\n", sizeof(menuNode_t) - nodeSize);
	Com_Printf("\t-Node extradata per behaviour: ");
	for (i = 0; i < MN_GetNodeBehaviourCount(); i++) {
		nodeBehaviour_t *b = MN_GetNodeBehaviourByIndex(i);
		if (b->extraDataSize == 0)
			continue;
		Com_Printf("%s ("UFO_SIZE_T" B), ", b->name, b->extraDataSize);
	}
	Com_Printf("\n");

	size = 0;
	for (i = 0; i < MN_GetNodeBehaviourCount(); i++) {
		nodeBehaviour_t *b = MN_GetNodeBehaviourByIndex(i);
		size += nodeSize * b->count + b->extraDataSize * b->count;
	}
	Com_Printf("\tGlobal memory:");
	Com_Printf("\t-Node size used: "UFO_SIZE_T" B\n", sizeof(menuNode_t) * mn.numNodes);
	Com_Printf("\t-Node size really used: "UFO_SIZE_T" B\n", size);
	Com_Printf("\t-AData size: %i B\n", mn.adataize);
	Com_Printf("\t-Full size: "UFO_SIZE_T" B\n", sizeof(menuGlobal_t) + mn.adataize);
}
#endif

#define MAX_CONFUNC_SIZE 512
/**
 * @brief Executes confunc - just to identify those confuncs in the code - in
 * this frame.
 * @param[in] fmt Construct string it will execute (command and param)
 */
void MN_ExecuteConfunc (const char *fmt, ...)
{
	va_list ap;
	char confunc[MAX_CONFUNC_SIZE];

	va_start(ap, fmt);
	Q_vsnprintf(confunc, sizeof(confunc), fmt, ap);
	Cmd_ExecuteString(confunc);
	va_end(ap);
}

/**
 * @brief Reset and free the menu data hunk
 * @note Even called in case of an error when CL_Shutdown was called - maybe even
 * before CL_InitLocal (and thus MN_Init) was called
 * @sa CL_Shutdown
 * @sa MN_Init
 */
void MN_Shutdown (void)
{
	if (mn.adataize)
		Mem_Free(mn.adata);
	mn.adata = NULL;
	mn.adataize = 0;
}

#define MENU_HUNK_SIZE 0x40000

void MN_Init (void)
{
#ifdef DEBUG
	MN_UnittestTimer();
#endif

#ifdef DEBUG
	mn_debug = Cvar_Get("debug_menu", "0", CVAR_DEVELOPER, "Prints node names for debugging purposes - valid values are 1 and 2");
#endif

	/* reset menu structures */
	memset(&mn, 0, sizeof(mn));

	/* add cvars */
	mn_sequence = Cvar_Get("mn_sequence", "sequence", 0, "This is the sequence menu to render the sequence in");
	/** @todo should be a client Cvar, not a menu */
	mn_hud = Cvar_Get("mn_hud", "hud", CVAR_ARCHIVE | CVAR_LATCH, "This is the current selected hud");

	/* add menu commands */
	Cmd_AddCommand("mn_modify", MN_Modify_f, NULL);
	Cmd_AddCommand("mn_modifywrap", MN_ModifyWrap_f, NULL);
	Cmd_AddCommand("mn_translate", MN_Translate_f, NULL);
#ifdef DEBUG
	Cmd_AddCommand("debug_mnmemory", MN_Memory_f, "Display info about menu memory allocation");
#endif

	mn_sysPool = Mem_CreatePool("Client: UI");
	mn_dynStringPool = Mem_CreatePool("Client: Dynamic string for UI");
	mn_dynPool = Mem_CreatePool("Client: Dynamic memory for UI");

	/* 256kb */
	mn.adataize = MENU_HUNK_SIZE;
	mn.adata = (byte*)Mem_PoolAlloc(mn.adataize, mn_sysPool, 0);
	mn.curadata = mn.adata;

	MN_InitData();
	MN_InitNodes();
	MN_InitWindows();
	MN_InitDraw();
	MN_InitActions();
}
