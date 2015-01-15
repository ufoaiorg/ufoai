/**
 * @file
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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "../../../../renderer/r_mesh_anim.h"
#include "e_event_cameraappear.h"

/**
 * @brief Adds a camera edicts to the client for displaying them
 * @sa EV_CAMERA_APPEAR
 */
void CL_CameraAppear (const eventRegister_t* self, dbuffer* msg)
{
	int entnum;
	int team;
	int levelflags;
	int dir;
	int rotate;
	vec3_t origin;
	camera_type_t cameraType;

	NET_ReadFormat(msg, self->formatString, &entnum, &origin, &team, &dir, &cameraType, &levelflags, &rotate);

	le_t* le = LE_Get(entnum);
	if (!le) {
		le = LE_Add(entnum);
	} else {
		le->inuse = true;
	}

	VectorCopy(origin, le->origin);
	le->type = ET_CAMERA;
	le->team = team;
	le->angles[YAW] = directionAngles[le->angle];
	le->flags |= LE_CHECK_LEVELFLAGS;
	le->levelflags = levelflags;
	le->model1 = R_FindModel(va("objects/cameras/camera%i", cameraType));
	if (rotate) {
		const char* rotateAnim = "rotate";
		R_AnimChange(&le->as, le->model1, rotateAnim);
	}

	Com_DPrintf(DEBUG_CLIENT, "CL_CameraAppear: entnum: %i\n", entnum);
}
