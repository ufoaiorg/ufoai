/**
 * @file r_program.c
 * @brief Shader (GLSL) backend functions
 */

/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "r_local.h"
#include "r_error.h"
#include "r_program.h"
#include "../../shared/parse.h"
#include "../../shared/shared.h"


#define SHADER_BUF_SIZE 16384

void R_UseProgram  (r_program_t *prog)
{
	if (!qglUseProgram || r_state.active_program == prog)
		return;

	r_state.active_program = prog;

	if (prog) {
		qglUseProgram(prog->id);

		if (prog->use)  /* invoke use function */
			prog->use(prog);
	} else {
		qglUseProgram(0);
	}
}

static r_progvar_t *R_ProgramVariable (int type, const char *name)
{
	r_progvar_t *v;
	int i;

	if (!r_state.active_program) {
		Com_DPrintf(DEBUG_RENDERER, "R_ProgramVariable: \"%s\" - No program bound.\n", name);
		return NULL;
	}

	/* find the variable */
	for (i = 0; i < MAX_PROGRAM_VARS; i++) {
		v = &r_state.active_program->vars[i];

		if (!v->location)
			break;

		if (v->type == type && Q_streq(v->name, name))
			return v;
	}

	if (i == MAX_PROGRAM_VARS) {
		Com_Printf("R_ProgramVariable: MAX_PROGRAM_VARS reached.\n");
		return NULL;
	}

	/* or query for it */
	if (type == GL_UNIFORM)
		v->location = qglGetUniformLocation(r_state.active_program->id, name);
	else
		v->location = qglGetAttribLocation(r_state.active_program->id, name);

	if (v->location == -1) {
		Com_Printf("R_ProgramVariable: Could not find parameter %s in program %s.\n", name, r_state.active_program->name);
		v->location = 0;
		return NULL;
	}

	v->type = type;
	Q_strncpyz(v->name, name, sizeof(v->name));

	return v;
}

void R_ProgramParameter1i (const char *name, GLint value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform1i(v->location, value);
}

void R_ProgramParameter1f (const char *name, GLfloat value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform1f(v->location, value);
}

void R_ProgramParameter1fvs (const char *name, GLint size, GLfloat *value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform1fv(v->location, size, value);
}

void R_ProgramParameter2fv (const char *name, GLfloat *value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform2fv(v->location, 1, value);
}

void R_ProgramParameter2fvs (const char *name, GLint size, GLfloat *value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform2fv(v->location, size, value);
}

void R_ProgramParameter3fv (const char *name, GLfloat *value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform3fv(v->location, 1, value);
}

void R_ProgramParameter4fv (const char *name, GLfloat *value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform4fv(v->location, 1, value);
}

void R_ProgramParameterMatrix4fv (const char *name, GLfloat *value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniformMatrix4fv(v->location, 1, GL_FALSE, value);
}

void R_AttributePointer (const char *name, GLuint size, const GLvoid *array)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_ATTRIBUTE, name)))
		return;

	qglVertexAttribPointer(v->location, size, GL_FLOAT, GL_FALSE, 0, array);
}

void R_EnableAttribute (const char *name)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_ATTRIBUTE, name)))
		return;

	qglEnableVertexAttribArray(v->location);
}

void R_DisableAttribute (const char *name)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_ATTRIBUTE, name)))
		return;

	qglDisableVertexAttribArray(v->location);
}

static void R_ShutdownShader (r_shader_t *sh)
{
	qglDeleteShader(sh->id);
	OBJZERO(*sh);
}

static void R_ShutdownProgram (r_program_t *prog)
{
	if (prog->v) {
		qglDetachShader(prog->id, prog->v->id);
		R_ShutdownShader(prog->v);
		R_CheckError();
	}
	if (prog->f) {
		qglDetachShader(prog->id, prog->f->id);
		R_ShutdownShader(prog->f);
		R_CheckError();
	}

	qglDeleteProgram(prog->id);

	OBJZERO(*prog);
}

