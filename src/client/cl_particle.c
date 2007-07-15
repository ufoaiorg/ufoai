/**
 * @file cl_particle.c
 * @brief Client particle parsing and rendering functions.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

static mp_t MPs[MAX_MAPPARTICLES];
int numMPs;

#define RADR(x)		((x < 0) ? (byte*)p-x : (byte*)pcmdData+x)
#define RSTACK		-0xFFF0
#define F(x)		(1<<x)
#define	V_VECS		(F(V_FLOAT) | F(V_POS) | F(V_VECTOR) | F(V_COLOR))
#define PTL_ONLY_ONE_TYPE		(1<<31)
#define V_UNTYPED   0x7FFF

typedef struct ptlCmd_s {
	byte cmd;
	byte type;
	int ref;
} ptlCmd_t;

typedef struct ptlDef_s {
	char name[MAX_VAR];
	ptlCmd_t *init, *run, *think, *round, *physics;
} ptlDef_t;


/* =========================================================== */

/** @brief particle functions enums - see pf_strings and pf_values */
typedef enum pf_s {
	PF_INIT,
	PF_RUN,
	PF_THINK,
	PF_ROUND,
	PF_PHYSICS,	/**< if a particle hit the ground */

	PF_NUM_PTLFUNCS
} pf_t;

/** @brief valid particle functions - see pf_t and pf_values */
static const char *pf_strings[PF_NUM_PTLFUNCS] = {
	"init",
	"run",
	"think",
	"round",
	"physics"
};

/** @brief particle functions offsets - see pf_strings and pf_t */
static const size_t pf_values[PF_NUM_PTLFUNCS] = {
	offsetof(ptlDef_t, init),
	offsetof(ptlDef_t, run),
	offsetof(ptlDef_t, think),
	offsetof(ptlDef_t, round),
	offsetof(ptlDef_t, physics)
};

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
	PC_SPAWN, PC_NSPAWN, PC_CHILD,

	PC_NUM_PTLCMDS
} pc_t;

/** @brief particle commands - see pc_t */
static const char *pc_strings[PC_NUM_PTLCMDS] = {
	"end",

	"push", "pop", "kpop",
	"add", "sub",
	"mul", "div",
	"sin", "cos", "tan",
	"rand", "crand",
	"v2", "v3", "v4",

	"kill",
	"spawn", "nspawn", "child"
};

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
	PTL_ONLY_ONE_TYPE | V_STRING, PTL_ONLY_ONE_TYPE | V_STRING, PTL_ONLY_ONE_TYPE | V_STRING
};

/** @brief particle script values */
static const value_t pps[] = {
	{"image", V_STRING, offsetof(ptl_t, pic), 0},
	{"model", V_STRING, offsetof(ptl_t, model), 0},
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
	{"light", V_INT, offsetof(ptl_t, light), MEMBER_SIZEOF(ptl_t, light)},
	{"physics", V_BOOL, offsetof(ptl_t, physics), MEMBER_SIZEOF(ptl_t, physics)},
	{"autohide", V_BOOL, offsetof(ptl_t, autohide), MEMBER_SIZEOF(ptl_t, autohide)},
	{"stayalive", V_BOOL, offsetof(ptl_t, stayalive), MEMBER_SIZEOF(ptl_t, stayalive)},
	{"weather", V_BOOL, offsetof(ptl_t, weather), MEMBER_SIZEOF(ptl_t, weather)},

	{NULL, 0, 0, 0}
};

/** @brief particle art type */
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

static void CL_ParticleRun2 (ptl_t *p);

/* =========================================================== */


/**
 * @brief Spawns the map particle
 * @param[in] ptl Particle name
 * @param[in] origin The position in the world
 * @param[in] wait
 * @param[in] info
 * @param[in] levelflags The levelflag mask to show the particle on
 */
