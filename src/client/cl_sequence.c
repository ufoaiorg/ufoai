// // cl_sequence.c -- render non-interactive sequences
// avi record functions

#include "client.h"

#define MAX_DATA_LENGTH 1024

typedef struct seqCmd_s
{
	void	(*handler)( char *name, char *data );
	char	name[MAX_VAR];
	char	data[MAX_DATA_LENGTH];
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
	char		name[MAX_VAR];
	struct model_s *model;
	int			skin;
	vec3_t		origin, speed;
	vec3_t		angles, omega;
	float		alpha;
	char		parent[MAX_VAR];
	char		tag[MAX_VAR];
	animState_t	as;
	entity_t	*ep;
} seqEnt_t;

typedef struct seq2D_s
{
	qboolean	inuse;
	char		name[MAX_VAR];
	char		text[MAX_VAR];
	char		image[MAX_VAR];
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

#define MAX_SEQCMDS		8192
#define MAX_SEQUENCES	32
#define MAX_SEQENTS		128
#define MAX_SEQ2DS		128

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
		if ( se->inuse && !Q_strncmp( se->name, name, MAX_VAR ) )
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
		if ( s2d->inuse && !Q_strncmp( s2d->name, name, MAX_VAR ) )
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
	float		sunfrac;
	int		i;

	// run script
	while ( seqTime <= cl.time )
	{
		// test for finish
		if ( seqCmd >= seqEndCmd )
		{
			CL_SequenceEnd_f();
			MN_PopMenu(false);
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

			sunfrac = 1.0;
			ent.lightparam = &sunfrac;

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
			if ( *s2d->text )
				re.DrawPropString( "f_big", s2d->align, s2d->pos[0], s2d->pos[1], _(s2d->text) );
			if ( *s2d->image )
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
		Com_Printf( _("Usage: seq_start <name>\n") );
		return;
	}
	name = Cmd_Argv( 1 );

	// find sequence
	for ( i = 0, sp = sequences; i < numSequences; i++, sp++ )
		if ( !Q_strncmp( name, sp->name, MAX_VAR ) )
			break;
	if ( i >= numSequences )
	{
		Com_Printf( _("Couldn't find sequence '%s'\n"), name );
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
	{ "name",		V_STRING,		SEQENTOFS( name ) },
	{ "skin",		V_INT,			SEQENTOFS( skin ) },
	{ "alpha",		V_FLOAT,		SEQENTOFS( alpha ) },
	{ "origin",		V_VECTOR,		SEQENTOFS( origin ) },
	{ "speed",		V_VECTOR,		SEQENTOFS( speed ) },
	{ "angles",		V_VECTOR,		SEQENTOFS( angles ) },
	{ "omega",		V_VECTOR,		SEQENTOFS( omega ) },
	{ "parent",		V_STRING,		SEQENTOFS( parent ) },
	{ "tag",		V_STRING,		SEQENTOFS( tag ) },
	{ NULL, 0, 0 },
};

#define	SEQ2DOFS(x)	(int)&(((seq2D_t *)0)->x)

value_t seq2D_vals[] =
{
	{ "name",		V_STRING,		SEQ2DOFS( name ) },
	{ "text",		V_TRANSLATION_STRING,		SEQ2DOFS( text ) },
	{ "image",		V_STRING,		SEQ2DOFS( image ) },
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
	if ( !Q_strncmp( name, "models", 6 ) )
	{
		while ( *data )
		{
			Com_DPrintf(_("Precaching model: %s\n"), data );
			re.RegisterModel( data );
			data += strlen( data ) + 1;
		}
	}
	else if ( !Q_strncmp( name, "pics", 4 ) )
	{
		while ( *data )
		{
			Com_DPrintf(_("Precaching image: %s\n"), data );
			re.RegisterPic( data );
			data += strlen( data ) + 1;
		}
	}
	else Com_Printf( _("SEQ_Precache: unknown format '%s'\n"), name );
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
			if ( !Q_strcmp( data, vp->string ) )
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
				Sys_Error( ERR_FATAL, _("Too many sequence entities\n") );
			se = &seqEnts[numSeqEnts++];
		}
		// allocate
		memset( se, 0, sizeof(seqEnt_t) );
		se->inuse = true;
		Com_sprintf( se->name, MAX_VAR, name );
	}

	// get values
	while ( *data )
	{
		for ( vp = seqEnt_vals; vp->string; vp++ )
			if ( !Q_strcmp( data, vp->string ) )
			{
				data += strlen( data ) + 1;
				Com_ParseValue( se, data, vp->type, vp->ofs );
				break;
			}
		if ( !vp->string )
		{
			if ( !Q_strncmp( data, "model", 5 ) )
			{
				data += strlen( data ) + 1;
				Com_DPrintf(_("Registering model: %s\n"), data );
				se->model = re.RegisterModel( data );
			} else if ( !Q_strncmp( data, "anim", 4 ) )
			{
				data += strlen( data ) + 1;
				Com_DPrintf(_("Change anim to: %s\n"), data );
				re.AnimChange( &se->as, se->model, data );
			}
			else Com_Printf( _("SEQ_Model: unknown token '%s'\n"), data );
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
				Sys_Error( ERR_FATAL, _("Too many sequence 2d objects\n") );
			s2d = &seq2Ds[numSeq2Ds++];
		}
		// allocate
		memset( s2d, 0, sizeof(seq2D_t) );
		for ( i = 0; i < 4; i++ ) s2d->color[i] = 1.0f;
		s2d->inuse = true;
		Com_sprintf( s2d->name, MAX_VAR, name );
	}

	// get values
	while ( *data )
	{
		for ( vp = seq2D_vals; vp->string; vp++ )
			if ( !Q_strcmp( data, vp->string ) )
			{
				data += strlen( data ) + 1;
				Com_ParseValue( s2d, data, vp->type, vp->ofs );
				break;
			}
		if ( !vp->string )
			Com_Printf( _("SEQ_Text: unknown token '%s'\n"), data );

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
		Com_Printf( _("SEQ_Remove: couldn't find '%s'\n"), name );
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
	char		*errhead = _("CL_ParseSequence: unexptected end of file (sequence ");
	sequence_t	*sp;
	seqCmd_t	*sc;
	char		*token, *data;
	int		i, depth, maxLength;

	// search for sequences with same name
	for ( i = 0; i < numSequences; i++ )
		if ( !Q_strncmp( name, sequences[i].name, MAX_VAR ) )
			break;

	if ( i < numSequences )
	{
		Com_Printf( _("CL_ParseSequence: sequence def \"%s\" with same name found, second ignored\n"), name );
		return;
	}

	// initialize the sequence
	if ( numSequences >= MAX_SEQUENCES )
		Sys_Error( ERR_FATAL, _("Too many sequences\n") );

	sp = &sequences[numSequences++];
	memset( sp, 0, sizeof(sequence_t) );
	Com_sprintf( sp->name, MAX_VAR, name );
	sp->start = numSeqCmds;

	// get it's body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( _("CL_ParseSequence: sequence def \"%s\" without body ignored\n"), name );
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
			if ( !Q_strcmp( token, seqCmdName[i] ) )
			{
				maxLength = MAX_DATA_LENGTH;
				// found a command
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;

				if ( numSeqCmds >= MAX_SEQCMDS )
					Sys_Error( ERR_FATAL, _("Too many sequence commands\n") );

				// init seqCmd
				sc = &seqCmds[numSeqCmds++];
				memset( sc, 0, sizeof(seqCmd_t) );
				sc->handler = seqCmdFunc[i];
				sp->length++;

				// copy name
				Com_sprintf( sc->name, MAX_VAR, token );

				// read data
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;
				if ( *token != '{' ) goto next_cmd;

				depth = 1;
				data = &sc->data[0];
				while ( depth )
				{
					if ( maxLength <= 0 )
					{
						Com_Printf(_("Too much data for sequence %s"), sc->name );
						break;
					}
					token = COM_EParse( text, errhead, name );
					if ( !*text ) return;

					if ( *token == '{' ) depth++;
					else if ( *token == '}' ) depth--;
					if ( depth )
					{
						Com_sprintf( data, maxLength, token );
						data += strlen(token)+1;
						maxLength -= (strlen(token)+1);
					}
				}
				break;
			}

		if ( i == SEQ_NUMCMDS )
		{
			Com_Printf( _("CL_ParseSequence: unknown command \"%s\" ignored (sequence %s)\n"), token, name );
			COM_EParse( text, errhead, name );
		}
	} while ( *text );
}

