/**
 * @file cl_particle.c
 * @brief Client particle parsing and rendering functions.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#include "client.h"

mp_t MPs[MAX_MAPPARTICLES];
int numMPs;

#define RADR(x)		((x < 0) ? (byte*)p-x : (byte*)pcmdData+x)
#define RSTACK		-0xFFF0

typedef struct ptlCmd_s {
	byte cmd;
	byte type;
	int ref;
} ptlCmd_t;

typedef struct ptlDef_s {
	char name[MAX_VAR];
	ptlCmd_t *init, *run, *think, *round;
} ptlDef_t;


/* =========================================================== */

typedef enum pf_s {
	PF_INIT,
	PF_RUN,
	PF_THINK,
	PF_ROUND,

	PF_NUM_PTLFUNCS
} pf_t;

static char *pf_strings[PF_NUM_PTLFUNCS] = {
	"init",
	"run",
	"think",
	"round"
};

static size_t pf_values[PF_NUM_PTLFUNCS] = {
	offsetof(ptlDef_t, init),
	offsetof(ptlDef_t, run),
	offsetof(ptlDef_t, think),
	offsetof(ptlDef_t, round)
};


typedef enum pc_s {
	PC_END,

	PC_PUSH, PC_POP, PC_KPOP,
	PC_ADD, PC_SUB,
	PC_MUL, PC_DIV,
	PC_SIN, PC_COS, PC_TAN,
	PC_RAND, PC_CRAND,
	PC_V2, PC_V3, PC_V4,

	PC_KILL,
	PC_SPAWN, PC_NSPAWN,

	PC_NUM_PTLCMDS
} pc_t;

static char *pc_strings[PC_NUM_PTLCMDS] = {
	"end",

	"push", "pop", "kpop",
	"add", "sub",
	"mul", "div",
	"sin", "cos", "tan",
	"rand", "crand",
	"v2", "v3", "v4",

	"kill",
	"spawn", "nspawn"
};

#define F(x)		(1<<x)
#define	V_VECS		(F(V_FLOAT) | F(V_POS) | F(V_VECTOR) | F(V_COLOR))
#define ONLY		(1<<31)

static int pc_types[PC_NUM_PTLCMDS] = {
	0,

	V_UNTYPED, V_UNTYPED, V_UNTYPED,
	V_VECS, V_VECS,
	V_VECS, V_VECS,
	ONLY | V_FLOAT, ONLY | V_FLOAT, ONLY | V_FLOAT,
	V_VECS, V_VECS,
	0, 0, 0,

	0,
	ONLY | V_STRING, ONLY | V_STRING
};

static value_t pps[] = {
	{"image", V_STRING, offsetof(ptl_t, pic)},
	{"model", V_STRING, offsetof(ptl_t, model)},
	{"blend", V_BLEND, offsetof(ptl_t, blend)},
	{"style", V_STYLE, offsetof(ptl_t, style)},
	{"tfade", V_FADE, offsetof(ptl_t, thinkFade)},
	{"ffade", V_FADE, offsetof(ptl_t, frameFade)},
	{"size", V_POS, offsetof(ptl_t, size)},
	{"scale", V_VECTOR, offsetof(ptl_t, scale)},
	{"color", V_COLOR, offsetof(ptl_t, color)},
	{"a", V_VECTOR, offsetof(ptl_t, a)},
	{"v", V_VECTOR, offsetof(ptl_t, v)},
	{"s", V_VECTOR, offsetof(ptl_t, s)},

	/* t and dt are not specified in particle definitions */
	/* but they can be used as references */
	{"t", V_FLOAT, offsetof(ptl_t, t)},
	{"dt", V_FLOAT, offsetof(ptl_t, dt)},

	{"rounds", V_INT, offsetof(ptl_t, rounds)},
	{"angles", V_VECTOR, offsetof(ptl_t, angles)},
	{"omega", V_VECTOR, offsetof(ptl_t, omega)},
	{"life", V_FLOAT, offsetof(ptl_t, life)},
	{"tps", V_FLOAT, offsetof(ptl_t, tps)},
	{"lastthink", V_FLOAT, offsetof(ptl_t, lastThink)},
	{"frame", V_INT, offsetof(ptl_t, frame)},
	{"endframe", V_INT, offsetof(ptl_t, endFrame)},
	{"fps", V_FLOAT, offsetof(ptl_t, fps)},
	{"lastframe", V_FLOAT, offsetof(ptl_t, lastFrame)},
	{"levelflags", V_INT, offsetof(ptl_t, levelFlags)},
	{"light", V_BOOL, offsetof(ptl_t, light)},

	{NULL, 0, 0}
};

