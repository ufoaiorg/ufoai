/**
 * @file
 * @brief Particle unittests
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
#include "../client/client.h"
#include "../client/battlescape/cl_particle.h"
#include "../client/renderer/r_state.h"
#include "../client/ui/ui_main.h"

class ParticleTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
		PTL_InitStartup();
		vid_imagePool = Mem_CreatePool("Vid: Image system");

		cl_genericPool = Mem_CreatePool("Client: Generic");

		r_state.active_texunit = &r_state.texunits[0];
		R_FontInit();
		UI_Init();

		OBJZERO(cls);
		Com_ParseScripts(false);
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}
};

TEST_F(ParticleTest, FloodParticles)
{
	for (int i = 0; i < MAX_PTLS; i++) {
		ASSERT_TRUE(nullptr != CL_ParticleSpawn("fire", 0xFF, vec3_origin));
	}
	CL_ParticleRun();
	ASSERT_TRUE(nullptr == CL_ParticleSpawn("fire", 0xFF, vec3_origin));
}

TEST_F(ParticleTest, NeededParticles)
{
	ASSERT_TRUE(nullptr != CL_ParticleGet("fadeTracer"));
	ASSERT_TRUE(nullptr != CL_ParticleGet("longRangeTracer"));
	ASSERT_TRUE(nullptr != CL_ParticleGet("inRangeTracer"));
	ASSERT_TRUE(nullptr != CL_ParticleGet("crawlTracer"));
	ASSERT_TRUE(nullptr != CL_ParticleGet("moveTracer"));
	ASSERT_TRUE(nullptr != CL_ParticleGet("stunnedactor"));
	ASSERT_TRUE(nullptr != CL_ParticleGet("circle"));
	ASSERT_TRUE(nullptr != CL_ParticleGet("cross"));
	ASSERT_TRUE(nullptr != CL_ParticleGet("cross_no"));
	ASSERT_TRUE(nullptr != CL_ParticleGet("lightTracerDebug"));
}
