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

// cl_ufopedia.h

#ifndef UFOPEDIA_DEFINED
#define UFOPEDIA_DEFINED 1
#include "client.h"

#define MAX_PEDIACHAPTERS	16

typedef struct pediaChapter_s
{
	char	id[MAX_VAR];
	char	name[MAX_VAR];
	int first;
	int last;
} pediaChapter_t;

// use this for saving and allocating
extern pediaChapter_t	upChapters[MAX_PEDIACHAPTERS];
extern	int	numChapters;

void UP_ResetUfopedia( void );
void UP_ParseUpChapters( char *title, char **text );
void UP_OpenWith ( char *name );

#endif

