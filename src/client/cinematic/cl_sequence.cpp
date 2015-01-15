/**
 * @file
 * @brief Non-interactive sequence rendering and AVI recording.
 * @note Sequences are rendered inside the UI code with the sequence node.
 * The default window sequence is used as facility with seq_start and seq_stop
 * commands
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "cl_sequence.h"
#include "../client.h"
#include "../battlescape/cl_localentity.h"
#include "../battlescape/cl_view.h"
#include "../renderer/r_main.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_misc.h" /* R_PushMatrix, R_PopMatrix */
#include "../renderer/r_mesh_anim.h"
#include "../cl_language.h"
#include "../../shared/parse.h"
#include "../ui/ui_render.h"

#define MAX_DATA_LENGTH 512

struct sequenceContext_s;

/**
 * @brief execution function of a command
 * @return @c 0 if the same command should be executed again - or @c 1 to execute the next event
 */
typedef int (*sequenceHandler_t) (struct sequenceContext_s* context, const char* name, const char* data);

typedef struct seqCmd_s {
	sequenceHandler_t handler;
	char name[MAX_VAR];
	char data[MAX_DATA_LENGTH];
} seqCmd_t;

typedef struct sequence_s {
	char name[MAX_VAR];	/**< the name of the sequence (script id) */
	int start;		/**< the start in the sequence command buffer for this sequence */
	int length;		/**< the amount of sequence commands */
} sequence_t;

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
	bool inuse;
	char name[MAX_VAR];
	model_t* model;		/**< the loaded model structure */
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
	entity_t* ep;
} seqEnt_t;

/**
 * @brief Represents a text object or image object
 */
typedef struct seq2D_s {
	bool inuse;			/**< still in use in this sequence? or already deactivated? */
	char name[MAX_VAR];		/**< the script id for this object */
	char* text;				/**< a placeholder for gettext (V_TRANSLATION_STRING) */
	char font[MAX_VAR];		/**< the font to use in case this is a text object */
	char image[MAX_VAR];	/**< the image to render */
	vec2_t pos;				/**< the position of the 2d obj */
	vec2_t speed;			/**< how fast the 2d obj will change (fade, scale, ...) */
	vec2_t size;			/**< the size of the 2d obj */
	vec2_t enlarge;			/**< enlarge in x and y direction */
	vec4_t color;			/**< 2d obj color */
	vec4_t fade;			/**< the fade color */
	vec4_t bgcolor;			/**< background color of the box define by @c pos, @c size */
	align_t align;			/**< the alignment of the 2d obj */
	bool inBackground;	/**< If true, display the object under the 3D objects */
	bool relativePos;	/**< useful for translations when sentence length may differ */
} seq2D_t;

#define MAX_SEQCMDS		768
#define MAX_SEQUENCES	32
#define MAX_SEQENTS		128
#define MAX_SEQ2DS		128

/** Global content */
/** @todo move it to cls? */

/** Store main sequence entries */
static sequence_t sequences[MAX_SEQUENCES];
static int numSequences;

/** Store all sequence commands */
static seqCmd_t* seqCmds;
static int numSeqCmds;

/** Sequence context
 */
typedef struct sequenceContext_s {
	/** position of the viewport */
	vec2_t pos;
	/** size of the viewport */
	vec2_t size;

	/** Milliseconds the sequence is already running */
	int time;
	/** If the menu node the sequence is rendered in fetches a click this is true */
	bool endClickLoop;
	/** Current position in the sequence command array of the current running sequence */
	int currentCmd;
	/** The number of all sequence commands in the current running sequence */
	int endCmd;

	/** Speed of the animation. Default is 1000. Number per seconds */
	int animspeed;

	/** Position of the camera */
	seqCamera_t camera;

	/** Store sequence entities */
	seqEnt_t ents[MAX_SEQENTS];
	int numEnts;

	/** Store sequence 2Dobj */
	seq2D_t obj2Ds[MAX_SEQ2DS];
	int numObj2Ds;

} sequenceContext_t;

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
	{nullptr, V_NULL, 0, 0}
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
	{nullptr, V_NULL, 0, 0}
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
	{"inbackground", V_BOOL, offsetof(seq2D_t, inBackground), MEMBER_SIZEOF(seq2D_t, inBackground)},
	{"relative", V_BOOL, offsetof(seq2D_t, relativePos), MEMBER_SIZEOF(seq2D_t, relativePos)},
	{nullptr, V_NULL, 0, 0}
};