void R_ShutdownPrograms (void)
{
	int i;

	if (!qglDeleteProgram)
		return;

	if (!r_programs->integer)
		return;

	for (i = 0; i < MAX_PROGRAMS; i++) {
		if (!r_state.programs[i].id)
			continue;

		R_ShutdownProgram(&r_state.programs[i]);
	}
}

/**
 * @brief Prefixes shader string (out) with in.
 * @param[in] name The name of the shader.
 * @param[in] in The string to prefix onto the shader string (out).
 * @param[in,out] out The shader string (initially was the whole shader file).
 * @param[in,out] len The amount of space left in the buffer pointed to by *out.
 * @return strlen(in)
 */
static size_t R_PreprocessShaderAddToShaderBuf (const char *name, const char *in, char **out, size_t *len)
{
	const size_t inLength = strlen(in);
	strcpy(*out, in);
	*out += inLength;
	*len -= inLength;
	return inLength;
}

/**
 * @brief Prefixes the shader string with user settings and the video hardware manufacturer.
 *
 * The shader needs to know the rendered width & height, the glsl version, and
 * the hardware manufacturer.
 * @param[in] name The name of the shader.
 * @param[in,out] out
 * @param[in,out] len The amount of space left in the buffer pointed to by *out.
 * @return The number of characters prefixed to the shader string.
 */
static size_t R_InitializeShader (const char *name, char *out, size_t len)
{
	size_t initialChars = 0;
	const char *hwHack, *defines;

	switch (r_config.hardwareType) {
	case GLHW_ATI:
		hwHack = "#ifndef ATI\n#define ATI\n#endif\n";
		break;
	case GLHW_INTEL:
		hwHack = "#ifndef INTEL\n#define INTEL\n#endif\n";
		break;
	case GLHW_NVIDIA:
		hwHack = "#ifndef NVIDIA\n#define NVIDIA\n#endif\n";
		break;
	case GLHW_GENERIC:
	case GLHW_MESA:
		hwHack = NULL;
		break;
	default:
		Com_Error(ERR_FATAL, "R_PreprocessShader: Unknown hardwaretype");
	}

	/*
	 * Prefix "#version xxx" onto shader string.
	 * This causes GLSL compiler to compile to that version.
	 */
	defines = va("#version %d\n", r_glsl_version->integer);
	initialChars += R_PreprocessShaderAddToShaderBuf(name, defines, &out, &len);

	/*
	 * Prefix "#define glslxxx" onto shader string.
	 * This named constant is used to setup shader code to match the desired GLSL spec.
	 */
	defines = va("#define glsl%d\n", r_glsl_version->integer);
	initialChars += R_PreprocessShaderAddToShaderBuf(name, defines, &out, &len);

	/* Define r_width.*/
	defines = va("#ifndef r_width\n#define r_width %f\n#endif\n", (float)viddef.context.width);
	initialChars += R_PreprocessShaderAddToShaderBuf(name, defines, &out, &len);

	/* Define r_height.*/
	defines = va("#ifndef r_height\n#define r_height %f\n#endif\n", (float)viddef.context.height);
	initialChars += R_PreprocessShaderAddToShaderBuf(name, defines, &out, &len);

	if (hwHack) {
		initialChars += R_PreprocessShaderAddToShaderBuf(name, hwHack, &out, &len);
	}

	return initialChars;
}

