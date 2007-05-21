/**
 * @file cl_map.c
 * @brief Geoscape/Map management
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "cl_global.h"

/*
==============================================================
MULTI SELECTION DEFINITION
==============================================================
*/

#define MULTISELECT_MAXSELECT 6	/**< Maximal count of elements that can be selected at once */

/**
  * @brief Types of elements that can be selected in map
  */
typedef enum {
	MULTISELECT_TYPE_BASE,
	MULTISELECT_TYPE_MISSION,
	MULTISELECT_TYPE_AIRCRAFT,
	MULTISELECT_TYPE_UFO,
	MULTISELECT_TYPE_NONE
} multiSelectType_t;


/**
  * @brief Structure to manage the multi selection
  */
typedef struct multiSelect_s {
	int nbSelect;						/**< Count of currently selected elements */
	multiSelectType_t selectType[MULTISELECT_MAXSELECT];	/**< Type of currently selected elements */
	int selectId[MULTISELECT_MAXSELECT];	/**< Identifier of currently selected element */
	char popupText[2048];				/**< Text to display in popup_multi_selection menu */
} multiSelect_t;

static multiSelect_t multiSelect;	/**< Data to manage the multi selection */


/*
==============================================================
STATIC DEFINITION
==============================================================
*/

/* Functions */
static qboolean MAP_IsMapPositionSelected(const menuNode_t* node, vec2_t pos, int x, int y, qboolean globe);
static void MAP3D_ScreenToMap(const menuNode_t* node, int x, int y, vec2_t pos);
static void MAP_ScreenToMap(const menuNode_t* node, int x, int y, vec2_t pos);

/* static variables */
static cvar_t* cl_showCoords;
static aircraft_t *selectedAircraft;	/**< Currently selected aircraft */
static aircraft_t *selectedUfo;			/**< Currently selected UFO */
static char text_standard[2048];		/**< Buffer to display standard text in geoscape */
static int centerOnEventIdx = 0;		/**< Current Event centered on 3D geoscape */
static vec3_t finalGlobeAngle = {0, GLOBE_ROTATE, 0};		/**< value of finale ccs.angles for a smooth change of angle (see MAP_CenterOnPoint)*/
static vec2_t final2DGeoscapeCenter = {0.5, 0.5};		/**< value of ccs.center for a smooth change of position (see MAP_CenterOnPoint) */
static qboolean smoothRotation = qfalse;			/**< qtrue if the rotation of 3D geoscape must me smooth */
/*
==============================================================
CLICK ON MAP and MULTI SELECTION FUNCTIONS
==============================================================
*/

/**
 * @brief Add an element in the multiselection list
 */
static void MAP_MultiSelectListAddItem (multiSelectType_t item_type, int item_id,
	const char* item_description, const char* item_name)
{
	Q_strcat(multiSelect.popupText, va("%s\t%s\n", item_description, item_name), sizeof(multiSelect.popupText));
	multiSelect.selectType[multiSelect.nbSelect] = item_type;
	multiSelect.selectId[multiSelect.nbSelect] = item_id;
	multiSelect.nbSelect++;
}

/**
 * @brief Execute action for 1 element of the multi selection
 * Param Cmd_Argv(1) is the element selected in the popup_multi_selection menu
 */
static void MAP_MultiSelectExecuteAction_f (void)
{
	int selected, id;
	aircraft_t* aircraft;

	if (Cmd_Argc() < 2) {
		/* Direct call from code, not from a popup menu */
		selected = 0;
	} else {
		/* Call from a geoscape popup menu (popup_multi_selection) */
		MN_PopMenu(qfalse);
		selected = atoi(Cmd_Argv(1));
	}

	if (selected < 0 || selected >= multiSelect.nbSelect)
		return;
	id = multiSelect.selectId[selected];

	/* Execute action on element */
	switch(multiSelect.selectType[selected]) {
	case MULTISELECT_TYPE_BASE:	/* Select a base */
		if (id >= gd.numBases)
			break;
		MAP_ResetAction();
		Cbuf_ExecuteText(EXEC_NOW, va("mn_select_base %i", id));
		MN_PushMenu("bases");
		break;

	case MULTISELECT_TYPE_MISSION: /* Select a mission */
		if (id >= ccs.numMissions)
			break;

		if (gd.mapAction == MA_INTERCEPT && selMis && selMis == ccs.mission + id) {
			CL_DisplayPopupIntercept(selMis, NULL);
			return;
		}

		MAP_ResetAction();
		selMis = ccs.mission + id;

		if (!Q_strncmp(selMis->def->name, "baseattack", 10)) {
			/* we need no dropship in our base */
			selMis->def->active = qtrue;
			gd.mapAction = MA_BASEATTACK;
			Com_DPrintf("Base attack: %s at %.0f:%.0f\n", selMis->def->name, selMis->realPos[0], selMis->realPos[1]);
			CL_DisplayPopupIntercept(selMis, NULL);
			return;
		} else {
			Com_DPrintf("Select mission: %s at %.0f:%.0f\n", selMis->def->name, selMis->realPos[0], selMis->realPos[1]);
			gd.mapAction = MA_INTERCEPT;
		}
		break;

	case MULTISELECT_TYPE_AIRCRAFT: /* Selection of an aircraft */
		aircraft = AIR_AircraftGetFromIdx(id);
		if (aircraft == NULL) {
			Com_DPrintf("MAP_MultiSelectExecuteAction: selection of an unknow aircraft idx %i\n", id);
			return;
		}

		if (aircraft == selectedAircraft) {
			/* Selection of an already selected aircraft */
			CL_DisplayPopupAircraft(aircraft);	/* Display popup_aircraft */
		} else {
			/* Selection of an unselected aircraft */
			MAP_ResetAction();
			selectedAircraft = aircraft;
		}
		break;

	case MULTISELECT_TYPE_UFO : /* Selection of an UFO */
		/* Get the ufo selected */
		if (id < 0 || id >= gd.numUfos)
			return;
		aircraft = gd.ufos + id;

		if (aircraft == selectedUfo) {
			/* Selection of an already selected ufo */
			CL_DisplayPopupIntercept(NULL, selectedUfo);
		} else {
			/* Selection of an unselected ufo */
			MAP_ResetAction();
			selectedUfo = aircraft;
		}
		break;

	case MULTISELECT_TYPE_NONE :	/* Selection of an element that has been removed */
		break;
	default:
		Com_DPrintf("MAP_MultiSelectExecuteAction: selection of an unknow element type %i\n", multiSelect.selectType[selected]);
	}
}

/**
 * @brief Notify the multi select system that a mission has been removed
 */
static void MAP_MultiSelectNotifyMissionRemoved (const actMis_t* mission)
{
	int num = mission - ccs.mission, i;

	for (i = 0; i < multiSelect.nbSelect; i++)
		if (multiSelect.selectType[i] == MULTISELECT_TYPE_MISSION) {
			if (multiSelect.selectId[i] == num)
				multiSelect.selectType[i] = MULTISELECT_TYPE_NONE;
			else if (multiSelect.selectId[i] > num)
				multiSelect.selectId[i]--;
		}
}

/**
 * @brief Notify the multi select system that an ufo has been removed
 */
static void MAP_MultiSelectNotifyUfoRemoved (const aircraft_t* ufo)
{
	int i;

	/* Deactive all ufos */
	/* @todo : Determine THE corresponding ufo in the multi select list */
	for (i = 0; i < multiSelect.nbSelect; i++)
		if (multiSelect.selectType[i] == MULTISELECT_TYPE_UFO)
			multiSelect.selectType[i] = MULTISELECT_TYPE_NONE;
}

/**
 * @brief Notify the multi select system that an ufo disapeard on radars
 */
static void MAP_MultiSelectNotifyUfoDisappeared (const aircraft_t* ufo)
{
	MAP_MultiSelectNotifyUfoRemoved(ufo);
}