void CL_AddMapParticle (const char *ptl, vec3_t origin, vec2_t wait, const char *info, int levelflags)
{
	mp_t *mp;

	mp = &MPs[numMPs];

	if (numMPs >= MAX_MAPPARTICLES) {
		Com_Printf("Too many map particles (don't add %s) - exceeded %i\n", ptl, MAX_MAPPARTICLES);
		return;
	}
	numMPs++;

	Q_strncpyz(mp->ptl, ptl, MAX_QPATH);
	VectorCopy(origin, mp->origin);
	mp->info = info;
	mp->levelflags = levelflags;
	mp->wait[0] = wait[0] * 1000;
	mp->wait[1] = wait[1] * 1000;
	mp->nextTime = cl.time + wait[0] + wait[1] * frand() + 1;

	Com_DPrintf("Adding map particle %s (%i) with levelflags %i\n", ptl, numMPs, levelflags);
}

/**
 * @brief
 */
void CL_ParticleRegisterArt (void)
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
static int CL_ParticleGetArt (const char *name, int frame, char type)
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

	a->skin = 0;
	a->type = type;
	a->frame = (type == ART_PIC) ? frame : 0;
	Q_strncpyz(a->name, name, sizeof(a->name));
	numPtlArt++;

	return numPtlArt - 1;
}

/*==============================================================================
PARTICLE EDITOR
==============================================================================*/

static int activeParticle = -1;
static char ptledit_ptlName[MAX_VAR];
static ptl_t* ptledit_ptl = NULL;
static cvar_t* ptledit_loop;

/** @brief Menu nodes that should be updated */
static struct ptleditMenu_s {
	menuNode_t* ptlName;
	menuNode_t* renderZone;
} ptleditMenu;

/**
 * @brief Set the camera values for a sequence
 * @sa CL_SequenceRender
 */
static void PE_SetCamera (void)
{
	if (!scr_vrect.width || !scr_vrect.height)
		return;

	/* set camera */
	VectorClear(cl.cam.reforg);
	cl.cam.angles[0] = 20;

	AngleVectors(cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);
	VectorMA(cl.cam.reforg, -100, cl.cam.axis[0], cl.cam.camorg);
	cl.cam.zoom = MIN_ZOOM;

	/* fudge to get isometric and perspective modes looking similar */
	if (cl_isometric->integer)
		cl.cam.zoom /= 1.35;

	CalcFovX();
}

/**
 * @brief
 */
void PE_RenderParticles (void)
{
	PE_SetCamera();
}

/**
 * @brief Updates menu nodes with current particle values
 */
static void PE_UpdateMenu (ptlDef_t* p)
{
	const char *type, *name, *text;
	static char ptlList[1024];

	FS_BuildFileList("ufos/*.ufo");
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	memset(ptlList, 0, sizeof(ptlList));
	memset(ptledit_ptlName, 0, sizeof(ptledit_ptlName));

	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != 0)
		if (!Q_strncmp(type, "particle", 8))
			Q_strcat(ptlList, va("%s\n", name), sizeof(ptlList));

	if (p)
		Q_strncpyz(ptledit_ptlName, p->name, sizeof(ptledit_ptlName));

	/* link them */
	ptleditMenu.ptlName->data[0] = ptledit_ptlName;
	menuText[TEXT_LIST] = ptlList;
}


/**
 * @brief
 */
static void PE_LoadParticle_f (void)
{
	int i;

	if (Cmd_Argc() != 2) {
		Com_Printf("usage: ptledit_load <particleid>\n");
		return;
	}

	if (ptledit_ptl)
		CL_ParticleFree(ptledit_ptl);
	ptledit_ptl = NULL;

	/* free any active particle (some don't have a kill function) */
	for (i = 0; i < numPtls; i++)
		ptl[i].inuse = qfalse;
	numPtls = 0;

	activeParticle = -1;
	for (i = 0; i < numPtlDefs; i++)
		if (!Q_strncmp(ptlDef[i].name, Cmd_Argv(1), sizeof(ptlDef[i].name)))
			activeParticle = i;

	if (activeParticle != -1) {
		PE_UpdateMenu(&ptlDef[activeParticle]);
		ptledit_ptl = CL_ParticleSpawn(ptlDef[activeParticle].name, 0, ptleditMenu.renderZone->origin, bytedirs[0], bytedirs[0]);
	} else
		PE_UpdateMenu(NULL);
}

