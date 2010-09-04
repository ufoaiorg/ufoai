/**
 * @file cl_sequence.h
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

#ifndef CLIENT_CL_SEQUENCE_H
#define CLIENT_CL_SEQUENCE_H

struct sequenceContext_s;

#define pSequenceContext_t struct sequenceContext_s*

pSequenceContext_t SEQ_AllocContext(void);
void SEQ_FreeContext(pSequenceContext_t context);
qboolean SEQ_InitContext(pSequenceContext_t context, const char *sequenceName);
qboolean SEQ_Render(pSequenceContext_t context);
void SEQ_ClickEvent (pSequenceContext_t context);

void CL_SequenceRender(void);
void SEQ_InitStartup(void);
void CL_ParseSequence(const char *name, const char **text);

#endif
