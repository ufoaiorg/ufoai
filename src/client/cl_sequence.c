// cl_sequence.c -- render non-interactive sequences

#include "client.h"

typedef struct seqCmd_s
{
	void	(*handler)( char *name, char *data );
	char	*name;
	char	*data;
} seqCmd_t;

typedef struct sequence_s
{
	char	name[MAX_VAR];
	int		start;
	int		length;
} sequence_t;

typedef enum
{
	SEQ_END,
	SEQ_WAIT,
	SEQ_PRECACHE,
	SEQ_CAMERA,
	SEQ_MODEL,
	SEQ_2DOBJ,
	SEQ_REM,
	SEQ_CMD,

	SEQ_NUMCMDS
} seqCmdEnum_t;

char *seqCmdName[SEQ_NUMCMDS] = 
{
	"end",
	"wait",
	"precache",
	"camera",
	"model",
	"2dobj",
	"rem",
	"cmd"
};

typedef struct seqCamera_s
{
	vec3_t		origin, speed;
	vec3_t		angles, omega;
	float		dist, ddist;
	float		zoom, dzoom;
} seqCamera_t;

typedef struct seqEnt_s
{
	qboolean	inuse;
	char		*name;
	struct model_s *model;
	int			skin;
	vec3_t		origin, speed;
	vec3_t		angles, omega;
	float		alpha;
	char		*parent;
	char		*tag;
	animState_t	as;
	entity_t	*ep;
} seqEnt_t;

typedef struct seq2D_s
{
	qboolean	inuse;
	char		*name;
	char		*text;
	char		*image;
	vec2_t		pos, speed;
	vec2_t		size, enlarge;
	vec4_t		color, fade;
	byte		align;
} seq2D_t;

void SEQ_Wait( char *name, char *data );
void SEQ_Precache( char *name, char *data );
void SEQ_Set( char *name, char *data );
void SEQ_Camera( char *name, char *data );
void SEQ_Model( char *name, char *data );
void SEQ_2Dobj( char *name, char *data );
void SEQ_Remove( char *name, char *data );
void SEQ_Command( char *name, char *data );

void (*seqCmdFunc[SEQ_NUMCMDS])( char *name, char *data ) = 
{
	NULL,
	SEQ_Wait,
	SEQ_Precache,
	SEQ_Camera,
	SEQ_Model,
	SEQ_2Dobj,
	SEQ_Remove,
	SEQ_Command
};

#define CMDDATA_SIZE	0x10000
#define MAX_SEQCMDS		8192
#define MAX_SEQUENCES	32
#define MAX_SEQENTS		128
#define MAX_SEQ2DS		128

char		*cmdData, *cmdDataCur;
int			cmdDataSize = 0;

seqCmd_t	seqCmds[MAX_SEQCMDS];
int			numSeqCmds;

sequence_t	sequences[MAX_SEQUENCES];
int			numSequences;

int			seqTime;
int			seqCmd, seqEndCmd;

seqCamera_t	seqCamera;

seqEnt_t	seqEnts[MAX_SEQENTS];
int			numSeqEnts;

seq2D_t		seq2Ds[MAX_SEQ2DS];
int			numSeq2Ds;

cvar_t		*seq_animspeed;


/*
======================
CL_SequenceCamera
======================
*/
void CL_SequenceCamera (void)
{
	if ( !scr_vrect.width || !scr_vrect.height )
		return;

	// advance time
	VectorMA( seqCamera.origin, cls.frametime, seqCamera.speed, seqCamera.origin );
	VectorMA( seqCamera.angles, cls.frametime, seqCamera.omega, seqCamera.angles );
	seqCamera.zoom += cls.frametime * seqCamera.dzoom;
	seqCamera.dist += cls.frametime * seqCamera.ddist;

	// set camera
	VectorCopy( seqCamera.origin, cl.cam.reforg );
	VectorCopy( seqCamera.angles, cl.cam.angles );

	AngleVectors( cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2] );
	VectorMA( cl.cam.reforg, -seqCamera.dist, cl.cam.axis[0], cl.cam.camorg );
	cl.cam.zoom = seqCamera.zoom;
}