/**
 * @brief Set the camera values for a sequence
 * @sa CL_SequenceRender3D
 */
static void SEQ_SetCamera (sequenceContext_t* context)
{
	if (!context->size[0] || !context->size[1])
		return;

	/* advance time */
	VectorMA(context->camera.origin, cls.frametime, context->camera.speed, context->camera.origin);
	VectorMA(context->camera.angles, cls.frametime, context->camera.omega, context->camera.angles);
	context->camera.zoom += cls.frametime * context->camera.dzoom;
	context->camera.dist += cls.frametime * context->camera.ddist;

	/* set camera */
	VectorCopy(context->camera.origin, cl.cam.origin);
	VectorCopy(context->camera.angles, cl.cam.angles);

	AngleVectors(cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);
	VectorMA(cl.cam.origin, -context->camera.dist, cl.cam.axis[0], cl.cam.camorg);
	cl.cam.zoom = std::max(context->camera.zoom, MIN_ZOOM);
	/* fudge to get isometric and perspective modes looking similar */
	if (cl_isometric->integer)
		cl.cam.zoom /= 1.35;
	CL_ViewCalcFieldOfViewX();
}


/**
 * @brief Finds a given entity in all sequence entities
 * @sa CL_SequenceFind2D
 */
static seqEnt_t* SEQ_FindEnt (sequenceContext_t* context, const char* name)
{
	seqEnt_t* se;
	int i;

	for (i = 0, se = context->ents; i < context->numEnts; i++, se++)
		if (se->inuse && Q_streq(se->name, name))
			break;
	if (i < context->numEnts)
		return se;
	return nullptr;
}


/**
 * @brief Finds a given 2d object in the current sequence data
 * @sa CL_SequenceFindEnt
 */
static seq2D_t* SEQ_Find2D (sequenceContext_t* context, const char* name)
{
	seq2D_t* s2d;
	int i;

	for (i = 0, s2d = context->obj2Ds; i < context->numObj2Ds; i++, s2d++)
		if (s2d->inuse && Q_streq(s2d->name, name))
			break;
	if (i < context->numObj2Ds)
		return s2d;
	return nullptr;
}

/**
 * @sa CL_Sequence2D
 * @sa CL_ViewRender
 * @sa CL_SequenceEnd_f
 * @sa UI_PopWindow
 * @sa CL_SequenceFindEnt
 */
static void SEQ_Render3D (sequenceContext_t* context)
{
	seqEnt_t* se;
	int i;

	if (context->numEnts == 0)
		return;

	/* set camera */
	SEQ_SetCamera(context);

	refdef.numEntities = 0;
	refdef.mapTiles = cl.mapTiles;

	/* render sequence */
	for (i = 0, se = context->ents; i < context->numEnts; i++, se++) {
		if (!se->inuse)
			continue;

		/* advance in time */
		VectorMA(se->origin, cls.frametime, se->speed, se->origin);
		VectorMA(se->angles, cls.frametime, se->omega, se->angles);
		R_AnimRun(&se->as, se->model, context->animspeed * cls.frametime);

		/* add to scene */
		entity_t ent(RF_NONE);
		ent.model = se->model;
		ent.skinnum = se->skin;
		ent.as = se->as;
		ent.alpha = se->alpha;

		R_EntitySetOrigin(&ent, se->origin);
		VectorCopy(se->origin, ent.oldorigin);
		VectorCopy(se->angles, ent.angles);

		if (se->parent && se->tag) {
			seqEnt_t* parent;

			parent = SEQ_FindEnt(context, se->parent);
			if (parent)
				ent.tagent = parent->ep;
			ent.tagname = se->tag;
		}

		/* add to render list */
		se->ep = R_GetFreeEntity();
		R_AddEntity(&ent);
	}

	refdef.rendererFlags |= RDF_NOWORLDMODEL;

	/* use a relative fixed size */
	viddef.x = context->pos[0];
	viddef.y = context->pos[1];
	viddef.viewWidth = context->size[0];
	viddef.viewHeight = context->size[1];

	/* update refdef */
	CL_ViewUpdateRenderData();

	/** @todo Models are not at the right position (relative to the node position). Maybe R_SetupFrustum erase matrix. Not a trivialous task. */
	/* render the world */
	R_PushMatrix();
	R_RenderFrame();
	R_PopMatrix();
}


