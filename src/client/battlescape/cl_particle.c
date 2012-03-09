/**
 * @file cl_particle.c
 * @brief Client particle parsing and rendering functions.
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../client.h"
#include "cl_particle.h"
#include "cl_localentity.h"
#include "cl_hud.h"
#include "../renderer/r_light.h"
#include "../renderer/r_particle.h"
#include "../../shared/parse.h"

#define MAX_MAPPARTICLES	1024
#define MAX_TIMEDPARTICLES 16

#define PTL_INTENSITY_TO_RADIUS 256 /** @todo better scale factor: is 256 too big or not? */

static cvar_t *cl_particleweather;

/** @brief map particles */
typedef struct mapParticle_s {
	char ptl[MAX_VAR];
	const char *info;
	vec3_t origin;
	vec2_t wait;
	int nextTime;
	int levelflags;
} mapParticle_t;

typedef struct timedParticle_s {
	char ptl[MAX_VAR];
	vec3_t origin;
	vec3_t s;	/**< current position */
	vec3_t a;	/**< acceleration vector */
	vec3_t v;	/**< velocity vector */
	ptl_t *parent;
	qboolean children;	/**< spawn as children of @c parent */
	int levelFlags;
	int max;	/**< the amount of particles to spawn */
	int n;		/**< the amount of particles already spawned */
	int dt;		/**< the time delta between of the n particle spawns */
	int lastTime;	/**< the last time a particle from this queue was spawned */
} timedParticle_t;

static mapParticle_t mapParticles[MAX_MAPPARTICLES];
static timedParticle_t timedParticles[MAX_TIMEDPARTICLES];

static ptlArt_t r_particlesArt[MAX_PTL_ART];
static int r_numParticlesArt;

#define	V_VECS		((1 << V_FLOAT) | (1 << V_POS) | (1 << V_VECTOR) | (1 << V_COLOR))
#define PTL_ONLY_ONE_TYPE		(1<<31)
#define V_UNTYPED   0x7FFF

/** @brief particle functions enums - see pf_strings and pf_values */
typedef enum pf_s {
	PF_INIT,
	PF_RUN,
	PF_THINK,
	PF_ROUND,
	PF_PHYSICS,	/**< if a particle hits the ground */

	PF_NUM_PTLFUNCS
} pf_t;

/** @brief valid particle functions - see pf_t and pf_values */
static char const* const pf_strings[] = {
	"init",
	"run",
	"think",
	"round",
	"physics"
};
CASSERT(lengthof(pf_strings) == PF_NUM_PTLFUNCS);

/** @brief particle functions offsets - see pf_strings and pf_t */
static const size_t pf_values[] = {
	offsetof(ptlDef_t, init),
	offsetof(ptlDef_t, run),
	offsetof(ptlDef_t, think),
	offsetof(ptlDef_t, round),
	offsetof(ptlDef_t, physics)
};
CASSERT(lengthof(pf_values) == PF_NUM_PTLFUNCS);

/** @brief particle commands - see pc_strings */
typedef enum pc_s {
	PC_END,

	PC_PUSH, PC_POP, PC_KPOP,
	PC_ADD, PC_SUB,
	PC_MUL, PC_DIV,
	PC_SIN, PC_COS, PC_TAN,
	PC_RAND, PC_CRAND,
	PC_V2, PC_V3, PC_V4,

	PC_KILL,
	PC_SPAWN, PC_NSPAWN, PC_TNSPAWN, PC_CHILD,

	PC_NUM_PTLCMDS
} pc_t;

/** @brief particle commands - see pc_t */
static char const* const pc_strings[] = {
	"end",

	"push", "pop", "kpop",
	"add", "sub",
	"mul", "div",
	"sin", "cos", "tan",
	"rand", "crand",
	"v2", "v3", "v4",

	"kill",
	"spawn", "nspawn", "tnspawn", "child"
};
CASSERT(lengthof(pc_strings) == PC_NUM_PTLCMDS);

/** @brief particle commands parameter and types */
static const int pc_types[PC_NUM_PTLCMDS] = {
	0,

	V_UNTYPED, V_UNTYPED, V_UNTYPED,
	V_VECS, V_VECS,
	V_VECS, V_VECS,
	PTL_ONLY_ONE_TYPE | V_FLOAT, PTL_ONLY_ONE_TYPE | V_FLOAT, PTL_ONLY_ONE_TYPE | V_FLOAT,
	V_VECS, V_VECS,
	0, 0, 0,

	0,
	PTL_ONLY_ONE_TYPE | V_STRING, PTL_ONLY_ONE_TYPE | V_STRING, PTL_ONLY_ONE_TYPE | V_STRING, PTL_ONLY_ONE_TYPE | V_STRING
};
CASSERT(lengthof(pc_types) == PC_NUM_PTLCMDS);

/**
 * @brief particle script values
 * @note image, model and program are special values - see @c CL_ParticleFunction
 */