/**
 * @brief Do our own preprocessing to the shader file, before the
 * GLSL implementation calls it's preprocessor.
 *
 * #if/#endif pairs, #unroll, #endunroll, #include, #replace are handled by our
 * preprocessor, not the GLSL implementation's preprocessor (except #include which may
 * also be handled by the implementation's preprocessor).  #if operates off
 * of the value of a cvar interpreted as a bool. Note the GLSL implementation
 * preprocessor handles #ifdef and #ifndef, not #if.
 * @param[in] name The file name of the shader (e.g. "world_fs.glsl").
 * @param[in] in The non-preprocessed shader string.
 * @param[in,out] out The preprocessed shader string, NULL if we don't want to write to it.
 * @param[in,out] remainingOutChars The number of characters left in the out buffer.
 * @param[in] terminateAtEndIf If true, return when we hit an #endif; if false, its an error if an #endif was encountered.
 * @param[in] writeEndifToOut If true, write the #endif to out.
 * @param[in] dontWriteElseClaseToOut If true, don't write the #else/#endif clause to out, just parse over it.
 * @param[in] dontParseInOverEndIf If true, don't move in over #endif.
 * @param[in] dontParseInOverElse If true, don't move in over #else.
 * @return The number of characters added to the buffer pointed to by out.
 */