#define MN_MAP_DIST_SELECTION 15

/**
 * @brief Click on the map/geoscape
 */
extern void MAP_MapClick (const menuNode_t* node, int x, int y, qboolean globe)
{
	aircraft_t *aircraft = NULL;
	actMis_t *ms;
	int i;
	vec2_t pos;
	nation_t* nation;
	char clickBuffer[30];

	/* get map position */
	if (globe) {
		MAP3D_ScreenToMap(node, x, y, pos);
	} else {
		MAP_ScreenToMap(node, x, y, pos);
	}
	if (cl_showCoords->value) {
		Com_sprintf(clickBuffer, sizeof(clickBuffer), "Long: %.1f Lat: %.1f", pos[0], pos[1]);
		MN_AddNewMessage("Click"	, clickBuffer, qfalse, MSG_DEBUG, NULL);
		Com_Printf("Clicked at %.1f %.1f\n", pos[0], pos[1]);
	}

	/* new base construction */
	if (gd.mapAction == MA_NEWBASE) {
		if (!MapIsWater(CL_GetMapColor(pos, MAPTYPE_CLIMAZONE))) {
			newBasePos[0] = pos[0];
			newBasePos[1] = pos[1];
			Com_DPrintf("MAP_MapClick: Build base at: %.0f:%.0f\n", pos[0], pos[1]);
			nation = MAP_GetNation(pos);
			if (nation)
				Com_DPrintf("MAP_MapClick: Build base in nation '%s'\n", nation->id);
			MN_PushMenu("popup_newbase");
			return;
		} else {
			MN_AddNewMessage(_("Notice"), _("Could not set up your base at this location"), qfalse, MSG_INFO, NULL);
		}
	} else if (gd.mapAction == MA_UFORADAR) {
		MN_PushMenu("popup_intercept_ufo");
		/* @todo: Select aircraft - follow ufo - fight */
		/* if shoot down - we have a new crashsite mission if color != water */
	}

	/* Init datas for multi selection */
	multiSelect.nbSelect = 0;
	memset(multiSelect.popupText, 0, sizeof(multiSelect.popupText));

	/* Get selected missions */
	for (i = 0, ms = ccs.mission; i < ccs.numMissions && multiSelect.nbSelect < MULTISELECT_MAXSELECT; i++, ms++)
		if (MAP_IsMapPositionSelected(node, ms->realPos, x, y, globe))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_MISSION, i, _(ms->def->type), _(ms->def->location));

	/* Get selected bases */
	for (i = 0; i < gd.numBases && multiSelect.nbSelect < MULTISELECT_MAXSELECT; i++) {
		if (MAP_IsMapPositionSelected(node, gd.bases[i].pos, x, y, globe))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_BASE, i, _("Base"), gd.bases[i].name);

		/* Get selected aircrafts wich belong to the base */
		for (aircraft = gd.bases[i].aircraft + gd.bases[i].numAircraftInBase - 1;
		aircraft >= gd.bases[i].aircraft; aircraft--)
			if (aircraft->status > AIR_HOME && aircraft->fuel > 0 && MAP_IsMapPositionSelected(node, aircraft->pos, x, y, globe))
				MAP_MultiSelectListAddItem(MULTISELECT_TYPE_AIRCRAFT, aircraft->idx, _("Aircraft"), aircraft->name);
	}

	/* Get selected ufos */
	for (aircraft = gd.ufos + gd.numUfos - 1; aircraft >= gd.ufos; aircraft--)
		if (aircraft->visible
#if DEBUG
		|| Cvar_VariableInteger("debug_showufos")
#endif
		)
			if (aircraft->status > AIR_HOME && MAP_IsMapPositionSelected(node, aircraft->pos, x, y, globe))
				MAP_MultiSelectListAddItem(MULTISELECT_TYPE_UFO, aircraft - gd.ufos, _("UFO"), aircraft->name);

	if (multiSelect.nbSelect == 1) {
		/* Execute directly action for the only one element selected */
		Cmd_ExecuteString("multi_select_click");
 	} else if (multiSelect.nbSelect > 1) {
		/* Display popup for multi selection */
		menuText[TEXT_MULTISELECTION] = multiSelect.popupText;
		MN_PushMenu("popup_multi_selection");
	} else {
		/* Nothing selected */
		if (!selectedAircraft)
			MAP_ResetAction();
		else if (selectedAircraft->status > AIR_HOME && AIR_AircraftHasEnoughFuel(selectedAircraft, pos)) {
			/* Move the selected aircraft to the position clicked */
			MAP_MapCalcLine(selectedAircraft->pos, pos, &(selectedAircraft->route));
			selectedAircraft->status = AIR_TRANSIT;
			selectedAircraft->time = aircraft->point = 0;
		}
	}
}


/*
==============================================================
GEOSCAPE DRAWING AND COORDINATES
==============================================================
*/

#define MN_MAP_DIST_SELECTION 15
/**
 * @brief Tell if the specified position is considered clicked
 */
static qboolean MAP_IsMapPositionSelected (const menuNode_t* node, vec2_t pos, int x, int y, qboolean globe)
{
	int msx, msy;

	if (!globe) {
		if (MAP_MapToScreen(node, pos, &msx, &msy))
			if (x >= msx - MN_MAP_DIST_SELECTION && x <= msx + MN_MAP_DIST_SELECTION
			 && y >= msy - MN_MAP_DIST_SELECTION && y <= msy + MN_MAP_DIST_SELECTION)
				return qtrue;
	} else {
		if (MAP_3DMapToScreen(node, pos, &msx, &msy, NULL))
			if (x >= msx - MN_MAP_DIST_SELECTION && x <= msx + MN_MAP_DIST_SELECTION
			 && y >= msy - MN_MAP_DIST_SELECTION && y <= msy + MN_MAP_DIST_SELECTION)
				return qtrue;
	}

	return qfalse;
}

#define GLOBE_RADIUS gl_3dmapradius->value * (ccs.zoom / 4.0f) * 0.1
/**
 * @brief Transform a 2D position on the map to screen coordinates.
 * @param[in] pos vector that holds longitude and latitude
 * @param[out] x normalized (rotated and scaled) x value of mouseclick
 * @param[out] y normalized (rotated and scaled) y value of mouseclick
 * @param[out] z z value of the given latitude and longitude - might also be NULL if not needed
 * @sa MAP_MapToScreen
 * @sa MAP3D_ScreenToMap
 * @return qtrue if the point is visible, qfalse else (if it's outside the node or on the wrong side of earth).
 * @note In the function, we do the opposite of MAP3D_ScreenToMap
 */
extern qboolean MAP_3DMapToScreen (const menuNode_t* node, const vec2_t pos, int *x, int *y, int *z)
{
	vec2_t mid;
	vec3_t v, v1, rotationAxis;
	const float radius = GLOBE_RADIUS;

	PolarToVec(pos, v);

	/* rotate the vector to switch of reference frame.
	 *	We switch from the static frame of earth to the local frame of the player (opposite rotation of MAP3D_ScreenToMap) */
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v1, rotationAxis, v, - ccs.angles[PITCH]);

	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v, rotationAxis, v1, - ccs.angles[YAW]);

	/* set mid to the coordinates of the center of the globe */
	Vector2Set(mid, (node->pos[0] + node->size[0]) / 2.0f, (node->pos[1] + node->size[1]) / 2.0f);

	/* We now convert those coordinates relative to the center of the globe to coordinates of the screen
	 *	(which are relative to the upper left side of the screen) */
	*x = (int) (mid[0] - radius * v[1]);
	*y = (int) (mid[1] - radius * v[0]);
	/* FIXME: I'm not sure of what z should contain (normalized value ? sign ?) */
	if (z) {
		*z = (int) (- radius * v[2]);
	}

	/* if the point is on the wrong side of earth, the player cannot see it */
	if (v[2] > 0)
		return qfalse;

	/* if the point is outside the screen, the player cannot see it */
	if (*x < node->pos[0] && *y < node->pos[1] && *x > node->pos[0] + node->size[0] && *y > node->pos[1] + node->size[1])
		return qfalse;

	return qtrue;
}