/**
 * @brief Renders text and images
 * @sa SEQ_InitStartup
 * @param[in] context Sequence context
 * @param[in] backgroundObjects if true, draw background objects, else display foreground objects
 */
static void SEQ_Render2D (sequenceContext_t* context, bool backgroundObjects)
{
	seq2D_t* s2d;
	int i, j;
	int height = 0;

	/* add texts */
	for (i = 0, s2d = context->obj2Ds; i < context->numObj2Ds; i++, s2d++) {
		if (!s2d->inuse)
			continue;
		if (backgroundObjects != s2d->inBackground)
			continue;

		if (s2d->relativePos && height > 0) {
			s2d->pos[1] += height;
			s2d->relativePos = false;
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
			const image_t* image = R_FindImage(s2d->image, it_pic);
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
			height += UI_DrawString(s2d->font, s2d->align, s2d->pos[0], s2d->pos[1], s2d->pos[0], maxWidth, -1 /** @todo use this for some nice line spacing */, CL_Translate(s2d->text));
		}
	}
	R_Color(nullptr);
}

/**
 * @brief Unlock a click event for the current sequence or ends the current sequence if not locked
 * @note Script binding for seq_click
 * @sa menu sequence in menu_main.ufo
 */
void SEQ_SendClickEvent (sequenceContext_t* context)
{
	context->endClickLoop = true;
}

/**
 * @brief Define the position of the viewport on the screen
 * @param context Context
 * @param pos Position of the context screen
 * @param size Size of the context screen
 */
void SEQ_SetView (sequenceContext_t* context, vec2_t pos, vec2_t size)
{
	context->pos[0] = pos[0];
	context->pos[1] = pos[1];
	context->size[0] = size[0];
	context->size[1] = size[1];
}

/**
 * @brief Initialize a sequence context from data of a named script sequence
 * @param context
 * @param name
 * @return True if the sequence is initialized.
 */
bool SEQ_InitContext (sequenceContext_t* context, const char* name)
{
	sequence_t* sp;
	int i;

	/* find sequence */
	for (i = 0, sp = sequences; i < numSequences; i++, sp++)
		if (Q_streq(name, sp->name))
			break;
	if (i >= numSequences) {
		Com_Printf("Couldn't find sequence '%s'\n", name);
		return false;
	}

	OBJZERO(*context);
	context->numEnts = 0;
	context->numObj2Ds = 0;
	context->time = cl.time;
	context->currentCmd = sp->start;
	context->endCmd = sp->start + sp->length;
	context->animspeed = 1000;

	return true;
}

static void SEQ_StopSequence (sequenceContext_t* context)
{
	context->endClickLoop = true;
}

/**
 * @brief Move the sequence to the right position according to the current time
 * @param context
 * @return True is the sequence is alive, false if it is the end of the sequence
 */
static bool SEQ_Execute (sequenceContext_t* context)
{
	/* we are inside a waiting command */
	if (context->time > cl.time) {
		/* if we clicked a button we end the waiting loop */
		if (context->endClickLoop) {
			context->time = cl.time;
			context->endClickLoop = false;
		}
	}

	/* run script */
	while (context->time <= cl.time) {
		/* test for finish */
		if (context->currentCmd >= context->endCmd) {
			SEQ_StopSequence(context);
			return false;
		}

		/* call handler */
		seqCmd_t* sc = &seqCmds[context->currentCmd];
		context->currentCmd += sc->handler(context, sc->name, sc->data);
	}

	return true;
}

/**
 * @brief Execute and render a sequence
 * @param context Sequence context
 * @return True if the sequence is alive.
 */
