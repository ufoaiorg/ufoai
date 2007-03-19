/**
 * @file gl_arb_shader.c
 * @brief Shader and image filter stuff
 * @note This code is only active if HAVE_SHADERS is true
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

#include "gl_local.h"

#ifdef HAVE_SHADERS
static unsigned int SH_LoadProgram_ARB_FP(const char *path);
static unsigned int SH_LoadProgram_ARB_VP(const char *path);
static unsigned int SH_LoadProgram_GLSL(shader_t* s);
static qboolean shaderInited = qfalse;

/**
 * @brief Cycle through all parsed shaders and compile them
 * @sa GL_ShutdownShaders
 */
void GL_ShaderInit (void)
{
	int i = 0;
	shader_t *s;

	ri.Con_Printf(PRINT_DEVELOPER, "Init shaders (num_shaders: %i)\n", r_newrefdef.num_shaders);
	for (i = 0; i < r_newrefdef.num_shaders; i++) {
		s = &r_newrefdef.shaders[i];
		if (s->glsl) {
			if (gl_state.glsl_program) {
				/* a glsl can be a shader or a vertex program */
				SH_LoadProgram_GLSL(s);
			}
		} else if (s->vertex) {
			s->vpid = SH_LoadProgram_ARB_VP(s->filename);
		} else if (s->frag) {
			s->fpid = SH_LoadProgram_ARB_FP(s->filename);
		}
	}
	shaderInited = qtrue;
}
#endif

/**
 * @brief Compares the shader titles with the image name
 * @param[in] image Image name
 * @return shader_t pointer if image name and shader title match otherwise NULL
 */
shader_t* GL_GetShaderForImage (char* image)
{
	int i = 0;
	shader_t *s;

#ifdef HAVE_SHADERS
	/* init the shaders */
	if (!shaderInited && r_newrefdef.num_shaders)
		GL_ShaderInit();
#endif

	/* search for shader title and check whether it matches an image name */
	for (i = 0; i < r_newrefdef.num_shaders; i++) {
		s = &r_newrefdef.shaders[i];
		if (!Q_strcmp(s->name, image)) {
			ri.Con_Printf(PRINT_DEVELOPER, "shader for '%s' found\n", image);
			return s;
		}
	}
	return NULL;
}

#ifdef HAVE_SHADERS
/**
 * @brief Delete all ARB shader programs at shutdown
 * GL_ShaderInit
 */
void GL_ShutdownShaders (void)
{
	int i = 0;
	shader_t *s;

	/* init the shaders */
	if (!shaderInited && r_newrefdef.num_shaders)
		GL_ShaderInit();

	ri.Con_Printf(PRINT_DEVELOPER, "Shader shutdown\n");
	/* search for shader title and check whether it matches an image name */
	for (i = 0; i < r_newrefdef.num_shaders; i++) {
		s = &r_newrefdef.shaders[i];
		if (s->glsl && gl_state.glsl_program) {
			if (s->fpid > 0)
				qglDeleteShader(s->fpid);
			if (s->fpid > 0)
				qglDeleteShader(s->vpid);
			qglDeleteProgram(s->glslpid);
			s->fpid = s->vpid = -1;
		}
		if (s->fpid > 0) {
			ri.Con_Printf(PRINT_DEVELOPER, "..unload shader %s\n", s->filename);
			qglDeleteProgramsARB(1, &s->fpid);
		}
		if (s->vpid > 0) {
			ri.Con_Printf(PRINT_DEVELOPER, "..unload shader %s\n", s->filename);
			qglDeleteProgramsARB(1, &s->vpid);
		}
		s->fpid = s->vpid = -1;
	}
}

/**
 * @brief Loads fragment program shader
 * @param[in] path The shader file path (relative to game-dir)
 * @return fpid - id of shader
 */