// ===================== AVI FUNCTIONS ====================================

#include "snd_loc.h"

#define INDEX_FILE_EXTENSION ".index.dat"

#define PAD(x,y) (((x)+(y)-1) & ~((y)-1))

#define MAX_RIFF_CHUNKS 16

typedef struct audioFormat_s
{
	int rate;
	int format;
	int channels;
	int bits;

	int sampleSize;
	int totalBytes;
} audioFormat_t;

typedef struct aviFileData_s
{
	qboolean	fileOpen;
	FILE*		f;
	char		fileName[ MAX_QPATH ];
	int		fileSize;
	int		moviOffset;
	int		moviSize;

	FILE*		idxF;
	int		numIndices;

	int		frameRate;
	int		framePeriod;
	int		width, height;
	int		numVideoFrames;
	int		maxRecordSize;
	qboolean	motionJpeg;
	qboolean	audio;
	audioFormat_t	a;
	int		numAudioFrames;

	int		chunkStack[ MAX_RIFF_CHUNKS ];
	int		chunkStackTop;

	byte		*cBuffer, *eBuffer;
} aviFileData_t;

static aviFileData_t afd;

#define MAX_AVI_BUFFER 2048

static byte buffer[ MAX_AVI_BUFFER ];
static int  bufIndex;
/*
===============
CL_Video_f

video
video [filename]
===============
*/
void CL_Video_f( void )
{
	char  filename[ MAX_OSPATH ];
	int   i, last;

	if( Cmd_Argc( ) == 2 )
	{
		// explicit filename
		Com_sprintf( filename, MAX_OSPATH, "videos/%s.avi", Cmd_Argv( 1 ) );
	}
	else
	{
		// scan for a free filename
		for( i = 0; i <= 9999; i++ )
		{
			int a, b, c, d;

			last = i;

			a = last / 1000;
			last -= a * 1000;
			b = last / 100;
			last -= b * 100;
			c = last / 10;
			last -= c * 10;
			d = last;

			Com_sprintf( filename, MAX_OSPATH, "videos/video%d%d%d%d.avi",
				a, b, c, d );

			if( FS_CheckFile( filename ) <= 0 )
				break; // file doesn't exist
		}

		if ( i > 9999 )
		{
			Com_Printf( _("ERROR: no free file names to create video\n") );
			return;
		}
	}
	CL_OpenAVIForWriting( filename );
}