bool SEQ_Render (sequenceContext_t* context)
{
	vec3_t pos;

	if (!context->size[0] || !context->size[1])
		return true;

	if (!SEQ_Execute(context))
		return false;

	/* center screen */
	pos[0] = context->pos[0] + (context->size[0] - VID_NORM_WIDTH) / 2;
	pos[1] = context->pos[1] + (context->size[1] - VID_NORM_HEIGHT) / 2;
	pos[2] = 0;
	UI_Transform(pos, nullptr, nullptr);

	SEQ_Render2D(context, true);
	SEQ_Render3D(context);
	SEQ_Render2D(context, false);

	UI_Transform(nullptr, nullptr, nullptr);
	return true;
}

/**
 * @brief Allocate a sequence context
 * @return Context
 */
sequenceContext_t* SEQ_AllocContext (void)
{
	sequenceContext_t* context;
	context = Mem_AllocType(sequenceContext_t);
	return context;
}

/**
 * @brief Free a sequence context
 * @return Context
 */
void SEQ_FreeContext (sequenceContext_t* context)
{
	Mem_Free(context);
}

/* =========================================================== */

/**
 * @brief Wait until someone clicks with the mouse
 * @return 0 if you wait for the click
 * @return 1 if the click occurred
 */
static int SEQ_ExecuteClick (sequenceContext_t* context, const char* name, const char* data)
{
	/* if a CL_SequenceClick_f event was called */
	if (context->endClickLoop) {
		context->endClickLoop = false;
		/* increase the command counter by 1 */
		return 1;
	}
	context->time += 1000;
	/* don't increase the command counter - stay at click command */
	return 0;
}

/**
 * @brief Increase the sequence time
 * @return 1 - increase the command position of the sequence by one
 */
static int SEQ_ExecuteWait (sequenceContext_t* context, const char* name, const char* data)
{
	context->time += 1000 * atof(name);
	return 1;
}

/**
 * @brief Set the animation speed, default value is 1000
 * @return 1 - increase the command position of the sequence by one
 */
static int SEQ_ExecuteAnimSpeed (sequenceContext_t* context, const char* name, const char* data)
{
	context->animspeed = atoi(name);
	return 1;
}

/**
 * @brief Precaches the models and images for a sequence
 * @return 1 - increase the command position of the sequence by one
 * @sa R_RegisterModelShort
 * @sa R_RegisterImage
 */
static int SEQ_ExecutePrecache (sequenceContext_t* context, const char* name, const char* data)
{
	if (Q_streq(name, "models")) {
		while (*data) {
			Com_DPrintf(DEBUG_CLIENT, "Precaching model: %s\n", data);
			R_FindModel(data);
			data += strlen(data) + 1;
		}
	} else if (Q_streq(name, "pics")) {
		while (*data) {
			Com_DPrintf(DEBUG_CLIENT, "Precaching image: %s\n", data);
			R_FindPics(data);
			data += strlen(data) + 1;
		}
	} else
		Com_Printf("SEQ_ExecutePrecache: unknown format '%s'\n", name);
	return 1;
}

/**
 * @brief Parse the values for the camera like given in seqCamera
 */