unsigned int SH_LoadProgram_ARB_FP (const char *path)
{
	char *fbuf, *buf;
	unsigned int size;

	const unsigned char *errors;
	int error_pos;
	unsigned int fpid;

	size = ri.FS_LoadFile(path, (void **) &fbuf);

	if (!fbuf) {
		ri.Con_Printf(PRINT_ALL, "Could not load shader %s\n", path);
		return -1;
	}

	if (size < 16) {
		ri.Con_Printf(PRINT_ALL, "Could not load invalid shader with size %i: %s\n", size, path);
		ri.FS_FreeFile(fbuf);
		return -1;
	}

	buf = (char *) malloc(size + 1);
	memcpy(buf, fbuf, size);
	buf[size] = 0;
	ri.FS_FreeFile(fbuf);

	qglGenProgramsARB(1, &fpid);
	qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpid);
	qglProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, size, buf);

	errors = qglGetString(GL_PROGRAM_ERROR_STRING_ARB);

	qglGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
	if (error_pos != -1) {
		ri.Con_Printf(PRINT_ALL, "!! FP error at position %d in %s\n", error_pos, path);
		ri.Con_Printf(PRINT_ALL, "!! Error: %s\n", (char *) errors);
		qglDeleteProgramsARB(1, &fpid);
		free(buf);
		return 0;
	}
	free(buf);
	ri.Con_Printf(PRINT_DEVELOPER, "...loaded fragment shader %s (pid: %i)\n", path, fpid);
	return fpid;
}

/**
 * @brief Loads vertex program shader
 * @param[in] path The shader file path (relative to game-dir)
 * @return vpid - id of shader
 */
unsigned int SH_LoadProgram_ARB_VP (const char *path)
{
	char *fbuf, *buf;
	unsigned int size, vpid;

	size = ri.FS_LoadFile(path, (void **) &fbuf);

	if (!fbuf) {
		ri.Con_Printf(PRINT_ALL, "Could not load shader %s\n", path);
		return -1;
	}

	if (size < 16) {
		ri.Con_Printf(PRINT_ALL, "Could not load invalid shader with size %i: %s\n", size, path);
		ri.FS_FreeFile(fbuf);
		return -1;
	}

	buf = (char *) malloc(size + 1);
	memcpy(buf, fbuf, size);
	buf[size] = 0;
	ri.FS_FreeFile(fbuf);

	qglGenProgramsARB(1, &vpid);
	qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpid);
	qglProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, size, buf);

	{
		const unsigned char *errors = qglGetString(GL_PROGRAM_ERROR_STRING_ARB);
		int error_pos;

		qglGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
		if (error_pos != -1) {
			ri.Con_Printf(PRINT_ALL, "!! VP error at position %d in %s\n", error_pos, path);
			ri.Con_Printf(PRINT_ALL, "!! Error: %s\n", (char *) errors);

			qglDeleteProgramsARB(1, &vpid);
			free(buf);
			return 0;
		}
	}
	free(buf);
	ri.Con_Printf(PRINT_DEVELOPER, "...loaded vertex shader %s (pid: %i)\n", path, vpid);
	return vpid;
}

/**
 * @brief Loads vertex program shader
 * @param[in] path The shader file path (relative to game-dir)
 * @return vpid - id of shader
 */
unsigned int SH_LoadProgram_GLSL (shader_t* s)
{
	char *fbuf;
	unsigned int size;

	size = ri.FS_LoadFile(s->filename, (void **) &fbuf);

	if (!fbuf) {
		ri.Con_Printf(PRINT_ALL, "Could not load shader %s\n", s->filename);
		return -1;
	}

	if (size < 16) {
		ri.Con_Printf(PRINT_ALL, "Could not load invalid shader with size %i: %s\n", size, s->filename);
		ri.FS_FreeFile(fbuf);
		return -1;
	}

	s->glslpid = qglCreateProgram();

	if (s->frag) {
		s->fpid = qglCreateShader(GL_FRAGMENT_SHADER_ARB);
		qglShaderSource(s->fpid, 1, (const char**)&fbuf, NULL);
		qglCompileShader(s->fpid);
		qglAttachShader(s->glslpid, s->fpid);
	} else if (s->vertex) {
		s->vpid = qglCreateShader(GL_VERTEX_SHADER_ARB);
		qglShaderSource(s->vpid, 1, (const char**)&fbuf, NULL);
		qglCompileShader(s->vpid);
		qglAttachShader(s->glslpid, s->vpid);
	}

	qglLinkProgram(s->glslpid);

	ri.FS_FreeFile(fbuf);
	ri.Con_Printf(PRINT_DEVELOPER, "...loaded glsl shader %s (pid: %i)\n", s->filename, s->glslpid);
	return s->glslpid;
}