/*
===============
CL_StopVideo_f
===============
*/
void CL_StopVideo_f( void )
{
	CL_CloseAVI( );
}

/*
===============
SafeFS_Write
===============
*/
static void SafeFS_Write( const void *buffer, int len, FILE* f )
{
	int write = FS_Write( buffer, len, f );
	if ( write < len )
		Com_Printf( _("Failed to write avi file %p - %i:%i\n"), f, write,len );
}

/*
===============
WRITE_STRING
===============
*/
static void WRITE_STRING( const char *s )
{
	memcpy( &buffer[ bufIndex ], s, strlen( s ) );
	bufIndex += strlen( s );
}

/*
===============
WRITE_4BYTES
===============
*/
static void WRITE_4BYTES( int x )
{
	buffer[ bufIndex + 0 ] = (byte)( ( x >>  0 ) & 0xFF );
	buffer[ bufIndex + 1 ] = (byte)( ( x >>  8 ) & 0xFF );
	buffer[ bufIndex + 2 ] = (byte)( ( x >> 16 ) & 0xFF );
	buffer[ bufIndex + 3 ] = (byte)( ( x >> 24 ) & 0xFF );
	bufIndex += 4;
}

/*
===============
WRITE_2BYTES
===============
*/
static void WRITE_2BYTES( int x )
{
	buffer[ bufIndex + 0 ] = (byte)( ( x >>  0 ) & 0xFF );
	buffer[ bufIndex + 1 ] = (byte)( ( x >>  8 ) & 0xFF );
	bufIndex += 2;
}

#if 0
/*
===============
WRITE_1BYTES
===============
*/
static void WRITE_1BYTES( int x )
{
	buffer[ bufIndex ] = x;
	bufIndex += 1;
}
#endif

/*
===============
START_CHUNK
===============
*/
static void START_CHUNK( const char *s )
{
	if( afd.chunkStackTop == MAX_RIFF_CHUNKS )
		Sys_Error( _("ERROR: Top of chunkstack breached\n") );

	afd.chunkStack[ afd.chunkStackTop ] = bufIndex;
	afd.chunkStackTop++;
	WRITE_STRING( s );
	WRITE_4BYTES( 0 );
}

