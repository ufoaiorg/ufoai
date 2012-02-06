/**
 * @file ui_main.c
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

#include "ui_main.h"
#include "ui_internal.h"
#include "ui_draw.h"
#include "ui_timer.h"
#include "ui_font.h"
#include "ui_sound.h"
#include "node/ui_node_abstractnode.h"

uiGlobal_t ui_global;

struct memPool_s *ui_dynStringPool;
struct memPool_s *ui_dynPool;
struct memPool_s *ui_sysPool;

#ifdef DEBUG
static cvar_t *ui_debug;
#endif

/**
 * @brief Get the current debug mode (0 mean disabled)
 * @sa UI_DisplayNotice
 */
int UI_DebugMode (void)
{
#ifdef DEBUG
	return ui_debug->integer;
#else
	return 0;
#endif
}

#ifdef DEBUG
/**
 * @brief display info about UI memory
 * @todo it can be nice to have number of nodes per behaviours
 */
static void UI_Memory_f (void)
{
	int i;
	const size_t nodeSize = sizeof(uiNode_t);
	size_t size;
	Com_Printf("Allocation:\n");
	Com_Printf("\t-Window allocation: %i/%i\n", ui_global.numWindows, UI_MAX_WINDOWS);
	Com_Printf("\t-Rendering window stack slot: %i\n", UI_MAX_WINDOWSTACK);
	Com_Printf("\t-Action allocation: %i/%i\n", ui_global.numActions, UI_MAX_ACTIONS);
	Com_Printf("\t-Model allocation: %i/%i\n", ui_global.numModels, UI_MAX_MODELS);
	Com_Printf("\t-Node allocation: %i\n", ui_global.numNodes);

	Com_Printf("Memory:\n");
	Com_Printf("\t-Action structure size: "UFO_SIZE_T" B\n", sizeof(uiAction_t));
	Com_Printf("\t-Model structure size: "UFO_SIZE_T" B\n", sizeof(uiModel_t));
	Com_Printf("\t-Node structure size: "UFO_SIZE_T" B x%d\n", sizeof(uiNode_t), ui_global.numNodes);
	for (i = 0; i < UI_GetNodeBehaviourCount(); i++) {
		uiBehaviour_t *b = UI_GetNodeBehaviourByIndex(i);
		Com_Printf("\t -Behaviour %20s structure size: "UFO_SIZE_T" (+"UFO_SIZE_T" B) x%4u\n", b->name, sizeof(uiNode_t) + b->extraDataSize, b->extraDataSize, b->count);
	}

	size = 0;
	for (i = 0; i < UI_GetNodeBehaviourCount(); i++) {
		uiBehaviour_t *b = UI_GetNodeBehaviourByIndex(i);
		size += nodeSize * b->count + b->extraDataSize * b->count;
	}
	Com_Printf("Global memory:\n");
	Com_Printf("\t-System pool: %ui B\n", _Mem_PoolSize(ui_sysPool));
	Com_Printf("\t -AData allocation: "UFO_SIZE_T"/%i B\n", (ptrdiff_t)(ui_global.curadata - ui_global.adata), ui_global.adataize);
	Com_Printf("\t -AData used by nodes: "UFO_SIZE_T" B\n", size);
	Com_Printf("\t-Dynamic node/data pool: %ui B\n", _Mem_PoolSize(ui_dynPool));
	Com_Printf("\t-Dynamic strings pool: %ui B\n", _Mem_PoolSize(ui_dynStringPool));

	size = _Mem_PoolSize(ui_sysPool) + _Mem_PoolSize(ui_dynPool) + _Mem_PoolSize(ui_dynStringPool);
	Com_Printf("\t-Full size: "UFO_SIZE_T" B\n", size);
}
#endif

#define MAX_CONFUNC_SIZE 512
/**
 * @brief Executes confunc - just to identify those confuncs in the code - in
 * this frame.
 * @param[in] fmt Construct string it will execute (command and param)
 */
void UI_ExecuteConfunc (const char *fmt, ...)
{
	va_list ap;
	char confunc[MAX_CONFUNC_SIZE];

	va_start(ap, fmt);
	Q_vsnprintf(confunc, sizeof(confunc), fmt, ap);
	Cmd_ExecuteString(confunc);
	va_end(ap);
}