static int SEQ_ExecuteCamera (sequenceContext_t* context, const char* name, const char* data)
{
	/* get values */
	while (*data) {
		const value_t* vp;
		for (vp = seqCamera_vals; vp->string; vp++)
			if (Q_streq(data, vp->string)) {
				data += strlen(data) + 1;
				Com_EParseValue(&context->camera, data, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!vp->string)
			Com_Printf("SEQ_ExecuteCamera: unknown token '%s'\n", data);

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
static int SEQ_ExecuteModel (sequenceContext_t* context, const char* name, const char* data)
{
	/* get sequence entity */
	seqEnt_t* se = SEQ_FindEnt(context, name);
	if (!se) {
		int i;
		/* create new sequence entity */
		for (i = 0, se = context->ents; i < context->numEnts; i++, se++)
			if (!se->inuse)
				break;
		if (i >= context->numEnts) {
			if (context->numEnts >= MAX_SEQENTS)
				Com_Error(ERR_FATAL, "Too many sequence entities");
			se = &context->ents[context->numEnts++];
		}
		/* allocate */
		OBJZERO(*se);
		se->inuse = true;
		Q_strncpyz(se->name, name, sizeof(se->name));
		VectorSet(se->color, 0.7, 0.7, 0.7);
	}

	/* get values */
	while (*data) {
		const value_t* vp;
		for (vp = seqEnt_vals; vp->string; vp++)
			if (Q_streq(data, vp->string)) {
				data += strlen(data) + 1;
				Com_EParseValue(se, data, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!vp->string) {
			if (Q_streq(data, "model")) {
				data += strlen(data) + 1;
				Com_DPrintf(DEBUG_CLIENT, "Registering model: %s\n", data);
				se->model = R_FindModel(data);
				if (se->model == nullptr)
					se->inuse = false;
			} else if (Q_streq(data, "anim")) {
				if (se->model == nullptr)
					Com_Error(ERR_FATAL, "could not change the animation - no model loaded yet");
				data += strlen(data) + 1;
				Com_DPrintf(DEBUG_CLIENT, "Change anim to: %s\n", data);
				R_AnimChange(&se->as, se->model, data);
			} else
				Com_Printf("SEQ_ExecuteModel: unknown token '%s'\n", data);
		}

		data += strlen(data) + 1;
	}
	return 1;
}

/**
 * @brief Changes the music in the sequence
 * @return 1 - increase the command position of the sequence by one
 */
static int SEQ_ExecuteMusic (sequenceContext_t* context, const char* name, const char* data)
{
	Com_DPrintf(DEBUG_CLIENT, "Change music to %s\n", name);
	Cvar_Set("snd_music", "%s", name);
	return 1;
}

/**
 * @brief Plays a sound in a sequence
 * @return 1 - increase the command position of the sequence by one
 */
static int SEQ_ExecuteSound (sequenceContext_t* context, const char* name, const char* data)
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
static int SEQ_ExecuteObj2D (sequenceContext_t* context, const char* name, const char* data)
{
	const value_t* vp;

	/* get sequence text */
	seq2D_t* s2d = SEQ_Find2D(context, name);
	if (!s2d) {
		/* create new sequence text */
		int i;
		for (i = 0, s2d = context->obj2Ds; i < context->numObj2Ds; i++, s2d++)
			if (!s2d->inuse)
				break;
		if (i >= context->numObj2Ds) {
			if (context->numObj2Ds >= MAX_SEQ2DS)
				Com_Error(ERR_FATAL, "Too many sequence 2d objects");
			s2d = &context->obj2Ds[context->numObj2Ds++];
		}
		/* allocate */
		OBJZERO(*s2d);
		for (i = 0; i < 4; i++)
			s2d->color[i] = 1.0f;
		s2d->inuse = true;
		Q_strncpyz(s2d->font, "f_big", sizeof(s2d->font));	/* default font */
		Q_strncpyz(s2d->name, name, sizeof(s2d->name));
	}

	/* get values */
	while (*data) {
		for (vp = seq2D_vals; vp->string; vp++)
			if (Q_streq(data, vp->string)) {
				data += strlen(data) + 1;
				switch (vp->type) {
				case V_TRANSLATION_STRING:
				case V_HUNK_STRING:
					Mem_PoolStrDupTo(data, &Com_GetValue<char*>(s2d, vp), cl_genericPool, 0);
					break;

				default:
					Com_EParseValue(s2d, data, vp->type, vp->ofs, vp->size);
					break;
				}
				break;
			}
		if (!vp->string)
			Com_Printf("SEQ_ExecuteObj2D: unknown token '%s'\n", data);

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
static int SEQ_ExecuteDelete (sequenceContext_t* context, const char* name, const char* data)
{
	seqEnt_t* se;
	seq2D_t* s2d;

	se = SEQ_FindEnt(context, name);
	if (se)
		se->inuse = false;

	s2d = SEQ_Find2D(context, name);
	if (s2d) {
		s2d->inuse = false;

		Mem_Free(s2d->text);
		s2d->text = nullptr;
	}

	if (!se && !s2d)
		Com_Printf("SEQ_ExecuteDelete: couldn't find '%s'\n", name);
	return 1;
}

/**
 * @brief Executes a sequence command
 * @return 1 - increase the command position of the sequence by one
 * @sa Cbuf_AddText
 */
static int SEQ_ExecuteCommand (sequenceContext_t* context, const char* name, const char* data)
{
	/* add the command */
	Cbuf_AddText("%s\n", name);
	return 1;
}

/* =========================================================== */

static char const* const seqCmdName[] = {
	"end",
	"wait",
	"click",
	"precache",
	"camera",
	"model",
	"obj2d",
	"music",
	"sound",
	"rem",
	"delete",
	"animspeed",
	"cmd"
};

#define SEQ_NUMCMDS lengthof(seqCmdName)

/**
 * @brief Function to exeute all available commands
 */
static sequenceHandler_t seqCmdFunc[] = {
	nullptr,
	SEQ_ExecuteWait,
	SEQ_ExecuteClick,
	SEQ_ExecutePrecache,
	SEQ_ExecuteCamera,
	SEQ_ExecuteModel,
	SEQ_ExecuteObj2D,
	SEQ_ExecuteMusic,
	SEQ_ExecuteSound,
	SEQ_ExecuteDelete,
	SEQ_ExecuteDelete,
	SEQ_ExecuteAnimSpeed,
	SEQ_ExecuteCommand
};
CASSERT(lengthof(seqCmdFunc) == lengthof(seqCmdName));

/**
 * Find a sequence command by name
 * @return The sequence command id, else -1 if not found
 */
static int CL_FindSequenceCommand (const char* commandName)
{
	int i;
	for (i = 0; i < SEQ_NUMCMDS; i++) {
		if (Q_streq(commandName, seqCmdName[i])) {
			return i;
		}
	}
	return -1;
}

/**
 * @brief Reads the sequence values from given text-pointer
 * @sa CL_ParseClientData
 */
void CL_ParseSequence (const char* name, const char** text)
{
	const char* errhead = "CL_ParseSequence: unexpected end of file (sequence ";
	sequence_t* sp;
	const char* token;
	int i;

	/* search for sequences with same name */
	for (i = 0; i < numSequences; i++)
		if (Q_streq(name, sequences[i].name))
			break;

	if (i < numSequences) {
		Com_Printf("CL_ParseSequence: sequence def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the sequence */
	if (numSequences >= MAX_SEQUENCES)
		Com_Error(ERR_FATAL, "Too many sequences");

	sp = &sequences[numSequences++];
	OBJZERO(*sp);
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
		if (*token == '}')
			break;

		/* check for commands */
		int i = CL_FindSequenceCommand(token);
		if (i != -1) {
			int maxLength = MAX_DATA_LENGTH;
			char* data;
			seqCmd_t* sc;

			/* found a command */
			token = Com_EParse(text, errhead, name);
			if (!*text)
				return;

			if (numSeqCmds >= MAX_SEQCMDS)
				Com_Error(ERR_FATAL, "Too many sequence commands for %s", name);

			/* init seqCmd */
			if (seqCmds == nullptr)
				seqCmds = Mem_PoolAllocTypeN(seqCmd_t, MAX_SEQCMDS, cl_genericPool);
			sc = &seqCmds[numSeqCmds++];
			OBJZERO(*sc);
			sc->handler = seqCmdFunc[i];
			sp->length++;

			/* copy name */
			Q_strncpyz(sc->name, token, sizeof(sc->name));

			/* read data */
			token = Com_EParse(text, errhead, name);
			if (!*text)
				return;
			if (*token == '{') {
				// TODO depth is useless IMHO (bayo)
				int depth = 1;
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
			} else if (*token == '(') {
				linkedList_t* list;
				Com_UnParseLastToken();
				if (!Com_ParseList(text, &list)) {
					Com_Error(ERR_DROP, "CL_ParseSequence: error while reading list (sequence \"%s\")", name);
				}
				data = &sc->data[0];
				for (linkedList_t* element = list; element != nullptr; element = element->next) {
					if (maxLength <= 0) {
						Com_Printf("Too much data for sequence %s", sc->name);
						break;
					}
					const char* v = (char*)element->data;
					Q_strncpyz(data, v, maxLength);
					data += strlen(v) + 1;
					maxLength -= (strlen(v) + 1);
				}
				LIST_Delete(&list);
			} else {
				Com_UnParseLastToken();
			}
		} else {
			Com_Printf("CL_ParseSequence: unknown command \"%s\" ignored (sequence %s)\n", token, name);
			Com_EParse(text, errhead, name);
		}
	} while (*text);
}

void SEQ_Shutdown (void)
{
	OBJZERO(sequences);
	numSequences = 0;
	seqCmds = nullptr;
	numSeqCmds = 0;
}
