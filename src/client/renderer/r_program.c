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

		if (v->type == type && !strcmp(v->name, name))
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
		Com_Printf("R_ProgramVariable: Could not find parameter in program %s.\n", r_state.active_program->name);
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
	memset(sh, 0, sizeof(*sh));
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

	memset(prog, 0, sizeof(*prog));
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

static size_t R_PreprocessShaderAddToShaderBuf (const char *name, const char *in, char **out, size_t *len)
{
	const size_t inLength = strlen(in);
	strcpy(*out, in);
	*out += inLength;
	*len -= inLength;
	return inLength;
}

static size_t R_InitializeShader (const char *name, char *out, size_t len)
{
	size_t i;
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

	i = 0;

	defines = "#version 110\n";
	i += R_PreprocessShaderAddToShaderBuf(name, defines, &out, &len);
	defines = va("#ifndef r_width\n#define r_width %f\n#endif\n", (float)viddef.width);
	i += R_PreprocessShaderAddToShaderBuf(name, defines, &out, &len);
	defines = va("#ifndef r_height\n#define r_height %f\n#endif\n", (float)viddef.height);
	i += R_PreprocessShaderAddToShaderBuf(name, defines, &out, &len);

	if (hwHack)
		i += R_PreprocessShaderAddToShaderBuf(name, hwHack, &out, &len);

	return i;
}

static size_t R_PreprocessShader (const char *name, const char *in, char *out, int len)
{
	byte *buffer;
	size_t i = 0;

	/** @todo (arisian): don't GLSL compilers have built-in preprocessors that can handle this kind of stuff? */
	while (*in) {
		if (!strncmp(in, "#include", 8)) {
			char path[MAX_QPATH];
			byte *buf;
			size_t inc_len;
			in += 8;
			Com_sprintf(path, sizeof(path), "shaders/%s", Com_Parse(&in));

			if (FS_LoadFile(path, &buf) == -1) {
				Com_Printf("Failed to resolve #include: %s.\n", path);
				continue;
			}

			inc_len = R_PreprocessShader(name, (const char *)buf, out, len);
			len -= inc_len;
			out += inc_len;
			FS_FreeFile(buf);
		}

		if (!strncmp(in, "#if", 3)) {  /* conditionals */
			float f;
			qboolean elseclause = qfalse;

			in += 3;

			f = Cvar_GetValue(Com_Parse(&in));

			while (*in) {
				if (!strncmp(in, "#endif", 6)) {
					in += 6;
					break;
				}

				if (!strncmp(in, "#else", 5)) {
					in += 5;
					elseclause = qtrue;
				}

				len--;
				if (len < 0) {
					Com_Error(ERR_DROP, "R_PreprocessShader: Overflow: %s", name);
				}

				if ((f && !elseclause) || (!f && elseclause)) {
					if (!strncmp(in, "#unroll", 7)) {  /* loop unrolling */
						int j, z;
						size_t subLength = 0;

						buffer = (byte *)Mem_PoolAlloc(SHADER_BUF_SIZE, vid_imagePool, 0);

						in += 7;
						z = Cvar_GetValue(Com_Parse(&in));

						while (*in) {
							if (!strncmp(in, "#endunroll", 10)) {
								in += 10;
								break;
							}

							buffer[subLength++] = *in++;

							if (subLength >= SHADER_BUF_SIZE)
								Com_Error(ERR_FATAL, "R_PreprocessShader: Overflow in shader loading '%s'", name);
						}

						for (j = 0; j < z; j++) {
							int l;
							for (l = 0; l < subLength; l++) {
								if (buffer[l] == '$') {
									Com_sprintf(out, subLength - l, "%d", j);
									out += (j / 10) + 1;
									i += (j / 10) + 1;
									len -= (j / 10) + 1;
								} else {
									*out++ = buffer[l];
									i++;
									len--;
								}
								if (len < 0)
									Com_Error(ERR_FATAL, "R_PreprocessShader: Overflow in shader loading '%s'", name);
							}
						}

						Mem_Free(buffer);
					} else {
						*out++ = *in++;
						i++;
					}
				} else
					in++;
			}

			if (!*in) {
				Com_Error(ERR_DROP, "R_PreprocessShader: Unterminated conditional: %s", name);
			}
		}


		if (!strncmp(in, "#unroll", 7)) {  /* loop unrolling */
			int j, z;
			size_t subLength = 0;

			buffer = (byte *)Mem_PoolAlloc(SHADER_BUF_SIZE, vid_imagePool, 0);

			in += 7;
			z = Cvar_GetValue(Com_Parse(&in));

			while (*in) {
				if (!strncmp(in, "#endunroll", 10)) {
					in += 10;
					break;
				}

				buffer[subLength++] = *in++;
				if (subLength >= SHADER_BUF_SIZE)
					Com_Error(ERR_FATAL, "R_PreprocessShader: Overflow in shader loading '%s'", name);
			}

			for (j = 0; j < z; j++) {
				int l;
				for (l = 0; l < subLength; l++) {
					if (buffer[l] == '$') {
						Com_sprintf(out, subLength - l, "%d", j);
						out += (j / 10) + 1;
						i += (j / 10) + 1;
						len -= (j / 10) + 1;
					} else {
						*out++ = buffer[l];
						i++;
						len--;
					}
					if (len < 0)
						Com_Error(ERR_FATAL, "R_PreprocessShader: Overflow in shader loading '%s'", name);
				}
			}

			Mem_Free(buffer);
		}

		if (!strncmp(in, "#replace", 8)) {
			int r;
			in += 8;
			r = Cvar_GetValue(Com_Parse(&in));
			Com_sprintf(out, len, "%d", r);
			out += (r / 10) + 1;
			len -= (r / 10) + 1;
		}

		/* general case is to copy so long as the buffer has room */

		len--;
		if (len < 0)
			Com_Error(ERR_FATAL, "R_PreprocessShader: Overflow in shader loading '%s'", name);
		*out++ = *in++;
		i++;
	}
	return i;
}