/**
 * @brief Draws a 3D marker on geoscape if the player can see it.
 * @param[in] node Menu node.
 * @param[in] pos Longitude and latitude of the marker to draw.
 * @param[in] theta Angle (degree) of the model to the horizontal.
 * @param[in] model The name of the model of the marker.
 * @param[in] globe qtrue if the 3D marker is to be drawn on 3D geoscape, qfalse else.
 */
extern qboolean MAP_Draw3DMarkerIfVisible (const menuNode_t* node, const vec2_t pos, float theta, const char *model, qboolean globe)
{
	int x, y, z;
	vec3_t screenPos, angles, v;
	float zoom;
	float costheta, sintheta;
	const float radius = GLOBE_RADIUS;
	qboolean test;

	if (globe)
		test = MAP_3DMapToScreen(node, pos, &x, &y, &z);
	else {
		test = MAP_MapToScreen(node, pos, &x, &y);
		z = 10;
	}

	if (test) {
		/* Set position of the model on the screen */
		VectorSet(screenPos, x, y, z);

		if (globe) {
			/* Set angles of the model */
			VectorCopy(screenPos, v);
			v[0] -= (node->pos[0] + node->size[0]) / 2.0f;
			v[1] -= (node->pos[1] + node->size[1]) / 2.0f;

			angles[0] = theta;
			costheta = cos(angles[0] * torad);
			sintheta = sin(angles[0] * torad);

			angles[1] = 180 - asin((v[0] * costheta + v[1] * sintheta) / radius) * todeg;
			angles[2] = + asin((v[0] * sintheta - v[1] * costheta) / radius) * todeg;

			/* Set zoom */
			zoom = 0.4 + ccs.zoom * (float) z / radius / 2.0;
		} else {
			VectorSet(angles, theta, 180, 0);
			zoom = 0.6f;
		}

		/* Draw */
		re.Draw3DMapMarkers(angles, zoom, screenPos, model);
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief Transform a 2D position on the map to screen coordinates.
 * @param[in] node Menu node
 * @param[in] pos Position on the map described by longitude and latitude
 * @param[out] x X coordinate on the screen
 * @param[out] y Y coordinate on the screen
 * @returns qtrue if the screen position is within the boundaries of the menu
 * node. Otherwise returns qfalse.
 * @sa MAP_3DMapToScreen
 */
extern qboolean MAP_MapToScreen (const menuNode_t* node, const vec2_t pos,
		int *x, int *y)
{
	float sx;

	/* get "raw" position */
	sx = pos[0] / 360 + ccs.center[0] - 0.5;

	/* shift it on screen */
	if (sx < -0.5)
		sx += 1.0;
	else if (sx > +0.5)
		sx -= 1.0;

	*x = node->pos[0] + 0.5 * node->size[0] - sx * node->size[0] * ccs.zoom;
	*y = node->pos[1] + 0.5 * node->size[1] -
		(pos[1] / 180 + ccs.center[1] - 0.5) * node->size[1] * ccs.zoom;

	if (*x < node->pos[0] &&
			*y < node->pos[1] &&
			*x > node->pos[0] + node->size[0] &&
			*y > node->pos[1] + node->size[1])
		return qfalse;
	return qtrue;
}

/**
 * @brief Return longitude and latitude of a point of the screen for 2D geoscape
 * @return pos vec2_t was filled with longitude and latitude
 * @param[in] x X coordinate on the screen that was clicked to
 * @param[in] y Y coordinate on the screen that was clicked to
 * @param[in] node The current menuNode we was clicking into (3dmap or map)
 */
static void MAP_ScreenToMap (const menuNode_t* node, int x, int y, vec2_t pos)
{
	pos[0] = (((node->pos[0] - x) / node->size[0] + 0.5) / ccs.zoom - (ccs.center[0] - 0.5)) * 360.0;
	pos[1] = (((node->pos[1] - y) / node->size[1] + 0.5) / ccs.zoom - (ccs.center[1] - 0.5)) * 180.0;

	while (pos[0] > 180.0)
		pos[0] -= 360.0;
	while (pos[0] < -180.0)
		pos[0] += 360.0;
}

/**
 * @brief Return longitude and latitude of a point of the screen for 3D geoscape (globe)
 * @return pos vec2_t was filled with longitude and latitude
 * @param[in] x X coordinate on the screen that was clicked to
 * @param[in] y Y coordinate on the screen that was clicked to
 * @param[in] node The current menuNode we was clicking into (3dmap or map)
 * @sa MAP_3DMapToScreen
 */
static void MAP3D_ScreenToMap (const menuNode_t* node, int x, int y, vec2_t pos)
{
	vec2_t mid;
	vec3_t v, v1, rotationAxis;
	float dist;
	const float radius = GLOBE_RADIUS;

	/* set mid to the coordinates of the center of the globe */
	Vector2Set(mid, (node->pos[0] + node->size[0]) / 2.0f, (node->pos[1] + node->size[1]) / 2.0f);

	/* stop if we click outside the globe (distance is the distance of the point to the center of the globe) */
	dist = sqrt((x - mid[0]) * (x - mid[0]) + (y - mid[1]) * (y - mid[1]));
	if (dist > radius) {
		Vector2Set(pos, -1.0, -1.0);
		return;
	}

	/* calculate the coordinates in the local frame
	 * this frame is the frame of the screen.
	 * v[0] is the vertical axis of the screen
	 * v[1] is the horizontal axis of the screen
	 * v[2] is the axis perpendicular to the screen - we get its value knowing that norm of v is egal to radius
	 * 	(because the point is on the globe) */
	v[0] = - (y - mid[1]);
	v[1] = - (x - mid[0]);
	v[2] = - sqrt(radius * radius - (x - mid[0]) * (x - mid[0]) - (y - mid[1]) * (y - mid[1]));
	VectorNormalize(v);

	/* rotate the vector to switch of reference frame */
	/* note the ccs.angles[ROLL] is always 0, so there is only 2 rotations and not 3 */
	/*	and that GLOBE_ROTATE is already included in ccs.angles[YAW] */
	/* first rotation is along the horizontal axis of the screen, to put north-south axis of the earth
	 *	perpendicular to the screen */
	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v1, rotationAxis, v, ccs.angles[YAW]);

	/* second rotation is to rotate the earth around its north-south axis
	 *	so that Greenwich meridian is along the vertical axis of the screen */
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v, rotationAxis, v1, ccs.angles[PITCH]);

	/* we therefore got in v the coordinates of the point in the static frame of the earth
	 *	that we can convert in polar coordinates to get its latitude and longitude */
	VecToPolar(v, pos);
}

/**
 * @brief Calculate the shortest way to go from start to end on a sphere
 * @param[in] start The point you start from
 * @param[in] end The point you go to
 * @param[out] line Contains the shortest path to go from start to end
 * @sa MAP_MapDrawLine
 */