static size_t R_PreprocessShaderR (const char *name, const char **inPtr, char *out, size_t *remainingOutChars, qboolean terminateAtEndIf,
		qboolean writeEndifToOut, qboolean dontWriteElseClaseToOut, qboolean dontParseInOverEndIf, qboolean dontParseInOverElse)
{
	const size_t INITIAL_REMAINING_OUT_CHARS = *remainingOutChars;
	/* Keep looping till we reach the end of the shader string, or a parsing error.*/
	while (**inPtr) {
		if ('#' == **inPtr) {
			(*inPtr)++;
			if (!strncmp(*inPtr, "endif", 5)) {
				if (!terminateAtEndIf) {
					/* Error in shader! Print a message saying our preprocessor failed parsing.*/
					Com_Error(ERR_DROP, "R_PreprocessShaderR: Unmatched #endif: %s", name);
				}
				/* Write the #endif, it goes into the out buffer since it does not correspond with an "#if".*/
				if (out && writeEndifToOut) {
					(*remainingOutChars) -= 6;
					if (*remainingOutChars <= 0)
						Com_Error(ERR_FATAL, "R_PreprocessShaderR: Overflow in shader loading '%s'", name);
					/* Write the #.*/
					*out++ = '#';
					/* Write the 'endif'.*/
					strncpy(out, *inPtr, 5);
					out += 5;
				}
				if (!dontParseInOverEndIf) {
					/* Only advance over the endif if we are not in an else clause, let the parent call do that.*/
					(*inPtr) += 5;
				} else {
					/* Move back to the starting '#', the parent call wants to see this token.*/
					(*inPtr)--;
				}
				return (INITIAL_REMAINING_OUT_CHARS - *remainingOutChars);
			} else if (dontWriteElseClaseToOut && !strncmp((*inPtr), "else", 4)) {
				if (dontParseInOverElse) {
					/* This else is in a structure with a '#if' that was false.*/
					/* Move back to the starting '#', the parent call wants to see this token.*/
					(*inPtr)--;
				} else {
					/* This else is in a structure with a '#if' that was true.*/
					/* Don't copy else into out.*/
					(*inPtr) += 4;
					/* Don't modify out.*/
					R_PreprocessShaderR(name, inPtr, (char*)0, remainingOutChars, qtrue, qfalse, qtrue, qtrue, qfalse);
				}
			} else if (!strncmp((*inPtr), "if ", 3)) {
				/* The line looks like "#if r_postprocess".*/
				float f = 0.0f;
				(*inPtr) += 3;
				/* Get the corresponding cvar value.*/
				f = Cvar_GetValue(Com_Parse(inPtr));
				if (f) {
					if (out) {
						out += R_PreprocessShaderR(name, inPtr, out, remainingOutChars, qtrue, qfalse, qtrue, qfalse, qfalse);
					} else {
						R_PreprocessShaderR(name, inPtr, out, remainingOutChars, qtrue, qfalse, qtrue, qfalse, qfalse);
					}
				} else {
					/* The cvar was false, don't add to out. Lets look and see if we hit a #else, or #endif.*/
					R_PreprocessShaderR(name, inPtr, (char*)0, remainingOutChars, qtrue, qfalse, qfalse, qtrue, qtrue);
					if (!strncmp((*inPtr), "#endif", 6)) {
						/* All right, parse over it, don't write to out.*/
						(*inPtr) += 6;
					} else if (!strncmp((*inPtr), "#else", 5)) {
						/* All right, we want to add this to out.*/
						(*inPtr)++;
						out += R_PreprocessShaderR(name, inPtr, out, remainingOutChars, qtrue, qfalse, qfalse, qfalse, qfalse);
					}
				}
			} else if (!strncmp((*inPtr), "ifndef", 6)) {
				if (out) {
					if (*remainingOutChars <= 0)
						Com_Error(ERR_FATAL, "R_PreprocessShaderR: Overflow in shader loading '%s'", name);
					*out++ = '#';
					(*remainingOutChars)--;
					out += R_PreprocessShaderR(name, inPtr, out, remainingOutChars, qtrue, qtrue, qfalse, qfalse, qfalse);
				} else {
					R_PreprocessShaderR(name, inPtr, out, remainingOutChars, qtrue, qtrue, qfalse, qfalse, qfalse);
				}
			} else if (!strncmp((*inPtr), "ifdef", 5)) {
				if (out) {
					if (*remainingOutChars <= 0)
						Com_Error(ERR_FATAL, "R_PreprocessShaderR: Overflow in shader loading '%s'", name);
					*out++ = '#';
					(*remainingOutChars)--;
					out += R_PreprocessShaderR(name, inPtr, out, remainingOutChars, qtrue, qtrue, qfalse, qfalse, qfalse);
				} else {
					R_PreprocessShaderR(name, inPtr, out, remainingOutChars, qtrue, qtrue, qfalse, qfalse, qfalse);
				}
			} else if (!strncmp((*inPtr), "include", 7)) {
				char path[MAX_QPATH];
				byte *buf = (byte*)0;
				const char* bufAsChar = (const char*)0;
				const char** bufAsCharPtr = (const char**)0;
				(*inPtr) += 8;
				Com_sprintf(path, sizeof(path), "shaders/%s", Com_Parse(inPtr));
				if (FS_LoadFile(path, &buf) == -1) {
					Com_Printf("Failed to resolve #include: %s.\n", path);
					continue;
				}
				bufAsChar = (const char*)buf;
				bufAsCharPtr = &bufAsChar;
				if (out) {
					out += R_PreprocessShaderR(name, bufAsCharPtr, out, remainingOutChars, qfalse, qfalse, qfalse, qfalse, qfalse);
				} else {
					R_PreprocessShaderR(name, bufAsCharPtr, out, remainingOutChars, qfalse, qfalse, qfalse, qfalse, qfalse);
				}
				FS_FreeFile(buf);
			} else if (!strncmp((*inPtr), "unroll", 6)) {
				/* loop unrolling */
				int j = 0,
					z = 0;
				size_t subLength = 0;
				byte* buffer = (byte*)Mem_PoolAlloc(SHADER_BUF_SIZE, vid_imagePool, 0);
				(*inPtr) += 6;
				z = Cvar_GetValue(Com_Parse(inPtr));
				while (*(*inPtr)) {
					if (!strncmp((*inPtr), "#endunroll", 10)) {
						(*inPtr) += 10;
						break;
					}
					buffer[subLength++] = *(*inPtr)++;
					if (subLength >= SHADER_BUF_SIZE)
						Com_Error(ERR_FATAL, "R_PreprocessShaderR: Overflow in shader loading '%s'", name);
				}
				if (out) {
					for (; j < z; j++) {
						int l = 0;
						for (; l < subLength; l++) {
							if (buffer[l] == '$') {
								byte insertedLen = (j / 10) + 1;
								if (!Com_sprintf(out, *remainingOutChars, "%d", j))
									Com_Error(ERR_FATAL, "R_PreprocessShaderR: Overflow in shader loading '%s'", name);
								out += insertedLen;
								(*remainingOutChars) -= insertedLen;
							} else {
								if (*remainingOutChars <= 0)
									Com_Error(ERR_FATAL, "R_PreprocessShaderR: Overflow in shader loading '%s'", name);
								*out++ = buffer[l];
								(*remainingOutChars)--;
							}
						}

						Mem_Free(buffer);
					}
				}
			} else if (!strncmp((*inPtr), "replace", 7)) {
				int r = 0;
				byte insertedLen = 0;
				(*inPtr) += 8;
				r = Cvar_GetValue(Com_Parse(inPtr));
				if (out) {
					if (!Com_sprintf(out, *remainingOutChars, "%d", r))
						Com_Error(ERR_FATAL, "R_PreprocessShaderR: Overflow in shader loading '%s'", name);
					insertedLen = (r / 10) + 1;
					out += insertedLen;
					(*remainingOutChars) -= insertedLen;
				}
			} else {
				/* general case is to copy so long as the buffer has room */
				if (out) {
					if (*remainingOutChars <= 0)
						Com_Error(ERR_FATAL, "R_PreprocessShaderR: Overflow in shader loading '%s'", name);
					*out++ = '#';
					(*remainingOutChars)--;
				}
			}
		} else {
			/* general case is to copy so long as the buffer has room */
			if (out) {
				if (*remainingOutChars <= 0)
					Com_Error(ERR_FATAL, "R_PreprocessShaderR: Overflow in shader loading '%s'", name);
				*out++ = *(*inPtr);
				(*remainingOutChars)--;
			}
			(*inPtr)++;
		}
	}
	/* Return the number of characters added to the buffer.*/
	return (INITIAL_REMAINING_OUT_CHARS - *remainingOutChars);
}