typedef enum art_s {
	ART_PIC,
	ART_MODEL
} art_t;


/* =========================================================== */

#define		MAX_PTLDEFS		256
#define		MAX_PTLCMDS		(MAX_PTLDEFS*32)

static ptlDef_t ptlDef[MAX_PTLDEFS];
static ptlCmd_t ptlCmd[MAX_PTLCMDS];

static int numPtlDefs;
static int numPtlCmds;

#define		MAX_PCMD_DATA	(MAX_PTLCMDS*8)

static byte pcmdData[MAX_PCMD_DATA];
static int pcmdPos;

#define		MAX_STACK_DEPTH	8
#define		MAX_STACK_DATA	512

static byte cmdStack[MAX_STACK_DATA];
static void *stackPtr[MAX_STACK_DEPTH];
static byte stackType[MAX_STACK_DEPTH];

ptlArt_t ptlArt[MAX_PTL_ART];
ptl_t ptl[MAX_PTLS];

int numPtlArt;
int numPtls;

/* =========================================================== */


/**
 * @brief
 */
void CL_ParticleRegisterArt(void)
{
	ptlArt_t *a;
	int i;

	for (i = 0, a = ptlArt; i < numPtlArt; i++, a++) {
		/* register the art */
		switch (a->type) {
		case ART_PIC:
			if (*a->name != '+')
				a->art = (char *) re.RegisterPic(a->name);
			else
				a->art = (char *) re.RegisterPic(va("%s%c%c", a->name + 1, a->frame / 10 + '0', a->frame % 10 + '0'));
			break;
		case ART_MODEL:
			a->art = (char *) re.RegisterModel(a->name);
			break;
		default:
			Sys_Error("CL_ParticleRegisterArt: Unknown art type\n");
		}
	}
}


/**
 * @brief Register art (pics, models) for each particle
 * @note searches the global particle art list and checks whether the pic or model was already loaded
 * @return index of global art array
 */
static int CL_ParticleGetArt(char *name, int frame, char type)
{
	ptlArt_t *a;
	int i;

	/* search for the pic in the list */
	for (i = 0, a = ptlArt; i < numPtlArt; i++, a++)
		if (a->type == type && !Q_strncmp(name, a->name, MAX_VAR) && a->frame == frame)
			break;

	if (i < numPtlArt)
		return i;

	if (i >= MAX_PTL_ART)
		Sys_Error("CL_ParticleGetArt: MAX_PTL_ART overflow\n");

	/* register new art */
	switch (type) {
	case ART_PIC:
		if (*name != '+')
			a->art = (char *) re.RegisterPic(name);
		else
			a->art = (char *) re.RegisterPic(va("%s%c%c", name + 1, frame / 10 + '0', frame % 10 + '0'));
		break;
	case ART_MODEL:
		a->art = (char *) re.RegisterModel(name);
		break;
	default:
		Sys_Error("CL_ParticleGetArt: Unknown art type\n");
	}

	/* check for an error */
	if (!a->art)
		return -1;

	a->type = type;
	a->frame = (type == ART_PIC) ? frame : 0;
	Q_strncpyz(a->name, name, MAX_VAR);
	numPtlArt++;

	return numPtlArt - 1;
}


/**
 * @brief
 */
void CL_ResetParticles(void)
{
	numPtls = 0;
	numPtlCmds = 0;
	numPtlDefs = 0;

	numPtlArt = 0;
	pcmdPos = 0;
}


/**
 * @brief
 */
