/**
 * @file cl_sequence.c
 * @brief Non-interactive sequence rendering and AVI recording.
 * @note Sequences are rendered on top of a menu node - the default menu
 * is stored in mn_sequence cvar
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "cl_sequence.h"
#include "battlescape/cl_localentity.h"
#include "battlescape/cl_view.h"
#include "renderer/r_draw.h"
#include "renderer/r_mesh_anim.h"
#include "../shared/parse.h"
#include "ui/ui_main.h"
#include "ui/ui_nodes.h"
#include "ui/ui_render.h"

#define MAX_DATA_LENGTH 2048

typedef struct seqCmd_s {
	/** @returns 0 if the same command should be executed again - or 1 to execute the next event */
	int (*handler) (const char *name, const char *data);
	char name[MAX_VAR];
	char data[MAX_DATA_LENGTH];
} seqCmd_t;

typedef struct sequence_s {
	char name[MAX_VAR];	/**< the name of the sequence (script id) */
	int start;		/**< the start in the sequence command buffer for this sequence */
	int length;		/**< the amount of sequence commands */
} sequence_t;

typedef enum {
	SEQ_END,
	SEQ_WAIT,
	SEQ_CLICK,
	SEQ_PRECACHE,
	SEQ_CAMERA,
	SEQ_MODEL,
	SEQ_2DOBJ,
	SEQ_MUSIC,
	SEQ_SOUND,
	SEQ_REM,
	SEQ_CMD,

	SEQ_NUMCMDS
} seqCmdEnum_t;

static const char *seqCmdName[] = {
	"end",
	"wait",
	"click",
	"precache",
	"camera",
	"model",
	"2dobj",
	"music",
	"sound",
	"rem",
	"cmd"
};
CASSERT(lengthof(seqCmdName) == SEQ_NUMCMDS);

typedef struct seqCamera_s {
	vec3_t origin;
	vec3_t speed;
	vec3_t angles;
	vec3_t omega;
	float dist;		/**< the distance of the camera */
	float ddist;	/**< will change the distance over time with this factor */
	float zoom;		/**< the zoom level of the camera */
	float dzoom;	/**< will change the zoom level over time with this factor */
} seqCamera_t;

/**
 * Render entities that represents an (animated) model
 */
typedef struct seqEnt_s {
	qboolean inuse;
	char name[MAX_VAR];
	model_t *model;		/**< the loaded model structure */
	int skin;			/**< the skin number of the model - starts at 0 for the first one */
	vec3_t origin;		/**< the origin of the model */
	vec3_t speed;		/**< advance origin in time with this speed */
	vec3_t angles;		/**< the rotation of the model */
	vec3_t omega;		/**< advance angles in time with this omega */
	vec3_t color;
	float alpha;
	char parent[MAX_VAR];	/**< in case this is a child model (should get placed onto a tag of the parent) */
	char tag[MAX_VAR];		/**< the tag to place this model onto */
	animState_t as;			/**< the current animation state */
	entity_t *ep;
} seqEnt_t;

/**
 * @brief Represents a text object or image object
 */
typedef struct seq2D_s {
	qboolean inuse;			/**< still in use in this sequence? or already deactivated? */
	char name[MAX_VAR];		/**< the script id for this object */
	char *text;				/**< a placeholder for gettext (V_TRANSLATION_STRING) */
	char font[MAX_VAR];		/**< the font to use in case this is a text object */
	char image[MAX_VAR];	/**< the image to render */
	vec2_t pos;			/**< the position of the 2d obj */
	vec2_t speed;		/**< how fast the 2d obj will change (fade, scale, ...) */
	vec2_t size;		/**< the size of the 2d obj */
	vec2_t enlarge;		/**< enlarge in x and y direction */
	vec4_t color;		/**< 2d obj color */
	vec4_t fade;		/**< the fade color */
	vec4_t bgcolor;		/**< background color of the box define by @c pos, @c size */
	align_t align;		/**< the alignment of the 2d obj */
	qboolean relativePos;	/**< useful for translations when sentence length may differ */
} seq2D_t;