extern void MAP_MapCalcLine (const vec2_t start, const vec2_t end, mapline_t* line)
{
	vec3_t s, e, v;
	vec3_t normal;
	vec2_t trafo, sa, ea;
	float cosTrafo, sinTrafo;
	float phiStart, phiEnd, dPhi, phi;
	float *p, *last;
	int i, n;

	/* get plane normal */
	PolarToVec(start, s);
	PolarToVec(end, e);
	CrossProduct(s, e, normal);
	VectorNormalize(normal);

	/* get transformation */
	VecToPolar(normal, trafo);
	cosTrafo = cos(trafo[1] * torad);
	sinTrafo = sin(trafo[1] * torad);

	sa[0] = start[0] - trafo[0];
	sa[1] = start[1];
	PolarToVec(sa, s);
	ea[0] = end[0] - trafo[0];
	ea[1] = end[1];
	PolarToVec(ea, e);

	phiStart = atan2(s[1], cosTrafo * s[2] - sinTrafo * s[0]);
	phiEnd = atan2(e[1], cosTrafo * e[2] - sinTrafo * e[0]);

	/* get waypoints */
	if (phiEnd < phiStart - M_PI)
		phiEnd += 2 * M_PI;
	if (phiEnd > phiStart + M_PI)
		phiEnd -= 2 * M_PI;

	n = (phiEnd - phiStart) / M_PI * LINE_MAXSEG;
	if (n > 0)
		n = n + 1;
	else
		n = -n + 1;

/*	Com_Printf("#(%3.1f %3.1f) -> (%3.1f %3.1f)\n", start[0], start[1], end[0], end[1]); */

	line->distance = fabs(phiEnd - phiStart) / n * todeg;
	line->numPoints = n + 1;
	dPhi = (phiEnd - phiStart) / n;
	p = NULL;
	for (phi = phiStart, i = 0; i <= n; phi += dPhi, i++) {
		last = p;
		p = line->point[i];
		VectorSet(v, -sinTrafo * cos(phi), sin(phi), cosTrafo * cos(phi));
		VecToPolar(v, p);
		p[0] += trafo[0];

		if (!last) {
			while (p[0] < -180.0)
				p[0] += 360.0;
			while (p[0] > +180.0)
				p[0] -= 360.0;
		} else {
			while (p[0] - last[0] > +180.0)
				p[0] -= 360.0;
			while (p[0] - last[0] < -180.0)
				p[0] += 360.0;
		}
	}
}

/**
 * @brief Draw a path on a menu node (usually the geoscape map)
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the geoscape menu node.
 * @param[in] line The path which is to be drawn
 * @sa MAP_MapCalcLine
 */
static void MAP_MapDrawLine (const menuNode_t* node, const mapline_t* line)
{
	const vec4_t color = {1, 0.5, 0.5, 1};
	int pts[LINE_MAXPTS * 2];
	int *p;
	int i, start, old;

	/* draw */
	re.DrawColor(color);
	start = 0;
	old = 512;
	for (i = 0, p = pts; i < line->numPoints; i++, p += 2) {
		MAP_MapToScreen(node, line->point[i], p, p + 1);

		/* If we cross longitude 180 degree (right/left edge of the screen), draw the first part of the path */
		if (i > start && abs(p[0] - old) > 512) {
			/* shift last point */
			int diff;

			if (p[0] - old > 512)
				diff = -node->size[0] * ccs.zoom;
			else
				diff = node->size[0] * ccs.zoom;
			p[0] += diff;

			/* wrap around screen border */
			re.DrawLineStrip(i - start + 1, pts);

			/* first path of the path is drawn, now we begin the second part of the path */
			/* shift first point, continue drawing */
			start = --i;
			pts[0] = p[-2] - diff;
			pts[1] = p[-1];
			p = pts;
		}
		old = p[0];
	}

	re.DrawLineStrip(i - start, pts);
	re.DrawColor(NULL);
}

/**
 * @brief Draw a path on a menu node (usually the 3Dgeoscape map)
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the 3Dgeoscape menu node.
 * @param[in] line The path which is to be drawn
 * @sa MAP_MapCalcLine
 */
static void MAP_3DMapDrawLine (const menuNode_t* node, const mapline_t* line)
{
	const vec4_t color = {1, 0.5, 0.5, 1};
	int pts[LINE_MAXPTS * 2];
	int *p;
	int i, numPoints;

	/* draw only when the point of the path is visible*/
	re.DrawColor(color);
	for (i = 0, numPoints = 0, p = pts; i < line->numPoints; i++) {
		if (MAP_3DMapToScreen(node, line->point[i], p, p + 1, NULL)) {
			 p += 2;
			 numPoints++;
		}
	}

	re.DrawLineStrip(numPoints, pts);
	re.DrawColor(NULL);
}

#define CIRCLE_DRAW_POINTS	60
/**
 * @brief Draw equidistant points from a given point on a menu node
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the geoscape menu node.
 * @param[in] center The latitude and longitude of center point
 * @param[in] angle The angle defining the distance of the equidistant points to center
 * @param[in] color The color for drawing
 * @param[in] globe qtrue is this is 3D geoscape, qfalse if this is 2D geoscape
 * @sa RADAR_DrawCoverage
 */
extern void MAP_MapDrawEquidistantPoints (const menuNode_t* node, vec2_t center, const float angle, const vec4_t color, qboolean globe)
{
	int i, xCircle, yCircle, zCircle;
	int pts[CIRCLE_DRAW_POINTS * 2 + 2];
	vec2_t posCircle;
	qboolean draw, oldDraw = qfalse;
	int numPoints = 0;
	vec3_t initialVector, rotationAxis, currentPoint, centerPos;

	/* Set color */
	re.DrawColor(color);

	/* Set centerPos corresponding to cartesian coordinates of the center point */
	PolarToVec(center, centerPos);

	/* Find a perpendicular vector to centerPos, and rotate centerPos around him to obtain one point distant of angle from centerPos */
	PerpendicularVector(rotationAxis, centerPos);
	RotatePointAroundVector(initialVector, rotationAxis, centerPos, angle);

	/* Now, each equidistant point is given by a rotation around centerPos */
	for (i = 0; i <= CIRCLE_DRAW_POINTS; i++) {
		RotatePointAroundVector(currentPoint, centerPos, initialVector, i * 360 / CIRCLE_DRAW_POINTS);
		VecToPolar(currentPoint, posCircle);
		draw = qfalse;
		if (!globe) {
			if (MAP_MapToScreen(node, posCircle, &xCircle, &yCircle)) {
				draw = qtrue;
				/* if we switch from left to right side of the screen (or the opposite), end this path */
				if (numPoints != 0 && abs(pts[(numPoints -1) * 2] - xCircle) > 512)
					oldDraw = qfalse;
			}
		}
		else {
			if (MAP_3DMapToScreen(node, posCircle, &xCircle, &yCircle, &zCircle))
				draw = qtrue;
		}
		/* if moving from a point of the screen to a distant one, draw the path we already calculated, and begin a new path
		 * (to avoid unwanted lines) */
		if (draw != oldDraw && i != 0) {
			re.DrawLineStrip(numPoints, pts);
			numPoints = 0;
		}
		/* if the current point is to be drawn, add it to the path */
		if (draw) {
			pts[numPoints * 2] = xCircle;
			pts[numPoints * 2 + 1] = yCircle;
			numPoints++;
		}
		/* update value of oldDraw */
		oldDraw = draw;
	}

	/* Draw the last path */
	re.DrawLineStrip(numPoints, pts);
	re.DrawColor(NULL);
}

/**
 * @brief Return the angle of a model given its position and destination.
 * @param[in] start Latitude and longitude of the position of the model.
 * @param[in] end Latitude and longitude of aimed point.
 * @param[in] direction vec3_t giving current direction of the model (NULL if the model is idle).
 * @param[out] ortVector If not NULL, this will be filled with the normalized vector around which rotation allows to go toward @c direction.
 * @param[in] globe qtrue if this is 3D geoscape, qfalse else.
 * @return Angle (degrees) of rotation around the axis perpendicular to the screen for a model in @c start going toward @c end.
 */
