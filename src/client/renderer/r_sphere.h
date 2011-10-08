/**
 * @file r_sphere.c
 * @brief Functions to generate and render spheres
 */
/*
Copyright (C) 1997-2001 Id Software, Inc.

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

typedef struct sphere_s {
	GLint list;			/**< the opengl list id */
	float *texes;		/**< globe tex coords, points in 2D on the texture to be mapped onto the sphere */
	float *verts;		/**< globe vertices, points in 3D on the surface of the sphere */
	float *normals;		/**< vertex normal array */
	image_t* texture;	/**< the texture for this globe - @note scaled texture matrix */
	image_t* blendTexture;	/**< the texture for the next season (for blending) */
	image_t* overlay;	/**< the overlay for the world (e.g. nation borders) */
	image_t* overlayAlphaMask;	/**< in case of multitexture this is the alpha mask */
	image_t* normalMap;	/**< bump map */
	int num_tris;		/**< number of tris */

	/** @note  extra stuff for new GLSL renderer*/
	r_program_t *glslProgram;
	float blendScale;
	float glowScale;
	vec4_t nightLightPos;
	int season;
} sphere_t;

extern sphere_t r_globeEarth;
extern sphere_t r_globeMoon;
extern sphere_t r_globeEarthAtmosphere;

void R_SphereGenerate(sphere_t *sphere, const int tris, const float radius);
void R_SphereRender(const sphere_t *sphere, const vec3_t pos, const vec3_t rotate, const float scale, const vec4_t lightPos);
void R_SphereInit(void);
