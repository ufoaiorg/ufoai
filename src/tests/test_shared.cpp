/**
 * @file
 * @brief Shared code for unittests
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
#include "../client/cl_renderer.h"
#include "../client/battlescape/cl_particle.h"
#include "../client/ui/ui_main.h"
#include <stdio.h>
#include <stdarg.h>

void TEST_Shutdown (void)
{
	PTL_InitStartup();
	R_ShutdownImages();
	SV_Shutdown("test shutdown", false);
	FS_Shutdown();
	UI_Shutdown();
	Cmd_Shutdown();
	developer = nullptr;
	Cvar_Shutdown();
	Mem_Shutdown();
	Com_Shutdown();
	Cbuf_Shutdown();
	NET_Shutdown();

	com_aliasSysPool = nullptr;
	com_cmdSysPool = nullptr;
	com_cmodelSysPool = nullptr;
	com_cvarSysPool = nullptr;
	com_fileSysPool = nullptr;
	com_genericPool = nullptr;
}

void TEST_vPrintfSilent (const char* fmt, va_list argptr)
{
}

void TEST_vPrintf (const char* fmt, va_list argptr)
{
	char text[1024];
	Q_vsnprintf(text, sizeof(text), fmt, argptr);

	fprintf(stderr, "%s", text);

	fflush(stderr);
}

void TEST_Init (void)
{
	try {
		com_aliasSysPool  = Mem_CreatePool("Common: Alias system");
		com_cmdSysPool    = Mem_CreatePool("Common: Command system");
		com_cmodelSysPool = Mem_CreatePool("Common: Collision model");
		com_cvarSysPool   = Mem_CreatePool("Common: Cvar system");
		com_fileSysPool   = Mem_CreatePool("Common: File system");
		com_genericPool   = Mem_CreatePool("Generic");

		Mem_Init();
		Cbuf_Init();
		Cmd_Init();
		Cvar_Init();
		FS_InitFilesystem(true);
		FS_AddGameDirectory("./unittest", false);
		FS_AddHomeAsGameDirectory("unittest", true);
		Swap_Init();
		SV_Init();
		NET_Init();

		FS_ExecAutoexec();

		OBJZERO(csi);
	} catch (comDrop_t const&) {
		Sys_Error("Error during initialization");
	}

	http_timeout = Cvar_Get("noname");
	http_proxy = Cvar_Get("noname");
	hwclass = Cvar_Get("hwclass", "5");
}


#define PROPERTY_HASH_SIZE	32

#define	MAX_PROPERTY_NAME	32

typedef struct test_property_s {
	char name[MAX_PROPERTY_NAME];
	const char* value;
	struct test_property_s* hash_next;
	struct test_property_s* next;
} test_property_t;

static test_property_t* test_property;
static test_property_t* test_property_hash[PROPERTY_HASH_SIZE];

/**
 * Register a property name-value to a global structure for all tests
 * @param name Name of the property
 * @param value Value of the property. Only the pointer of the value is used. Use it ONLY with stable memory.
 */
void TEST_RegisterProperty (const char* name, const char* value)
{
	unsigned int hash;
	test_property_t* element;

	/* if the alias already exists, reuse it */
	hash = Com_HashKey(name, PROPERTY_HASH_SIZE);
	for (element = test_property_hash[hash]; element; element = element->hash_next) {
		if (Q_streq(name, element->name)) {
			break;
		}
	}

	if (!element) {
		element = (test_property_t*) malloc(sizeof(*element));
		Q_strncpyz(element->name, name, sizeof(element->name));
		/** TODO maybe copy the value instead of copying the pointer of the value */
		element->value = value;
		element->next = test_property;
		element->hash_next = test_property_hash[hash];
		test_property_hash[hash] = element;
		test_property = element;
		Com_Printf("Register test property \"%s\" = \"%s\"\n", name, value);
	}
}

/**
 * Get a property name from global test structure
 * @param name Name of the property
 * @return A property element, else nullptr if property not found.
 */
static const test_property_t* TEST_GetProperty (const char* name)
{
	unsigned int hash;
	const test_property_t* element;

	hash = Com_HashKey(name, PROPERTY_HASH_SIZE);
	for (element = test_property_hash[hash]; element; element = element->hash_next) {
		if (!Q_strcasecmp(name, element->name)) {
			return element;
		}
	}
	return nullptr;
}

/**
 * Test if a property from global test structure exists
 * @param name Name of the property
 * @return True if the property exists
 */
bool TEST_ExistsProperty (const char* name)
{
	return TEST_GetProperty(name) != nullptr;
}

/**
 * Get a property value from global test structure
 * @param name Name of the property
 * @return A property value, else nullptr if property not found.
 */
const char* TEST_GetStringProperty (const char* name)
{
	const test_property_t* element = TEST_GetProperty(name);
	if (element == nullptr) {
		Com_Printf("WARNING: Test property \"%s\" not found. nullptr returned.\n", name);
		return nullptr;
	}
	return element->value;
}

/**
 * Get a property value from global test structure
 * @param name Name of the property
 * @return A property value, else 0 if property not found.
 */
int TEST_GetIntProperty (const char* name)
{
	const test_property_t* element = TEST_GetProperty(name);
	if (element == nullptr) {
		Com_Printf("WARNING: Test property \"%s\" not found. 0 returned.\n", name);
		return 0;
	}
	return atoi(element->value);
}

/**
 * Get a property value from global test structure
 * @param name Name of the property
 * @return A property value, else 0 if property not found.
 */
long TEST_GetLongProperty (const char* name)
{
	const test_property_t* element = TEST_GetProperty(name);
	if (element == nullptr) {
		Com_Printf("WARNING: Test property \"%s\" not found. 0 returned.\n", name);
		return 0;
	}
	return atol(element->value);
}
