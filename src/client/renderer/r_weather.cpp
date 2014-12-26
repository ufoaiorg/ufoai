/**
 * @file
 * @brief Weather effects rendering subsystem implementation
 */

/*
Copyright (C) 2013 Alexander Y. Tishin
Copyright (C) 2013 UFO: Alien Invasion.

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

#include <stdint.h>
#include "r_local.h"
#include "../client.h"
#include "r_weather.h"

/* wp stands for "weather particle */
static const uint8_t wpImage[8][8] = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,},
	{0x00, 0x00, 0x20, 0x40, 0x40, 0x20, 0x00, 0x00,},
	{0x00, 0x20, 0xa0, 0xc0, 0xc0, 0xa0, 0x20, 0x00,},
	{0x00, 0x40, 0xc0, 0xff, 0xff, 0xc0, 0x40, 0x00,},
	{0x00, 0x40, 0xc0, 0xff, 0xff, 0xc0, 0x40, 0x00,},
	{0x00, 0x20, 0xa0, 0xc0, 0xc0, 0xa0, 0x20, 0x00,},
	{0x00, 0x00, 0x20, 0x40, 0x40, 0x20, 0x00, 0x00,},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,}
};

/**
 * @brief Gives handle to (and uploads if needed) weather particle texture; also binds it to the active texture unit
 * @return OpenGL handle of texture object
 * @note alas, image_t does not support anything but RGB(A) textures, so we've got to reinvent the wheel
 */
static GLuint wpTexture (void)
{
	static GLuint wpTextureHandle = 0;

	if (wpTextureHandle)
		return wpTextureHandle;

	R_EnableTexture(&texunit_diffuse, true);
	glGenTextures(1, &wpTextureHandle);
	glBindTexture(GL_TEXTURE_2D, wpTextureHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 8, 8, 0, GL_ALPHA, GL_UNSIGNED_BYTE, wpImage);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

	return wpTextureHandle;
}

/**
 * @brief Sets clean weather, but generate some parameters usable for easily changing it into the worse varieties
 */
void Weather::setDefaults (void)
{
	weatherType = WEATHER_CLEAN;
	weatherStrength = 0;
	windStrength = 0;
	windDirection = frand() * M_PI_2; /* no preferred direction (arc) for the wind */
	windTurbulence = 0;
	fallingSpeed = 30; /* to avoid permanently levitating particles if someone decides to alter the weather but forgets to set it */
	smearLength = 0.1f; /* 100 msec is the typical duration of the human eye motion blur */
	particleSize = 1.0f; /* 1x scale by default */

	splashTime = 0.0f;
	splashSize = 1.0f;

	color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f; color[3] = 0.6f;
}

/**
 * @brief changes to weather of given type; parameters are randomized
 */
void Weather::changeTo (weatherTypes weather)
{
	setDefaults();

	weatherType = weather;

	switch (weather){
		case WEATHER_RAIN:
			weatherStrength = frand() * 0.8f + 0.2f;
			windStrength = frand() * 20;
			fallingSpeed = 500;
			splashTime = 500;
			splashSize = particleSize * 4.0f;
			color[0] = 0.6f; color[1] = 0.6f; color[2] = 0.6f; color[3] = 0.6f;
			break;
		case WEATHER_SNOW:
			weatherStrength = frand() * 0.8f + 0.2f;
			windStrength = frand() * 10;
			windTurbulence = 15;
			fallingSpeed = 50;
			smearLength = 0;
			particleSize = 1.5f;
			splashTime = 1500;
			splashSize = particleSize;
			break;
		case WEATHER_SANDSTORM:
			weatherStrength = frand() * 0.1f + 0.9f;
			windStrength = frand() * 500 + 1000;
			windTurbulence = 23;
			fallingSpeed = 200; /* in a sandstorm, sand is blown mostly horizontally */
			particleSize = 5.0f;
			color[0] = 1.0f; color[1] = 0.7f; color[2] = 0.25f; color[3] = 0.6f;
			break;
		default: /* clean weather */
			break;
	};
}