extern float MAP_AngleOfPath (const vec3_t start, const vec2_t end, vec3_t direction, vec3_t ortVector, qboolean globe)
{
	float angle = 0.0f;
	vec3_t start3D, end3D, tangentVector, v, rotationAxis;
	float dist;

	/* calculate the vector tangent to movement */
	PolarToVec(start, start3D);
	PolarToVec(end, end3D);
	if (ortVector) {
		CrossProduct(start3D, end3D, ortVector);
		VectorNormalize(ortVector);
		CrossProduct(ortVector, start3D, tangentVector);
	} else {
		CrossProduct(start3D, end3D, v);
		CrossProduct(v, start3D, tangentVector);
	}
	VectorNormalize(tangentVector);

	/* smooth change of direction if the model is not idle */
	if (direction) {
		VectorSubtract(tangentVector, direction, v);
		dist = VectorLength(v);
		if (dist > 0.01) {
			CrossProduct(direction, tangentVector, rotationAxis);
			VectorNormalize(rotationAxis);
			RotatePointAroundVector(v, rotationAxis, direction, 5.0);
			VectorCopy(v, direction);
			VectorSubtract(tangentVector, direction, v);
			if (VectorLength(v) < dist) {
				VectorCopy(direction, tangentVector);
			}
			else {
				VectorCopy(tangentVector, direction);
			}
		}
	}

	if (globe) {
		/* rotate vector tangent to movement in the frame of the screen */
		VectorSet(rotationAxis, 0, 0, 1);
		RotatePointAroundVector(v, rotationAxis, tangentVector, - ccs.angles[PITCH]);
		VectorSet(rotationAxis, 0, 1, 0);
		RotatePointAroundVector(tangentVector, rotationAxis, v, - ccs.angles[YAW]);
	} else {
		VectorSet(rotationAxis, 0, 0, 1);
		RotatePointAroundVector(v, rotationAxis, tangentVector, - start[0]);
		VectorSet(rotationAxis, 0, 1, 0);
		RotatePointAroundVector(tangentVector, rotationAxis, v, start[1] + 90.0f);
	}

	/* calculate the orientation angle of the model around axis perpendicular to the screen */
	angle = todeg * atan(tangentVector[0] / tangentVector[1]);
	if (tangentVector[1] > 0)
		angle += 180.0f;

	return angle;
}

/**
 * @brief Returns position of the model corresponding to centerOnEventIdx.
 * @param[out] Vector Latitude and longitude of the model (finalAngle[2] is always 0).
 * @note Vector is a vec3_t if globe is qtrue, and a vec2_t if globe is qfalse.
 * @sa MAP_CenterOnPoint
 */
static void MAP_GetGeoscapeAngle (float *Vector, qboolean globe)
{
	int i;
	int counter = 0;
	int maxEventIdx;
	base_t *base;
	aircraft_t *aircraft;

	/* If the value of maxEventIdx is too big or to low, restart from begining */
	maxEventIdx = ccs.numMissions + gd.numBases - 1;
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases ; base--) {
		for (i = 0, aircraft = (aircraft_t *) base->aircraft; i < base->numAircraftInBase; i++, aircraft++) {
			if (aircraft->status > AIR_HOME)
				maxEventIdx++;
		}
	}
	for (aircraft = gd.ufos + gd.numUfos - 1; aircraft >= gd.ufos; aircraft --) {
		if (aircraft->visible) {
			maxEventIdx++;
		}
	}
	/* check centerOnEventIdx is within the bounds */
	if (centerOnEventIdx < 0)
		centerOnEventIdx = maxEventIdx;
	if (centerOnEventIdx > maxEventIdx)
		centerOnEventIdx = 0;

	/* Cycle through missions */
	if (centerOnEventIdx < ccs.numMissions) {
		if (globe)
			VectorSet(Vector, ccs.mission[centerOnEventIdx - counter].realPos[0], -ccs.mission[centerOnEventIdx - counter].realPos[1], 0);
		else
			Vector2Set(Vector, ccs.mission[centerOnEventIdx - counter].realPos[0], ccs.mission[centerOnEventIdx - counter].realPos[1]);
		MAP_ResetAction();
		selMis = ccs.mission + centerOnEventIdx - counter;
		return;
	}
	counter += ccs.numMissions;

	/* Cycle through bases */
	if (centerOnEventIdx < gd.numBases + counter) {
		if (globe)
			VectorSet(Vector, gd.bases[centerOnEventIdx - counter].pos[0], -gd.bases[centerOnEventIdx - counter].pos[1], 0);
		else
			Vector2Set(Vector, gd.bases[centerOnEventIdx - counter].pos[0], gd.bases[centerOnEventIdx - counter].pos[1]);
		return;
	}
	counter += gd.numBases;

	/* Cycle through aircrafts (only those present on geoscape) */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases ; base--) {
		for (i = 0, aircraft = (aircraft_t *) base->aircraft; i < base->numAircraftInBase; i++, aircraft++) {
			if (aircraft->status > AIR_HOME) {
				if (centerOnEventIdx == counter) {
					if (globe)
						VectorSet(Vector, aircraft->pos[0], -aircraft->pos[1], 0);
					else
						Vector2Set(Vector, aircraft->pos[0], aircraft->pos[1]);
					MAP_ResetAction();
					selectedAircraft = aircraft;
					return;
				}
				counter++;
			}
		}
	}

	/* Cycle through UFO (only those visible on geoscape) */
	for (aircraft = gd.ufos + gd.numUfos - 1; aircraft >= gd.ufos; aircraft --) {
		if (aircraft->visible) {
			if (centerOnEventIdx == counter) {
				if (globe)
					VectorSet(Vector, aircraft->pos[0], -aircraft->pos[1], 0);
				else
					Vector2Set(Vector, aircraft->pos[0], aircraft->pos[1]);
				MAP_ResetAction();
				selectedUfo = aircraft;
				return;
			}
			counter++;
		}
	}
}

#define ZOOM_LIMIT	2.0f
/**
 * @brief Switch to next model on 2D and 3D geoscape.
 * @note Set @c smoothRotation to @c qtrue to allow a smooth rotation in MAP_DrawMap.
 * @note This function sets the value of finalGlobeAngle (for 3D) or final2DGeoscapeCenter (for 2D),
 *  which contains the finale value that ccs.angles or ccs.centre must respectively take.
 * @sa MAP_GetGeoscapeAngle
 * @sa MAP_DrawMap
 * @sa MAP3D_SmoothRotate
 * @sa MAP_SmoothTranslate
 */
extern void MAP_CenterOnPoint (void)
{
	menu_t *activeMenu = NULL;

	/* this function only concerns maps */
	activeMenu = MN_ActiveMenu();
	if (Q_strncmp(activeMenu->name, "map", 3))
		return;

	centerOnEventIdx++;

	if (cl_3dmap->value) {
		/* case 3D geoscape */
		MAP_GetGeoscapeAngle(finalGlobeAngle, qtrue);
		finalGlobeAngle[1] += GLOBE_ROTATE;
	} else {
		/* case 2D geoscape */
		MAP_GetGeoscapeAngle(final2DGeoscapeCenter, qfalse);
		Vector2Set(final2DGeoscapeCenter, 0.5f - final2DGeoscapeCenter[0] / 360.0f, 0.5f - final2DGeoscapeCenter[1] / 180.0f);
		if (final2DGeoscapeCenter[1] < 0.5 / ZOOM_LIMIT)
			final2DGeoscapeCenter[1] = 0.5 / ZOOM_LIMIT;
		if (final2DGeoscapeCenter[1] > 1.0 - 0.5 / ZOOM_LIMIT)
			final2DGeoscapeCenter[1] = 1.0 - 0.5 / ZOOM_LIMIT;
	}

	smoothRotation = qtrue;
}

#define SMOOTHING_STEP	5.0f
/**
 * @brief smooth rotation of the 3D geoscape
 * @note updates slowly values of ccs.angles so that its value goes to finalGlobeAngle
 * @sa MAP_DrawMap
 * @sa MAP_CenterOnPoint
 */
