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

unsigned int LoadProgram_ARB_FP(char *filename)
{
	char			*fbuf, *buf, path[128];
	unsigned int	size;

	const unsigned char *errors;
	int error_pos;
	unsigned int	fpid;

	sprintf(path,"glprogs//%s.fp",filename);
	size = ri.FS_LoadFile (path, (void **)&fbuf);

	if (!fbuf)
	{
		ri.Con_Printf (PRINT_ALL, "Could not load shader %s\n", filename);
		return -1;
	}

	if (size < 16)
	{
		ri.Con_Printf (PRINT_ALL, "Could not load shader %s\n", filename);
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

unsigned int LoadProgram_ARB_VP(char *vp)
{
	char			*fbuf, *buf, path[128];
	unsigned int	size;

	unsigned int	vpid;

	return -1;

	sprintf(path,"glprogs//%s",vp);
	size = ri.FS_LoadFile (path, (void **)&fbuf);

	if (!fbuf)
	{
		ri.Con_Printf (PRINT_ALL, "Could not load shader %s\n", vp);
		return -1;
	}

	if (size < 16)
	{
		ri.Con_Printf (PRINT_ALL, "Could not load shader %s\n", vp);
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

			ri.Con_Printf(PRINT_ALL,"!! VP error at position %d in %s\n", error_pos, vp);
			ri.Con_Printf(PRINT_ALL,"!! Error: %s\n", (char *)errors);

			qglDeleteProgramsARB(1, &vpid);
			free(buf);
			return 0;
		}
	}

	free(buf);
	return vpid;
}
/*
//================================
Shaders
//================================
*/

//=================
// ABR Water Shader
//=================

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


unsigned int  CompileWaterShader(int arb_water_id)
{
	qglGenProgramsARB(1, &arb_water_id);
	qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, arb_water_id);
	qglProgramStringARB ( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen ( arb_water ), arb_water );
	return arb_water_id;
//	qglDeleteProgramsARB(1, &arb_water_id);
}