static const value_t pps[] = {
	{"image", V_STRING, offsetof(ptl_t, pic), 0},
	{"model", V_STRING, offsetof(ptl_t, model), 0},
	{"program", V_STRING, offsetof(ptl_t, program), 0},
	{"skin", V_INT, offsetof(ptl_t, skin), MEMBER_SIZEOF(ptl_t, skin)},
	{"blend", V_BLEND, offsetof(ptl_t, blend), MEMBER_SIZEOF(ptl_t, blend)},
	{"style", V_STYLE, offsetof(ptl_t, style), MEMBER_SIZEOF(ptl_t, style)},
	{"thinkfade", V_FADE, offsetof(ptl_t, thinkFade), MEMBER_SIZEOF(ptl_t, thinkFade)},
	{"framefade", V_FADE, offsetof(ptl_t, frameFade), MEMBER_SIZEOF(ptl_t, frameFade)},
	{"size", V_POS, offsetof(ptl_t, size), MEMBER_SIZEOF(ptl_t, size)},
	{"scale", V_VECTOR, offsetof(ptl_t, scale), MEMBER_SIZEOF(ptl_t, scale)},
	{"color", V_COLOR, offsetof(ptl_t, color), MEMBER_SIZEOF(ptl_t, color)},
	{"a", V_VECTOR, offsetof(ptl_t, a), MEMBER_SIZEOF(ptl_t, a)},
	{"v", V_VECTOR, offsetof(ptl_t, v), MEMBER_SIZEOF(ptl_t, v)},
	{"s", V_VECTOR, offsetof(ptl_t, s), MEMBER_SIZEOF(ptl_t, s)},
	{"offset", V_VECTOR, offsetof(ptl_t, offset), MEMBER_SIZEOF(ptl_t, offset)},
	{"scroll_s", V_FLOAT, offsetof(ptl_t, scrollS), MEMBER_SIZEOF(ptl_t, scrollS)},
	{"scroll_t", V_FLOAT, offsetof(ptl_t, scrollT), MEMBER_SIZEOF(ptl_t, scrollT)},

	/* t and dt are not specified in particle definitions */
	/* but they can be used as references */
	{"t", V_FLOAT, offsetof(ptl_t, t), MEMBER_SIZEOF(ptl_t, t)},
	{"dt", V_FLOAT, offsetof(ptl_t, dt), MEMBER_SIZEOF(ptl_t, dt)},

	{"rounds", V_INT, offsetof(ptl_t, rounds), MEMBER_SIZEOF(ptl_t, rounds)},
	{"angles", V_VECTOR, offsetof(ptl_t, angles), MEMBER_SIZEOF(ptl_t, angles)},
	{"omega", V_VECTOR, offsetof(ptl_t, omega), MEMBER_SIZEOF(ptl_t, omega)},
	{"life", V_FLOAT, offsetof(ptl_t, life), MEMBER_SIZEOF(ptl_t, life)},
	{"tps", V_FLOAT, offsetof(ptl_t, tps), MEMBER_SIZEOF(ptl_t, tps)},
	{"lastthink", V_FLOAT, offsetof(ptl_t, lastThink), MEMBER_SIZEOF(ptl_t, lastThink)},
	{"frame", V_INT, offsetof(ptl_t, frame), MEMBER_SIZEOF(ptl_t, frame)},
	{"endframe", V_INT, offsetof(ptl_t, endFrame), MEMBER_SIZEOF(ptl_t, endFrame)},
	{"fps", V_FLOAT, offsetof(ptl_t, fps), MEMBER_SIZEOF(ptl_t, fps)},
	{"lastframe", V_FLOAT, offsetof(ptl_t, lastFrame), MEMBER_SIZEOF(ptl_t, lastFrame)},
	{"levelflags", V_INT, offsetof(ptl_t, levelFlags), MEMBER_SIZEOF(ptl_t, levelFlags)},
	{"physics", V_BOOL, offsetof(ptl_t, physics), MEMBER_SIZEOF(ptl_t, physics)},
	{"stick", V_BOOL, offsetof(ptl_t, stick), MEMBER_SIZEOF(ptl_t, stick)},
	{"bounce", V_BOOL, offsetof(ptl_t, bounce), MEMBER_SIZEOF(ptl_t, bounce)},
	{"autohide", V_BOOL, offsetof(ptl_t, autohide), MEMBER_SIZEOF(ptl_t, autohide)},
	{"stayalive", V_BOOL, offsetof(ptl_t, stayalive), MEMBER_SIZEOF(ptl_t, stayalive)},
	{"weather", V_BOOL, offsetof(ptl_t, weather), MEMBER_SIZEOF(ptl_t, weather)},
	{"lightcolor", V_VECTOR, offsetof(ptl_t, lightColor), MEMBER_SIZEOF(ptl_t, lightColor)},
	{"lightintensity", V_FLOAT, offsetof(ptl_t, lightIntensity), MEMBER_SIZEOF(ptl_t, lightIntensity)},
	{"lightsustain", V_FLOAT, offsetof(ptl_t, lightSustain), MEMBER_SIZEOF(ptl_t, lightSustain)},

	{NULL, 0, 0, 0}
};

/* =========================================================== */

#define		MAX_PTLDEFS		256
#define		MAX_PTLCMDS		(MAX_PTLDEFS * 32)

static ptlDef_t ptlDef[MAX_PTLDEFS];
static ptlCmd_t ptlCmd[MAX_PTLCMDS];

static int numPtlDefs;
static int numPtlCmds;

#define		MAX_PCMD_DATA	(MAX_PTLCMDS * 8)

/** @todo check pcmdData overflow */
static byte pcmdData[MAX_PCMD_DATA];
static byte *pcmdPos = pcmdData;

static const int RSTACK = -(MAX_PCMD_DATA);

#define		MAX_STACK_DEPTH	8
#define		MAX_STACK_DATA	512

static byte cmdStack[MAX_STACK_DATA];
static void *stackPtr[MAX_STACK_DEPTH];
static byte stackType[MAX_STACK_DEPTH];

/**
 * @brief Will spawn a @c n particles @c deltaTime ms after the parent was spawned
 * @param[in] name The id of the particle (see ptl_*.ufo script files in base/ufos)
 * @param[in] parent The parent particle
 * @param[in] children Spawn as children
 * @param[in] deltaTime The time to wait until this particle should get spawned
 * @param[in] n The amount of particles to spawn (each after deltaTime of its predecessor)
 */
static void CL_ParticleSpawnTimed (const char *name, ptl_t *parent, qboolean children, int deltaTime, int n)
{
	const size_t length = lengthof(timedParticles);
	int i;

	if (n <= 0)
		Com_Error(ERR_DROP, "Timed particle should spawn particles");

	if (deltaTime <= 0)
		Com_Error(ERR_DROP, "Delta time for timed particle is invalid");

	for (i = 0; i < length; i++) {
		timedParticle_t *tp = &timedParticles[i];
		if (tp->n == tp->max) {
			/* found a free slot */
			Q_strncpyz(tp->ptl, name, sizeof(tp->ptl));
			tp->levelFlags = parent->levelFlags;
			tp->dt = deltaTime;
			tp->n = 0;
			tp->children = children;
			tp->max = n;
			tp->parent = parent;
			return;
		}
	}
	Com_Printf("Could not spawn timed particles due to overflow\n");
}

/**
 * @brief Spawns the map particle
 * @param[in] ptl Particle name
 * @param[in] origin The position in the world
 * @param[in] wait
 * @param[in] info
 * @param[in] levelflags The levelflag mask to show the particle on
 */
void CL_AddMapParticle (const char *ptl, const vec3_t origin, const vec2_t wait, const char *info, int levelflags)
{
	mapParticle_t *mp;

	mp = &mapParticles[cl.numMapParticles];

	if (cl.numMapParticles >= MAX_MAPPARTICLES) {
		Com_Printf("Too many map particles (don't add %s) - exceeded %i\n", ptl, MAX_MAPPARTICLES);
		return;
	}
	cl.numMapParticles++;

	Q_strncpyz(mp->ptl, ptl, sizeof(mp->ptl));
	VectorCopy(origin, mp->origin);
	mp->info = info;
	mp->levelflags = levelflags;
	mp->wait[0] = wait[0] * 1000;
	mp->wait[1] = wait[1] * 1000;
	mp->nextTime = cl.time + wait[0] + wait[1] * frand() + 1;

	Com_DPrintf(DEBUG_CLIENT, "Adding map particle %s (%i) with levelflags %i\n", ptl, cl.numMapParticles, levelflags);
}

/**
 * @brief Loads the image or model for a given particle art
 */