/*
===============
END_CHUNK
===============
*/
static void END_CHUNK( void )
{
	int endIndex = bufIndex;

	if( afd.chunkStackTop <= 0 )
		Sys_Error( _("ERROR: Bottom of chunkstack breached\n") );

	afd.chunkStackTop--;
	bufIndex = afd.chunkStack[ afd.chunkStackTop ];
	bufIndex += 4;
	WRITE_4BYTES( endIndex - bufIndex - 1 );
	bufIndex = endIndex;
	bufIndex = PAD( bufIndex, 2 );
}

/*
===============
CL_WriteAVIHeader
===============
*/
void CL_WriteAVIHeader( void )
{
	bufIndex = 0;
	afd.chunkStackTop = 0;

	START_CHUNK( "RIFF" );
	{
		WRITE_STRING( "AVI " );
		{
			START_CHUNK( "LIST" );
			{
				WRITE_STRING( "hdrl" );
				WRITE_STRING( "avih" );
				WRITE_4BYTES( 56 );                     //"avih" "chunk" size
				WRITE_4BYTES( afd.framePeriod );        //dwMicroSecPerFrame
				WRITE_4BYTES( afd.maxRecordSize *
				afd.frameRate );                    //dwMaxBytesPerSec
				WRITE_4BYTES( 0 );                      //dwReserved1
				WRITE_4BYTES( 0x110 );                  //dwFlags bits HAS_INDEX and IS_INTERLEAVED
				WRITE_4BYTES( afd.numVideoFrames );     //dwTotalFrames
				WRITE_4BYTES( 0 );                      //dwInitialFrame

				if( afd.audio )                         //dwStreams
					WRITE_4BYTES( 2 );
				else
					WRITE_4BYTES( 1 );

				WRITE_4BYTES( afd.maxRecordSize );      //dwSuggestedBufferSize
				WRITE_4BYTES( afd.width );              //dwWidth
				WRITE_4BYTES( afd.height );             //dwHeight
				WRITE_4BYTES( 0 );                      //dwReserved[ 0 ]
				WRITE_4BYTES( 0 );                      //dwReserved[ 1 ]
				WRITE_4BYTES( 0 );                      //dwReserved[ 2 ]
				WRITE_4BYTES( 0 );                      //dwReserved[ 3 ]

				START_CHUNK( "LIST" );
				{
					WRITE_STRING( "strl" );
					WRITE_STRING( "strh" );
					WRITE_4BYTES( 56 );                   //"strh" "chunk" size
					WRITE_STRING( "vids" );

					if( afd.motionJpeg )
						WRITE_STRING( "MJPG" );
					else
						WRITE_STRING( " BGR" );

					WRITE_4BYTES( 0 );                    //dwFlags
					WRITE_4BYTES( 0 );                    //dwPriority
					WRITE_4BYTES( 0 );                    //dwInitialFrame

					WRITE_4BYTES( 1 );                    //dwTimescale
					WRITE_4BYTES( afd.frameRate );        //dwDataRate
					WRITE_4BYTES( 0 );                    //dwStartTime
					WRITE_4BYTES( afd.numVideoFrames );   //dwDataLength

					WRITE_4BYTES( afd.maxRecordSize );    //dwSuggestedBufferSize
					WRITE_4BYTES( -1 );                   //dwQuality
					WRITE_4BYTES( 0 );                    //dwSampleSize
					WRITE_2BYTES( 0 );                    //rcFrame
					WRITE_2BYTES( 0 );                    //rcFrame
					WRITE_2BYTES( afd.width );            //rcFrame
					WRITE_2BYTES( afd.height );           //rcFrame

					WRITE_STRING( "strf" );
					WRITE_4BYTES( 40 );                   //"strf" "chunk" size
					WRITE_4BYTES( 40 );                   //biSize
					WRITE_4BYTES( afd.width );            //biWidth
					WRITE_4BYTES( afd.height );           //biHeight
					WRITE_2BYTES( 1 );                    //biPlanes
					WRITE_2BYTES( 24 );                   //biBitCount

					if( afd.motionJpeg )                        //biCompression
						WRITE_STRING( "MJPG" );
					else
						WRITE_STRING( " BGR" );

					WRITE_4BYTES( afd.width *
					afd.height );                     //biSizeImage
					WRITE_4BYTES( 0 );                    //biXPelsPetMeter
					WRITE_4BYTES( 0 );                    //biYPelsPetMeter
					WRITE_4BYTES( 0 );                    //biClrUsed
					WRITE_4BYTES( 0 );                    //biClrImportant
				}
				END_CHUNK( );

				if( afd.audio )
				{
					START_CHUNK( "LIST" );
					{
						WRITE_STRING( "strl" );
						WRITE_STRING( "strh" );
						WRITE_4BYTES( 56 );                 //"strh" "chunk" size
						WRITE_STRING( "auds" );
						WRITE_4BYTES( 0 );                  //FCC
						WRITE_4BYTES( 0 );                  //dwFlags
						WRITE_4BYTES( 0 );                  //dwPriority
						WRITE_4BYTES( 0 );                  //dwInitialFrame

						WRITE_4BYTES( afd.a.sampleSize );   //dwTimescale
						WRITE_4BYTES( afd.a.sampleSize *
							afd.a.rate );                   //dwDataRate
						WRITE_4BYTES( 0 );                  //dwStartTime
						WRITE_4BYTES( afd.a.totalBytes /
							afd.a.sampleSize );             //dwDataLength

						WRITE_4BYTES( 0 );                  //dwSuggestedBufferSize
						WRITE_4BYTES( -1 );                 //dwQuality
						WRITE_4BYTES( afd.a.sampleSize );   //dwSampleSize
						WRITE_2BYTES( 0 );                  //rcFrame
						WRITE_2BYTES( 0 );                  //rcFrame
						WRITE_2BYTES( 0 );                  //rcFrame
						WRITE_2BYTES( 0 );                  //rcFrame

						WRITE_STRING( "strf" );
						WRITE_4BYTES( 18 );                 //"strf" "chunk" size
						WRITE_2BYTES( afd.a.format );       //wFormatTag
						WRITE_2BYTES( afd.a.channels );     //nChannels
						WRITE_4BYTES( afd.a.rate );         //nSamplesPerSec
						WRITE_4BYTES( afd.a.sampleSize *
							afd.a.rate );                   //nAvgBytesPerSec
						WRITE_2BYTES( afd.a.sampleSize );   //nBlockAlign
						WRITE_2BYTES( afd.a.bits );         //wBitsPerSample
						WRITE_2BYTES( 0 );                  //cbSize
					}
					END_CHUNK( );
				}
			}
			END_CHUNK( );

			afd.moviOffset = bufIndex;

			START_CHUNK( "LIST" );
			{
				WRITE_STRING( "movi" );
			}
		}
	}
}

