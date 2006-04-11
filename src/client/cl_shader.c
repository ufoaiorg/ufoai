/*

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

#define	SHADERFS(x)	(int)&(((shader_t *)0)->x)

int			r_numshaders;

// shader_t is in client/ref.h
// r_shaders is external and is linked to cl.refdef.shaders in cl_view.c V_RenderView
shader_t	r_shaders[MAX_SHADERS];

value_t shader_values[] =
{
	{ "filename",	V_STRING,	SHADERFS( filename ) },
	{ "frag",	V_BOOL,	SHADERFS( frag ) },
	{ "vertex",	V_BOOL,	SHADERFS( vertex ) },
	{ NULL,	0, 0 }
};

/*
======================
CL_ParseShaders

Called from CL_ParseScriptSecond
======================
*/
void CL_ParseShaders( char *title, char **text )
{
	shader_t *entry;
	value_t *v;
	char    *errhead = _("CL_ParseShaders: unexptected end of file (names ");
	char    *token;

	// get name list body body
	token = COM_Parse( text );
	if ( !*text || *token != '{' ) {
		Com_Printf( _("CL_ParseShaders: shader \"%s\" without body ignored\n"), title );
		return;
	}

	// new entry
	entry = &r_shaders[r_numshaders++];
	memset( entry, 0, sizeof( shader_t ) );
	Q_strncpyz( entry->title, title, MAX_VAR );
	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		// get values
		for ( v = shader_values; v->string; v++ )
			if ( !Q_strcmp( token, v->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, title );
				if ( !*text ) return;

				if ( v->ofs && v->type != V_NULL )
					Com_ParseValue( entry, token, v->type, v->ofs );
				break;
			}

		if ( !v->string )
			Com_Printf( _("CL_ParseShaders: unknown token \"%s\" ignored (entry %s)\n"), token, title );

	} while ( *text );
}

void CL_ShaderList_f ( void )
{
	int i;

	for ( i = 0; i < r_numshaders; i++ )
	{
		Com_Printf( _("Shader %s\n"), r_shaders[i].title );
		Com_Printf( _("..filename: %s\n"), r_shaders[i].filename );
		Com_Printf( _("..frag %i\n"), (int)r_shaders[i].frag );
		Com_Printf( _("..vertex %i\n"), (int)r_shaders[i].vertex );
		Com_Printf( "\n" );
	}
}