static inline void CL_ParticleLoadArt (ptlArt_t *a)
{
	/* register the art */
	switch (a->type) {
	case ART_PIC:
		{
			const char *imageName;
			/* only one image */
			if (a->name[0] != '+')
				imageName = a->name;
			else /* load several frames */
				imageName = va("%s%c%c", a->name + 1, a->frame / 10 + '0', a->frame % 10 + '0');
			a->art.image = R_FindPics(imageName);
			if (!a->art.image)
				Com_Printf("CL_ParticleLoadArt: Could not load image: '%s'\n", imageName);
		}
		break;
	case ART_MODEL:
		/** @todo Support the frame data from ptlArt_t for models, too */
		a->art.model = R_FindModel(a->name);
		break;
	default:
		Com_Error(ERR_DROP, "CL_ParticleLoadArt: Unknown art type\n");
	}
}

void CL_ParticleRegisterArt (void)
{
	ptlArt_t *a;
	int i;

	for (i = 0, a = r_particlesArt; i < r_numParticlesArt; i++, a++)
		CL_ParticleLoadArt(a);
}

/**
 * @brief Register art (pics, models) for each particle
 * @note searches the global particle art list and checks whether the pic or model was already loaded
 * @return index of global art array
 * have their names at the beginning of their structs
 */
static ptlArt_t *CL_ParticleGetArt (const char *name, int frame, artType_t type)
{
	ptlArt_t *a;
	int i;

	/* search for the pic in the list */
	for (i = 0, a = r_particlesArt; i < r_numParticlesArt; i++, a++)
		if (a->type == type && (type == ART_PIC && a->frame == frame) && Q_streq(name, a->name))
			break;

	if (i < r_numParticlesArt)
		return a;

	if (i >= MAX_PTL_ART)
		Com_Error(ERR_DROP, "CL_ParticleGetArt: MAX_PTL_ART overflow");

	a->skin = 0;
	a->type = type;
	a->frame = frame;
	Q_strncpyz(a->name, name, sizeof(a->name));

	CL_ParticleLoadArt(a);

	/* check for an error */
	if (!a->art.image)
		return NULL;

	r_numParticlesArt++;

	return a;
}

/**
 * @sa CL_InitLocal
 */
void PTL_InitStartup (void)
{
	r_numParticles = 0;
	numPtlCmds = 0;
	numPtlDefs = 0;

	r_numParticlesArt = 0;
}

/**
 * @brief Determine the memory location where the command accesses and stores its data.
 * @param p The particle that is used to get local command data locations.
 * @param cmd The command to get the data location for.
 * @return The position where the command stores the data.
 */
static inline void *CL_ParticleCommandGetDataLocation (ptl_t *p, const ptlCmd_t *cmd)
{
	if (cmd->ref < 0)
		/* a negative ref value is relative to the particle */
		return (byte*)p - cmd->ref;
	else
		/* data is stored on the global command data hunk */
		return (byte*)pcmdData + cmd->ref;
}