static int SEQ_Click(const char *name, const char *data);
static int SEQ_Wait(const char *name, const char *data);
static int SEQ_Precache(const char *name, const char *data);
static int SEQ_Camera(const char *name, const char *data);
static int SEQ_Model(const char *name, const char *data);
static int SEQ_2Dobj(const char *name, const char *data);
static int SEQ_Music(const char *name, const char *data);
static int SEQ_Sound(const char *name, const char *data);
static int SEQ_Remove(const char *name, const char *data);
static int SEQ_Command(const char *name, const char *data);

static int (*seqCmdFunc[]) (const char *name, const char *data) = {
	NULL,
	SEQ_Wait,
	SEQ_Click,
	SEQ_Precache,
	SEQ_Camera,
	SEQ_Model,
	SEQ_2Dobj,
	SEQ_Music,
	SEQ_Sound,
	SEQ_Remove,
	SEQ_Command
};
CASSERT(lengthof(seqCmdFunc) == SEQ_NUMCMDS);

#define MAX_SEQCMDS		8192
#define MAX_SEQUENCES	32
#define MAX_SEQENTS		128
#define MAX_SEQ2DS		128

static seqCmd_t seqCmds[MAX_SEQCMDS];
static int numSeqCmds;

static sequence_t sequences[MAX_SEQUENCES];
static int numSequences;

static int seqTime;	/**< milliseconds the sequence is already running */
static qboolean seqEndClickLoop = qfalse; /**< if the menu node the sequence is rendered in fetches a click this is true */
static int seqCmd;	/**< current position in the sequence command array of the current running sequence */
static int seqEndCmd; /**< the number of all sequence commands in the current running sequence */

static seqCamera_t seqCamera;

static seqEnt_t seqEnts[MAX_SEQENTS];
static int numSeqEnts;

static seq2D_t seq2Ds[MAX_SEQ2DS];
static int numSeq2Ds;

static cvar_t *seq_animspeed;


/**
 * @brief Sets the client state to ca_disconnected
 * @sa CL_SequenceStart_f
 */
static void CL_SequenceEnd_f (void)
{
	if (cls.state == ca_sequence)
		CL_SetClientState(ca_disconnected);
}


/**
 * @brief Set the camera values for a sequence
 * @sa CL_SequenceRender
 */
static void CL_SequenceCamera (void)
{
	if (!viddef.viewWidth || !viddef.viewHeight)
		return;

	/* advance time */
	VectorMA(seqCamera.origin, cls.frametime, seqCamera.speed, seqCamera.origin);
	VectorMA(seqCamera.angles, cls.frametime, seqCamera.omega, seqCamera.angles);
	seqCamera.zoom += cls.frametime * seqCamera.dzoom;
	seqCamera.dist += cls.frametime * seqCamera.ddist;

	/* set camera */
	VectorCopy(seqCamera.origin, cl.cam.origin);
	VectorCopy(seqCamera.angles, cl.cam.angles);

	AngleVectors(cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);
	VectorMA(cl.cam.origin, -seqCamera.dist, cl.cam.axis[0], cl.cam.camorg);
	cl.cam.zoom = max(seqCamera.zoom, MIN_ZOOM);
	/* fudge to get isometric and perspective modes looking similar */
	if (cl_isometric->integer)
		cl.cam.zoom /= 1.35;
	CL_ViewCalcFieldOfViewX();
}


/**
 * @brief Finds a given entity in all sequence entities
 * @sa CL_SequenceFind2D
 */
static seqEnt_t *CL_SequenceFindEnt (const char *name)
{
	seqEnt_t *se;
	int i;

	for (i = 0, se = seqEnts; i < numSeqEnts; i++, se++)
		if (se->inuse && !strncmp(se->name, name, MAX_VAR))
			break;
	if (i < numSeqEnts)
		return se;
	else
		return NULL;
}


/**
 * @brief Finds a given 2d object in the current sequence data
 * @sa CL_SequenceFindEnt
 */
static seq2D_t *CL_SequenceFind2D (const char *name)
{
	seq2D_t *s2d;
	int i;

	for (i = 0, s2d = seq2Ds; i < numSeq2Ds; i++, s2d++)
		if (s2d->inuse && !strcmp(s2d->name, name))
			break;
	if (i < numSeq2Ds)
		return s2d;
	else
		return NULL;
}