/*
======================
CL_SequenceFindEnt
======================
*/
seqEnt_t *CL_SequenceFindEnt( char *name )
{
	seqEnt_t	*se;
	int		i;

	for ( i = 0, se = seqEnts; i < numSeqEnts; i++, se++ )
		if ( se->inuse && !strcmp( se->name, name ) )
			break;
	if ( i < numSeqEnts ) return se;
	else return NULL;
}


/*
======================
CL_SequenceFind2D
======================
*/
seq2D_t *CL_SequenceFind2D( char *name )
{
	seq2D_t	*s2d;
	int		i;

	for ( i = 0, s2d = seq2Ds; i < numSeq2Ds; i++, s2d++ )
		if ( s2d->inuse && !strcmp( s2d->name, name ) )
			break;
	if ( i < numSeq2Ds ) return s2d;
	else return NULL;
}


/*
======================
CL_SequenceRender
======================
*/
void CL_SequenceRender( void )
{
	entity_t	ent;
	seqCmd_t	*sc;
	seqEnt_t	*se;
	int		i;

	// run script
	while ( seqTime <= cl.time )
	{
		// test for finish
		if ( seqCmd >= seqEndCmd )
		{
			CL_SequenceEnd_f();
			return;
		}

		// call handler
		sc = &seqCmds[seqCmd];
		sc->handler( sc->name, sc->data );

		seqCmd++;
	}

	// set camera
	CL_SequenceCamera();

	// render sequence
	for ( i = 0, se = seqEnts; i < numSeqEnts; i++, se++ )
		if ( se->inuse )
		{
			// advance in time
			VectorMA( se->origin, cls.frametime, se->speed, se->origin );
			VectorMA( se->angles, cls.frametime, se->omega, se->angles );
			re.AnimRun( &se->as, se->model, seq_animspeed->value * cls.frametime );

			// add to scene
			memset( &ent, 0, sizeof(ent) );
			ent.model = se->model;
			ent.skinnum = se->skin;
			ent.as = se->as;
			ent.alpha = se->alpha;
			ent.sunfrac = 1.0;
			
			VectorCopy( se->origin, ent.origin );
			VectorCopy( se->origin, ent.oldorigin );
			VectorCopy( se->angles, ent.angles );

			if ( se->parent && se->tag )
			{
				seqEnt_t *parent;
				parent = CL_SequenceFindEnt( se->parent );
				if ( parent ) ent.tagent = parent->ep;
				ent.tagname = se->tag;
			}

			// add to render list
			se->ep = V_GetEntity();
			V_AddEntity( &ent );
		}

}


/*
======================
CL_Sequence2D
======================
*/
void CL_Sequence2D( void )
{
	seq2D_t	*s2d;
	int		i, j;

	// add texts
	for ( i = 0, s2d = seq2Ds; i < numSeq2Ds; i++, s2d++ )
		if ( s2d->inuse )
		{
			// advance in time
			for ( j = 0; j < 4; j++ )
			{
				s2d->color[j] += cls.frametime * s2d->fade[j];
				if ( s2d->color[j] < 0.0 ) s2d->color[j] = 0.0;
				else if ( s2d->color[j] > 1.0 ) s2d->color[j] = 1.0;
			}
			for ( j = 0; j < 2; j++ )
			{
				s2d->pos[j] += cls.frametime * s2d->speed[j];
				s2d->size[j] += cls.frametime * s2d->enlarge[j];
			}

			// render
			re.DrawColor( s2d->color );
			if ( s2d->text )
				re.DrawPropString( "f_big", s2d->align, s2d->pos[0], s2d->pos[1], s2d->text );
			if ( s2d->image )
				re.DrawNormPic( s2d->pos[0], s2d->pos[1], s2d->size[0], s2d->size[1], 
					0, 0, 0, 0, s2d->align, true, s2d->image);
		}
	re.DrawColor( NULL );
}