static void CL_ParticleFunction (ptl_t * p, ptlCmd_t * cmd)
{
	int stackIdx;
	ptrdiff_t e;
	int type;
	int i, j, n;
	void *cmdData;
	float arg;
	ptl_t *pnew;

	/* test for null cmd */
	if (!cmd)
		return;

	/* run until finding PC_END */
	for (stackIdx = 0, e = 0; cmd->cmd != PC_END; cmd++) {
		if (cmd->ref > RSTACK)
			cmdData = CL_ParticleCommandGetDataLocation(p, cmd);
		else {
			if (!stackIdx)
				Com_Error(ERR_DROP, "CL_ParticleFunction: stack underflow");

			/* pop an element off the stack */
			e = (byte *) stackPtr[--stackIdx] - cmdStack;

			i = RSTACK - cmd->ref;
			if (!i) {
				/* normal stack reference */
				cmdData = stackPtr[stackIdx];
				cmd->type = stackType[stackIdx];
			} else {
				/* stack reference to element of vector */
				if ((1 << stackType[stackIdx]) & V_VECS) {
					cmd->type = V_FLOAT;
					cmdData = (float *) stackPtr[stackIdx] + (i - 1);
				} else {
					Com_Error(ERR_DROP, "CL_ParticleFunction: can't get components of a non-vector type (particle %s)", p->ctrl->name);
				}
			}
		}

		switch (cmd->cmd) {
		case PC_PUSH:
			/* check for stack overflow */
			if (stackIdx >= MAX_STACK_DEPTH)
				Com_Error(ERR_DROP, "CL_ParticleFunction: stack overflow");

			/* store the value in the stack */
			stackPtr[stackIdx] = &cmdStack[e];
			stackType[stackIdx] = cmd->type;
			e += Com_SetValue(stackPtr[stackIdx++], cmdData, cmd->type, 0, 0);
			break;

		case PC_POP:
		case PC_KPOP:
			/* check for stack underflow */
			if (stackIdx == 0)
				Com_Error(ERR_DROP, "CL_ParticleFunction: stack underflow");

			/* get pics and models */
			if (offsetof(ptl_t, pic) == -cmd->ref) {
				if (stackType[--stackIdx] != V_STRING)
					Com_Error(ERR_DROP, "Bad type '%s' for pic (particle %s)", vt_names[stackType[stackIdx - 1]], p->ctrl->name);
				p->pic = CL_ParticleGetArt((char *) stackPtr[stackIdx], p->frame, ART_PIC);
				e = (byte *) stackPtr[stackIdx] - cmdStack;
				break;
			}
			if (offsetof(ptl_t, model) == -cmd->ref) {
				if (stackType[--stackIdx] != V_STRING)
					Com_Error(ERR_DROP, "Bad type '%s' for model (particle %s)", vt_names[stackType[stackIdx - 1]], p->ctrl->name);
				p->model = CL_ParticleGetArt((char *) stackPtr[stackIdx], p->frame, ART_MODEL);
				e = (byte *) stackPtr[stackIdx] - cmdStack;
				break;
			}
			if (offsetof(ptl_t, program) == -cmd->ref) {
				if (stackType[--stackIdx] != V_STRING)
					Com_Error(ERR_DROP, "Bad type '%s' for program (particle %s)", vt_names[stackType[stackIdx - 1]], p->ctrl->name);
				p->program = R_LoadProgram((char *) stackPtr[stackIdx], R_InitParticleProgram, R_UseParticleProgram);
				if (p->program)
					p->program->userdata = p;
				e = (byte *) stackPtr[stackIdx] - cmdStack;
				break;
			}

			/* get different data */
			if (cmd->cmd == PC_POP)
				e -= Com_SetValue(cmdData, stackPtr[--stackIdx], cmd->type, 0, 0);
			else
				Com_SetValue(cmdData, stackPtr[stackIdx - 1], cmd->type, 0, 0);
			break;

		case PC_ADD:
		case PC_SUB:
			/* check for stack underflow */
			if (stackIdx == 0)
				Com_Error(ERR_DROP, "CL_ParticleFunction: stack underflow");

			type = stackType[stackIdx - 1];
			if (!((1 << type) & V_VECS))
				Com_Error(ERR_DROP, "CL_ParticleFunction: bad type '%s' for add (particle %s)", vt_names[stackType[stackIdx - 1]], p->ctrl->name);

			/* float based vector addition */
			if (type != cmd->type)
				Com_Error(ERR_DROP, "CL_ParticleFunction: bad vector dimensions for add/sub (particle %s)", p->ctrl->name);

			n = type - V_FLOAT + 1;

			for (i = 0; i < n; i++) {
				if (cmd->cmd == PC_SUB)
					arg = -(*((float *) cmdData + i));
				else
					arg = *((float *) cmdData + i);
				*((float *) stackPtr[stackIdx - 1] + i) += arg;
			}
			break;

		case PC_MUL:
		case PC_DIV:
			/* check for stack underflow */
			if (stackIdx == 0)
				Com_Error(ERR_DROP, "CL_ParticleFunction: stack underflow");

			type = stackType[stackIdx - 1];
			if (!((1 << type) & V_VECS))
				Com_Error(ERR_DROP, "CL_ParticleFunction: bad type '%s' for add (particle %s)", vt_names[stackType[stackIdx - 1]], p->ctrl->name);

			n = type - V_FLOAT + 1;

			if (type > V_FLOAT && cmd->type > V_FLOAT) {
				/* component wise multiplication */
				if (type != cmd->type)
					Com_Error(ERR_DROP, "CL_ParticleFunction: bad vector dimensions for mul/div (particle %s)", p->ctrl->name);

				for (i = 0; i < n; i++) {
					if (cmd->cmd == PC_DIV)
						arg = 1.0 / (*((float *) cmdData + i));
					else
						arg = *((float *) cmdData + i);
					*((float *) stackPtr[stackIdx - 1] + i) *= arg;
				}
				break;
			}

			if (cmd->type > V_FLOAT)
				Com_Error(ERR_DROP, "CL_ParticleFunction: bad vector dimensions for mul/div (particle %s)", p->ctrl->name);

			/* scalar multiplication with scalar in second argument */
			if (cmd->cmd == PC_DIV)
				arg = 1.0 / (*(float *) cmdData);
			else
				arg = *(float *) cmdData;
			for (i = 0; i < n; i++)
				*((float *) stackPtr[stackIdx - 1] + i) *= arg;

			break;

		case PC_SIN:
			if (cmd->type != V_FLOAT)
				Com_Error(ERR_DROP, "CL_ParticleFunction: bad type '%s' for sin (particle %s)", vt_names[stackType[stackIdx - 1]], p->ctrl->name);
			stackPtr[stackIdx] = &cmdStack[e];
			stackType[stackIdx] = cmd->type;
			*(float *) stackPtr[stackIdx++] = sin(*(float *) cmdData * (2 * M_PI));
			e += sizeof(float);
			break;

		case PC_COS:
			if (cmd->type != V_FLOAT)
				Com_Error(ERR_DROP, "CL_ParticleFunction: bad type '%s' for cos (particle %s)", vt_names[stackType[stackIdx - 1]], p->ctrl->name);
			stackPtr[stackIdx] = &cmdStack[e];
			stackType[stackIdx] = cmd->type;
			*(float *) stackPtr[stackIdx++] = sin(*(float *) cmdData * (2 * M_PI));
			e += sizeof(float);
			break;

		case PC_TAN:
			if (cmd->type != V_FLOAT)
				Com_Error(ERR_DROP, "CL_ParticleFunction: bad type '%s' for tan (particle %s)", vt_names[stackType[stackIdx - 1]], p->ctrl->name);
			stackPtr[stackIdx] = &cmdStack[e];
			stackType[stackIdx] = cmd->type;
			*(float *) stackPtr[stackIdx++] = sin(*(float *) cmdData * (2 * M_PI));
			e += sizeof(float);
			break;

		case PC_RAND:
		case PC_CRAND:
			stackPtr[stackIdx] = &cmdStack[e];
			stackType[stackIdx] = cmd->type;

			n = cmd->type - V_FLOAT + 1;

			if (cmd->cmd == PC_RAND)
				for (i = 0; i < n; i++)
					*((float *) stackPtr[stackIdx] + i) = *((float *) cmdData + i) * frand();
			else
				for (i = 0; i < n; i++)
					*((float *) stackPtr[stackIdx] + i) = *((float *) cmdData + i) * crand();

			e += n * sizeof(float);
			stackIdx++;
			break;

		case PC_V2:
		case PC_V3:
		case PC_V4:
			n = cmd->cmd - PC_V2 + 2;
			j = 0;

			if (stackIdx < n)
				Com_Error(ERR_DROP, "CL_ParticleFunction: stack underflow");

			for (i = 0; i < n; i++) {
				if (!((1 << stackType[--stackIdx]) & V_VECS))
					Com_Error(ERR_DROP, "CL_ParticleFunction: bad type '%s' for vector creation (particle %s)", vt_names[stackType[stackIdx]], p->ctrl->name);
				j += stackType[stackIdx] - V_FLOAT + 1;
			}

			if (j > 4)
				Com_Error(ERR_DROP, "CL_ParticleFunction: created vector with dim > 4 (particle %s)", p->ctrl->name);

			stackType[stackIdx++] = V_FLOAT + j - 1;
			break;

		case PC_KILL:
			CL_ParticleFree(p);
			return;

		case PC_SPAWN:
			pnew = CL_ParticleSpawn((const char *) cmdData, p->levelFlags, p->s, p->v, p->a);
			if (!pnew)
				Com_Printf("PC_SPAWN: Could not spawn child particle for '%s'\n", p->ctrl->name);
			break;

		case PC_TNSPAWN:
			/* check for stack underflow */
			if (stackIdx < 2)
				Com_Error(ERR_DROP, "CL_ParticleFunction: stack underflow");

			/* pop elements off the stack */
			/* amount of timed particles */
			type = stackType[--stackIdx];
			if (type != V_INT)
				Com_Error(ERR_DROP, "CL_ParticleFunction: bad type '%s' int required for tnspawn (particle %s)", vt_names[stackType[stackIdx]], p->ctrl->name);
			n = *(int *) stackPtr[stackIdx];

			/* delta time */
			type = stackType[--stackIdx];
			if (type != V_INT)
				Com_Error(ERR_DROP, "CL_ParticleFunction: bad type '%s' int required for tnspawn (particle %s)", vt_names[stackType[stackIdx]], p->ctrl->name);
			i = *(int *) stackPtr[stackIdx];

			/** @todo make the children boolean configurable */
			CL_ParticleSpawnTimed((const char *) cmdData, p, qtrue, i, n);

			e -= 2 * sizeof(int);

			break;

		case PC_NSPAWN:
			/* check for stack underflow */
			if (stackIdx == 0)
				Com_Error(ERR_DROP, "CL_ParticleFunction: stack underflow");

			type = stackType[--stackIdx];
			if (type != V_INT)
				Com_Error(ERR_DROP, "CL_ParticleFunction: bad type '%s' int required for nspawn (particle %s)", vt_names[stackType[stackIdx]], p->ctrl->name);

			n = *(int *) stackPtr[stackIdx];
			e -= sizeof(int);

			for (i = 0; i < n; i++) {
				pnew = CL_ParticleSpawn((const char *) cmdData, p->levelFlags, p->s, p->v, p->a);
				if (!pnew)
					Com_Printf("PC_NSPAWN: Could not spawn child particle for '%s'\n", p->ctrl->name);
			}
			break;

		case PC_CHILD:
			pnew = CL_ParticleSpawn((const char *)cmdData, p->levelFlags, p->s, p->v, p->a);
			if (pnew) {
				pnew->next = p->children;
				pnew->parent = p;
				p->children = pnew;
			} else {
				Com_Printf("PC_CHILD: Could not spawn child particle for '%s'\n", p->ctrl->name);
			}
			break;

		default:
			Com_Error(ERR_DROP, "CL_ParticleFunction: unknown cmd type %i", cmd->type);
			break;
		}
	}
}