/**
 * @brief Activate fragment shaders
 * @param[in] fpid the shader id
 * @sa SH_LoadProgram_ARB_FP
 */
void SH_UseProgram_ARB_FP (unsigned int fpid)
{
	if (fpid > 0) {
		qglEnable(GL_FRAGMENT_PROGRAM_ARB);
		qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpid);
	} else {
		qglDisable(GL_FRAGMENT_PROGRAM_ARB);
	}
}

/**
 * @brief Activate vertex shader
 * @param[in] vpid the shader id
 * @sa SH_LoadProgram_ARB_VP
 */
void SH_UseProgram_ARB_VP (unsigned int vpid)
{
	if (vpid > 0) {
		qglEnable(GL_VERTEX_PROGRAM_ARB);
		qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpid);
	} else {
		qglDisable(GL_VERTEX_PROGRAM_ARB);
	}
}

/**
 * @brief Use a shader if arg fragment program are supported
 * @sa SH_UseProgram_ARB_FP
 * @sa SH_UseProgram_ARB_VP
 * @param[in] shader Shader pointer (see image_t)
 */
void SH_UseShader (shader_t * shader, qboolean deactivate)
{
	image_t *gl, *normal, *distort;

	/* no shaders supported */
	if (gl_state.arb_fragment_program == qfalse)
		return;

	assert(shader);

	if (!deactivate)
		gl = GL_FindImageForShader(shader->name);

	if (shader->glslpid > 0) {
		if (deactivate)
			qglUseProgram(0);
		else
			qglUseProgram(shader->glslpid);
	} else if (shader->fpid > 0) {
		if (deactivate) {
			SH_UseProgram_ARB_FP(0);
			if (shader->distort[0]) {
				qglActiveTextureARB(GL_TEXTURE1_ARB);
				qglDisable(GL_TEXTURE_2D);
				qglActiveTextureARB(GL_TEXTURE0_ARB);
			}
		} else {
			assert(gl);
			qglActiveTextureARB(GL_TEXTURE0_ARB);
			qglBindTexture(GL_TEXTURE_2D, gl->texnum);
			if (shader->distort[0]) {
				distort = GL_FindImage(shader->distort, it_pic);
				qglActiveTextureARB(GL_TEXTURE1_ARB);
				qglBindTexture(GL_TEXTURE_2D, distort->texnum);
				qglEnable(GL_TEXTURE_2D);
			}
			if (shader->normal[0]) {
				normal = GL_FindImage(shader->normal, it_pic);
			}
			SH_UseProgram_ARB_FP(shader->fpid);
		}
	} else if (shader->vpid > 0) {
		if (deactivate) {
			SH_UseProgram_ARB_VP(0);
			if (shader->distort[0]) {
				qglActiveTextureARB(GL_TEXTURE1_ARB);
				qglDisable(GL_TEXTURE_2D);
				qglActiveTextureARB(GL_TEXTURE0_ARB);
			}
		} else {
			assert(gl);
			qglActiveTextureARB(GL_TEXTURE0_ARB);
			qglBindTexture(GL_TEXTURE_2D, gl->texnum);
			if (shader->distort[0]) {
				distort = GL_FindImage(shader->distort, it_pic);
				if (distort) {
					qglActiveTextureARB(GL_TEXTURE1_ARB);
					qglBindTexture(GL_TEXTURE_2D, distort->texnum);
					qglEnable(GL_TEXTURE_2D);
				}
			}
			if (shader->normal[0]) {
				normal = GL_FindImage(shader->normal, it_pic);
			}
			SH_UseProgram_ARB_VP(shader->vpid);
		}
	} else {
		return;
	}
}

#endif /* HAVE_SHADERS */