static void MAP3D_SmoothRotate (void)
{
	vec3_t diff;
	float length;

	VectorSubtract(finalGlobeAngle, ccs.angles, diff);
	length = VectorLength(diff);
	if (length < SMOOTHING_STEP) {
		VectorCopy(finalGlobeAngle, ccs.angles);
		smoothRotation = qfalse;
	} else {
		VectorScale(diff, SMOOTHING_STEP / length, diff);
		VectorAdd(ccs.angles, diff, ccs.angles);
	}
}

#define SMOOTHING_STEP_2D	0.02f
/**
 * @brief smooth translation of the 2D geoscape
 * @note updates slowly values of ccs.center so that its value goes to final2DGeoscapeCenter
 * @note updates slowly values of ccs.zoom so that its value goes to ZOOM_LIMIT
 * @sa MAP_DrawMap
 * @sa MAP_CenterOnPoint
 */
static void MAP_SmoothTranslate (void)
{
	vec2_t diff;
	float length, diff_zoom;

	diff[0] = final2DGeoscapeCenter[0] - ccs.center[0];
	diff[1] = final2DGeoscapeCenter[1] - ccs.center[1];
	diff_zoom = ZOOM_LIMIT - ccs.zoom;

	length = sqrt (diff[0] * diff[0] + diff[1] * diff[1]);
	if (length < SMOOTHING_STEP_2D) {
		ccs.center[0] = final2DGeoscapeCenter[0];
		ccs.center[1] = final2DGeoscapeCenter[1];
		ccs.zoom = ZOOM_LIMIT;
		smoothRotation = qfalse;
	} else {
		ccs.center[0] = ccs.center[0] + SMOOTHING_STEP_2D * diff[0] / length;
		ccs.center[1] = ccs.center[1] + SMOOTHING_STEP_2D * diff[1] / length;
		ccs.zoom = ccs.zoom + SMOOTHING_STEP_2D * diff_zoom / length;
	}
}

#define SELECT_CIRCLE_RADIUS	6
/**
 * @brief
 */
static void MAP_Draw3DMapMarkers (const menuNode_t * node)
{
	aircraft_t *aircraft;
	actMis_t *ms = NULL;
	aircraftProjectile_t *projectile;
	int i, j;
	base_t* base;
	int borders[MAX_NATION_BORDERS * 2];	/**< GL_LINE_LOOP coordinates for nation borders */
	int x, y, z;
	float angle = 0.0f;
	const vec2_t northPole = {0.0f, 90.0f};
	const vec4_t yellow = {1.0f, 0.874f, 0.294f, 1.0f};
#if 0
	vec2_t pos = {0, 0};

	if (MAP_3DMapToScreen(node, pos, &x, &y, &z))
		re.FontDrawString("f_verysmall", 0, x, y, x, y, node->size[0], 0, 0, "0, 0", 0, 0, NULL, qfalse);
	pos[0] = 90; pos[1] = 90;
	if (MAP_3DMapToScreen(node, pos, &x, &y, &z))
		re.FontDrawString("f_verysmall", 0, x, y, x, y, node->size[0], 0, 0, "90, 90", 0, 0, NULL, qfalse);
	pos[0] = 90; pos[1] = 180;
	if (MAP_3DMapToScreen(node, pos, &x, &y, &z))
		re.FontDrawString("f_verysmall", 0, x, y, x, y, node->size[0], 0, 0, "90, 180", 0, 0, NULL, qfalse);
#endif

	/* draw mission pics */
	Cvar_Set("mn_mapdaytime", "");
	for (i = 0; i < ccs.numMissions; i++) {
		ms = &ccs.mission[i];
		angle = MAP_AngleOfPath(ms->realPos, northPole, NULL, NULL, qtrue);
		if (!MAP_3DMapToScreen(node, ms->realPos, &x, &y, NULL))
			continue;

		if (ms == selMis) {
			if (!selMis->def->active)
				MAP_MapDrawEquidistantPoints (node,  ms->realPos, SELECT_CIRCLE_RADIUS, yellow, qtrue);
			Cvar_Set("mn_mapdaytime", CL_MapIsNight(ms->realPos) ? _("Night") : _("Day"));
		}
		/* Draw mission model (this must be after drawing 'selected circle' so that the model looks above it)*/
		MAP_Draw3DMarkerIfVisible(node, ms->realPos, angle, "mission", qtrue);
	}

	/* draw base pics */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--) {
		if (!base->founded)
			continue;

		angle = MAP_AngleOfPath(base->pos, northPole, NULL, NULL, qtrue);

		/* Draw base radar info */
		RADAR_DrawInMap(node, &(base->radar), base->pos, qtrue);

		/* Draw base */
		if (base->baseStatus == BASE_UNDER_ATTACK)
			MAP_Draw3DMarkerIfVisible(node, base->pos, angle, "baseattack", qtrue);
		else
			MAP_Draw3DMarkerIfVisible(node, base->pos, angle, "base", qtrue);

		/* draw aircraft */
		for (i = 0, aircraft = (aircraft_t *) base->aircraft; i < base->numAircraftInBase; i++, aircraft++)
			if (aircraft->status > AIR_HOME) {

				/* Draw aircraft radar */
				RADAR_DrawInMap(node, &(aircraft->radar), aircraft->pos, qtrue);

				/* Draw aircraft route */
				if (aircraft->status >= AIR_TRANSIT) {
					mapline_t path;

					path.numPoints = aircraft->route.numPoints - aircraft->point;
					/* @todo : check why path.numPoints can be sometime equal to -1 */
					if (path.numPoints > 1) {
						memcpy(path.point, aircraft->pos, sizeof(vec2_t));
							memcpy(path.point + 1, aircraft->route.point + aircraft->point + 1, (path.numPoints - 1) * sizeof(vec2_t));
						MAP_3DMapDrawLine(node, &path);
					}
					angle = MAP_AngleOfPath(aircraft->pos, aircraft->route.point[aircraft->route.numPoints - 1], aircraft->direction, NULL, qtrue);
				} else {
					angle = MAP_AngleOfPath(aircraft->pos, northPole, aircraft->direction, NULL, qtrue);
				}

				/* Draw a circle around selected aircraft */
				if (aircraft == selectedAircraft) {
					MAP_MapDrawEquidistantPoints(node, aircraft->pos, SELECT_CIRCLE_RADIUS, yellow, qtrue);

					/* Draw a circle around ufo purchased by selected aircraft */
					if (aircraft->status == AIR_UFO && MAP_3DMapToScreen(node, (gd.ufos + aircraft->ufo)->pos, &x, &y, NULL))
						MAP_MapDrawEquidistantPoints(node, aircraft->pos, SELECT_CIRCLE_RADIUS, yellow, qtrue);
				}

				/* Draw aircraft (this must be after drawing 'selected circle' so that the aircraft looks above it)*/
				MAP_Draw3DMarkerIfVisible(node, aircraft->pos, angle, aircraft->model, qtrue);
			}
	}

	/* draws ufos */
	for (aircraft = gd.ufos + gd.numUfos - 1; aircraft >= gd.ufos; aircraft --) {
#ifdef DEBUG
		/* in debug mode you execute set showufos 1 to see the ufos on geoscape */
		if (Cvar_VariableInteger("debug_showufos")) {
			MAP_3DMapDrawLine(node, &(aircraft->route)); /* Draw ufo route */
		} else
#endif
		if (!aircraft->visible || !MAP_3DMapToScreen(node, aircraft->pos, &x, &y, NULL))
			continue;
		if (aircraft == selectedUfo)
			MAP_MapDrawEquidistantPoints (node, aircraft->pos, SELECT_CIRCLE_RADIUS, yellow, qtrue);
		angle = MAP_AngleOfPath(aircraft->pos, aircraft->route.point[aircraft->route.numPoints - 1], aircraft->direction, NULL, qtrue);
		MAP_Draw3DMarkerIfVisible(node, aircraft->pos, angle, aircraft->model, qtrue);
	}

	/* draws projectiles */
	for (projectile = gd.projectiles + gd.numProjectiles - 1; projectile >= gd.projectiles; projectile --) {
		MAP_Draw3DMarkerIfVisible(node, projectile->pos, projectile->angle, "missile", qtrue);
	}

	/* FIXME */
	/* use the latitude and longitude values from nation border definition to draw a polygon */
	for (i = 0; i < gd.numNations; i++) {
		/* font color for nations */
		re.DrawColor(gd.nations[i].color);
		if (gd.nations[i].numBorders) {
			for (j = 0; j < gd.nations[i].numBorders; j++) {
				/* FIXME: doesn't work for z positions */
				MAP_3DMapToScreen(node, gd.nations[i].borders[j], &x, &y, &z);
				borders[j * 2] = x;
				borders[j * 2 + 1] = y;
/*				borders[j * 2 + 2] = z;*/
			}
			re.DrawPolygon(gd.nations[i].numBorders, borders);
			re.DrawColor(NULL);
			re.DrawLineLoop(gd.nations[i].numBorders, borders);
		}
	}

	re.DrawColor(NULL);
}

