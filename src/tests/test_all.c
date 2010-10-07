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
#include "CUnit/TestDB.h"

#include "test_shared.h"

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
	qboolean console;
	qboolean automated;
	qboolean log;
} config_t;

static const char* resultPrefix = "ufoai";

static config_t config;

static FILE *logFile;

static void Test_List (void)
{
	CU_pTestRegistry registry;
	CU_pSuite suite;

	printf("Suites:\n");
	registry = CU_get_registry();
	suite = registry->pSuite;
	while (suite) {
		printf("* %s\n", suite->pName);
		suite = suite->pNext;
	}
}

static void Test_RemovePSuite (CU_pSuite suite)
{
	if (suite->pNext)
		suite->pNext->pPrev = suite->pPrev;
	if (suite->pPrev)
		suite->pPrev->pNext = suite->pNext;
	else
		CU_get_registry()->pSuite = suite->pNext;
}

static int Test_RemoveSuite (const char *name)
{
	CU_pTestRegistry registry;
	CU_pSuite suite;

	registry = CU_get_registry();
	suite = registry->pSuite;
	while (suite) {
		if (!strcmp(suite->pName, name)) {
			Test_RemovePSuite(suite);
			return 0;
		}
		suite = suite->pNext;
	}
	return 1;
}

static int Test_RemoveAllSuitesExcept (const char *name)
{
	int found = 0;
	CU_pTestRegistry registry;
	CU_pSuite suite;

	registry = CU_get_registry();
	suite = registry->pSuite;
	while (suite) {
		if (!strcmp(suite->pName, name)) {
			suite = suite->pNext;
			found = 1;
		} else {
			CU_pSuite remove = suite;
			suite = suite->pNext;
			Test_RemovePSuite(remove);
		}
	}
	if (found)
		return 0;
	return 1;
}

static void Test_Parameters (const int argc, const char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--console")) {
			config.console = qtrue;
		} else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--automated")) {
			config.automated = qtrue;
		} else if (!strncmp(argv[i], "--disable-", 10)) {
			const char *name = argv[i] + 10;
			if (Test_RemoveSuite(name) != 0) {
				printf("Suite \"%s\" unknown\n", name);
				printf("Use \"%s -l\" to show the list of suites\n", argv[0]);
				exit(2);
			}
		} else if (!strncmp(argv[i], "--only-", 7)) {
			const char *name = argv[i] + 7;
			if (Test_RemoveAllSuitesExcept(name) != 0) {
				printf("Suite \"%s\" unknown\n", name);
				printf("Use \"%s -l\" to show the list of suites\n", argv[0]);
				exit(2);
			}
		} else if (!strncmp(argv[i], "--output-prefix=", 16)) {
			resultPrefix = argv[i] + 16;
		} else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--list")) {
			Test_List();
			exit(0);
		} else if (!strcmp(argv[i], "--log")) {
			config.log = qtrue;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			printf("Usage:\n");
			printf("-h  --help                 | show this help screen\n");
			printf("-c  --console              | run tests in console mode\n");
			printf("-a  --automated            | run tests in automated mode (create xml file)\n");
			printf("-l  --list                 | list suite name available\n");
			printf("    --log                  | log ufo output to file\n");
			printf("    --output-prefix=PREFIX | set a prefix for the xml result\n");
			printf("                           | default value is \"ufoai\"\n");
			printf("    --disable-SUITE        | Disable a suite by name\n");
			printf("                           | SUITE=suite name\n");
			printf("    --only-SUITE           | Enable an only one suite by name\n");
			printf("                           | SUITE=suite name\n");
			exit(0);
		} else {
			printf("Param \"%s\" unknown\n", argv[i]);
			printf("Use \"%s -h\" to show the help screen\n", argv[0]);
			exit(2);
		}
	}
}

static void TEST_vPrintfLog (const char *fmt, va_list ap)
{
	if (logFile) {
		char msg[2048];

		Q_vsnprintf(msg, sizeof(msg), fmt, ap);

		fprintf(logFile, "%s", msg);
		fflush(logFile);
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
	int failures;

	Sys_InitSignals();

	/* initialize the CUnit test registry */
	if (CU_initialize_registry() != CUE_SUCCESS)
		return CU_get_error();

	for (i = 0; i < NUMBER_OF_TESTS - 1; i++) {
		if (testSuites[i]() != CUE_SUCCESS)
			return fatalError();
	}

	Test_Parameters(argc, argv);

	if (config.log) {
		const char *path = "testall.log";
		logFile = fopen(path, "wb");
		Qcommon_SetPrintFunction(TEST_vPrintfLog);
	} else
		Qcommon_SetPrintFunction(TEST_vPrintf);

	if (config.console)
		/* Run all tests using the console interface */
		CU_console_run_tests();
	else if (config.automated) {
		CU_basic_set_mode(CU_BRM_VERBOSE);
		CU_set_output_filename(resultPrefix);
		CU_automated_run_tests();
	} else {
		/* Run all tests using the CUnit Basic interface */
		CU_basic_set_mode(CU_BRM_VERBOSE);
		CU_basic_run_tests();
	}

	failures = CU_get_number_of_failures();
	CU_cleanup_registry();

	if (logFile)
		fclose(logFile);

	/* there is a problem on the framework (use git bisect value for IDK) */
	if (CU_get_error() != 0)
		return 125;

	return failures != 0;
}