/**
 * @sa CL_Sequence2D
 * @sa CL_ViewRender
 * @sa CL_SequenceEnd_f
 * @sa UI_PopWindow
 * @sa CL_SequenceFindEnt
 */
void CL_SequenceRender (void)
{
	entity_t ent;
	seqCmd_t *sc;
	seqEnt_t *se;
	int i;

	/* we are inside a waiting command */
	if (seqTime > cl.time) {
		/* if we clicked a button we end the waiting loop */
		if (seqEndClickLoop) {
			seqTime = cl.time;
			seqEndClickLoop = qfalse;
		}
	}

	/* run script */
	while (seqTime <= cl.time) {
		/* test for finish */
		if (seqCmd >= seqEndCmd) {
			CL_SequenceEnd_f();
			UI_PopWindow(qfalse);
			return;
		}

		/* call handler */
		sc = &seqCmds[seqCmd];
		seqCmd += sc->handler(sc->name, sc->data);
	}

	/* set camera */
	CL_SequenceCamera();

	/* render sequence */
	for (i = 0, se = seqEnts; i < numSeqEnts; i++, se++)
		if (se->inuse) {
			/* advance in time */
			VectorMA(se->origin, cls.frametime, se->speed, se->origin);
			VectorMA(se->angles, cls.frametime, se->omega, se->angles);
			R_AnimRun(&se->as, se->model, seq_animspeed->integer * cls.frametime);

			/* add to scene */
			memset(&ent, 0, sizeof(ent));
			ent.model = se->model;
			ent.skinnum = se->skin;
			ent.as = se->as;
			ent.alpha = se->alpha;

			VectorCopy(se->origin, ent.origin);
			VectorCopy(se->origin, ent.oldorigin);
			VectorCopy(se->angles, ent.angles);

			if (se->parent && se->tag) {
				seqEnt_t *parent;

				parent = CL_SequenceFindEnt(se->parent);
				if (parent)
					ent.tagent = parent->ep;
				ent.tagname = se->tag;
			}

			/* add to render list */
			se->ep = R_GetFreeEntity();
			R_AddEntity(&ent);
		}
}


/**
 * @brief Renders text and images
 * @sa SEQ_InitStartup
 */
void CL_Sequence2D (void)
{
	seq2D_t *s2d;
	int i, j;
	int height = 0;
	vec3_t pos;

	/* center screen */
	pos[0] = (viddef.virtualWidth - VID_NORM_WIDTH) / 2;
	pos[1] = (viddef.virtualHeight - VID_NORM_HEIGHT) / 2;
	pos[2] = 0;
	UI_Transform(pos, NULL, NULL);

	/* add texts */
	for (i = 0, s2d = seq2Ds; i < numSeq2Ds; i++, s2d++)
		if (s2d->inuse) {
			if (s2d->relativePos && height > 0) {
				s2d->pos[1] += height;
				s2d->relativePos = qfalse;
			}
			/* advance in time */
			for (j = 0; j < 4; j++) {
				s2d->color[j] += cls.frametime * s2d->fade[j];
				if (s2d->color[j] < 0.0)
					s2d->color[j] = 0.0;
				else if (s2d->color[j] > 1.0)
					s2d->color[j] = 1.0;
			}
			for (j = 0; j < 2; j++) {
				s2d->pos[j] += cls.frametime * s2d->speed[j];
				s2d->size[j] += cls.frametime * s2d->enlarge[j];
			}

			/* outside the screen? */
			/** @todo We need this check - but this does not work */
			/*if (s2d->pos[1] >= VID_NORM_HEIGHT || s2d->pos[0] >= VID_NORM_WIDTH)
				continue;*/

			/* render */
			R_Color(s2d->color);

			/* image can be background */
			if (s2d->image[0] != '\0') {
				const image_t *image = R_FindImage(s2d->image, it_pic);
				R_DrawImage(s2d->pos[0], s2d->pos[1], image);
			}

			/* bgcolor can be overlay */
			if (s2d->bgcolor[3] > 0.0)
				R_DrawFill(s2d->pos[0], s2d->pos[1], s2d->size[0], s2d->size[1], s2d->bgcolor);

			/* render */
			R_Color(s2d->color);

			/* gettext placeholder */
			if (s2d->text) {
				int maxWidth = (int) s2d->size[0];
				if (maxWidth <= 0)
					maxWidth = VID_NORM_WIDTH;
				height += UI_DrawString(s2d->font, s2d->align, s2d->pos[0], s2d->pos[1], s2d->pos[0], maxWidth, -1 /** @todo use this for some nice line spacing */, _(s2d->text), 0, 0, NULL, qfalse, LONGLINES_WRAP);
			}
		}
	R_Color(NULL);

	UI_Transform(NULL, NULL, NULL);
}