void CL_ParticleFunction(ptl_t * p, ptlCmd_t * cmd)
{
	int s, e;
	int type;
	int i, j, n;
	void *radr;
	float arg;

	/* test for null cmd */
	if (!cmd)
		return;

	/* run until finding PC_END */
	for (s = 0, e = 0; cmd->cmd != PC_END; cmd++) {
		if (cmd->ref > RSTACK)
			radr = RADR(cmd->ref);
		else {
			if (!s)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: stack underflow\n");

			/* pop an element off the stack */
			e = (byte *) stackPtr[--s] - cmdStack;

			i = RSTACK - cmd->ref;
			if (!i) {
				/* normal stack reference */
				radr = stackPtr[s];
				cmd->type = stackType[s];
			} else {
				/* stack reference to element of vector */
				if ((1 << stackType[s]) & V_VECS) {
					cmd->type = V_FLOAT;
					radr = (float *) stackPtr[s] + (i - 1);
				} else {
					Com_Error(ERR_FATAL, "CL_ParticleFunction: can't get components of a non-vector type (particle %s)\n", p->ctrl->name);
					radr = NULL;
				}
			}
		}

		switch (cmd->cmd) {
		case PC_PUSH:
			/* check for stack overflow */
			if (s >= MAX_STACK_DEPTH)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: stack overflow\n");

			/* store the value in the stack */
			stackPtr[s] = &cmdStack[e];
			stackType[s] = cmd->type;
			e += Com_SetValue(stackPtr[s++], radr, cmd->type, 0);
			break;

		case PC_POP:
		case PC_KPOP:
			/* check for stack underflow */
			if (s == 0)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: stack underflow\n");

			/* get pics and models */
			if (offsetof(ptl_t, pic) == -cmd->ref) {
				if (stackType[--s] != V_STRING)
					Sys_Error("Bad type '%s' for pic (particle %s)\n", vt_names[stackType[s - 1]], p->ctrl->name);
				p->pic = CL_ParticleGetArt((char *) stackPtr[s], p->frame, ART_PIC);
				e = (byte *) stackPtr[s] - cmdStack;
				break;
			}
			if (offsetof(ptl_t, model) == -cmd->ref) {
				if (stackType[--s] != V_STRING)
					Sys_Error("Bad type '%s' for model (particle %s)\n", vt_names[stackType[s - 1]], p->ctrl->name);
				p->model = CL_ParticleGetArt((char *) stackPtr[s], 0, ART_MODEL);
				e = (byte *) stackPtr[s] - cmdStack;
				break;
			}

			/* get different data */
			if (cmd->cmd == PC_POP)
				e -= Com_SetValue(radr, stackPtr[--s], cmd->type, 0);
			else
				Com_SetValue(radr, stackPtr[s - 1], cmd->type, 0);
			break;

		case PC_ADD:
		case PC_SUB:
			/* check for stack underflow */
			if (s == 0)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: stack underflow\n");

			type = stackType[s - 1];
			if (!((1 << type) & V_VECS))
				Com_Error(ERR_FATAL, "CL_ParticleFunction: bad type '%s' for add (particle %s)\n", vt_names[stackType[s - 1]], p->ctrl->name);

			/* float based vector addition */
			if (type != cmd->type)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: bad vector dimensions for add/sub (particle %s)\n", p->ctrl->name);

			n = type - V_FLOAT + 1;

			for (i = 0; i < n; i++) {
				if (cmd->cmd == PC_SUB)
					arg = -(*((float *) radr + i));
				else
					arg = *((float *) radr + i);
				*((float *) stackPtr[s - 1] + i) += arg;
			}
			break;

		case PC_MUL:
		case PC_DIV:
			/* check for stack underflow */
			if (s == 0)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: stack underflow\n");

			type = stackType[s - 1];
			if (!((1 << type) & V_VECS))
				Com_Error(ERR_FATAL, "CL_ParticleFunction: bad type '%s' for add (particle %s)\n", vt_names[stackType[s - 1]], p->ctrl->name);

			n = type - V_FLOAT + 1;

			if (type > V_FLOAT && cmd->type > V_FLOAT) {
				/* component wise multiplication */
				if (type != cmd->type)
					Com_Error(ERR_FATAL, "CL_ParticleFunction: bad vector dimensions for mul/div (particle %s)\n", p->ctrl->name);

				for (i = 0; i < n; i++) {
					if (cmd->cmd == PC_DIV)
						arg = 1.0 / (*((float *) radr + i));
					else
						arg = *((float *) radr + i);
					*((float *) stackPtr[s - 1] + i) *= arg;
				}
				break;
			}

			if (cmd->type > V_FLOAT)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: bad vector dimensions for mul/div (particle %s)\n", p->ctrl->name);

			/* scalar multiplication with scalar in second argument */
			if (cmd->cmd == PC_DIV)
				arg = 1.0 / (*(float *) radr);
			else
				arg = *(float *) radr;
			for (i = 0; i < n; i++)
				*((float *) stackPtr[s - 1] + i) *= arg;

			break;

		case PC_SIN:
			if (cmd->type != V_FLOAT)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: bad type '%s' for sin (particle %s)\n", vt_names[stackType[s - 1]], p->ctrl->name);
			stackPtr[s] = &cmdStack[e];
			stackType[s] = cmd->type;
			*(float *) stackPtr[s++] = sin(*(float *) radr * 2 * M_PI);
			e += sizeof(float);
			break;

		case PC_COS:
			if (cmd->type != V_FLOAT)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: bad type '%s' for cos (particle %s)\n", vt_names[stackType[s - 1]], p->ctrl->name);
			stackPtr[s] = &cmdStack[e];
			stackType[s] = cmd->type;
			*(float *) stackPtr[s++] = sin(*(float *) radr * 2 * M_PI);
			e += sizeof(float);
			break;

		case PC_TAN:
			if (cmd->type != V_FLOAT)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: bad type '%s' for tan (particle %s)\n", vt_names[stackType[s - 1]], p->ctrl->name);
			stackPtr[s] = &cmdStack[e];
			stackType[s] = cmd->type;
			*(float *) stackPtr[s++] = sin(*(float *) radr * 2 * M_PI);
			e += sizeof(float);
			break;

		case PC_RAND:
		case PC_CRAND:
			stackPtr[s] = &cmdStack[e];
			stackType[s] = cmd->type;

			n = cmd->type - V_FLOAT + 1;

			if (cmd->cmd == PC_RAND)
				for (i = 0; i < n; i++)
					*((float *) stackPtr[s] + i) = *((float *) radr + i) * frand();
			else
				for (i = 0; i < n; i++)
					*((float *) stackPtr[s] + i) = *((float *) radr + i) * crand();

			e += n * sizeof(float);
			s++;
			break;

		case PC_V2:
		case PC_V3:
		case PC_V4:
			n = cmd->cmd - PC_V2 + 2;
			j = 0;

			if (s < n)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: stack underflow\n");

			for (i = 0; i < n; i++) {
				if (!((1 << stackType[--s]) & V_VECS))
					Com_Error(ERR_FATAL, "CL_ParticleFunction: bad type '%s' for vector creation (particle %s)\n", vt_names[stackType[s]], p->ctrl->name);
				j += stackType[s] - V_FLOAT + 1;
			}

			if (j > 4)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: created vector with dim > 4 (particle %s)\n", p->ctrl->name);

			stackType[s++] = V_FLOAT + j - 1;
			break;

		case PC_KILL:
			p->inuse = qfalse;
			return;

		case PC_SPAWN:
			CL_ParticleSpawn((char *) radr, p->levelFlags, p->s, p->v, p->a);
			break;

		case PC_NSPAWN:
			/* check for stack underflow */
			if (s == 0)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: stack underflow\n");

			type = stackType[--s];
			if (type != V_INT)
				Com_Error(ERR_FATAL, "CL_ParticleFunction: bad type '%s' int required for nspawn (particle %s)\n", vt_names[stackType[s]], p->ctrl->name);

			n = *(int *) stackPtr[s];
			e -= sizeof(int);

			for (i = 0; i < n; i++)
				CL_ParticleSpawn((char *) radr, p->levelFlags, p->s, p->v, p->a);
			break;

		default:
			Sys_Error("CL_ParticleFunction: unknown cmd type %i\n", cmd->type);
			break;
		}
	}
}


