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

#include "gl_local.h"

shaderlist_t* shaderList;
int numShaders;

/*
============
GL_ShaderInit
============
*/
void GL_ShaderInit( void )
{
	// only create the list if supported
	if ( gl_state.arb_fragment_program == true )
		shaderList = CreateShaderList();
}

/*
============
GL_ShutdownShaders
============
*/
void GL_ShutdownShaders( void )
{
    	//otherwise the list is not initialized
	if ( gl_state.arb_fragment_program == true )
		free ( shaderList );
}

/*
============
LoadProgram_ARB_FP

Load and link files containing shaders
============
*/
unsigned int LoadProgram_ARB_FP(char *path)
{
	char			*fbuf, *buf;
	unsigned int	size;

	const unsigned char *errors;
	int error_pos;
	unsigned int	fpid;

	size = ri.FS_LoadFile (path, (void **)&fbuf);

	if (!fbuf)
	{
		ri.Con_Printf (PRINT_ALL, "Could not load shader %s\n", path);
		return -1;
	}

	if (size < 16)
	{
		ri.Con_Printf (PRINT_ALL, "Could not load shader %s\n", path);
		ri.FS_FreeFile (fbuf);
		return -1;
	}

	buf = (char *)malloc(size+1);
	memcpy (buf, fbuf, size);
	buf[size] = 0;
	ri.FS_FreeFile (fbuf);

	qglGenProgramsARB(1,&fpid);
	qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpid);
	qglProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, size, buf);

	errors=qglGetString(GL_PROGRAM_ERROR_STRING_ARB);

	qglGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
	if(error_pos != -1) {
		qglDeleteProgramsARB(1, &fpid);
		free(buf);
		return 0;
	}
	free(buf);
	return fpid;
}

unsigned int LoadProgram_ARB_VP(char *path)
{
	char			*fbuf, *buf;
	unsigned int	size, vpid;

	size = ri.FS_LoadFile (path, (void **)&fbuf);

	if (!fbuf)
	{
		ri.Con_Printf (PRINT_ALL, "Could not load shader %s\n", path);
		return -1;
	}

	if (size < 16)
	{
		ri.Con_Printf (PRINT_ALL, "Could not load shader %s\n", path);
		ri.FS_FreeFile (fbuf);
		return -1;
	}

	buf = (char *)malloc(size+1);
	memcpy (buf, fbuf, size);
	buf[size] = 0;
	ri.FS_FreeFile (fbuf);

	qglGenProgramsARB(1,&vpid);
	qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpid);
	qglProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, size, buf);

	{
		const unsigned char *errors=qglGetString(GL_PROGRAM_ERROR_STRING_ARB);
		int error_pos;

		qglGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
		if(error_pos != -1) {

			ri.Con_Printf(PRINT_DEVELOPER,"!! VP error at position %d in %s\n", error_pos, path);
			ri.Con_Printf(PRINT_DEVELOPER,"!! Error: %s\n", (char *)errors);

			qglDeleteProgramsARB(1, &vpid);
			free(buf);
			return 0;
		}
	}

	free(buf);
	return vpid;
}

/*
============
UseProgram_ARB_FP

Activate Shaders
============
*/
void UseProgram_ARB_FP(unsigned int fpid)
{
	if (fpid>0)
	{
		qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpid);
		qglEnable(GL_FRAGMENT_PROGRAM_ARB);
	} else {
		qglDisable(GL_FRAGMENT_PROGRAM_ARB);
	}
}

void UseProgram_ARB_VP(unsigned int vpid)
{
	if (vpid>0)
	{
		qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpid);
		qglEnable(GL_VERTEX_PROGRAM_ARB);
	} else {
		qglDisable(GL_VERTEX_PROGRAM_ARB);
	}
}

/*
============
CreateShader
============
*/
shader_t* CreateShader(char* name)
{
	shader_t* toReturn = NULL;

	// no shaders supported
	if ( gl_state.arb_fragment_program == false )
		return NULL;

	toReturn = (shader_t*) malloc (sizeof(shader_t));
	strcpy(toReturn->sname, name);
	toReturn->fpid=LoadProgram_ARB_FP(va("arb/%s", name));
	toReturn->vpid=LoadProgram_ARB_VP(va("arb/%s", name));
	return toReturn;
}

void UseShader(shader_t* shader)
{
	assert(shader);
	// no shaders supported
	if ( gl_state.arb_fragment_program == false )
		return;
	if ( shader->fpid > 0 )
		UseProgram_ARB_FP(shader->fpid);
	if ( shader->vpid > 0 )
		UseProgram_ARB_VP(shader->vpid);
}

/*
===========
CreateShaderlist

Loads all shaders from base/arb into shaderlist
===========
*/
shaderlist_t* CreateShaderList( void )
{
	char files[MAX_VAR];
	char **shaderName = NULL;
	char *name = NULL;
	char *path = NULL;
	int num = 0, i = 0;

	// global shader count
	numShaders = 0;

	shaderlist_t* toReturn = (shaderlist_t*) malloc (sizeof(shaderlist_t*));

	while ( ( path = ri.FS_NextPath( path ) ) )
	{
		ri.Con_Printf(PRINT_DEVELOPER, "...searching %s/arb for shaders\n", path );
		Com_sprintf( files, MAX_VAR, "%s/arb/*.vp", path );

		shaderName = ri.FS_ListFiles( files, &num, 0, 0 );

		ri.Con_Printf(PRINT_DEVELOPER, "....found %i shader(s)\n", num-1 );
		for ( i = 0; i < num-1; i++ )
		{
			if ( (name=strrchr( shaderName[i], '/' )) != NULL )
			{
				name++; // no /
				ri.Con_Printf(PRINT_DEVELOPER, "....found %s\n", name );
				toReturn->shader[numShaders] = CreateShader(name);
				numShaders++;
				if ( numShaders >= MAX_SHADERS )
				{
					ri.Con_Printf(PRINT_ALL, "Max shaders reached...\n");
					break;
				}
			}
			free (shaderName[i]);
		}
	}

	return toReturn;
}

/*
===========
UseShaderFromList

The interesting part. You can ask this function, if a particular
shader exists, if so it will be activated, else an errorstring will
be returned to console (all hopefully ;) ).
===========
*/
void UseShaderFromList(char* name, shaderlist_t* shaderlist)
{
	int i=0;
	for (i=0; i<numShaders ; i++)
	{
		if (shaderlist->shader[i]==0)
		{
			ri.Con_Printf(PRINT_ALL,"Shader %s not found\n", name);
			break;
		}

		if (strcmp(name, shaderlist->shader[i]->sname)==0)
		{
			UseShader(shaderlist->shader[i]);
			break;
		}
	}
}

/*
================================
Shaders
================================
*/

/*
=================
ABR Water Shader
=================
*/
const char *arb_water =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"PARAM c[1] = { { 2.75 } };\n"
"TEMP R0;\n"
"TEX R0.zw, fragment.texcoord[1], texture[1], 2D;\n"
"MUL R0.xy, -R0.zwzw, fragment.color.primary.x;\n"
"ADD R0.xy, R0, fragment.texcoord[0];\n"
"TEX R0, R0, texture[0], 2D;\n"
"MUL R0, R0, fragment.color.primary;\n"
"MUL result.color, R0, c[0].x;\n"
"END\n";


unsigned int  CompileWaterShader(unsigned int arb_water_id)
{
	qglGenProgramsARB(1, &arb_water_id);
	qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, arb_water_id);
	qglProgramStringARB ( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen ( arb_water ), arb_water );
	return arb_water_id;
//	qglDeleteProgramsARB(1, &arb_water_id);
}

