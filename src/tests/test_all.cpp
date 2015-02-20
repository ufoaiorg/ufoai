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

#include "stdlib.h"
#include "stdio.h"
#include <gtest/gtest.h>
#include <SDL_main.h>

#include "test_shared.h"

void Sys_Init(void);
void Sys_Init (void)
{
}

typedef struct config_s {
	bool log;
	bool verbose;
} config_t;

static const char* resultPrefix = "ufoai";
static config_t config;
static FILE* logFile;

static void Test_Parameters (const int argc, char** argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (char const* const arg = Q_strstart(argv[i], "-D")) {
			if (char const* const value = strchr(arg, '=')) {
				char name[32];
				size_t const size = value - arg + 1;
				if (size >= sizeof(name)) {
					fprintf(stderr, "Error: Argument \"%s\" use a property name too much big.\n", argv[i]);
					exit(2);
				}
				Q_strncpyz(name, arg, size);
				TEST_RegisterProperty(name, value + 1);
			} else {
				fprintf(stderr, "Warning: \"%s\" do not value. Command line argument ignored.\n", argv[i]);
			}
		} else if (char const* const prefix = Q_strstart(argv[i], "--output-prefix=")) {
			resultPrefix = prefix;
		} else if (Q_streq(argv[i], "--log")) {
			config.log = true;
		} else if (Q_streq(argv[i], "-v") || Q_streq(argv[i], "--verbose")) {
			config.verbose = true;
		} else if (Q_streq(argv[i], "-h") || Q_streq(argv[i], "--help")) {
			printf("Usage:\n");
			printf("-h  --help                 | show this help screen\n");
			printf("-v  --verbose              | be verbose\n");
			printf("-Dprop=value               | defines a test property\n");
			printf("    --log                  | log ufo output to file (testall.log in working dir)\n");
			printf("    --output-prefix=PREFIX | set a prefix for the xml result\n");
			printf("                           | default value is \"ufoai\"\n");
			exit(0);
		} else {
			fprintf(stderr, "Error: Param \"%s\" unknown\n", argv[i]);
			fprintf(stderr, "Use \"%s -h\" to show the help screen\n", argv[0]);
			exit(2);
		}
	}
}

static void TEST_vPrintfLog (const char* fmt, va_list ap)
{
	if (!logFile)
		return;
	static char msg[2048];
	Q_vsnprintf(msg, sizeof(msg), fmt, ap);

	fprintf(logFile, "%s", msg);
	fflush(logFile);
}

class LocalEnv: public ::testing::Environment {
public:
	virtual ~LocalEnv() {
	}
	virtual void SetUp() override {
	}
	virtual void TearDown() override {
	}
};

/**
 * Setting up and running all tests.
 * Returns a CUE_SUCCESS on successful running, another
 * CUnit error code on failure.
 */
extern "C" int main (int argc, char** argv)
{
	Sys_InitSignals();

	::testing::AddGlobalTestEnvironment(new LocalEnv);
	//::testing::GTEST_FLAG(throw_on_failure) = true;
	::testing::InitGoogleTest(&argc, argv);

	Test_Parameters(argc, argv);

	if (config.log) {
		const char* path = "testall.log";
		logFile = Sys_Fopen(path, "wb");
		Qcommon_SetPrintFunction(TEST_vPrintfLog);
	} else if (config.verbose){
		Qcommon_SetPrintFunction(TEST_vPrintf);
	} else {
		Qcommon_SetPrintFunction(TEST_vPrintfSilent);
	}

	try {
		try {
			return RUN_ALL_TESTS();
		} catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	} catch (comDrop_t const&) {
		Sys_Error("There was a Com_Error or Com_Drop call during the execution of this test");
	}

	if (logFile)
		fclose(logFile);

	return EXIT_SUCCESS;
}