/**
 * @brief
 */
ptl_t *CL_ParticleSpawn(char *name, int levelFlags, vec3_t s, vec3_t v, vec3_t a)
{
	ptlDef_t *pd;
	ptl_t *p;
	int i;

	/* find the particle definition */
	for (i = 0; i < numPtlDefs; i++)
		if (!Q_strncmp(name, ptlDef[i].name, MAX_VAR))
			break;

	if (i == numPtlDefs) {
		Com_Printf("Particle definiton \"%s\" not found\n", name);
		return NULL;
	}

	pd = &ptlDef[i];

	/* add the particle */
	for (i = 0; i < numPtls; i++)
		if (!ptl[i].inuse)
			break;

	if (i == numPtls) {
		if (numPtls < MAX_PTLS)
			numPtls++;
		else {
			Com_Printf("Too many particles...\n");
			return NULL;
		}
	}

	/* allocate particle */
	p = &ptl[i];
	memset(p, 0, sizeof(ptl_t));

	/* set basic values */
	p->inuse = qtrue;
	p->startTime = cl.time;
	p->ctrl = pd;
	p->color[0] = p->color[1] = p->color[2] = p->color[3] = 1.0f;

	p->pic = -1;
	p->model = -1;

	/* execute init function */
	if (s)
		VectorCopy(s, p->s);
	if (v)
		VectorCopy(v, p->v);
	if (a)
		VectorCopy(a, p->a);

	p->levelFlags = levelFlags;

	/* run init function */
	CL_ParticleFunction(p, pd->init);

	return p;
}