/**
 * @brief Do our own preprocessing to the shader file, before the
 * GLSL implementation calls it's preprocessor.
 *
 * #if/#endif pairs, #unroll, #endunroll, #include, #replace are handled by our
 * preprocessor, not the GLSL implementation's preprocessor (except #include which may
 * also be handled by the implementation's preprocessor).  #if operates off
 * of the value of a cvar interpreted as a bool. Note the GLSL implementation
 * preprocessor handles #ifdef and #ifndef, not #if.
 * @param[in] name The file name of the shader (e.g. "world_fs.glsl").
 * @param[in] in The non-preprocessed shader string.
 * @param[in,out] out The preprocessed shader string, NULL if we don't want to write to it.
 * @param[in,out] remainingOutChars The number of characters left in the out buffer.
 * @return The number of characters added to the buffer pointed to by out.
 */
static size_t R_PreprocessShader (const char *name, const char *in, char *out, size_t *remainingOutChars)
{
	return R_PreprocessShaderR(name, &in, out, remainingOutChars, qfalse, qfalse, qfalse, qfalse, qfalse);
}

/**
 * @brief Reads/Preprocesses/Compiles the specified shader into a program.
 * @param[in] type The type of shader, currently either GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
 * @param[in] name The file name of the shader to load from ./base/shaders/ (e.g. "world_fs.glsl").
 * @return A structure used as a handle to the compiled shader (program).
 */
