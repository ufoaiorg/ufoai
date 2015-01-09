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
#include "../../../cl_actor.h"
#include "../../../cl_hud.h"
#include "../../../../ui/ui_main.h"
#include "e_event_actorreactionfireaddtarget.h"

static void CL_GetReactionFireHead (const le_t* le, const char** model, int* skin, bool* robot)
{
	/* hack for robots, they don't have a head */
	if (le->teamDef->robot) {
		*model = le->model1->name;
		*skin = le->bodySkin;
	} else {
		*model = le->model2->name;
		*skin = le->headSkin;
	}
	*robot = le->teamDef->robot;
}

int CL_ActorReactionFireAddTargetTime (const eventRegister_t* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	int targetEntNum;
	int unused;
	int step;

	NET_ReadFormat(msg, self->formatString, &unused, &targetEntNum, &unused, &step);

	const le_t* target = LE_Get(targetEntNum);
	if (!target)
		LE_NotFoundError(targetEntNum);
	if (step >= MAX_ROUTE)
		return eventTiming->nextTime;
	const int stepTime = CL_GetStepTime(eventTiming, target, step);
	if (eventTiming->shootTime > stepTime)
		return eventTiming->impactTime;
	return stepTime;
}

/**
 * @brief Network event function for reaction fire target handling. Responsible for updating
 * the HUD with the information that were received from the server
 * @param self The event pointer
 * @param msg The network message to parse the event data from
 */
void CL_ActorReactionFireAddTarget (const eventRegister_t* self, dbuffer* msg)
{
	int shooterEntNum;
	int targetEntNum;
	// if these TUs have arrived at 0, the reaction fire can be triggered
	int tusUntilTriggered;
	int unused;

	NET_ReadFormat(msg, self->formatString, &shooterEntNum, &targetEntNum, &tusUntilTriggered, &unused);

	const le_t* shooter = LE_Get(shooterEntNum);
	if (!shooter)
		LE_NotFoundError(shooterEntNum);

	const le_t* target = LE_Get(targetEntNum);
	if (!target)
		LE_NotFoundError(targetEntNum);

	const char* shooterModel;
	int shooterSkin;
	bool shooterRobot;
	CL_GetReactionFireHead(shooter, &shooterModel, &shooterSkin, &shooterRobot);
	const fireDef_t* shooterFiredef = CL_ActorGetReactionFireFireDef(shooter);
	const float distance = VectorDist(shooter->origin, target->origin);
	const bool outOfRange = shooterFiredef->range < distance;

	UI_ExecuteConfunc("reactionfire_addtarget %i %i \"%s\" %i %i %i %i \"%s\"", shooterEntNum, target->entnum, shooterModel,
			shooterSkin, shooterRobot, tusUntilTriggered, outOfRange, _(shooterFiredef->name));

	LE_CenterView(target);
}