/**
 * @brief Spawn a particle given by EV_SPAWN_PARTICLE event
 * @param[in] sb sizebuf that holds the network transfer
 */
void CL_ParticleSpawnFromSizeBuf (sizebuf_t* sb)
{
	char *particle;
	int levelflags, i;
	pos3_t originPos;
	vec3_t origin;

	levelflags = MSG_ReadShort(sb);
	MSG_ReadGPos(sb, originPos);
	for (i=0;i<3;i++)
		origin[i] = (float)originPos[i];

	MSG_ReadByte(sb); /* for stringlength */
	particle = MSG_ReadString(sb);

	if (particle && Q_strcmp(particle, "null")) {
		CL_ParticleSpawn(particle, levelflags, origin, NULL, NULL);
	}
}

/**
 * @brief
 */
void CL_Fading(vec4_t color, byte fade, float frac, qboolean onlyAlpha)
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
	default:
		/* shouldn't happen */
		break;
	}
}

/**
 * @brief checks whether a particle is still active in the current round
 * @note also calls the round function of each particle (if defined)
 * @sa CL_ParticleFunction
 */
void CL_ParticleCheckRounds(void)
{
	ptl_t *p;
	int i;

	for (i = 0, p = ptl; i < numPtls; i++, p++)
		if (p->inuse) {
			/* run round function */
			CL_ParticleFunction(p, p->ctrl->round);

			if (p->rounds) {
				p->roundsCnt--;
				if (p->roundsCnt <= 0)
					p->inuse = qfalse;
			}
		}
}


/**
 * @brief
 */
void CL_ParticleRun(void)
{
	qboolean onlyAlpha;
	ptl_t *p;
	int i;

	for (i = 0, p = ptl; i < numPtls; i++, p++)
		if (p->inuse) {
			/* advance time */
			p->dt = cls.frametime;
			p->t = (cl.time - p->startTime) * 0.001f;
			p->lastThink += p->dt;
			p->lastFrame += p->dt;

			if (p->rounds && !p->roundsCnt)
				p->roundsCnt = p->rounds;

			/* test for end of life */
			if (p->life && p->t >= p->life) {
				p->inuse = qfalse;
				continue;
			}

			/* kinematics */
			if (p->style != STYLE_LINE) {
				VectorMA(p->s, 0.5 * p->dt * p->dt, p->a, p->s);
				VectorMA(p->s, p->dt, p->v, p->s);
				VectorMA(p->v, p->dt, p->a, p->v);
				VectorMA(p->angles, p->dt, p->omega, p->angles);
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
				p->pic = CL_ParticleGetArt(ptlArt[p->pic].name, p->frame, ART_PIC);
			}

			/* fading */
			if (p->thinkFade || p->frameFade) {
				onlyAlpha = (p->blend == BLEND_BLEND);
				if (!onlyAlpha) {
					p->color[0] = p->color[1] = p->color[2] = p->color[3] = 1.0;
				} else
					p->color[3] = 1.0;
				if (p->thinkFade)
					CL_Fading(p->color, p->thinkFade, p->lastThink * p->tps, p->blend == BLEND_BLEND);
				if (p->frameFade)
					CL_Fading(p->color, p->frameFade, p->lastFrame * p->fps, p->blend == BLEND_BLEND);
			}
			if (p->light)
				V_AddLight(p->s, 256, p->color[0], p->color[1], p->color[2]);
		}
}


