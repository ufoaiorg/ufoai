/**
 * @file r_shader.c
 * @brief Shader and image filter stuff
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

#include "r_local.h"
#include "r_shader.h"
#include "r_error.h"

static int SH_LoadProgram_ARB_FP(const char *path);
static int SH_LoadProgram_ARB_VP(const char *path);
static int SH_LoadProgram_GLSL(shader_t* s);
static qboolean shaderInited = qfalse;

/**
 * @brief Cycle through all parsed shaders and compile them
 * @sa R_ShutdownShaders
 */
void R_ShaderInit (void)
{
	int i = 0, uploadedCnt = 0;
	shader_t *s;

	for (i = 0; i < r_numShaders; i++) {
		s = &r_shaders[i];
		if (s->glsl) {
			if (r_state.glsl_program) {
				/* a glsl can be a shader or a vertex program */
				if (SH_LoadProgram_GLSL(s) > 0)
					uploadedCnt++;
			}
		} else if (s->vertex) {
			s->vpid = SH_LoadProgram_ARB_VP(s->filename);
			if (s->vpid > 0)
				uploadedCnt++;
		} else if (s->frag) {
			s->fpid = SH_LoadProgram_ARB_FP(s->filename);
			if (s->fpid > 0)
				uploadedCnt++;
		}
	}
	shaderInited = qtrue;
	Com_Printf("...uploaded %i shaders\n", uploadedCnt);
}

/**
 * @brief Compares the shader titles with the image name
 * @param[in] name Shader name
 * @return shader_t pointer or NULL if not found
 */
shader_t* R_GetShader (const char* name)
{
	int i = 0;
	shader_t *s;

	/* search for shader title and check whether it matches an image name */
	for (i = 0; i < r_numShaders; i++) {
		s = &r_shaders[i];
		if (!Q_strcmp(s->name, name)) {
			Com_DPrintf(DEBUG_RENDERER, "shader '%s' found\n", name);
			return s;
		}
	}
	return NULL;
}

/**
 * @brief Delete all ARB shader programs at shutdown
 * R_ShaderInit
 */
void R_ShutdownShaders (void)
{
	int i = 0;
	shader_t *s;

	if (!shaderInited)
		return;

	Com_DPrintf(DEBUG_RENDERER, "Shader shutdown\n");
	/* search for shader title and check whether it matches an image name */
	for (i = 0; i < r_numShaders; i++) {
		s = &r_shaders[i];
		if (s->glsl && r_state.glsl_program) {
			if (s->fpid > 0)
				qglDeleteShader(s->fpid);
			if (s->fpid > 0)
				qglDeleteShader(s->vpid);
			if (s->glslpid > 0)
				qglDeleteProgram(s->glslpid);
			s->glslpid = s->fpid = s->vpid = -1;
		}
		if (s->fpid > 0) {
			Com_DPrintf(DEBUG_RENDERER, "..unload shader %s\n", s->filename);
			qglDeleteProgramsARB(1, (unsigned int*)&s->fpid);
		}
		if (s->vpid > 0) {
			Com_DPrintf(DEBUG_RENDERER, "..unload shader %s\n", s->filename);
			qglDeleteProgramsARB(1, (unsigned int*)&s->vpid);
		}
		s->fpid = s->vpid = -1;
	}
	R_CheckError();
}

/**
 * @brief Loads fragment program shader
 * @param[in] path The shader file path (relative to game-dir)
 * @return fpid - id of shader
 */
static int SH_LoadProgram_ARB_FP (const char *path)
{
	byte *fbuf;
	int size;
	const unsigned char *errors;
	int error_pos;
	unsigned int fpid;

	if (!r_state.arb_fragment_program)
		return -1;

	size = FS_LoadFile(path, &fbuf);

	if (!fbuf) {
		Com_Printf("Could not load shader %s\n", path);
		return -1;
	}

	if (size < 16) {
		Com_Printf("Could not load invalid shader with size %i: %s\n", size, path);
		FS_FreeFile(fbuf);
		return -1;
	}

	qglEnable(GL_FRAGMENT_PROGRAM_ARB);
	qglGenProgramsARB(1, &fpid);
	qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpid);
	qglProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, size, *(const char **)&fbuf);
	qglDisable(GL_FRAGMENT_PROGRAM_ARB);

	errors = qglGetString(GL_PROGRAM_ERROR_STRING_ARB);

	qglGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
	if (error_pos != -1) {
		Com_Printf("!! FP error at position %d in %s\n", error_pos, path);
		Com_Printf("!! Error: %s\n", errors);
		qglDeleteProgramsARB(1, &fpid);
		FS_FreeFile(fbuf);
		return 0;
	}
	FS_FreeFile(fbuf);
	Com_DPrintf(DEBUG_RENDERER, "...loaded fragment shader %s (pid: %i)\n", path, fpid);
	R_CheckError();
	return fpid;
}