/**
 * @brief Unlock a click event for the current sequence or ends the current sequence if not locked
 * @note Script binding for seq_click
 * @sa menu sequence in menu_main.ufo
 */
static void CL_SequenceClick_f (void)
{
	seqEndClickLoop = qtrue;
}

/**
 * @brief Start a sequence
 * @sa CL_SequenceEnd_f
 */
static void CL_SequenceStart_f (void)
{
	sequence_t *sp;
	const char *name, *menuName;
	int i;
	const uiNode_t* menu;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <name> [<menu>]\n", Cmd_Argv(0));
		return;
	}
	name = Cmd_Argv(1);

	/* find sequence */
	for (i = 0, sp = sequences; i < numSequences; i++, sp++)
		if (!strcmp(name, sp->name))
			break;
	if (i >= numSequences) {
		Com_Printf("Couldn't find sequence '%s'\n", name);
		return;
	}

	/* display the sequence menu */
	/* the default is in menu_main.ufo - menu sequence */
	menuName = Cmd_Argc() < 3 ? mn_sequence->string : Cmd_Argv(2);
	menu = UI_PushWindow(menuName, NULL);
	if (!menu) {
		Com_Printf("CL_SequenceStart_f: can't display menu '%s'\n", menuName);
		return;
	}

	/* init script parsing */
	numSeqEnts = 0;
	numSeq2Ds = 0;
	memset(&seqCamera, 0, sizeof(seqCamera));
	seqTime = cl.time;
	seqCmd = sp->start;
	seqEndCmd = sp->start + sp->length;

	/* If running a local server, kill it */
	SV_Shutdown("Server quit", qfalse);
	/* if still connected - disconnect */
	CL_Disconnect();

	/* init sequence state */
	CL_SetClientState(ca_sequence);
}


void SEQ_InitStartup (void)
{
	/* reset counters */
	seq_animspeed = Cvar_Get("seq_animspeed", "1000", 0, NULL);
	Cmd_AddCommand("seq_click", CL_SequenceClick_f, NULL);
	Cmd_AddCommand("seq_start", CL_SequenceStart_f, NULL);
	Cmd_AddCommand("seq_end", CL_SequenceEnd_f, NULL);
}


/* =========================================================== */

/** @brief valid id names for camera */
static const value_t seqCamera_vals[] = {
	{"origin", V_VECTOR, offsetof(seqCamera_t, origin), MEMBER_SIZEOF(seqCamera_t, origin)},
	{"speed", V_VECTOR, offsetof(seqCamera_t, speed), MEMBER_SIZEOF(seqCamera_t, speed)},
	{"angles", V_VECTOR, offsetof(seqCamera_t, angles), MEMBER_SIZEOF(seqCamera_t, angles)},
	{"omega", V_VECTOR, offsetof(seqCamera_t, omega), MEMBER_SIZEOF(seqCamera_t, omega)},
	{"dist", V_FLOAT, offsetof(seqCamera_t, dist), MEMBER_SIZEOF(seqCamera_t, dist)},
	{"ddist", V_FLOAT, offsetof(seqCamera_t, ddist), MEMBER_SIZEOF(seqCamera_t, ddist)},
	{"zoom", V_FLOAT, offsetof(seqCamera_t, zoom), MEMBER_SIZEOF(seqCamera_t, zoom)},
	{"dzoom", V_FLOAT, offsetof(seqCamera_t, dzoom), MEMBER_SIZEOF(seqCamera_t, dzoom)},
	{NULL, 0, 0, 0}
};