ptlDef_t *CL_ParticleGet (const char *name)
{
	int i;

	if (!name || strlen(name) <= 0)
		return NULL;

	/* find the particle definition */
	for (i = 0; i < numPtlDefs; i++) {
		ptlDef_t *pd = &ptlDef[i];
		if (Q_streq(name, pd->name)) {
			return pd;
		}
	}

	return NULL;
}

/**
 * @brief Spawn a new particle to the map
 * @param[in] name The id of the particle (see ptl_*.ufo script files in base/ufos)
 * @param[in] levelFlags Show at which levels
 * @param[in] s starting/location vector
 * @param[in] v velocity vector
 * @param[in] a acceleration vector
 * @sa CL_ParticleFree
 * @sa CL_ViewUpdateRenderData
 * @sa R_DrawParticles
 */
ptl_t *CL_ParticleSpawn (const char *name, int levelFlags, const vec3_t s, const vec3_t v, const vec3_t a)
{
	ptlDef_t *pd;
	ptl_t *p;
	int i;

	if (!name || strlen(name) <= 0)
		return NULL;

	/* find the particle definition */
	pd = CL_ParticleGet(name);
	if (pd == NULL) {
		Com_Printf("Particle definition \"%s\" not found\n", name);
		return NULL;
	}

	/* add the particle */
	for (i = 0; i < r_numParticles; i++)
		if (!r_particleArray[i].inuse)
			break;

	if (i == r_numParticles) {
		if (r_numParticles < MAX_PTLS)
			r_numParticles++;
		else {
			Com_DPrintf(DEBUG_CLIENT, "Too many particles (don't add %s) - exceeded %i\n", name, MAX_PTLS);
			return NULL;
		}
	}

	/* allocate particle */
	p = &r_particleArray[i];
	OBJZERO(*p);

	/* set basic values */
	p->inuse = qtrue;
	p->startTime = cl.time;
	p->ctrl = pd;
	Vector4Set(p->color, 1.0f, 1.0f, 1.0f, 1.0f);

	p->pic = NULL;
	p->model = NULL;

	/* copy location */
	if (s) {
		VectorCopy(s, p->origin);
		VectorCopy(s, p->s);
	}
	/* copy velocity */
	if (v)
		VectorCopy(v, p->v);
	/* copy acceleration */
	if (a)
		VectorCopy(a, p->a);

	/* copy levelflags */
	p->levelFlags = levelFlags;

	/* run init function */
	CL_ParticleFunction(p, pd->init);
	if (p->inuse && !p->tps && !p->life) {
		Com_DPrintf(DEBUG_CLIENT, "Particle %s does not have a tps nor a life set - this is only valid for projectile particles\n",
				name);
		p->tps = 1;
	}

	return p;
}

/**
 * @brief Mark a particle an all its children as invisible or visible
 * @param[in] p Particle to set the invis flags for
 * @param[in] hide Boolean value for hiding the particle
 */
void CL_ParticleVisible (ptl_t *p, qboolean hide)
{
	ptl_t *c;

	p->invis = hide;

	for (c = p->children; c; c = c->next) {
		CL_ParticleVisible(c, hide);
	}
}

/**
 * @brief Free a particle and all it's children
 * @param[in] p the particle to free
 * @sa CL_ParticleSpawn
 */
void CL_ParticleFree (ptl_t *p)
{
	ptl_t *c;

	p->inuse = qfalse;
	p->invis = qtrue;
	for (c = p->children; c; c = c->next) {
		CL_ParticleFree(c);
	}
}

/**
 * @brief Color fade function.
 * @param[in,out] color The color vector to fade.
 * @param[in] fade The type of the fade.
 * @param[in] frac The fraction to fade the color with.
 * @param[in] onlyAlpha Only fade the alpha channel of the given RGBA color.
 */
static void CL_Fading (vec4_t color, fade_t fade, float frac, qboolean onlyAlpha)
{
	int i;

	switch (fade) {
	case FADE_IN:
		for (i = onlyAlpha ? 3 : 0; i < 4; i++)
			color[i] *= frac;
		break;
	case FADE_OUT:
		for (i = onlyAlpha ? 3 : 0; i < 4; i++)
			color[i] *= (1.0 - frac);
		break;
	case FADE_SIN:
		for (i = onlyAlpha ? 3 : 0; i < 4; i++)
			color[i] *= sin(frac * M_PI);
		break;
	case FADE_SAW:
		if (frac < 0.5)
			for (i = onlyAlpha ? 3 : 0; i < 4; i++)
				color[i] *= frac * 2;
		else
			for (i = onlyAlpha ? 3 : 0; i < 4; i++)
				color[i] *= (1.0 - frac) * 2;
		break;
	case FADE_NONE:
		break;
	case FADE_LAST:
		break;
	}
}

/**
 * @brief checks whether a particle is still active in the current round
 * @note also calls the round function of each particle (if defined)
 * @sa CL_ParticleFunction
 */
void CL_ParticleCheckRounds (void)
{
	ptl_t *p;
	int i;

	for (i = 0, p = r_particleArray; i < r_numParticles; i++, p++)
		if (p->inuse) {
			/* run round function */
			CL_ParticleFunction(p, p->ctrl->round);

			if (p->rounds) {
				p->roundsCnt--;
				if (p->roundsCnt <= 0)
					CL_ParticleFree(p);
			}
		}
}

typedef struct ptlTraceCache_s {
	int count;
	vec3_t start;
	vec3_t end;
	vec3_t mins;
	vec3_t maxs;
	trace_t trace;
} ptlTraceCache_t;

/**
 * @brief Particle tracing with caching
 */
