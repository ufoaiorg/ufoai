/**
 * @file r_shader.h
 * @brief Shader and image filter stuff
 */

/*

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

#ifndef __R_ARB_SHADER_H__
#define __R_ARB_SHADER_H__

#define MAX_SHADERS 64
typedef struct shader_s {
	/* title is internal for finding the shader */

	/** we should use this shader when loading the image */
	char *name;

	/** filename is for an external filename to load the shader from */
	char *filename;

	/** image filenames */
	char *distort;
	char *normal;

	qboolean glsl;				/**< glsl shader */
	qboolean frag;				/**< fragment-shader */
	qboolean vertex;			/**< vertex-shader */

	byte glMode;
	/** @todo */

	/* vpid and fpid are vertexpid and fragmentpid for binding */
	int vpid, fpid, glslpid;
} shader_t;

extern int r_numShaders;
extern shader_t r_shaders[MAX_SHADERS];

shader_t* R_GetShader(const char* name);
void R_ShutdownShaders(void);
void SH_UseShader(shader_t * shader, qboolean activate);
void R_ShaderFragmentParameter(unsigned int index, float *p);
void R_ShaderInit(void);

#endif