/** @brief valid entity names for a sequence */
static const value_t seqEnt_vals[] = {
	{"name", V_STRING, offsetof(seqEnt_t, name), 0},
	{"skin", V_INT, offsetof(seqEnt_t, skin), MEMBER_SIZEOF(seqEnt_t, skin)},
	{"alpha", V_FLOAT, offsetof(seqEnt_t, alpha), MEMBER_SIZEOF(seqEnt_t, alpha)},
	{"origin", V_VECTOR, offsetof(seqEnt_t, origin), MEMBER_SIZEOF(seqEnt_t, origin)},
	{"speed", V_VECTOR, offsetof(seqEnt_t, speed), MEMBER_SIZEOF(seqEnt_t, speed)},
	{"angles", V_VECTOR, offsetof(seqEnt_t, angles), MEMBER_SIZEOF(seqEnt_t, angles)},
	{"omega", V_VECTOR, offsetof(seqEnt_t, omega), MEMBER_SIZEOF(seqEnt_t, omega)},
	{"color", V_VECTOR, offsetof(seqEnt_t, color), MEMBER_SIZEOF(seqEnt_t, color)},
	{"parent", V_STRING, offsetof(seqEnt_t, parent), 0},
	{"tag", V_STRING, offsetof(seqEnt_t, tag), 0},
	{NULL, 0, 0, 0}
};

/** @brief valid id names for 2d entity */
static const value_t seq2D_vals[] = {
	{"name", V_STRING, offsetof(seq2D_t, name), 0},
	{"text", V_TRANSLATION_STRING, offsetof(seq2D_t, text), 0},
	{"font", V_STRING, offsetof(seq2D_t, font), 0},
	{"image", V_STRING, offsetof(seq2D_t, image), 0},
	{"pos", V_POS, offsetof(seq2D_t, pos), MEMBER_SIZEOF(seq2D_t, pos)},
	{"speed", V_POS, offsetof(seq2D_t, speed), MEMBER_SIZEOF(seq2D_t, speed)},
	{"size", V_POS, offsetof(seq2D_t, size), MEMBER_SIZEOF(seq2D_t, size)},
	{"enlarge", V_POS, offsetof(seq2D_t, enlarge), MEMBER_SIZEOF(seq2D_t, enlarge)},
	{"bgcolor", V_COLOR, offsetof(seq2D_t, bgcolor), MEMBER_SIZEOF(seq2D_t, bgcolor)},
	{"color", V_COLOR, offsetof(seq2D_t, color), MEMBER_SIZEOF(seq2D_t, color)},
	{"fade", V_COLOR, offsetof(seq2D_t, fade), MEMBER_SIZEOF(seq2D_t, fade)},
	{"align", V_ALIGN, offsetof(seq2D_t, align), MEMBER_SIZEOF(seq2D_t, align)},
	{"relative", V_BOOL, offsetof(seq2D_t, relativePos), MEMBER_SIZEOF(seq2D_t, relativePos)},
	{NULL, 0, 0, 0},
};

/**
 * @brief Wait until someone clicks with the mouse
 * @return 0 if you wait for the click
 * @return 1 if the click occurred
 */
static int SEQ_Click (const char *name, const char *data)
{
	/* if a CL_SequenceClick_f event was called */
	if (seqEndClickLoop) {
		seqEndClickLoop = qfalse;
		/* increase the command counter by 1 */
		return 1;
	}
	seqTime += 1000;
	/* don't increase the command counter - stay at click command */
	return 0;
}

/**
 * @brief Increase the sequence time
 * @return 1 - increase the command position of the sequence by one
 */
static int SEQ_Wait (const char *name, const char *data)
{
	seqTime += 1000 * atof(name);
	return 1;
}

/**
 * @brief Precaches the models and images for a sequence
 * @return 1 - increase the command position of the sequence by one
 * @sa R_RegisterModelShort
 * @sa R_RegisterImage
 */
static int SEQ_Precache (const char *name, const char *data)
{
	if (!strcmp(name, "models")) {
		while (*data) {
			Com_DPrintf(DEBUG_CLIENT, "Precaching model: %s\n", data);
			R_RegisterModelShort(data);
			data += strlen(data) + 1;
		}
	} else if (!strcmp(name, "pics")) {
		while (*data) {
			Com_DPrintf(DEBUG_CLIENT, "Precaching image: %s\n", data);
			R_RegisterImage(data);
			data += strlen(data) + 1;
		}
	} else
		Com_Printf("SEQ_Precache: unknown format '%s'\n", name);
	return 1;
}

