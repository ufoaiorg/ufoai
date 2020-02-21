/**
 * @file
 * @brief Weather effects rendering subsystem header file
 */

/*
Copyright (C) 2013 Alexander Y. Tishin
Copyright (C) 2013-2020 UFO: Alien Invasion.

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

#pragma once

class Weather {
	public:
		/* public members are intended; they are to avoid need of getters/setters to change weather in-flight */
		enum weatherTypes {WEATHER_CLEAN, WEATHER_RAIN, WEATHER_SNOW, WEATHER_SANDSTORM, WEATHER_MAX};
		weatherTypes weatherType;
		float weatherStrength; /** < 0 to 1, amount of particles generated, 1 is the maximum possible */
		float windStrength; /** < units/sec */
		float windDirection; /** < in radians, 0 is due east */
		float windTurbulence; /** < variety of wind strength and direction in units per second */
		float fallingSpeed; /** < how fast particles fall */

		int splashTime; /** < how long splash effect is being played, in milliseconds */
		float splashSize; /** < final particle size */

		Weather(void);
		Weather(weatherTypes weather);
		virtual ~Weather();

		void setDefaults(void);
		void changeTo(weatherTypes weather);
		void clearParticles(void);

		void update(int milliseconds);
		void render(void);
	protected:
		static const size_t MAX_PARTICLES = 8192; /** < particle array size, must be a power of two */
		float smearLength; /** < how much particle is motion-blurred, in seconds */
		float particleSize; /** < how much to scale from default */

		vec4_t color;

		struct particle {
			GLfloat x, y, z; /** < position */
			GLfloat vx, vy, vz; /** < current velocity; vz == 0 indicates a splash particle */
			int timeout; /** < milliseconds to physics update */
			int ttl; /** < time to live in milliseconds */
		};

		particle particles[MAX_PARTICLES];
};