static inline trace_t PTL_Trace (ptl_t *ptl, const vec3_t mins, const vec3_t maxs)
{
	static ptlTraceCache_t ptlCache;
	const float epsilonPos = 3.0f;
	const float epsilonBBox = 1.0f;

	if (VectorCompareEps(ptlCache.start, ptl->origin, epsilonPos) && VectorCompareEps(ptlCache.end, ptl->s, epsilonPos)
			&& VectorCompareEps(ptlCache.mins, mins, epsilonBBox) && VectorCompareEps(ptlCache.maxs, maxs, epsilonBBox)) {
		ptlCache.count++;
		return ptlCache.trace;
	}

	VectorCopy(ptl->origin, ptlCache.start);
	VectorCopy(ptl->s, ptlCache.end);
	VectorCopy(mins, ptlCache.mins);
	VectorCopy(maxs, ptlCache.maxs);

	ptlCache.trace = CL_Trace(ptl->origin, ptl->s, mins, maxs, NULL, NULL, MASK_SOLID, cl.mapMaxLevel - 1);
	return ptlCache.trace;
}

/**
 * @brief Prepares the particle rendering, calculate new position, velocity and all the
 * other particle values that are needed to display it
 * @sa CL_ParticleRun
 * @param[in,out] p The particle to handle
 */
static void CL_ParticleRun2 (ptl_t *p)
{
	/* advance time */
	p->dt = cls.frametime;
	p->t = (cl.time - p->startTime) * 0.001f;
	p->lastThink += p->dt;
	p->lastFrame += p->dt;

	if (p->rounds && !p->roundsCnt)
		p->roundsCnt = p->rounds;

	/* test for end of life */
	if (p->life && p->t >= p->life && !p->parent) {
		CL_ParticleFree(p);
		return;
	/* don't play the weather particles if a user don't want them there can
	 * be a lot of weather particles - which might slow the computer down */
	} else if (p->weather && !cl_particleweather->integer) {
		CL_ParticleFree(p);
		return;
	}

	/* kinematics */
	if (p->style != STYLE_LINE) {
		VectorMA(p->s, 0.5 * p->dt * p->dt, p->a, p->s);
		VectorMA(p->s, p->dt, p->v, p->s);
		VectorMA(p->v, p->dt, p->a, p->v);
		VectorMA(p->angles, p->dt, p->omega, p->angles);
	}

	/* basic 'physics' for particles */
	if (p->physics) {
		trace_t tr;
		vec3_t mins, maxs;
		const float size = max(p->size[0], p->size[1]);

		if (p->hitSolid && p->bounce) {
			VectorCopy(p->oldV, p->v);
			VectorNegate(p->a, p->a);
			p->hitSolid = qfalse;
		}

		/* if the particle hit a solid already and is sticking to the surface, no further
		 * traces are needed */
		if (p->hitSolid && p->stick)
			return;

		VectorSet(mins, -size, -size, -size);
		VectorSet(maxs, size, size, size);
		tr = PTL_Trace(p, mins, maxs);

		/* hit something solid */
		if (tr.fraction < 1.0 || tr.startsolid) {
			vec3_t temp;

			p->hitSolid = qtrue;

			/* now execute the physics handler */
			if (p->ctrl->physics)
				CL_ParticleFunction(p, p->ctrl->physics);
			/* let them stay on the ground until they fade out or die */
			if (!p->stayalive) {
				CL_ParticleFree(p);
				return;
			} else if (p->bounce) {
				/* bounce */
				VectorCopy(p->v, p->oldV);
				VectorScale(tr.plane.normal, -DotProduct(tr.plane.normal, p->v), temp);
				VectorAdd(temp, p->v, temp);
				VectorAdd(temp, temp, p->v);
				VectorNegate(p->a, p->a);
			} else {
				VectorClear(p->v);
			}
			VectorCopy(tr.endpos, p->s);
		}
	}

	/* run */
	CL_ParticleFunction(p, p->ctrl->run);

	/* think */
	while (p->tps && p->lastThink * p->tps >= 1) {
		CL_ParticleFunction(p, p->ctrl->think);
		p->lastThink -= 1.0 / p->tps;
	}

	/* animate */
	while (p->fps && p->lastFrame * p->fps >= 1) {
		/* advance frame */
		p->frame++;
		if (p->frame > p->endFrame)
			p->frame = 0;
		p->lastFrame -= 1.0 / p->fps;

		/* load next frame */
		assert(p->pic);
		p->pic = CL_ParticleGetArt(p->pic->name, p->frame, ART_PIC);
	}

	/* fading */
	if (p->thinkFade || p->frameFade) {
		const qboolean onlyAlpha = (p->blend == BLEND_BLEND);
		if (!onlyAlpha)
			Vector4Set(p->color, 1.0f, 1.0f, 1.0f, 1.0f);
		else
			p->color[3] = 1.0;

		if (p->thinkFade)
			CL_Fading(p->color, p->thinkFade, p->lastThink * p->tps, onlyAlpha);
		if (p->frameFade)
			CL_Fading(p->color, p->frameFade, p->lastFrame * p->fps, onlyAlpha);
	}

	/* this is useful for particles like weather effects that are on top of
	 * some other brushes in higher level but should be visible in lower ones */
	if (p->autohide) {
		const int z = (int)p->s[2] / UNIT_HEIGHT;
		if (z > cl_worldlevel->integer) {
			p->invis = qtrue;
			return;
		} else if (z < 0) {
			CL_ParticleFree(p);
			return;
		}
	}

	/* add light to the scene */
	if (VectorNotEmpty(p->lightColor)) {
		const float intensity = 0.5 + p->lightIntensity;
		if (p->lightSustain)
			R_AddSustainedLight(p->s, intensity * PTL_INTENSITY_TO_RADIUS, p->lightColor, p->lightSustain);
		else
			R_AddLight(p->s, intensity * PTL_INTENSITY_TO_RADIUS, p->lightColor);
	}

	/* set the new origin */
	VectorCopy(p->s, p->origin);

	p->invis = qfalse;
}

/**
 * @brief Called every frame and checks whether a timed particle should be spawned
 */
static void CL_ParticleRunTimed (void)
{
	int i;
	const size_t length = lengthof(timedParticles);

	for (i = 0; i < length; i++) {
		timedParticle_t *tp = &timedParticles[i];
		if (!tp->parent || !tp->parent->inuse)
			continue;
		if (tp->n >= tp->max)
			continue;
		if (CL_Milliseconds() - tp->lastTime < tp->dt)
			continue;
		{
			ptl_t *p;
			if (!tp->n) {
				/* first spawn? - then copy the parent values. We have to
				 * do this here and now earlier because projectile particles
				 * get these values set after spawn. */
				VectorCopy(tp->parent->s, tp->s);
				VectorCopy(tp->parent->v, tp->v);
				VectorCopy(tp->parent->a, tp->a);
			}
			tp->n++;
			tp->lastTime = CL_Milliseconds();
			p = CL_ParticleSpawn(tp->ptl, tp->levelFlags, tp->s, tp->v, tp->a);
			if (p && tp->children) {
				p->next = tp->parent->children;
				p->parent = tp->parent;
				tp->parent->children = p;
			}
		}
	}
}