static r_shader_t *R_LoadShader (const GLenum type, const char *name)
{
	r_shader_t *sh;
	char path[MAX_QPATH], *src[1];
	unsigned e, length[1];
	char *source, *srcBuf;
	byte *buf;
	int i;
	size_t bufLength = SHADER_BUF_SIZE;
	size_t initializeLength;

#ifdef DEBUG
	/* Used to contain result of shader compile.*/
	char log[MAX_STRING_CHARS];
#endif

	snprintf(path, sizeof(path), "shaders/%s", name);

	if (FS_LoadFile(path, &buf) == -1) {
		Com_DPrintf(DEBUG_RENDERER, "R_LoadShader: Failed to load ./base/shaders/%s.\n", name);
		return NULL;
	}

	Com_DPrintf(DEBUG_RENDERER, "R_LoadShader: Loading ./base/shaders/%s.\n", name);

	srcBuf = source = (char *)Mem_PoolAlloc(bufLength, vid_imagePool, 0);

	initializeLength = R_InitializeShader(name, srcBuf, bufLength);
	srcBuf += initializeLength;
	bufLength -= initializeLength;

	R_PreprocessShader(name, (const char *)buf, srcBuf, &bufLength);
	FS_FreeFile(buf);

	src[0] = source;
	length[0] = strlen(source);

	for (i = 0; i < MAX_SHADERS; i++) {
		sh = &r_state.shaders[i];

		if (!sh->id)
			break;
	}

	if (i == MAX_SHADERS) {
		Com_Printf("R_LoadShader: MAX_SHADERS reached.\n");
		Mem_Free(source);
		return NULL;
	}

	Q_strncpyz(sh->name, name, sizeof(sh->name));

	sh->type = type;

	sh->id = qglCreateShader(sh->type);
	if (!sh->id)
		return NULL;

	/* upload the shader source */
	qglShaderSource(sh->id, 1, src, length);

	/* compile it and check for errors */
	qglCompileShader(sh->id);

	Mem_Free(source);

	qglGetShaderiv(sh->id, GL_COMPILE_STATUS, &e);
#ifdef DEBUG
	qglGetShaderInfoLog(sh->id, sizeof(log) - 1, NULL, log);
	Com_Printf("R_LoadShader: %s: %s", sh->name, log);
#endif
	if (!e) {
#ifndef DEBUG
		char log[MAX_STRING_CHARS];
		qglGetShaderInfoLog(sh->id, sizeof(log) - 1, NULL, log);
		Com_Printf("R_LoadShader: %s: %s", sh->name, log);
#endif

		qglDeleteShader(sh->id);
		OBJZERO(*sh);

		return NULL;
	}

	return sh;
}

r_program_t *R_LoadProgram (const char *name, programInitFunc_t init, programUseFunc_t use)
{
	r_program_t *prog;
	unsigned e;
	int i;

	/* search existing one */
	for (i = 0; i < MAX_PROGRAMS; i++) {
		prog = &r_state.programs[i];

		if (Q_streq(prog->name, name))
			return prog;
	}

	/* search free slot */
	for (i = 0; i < MAX_PROGRAMS; i++) {
		prog = &r_state.programs[i];

		if (!prog->id)
			break;
	}

	if (i == MAX_PROGRAMS) {
		Com_Printf("R_LoadProgram: MAX_PROGRAMS reached.\n");
		return NULL;
	}

	Q_strncpyz(prog->name, name, sizeof(prog->name));

	prog->id = qglCreateProgram();

	prog->v = R_LoadShader(GL_VERTEX_SHADER, va("%s_vs.glsl", name));
	prog->f = R_LoadShader(GL_FRAGMENT_SHADER, va("%s_fs.glsl", name));

	if (prog->v)
		qglAttachShader(prog->id, prog->v->id);
	if (prog->f)
		qglAttachShader(prog->id, prog->f->id);

	qglLinkProgram(prog->id);

	qglGetProgramiv(prog->id, GL_LINK_STATUS, &e);
	if (!e || !prog->v || !prog->f) {
		char log[MAX_STRING_CHARS];
		qglGetProgramInfoLog(prog->id, sizeof(log) - 1, NULL, log);
		Com_Printf("R_LoadProgram: %s: %s\n", prog->name, log);

		R_ShutdownProgram(prog);
		return NULL;
	}

	prog->init = init;

	if (prog->init) {  /* invoke initialization function */
		R_UseProgram(prog);

		prog->init(prog);

		R_UseProgram(NULL);
	}

	prog->use = use;

	Com_Printf("R_LoadProgram: '%s' loaded.\n", name);

	return prog;
}