/*
======================
CL_SequenceStart_f
======================
*/
void CL_SequenceStart_f( void )
{
	sequence_t	*sp;
	char		*name;
	int		i;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: seq_start <name>\n" );
		return;
	}
	name = Cmd_Argv( 1 );

	// find sequence
	for ( i = 0, sp = sequences; i < numSequences; i++, sp++ )
		if ( !strcmp( name, sp->name ) )
			break;
	if ( i >= numSequences )
	{
		Com_Printf( "Couldn't find sequence '%s'\n", name );
		return;
	}

	// init script parsing
	numSeqEnts = 0;
	numSeq2Ds = 0;
	memset( &seqCamera, 0, sizeof(seqCamera_t) );
	seqTime = cl.time;
	seqCmd = sp->start;
	seqEndCmd = sp->start + sp->length;

	// init sequence state
	Cbuf_AddText( "stopsound\n" );
	MN_PushMenu( mn_sequence->string );
	cls.state = ca_sequence;
	cl.refresh_prepped = true;

	// init sun
	VectorSet( map_sun.dir, 2, 2, 3 );
	VectorSet( map_sun.ambient, 0.2, 0.2, 0.2 );
	map_sun.ambient[3] = 1.0;
	VectorSet( map_sun.color, 1.2, 1.2, 1.2 );
	map_sun.color[3] = 1.0;
}

/*
======================
CL_SequenceEnd_f
======================
*/
void CL_SequenceEnd_f( void )
{
	MN_PopMenu( false );
	cls.state = ca_disconnected;
}


/*
=================
CL_ResetSequences
=================
*/
void CL_ResetSequences( void )
{
	// reset counters
	seq_animspeed = Cvar_Get( "seq_animspeed", "1000", 0 );
	numSequences = 0;
	numSeqCmds = 0;
	numSeqEnts = 0;
	numSeq2Ds = 0;

	// get command data memory
	if ( cmdDataSize )
		memset( cmdData, 0, cmdDataSize );
	else
	{
		Hunk_Begin( CMDDATA_SIZE );
		cmdData = Hunk_Alloc( CMDDATA_SIZE );
		cmdDataSize = Hunk_End();
	}
	cmdDataCur = cmdData;
}


// ===========================================================

#define	SEQCAMOFS(x)	(int)&(((seqCamera_t *)0)->x)

value_t seqCamera_vals[] =
{
	{ "origin",		V_VECTOR,		SEQCAMOFS( origin ) },
	{ "speed",		V_VECTOR,		SEQCAMOFS( speed ) },
	{ "angles",		V_VECTOR,		SEQCAMOFS( angles ) },
	{ "omega",		V_VECTOR,		SEQCAMOFS( omega ) },
	{ "dist",		V_FLOAT,		SEQCAMOFS( dist ) },
	{ "ddist",		V_FLOAT,		SEQCAMOFS( dist ) },
	{ "zoom",		V_FLOAT,		SEQCAMOFS( zoom ) },
	{ "dzoom",		V_FLOAT,		SEQCAMOFS( dist ) },
	{ NULL, 0, 0 },
};

#define	SEQENTOFS(x)	(int)&(((seqEnt_t *)0)->x)

value_t seqEnt_vals[] =
{
	{ "name",		V_POINTER,		SEQENTOFS( name ) },
	{ "skin",		V_INT,			SEQENTOFS( skin ) },
	{ "alpha",		V_FLOAT,		SEQENTOFS( alpha ) },
	{ "origin",		V_VECTOR,		SEQENTOFS( origin ) },
	{ "speed",		V_VECTOR,		SEQENTOFS( speed ) },
	{ "angles",		V_VECTOR,		SEQENTOFS( angles ) },
	{ "omega",		V_VECTOR,		SEQENTOFS( omega ) },
	{ "parent",		V_POINTER,		SEQENTOFS( parent ) },
	{ "tag",		V_POINTER,		SEQENTOFS( tag ) },
	{ NULL, 0, 0 },
};

#define	SEQ2DOFS(x)	(int)&(((seq2D_t *)0)->x)