/**
 * @brief Parse the values for the camera like given in seqCamera
 */
static int SEQ_Camera (const char *name, const char *data)
{
	/* get values */
	while (*data) {
		const value_t *vp;
		for (vp = seqCamera_vals; vp->string; vp++)
			if (!strcmp(data, vp->string)) {
				data += strlen(data) + 1;
				Com_EParseValue(&seqCamera, data, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!vp->string)
			Com_Printf("SEQ_Camera: unknown token '%s'\n", data);

		data += strlen(data) + 1;
	}
	return 1;
}

/**
 * @brief Parse values for a sequence model
 * @return 1 - increase the command position of the sequence by one
 * @sa seqEnt_vals
 * @sa CL_SequenceFindEnt
 */
static int SEQ_Model (const char *name, const char *data)
{
	/* get sequence entity */
	seqEnt_t *se = CL_SequenceFindEnt(name);
	if (!se) {
		int i;
		/* create new sequence entity */
		for (i = 0, se = seqEnts; i < numSeqEnts; i++, se++)
			if (!se->inuse)
				break;
		if (i >= numSeqEnts) {
			if (numSeqEnts >= MAX_SEQENTS)
				Com_Error(ERR_FATAL, "Too many sequence entities");
			se = &seqEnts[numSeqEnts++];
		}
		/* allocate */
		memset(se, 0, sizeof(*se));
		se->inuse = qtrue;
		Q_strncpyz(se->name, name, sizeof(se->name));
		VectorSet(se->color, 0.7, 0.7, 0.7);
	}

	/* get values */
	while (*data) {
		const value_t *vp;
		for (vp = seqEnt_vals; vp->string; vp++)
			if (!strcmp(data, vp->string)) {
				data += strlen(data) + 1;
				Com_EParseValue(se, data, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!vp->string) {
			if (!strcmp(data, "model")) {
				data += strlen(data) + 1;
				Com_DPrintf(DEBUG_CLIENT, "Registering model: %s\n", data);
				se->model = R_RegisterModelShort(data);
			} else if (!strcmp(data, "anim")) {
				data += strlen(data) + 1;
				Com_DPrintf(DEBUG_CLIENT, "Change anim to: %s\n", data);
				R_AnimChange(&se->as, se->model, data);
			} else
				Com_Printf("SEQ_Model: unknown token '%s'\n", data);
		}

		data += strlen(data) + 1;
	}
	return 1;
}

/**
 * @brief Changes the music in the sequence
 * @return 1 - increase the command position of the sequence by one
 */
static int SEQ_Music (const char *name, const char *data)
{
	Com_Printf("change music to %s\n", name);
	Cvar_Set("snd_music", name);
	return 1;
}

/**
 * @brief Plays a sound in a sequence
 * @return 1 - increase the command position of the sequence by one
 */
static int SEQ_Sound (const char *name, const char *data)
{
	S_StartLocalSample(name, SND_VOLUME_DEFAULT);
	return 1;
}

/**
 * @brief Parse 2D objects like text and images
 * @return 1 - increase the command position of the sequence by one
 * @sa seq2D_vals
 * @sa CL_SequenceFind2D
 */
static int SEQ_2Dobj (const char *name, const char *data)
{
	seq2D_t *s2d;
	const value_t *vp;
	int i;

	/* get sequence text */
	s2d = CL_SequenceFind2D(name);
	if (!s2d) {
		/* create new sequence text */
		for (i = 0, s2d = seq2Ds; i < numSeq2Ds; i++, s2d++)
			if (!s2d->inuse)
				break;
		if (i >= numSeq2Ds) {
			if (numSeq2Ds >= MAX_SEQ2DS)
				Com_Error(ERR_FATAL, "Too many sequence 2d objects");
			s2d = &seq2Ds[numSeq2Ds++];
		}
		/* allocate */
		memset(s2d, 0, sizeof(*s2d));
		for (i = 0; i < 4; i++)
			s2d->color[i] = 1.0f;
		s2d->inuse = qtrue;
		Q_strncpyz(s2d->font, "f_big", sizeof(s2d->font));	/* default font */
		Q_strncpyz(s2d->name, name, sizeof(s2d->name));
	}

	/* get values */
	while (*data) {
		for (vp = seq2D_vals; vp->string; vp++)
			if (!strcmp(data, vp->string)) {
				data += strlen(data) + 1;
				switch (vp->type) {
				case V_TRANSLATION_STRING:
					data++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(data, (char**) ((char*)s2d + (int)vp->ofs), cl_genericPool, 0);
					break;

				default:
					Com_EParseValue(s2d, data, vp->type, vp->ofs, vp->size);
					break;
				}
				break;
			}
		if (!vp->string)
			Com_Printf("SEQ_Text: unknown token '%s'\n", data);

		data += strlen(data) + 1;
	}
	return 1;
}

/**
 * @brief Removed a sequence entity from the current sequence
 * @return 1 - increase the command position of the sequence by one
 * @sa CL_SequenceFind2D
 * @sa CL_SequenceFindEnt
 */
static int SEQ_Remove (const char *name, const char *data)
{
	seqEnt_t *se;
	seq2D_t *s2d;

	se = CL_SequenceFindEnt(name);
	if (se)
		se->inuse = qfalse;

	s2d = CL_SequenceFind2D(name);
	if (s2d) {
		s2d->inuse = qfalse;

		if (s2d->text) {
			Mem_Free(s2d->text);
			s2d->text = NULL;
		}
	}

	if (!se && !s2d)
		Com_Printf("SEQ_Remove: couldn't find '%s'\n", name);
	return 1;
}

/**
 * @brief Executes a sequence command
 * @return 1 - increase the command position of the sequence by one
 * @sa Cbuf_AddText
 */
static int SEQ_Command (const char *name, const char *data)
{
	/* add the command */
	Cbuf_AddText(name);
	return 1;
}

/**
 * @brief Reads the sequence values from given text-pointer
 * @sa CL_ParseClientData
 */
void CL_ParseSequence (const char *name, const char **text)
{
	const char *errhead = "CL_ParseSequence: unexpected end of file (sequence ";
	sequence_t *sp;
	const char *token;
	int i;

	/* search for sequences with same name */
	for (i = 0; i < numSequences; i++)
		if (!strcmp(name, sequences[i].name))
			break;

	if (i < numSequences) {
		Com_Printf("CL_ParseSequence: sequence def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the sequence */
	if (numSequences >= MAX_SEQUENCES)
		Com_Error(ERR_FATAL, "Too many sequences");

	sp = &sequences[numSequences++];
	memset(sp, 0, sizeof(*sp));
	Q_strncpyz(sp->name, name, sizeof(sp->name));
	sp->start = numSeqCmds;

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseSequence: sequence def \"%s\" without body ignored\n", name);
		numSequences--;
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
	next_cmd:
		if (*token == '}')
			break;

		/* check for commands */
		for (i = 0; i < SEQ_NUMCMDS; i++)
			if (!strcmp(token, seqCmdName[i])) {
				int maxLength = MAX_DATA_LENGTH;
				int depth;
				char *data;
				seqCmd_t *sc;

				/* found a command */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				if (numSeqCmds >= MAX_SEQCMDS)
					Com_Error(ERR_FATAL, "Too many sequence commands");

				/* init seqCmd */
				sc = &seqCmds[numSeqCmds++];
				memset(sc, 0, sizeof(*sc));
				sc->handler = seqCmdFunc[i];
				sp->length++;

				/* copy name */
				Q_strncpyz(sc->name, token, sizeof(sc->name));

				/* read data */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;
				if (*token != '{')
					goto next_cmd;

				depth = 1;
				data = &sc->data[0];
				while (depth) {
					if (maxLength <= 0) {
						Com_Printf("Too much data for sequence %s", sc->name);
						break;
					}
					token = Com_EParse(text, errhead, name);
					if (!*text)
						return;

					if (*token == '{')
						depth++;
					else if (*token == '}')
						depth--;
					if (depth) {
						Q_strncpyz(data, token, maxLength);
						data += strlen(token) + 1;
						maxLength -= (strlen(token) + 1);
					}
				}
				break;
			}

		if (i == SEQ_NUMCMDS) {
			Com_Printf("CL_ParseSequence: unknown command \"%s\" ignored (sequence %s)\n", token, name);
			Com_EParse(text, errhead, name);
		}
	} while (*text);
}