static void R_InitWorldProgram (r_program_t *prog)
{
	R_ProgramParameter1i("SAMPLER0", 0);
	R_ProgramParameter1i("SAMPLER1", 1);
	R_ProgramParameter1i("SAMPLER2", 2);
	R_ProgramParameter1i("SAMPLER3", 3);
	if (r_postprocess->integer)
		R_ProgramParameter1i("SAMPLER4", 4);

	R_ProgramParameter1i("BUMPMAP", 0);
	R_ProgramParameter1i("ROUGHMAP", 0);
	R_ProgramParameter1i("SPECULARMAP", 0);
	R_ProgramParameter1i("ANIMATE", 0);

	R_ProgramParameter1f("HARDNESS", defaultMaterial.hardness);
	R_ProgramParameter1f("SPECULAR", defaultMaterial.specular);
	R_ProgramParameter1f("BUMP", defaultMaterial.bump);
	R_ProgramParameter1f("PARALLAX", defaultMaterial.parallax);
	if (r_postprocess->integer)
		R_ProgramParameter1f("GLOWSCALE", defaultMaterial.glowscale);
}

static void R_UseWorldProgram (r_program_t *prog)
{
	/*R_ProgramParameter1i("LIGHTS", refdef.numLights);*/
}

static void R_InitWarpProgram (r_program_t *prog)
{
	static vec4_t offset;

	R_ProgramParameter1i("SAMPLER0", 0);
	R_ProgramParameter1i("SAMPLER1", 1);
	if (r_postprocess->integer) {
		R_ProgramParameter1i("SAMPLER4", 4);
		R_ProgramParameter1f("GLOWSCALE", 0.0);
	}
	R_ProgramParameter4fv("OFFSET", offset);
}

static void R_UseWarpProgram (r_program_t *prog)
{
	static vec4_t offset;

	offset[0] = offset[1] = refdef.time / 8.0;
	R_ProgramParameter4fv("OFFSET", offset);
}

static void R_InitGeoscapeProgram (r_program_t *prog)
{
	static vec4_t defaultColor = {0.0, 0.0, 0.0, 1.0};
	static vec4_t cityLightColor = {1.0, 1.0, 0.8, 1.0};
	static vec2_t uvScale = {2.0, 1.0};

	R_ProgramParameter1i("SAMPLER0", 0);
	R_ProgramParameter1i("SAMPLER1", 1);
	R_ProgramParameter1i("SAMPLER2", 2);

	R_ProgramParameter4fv("DEFAULTCOLOR", defaultColor);
	R_ProgramParameter4fv("CITYLIGHTCOLOR", cityLightColor);
	R_ProgramParameter2fv("UVSCALE", uvScale);
}

/**
 * @note this is a not-terribly-efficient recursive implementation,
 * but it only happens once and shouldn't have to go very deep.
 */
static int R_PascalTriangle (int row, int col)
{
	if (row <= 1 || col <= 1 || col >= row)
		return 1;
	return R_PascalTriangle(row - 1, col) + R_PascalTriangle(row - 1, col - 1);
}

/** @brief width of convolution filter (for blur/bloom effects) */
#define FILTER_SIZE 3

static void R_InitConvolveProgram (r_program_t *prog)
{
	float filter[FILTER_SIZE];
	float sum = 0;
	int i;
	const size_t size = lengthof(filter);

	/* approximate a Gaussian by normalizing the Nth row of Pascale's Triangle */
	for (i = 0; i < size; i++) {
		filter[i] = (float)R_PascalTriangle(size, i + 1);
		sum += filter[i];
	}

	for (i = 0; i < size; i++)
		filter[i] = (filter[i] / sum);

	R_ProgramParameter1i("SAMPLER0", 0);
	R_ProgramParameter1fvs("COEFFICIENTS", size, filter);
}

