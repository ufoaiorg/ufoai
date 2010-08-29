/**
 * @file test_all.c
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

#include "stdlib.h"
#include "stdio.h"
#include "CUnit/Basic.h"
#include "CUnit/Automated.h"
#include "CUnit/Console.h"

typedef int (*testSuite_t) (void);

/* include the tests here */
#include "test_generic.h"
#include "test_ui.h"
#include "test_routing.h"
#include "test_inventory.h"
#include "test_campaign.h"
#include "test_rma.h"
#include "test_parser.h"

static const testSuite_t testSuites[] = {
	UFO_AddGenericTests,
	UFO_AddParserTests,
	UFO_AddUITests,
	UFO_AddCampaignTests,
	UFO_AddRoutingTests,
	UFO_AddInventoryTests,
	UFO_AddRandomMapAssemblyTests,
	NULL
};
#define NUMBER_OF_TESTS (sizeof(testSuites) / sizeof(*(testSuites)))

static inline int fatalError (void)
{
	CU_cleanup_registry();
	return CU_get_error();
}

void Sys_Init(void);
void Sys_Init (void)
{
}

typedef struct config_s {
	int console;
	int automated;
	int disableRMA;
} config_t;

static config_t config;

static void Test_Parameters (const int argc, const char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--console")) {
			config.console = 1;
		} else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--automated")) {
			config.automated = 1;
		} else if (!strcmp(argv[i], "--disable-rma")) {
			config.disableRMA = 1;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			printf("Usage:\n");
			printf("-h  --help        | show this help screen\n");
			printf("-c  --console     | run tests in console mode\n");
			printf("-a  --automated   | run tests in automated mode (create xml file)\n");
			printf("    --disable-rma | exclude RMA test from the test suite\n");
			exit(0);
		} else {
			printf("Param \"%s\" unknown\n", argv[i]);
			printf("Use \"%s -h\" to show the help screen\n", argv[0]);
			exit(0);
		}
	}
}

/**
 * Setting up and running all tests.
 * Returns a CUE_SUCCESS on successful running, another
 * CUnit error code on failure.
 */
int main (int argc, const char **argv)
{
	int i;

	/* initialize the CUnit test registry */
	if (CU_initialize_registry() != CUE_SUCCESS)
		return CU_get_error();

	Test_Parameters(argc, argv);

	for (i = 0; i < NUMBER_OF_TESTS - 1; i++) {
		/** @todo find a more generic way if it is possible (framework should allow it, by suite name) */
		if (config.disableRMA != 0 && testSuites[i] == UFO_AddRandomMapAssemblyTests)
			continue;
		if (testSuites[i]() != CUE_SUCCESS)
			return fatalError();
	}

	/* Run all tests using the CUnit Basic interface */
	CU_basic_set_mode(CU_BRM_VERBOSE);

	if (config.console)
		/* Run all tests using the console interface */
		CU_console_run_tests();
	else if (config.automated) {
		CU_set_output_filename("ufoai");
		CU_automated_run_tests();
	} else
		CU_basic_run_tests();

	CU_cleanup_registry();
	return CU_get_error();
}