/**
 * @brief Just a shared methods for ctors to initialize the weather
 */
void Weather::clearParticles (void)
{
	for (size_t i = 0; i < Weather::MAX_PARTICLES; i++) {
		particles[i].ttl = -1;
	}
}

/**
 * @brief make a default (clean) weather
 */
Weather::Weather (void)
{
	setDefaults();
	clearParticles();
}

/**
 * @brief make a weather of given type
 */
Weather::Weather (weatherTypes weather)
{
	changeTo(weather);
	clearParticles();
}

Weather::~Weather ()
{
}

/**
 * @brief Updates weather for the time passed; handles particle creation/removal automatically
 */
void Weather::update (int milliseconds)
{
	size_t dead = 0;
	/* physics: check for ttl and move live particles */
	for (size_t i = 0; i < Weather::MAX_PARTICLES; i++) {
		Weather::particle &prt = particles[i];
		if (prt.ttl < 0) {
			dead++;
			continue;
		} else { /** @todo creates vanishing-before-impact particles at low framerates -- should improve that somehow */
			int restOfLife = prt.ttl -= milliseconds;
			if (restOfLife < 0) {
				if (fabs(prt.vz) < 0.001f || splashTime < 1) {
					/* either no splash or is a splash particle dying */
					dead++;
					continue;
				}
				/* convert it into splash particle */
				/* technically, we should complete the last frame of movement, but with current particle types it looks good even without it */
				prt.vz = 0; prt.vx = 0; prt.vy = 0;
				prt.ttl += splashTime;
			}
		}
		/* if we got so far, particle is alive and probably needs a physics update */
		const int timeAfterUpdate = prt.timeout -= milliseconds;
		const float moveDuration = milliseconds * 0.001f;

		prt.x += prt.vx * moveDuration; prt.y += prt.vy * moveDuration; prt.z += prt.vz * moveDuration;

		if (timeAfterUpdate > 0) {
			/* just move linearly */
			continue;
		}
		/* alter motion vector */
		/** @todo */
	}
	/* create new ones in place of dead */
	if (dead) {
		const float windX = cos(windDirection) * (windStrength + crand() * windTurbulence);
		const float windY = sin(windDirection) * (windStrength + crand() * windTurbulence);

		AABB weatherZone; /** < extents of zone in which weather particles will be rendered */
		weatherZone = cl.mapData->mapBox;
		weatherZone.expandXY(256);
		weatherZone.maxs[2] += 512;

		Line prtPath;

		if (dead > 200) dead = 200; /* avoid creating too much particles in the single frame, it could kill the low-end hardware */

		int debugCreated = 0;
		for (size_t i = 0; dead &&  i < Weather::MAX_PARTICLES && i < weatherStrength * Weather::MAX_PARTICLES; i++) {
			Weather::particle &prt = particles[i];
			if (prt.ttl >= 0)
				continue;
			prt.x = (frand() * (weatherZone.getMaxX() - weatherZone.getMinX())) + weatherZone.getMinX();
			prt.y = (frand() * (weatherZone.getMaxY() - weatherZone.getMinY())) + weatherZone.getMinY();
			prt.z = weatherZone.getMaxZ();

			prt.vx = windX;
			prt.vy = windY;
			prt.vz = -fallingSpeed * (frand() * 0.2f + 0.9f);

			float lifeTime = (weatherZone.getMaxZ() - weatherZone.getMinZ()) / -prt.vz; /* default */

			VectorSet(prtPath.start, prt.x, prt.y, prt.z);
			VectorSet(prtPath.stop, prt.x + prt.vx * lifeTime, prt.y + prt.vy * lifeTime, prt.z + prt.vz * lifeTime);

			trace_t trace = CL_Trace(prtPath, AABB::EMPTY, nullptr, nullptr, MASK_SOLID, cl.mapMaxLevel - 1); /* find the collision point */
			lifeTime *= trace.fraction;

			prt.ttl = 1000 * lifeTime; /* convert to milliseconds */

			prt.timeout = prt.ttl + 1000000; /** @todo proper code for physics */
			debugCreated++;
			dead--;
		}
	}
#if 0
	if (debugCreated) Com_Printf("created %i weather particles\n", debugCreated);
#endif
}