/* =========================================================== */

/**
 * @brief
 */
void CL_ParseMapParticle(ptl_t * ptl, char *es, qboolean afterwards)
{
	char keyname[MAX_QPATH];
	char *key, *token;
	value_t *pp;

	key = keyname + 1;
	do {
		/* get keyname */
		token = COM_Parse(&es);
		if (token[0] == '}')
			break;
		if (!es)
			Com_Error(ERR_DROP, "ED_ParseEntity: EOF without closing brace");

		Q_strncpyz(keyname, token, MAX_QPATH);

		/* parse value */
		token = COM_Parse(&es);
		if (!es)
			Com_Error(ERR_DROP, "ED_ParseEntity: EOF without closing brace");

		if (token[0] == '}')
			Com_Error(ERR_DROP, "ED_ParseEntity: closing brace without data");

		if (!afterwards && keyname[0] != '-')
			continue;
		if (afterwards && keyname[0] != '+')
			continue;

		for (pp = pps; pp->string; pp++)
			if (!Q_strncmp(key, pp->string, MAX_QPATH - 1)) {
				/* register art */
				if (!Q_strncmp(pp->string, "image", 5)) {
					ptl->pic = CL_ParticleGetArt(token, ptl->frame, ART_PIC);
					break;
				}
				if (!Q_strncmp(pp->string, "model", 5)) {
					ptl->model = CL_ParticleGetArt(token, 0, ART_MODEL);
					break;
				}

				/* found a normal particle value */
				Com_ParseValue(ptl, token, pp->type, pp->ofs);
				break;
			}
	} while (token);
}


/**
 * @brief
 */
void CL_RunMapParticles(void)
{
	mp_t *mp;
	ptl_t *ptl;
	int i;

	for (i = 0, mp = MPs; i < numMPs; i++, mp++)
		if (mp->nextTime && cl.time >= mp->nextTime) {
			/* spawn a new particle */
			ptl = CL_ParticleSpawn(mp->ptl, mp->levelflags, mp->origin, NULL, NULL);
			if (!ptl) {
				mp->nextTime = 0;
				continue;
			}

			/* init the particle */
			CL_ParseMapParticle(ptl, mp->info, qfalse);
			CL_ParticleFunction(ptl, ptl->ctrl->init);
			CL_ParseMapParticle(ptl, mp->info, qtrue);

			/* prepare next spawning */
			if (mp->wait[0] || mp->wait[1])
				mp->nextTime += mp->wait[0] + mp->wait[1] * frand();
			else
				mp->nextTime = 0;
		}
}


/* =========================================================== */


/**
 * @brief
 */