value_t seq2D_vals[] =
{
	{ "name",		V_POINTER,		SEQ2DOFS( name ) },
	{ "text",		V_POINTER,		SEQ2DOFS( text ) },
	{ "image",		V_POINTER,		SEQ2DOFS( image ) },
	{ "pos",		V_POS,			SEQ2DOFS( pos ) },
	{ "speed",		V_POS,			SEQ2DOFS( speed ) },
	{ "size",		V_POS,			SEQ2DOFS( size ) },
	{ "enlarge",	V_POS,			SEQ2DOFS( enlarge ) },
	{ "color",		V_COLOR,		SEQ2DOFS( color ) },
	{ "fade",		V_COLOR,		SEQ2DOFS( fade ) },
	{ "align",		V_ALIGN,		SEQ2DOFS( align ) },
	{ NULL, 0, 0 },
};

/*
======================
SEQ_Wait
======================
*/
void SEQ_Wait( char *name, char *data )
{
	seqTime += 1000 * atof( name );
}

/*
======================
SEQ_Precache
======================
*/
void SEQ_Precache( char *name, char *data )
{
	if ( !strcmp( name, "models" ) )
	{
		while ( *data )
		{
			re.RegisterModel( data );
			data += strlen( data ) + 1;
		}
	} 
	else if ( !strcmp( name, "pics" ) )
	{
		while ( *data )
		{
			re.RegisterPic( data );
			data += strlen( data ) + 1;
		}
	}
	else Com_Printf( "SEQ_Precache: unknown format '%s'\n", name );
}

/*
======================
SEQ_Camera
======================
*/
void SEQ_Camera( char *name, char *data )
{
	value_t		*vp;

	// get values
	while ( *data )
	{
		for ( vp = seqCamera_vals; vp->string; vp++ )
			if ( !strcmp( data, vp->string ) )
			{
				data += strlen( data ) + 1;
				Com_ParseValue( &seqCamera, data, vp->type, vp->ofs );
				break;
			}
		if ( !vp->string )
			Com_Printf( "SEQ_Camera: unknown token '%s'\n", data );

		data += strlen( data ) + 1;
	}
}

/*
======================
SEQ_Model
======================
*/
void SEQ_Model( char *name, char *data )
{
	seqEnt_t	*se;
	value_t		*vp;
	int		i;

	// get sequence entity
	se = CL_SequenceFindEnt( name );
	if ( !se )
	{
		// create new sequence entity
		for ( i = 0, se = seqEnts; i < numSeqEnts; i++, se++ )
			if ( !se->inuse )
				break;
		if ( i >= numSeqEnts )
		{
			if ( numSeqEnts >= MAX_SEQENTS )
				Sys_Error( ERR_FATAL, "Too many sequence entities\n" );
			se = &seqEnts[numSeqEnts++];
		}
		// allocate
		memset( se, 0, sizeof(seqEnt_t) );
		se->inuse = true;
		se->name = name;
	}
	
	// get values
	while ( *data )
	{
		for ( vp = seqEnt_vals; vp->string; vp++ )
			if ( !strcmp( data, vp->string ) )
			{
				data += strlen( data ) + 1;
				Com_ParseValue( se, data, vp->type, vp->ofs );
				break;
			}
		if ( !vp->string )
		{
			if ( !strcmp( data, "model" ) )
			{
				data += strlen( data ) + 1;
				se->model = re.RegisterModel( data );
			} else if ( !strcmp( data, "anim" ) )
			{
				data += strlen( data ) + 1;
				re.AnimChange( &se->as, se->model, data );
			}
			else Com_Printf( "SEQ_Model: unknown token '%s'\n", data );
		}

		data += strlen( data ) + 1;
	}
}

/*
======================
SEQ_2Dobj
======================
*/
void SEQ_2Dobj( char *name, char *data )
{
	seq2D_t		*s2d;
	value_t		*vp;
	int		i;

	// get sequence text
	s2d = CL_SequenceFind2D( name );
	if ( !s2d )
	{
		// create new sequence text
		for ( i = 0, s2d = seq2Ds; i < numSeq2Ds; i++, s2d++ )
			if ( !s2d->inuse )
				break;
		if ( i >= numSeq2Ds )
		{
			if ( numSeq2Ds >= MAX_SEQ2DS )
				Sys_Error( ERR_FATAL, "Too many sequence 2d objects\n" );
			s2d = &seq2Ds[numSeq2Ds++];
		}
		// allocate
		memset( s2d, 0, sizeof(seq2D_t) );
		for ( i = 0; i < 4; i++ ) s2d->color[i] = 1.0f;
		s2d->inuse = true;
		s2d->name = name;
	}
	
	// get values
	while ( *data )
	{
		for ( vp = seq2D_vals; vp->string; vp++ )
			if ( !strcmp( data, vp->string ) )
			{
				data += strlen( data ) + 1;
				Com_ParseValue( s2d, data, vp->type, vp->ofs );
				break;
			}
		if ( !vp->string )
			Com_Printf( "SEQ_Text: unknown token '%s'\n", data );

		data += strlen( data ) + 1;
	}
}