/**
 * @brief Event function that will be called every frame
 */
static void PE_Frame_f (void)
{
	if (ptledit_ptl) {
		if (ptledit_ptl->inuse) {
			CL_ParticleRun2(ptledit_ptl);
			PE_UpdateMenu(&ptlDef[activeParticle]);
		} else {
			/* let main particles respawn if the ptledit_loop cvar is set */
			if (ptledit_loop->integer && !ptledit_ptl->parent) {
				ptledit_ptl = CL_ParticleSpawn(ptlDef[activeParticle].name, 0, ptleditMenu.renderZone->origin, bytedirs[0], bytedirs[0]);
			} else {
				if (ptledit_ptl->parent)
					Com_Printf("particle '%s' - lifetime exceeded\n", ptlDef[activeParticle].name);
				PE_UpdateMenu(NULL);
				ptledit_ptl = NULL;
			}
		}
	}
}

/**
 * @brief
 */
static void PE_ListClick_f (void)
{
	int num, i;

	if (Cmd_Argc() < 2)
		return;

	/* which particle? */
	num = atoi(Cmd_Argv(1));

	if (num < 0 || num >= numPtlDefs)
		return;

	for (i = 0; i <= num; i++) {
		/* pre defined internal particles */
		if (*ptlDef[i].name == '*')
			num++;
		if (num >= numPtlDefs)
			return;
	}
	Cbuf_ExecuteText(EXEC_NOW, va("ptledit_load %s", ptlDef[num].name));
}

/**
 * @brief particle editor commands - use particle_editor_open to activate them
 * and particle_editor_close to remove them again
 */
static const cmdList_t ptl_edit[] = {
	{"ptledit_frame", PE_Frame_f, "Renders the particle editor menu frame"},
	{"ptledit_load", PE_LoadParticle_f, "Loads the particle from buffer"},
	{"ptledit_list_click", PE_ListClick_f, "Click callback for particle list"},

	{NULL, NULL, NULL}
};

/**
 * @brief This will open up the particle editor
 */
static void CL_ParticleEditor_f (void)
{
	const cmdList_t *commands;

	if (!Q_strncmp(Cmd_Argv(0), "particle_editor_open", MAX_VAR)) {
		for (commands = ptl_edit; commands->name; commands++)
			Cmd_AddCommand(commands->name, commands->function, commands->description);
		MN_PushMenu("particle_editor");

		/* now init the menu nodes */
		ptleditMenu.ptlName = MN_GetNodeFromCurrentMenu("ptledit_name");
		if (!ptleditMenu.ptlName)
			Sys_Error("Could not find the menu node ptledit_name in particle_editor menu\n");
		ptleditMenu.ptlName->data[0] = _("No particle loaded");

		ptleditMenu.renderZone = MN_GetNodeFromCurrentMenu("render");
		if (!ptleditMenu.renderZone)
			Sys_Error("Could not find the menu node render in particle_editor menu\n");

		/* init sequence state */
		cls.state = ca_ptledit;
		cl.refresh_prepped = qtrue;

		/* init sun */
		VectorSet(map_sun.dir, 2, 2, 3);
		VectorSet(map_sun.ambient, 1.6, 1.6, 1.6);
		map_sun.ambient[3] = 5.4;
		VectorSet(map_sun.color, 1.2, 1.2, 1.2);
		map_sun.color[3] = 1.0;

		PE_UpdateMenu(NULL);
	} else {
		MN_PopMenu(qfalse);
		for (commands = ptl_edit; commands->name; commands++)
			Cmd_RemoveCommand(commands->name);
		CL_ClearState();
	}

	ptledit_loop = Cvar_Get("ptledit_loop", "1", 0, "Should particles automatically respawn in editor mode");
}

/**
 * @brief
 * @sa CL_InitLocal
 */