static r_shader_t *R_LoadShader (GLenum type, const char *name)
{
	r_shader_t *sh;
	char path[MAX_QPATH], *src[1];
	unsigned e, len, length[1];
	char *source, *srcBuf;
	byte *buf;
	int i;
	size_t bufLength = SHADER_BUF_SIZE;
	size_t initializeLength;

	snprintf(path, sizeof(path), "shaders/%s", name);

	if ((len = FS_LoadFile(path, &buf)) == -1) {
		Com_DPrintf(DEBUG_RENDERER, "R_LoadShader: Failed to load %s.\n", name);
		return NULL;
	}

	srcBuf = source = (char *)Mem_PoolAlloc(bufLength, vid_imagePool, 0);

	initializeLength = R_InitializeShader(name, srcBuf, bufLength);
	srcBuf += initializeLength;
	bufLength -= initializeLength;

	R_PreprocessShader(name, (const char *)buf, srcBuf, bufLength);
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
	if (!e) {
		char log[MAX_STRING_CHARS];
		qglGetShaderInfoLog(sh->id, sizeof(log) - 1, NULL, log);
		Com_Printf("R_LoadShader: %s: %s\n", sh->name, log);

		qglDeleteShader(sh->id);
		memset(sh, 0, sizeof(*sh));

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

		if (!strcmp(prog->name, name))
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

	R_ProgramParameter1f("BUMP", 1.0);
	R_ProgramParameter1f("PARALLAX", 1.0);
	R_ProgramParameter1f("HARDNESS", 0.2);
	R_ProgramParameter1f("SPECULAR", 1.0);
	if (r_postprocess->integer)
		R_ProgramParameter1f("GLOWSCALE", 1.0);
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

	memset(r_state.shaders, 0, sizeof(r_state.shaders));
	memset(r_state.programs, 0, sizeof(r_state.programs));

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
	Com_Printf("glsl restart\n");

	R_ShutdownPrograms();
	R_InitPrograms();
}
