/**
 * @file cl_map.c
 * @brief Geoscape/Map management
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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
		aircraft = CL_AircraftGetFromIdx(id);
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

	for (i = 0 ; i < multiSelect.nbSelect ; i++)
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
	/* TODO : Determine THE corresponding ufo in the multi select list */
	for (i = 0 ; i < multiSelect.nbSelect ; i++)
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
		/* TODO: Select aircraft - follow ufo - fight */
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
		aircraft >= gd.bases[i].aircraft ; aircraft--)
			if (aircraft->status > AIR_HOME && aircraft->fuel > 0 && MAP_IsMapPositionSelected(node, aircraft->pos, x, y, globe))
				MAP_MultiSelectListAddItem(MULTISELECT_TYPE_AIRCRAFT, aircraft->idx, _("Aircraft"), aircraft->name);
	}

	/* Get selected ufos */
	for (aircraft = gd.ufos + gd.numUfos - 1 ; aircraft >= gd.ufos ; aircraft--)
		if (aircraft->visible
#if DEBUG
		|| Cvar_VariableInteger("showufos")
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
		else if (selectedAircraft->status > AIR_HOME && selectedAircraft->fuel > 0) {
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
 * @brief
 * @param[out] x normalized (rotated and scaled) x value of mouseclick
 * @param[out] y normalized (rotated and scaled) y value of mouseclick
 * @param[out] z z value of the given latitude and longitude - might also be NULL if not needed
 * @param[in] pos vector that holds longitude and latitude
 * @sa MAP_MapToScreen
 */
extern qboolean MAP_3DMapToScreen (const menuNode_t* node, const vec2_t pos, int *x, int *y, int *z)
{
	vec2_t tmp;
	const float radius = GLOBE_RADIUS;

	Vector2Set(tmp, (node->pos[0] + node->size[0]) / 2.0f, (node->pos[1] + node->size[1]) / 2.0f);

	/* pos[0] = longitude (theta) */
	/* pos[1] = latitude (phi) */
	*x = (radius * cos(pos[0]) * cos(pos[1]+ccs.angles[1])) + tmp[0];
	*y = (radius * cos(pos[0]) * sin(pos[1]+ccs.angles[1])) + tmp[1];
	if (z) {
		*z = 0; /* FIXME */
	}
/*	Com_Printf("MAP_3DMapToScreen: %i:%i\n", *x, *y);*/

	/* TODO: Check ccs.angle */
/*	if (*x < node->pos[0] && *y < node->pos[1] && *x > node->pos[0] + node->size[0] && *y > node->pos[1] + node->size[1])
		return qfalse;*/
	return qtrue;
}

/**
 * @brief
 * @param[out] x normalized (shiften and scaled) x value of mouseclick
 * @param[out] y normalized (shiften and scaled) y value of mouseclick
 * @param[in] pos vector that holds longitude and latitude
 * @sa MAP_3DMapToScreen
 */
extern qboolean MAP_MapToScreen (const menuNode_t* node, const vec2_t pos, int *x, int *y)
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
	*y = node->pos[1] + 0.5 * node->size[1] - (pos[1] / 180 + ccs.center[1] - 0.5) * node->size[1] * ccs.zoom;

	if (*x < node->pos[0] && *y < node->pos[1] && *x > node->pos[0] + node->size[0] && *y > node->pos[1] + node->size[1])
		return qfalse;
	return qtrue;
}

/**
 * @brief
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
 * @brief Return longitude and latitude for the 3d globe
 * FIXME
 * @todo implement me
 * x = cos(alpha)
 * y = sin(alpha)
 * long -180° (w) - 180° (e)
 * lat -90 (n) - 90° (s)
 * @return pos vec2_t was filled with longitude and latitude
 * @param[in] x X coordinate on the screen that was clicked to
 * @param[in] y Y coordinate on the screen that was clicked to
 * @param[in] node The current menuNode we was clicking into (3dmap or map)
 */
static void MAP3D_ScreenToMap (const menuNode_t* node, int x, int y, vec2_t pos)
{
	vec2_t mid;
	float dist;
	float theta;
	float phi;
	const float radius = GLOBE_RADIUS;

	Vector2Set(mid, (node->pos[0] + node->size[0]) / 2.0f, (node->pos[1] + node->size[1]) / 2.0f);

	dist = sqrt((abs(x - mid[0]) * abs(x - mid[0])) + ((abs(y - mid[1]) * abs(y - mid[1]))));

	/* check whether we clicked the geoscape */
	if (dist <= radius) {
		/* z-coordinates (y on screen) */
		theta = asin((float)(y - mid[1]) / dist);
		/* x-coordinates */
		phi = acos((float)(x - mid[0]) / dist / cos(theta));
		pos[0] = theta * todeg;	/* longitude */
		pos[1] = 90.0f - (phi * todeg); /* latitude */
		/* FIXME */
/*		pos[0] += ccs.angles[PITCH];
		pos[1] += (ccs.angles[YAW] - GLOBE_ROTATE);*/
#if 1
		Com_Printf("long: %.1f, lat: %.1f\n", pos[0], pos[1]);
		Com_Printf("rotate: %.1f: %.1f\n", ccs.angles[PITCH], ccs.angles[YAW] - GLOBE_ROTATE);
#endif
	} else
		Vector2Set(pos, -1.0, -1.0);
}

/**
 * @brief
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

/*	Com_Printf( "#(%3.1f %3.1f) -> (%3.1f %3.1f)\n", start[0], start[1], end[0], end[1] ); */

	line->dist = fabs(phiEnd - phiStart) / n * todeg;
	line->n = n + 1;
	dPhi = (phiEnd - phiStart) / n;
	p = NULL;
	for (phi = phiStart, i = 0; i <= n; phi += dPhi, i++) {
		last = p;
		p = line->p[i];
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
 * @brief
 * @sa MAP_MapCalcLine
 */
static void MAP_MapDrawLine (const menuNode_t* node, const mapline_t* line)
{
	vec4_t color = {1, 0.5, 0.5, 1};
	int pts[LINE_MAXPTS * 2];
	int *p;
	int i, start, old;

	/* draw */
	re.DrawColor(color);
	start = 0;
	old = 512;
	for (i = 0, p = pts; i < line->n; i++, p += 2) {
		MAP_MapToScreen(node, line->p[i], p, p + 1);

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
 * @brief
 */
static void MAP_Draw3DMapMarkers (const menuNode_t * node)
{
	aircraft_t *aircraft;
	actMis_t *ms;
	int i, j;
	int borders[MAX_NATION_BORDERS * 2];	/**< GL_LINE_LOOP coordinates for nation borders */
	int x, y, z;
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
/*		if (!MAP_3DMapToScreen(node, ms->realPos, &x, &y, &z))
			continue; */
		re.Draw3DMapMarkers(ccs.angles, ccs.zoom, ms->def->pos[0], ms->def->pos[1], "cross");

		if (ms == selMis) {
		/*	re.Draw3DMapMarkers(ccs.angles, ccs.zoom, latitude, longitude, selMis->def->active ? "circleactive" : "circle");*/
			Cvar_Set("mn_mapdaytime", CL_MapIsNight(ms->realPos) ? _("Night") : _("Day"));
		}
	}

	/* draw base pics */
	for (j = 0; j < gd.numBases; j++)
		if (gd.bases[j].founded) {
			re.Draw3DMapMarkers(ccs.angles, ccs.zoom, gd.bases[j].pos[0], gd.bases[j].pos[1], "base");

			/* draw aircraft */
			for (i = 0, aircraft = (aircraft_t *) gd.bases[j].aircraft; i < gd.bases[j].numAircraftInBase; i++, aircraft++)
				if (aircraft->status > AIR_HOME) {
					re.Draw3DMapMarkers(ccs.angles, ccs.zoom, aircraft->pos[0], aircraft->pos[0], aircraft->image);

					if (aircraft->status >= AIR_TRANSIT) {
						mapline_t path;

						path.n = aircraft->route.n - aircraft->point;
						memcpy(path.p + 1, aircraft->route.p + aircraft->point + 1, (path.n - 1) * sizeof(vec2_t));
						memcpy(path.p, aircraft->pos, sizeof(vec2_t));
						re.Draw3DMapLine(ccs.angles, ccs.zoom, path.n, path.dist, path.p);
					}
				}
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
/*					borders[j * 2 + 2] = z;*/
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
	int x, y, i, j;
	base_t* base;
	const char* font = NULL;
	int borders[MAX_NATION_BORDERS * 2];	/**< GL_LINE_LOOP coordinates for nation borders */

	assert(node);

	/* font color on geoscape */
	re.DrawColor(node->color);
	/* default font */
	font = MN_GetFont(NULL, node);

	/* draw mission pics */
	Cvar_Set("mn_mapdaytime", "");
	for (ms = ccs.mission + ccs.numMissions - 1 ; ms >= ccs.mission ; ms--)
		if (MAP_MapToScreen(node, ms->realPos, &x, &y)) {
			re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "cross");
			if (ms == selMis) {
				re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, selMis->def->active ? "circleactive" : "circle");
				Cvar_Set("mn_mapdaytime", CL_MapIsNight(ms->realPos) ? _("Night") : _("Day"));
			}
		}

	/* draw bases pics */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases ; base--) {
		if (! base->founded || ! MAP_MapToScreen(node, base->pos, &x, &y))
			continue;

		/* Draw base radar info */
		RADAR_DrawInMap(node, &(base->radar), x, y, base->pos, qfalse);

		/* Draw base */
		if (base->baseStatus == BASE_UNDER_ATTACK)
			re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "baseattack");
		else
			re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "base");

		re.FontDrawString(font, ALIGN_UL, x, y+10, node->pos[0], node->pos[1], node->size[0], node->size[1], node->size[1], base->name, 0, 0, NULL, qfalse);
		/* draw aircrafts of base */
		for (aircraft = base->aircraft + base->numAircraftInBase - 1; aircraft >= base->aircraft ; aircraft--)
			if (aircraft->status > AIR_HOME && MAP_MapToScreen(node, aircraft->pos, &x, &y)) {

				/* Draw aircraft radar */
				RADAR_DrawInMap(node, &(aircraft->radar), x, y, aircraft->pos, qfalse);

				/* Draw aircraft */
				re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, aircraft->image);

				/* Draw aircraft route */
				if (aircraft->status >= AIR_TRANSIT) {
					mapline_t path;

					path.n = aircraft->route.n - aircraft->point;
					/* TODO : check why path.n can be sometime equal to -1 */
					if (path.n > 1) {
						memcpy(path.p, aircraft->pos, sizeof(vec2_t));
							memcpy(path.p + 1, aircraft->route.p + aircraft->point + 1, (path.n - 1) * sizeof(vec2_t));
						MAP_MapDrawLine(node, &path);
					}
				}

				/* Draw selected aircraft */
				if (aircraft == selectedAircraft) {
					re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "circle");

					/* Draw ufo purchased by selected aircraft */
					if (aircraft->status == AIR_UFO && MAP_MapToScreen(node, (gd.ufos + aircraft->ufo)->pos, &x, &y))
						re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "circle");
				}
			}
	}

	/* draws ufos */
	for (aircraft = gd.ufos + gd.numUfos - 1 ; aircraft >= gd.ufos ; aircraft --) {
#ifdef DEBUG
		/* in debug mode you execute set showufos 1 to see the ufos on geoscape */
		if (Cvar_VariableInteger("showufos")) {
			if (!MAP_MapToScreen(node, aircraft->pos, &x, &y))
				continue;
			MAP_MapDrawLine(node, &(aircraft->route)); /* Draw ufo route */
		} else
#endif
		if (!aircraft->visible || !MAP_MapToScreen(node, aircraft->pos, &x, &y))
			continue;
		re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, aircraft->image);
		if (aircraft == selectedUfo)
			re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "circle");
	}

	/* FIXME */
	/* use the latitude and longitude values from nation border definition to draw a polygon */
	for (i = 0; i < gd.numNations; i++) {
		/* font color for nations */
		re.DrawColor(gd.nations[i].color);
		if (gd.nations[i].numBorders) {
			for (j = 0; j < gd.nations[i].numBorders; j++) {
				/* FIXME: doesn't work for scrolling the map */
				MAP_MapToScreen(node, gd.nations[i].borders[j], &x, &y);
				borders[j * 2] = x;
				borders[j * 2 + 1] = y;
			}
			re.DrawPolygon(gd.nations[i].numBorders, borders);
			re.DrawColor(NULL);
			re.DrawLineLoop(gd.nations[i].numBorders, borders);
		}
	}

	re.DrawColor(NULL);

	for (i = 0; i < gd.numNations; i++) {
		/* font color for nations */
		re.DrawColor(gd.nations[i].color);
		MAP_MapToScreen(node, gd.nations[i].pos, &x, &y);
		re.FontDrawString("f_verysmall", ALIGN_UC, x , y, node->pos[0], node->pos[1], node->size[0], node->size[1], node->size[1], gd.nations[i].name, 0, 0, NULL, qfalse);
	}

	/* reset color */
	re.DrawColor(NULL);
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
		/* TODO change texh, texl of geobackground with zoomlevel */
		q = (ccs.date.day % 365 + (float) (ccs.date.sec / (3600 * 6)) / 4) * 2 * M_PI / 365 - M_PI;
		re.Draw3DGlobe(node->pos[0], node->pos[1], node->size[0], node->size[1],
			(float) ccs.date.sec / (3600 * 24), q, ccs.angles, ccs.zoom / 10, curCampaign->map);

		MAP_Draw3DMapMarkers(node);
	} else {
		q = (ccs.date.day % 365 + (float) (ccs.date.sec / (3600 * 6)) / 4) * 2 * M_PI / 365 - M_PI;
		re.DrawDayAndNight(node->pos[0], node->pos[1], node->size[0], node->size[1], (float) ccs.date.sec / (3600 * 24), q,
			ccs.center[0], ccs.center[1], 0.5 / ccs.zoom, curCampaign->map);
		MAP_DrawMapMarkers(node);
	}

	/* display text */
	menuText[TEXT_STANDARD] = NULL;
	switch (gd.mapAction) {
	case MA_NEWBASE:
		for (base = gd.bases + gd.numBases - 1; base >= gd.bases ; base--)
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
			Q_strcat(text_standard, va(_("Status:\t%s\n"), CL_AircraftStatusToName(selectedAircraft)), sizeof(text_standard));
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
	/* TODO: I think this doesn't matter anymore, don't it? */
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
 * TODO: Destroy base after removing a baseattack mission??
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
	Com_DPrintf("MAP_GetNation: No nation found at %.0f:%.0f\n", pos[0], pos[1]);
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
		return "gras";
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