/**
 * Allocate memory from hunk managed by the UI code
 * This memory is allocated one time, and never released.
 * @param size Quantity of memory expected
 * @param align Alignement of the expected memory
 * @param reset If true initilize the memory with 0
 * @return available memory, else NULL
 */
void* UI_AllocHunkMemory (size_t size, int align, qboolean reset)
{
	byte *memory = ALIGN_PTR(ui_global.curadata, align);
	if (memory + size > ui_global.adata + ui_global.adataize)
		return NULL;
	if (reset)
		memset(memory, 0, size);
	ui_global.curadata = memory + size;
	return memory;
}

/**
 * Reinit input and font
 */
void UI_Reinit (void)
{
	UI_InitFonts();
	UI_ReleaseInput();
	UI_InvalidateMouse();
}

/**
 * @brief Reset and free the UI data hunk
 * @note Even called in case of an error when CL_Shutdown was called - maybe even
 * before CL_InitLocal (and thus UI_Init) was called
 * @sa CL_Shutdown
 * @sa UI_Init
 */
void UI_Shutdown (void)
{
	int i;
	const uiBehaviour_t *confunc;

	/* MN is not yet initialized */
	if (ui_global.adata == NULL)
		return;

	confunc = UI_GetNodeBehaviour("confunc");

	/* remove all confunc commands */
	for (i = 0; i < ui_global.numWindows; i++) {
		uiNode_t *node = ui_global.windows[i];
		while (node) {

			/* remove the command */
			if (node->behaviour == confunc) {
				/* many nodes to one command is allowed */
				if (Cmd_Exists(node->name))
					Cmd_RemoveCommand(node->name);
			}

			/* recursive next */
			if (node->firstChild != NULL)
				node = node->firstChild;
			else {
				while (node) {
					if (node->next != NULL) {
						node = node->next;
						break;
					}
					node = node->parent;
				}
			}
		}
	}

	if (ui_global.adataize)
		Mem_Free(ui_global.adata);
	ui_global.adata = NULL;
	ui_global.adataize = 0;

	/* release pools */
	Mem_FreePool(ui_sysPool);
	Mem_FreePool(ui_dynStringPool);
	Mem_FreePool(ui_dynPool);
	ui_sysPool = NULL;
	ui_dynStringPool = NULL;
	ui_dynPool = NULL;
}

/**
 * @brief Finish initialization after everything was loaded
 * @note private function
 */
void UI_FinishInit (void)
{
	static qboolean initialized = qfalse;
	if (initialized)
		return;
	initialized = qtrue;

	UI_FinishWindowsInit();
}

void UI_Init (void)
{
	cvar_t *ui_hunkSize = Cvar_Get("ui_hunksize", "3", 0, "UI memory hunk size in megabytes");

#ifdef DEBUG
	ui_debug = Cvar_Get("debug_ui", "0", CVAR_DEVELOPER, "Prints node names for debugging purposes - valid values are 1 and 2");
#endif

	/* reset global UI structures */
	OBJZERO(ui_global);

	ui_sounds = Cvar_Get("ui_sounds", "1", CVAR_ARCHIVE, "Activates UI sounds");

#ifdef DEBUG
	Cmd_AddCommand("debug_uimemory", UI_Memory_f, "Display info about UI memory allocation");
#endif

	ui_sysPool = Mem_CreatePool("Client: UI");
	ui_dynStringPool = Mem_CreatePool("Client: Dynamic string for UI");
	ui_dynPool = Mem_CreatePool("Client: Dynamic memory for UI");

	Com_Printf("Allocate %i megabytes for the ui hunk\n", ui_hunkSize->integer);
	ui_global.adataize = ui_hunkSize->integer * 1024 * 1024;
	ui_global.adata = (byte*)Mem_PoolAlloc(ui_global.adataize, ui_sysPool, 0);
	ui_global.curadata = ui_global.adata;

	UI_InitData();
	UI_InitNodes();
	UI_InitWindows();
	UI_InitDraw();
	UI_InitActions();
}
