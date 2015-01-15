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

#include "g_local.h"
#include "g_spawn.h"
#include "g_utils.h"
#include "g_vis.h"

static bool G_CameraUse (Edict* self, Edict* activator)
{
	if (!activator || !G_IsActor(activator)) {
		return false;
	}

	self->toggleActive();

	return false;
}

static bool Destroy_Camera (Edict* self)
{
	G_SpawnParticle(self->origin, self->spawnflags, self->particle);
	G_FreeEdict(self);
	return true;
}

#define CAMERAMODEL(X, IDX) case X: ent->model = "models/objects/cameras/camera" STRINGIFY(IDX); break

void G_InitCamera (Edict* ent, camera_type_t cameraType, float angle, bool rotate)
{
	switch (cameraType) {
	CAMERAMODEL(CAMERA_MOBILE, 0);
	CAMERAMODEL(CAMERA_STATIONARY, 1);
	default:
		gi.DPrintf("unknown camera type given: %i\n", cameraType);
		G_FreeEdict(ent);
		return;
	}

	AABB modelAabb;
	if (gi.LoadModelAABB(ent->model, 0, modelAabb)) {
		ent->entBox.set(modelAabb);

		ent->camera.cameraType = cameraType;
		ent->camera.rotate = rotate;
		ent->classname = "misc_camera";
		ent->type = ET_CAMERA;
		ent->solid = SOLID_BBOX;
		ent->flags |= FL_DESTROYABLE;
		ent->material = MAT_ELECTRICAL;
		ent->fieldSize = ACTOR_SIZE_NORMAL;
		ent->destroy = Destroy_Camera;
		ent->use = G_CameraUse;
		ent->dir = AngleToDir(angle);

		/* Set the position of the entity */
		VecToPos(ent->origin, ent->pos);

		gi.LinkEdict(ent);
	} else {
		gi.DPrintf("Could not get bounding box for model '%s'\n", ent->model);
		G_FreeEdict(ent);
	}
}

Edict* G_SpawnCamera (const vec3_t origin, int team, camera_type_t cameraType)
{
	Edict* ent = G_Spawn();
	VectorCopy(origin, ent->origin);
	ent->setTeam(team);

	G_InitCamera(ent, cameraType, 0, true);

	G_CheckVis(ent);

	return ent;
}
