/**
 * @file gl_arb_shader.c
 * @brief Shader and image filter stuff
 * @note This code is only active if SHADERS is true (see Makefile)
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

#ifdef SHADERS
#include "gl_local.h"

static unsigned int SH_LoadProgram_ARB_FP(char *path);
static unsigned int SH_LoadProgram_ARB_VP(char *path);
static qboolean shaderInited = qfalse;

/**
 * @brief Cycle through all parsed shaders and compile them
 * @sa GL_ShutdownShaders
 */
void GL_ShaderInit(void)
{
	int i = 0;
	shader_t *s;

	Com_DPrintf("Init shaders (num_shaders: %i)\n", r_newrefdef.num_shaders);
	for (i = 0; i < r_newrefdef.num_shaders; i++) {
		s = &r_newrefdef.shaders[i];
		if (s->frag)
			s->fpid = SH_LoadProgram_ARB_FP(s->filename);
		else if (s->vertex)
			s->vpid = SH_LoadProgram_ARB_VP(s->filename);
	}
	shaderInited = qtrue;
}

/**
 * @brief Compares the shader titles with the image name
 * @param[in] image Image name
 * @return shader_t pointer if image name and shader title match otherwise NULL
 */
shader_t* GL_GetShaderForImage(char* image)
{
	int i = 0;
	shader_t *s;

	/* init the shaders */
	if (!shaderInited && r_newrefdef.num_shaders)
		GL_ShaderInit();

	/* search for shader title and check whether it matches an image name */
	for (i = 0; i < r_newrefdef.num_shaders; i++) {
		s = &r_newrefdef.shaders[i];
		if (!Q_strcmp(s->title, image))
			return s;
	}
	return NULL;
}

/**
 * @brief Delete all ARB shader programs at shutdown
 * GL_ShaderInit
 */
void GL_ShutdownShaders(void)
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
		if (s->fpid) {
			ri.Con_Printf(PRINT_DEVELOPER, "..unload shader %s\n", s->filename);
			qglDeleteProgramsARB(1, &s->fpid);
		}
		if (s->vpid) {
			ri.Con_Printf(PRINT_DEVELOPER, "..unload shader %s\n", s->filename);
			qglDeleteProgramsARB(1, &s->vpid);
		}
		s->fpid = s->vpid = 0;
	}
}

/**
 * @brief Loads fragment program shader
 * @param[in] path The shader file path (relative to game-dir)
 * @return fpid - id of shader
 */
unsigned int SH_LoadProgram_ARB_FP(char *path)
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
unsigned int SH_LoadProgram_ARB_VP(char *path)
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
 * @brief Activate fragment shaders
 * @param[in] fpid the shader id
 * @sa SH_LoadProgram_ARB_FP
 */
void SH_UseProgram_ARB_FP(unsigned int fpid)
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
void SH_UseProgram_ARB_VP(unsigned int vpid)
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
void SH_UseShader(shader_t * shader, qboolean deactivate)
{
	assert(shader);
	/* no shaders supported */
	if (gl_state.arb_fragment_program == qfalse)
		return;
	if (shader->fpid > 0) {
		if (deactivate)
			SH_UseProgram_ARB_FP(-1);
		else {
			qglActiveTextureARB(GL_TEXTURE0_ARB);
			SH_UseProgram_ARB_FP(shader->fpid);
		}
	} else if (shader->vpid > 0) {
		if (deactivate)
			SH_UseProgram_ARB_VP(-1);
		else {
			qglActiveTextureARB(GL_TEXTURE0_ARB);
			SH_UseProgram_ARB_VP(shader->vpid);
		}
	}
}

#endif /* SHADERS */