/*
===============
CL_OpenAVIForWriting

Creates an AVI file and gets it into a state where
writing the actual data can begin
===============
*/
qboolean CL_OpenAVIForWriting( char *fileName )
{
	if( afd.fileOpen )
		return false;

	memset( &afd, 0, sizeof( aviFileData_t ) );

	// Don't start if a framerate has not been chosen
	if( cl_aviFrameRate->value <= 0 )
	{
		Com_Printf( _("cl_aviFrameRate must be >= 1\n") );
		return false;
	}

	FS_FOpenFileWrite( fileName, &afd.f );
	if ( afd.f == NULL )
	{
		Com_Printf( _("Could not open %s for writing\n"), fileName );
		return false;
	}

	FS_FOpenFileWrite( va( "%s" INDEX_FILE_EXTENSION, fileName ), &afd.idxF );
	if ( afd.idxF == NULL )
	{
		Com_Printf( _("Could not open index file for writing\n") );
		FS_FCloseFile( afd.f );
		return false;
	}

	Com_sprintf( afd.fileName, MAX_QPATH, fileName );

	afd.frameRate = cl_aviFrameRate->value;
	afd.framePeriod = (int)( 1000000.0f / afd.frameRate );
	afd.width = viddef.width;
	afd.height = viddef.height;

	if( cl_aviMotionJpeg->value )
		afd.motionJpeg = true;
	else
		afd.motionJpeg = false;

	afd.cBuffer = Z_Malloc( afd.width * afd.height * 4 );
	afd.eBuffer = Z_Malloc( afd.width * afd.height * 4 );

	afd.a.rate = dma.speed;
	afd.a.format = 1;
	afd.a.channels = dma.channels;
	afd.a.bits = dma.samplebits;
	afd.a.sampleSize = ( afd.a.bits / 8 ) * afd.a.channels;

	if( afd.a.rate % afd.frameRate )
	{
		int suggestRate = afd.frameRate;

		while( ( afd.a.rate % suggestRate ) && suggestRate >= 1 )
			suggestRate--;

		Com_Printf(_("WARNING: cl_aviFrameRate is not a divisor of the audio rate, suggest %d\n"), suggestRate );
	}

	if( !(int)Cvar_VariableValue( "s_initsound" ) )
	{
		afd.audio = false;
		Com_Printf(_("No audio for video capturing\n"));
	}
	else
	{
		if( afd.a.bits == 16 && afd.a.channels == 2 )
			afd.audio = true;
		else
		{
			Com_Printf(_("No audio for video capturing\n"));
			afd.audio = false; //FIXME: audio not implemented for this case
		}
	}

	// This doesn't write a real header, but allocates the
	// correct amount of space at the beginning of the file
	CL_WriteAVIHeader();

	SafeFS_Write( buffer, bufIndex, afd.f );
	afd.fileSize = bufIndex;

	bufIndex = 0;
	START_CHUNK( "idx1" );
	SafeFS_Write( buffer, bufIndex, afd.idxF );

	afd.moviSize = 4; // For the "movi"
	afd.fileOpen = true;

	return true;
}