/**
 * @brief Use the filter convolution glsl program
 */
static void R_UseConvolveProgram (r_program_t *prog)
{
	int i;
	const float *userdata= (float *)prog->userdata;
	float offsets[FILTER_SIZE * 2];
	const float halfWidth = (FILTER_SIZE - 1) * 0.5;
	const float offset = 1.2f / userdata[0];
	const float x = userdata[1] * offset;

	for (i = 0; i < FILTER_SIZE; i++) {
		const float y = (float)i - halfWidth;
		const float z = x * y;
		offsets[i * 2 + 0] = offset * y - z;
		offsets[i * 2 + 1] = z;
	}
	R_ProgramParameter2fvs("OFFSETS", FILTER_SIZE, offsets);
}

static void R_InitCombine2Program (r_program_t *prog)
{
	GLfloat defaultColor[4] = {0.0, 0.0, 0.0, 0.0};

	R_ProgramParameter1i("SAMPLER0", 0);
	R_ProgramParameter1i("SAMPLER1", 1);

	R_ProgramParameter4fv("DEFAULTCOLOR", defaultColor);
}

static void R_InitAtmosphereProgram (r_program_t *prog)
{
	static vec4_t defaultColor = {0.0, 0.0, 0.0, 1.0};
	static vec2_t uvScale = {2.0, 1.0};

	R_ProgramParameter1i("SAMPLER0", 0);
	R_ProgramParameter1i("SAMPLER2", 2);

	R_ProgramParameter4fv("DEFAULTCOLOR", defaultColor);
	R_ProgramParameter2fv("UVSCALE", uvScale);
}

static void R_InitSimpleGlowProgram (r_program_t *prog)
{
	R_ProgramParameter1i("SAMPLER0", 0);
	R_ProgramParameter1i("SAMPLER1", 4);
	R_ProgramParameter1f("GLOWSCALE", 1.0);
}

void R_InitParticleProgram (r_program_t *prog)
{
	R_ProgramParameter1i("SAMPLER0", 0);
}

void R_UseParticleProgram (r_program_t *prog)
{
/*	ptl_t *ptl = (ptl_t *)prog->userdata;*/
}

void R_InitPrograms (void)
{
	if (!qglCreateProgram) {
		Com_Printf("not using GLSL shaders\n");
		Cvar_Set("r_programs", "0");
		r_programs->modified = qfalse;
		return;
	}

	OBJZERO(r_state.shaders);
	OBJZERO(r_state.programs);

	/* shaders are deactivated - don't try to load them - some cards
	 * even have problems with this */
	if (!r_programs->integer)
		return;

	r_state.world_program = R_LoadProgram("world", R_InitWorldProgram, R_UseWorldProgram);
	r_state.warp_program = R_LoadProgram("warp", R_InitWarpProgram, R_UseWarpProgram);
	r_state.geoscape_program = R_LoadProgram("geoscape", R_InitGeoscapeProgram, NULL);
	r_state.combine2_program = R_LoadProgram("combine2", R_InitCombine2Program, NULL);
	r_state.convolve_program = R_LoadProgram("convolve" DOUBLEQUOTE(FILTER_SIZE), R_InitConvolveProgram, R_UseConvolveProgram);
	r_state.atmosphere_program = R_LoadProgram("atmosphere", R_InitAtmosphereProgram, NULL);
	r_state.simple_glow_program = R_LoadProgram("simple_glow", R_InitSimpleGlowProgram, NULL);
}

/**
 * @brief Reloads the glsl shaders
 */
void R_RestartPrograms_f (void)
{
	if (r_programs->integer) {
		Com_Printf("glsl restart to a version of v%s\n", Cvar_Get("r_glsl_version", NULL, 0, NULL)->string);
	} else {
		Com_Printf("glsl shutdown\n");
	}

	R_ShutdownPrograms();
	R_InitPrograms();
	R_InitFBObjects();
}