void CL_ResetParticles (void)
{
	numPtls = 0;
	numPtlCmds = 0;
	numPtlDefs = 0;

	numPtlArt = 0;
	pcmdPos = 0;

	Cmd_AddCommand("particle_editor_open", CL_ParticleEditor_f, "Open the particle editor");
	Cmd_AddCommand("particle_editor_close", CL_ParticleEditor_f, "Open the particle editor");
}


/**
 * @brief
 */
static void CL_ParticleFunction (ptl_t * p, ptlCmd_t * cmd)
{
	int s, e;
	int type;
	int i, j, n;
	void *radr;
	float arg;
	ptl_t *pnew;

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
			e += Com_SetValue(stackPtr[s++], radr, cmd->type, 0, 0);
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
				e -= Com_SetValue(radr, stackPtr[--s], cmd->type, 0, 0);
			else
				Com_SetValue(radr, stackPtr[s - 1], cmd->type, 0, 0);
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
			CL_ParticleFree(p);
			return;

		case PC_SPAWN:
#if 1
			if (CL_ParticleSpawn((char *) radr, p->levelFlags, p->s, p->v, p->a) == NULL)
				Com_Printf("PC_SPAWN: Could not spawn child particle for '%s'\n", p->ctrl->name);
#else
			pnew = CL_ParticleSpawn((char *) radr, p->levelFlags, p->s, p->v, p->a);
			if (pnew) {
				/* Also hand down the "angles" values. */
				VectorCopy(p->angles, pnew->angles);
			}
#endif
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

			for (i = 0; i < n; i++) {
#if 1
				if (CL_ParticleSpawn((char *) radr, p->levelFlags, p->s, p->v, p->a) == NULL)
					Com_Printf("PC_NSPAWN: Could not spawn child particle for '%s'\n", p->ctrl->name);
#else
				pnew = CL_ParticleSpawn((char *) radr, p->levelFlags, p->s, p->v, p->a);
				if (pnew) {
					/* Also hand down the "angles" values. */
					VectorCopy(p->angles, pnew->angles);
				}
#endif
			}
			break;

		case PC_CHILD:
			pnew = CL_ParticleSpawn((char *)radr, p->levelFlags, p->s, p->v, p->a);
			if (pnew) {
				pnew->next = p->children;
				pnew->parent = p;
				p->children = pnew;
			} else {
				Com_Printf("PC_CHILD: Could not spawn child particle for '%s'\n", p->ctrl->name);
			}
			break;

		default:
			Sys_Error("CL_ParticleFunction: unknown cmd type %i\n", cmd->type);
			break;
		}
	}
}


/**
 * @brief Spawn a new particle to the map
 * @param[in] name The id of the particle (see ptl_*.ufo script files in base/ufos)
 * @param[in] levelFlags Show at which levels
 * @param[in] s starting/location vector
 * @param[in] v velocity vector
 * @param[in] a acceleration vector
 * @sa CL_ParticleFree
 * @sa V_UpdateRefDef
 * @sa R_DrawPtls
 */
ptl_t *CL_ParticleSpawn (const char *name, int levelFlags, const vec3_t s, const vec3_t v, const vec3_t a)
{
	ptlDef_t *pd;
	ptl_t *p;
	int i;

	if (!name || strlen(name) <= 0)
		return NULL;

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
			Com_Printf("Too many particles (don't add %s) - exceeded %i\n", name, MAX_PTLS);
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
	Vector4Set(p->color, 1.0f, 1.0f, 1.0f, 1.0f);

	p->pic = -1;
	p->model = -1;

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

	return p;
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
 * @brief Spawn a particle given by EV_SPAWN_PARTICLE event
 * @param[in] sb sizebuf that holds the network transfer
 * @sa CL_ParticleSpawn
 */
void CL_ParticleSpawnFromSizeBuf (struct dbuffer *msg)
{
	char *particle;
	int levelflags, i;
	pos3_t originPos;
	vec3_t origin;

	levelflags = NET_ReadShort(msg);
	NET_ReadGPos(msg, originPos);
	for (i = 0; i < 3; i++)
		origin[i] = (float)originPos[i];

	NET_ReadByte(msg); /* for stringlength */
	particle = NET_ReadString(msg);

	if (particle && Q_strcmp(particle, "null")) {
		CL_ParticleSpawn(particle, levelflags, origin, NULL, NULL);
	}
}

/**
 * @brief
 */
static void CL_Fading (vec4_t color, byte fade, float frac, qboolean onlyAlpha)
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
void CL_ParticleCheckRounds (void)
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
					CL_ParticleFree(p);
			}
		}
}