/**
 * @brief Loads vertex program shader
 * @param[in] path The shader file path (relative to game-dir)
 * @return vpid - id of shader
 */
static int SH_LoadProgram_ARB_VP (const char *path)
{
	byte *fbuf;
	int size, vpid;
	const unsigned char *errors;
	int error_pos;

	if (!r_state.arb_fragment_program)
		return -1;

	size = FS_LoadFile(path, &fbuf);

	if (!fbuf) {
		Com_Printf("Could not load shader %s\n", path);
		return -1;
	}

	if (size < 16) {
		Com_Printf("Could not load invalid shader with size %i: %s\n", size, path);
		FS_FreeFile(fbuf);
		return -1;
	}

	qglEnable(GL_VERTEX_PROGRAM_ARB);
	qglGenProgramsARB(1, (unsigned int*)&vpid);
	qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpid);
	qglProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, size, *(const char**)&fbuf);
	qglDisable(GL_VERTEX_PROGRAM_ARB);

	errors = qglGetString(GL_PROGRAM_ERROR_STRING_ARB);

	qglGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
	if (error_pos != -1) {
		Com_Printf("!! VP error at position %d in %s\n", error_pos, path);
		Com_Printf("!! Error: %s\n", errors);

		qglDeleteProgramsARB(1, (unsigned int*)&vpid);
		FS_FreeFile(fbuf);
		return 0;
	}

	FS_FreeFile(fbuf);
	Com_DPrintf(DEBUG_RENDERER, "...loaded vertex shader %s (pid: %i)\n", path, vpid);
	R_CheckError();
	return vpid;
}

/**
 * @brief Loads vertex program shader
 * @param[in] s The shader to load
 * @return vpid - id of shader
 */
static int SH_LoadProgram_GLSL (shader_t* s)
{
	byte *fbuf;
	int size;

	if (!r_state.glsl_program)
		return -1;

	size = FS_LoadFile(s->filename, &fbuf);

	if (!fbuf) {
		Com_Printf("Could not load shader %s\n", s->filename);
		return -1;
	}

	if (size < 16) {
		Com_Printf("Could not load invalid shader with size %i: %s\n", size, s->filename);
		FS_FreeFile(fbuf);
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

	FS_FreeFile(fbuf);
	Com_DPrintf(DEBUG_RENDERER, "...loaded glsl shader %s (pid: %i)\n", s->filename, s->glslpid);
	R_CheckError();
	return s->glslpid;
}

/**
 * @brief Activate fragment shaders
 * @param[in] fpid the shader id
 * @sa SH_LoadProgram_ARB_FP
 */
static void SH_UseProgram_ARB_FP (int fpid)
{
	if (fpid > 0) {
		qglEnable(GL_FRAGMENT_PROGRAM_ARB);
		qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpid);
	} else {
		qglDisable(GL_FRAGMENT_PROGRAM_ARB);
	}
	R_CheckError();
}

/**
 * @brief Activate vertex shader
 * @param[in] vpid the shader id
 * @sa SH_LoadProgram_ARB_VP
 */
static void SH_UseProgram_ARB_VP (int vpid)
{
	if (vpid > 0) {
		qglEnable(GL_VERTEX_PROGRAM_ARB);
		qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpid);
	} else {
		qglDisable(GL_VERTEX_PROGRAM_ARB);
	}
	R_CheckError();
}

/**
 * @brief Use a shader if arg fragment program are supported
 * @sa SH_UseProgram_ARB_FP
 * @sa SH_UseProgram_ARB_VP
 * @param[in] shader Shader pointer (see image_t)
 */
void SH_UseShader (shader_t * shader, qboolean activate)
{
	if (!r_state.arb_fragment_program || !shader)
		return;

	assert(shader);

	if (shader->glslpid > 0) {
		if (activate) {
			qglUseProgram(shader->glslpid);
		} else {
			qglUseProgram(0);
		}
	}
	if (shader->fpid > 0) {
		if (activate) {
			SH_UseProgram_ARB_FP(shader->fpid);
		} else {
			SH_UseProgram_ARB_FP(0);
		}
	}
	if (shader->vpid > 0) {
		if (activate) {
			SH_UseProgram_ARB_VP(shader->vpid);
		} else {
			SH_UseProgram_ARB_VP(0);
		}
	}
}

/**
 * @brief Set some shader values in the current active shader
 * @param[in] index The index of the local parameter for the shader
 * @param[in] p The list of values
 */
void R_ShaderFragmentParameter (unsigned int index, float *p)
{
	if (!r_state.arb_fragment_program)
		return;

	qglProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, index, p);
}
