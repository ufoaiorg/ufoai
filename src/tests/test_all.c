#include "CUnit/Basic.h"
typedef int (*testSuite_t) (void);

/* include the tests here */
#include "test_routing.h"
#include "test_inventory.h"

static const testSuite_t testSuites[] = {
	UFO_AddRoutingTests,
	UFO_AddInventoryTests,
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

/**
 * Setting up and running all tests.
 * Returns a CUE_SUCCESS on successful running, another
 * CUnit error code on failure.
 */
int main (void)
{
	int i;

	/* initialize the CUnit test registry */
	if (CU_initialize_registry() != CUE_SUCCESS)
		return CU_get_error();

	for (i = 0; i < NUMBER_OF_TESTS - 1; i++) {
		if (testSuites[i]() != CUE_SUCCESS)
			return fatalError();
	}

	/* Run all tests using the CUnit Basic interface */
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();
	return CU_get_error();
}