void CL_ParsePtlCmds(char *name, char **text)
{
	ptlCmd_t *pc;
	value_t *pp;
	char *errhead = "CL_ParsePtlCmds: unexptected end of file";
	char *token;
	int i, j;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParsePtlCmds: particle cmds \"%s\" without body ignored\n", name);
		pc = &ptlCmd[numPtlCmds++];
		memset(pc, 0, sizeof(ptlCmd_t));
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (i = 0; i < PC_NUM_PTLCMDS; i++)
			if (!Q_strcmp(token, pc_strings[i])) {
				/* allocate an new cmd */
				pc = &ptlCmd[numPtlCmds++];
				memset(pc, 0, sizeof(ptlCmd_t));

				pc->cmd = i;

				if (!pc_types[i])
					break;

				/* get parameter type */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				if (token[0] == '#') {
					pc->ref = RSTACK;
					if (token[1] == '.')
						pc->ref -= (token[2] - '0');
					break;
				}

				if (token[0] == '*') {
					int len;

					/* it's a variable reference */
					token++;

					/* check for component specifier */
					len = strlen(token);
					if (token[len - 2] == '.')
						token[len - 2] = 0;
					else
						len = 0;

					for (pp = pps; pp->string; pp++)
						if (!Q_strcmp(token, pp->string))
							break;

					if (!pp->string) {
						Com_Printf("CL_ParsePtlCmds: bad reference \"%s\" specified (particle %s)\n", token, name);
						numPtlCmds--;
						break;
					}

					if (((pc_types[i] & ONLY) && (pc_types[i] & ~ONLY) != pp->type) || (!(pc_types[i] & ONLY) && !((1 << pp->type) & pc_types[i]))) {
						Com_Printf("CL_ParsePtlCmds: bad type in var \"%s\" specified (particle %s)\n", token, name);
						numPtlCmds--;
						break;
					}

					if (len) {
						/* get single component */
						if ((1 << pp->type) & V_VECS) {
							pc->type = V_FLOAT;
							pc->ref = -pp->ofs - (token[len - 1] - '1') * sizeof(float);
							break;
						} else
							Com_Printf("CL_ParsePtlCmds: can't get components of a non-vector type (particle %s)\n", name);
					}

					/* set the values */
					pc->type = pp->type;
					pc->ref = -pp->ofs;
					break;
				}

				/* get the type */
				if (pc_types[i] & ONLY)
					j = pc_types[i] & ~ONLY;
				else {
					for (j = 0; j < V_NUM_TYPES; j++)
						if (!Q_strcmp(token, vt_names[j]))
							break;

					if (j >= V_NUM_TYPES || !((1 << j) & pc_types[i])) {
						Com_Printf("CL_ParsePtlCmds: bad type \"%s\" specified (particle %s)\n", token, name);
						numPtlCmds--;
						break;
					}

					/* get the value */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
				}

				/* set the values */
				pc->type = j;
				pc->ref = pcmdPos;
				pcmdPos += Com_ParseValue(&pcmdData[pc->ref], token, pc->type, 0);

/*				Com_Printf( "%s %s %i\n", vt_names[pc->type], token, pcmdPos - pc->ref, (char *)pc->ref ); */
				break;
			}

		if (i < PC_NUM_PTLCMDS)
			continue;

		for (pp = pps; pp->string; pp++)
			if (!Q_strcmp(token, pp->string)) {
				/* get parameter */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;


				/* translate set to a push and pop */
				pc = &ptlCmd[numPtlCmds++];
				pc->cmd = PC_PUSH;
				pc->type = pp->type;
				pc->ref = pcmdPos;
				pcmdPos += Com_ParseValue(&pcmdData[pc->ref], token, pc->type, 0);

				pc = &ptlCmd[numPtlCmds++];
				pc->cmd = PC_POP;
				pc->type = pp->type;
				pc->ref = -pp->ofs;
				break;
			}

		if (!pp->string)
			Com_Printf("CL_ParsePtlCmds: unknown token \"%s\" ignored (particle %s)\n", token, name);

	} while (*text);

	/* terminalize cmd chain */
	pc = &ptlCmd[numPtlCmds++];
	memset(pc, 0, sizeof(ptlCmd_t));
}

/**
 * @brief Searches for the particle index in ptlDef array
 * @param[in] name Name of the particle
 * @return id of the particle in ptlDef array
 */
int CL_GetParticleIndex(char *name)
{
	int i;

	/* search for menus with same name */
	for (i = 0; i < numPtlDefs; i++)
		if (!Q_strncmp(name, ptlDef[i].name, MAX_VAR))
			break;

	if (i >= numPtlDefs) {
		Com_Printf("CL_GetParticleIndex: unknown particle '%s'\n", name);
		return -1;
	}

	return i;
}

/**
 * @brief
 */
void CL_ParseParticle(char *name, char **text)
{
	char *errhead = "CL_ParseParticle: unexptected end of file (particle ";
	ptlDef_t *pd;
	char *token;
	int i;

	/* search for menus with same name */
	for (i = 0; i < numPtlDefs; i++)
		if (!Q_strncmp(name, ptlDef[i].name, MAX_VAR))
			break;

	if (i < numPtlDefs) {
		Com_Printf("CL_ParseParticle: particle def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the menu */
	pd = &ptlDef[numPtlDefs++];
	memset(pd, 0, sizeof(ptlDef_t));

	Q_strncpyz(pd->name, name, MAX_VAR);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseParticle: particle def \"%s\" without body ignored\n", name);
		numPtlDefs--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (i = 0; i < PF_NUM_PTLFUNCS; i++)
			if (!Q_strcmp(token, pf_strings[i])) {
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
		numPtlDefs--;
	}
}