/**
 * @brief General system for particle running during the game.
 * @sa CL_Frame
 */
void CL_ParticleRun (void)
{
	ptl_t *p;
	int i;

	if (cls.state != ca_active)
		return;

	CL_ParticleRunTimed();

	for (i = 0, p = r_particleArray; i < r_numParticles; i++, p++)
		if (p->inuse)
			CL_ParticleRun2(p);
}

/**
 * @brief Parses particle used on maps.
 * @param[in,out] ptl Pointer to particle being parsed and updated.
 * @param[in] es Entity string to parse the particle from
 * @param[in] afterwards If this is true you can modify the particle after the init
 * function for the particle was already called
 */
static void CL_ParseMapParticle (ptl_t * ptl, const char *es, qboolean afterwards)
{
	char keyname[MAX_VAR];
	const char *token;
	char *key;
	const value_t *pp;

	key = keyname + 1;
	do {
		/* get keyname */
		token = Com_Parse(&es);
		if (token[0] == '}')
			break;
		if (!es)
			Com_Error(ERR_DROP, "CL_ParseMapParticle: EOF without closing brace");

		Q_strncpyz(keyname, token, sizeof(keyname));

		/* parse value */
		token = Com_Parse(&es);
		if (!es)
			Com_Error(ERR_DROP, "CL_ParseMapParticle: EOF without closing brace");

		if (token[0] == '}')
			Com_Error(ERR_DROP, "CL_ParseMapParticle: closing brace without data");

		if (!afterwards && keyname[0] != '-')
			continue;
		if (afterwards && keyname[0] != '+')
			continue;

		for (pp = pps; pp->string; pp++) {
			if (Q_streq(key, pp->string)) {
				/* found a normal particle value */
				Com_EParseValue(ptl, token, pp->type, pp->ofs, pp->size);
				break;
			}
		}

		if (!pp->string) {
			/* register art */
			if (Q_streq(key, "image"))
				ptl->pic = CL_ParticleGetArt(token, ptl->frame, ART_PIC);
			else if (Q_streq(key, "model"))
				ptl->model = CL_ParticleGetArt(token, ptl->frame, ART_MODEL);
			else if (Q_streq(key, "program")) {
				ptl->program = R_LoadProgram(token, R_InitParticleProgram, R_UseParticleProgram);
				if (ptl->program)
					ptl->program->userdata = ptl;
			}
		}
	} while (token);
}


void CL_RunMapParticles (void)
{
	mapParticle_t *mp;
	ptl_t *ptl;
	int i;

	if (cls.state != ca_active)
		return;

	for (i = 0, mp = mapParticles; i < cl.numMapParticles; i++, mp++)
		if (mp->nextTime && cl.time >= mp->nextTime) {
			/* spawn a new particle */
			ptl = CL_ParticleSpawn(mp->ptl, mp->levelflags, mp->origin, NULL, NULL);
			if (!ptl) {
				Com_Printf(S_COLOR_YELLOW "Could not spawn particle '%s'\n", mp->ptl);
				mp->nextTime = 0;
				continue;
			}

			/* init the particle */
			CL_ParseMapParticle(ptl, mp->info, qfalse);
			CL_ParticleFunction(ptl, ptl->ctrl->init);
			CL_ParseMapParticle(ptl, mp->info, qtrue);

			/* prepare next spawning */
			if (Vector2NotEmpty(mp->wait))
				mp->nextTime += mp->wait[0] + mp->wait[1] * frand();
			else
				mp->nextTime = 0;
		}
}


static void CL_ParsePtlCmds (const char *name, const char **text)
{
	ptlCmd_t *pc;
	const value_t *pp;
	const char *errhead = "CL_ParsePtlCmds: unexpected end of file";
	const char *token;
	int i, j;

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParsePtlCmds: particle cmds \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (i = 0; i < PC_NUM_PTLCMDS; i++)
			if (Q_streq(token, pc_strings[i])) {
				/* allocate an new cmd */
				if (numPtlCmds >= MAX_PTLCMDS)
					Com_Error(ERR_DROP, "CL_ParsePtlCmds: MAX_PTLCMDS exceeded");
				pc = &ptlCmd[numPtlCmds++];
				OBJZERO(*pc);

				pc->cmd = i;

				if (!pc_types[i])
					break;

				/* get parameter type */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				/* operate on the top element on the stack */
				if (token[0] == '#') {
					pc->ref = RSTACK;
					if (token[1] == '.')
						pc->ref -= (token[2] - '0');
					break;
				}

				if (token[0] == '*') {
					int len;
					char baseComponentToken[MAX_VAR];

					/* it's a variable reference */
					token++;

					/* we maybe have to modify it */
					Q_strncpyz(baseComponentToken, token, sizeof(baseComponentToken));

					/* check for component specifier */
					len = strlen(baseComponentToken);
					/* it's possible to change only the second value of e.g. a vector
					 * just defined e.g. 'size.2' to modify the second value of size */
					if (len >= 2 && baseComponentToken[len - 2] == '.') {
						baseComponentToken[len - 2] = 0;
					} else
						len = 0;

					for (pp = pps; pp->string; pp++)
						if (Q_streq(baseComponentToken, pp->string))
							break;

					if (!pp->string) {
						Com_Printf("CL_ParsePtlCmds: bad reference \"%s\" specified (particle %s)\n", token, name);
						numPtlCmds--;
						break;
					}

					if ((pc_types[i] & PTL_ONLY_ONE_TYPE)) {
						if ((pc_types[i] & ~PTL_ONLY_ONE_TYPE) != pp->type) {
							Com_Printf("CL_ParsePtlCmds: bad type in var \"%s\" (PTL_ONLY_ONE_TYPE) specified (particle %s) (ptl type: %i (pc_type: %i), string: %s)\n", token, name, pp->type, pc_types[i], pc_strings[i]);
							numPtlCmds--;
							break;
						}
					} else if (pp->type >= V_NUM_TYPES || !((1 << pp->type) & pc_types[i])) {
						Com_Printf("CL_ParsePtlCmds: bad type in var \"%s\" specified (particle %s) (ptl type: %i (pc_type: %i), string: %s)\n", token, name, pp->type, pc_types[i], pc_strings[i]);
						numPtlCmds--;
						break;
					}

					if (len) {
						/* get single component */
						if ((1 << pp->type) & V_VECS) {
							const int component = (baseComponentToken[len - 1] - '1');
							/* get the component we want to modify */
							if (component > 3) {
								Com_Printf("CL_ParsePtlCmds: bad component value - it's bigger than 3: %i (particle %s)\n", component, name);
								numPtlCmds--;
								break;
							}
							pc->type = V_FLOAT;
							/* go to component offset */
							pc->ref = -((int)pp->ofs) - component * sizeof(float);
							break;
						} else {
							Com_Printf("CL_ParsePtlCmds: can't get components of a non-vector type (particle %s)\n", name);
							numPtlCmds--;
							break;
						}
					}

					/* set the values */
					pc->type = pp->type;
					pc->ref = -((int)pp->ofs);
					break;
				}

				/* get the type */
				if (pc_types[i] & PTL_ONLY_ONE_TYPE)
					/* extract the real type */
					j = pc_types[i] & ~PTL_ONLY_ONE_TYPE;
				else {
					for (j = 0; j < V_NUM_TYPES; j++)
						if (Q_streq(token, vt_names[j]))
							break;

					if (j >= V_NUM_TYPES || !((1 << j) & pc_types[i])) {
						Com_Printf("CL_ParsePtlCmds: bad type \"%s\" specified (particle %s)\n", token, name);
						numPtlCmds--;
						break;
					}

					/* get the value */
					token = Com_EParse(text, errhead, name);
					if (!*text)
						return;
				}

				/* set the values */
				pc->type = j;

				pcmdPos = Com_AlignPtr(pcmdPos, pc->type);
				pc->ref = (int) (pcmdPos - pcmdData);
				pcmdPos += Com_EParseValue(pcmdPos, token, pc->type, 0, 0);

/*				Com_Printf("%s %s %i\n", vt_names[pc->type], token, pcmdPos - pc->ref, (char *)pc->ref); */
				break;
			}

		if (i < PC_NUM_PTLCMDS)
			continue;

		for (pp = pps; pp->string; pp++)
			if (Q_streq(token, pp->string)) {
				/* get parameter */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				/* translate set to a push and pop */
				if (numPtlCmds >= MAX_PTLCMDS)
					Com_Error(ERR_DROP, "CL_ParsePtlCmds: MAX_PTLCMDS exceeded");
				pc = &ptlCmd[numPtlCmds++];
				pc->cmd = PC_PUSH;
				pc->type = pp->type;

				pcmdPos = Com_AlignPtr(pcmdPos, pc->type);
				pc->ref = (int) (pcmdPos - pcmdData);
				pcmdPos += Com_EParseValue(pcmdPos, token, pc->type, 0, 0);

				pc = &ptlCmd[numPtlCmds++];
				pc->cmd = PC_POP;
				pc->type = pp->type;
				pc->ref = -((int)pp->ofs);
				break;
			}

		if (!pp->string)
			Com_Printf("CL_ParsePtlCmds: unknown token \"%s\" ignored (particle %s)\n", token, name);

	} while (*text);

	/* terminalize cmd chain */
	if (numPtlCmds >= MAX_PTLCMDS)
		Com_Error(ERR_DROP, "CL_ParsePtlCmds: MAX_PTLCMDS exceeded");
	pc = &ptlCmd[numPtlCmds++];
	OBJZERO(*pc);
}