/*
===============
CL_CheckFileSize
===============
*/
static qboolean CL_CheckFileSize( int bytesToAdd )
{
	unsigned int newFileSize;

	newFileSize =
		afd.fileSize +                // Current file size
		bytesToAdd +                  // What we want to add
		( afd.numIndices * 16 ) +     // The index
		4;                            // The index size

	// I assume all the operating systems
	// we target can handle a 2Gb file
	if( newFileSize > INT_MAX )
	{
		// Close the current file...
		CL_CloseAVI();

		// ...And open a new one
		CL_OpenAVIForWriting( va( "%s_", afd.fileName ) );

		return true;
	}

	return false;
}

/*
===============
CL_WriteAVIVideoFrame
===============
*/
void CL_WriteAVIVideoFrame( const byte *imageBuffer, int size )
{
	int   chunkOffset = afd.fileSize - afd.moviOffset - 8;
	int   chunkSize = 8 + size;
	int   paddingSize = PAD( size, 2 ) - size;
	byte  padding[ 4 ] = { 0 };

	if( !afd.fileOpen )
		return;

	// Chunk header + contents + padding
	if( CL_CheckFileSize( 8 + size + 2 ) )
		return;

	bufIndex = 0;
	WRITE_STRING( "00dc" );
	WRITE_4BYTES( size );

	SafeFS_Write( buffer, 8, afd.f );
	SafeFS_Write( imageBuffer, size, afd.f );
	SafeFS_Write( padding, paddingSize, afd.f );
	afd.fileSize += ( chunkSize + paddingSize );

	afd.numVideoFrames++;
	afd.moviSize += ( chunkSize + paddingSize );

	if( size > afd.maxRecordSize )
		afd.maxRecordSize = size;

	// Index
	bufIndex = 0;
	WRITE_STRING( "00dc" );           //dwIdentifier
	WRITE_4BYTES( 0 );                //dwFlags
	WRITE_4BYTES( chunkOffset );      //dwOffset
	WRITE_4BYTES( size );             //dwLength
	SafeFS_Write( buffer, 16, afd.idxF );

	afd.numIndices++;
}

#define PCM_BUFFER_SIZE 44100

