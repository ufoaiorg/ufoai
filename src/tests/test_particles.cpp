/**
 * @file
 * @brief Particle unittests
 */

/*
 Copyright (C) 2002-2014 UFO: Alien Invasion.

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
#include "test_particles.h"
#include "../client/client.h"
#include "../client/battlescape/cl_particle.h"
#include "../client/renderer/r_state.h"
#include "../client/ui/ui_main.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteParticles (void)
{
	TEST_Init();
	PTL_InitStartup();
	vid_imagePool = Mem_CreatePool("Vid: Image system");

	cl_genericPool = Mem_CreatePool("Client: Generic");

	r_state.active_texunit = &r_state.texunits[0];
	R_FontInit();
	UI_Init();

	OBJZERO(cls);
	Com_ParseScripts(false);
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteParticles (void)
{
	TEST_Shutdown();
	return 0;
}

static void testFloodParticles (void)
{
	for (int i = 0; i < MAX_PTLS; i++) {
		CU_ASSERT_PTR_NOT_NULL(CL_ParticleSpawn("fire", 0xFF, vec3_origin));
	}
	CL_ParticleRun();
	CU_ASSERT_PTR_NULL(CL_ParticleSpawn("fire", 0xFF, vec3_origin));
}

static void testNeededParticles (void)
{
	CU_ASSERT_PTR_NOT_NULL(CL_ParticleGet("fadeTracer"));
	CU_ASSERT_PTR_NOT_NULL(CL_ParticleGet("longRangeTracer"));
	CU_ASSERT_PTR_NOT_NULL(CL_ParticleGet("inRangeTracer"));
	CU_ASSERT_PTR_NOT_NULL(CL_ParticleGet("crawlTracer"));
	CU_ASSERT_PTR_NOT_NULL(CL_ParticleGet("moveTracer"));
	CU_ASSERT_PTR_NOT_NULL(CL_ParticleGet("stunnedactor"));
	CU_ASSERT_PTR_NOT_NULL(CL_ParticleGet("circle"));
	CU_ASSERT_PTR_NOT_NULL(CL_ParticleGet("cross"));
	CU_ASSERT_PTR_NOT_NULL(CL_ParticleGet("cross_no"));
	CU_ASSERT_PTR_NOT_NULL(CL_ParticleGet("lightTracerDebug"));
}

int UFO_AddParticlesTests (void)
{
	/* add a suite to the registry */
	CU_pSuite ParticlesSuite = CU_add_suite("ParticlesTests", UFO_InitSuiteParticles, UFO_CleanSuiteParticles);

	if (ParticlesSuite == nullptr)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(ParticlesSuite, testFloodParticles) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(ParticlesSuite, testNeededParticles) == nullptr)
		return CU_get_error();

	return CUE_SUCCESS;
}