/**
 * @brief
 * @sa CL_ParticleRun
 */
static void CL_ParticleRun2 (ptl_t *p)
{
	qboolean onlyAlpha;
	trace_t tr;
	int z, oldLevel;

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
	/* don't play the weather particles if a user don't want them
	 * there can be a lot of weather particles - which might slow the computer
	 * down - so i made them switchable */
	} else if (p->weather && !cl_particleWeather->integer) {
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

	/* run */
	CL_ParticleFunction(p, p->ctrl->run);

	/* think */
	while (p->tps && p->lastThink * p->tps >= 1) {
		CL_ParticleFunction(p, p->ctrl->think);
		p->lastThink -= 1.0 / p->tps;
	}

	if (p->light)
		V_AddLight(p->s, p->light, p->color[0], p->color[1], p->color[2]);

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
			Vector4Set(p->color, 1.0f, 1.0f, 1.0f, 1.0f);
		} else
			p->color[3] = 1.0;
			if (p->thinkFade)
				CL_Fading(p->color, p->thinkFade, p->lastThink * p->tps, p->blend == BLEND_BLEND);
			if (p->frameFade)
				CL_Fading(p->color, p->frameFade, p->lastFrame * p->fps, p->blend == BLEND_BLEND);
	}

	/* this is useful for particles like weather effects that are on top of
	 * some other brushes in higher level but should be visible in lower ones */
	if (p->autohide) {
		z = (int)p->s[2] / UNIT_HEIGHT;
		if (z > cl_worldlevel->integer) {
			p->invis = qtrue;
			return;
		} else if (z < 0) {
			CL_ParticleFree(p);
			return;
		}
	}

	/* basic 'physics' for particles */
	if (p->physics) {
		oldLevel = cl_worldlevel->integer;
		cl_worldlevel->integer = map_maxlevel - 1;
		tr = CL_Trace(p->origin, p->s, vec3_origin, vec3_origin, NULL, NULL, MASK_SOLID);
		cl_worldlevel->integer = oldLevel;

		/* hit something solid */
		if (tr.fraction < 1.0 || tr.startsolid) {
			/* now execute the physics handler */
			if (p->ctrl->physics)
				CL_ParticleFunction(p, p->ctrl->physics);
			/* let them stay on the ground until they fade out or die */
			if (!p->stayalive) {
				CL_ParticleFree(p);
			} else {
				VectorCopy(vec3_origin, p->v);
				VectorCopy(tr.endpos, p->s);
			}
			return;
		}
	}
	p->invis = qfalse;
}

/**
 * @brief
 * @sa CL_Frame
 */
void CL_ParticleRun (void)
{
	ptl_t *p;
	int i;

	if (cls.state != ca_active)
		return;

	for (i = 0, p = ptl; i < numPtls; i++, p++)
		if (p->inuse)
			CL_ParticleRun2(p);
}


/* =========================================================== */

/**
 * @brief
 */
static void CL_ParseMapParticle (ptl_t * ptl, const char *es, qboolean afterwards)
{
	char keyname[MAX_QPATH];
	const char *token;
	char *key;
	const value_t *pp;

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
				Com_ParseValue(ptl, token, pp->type, pp->ofs, pp->size);
				break;
			}
	} while (token);
}


/**
 * @brief
 */