/**
 * @brief Draws all ufos, aircraft, bases and so on to the geoscape map
 * @sa MAP_DrawMap
 */
static void MAP_DrawMapMarkers (const menuNode_t* node)
{
	aircraft_t *aircraft;
	actMis_t *ms;
	aircraftProjectile_t *projectile;
	int x, y, i, j;
	float angle = 0.0f;
	base_t* base;
	const char* font = NULL;
	int borders[MAX_NATION_BORDERS * 2];	/**< GL_LINE_LOOP coordinates for nation borders */
	const vec2_t northPole = {0.0f, 90.0f};

	assert(node);

	/* font color on geoscape */
	re.DrawColor(node->color);
	/* default font */
	font = MN_GetFont(NULL, node);

	/* draw mission pics */
	Cvar_Set("mn_mapdaytime", "");
	for (ms = ccs.mission + ccs.numMissions - 1; ms >= ccs.mission; ms--)
		if (MAP_MapToScreen(node, ms->realPos, &x, &y)) {
			re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "cross");
			if (ms == selMis) {
				re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, selMis->def->active ? "circleactive" : "circle");
				Cvar_Set("mn_mapdaytime", CL_MapIsNight(ms->realPos) ? _("Night") : _("Day"));
			}
		}

	/* draw bases pics */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--) {
		if (!base->founded || !MAP_MapToScreen(node, base->pos, &x, &y))
			continue;

		/* Draw base radar info */
		RADAR_DrawInMap(node, &(base->radar), base->pos, qfalse);

		/* Draw base */
		if (base->baseStatus == BASE_UNDER_ATTACK)
			re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "baseattack");
		else
			re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "base");

		re.FontDrawString(font, ALIGN_UL, x, y+10, node->pos[0], node->pos[1], node->size[0], node->size[1], node->size[1], base->name, 0, 0, NULL, qfalse);
		/* draw aircrafts of base */
		for (aircraft = base->aircraft + base->numAircraftInBase - 1; aircraft >= base->aircraft; aircraft--)
			if (aircraft->status > AIR_HOME && MAP_MapToScreen(node, aircraft->pos, &x, &y)) {

				/* Draw aircraft radar */
				RADAR_DrawInMap(node, &(aircraft->radar), aircraft->pos, qfalse);

				/* Draw aircraft route */
				if (aircraft->status >= AIR_TRANSIT) {
					mapline_t path;

					path.numPoints = aircraft->route.numPoints - aircraft->point;
					/* @todo : check why path.numPoints can be sometime equal to -1 */
					if (path.numPoints > 1) {
						memcpy(path.point, aircraft->pos, sizeof(vec2_t));
							memcpy(path.point + 1, aircraft->route.point + aircraft->point + 1, (path.numPoints - 1) * sizeof(vec2_t));
						MAP_MapDrawLine(node, &path);
					}
					angle = MAP_AngleOfPath(aircraft->pos, aircraft->route.point[aircraft->route.numPoints - 1], aircraft->direction, NULL, qfalse);
				} else {
					angle = MAP_AngleOfPath(aircraft->pos, northPole, aircraft->direction, NULL, qfalse);
				}

				/* Draw selected aircraft */
				if (aircraft == selectedAircraft) {
					re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "circle");

					/* Draw ufo purchased by selected aircraft */
					if (aircraft->status == AIR_UFO && MAP_MapToScreen(node, (gd.ufos + aircraft->ufo)->pos, &x, &y))
						re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "circle");
				}

				/* Draw aircraft (this must be after drawing 'selected circle' so that the aircraft looks above it)*/
				MAP_Draw3DMarkerIfVisible(node, aircraft->pos, angle, aircraft->model, qfalse);
			}
	}

	/* draws ufos */
	for (aircraft = gd.ufos + gd.numUfos - 1; aircraft >= gd.ufos; aircraft --) {
#ifdef DEBUG
		/* in debug mode you execute set showufos 1 to see the ufos on geoscape */
		if (Cvar_VariableInteger("debug_showufos")) {
			MAP_MapDrawLine(node, &(aircraft->route)); /* Draw ufo route */
		} else
#endif
		if (!aircraft->visible || !MAP_MapToScreen(node, aircraft->pos, &x, &y))
			continue;
		if (aircraft == selectedUfo)
			re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "circle");
		angle = MAP_AngleOfPath(aircraft->pos, aircraft->route.point[aircraft->route.numPoints - 1], aircraft->direction, NULL, qfalse);
		MAP_Draw3DMarkerIfVisible(node, aircraft->pos, angle, aircraft->model, qfalse);
	}

	/* draws projectiles */
	for (projectile = gd.projectiles + gd.numProjectiles - 1; projectile >= gd.projectiles; projectile --) {
		MAP_Draw3DMarkerIfVisible(node, projectile->pos, projectile->angle, "missile", qfalse);
	}

	/* FIXME */
	/* use the latitude and longitude values from nation border definition to draw a polygon */
	for (i = 0; i < gd.numNations; i++) {
		if (gd.nations[i].numBorders) {
			for (j = 0; j < gd.nations[i].numBorders; j++) {
				/* FIXME: doesn't work for scrolling the map */
				MAP_MapToScreen(node, gd.nations[i].borders[j], &x, &y);
				borders[j * 2] = x;
				borders[j * 2 + 1] = y;
			}
			re.DrawPolygon(gd.nations[i].numBorders, borders);
			re.DrawLineLoop(gd.nations[i].numBorders, borders);
		}
	}

	for (i = 0; i < gd.numNations; i++) {
		MAP_MapToScreen(node, gd.nations[i].pos, &x, &y);
		re.FontDrawString("f_verysmall", ALIGN_UC, x , y, node->pos[0], node->pos[1], node->size[0], node->size[1], node->size[1], gd.nations[i].name, 0, 0, NULL, qfalse);
	}
}

/** @brief geobackground image for 3d globe */
static menuNode_t* geobackground = NULL;

/**
 * @brief Draw the geoscape
 * @param[in] node The map menu node
 * @param[in] map3D Draw the map as flat map or as globe?
 * @sa MAP_DrawMapMarkers
 * @sa MAP_Draw3DMapMarkers
 */