/**
 * @brief Parses particle definitions from UFO-script files
 * @param[in] name particle name/id
 * @param[in] text pointer to the buffer to parse from
 * @return the position of the particle in ptlDef array
 * @sa CL_ParseClientData
 */
void CL_ParseParticle (const char *name, const char **text)
{
	const char *errhead = "CL_ParseParticle: unexpected end of file (particle ";
	ptlDef_t *pd;
	const char *token;
	int i;

	/* search for particles with same name */
	for (i = 0; i < numPtlDefs; i++)
		if (Q_streq(name, ptlDef[i].name))
			break;

	if (i < numPtlDefs) {
		Com_Printf("CL_ParseParticle: particle def \"%s\" with same name found, reset first one\n", name);
		pd = &ptlDef[i];
	} else {
		if (numPtlDefs < MAX_PTLDEFS - 1) {
			/* initialize the new particle */
			pd = &ptlDef[numPtlDefs++];
		} else {
			Com_Printf("CL_ParseParticle: max particle definitions reached - skip the current one: '%s'\n", name);
			return;
		}
	}
	OBJZERO(*pd);

	Q_strncpyz(pd->name, name, sizeof(pd->name));

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseParticle: particle def \"%s\" without body ignored\n", name);
		if (i == numPtlDefs)
			numPtlDefs--;
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (i = 0; i < PF_NUM_PTLFUNCS; i++)
			if (Q_streq(token, pf_strings[i])) {
				/* allocate the first particle command */
				ptlCmd_t **pc;

				pc = (ptlCmd_t **) ((byte *) pd + pf_values[i]);
				*pc = &ptlCmd[numPtlCmds];

				/* parse the commands */
				CL_ParsePtlCmds(name, text);
				break;
			}

		if (i == PF_NUM_PTLFUNCS)
			Com_Printf("CL_ParseParticle: unknown token \"%s\" ignored (particle %s)\n", token, name);

	} while (*text);

	/* check for an init function */
	if (!pd->init) {
		Com_Printf("CL_ParseParticle: particle definition %s without init function ignored\n", name);
		if (i == numPtlDefs)
			numPtlDefs--;
	}
}

/**
 * @brief Spawns a debug marker particle in the world
 */
static void PTL_DebugSpawnMarker_f (void)
{
	vec3_t worldOrigin;

	if (Cmd_Argc() < 4) {
		Com_Printf("Usage: %s <x> <y> <z>\n", Cmd_Argv(0));
		return;
	}

	worldOrigin[0] = atof(Cmd_Argv(1));
	worldOrigin[1] = atof(Cmd_Argv(2));
	worldOrigin[2] = atof(Cmd_Argv(3));

	CL_ParticleSpawn("debug_marker", 0, worldOrigin, NULL, NULL);
}

static void PTL_DebugList_f (void)
{
	int i;

	Com_Printf("%i particles\n", r_numParticles);
	for (i = 0; i < r_numParticles; i++) {
		const ptl_t *p = &r_particleArray[i];
		const ptlDef_t *def = p->ctrl;
		const value_t *pp = pps;
		if (!p->inuse)
			continue;
		Com_Printf("particle %i\n", i);
		Com_Printf(" name: %s\n", def->name);
		for (pp = pps; pp->string; pp++) {
			const char* value = "";
			if (Q_streq(pp->string, "image") && p->pic) {
				value = p->pic->name;
			} else if (Q_streq(pp->string, "model") && p->model) {
				value = p->model->name;
			} else if (Q_streq(pp->string, "program") && p->program) {
				value = p->program->name;
			} else {
				value = Com_ValueToStr(p, pp->type, pp->ofs);
			}
			Com_Printf(" %s: %s\n", pp->string, value);
		}
	}
}

/**
 * @brief Initializes cvars and commands
 */
void CL_InitParticles (void)
{
	cl_particleweather = Cvar_Get("cl_particleweather", "1", CVAR_ARCHIVE, "Switch the weather particles on or off");
	Cmd_AddCommand("debug_spawnmarker", PTL_DebugSpawnMarker_f, "Spawn a marker particle in the world at a given location");
	Cmd_AddCommand("debug_particlelist", PTL_DebugList_f, NULL);
}