void CL_RunMapParticles (void)
{
	mp_t *mp;
	ptl_t *ptl;
	int i;

	if (cls.state != ca_active)
		return;

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
static void CL_ParsePtlCmds (const char *name, const char **text)
{
	ptlCmd_t *pc;
	const value_t *pp;
	const char *errhead = "CL_ParsePtlCmds: unexpected end of file";
	const char *token;
	int i, j;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParsePtlCmds: particle cmds \"%s\" without body ignored\n", name);
		if (numPtlCmds >= MAX_PTLCMDS)
			Sys_Error("CL_ParsePtlCmds: MAX_PTLCMDS exceeded\n");
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
				if (numPtlCmds >= MAX_PTLCMDS)
					Sys_Error("CL_ParsePtlCmds: MAX_PTLCMDS exceeded\n");
				pc = &ptlCmd[numPtlCmds++];
				memset(pc, 0, sizeof(ptlCmd_t));

				pc->cmd = i;

				if (!pc_types[i])
					break;

				/* get parameter type */
				token = COM_EParse(text, errhead, name);
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
						if (!Q_strcmp(baseComponentToken, pp->string))
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
							int component = (baseComponentToken[len - 1] - '1');
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
				pcmdPos += Com_ParseValue(&pcmdData[pc->ref], token, pc->type, 0, 0);

/*				Com_Printf("%s %s %i\n", vt_names[pc->type], token, pcmdPos - pc->ref, (char *)pc->ref); */
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
				if (numPtlCmds >= MAX_PTLCMDS)
					Sys_Error("CL_ParsePtlCmds: MAX_PTLCMDS exceeded\n");
				pc = &ptlCmd[numPtlCmds++];
				pc->cmd = PC_PUSH;
				pc->type = pp->type;
				pc->ref = pcmdPos;
				pcmdPos += Com_ParseValue(&pcmdData[pc->ref], token, pc->type, 0, 0);

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
		Sys_Error("CL_ParsePtlCmds: MAX_PTLCMDS exceeded\n");
	pc = &ptlCmd[numPtlCmds++];
	memset(pc, 0, sizeof(ptlCmd_t));
}

#if 0
/**
 * @brief Searches for the particle index in ptlDef array
 * @param[in] name Name of the particle
 * @return id of the particle in ptlDef array
 */
int CL_GetParticleIndex (const char *name)
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
#endif

/**
 * @brief Parses particle definitions from UFO-script files
 * @param[in] name particle name/id
 * @param[in] text pointer to the buffer to parse from
 * @return the position of the particle in ptlDef array
 * @sa CL_ParseClientData
 */
int CL_ParseParticle (const char *name, const char **text)
{
	const char *errhead = "CL_ParseParticle: unexpected end of file (particle ";
	ptlDef_t *pd;
	const char *token;
	int i, pos;

	/* search for particles with same name */
	for (i = 0; i < numPtlDefs; i++)
		if (!Q_strncmp(name, ptlDef[i].name, MAX_VAR))
			break;

	if (i < numPtlDefs) {
		Com_Printf("CL_ParseParticle: particle def \"%s\" with same name found, reset first one\n", name);
		pos = i;
		pd = &ptlDef[i];
	} else {
		if (numPtlDefs < MAX_PTLDEFS - 1) {
			/* initialize the new particle */
			pos = numPtlDefs;
			pd = &ptlDef[numPtlDefs++];
		} else {
			Com_Printf("CL_ParseParticle: max particle definitions reached - skip the current one: '%s'\n", name);
			return -1;
		}
	}
	memset(pd, 0, sizeof(ptlDef_t));

	Q_strncpyz(pd->name, name, MAX_VAR);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseParticle: particle def \"%s\" without body ignored\n", name);
		if (i == numPtlDefs)
			numPtlDefs--;
		return -1;
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
		if (i == numPtlDefs)
			numPtlDefs--;
		return -1;
	}
	return pos;
}

/**
 * @brief Inits some fixed particles that are not in our script files
 * @note fixed particles start with a * in their name
 */
void CL_InitParticles (void)
{
	ptlDef_t *pd;

	pd = &ptlDef[numPtlDefs++];
	memset(pd, 0, sizeof(ptlDef_t));
	Q_strncpyz(pd->name, "*circle", MAX_VAR);
}