/**
 * @brief Draws weather effects
 */
void Weather::render (void)
{
	GLfloat prtPos[3 * 4 * Weather::MAX_PARTICLES];
	GLfloat prtTexcoord[2 * 4 * Weather::MAX_PARTICLES];
	size_t prtCount = 0;
	//const float splashTimeScale = 0.001f / splashTime; /* from msec to 1/sec units, so division is done only once per frame */
	/** @todo shadowcasting at least for the sunlight */
#if 0 // disabled because of bizarre colors
	vec4_t prtColor = {color[0] * (refdef.ambientColor[0] + refdef.sunDiffuseColor[0]),
		color[1] * (refdef.ambientColor[1] + refdef.sunDiffuseColor[1]),
		color[2] * (refdef.ambientColor[2] + refdef.sunDiffuseColor[2]),
		color[3]};
#else
	vec_t prtBrightness = VectorLength(refdef.ambientColor) + VectorLength(refdef.sunDiffuseColor); /** @todo proper color to brightness mapping */
	vec4_t prtColor = {color[0] * prtBrightness, color[1] * prtBrightness, color[2] * prtBrightness, color[3]}; /* alpha is not to be scaled */
#endif

	for (size_t i = 0; i < Weather::MAX_PARTICLES; i++) {
		Weather::particle &prt = particles[i];

		if (prt.ttl < 0)
			continue;

		/* if particle is alive, construct a camera-facing quad */
		GLfloat x, y, z;
		GLfloat dx, dy, dz;
		GLfloat thisParticleSize = particleSize;
		if (prt.vz == 0 && splashTime > 0) {
			/* splash particle, do zoom and other things */
			/** @todo alpha decay */
			const float splashFactor = prt.ttl / 500.0f;//1.0f - prt.ttl * splashTimeScale;
			thisParticleSize *= (splashSize - 1.0f) * splashFactor  + 1.0f;
			dx = dy = thisParticleSize;
		} else {
			dx = i & 1 ? thisParticleSize* 2 : thisParticleSize; dy = i & 1 ? thisParticleSize : thisParticleSize * 2 ; /** @todo make a proper projection instead of this hack */
		}

		x = prt.x; y = prt.y; z = prt.z;
		dz = smearLength * prt.vz * 0.5; /** @todo should use proper velocity vector ... or maybe not */
		/* construct a particle geometry; */
		GLfloat *pos = &prtPos[3 * 4 * prtCount];
		GLfloat *texcoord = &prtTexcoord[2 * 4 * prtCount];

		/** @todo actually rotate billboard in the correct position */
		pos[0] = x - dx; pos[1] = y - dy; pos[2] = z + dz * 2;
		pos[3] = x + dx; pos[4] = y - dy; pos[5] = z + dz *2;
		pos[6] = x + dx; pos[7] = y + dy; pos[8] = z;
		pos[9] = x - dx; pos[10] = y + dy; pos[11] = z;

		texcoord[0] = 0; texcoord[1] = 0; /* upper left vertex */
		texcoord[2] = 1.0f; texcoord[3] = 0; /* upper right vertex */
		texcoord[4] = 1.0f; texcoord[5] = 1.0f; /* bottom right vertex */
		texcoord[6] = 0; texcoord[7] = 1.0f; /* bottom left vertex */

		prtCount++;
	}

	if (prtCount < 1)
		return;

	/* draw the generated array */
	R_Color(prtColor);
	glBindTexture(GL_TEXTURE_2D, wpTexture());
	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, prtPos);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, prtTexcoord);
	R_EnableBlend(true);
	glDrawArrays(GL_QUADS, 0, prtCount);
	R_EnableBlend(false);
	R_ResetArrayState();
	R_Color(nullptr);

	refdef.batchCount++;
}