extern void MAP_DrawMap (const menuNode_t* node, qboolean map3D)
{
	float q;
	base_t* base;

	/* store these values in ccs struct to be able to handle this even in the input code */
	Vector2Copy(node->pos, ccs.mapPos);
	Vector2Copy(node->size, ccs.mapSize);

	/* Draw the map and markers */
	if (map3D || cl_3dmap->value) {
		if (!geobackground)
			geobackground = MN_GetNodeFromCurrentMenu("geobackground");
		/* @todo change texh, texl of geobackground with zoomlevel */
		if (smoothRotation)
			MAP3D_SmoothRotate();
		q = (ccs.date.day % 365 + (float) (ccs.date.sec / (3600 * 6)) / 4) * 2 * M_PI / 365 - M_PI;
		re.Draw3DGlobe(node->pos[0], node->pos[1], node->size[0], node->size[1],
			(float) ccs.date.sec / (3600 * 24), q, ccs.angles, ccs.zoom / 10, curCampaign->map);

		MAP_Draw3DMapMarkers(node);
	} else {
		if (smoothRotation)
			MAP_SmoothTranslate();
		q = (ccs.date.day % 365 + (float) (ccs.date.sec / (3600 * 6)) / 4) * 2 * M_PI / 365 - M_PI;
		re.DrawDayAndNight(node->pos[0], node->pos[1], node->size[0], node->size[1], (float) ccs.date.sec / (3600 * 24), q,
			ccs.center[0], ccs.center[1], 0.5 / ccs.zoom, curCampaign->map);
		MAP_DrawMapMarkers(node);
	}

	/* display text */
	menuText[TEXT_STANDARD] = NULL;
	switch (gd.mapAction) {
	case MA_NEWBASE:
		for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
			/* Draw base radar info */
			RADAR_DrawCoverage(node, &(base->radar),base->pos, map3D);

		menuText[TEXT_STANDARD] = _("Select the desired location of the new base on the map.\n");
		return;
	case MA_BASEATTACK:
		if (selMis)
			break;
		menuText[TEXT_STANDARD] = _("Aliens are attacking our base at this very moment.\n");
		return;
	case MA_INTERCEPT:
		if (selMis)
			break;
		menuText[TEXT_STANDARD] = _("Select ufo or mission on map\n");
		return;
	case MA_UFORADAR:
		if (selMis)
			break;
		menuText[TEXT_STANDARD] = _("UFO in radar range\n");
		return;
	case MA_NONE:
		break;
	}

	/* Nothing is displayed yet */
	if (selMis) {
		if (!selMis->def)
			Sys_Error("the selected mission has no def pointer set\n");
		menuText[TEXT_STANDARD] = va(_("Location: %s\nType: %s\nObjective: %s\n"), selMis->def->location, selMis->def->type, selMis->def->text);
	} else if (selectedAircraft) {
		if (selectedAircraft->status <= AIR_HOME)
			MAP_ResetAction();
		else {
			Com_sprintf(text_standard, sizeof(text_standard), va(_("Name:\t%s (%i/%i)\n"), selectedAircraft->name, *selectedAircraft->teamSize, selectedAircraft->size));
			Q_strcat(text_standard, va(_("Status:\t%s\n"), AIR_AircraftStatusToName(selectedAircraft)), sizeof(text_standard));
			Q_strcat(text_standard, va(_("Speed:\t%.0f\n"), selectedAircraft->speed), sizeof(text_standard));
			Q_strcat(text_standard, va(_("Fuel:\t%i/%i\n"), selectedAircraft->fuel / 1000, selectedAircraft->fuelSize / 1000), sizeof(text_standard));
			menuText[TEXT_STANDARD] = text_standard;
		}
	} else if (selectedUfo)
		menuText[TEXT_STANDARD] = va("%s\n", selectedUfo->name);
}

/**
 * @brief No more special action in geoscape
 */
extern void MAP_ResetAction (void)
{
	/* don't allow a reset when no base is set up */
	if (gd.numBases)
		gd.mapAction = MA_NONE;

	/* FIXME: Don't reset the selMis when we are in tactical mission and enter the geoscape via mn_push map */
	/* @todo: I think this doesn't matter anymore, don't it? */
	gd.interceptAircraft = -1;
	if (selMis) {
		selMis->def->active = qfalse;
		selMis = NULL;				/* reset selected mission */
	}
	selectedAircraft = NULL;
	selectedUfo = NULL;
}

/**
 * @brief Select the specified aircraft in geoscape
 */
extern void MAP_SelectAircraft (aircraft_t* aircraft)
{
	MAP_ResetAction();
	selectedAircraft = aircraft;
}

/**
 * @brief Selected the specified mission
 */
extern void MAP_SelectMission (actMis_t* mission)
{
	if (! mission || mission == selMis)
		return;
	MAP_ResetAction();
	gd.mapAction = MA_INTERCEPT;
	selMis = mission;
}

/**
 * @brief Notify that a mission has been removed
 * @todo: Destroy base after removing a baseattack mission??
 */
extern void MAP_NotifyMissionRemoved (const actMis_t* mission)
{
	/* Unselect the current selected mission if its the same */
	if (selMis == mission && (gd.mapAction == MA_BASEATTACK || gd.mapAction == MA_INTERCEPT))
		MAP_ResetAction();
	else if (selMis > mission)
		selMis--;

	/* Notify the multi selection popup */
	MAP_MultiSelectNotifyMissionRemoved(mission);
}

/**
 * @brief Notify that an ufo has been removed
 */
extern void MAP_NotifyUfoRemoved (const aircraft_t* ufo)
{
	/* Unselect the current selected ufo if its the same */
	if (selectedUfo == ufo)
		MAP_ResetAction();
	else if (selectedUfo > ufo)
		selectedUfo--;

	/* Notify the multi selection popup */
	MAP_MultiSelectNotifyUfoRemoved(ufo);
}

/**
 * @brief Translate nation map color to nation
 * @sa CL_GetMapColor
 * @param[in] pos Map Coordinates to get the nation from
 * @return returns the nation pointer with the given color on nationPic at given pos
 * @return NULL if no nation with the given color value was found
 * @note The coodinates already have to be transfored to map coordinates via MAP_ScreenToMap
 */
extern nation_t* MAP_GetNation (const vec2_t pos)
{
	int i;
	nation_t* nation;
	byte* color = CL_GetMapColor(pos, MAPTYPE_NATIONS);
#ifdef PARANOID
	Com_DPrintf("MAP_GetNation: color value for %.0f:%.0f is r:%i, g:%i, b: %i\n", pos[0], pos[1], color[0], color[1], color[2]);
#endif
	for (i = 0; i < gd.numNations; i++) {
		nation = &gd.nations[i];
		/* compare the first three color values with color value at pos */
		if (VectorCompare(nation->color, color))
			return nation;
	}
	Com_Printf("MAP_GetNation: No nation found at %.0f:%.0f - color: %i:%i:%i\n", pos[0], pos[1], color[0], color[1], color[2]);
	return NULL;
}

/**
 * @brief Translate color value to zone type
 * @sa CL_GetMapColor
 * @param[in] byte the color value to the the zone type from
 * @return returns the zone name - the first char is used for base assemble themes
 * @note never may return a null pointer or an empty string
 */
extern const char* MAP_GetZoneType (byte* color)
{
	if (MapIsDesert(color))
		return "desert";
	else if (MapIsArctic(color))
		return "arctic";
	else if (MapIsWater(color))
		return "water";
	else
		return "grass";
}

/**
 * @brief Notify that an ufo disappears on radars
 */
extern void MAP_NotifyUfoDisappear (const aircraft_t* ufo)
{
	/* Unselect the current selected ufo if its the same */
	if (selectedUfo == ufo)
		MAP_ResetAction();

	/* Notify the multi selection popup */
	MAP_MultiSelectNotifyUfoDisappeared(ufo);
}

/**
 * @brief Initialise MAP/Geoscape
 */
extern void MAP_GameInit (void)
{
	cl_showCoords = Cvar_Get("cl_showcoords", "0", CVAR_ARCHIVE, NULL);
	Cmd_AddCommand("multi_select_click", MAP_MultiSelectExecuteAction_f, NULL);
}
