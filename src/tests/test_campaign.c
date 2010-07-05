/**
 * @file test_campaign.c
 * @brief Test cases for the campaign code
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

#include "CUnit/Basic.h"
#include "test_shared.h"
#include "test_campaign.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteCampaign (void)
{
	TEST_Init();
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteCampaign (void)
{
	TEST_Shutdown();
	return 0;
}

static void testCampaign (void)
{
}

int UFO_AddCampaignTests (void)
{
	/* add a suite to the registry */
	CU_pSuite campaignSuite = CU_add_suite("CampaignTests", UFO_InitSuiteCampaign, UFO_CleanSuiteCampaign);

	if (campaignSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(campaignSuite, testCampaign) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