/*
======================
SEQ_Remove
======================
*/
void SEQ_Remove( char *name, char *data )
{
	seqEnt_t	*se;
	seq2D_t		*s2d;

	se = CL_SequenceFindEnt( name );
	if ( se ) se->inuse = false;

	s2d = CL_SequenceFind2D( name );
	if ( s2d ) s2d->inuse = false;

	if ( !se && !s2d ) 
		Com_Printf( "SEQ_Remove: couldn't find '%s'\n", name );
}

/*
======================
SEQ_Command
======================
*/
void SEQ_Command( char *name, char *data )
{
	// add the command
	Cbuf_AddText( name );
}


// ===========================================================

/*
======================
CL_ParseSequence
======================
*/
void CL_ParseSequence( char *name, char **text )
{
	char		*errhead = "CL_ParseSequence: unexptected end of file (sequence ";
	sequence_t	*sp;
	seqCmd_t	*sc;
	char		*token;
	int			i, depth;

	// search for sequences with same name
	for ( i = 0; i < numSequences; i++ )
		if ( !strcmp( name, sequences[i].name ) )
			break;

	if ( i < numSequences )
	{
		Com_Printf( "CL_ParseSequence: sequence def \"%s\" with same name found, second ignored\n", name );
		return;
	}

	// initialize the sequence
	if ( numSequences >= MAX_SEQUENCES ) 
		Sys_Error( ERR_FATAL, "Too many sequences\n" );

	sp = &sequences[numSequences++];
	memset( sp, 0, sizeof(sequence_t) );
	strncpy( sp->name, name, MAX_VAR );
	sp->start = numSeqCmds;

	// get it's body
	token = COM_Parse( text );

	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( "CL_ParseSequence: sequence def \"%s\" without body ignored\n", name );
		numSequences--;
		return;
	}

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text ) break;
next_cmd:
		if ( *token == '}' ) break;

		// check for commands
		for ( i = 0; i < SEQ_NUMCMDS; i++ )
			if ( !strcmp( token, seqCmdName[i] ) )
			{
				// found a command
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;

				if ( numSeqCmds >= MAX_SEQCMDS ) 
					Sys_Error( ERR_FATAL, "Too many sequence commands\n" );

				// init seqCmd
				sc = &seqCmds[numSeqCmds++];
				memset( sc, 0, sizeof(seqCmd_t) );
				sc->handler = seqCmdFunc[i];
				sc->name = cmdDataCur;
				sp->length++;

				// copy name
				strcpy( cmdDataCur, token );
				cmdDataCur += strlen( token ) + 1;
				sc->data = cmdDataCur;

				// read data
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;
				if ( *token != '{' )
				{
					sc->data = NULL;
					goto next_cmd;
				}
				sc->data = cmdDataCur;
				depth = 1;
				while ( depth )
				{
					token = COM_EParse( text, errhead, name );
					if ( !*text ) return;

					if ( *token == '{' ) depth++;
					else if ( *token == '}' ) depth--;
					if ( depth ) 
					{
						strcpy( cmdDataCur, token );
						cmdDataCur += strlen( token ) + 1;
					}
				}
				*cmdDataCur++ = 0;
				break;
			}

		if ( i == SEQ_NUMCMDS )
		{
			Com_Printf( "CL_ParseSequence: unknown command \"%s\" ignored (sequence %s)\n", token, name );
			COM_EParse( text, errhead, name );
		}
	} while ( *text );
}