/*
===============
CL_WriteAVIAudioFrame
===============
*/
void CL_WriteAVIAudioFrame( const byte *pcmBuffer, int size )
{
	static byte pcmCaptureBuffer[ PCM_BUFFER_SIZE ] = { 0 };
	static int  bytesInBuffer = 0;

	if( !afd.audio || !afd.fileOpen )
		return;

	// Chunk header + contents + padding
	if( CL_CheckFileSize( 8 + bytesInBuffer + size + 2 ) )
		return;

	if( bytesInBuffer + size > PCM_BUFFER_SIZE )
	{
		Com_Printf( _("WARNING: Audio capture buffer overflow -- truncating\n") );
		size = PCM_BUFFER_SIZE - bytesInBuffer;
	}

	memcpy( &pcmCaptureBuffer[ bytesInBuffer ], pcmBuffer, size );
	bytesInBuffer += size;

	// Only write if we have a frame's worth of audio
	if( bytesInBuffer >= (int)ceil( (float)afd.a.rate / (float)afd.frameRate ) *
		afd.a.sampleSize )
	{
		int   chunkOffset = afd.fileSize - afd.moviOffset - 8;
		int   chunkSize = 8 + bytesInBuffer;
		int   paddingSize = PAD( bytesInBuffer, 2 ) - bytesInBuffer;
		byte  padding[ 4 ] = { 0 };

		bufIndex = 0;
		WRITE_STRING( "01wb" );
		WRITE_4BYTES( bytesInBuffer );

		SafeFS_Write( buffer, 8, afd.f );
		SafeFS_Write( pcmBuffer, bytesInBuffer, afd.f );
		SafeFS_Write( padding, paddingSize, afd.f );
		afd.fileSize += ( chunkSize + paddingSize );

		afd.numAudioFrames++;
		afd.moviSize += ( chunkSize + paddingSize );
		afd.a.totalBytes =+ bytesInBuffer;

		// Index
		bufIndex = 0;
		WRITE_STRING( "01wb" );           //dwIdentifier
		WRITE_4BYTES( 0 );                //dwFlags
		WRITE_4BYTES( chunkOffset );      //dwOffset
		WRITE_4BYTES( bytesInBuffer );    //dwLength
		SafeFS_Write( buffer, 16, afd.idxF );

		afd.numIndices++;

		bytesInBuffer = 0;
	}
}

/*
===============
CL_TakeVideoFrame
===============
*/
void CL_TakeVideoFrame( void )
{
	// AVI file isn't open
	if( !afd.fileOpen )
		return;

	re.TakeVideoFrame( afd.width, afd.height, afd.cBuffer, afd.eBuffer, afd.motionJpeg );
}

/*
===============
CL_CloseAVI

Closes the AVI file and writes an index chunk
===============
*/
qboolean CL_CloseAVI( void )
{
	int indexRemainder;
	int indexSize = afd.numIndices * 16;
	char *idxFileName = va( "%s" INDEX_FILE_EXTENSION, afd.fileName );

	// AVI file isn't open
	if( !afd.fileOpen )
		return false;

	afd.fileOpen = false;

	FS_Seek( afd.idxF, 4, FS_SEEK_SET );
	bufIndex = 0;
	WRITE_4BYTES( indexSize );
	SafeFS_Write( buffer, bufIndex, afd.idxF );
	FS_FCloseFile( afd.idxF );

	// Write index

	// Open the temp index file
	if( ( indexSize = FS_FOpenFile( idxFileName, &afd.idxF ) ) <= 0 )
	{
		FS_FCloseFile( afd.f );
		return false;
	}

	indexRemainder = indexSize;

	// Append index to end of avi file
	while( indexRemainder > MAX_AVI_BUFFER )
	{
		FS_Read( buffer, MAX_AVI_BUFFER, afd.idxF );
		SafeFS_Write( buffer, MAX_AVI_BUFFER, afd.f );
		afd.fileSize += MAX_AVI_BUFFER;
		indexRemainder -= MAX_AVI_BUFFER;
	}
	FS_Read( buffer, indexRemainder, afd.idxF );
	SafeFS_Write( buffer, indexRemainder, afd.f );
	afd.fileSize += indexRemainder;
	FS_FCloseFile( afd.idxF );

	// Remove temp index file
// 	FS_HomeRemove( idxFileName );

	// Write the real header
	FS_Seek( afd.f, 0, FS_SEEK_SET );
	CL_WriteAVIHeader();

	bufIndex = 4;
	WRITE_4BYTES( afd.fileSize - 8 ); // "RIFF" size

	bufIndex = afd.moviOffset + 4;    // Skip "LIST"
	WRITE_4BYTES( afd.moviSize );

	SafeFS_Write( buffer, bufIndex, afd.f );

	Z_Free( afd.cBuffer );
	Z_Free( afd.eBuffer );
	FS_FCloseFile( afd.f );

	Com_Printf( _("Wrote %d:%d frames to %s\n"), afd.numVideoFrames, afd.numAudioFrames, afd.fileName );

	return true;
}

/*
===============
CL_VideoRecording
===============
*/
qboolean CL_VideoRecording( void )
{
	return afd.fileOpen;
}

