
#include "CUnit/Basic.h"
#include "test_routing.h"

#include "../common/common.h"
#include "../common/routing.h"
#include "../common/tracing.h"
#include "../common/cmodel.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteRouting (void)
{
	unsigned int checksum;

	com_aliasSysPool = Mem_CreatePool("Common: Alias system");
	com_cmdSysPool = Mem_CreatePool("Common: Command system");
	com_cmodelSysPool = Mem_CreatePool("Common: Collision model");
	com_cvarSysPool = Mem_CreatePool("Common: Cvar system");
	com_fileSysPool = Mem_CreatePool("Common: File system");
	com_genericPool = Mem_CreatePool("Generic");

	Mem_Init();
	Cmd_Init();
	Cvar_Init();
	FS_InitFilesystem(qtrue);

	/**
	 * @todo use a special testmap
	 */
	CM_LoadMap("maps/fueldump", qtrue, "", &checksum);
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteRouting (void)
{
	Mem_Shutdown();
	FS_Shutdown();
	return 0;
}

static void testConnection (void)
{
	/**
	 * @todo implement the test
	 */
}

int UFO_AddRoutingTests (void) {
	/* add a suite to the registry */
	CU_pSuite routingSuite = CU_add_suite("RoutingTests", UFO_InitSuiteRouting, UFO_CleanSuiteRouting);
	if (routingSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(routingSuite, testConnection) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
