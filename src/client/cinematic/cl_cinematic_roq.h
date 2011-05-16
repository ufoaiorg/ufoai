/**
 * @file cl_cinematic_roq.h
 * @brief Header file for ROQ cinematics
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CLIENT_CL_CINEMATIC_ROQ_H

#include "../../shared/ufotypes.h"	/* just for qboolean */

struct cinematic_s;

int CIN_ROQ_OpenCinematic(struct cinematic_s *cin, const char *fileName);
void CIN_ROQ_CloseCinematic(struct cinematic_s *cin);
qboolean CIN_ROQ_RunCinematic(struct cinematic_s *cin);

void CIN_ROQ_Init(void);

#endif
