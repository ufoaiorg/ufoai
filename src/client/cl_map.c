/**
 * @file cl_multiselect.c
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
#include "../qcommon/ufotypes.h"

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
	MULTISELECT_TYPE_AIRCRAFT
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

/* Static functions : clic on map and multi selection */
static void MAP_MultiSelectListAddItem(multiSelectType_t item_type, int item_id,
	const char* item_description, const char* item_name);
static void MAP_MultiSelectExecuteAction_f(void);

/* Static functions : Drawing map and coordinates */
static qboolean MAP_IsMapPositionSelected(const menuNode_t* node, vec2_t pos, int x, int y);
static qboolean MAP_MapToScreen(const menuNode_t* node, const vec2_t pos, int *x, int *y);
static void MAP_ScreenToMap(const menuNode_t* node, int x, int y, vec2_t pos);
static void PolarToVec(vec2_t a, vec3_t v);
static void VecToPolar(vec3_t v, vec2_t a);
static void MAP_MapDrawLine(const menuNode_t* node, const mapline_t* line);
static void MAP_Draw3DMapMarkers(const menuNode_t* node, float latitude, float longitude);
static void MAP_DrawMapMarkers(const menuNode_t* node);

/* static variables */
static cvar_t* cl_showCoords;
static aircraft_t *selectedAircraft;	/**< Currently selected aircraft */
static char text_standard[2048];		/**< Buffer to display standard text in geoscape */
/*
==============================================================

CLIC ON MAP and MULTI SELECTION FUNCTIONS

==============================================================
*/

/**
  * @ brief Add an element in the multiselection list
  */
static void MAP_MultiSelectListAddItem(multiSelectType_t item_type, int item_id,
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
static void MAP_MultiSelectExecuteAction_f(void)
{
	int selected, id;
	aircraft_t* air;

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
			MN_PushMenu("popup_intercept");
			return;
		} else if (gd.mapAction == MA_BASEATTACK && selMis && selMis == ccs.mission + id) {
			MN_PushMenu("popup_baseattack");
			return;
		}

		MAP_ResetAction();
		selMis = ccs.mission + id;

		if (!Q_strncmp(selMis->def->name, "baseattack", 10)) {
			gd.mapAction = MA_BASEATTACK;
			/* we need no dropship in our base */
			selMis->def->active = qtrue;
		} else {
			Com_DPrintf("Select mission: %s at %.0f:%.0f\n", selMis->def->name, selMis->realPos[0], selMis->realPos[1]);
			gd.mapAction = MA_INTERCEPT;
		}
		break;

	case MULTISELECT_TYPE_AIRCRAFT: /* Selection of an aircraft */
		air = CL_AircraftGetFromIdx(id);
		if (air == NULL) {
			Com_DPrintf("MAP_MultiSelectExecuteAction: selection of an unknow aircraft idx %i\n", id);
			return;
		}

		if (air == selectedAircraft) {
			/* Selection of an already selected aircraft */
			CL_DisplayPopupAircraft(air);	/* Display popup_aircraft */
		} else {
			/* Selection of an unselected aircraft */
			MAP_ResetAction();
			selectedAircraft = air;
		}
		break;

	default:
		Com_DPrintf("MAP_MultiSelectExecuteAction: selection of an unknow element type %i\n", multiSelect.selectType[selected]);
	}
}

/*
 * @brief: Click on the 3D map/geoscape
*/
extern void MAP_3DMapClick(const menuNode_t* node, int x, int y)
{
}

#define MN_MAP_DIST_SELECTION 15

/*
 * @brief Click on the map/geoscape
 */
extern void MAP_MapClick(const menuNode_t* node, int x, int y)
{
	aircraft_t *air = NULL;
	actMis_t *ms;
	int i;
	vec2_t pos;
	char clickBuffer[30];

	/* get map position */
	MAP_ScreenToMap(node, x, y, pos);
	if (cl_showCoords->value) {
		Com_sprintf(clickBuffer, sizeof(clickBuffer), "Long: %.1f Lat: %.1f", pos[0], pos[1]);
		MN_AddNewMessage(_("Click"), clickBuffer, qfalse, MSG_DEBUG, NULL);
		Com_Printf("Clicked at %.1f %.1f\n", pos[0], pos[1]);
	}

	/* new base construction */
	if (gd.mapAction == MA_NEWBASE) {
		if (!MapIsWater(CL_GetmapColor(pos))) {
			newBasePos[0] = pos[0];
			newBasePos[1] = pos[1];
			MN_PushMenu("popup_newbase");
			return;
		} else {
			MN_AddNewMessage(_("Notice"), _("Could not set up your base at this location"), qfalse, MSG_STANDARD, NULL);
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
		if (MAP_IsMapPositionSelected(node, ms->realPos, x, y))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_MISSION, i, _(ms->def->type), _(ms->def->location));

	/* Get selected bases */
	for (i = 0; i < gd.numBases && multiSelect.nbSelect < MULTISELECT_MAXSELECT; i++) {
		if (MAP_IsMapPositionSelected(node, gd.bases[i].pos, x, y))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_BASE, i, _("Base"), gd.bases[i].name);

		/* Get selected aircrafts wich belong to the base */
		for (air = gd.bases[i].aircraft + gd.bases[i].numAircraftInBase - 1;
		air >= gd.bases[i].aircraft ; air--)
			if (air->status > AIR_HOME && air->fuel > 0 && MAP_IsMapPositionSelected(node, air->pos, x, y))
				MAP_MultiSelectListAddItem(MULTISELECT_TYPE_AIRCRAFT, air->idx, _("Aircraft"), air->name);
	}

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
			selectedAircraft->time = air->point = 0;
		}
	}
}


/*
==============================================================

GEOSCAPE DRAWING AND COORDINATES

==============================================================
*/

/**
  * @brief Tell if the specified position is considered clicked
  */
#define MN_MAP_DIST_SELECTION 15
static qboolean MAP_IsMapPositionSelected(const menuNode_t* node, vec2_t pos, int x, int y) {
	int msx, msy;

	if (MAP_MapToScreen(node, pos, &msx, &msy))
		if (x >= msx - MN_MAP_DIST_SELECTION && x <= msx + MN_MAP_DIST_SELECTION
		&& y >= msy - MN_MAP_DIST_SELECTION && y <= msy + MN_MAP_DIST_SELECTION)
			return qtrue;

	return qfalse;
}

/**
  * @brief
  */
static qboolean MAP_MapToScreen(const menuNode_t* node, const vec2_t pos, int *x, int *y)
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
  */
static void MAP_ScreenToMap(const menuNode_t* node, int x, int y, vec2_t pos)
{
	pos[0] = (((node->pos[0] - x) / node->size[0] + 0.5) / ccs.zoom - (ccs.center[0] - 0.5)) * 360.0;
	pos[1] = (((node->pos[1] - y) / node->size[1] + 0.5) / ccs.zoom - (ccs.center[1] - 0.5)) * 180.0;

	while (pos[0] > 180.0)
		pos[0] -= 360.0;
	while (pos[0] < -180.0)
		pos[0] += 360.0;
}

/**
  * @brief
  */
static void PolarToVec(vec2_t a, vec3_t v)
{
	float p, t;

	p = a[0] * M_PI / 180;
	t = a[1] * M_PI / 180;
	VectorSet(v, cos(p) * cos(t), sin(p) * cos(t), sin(t));
}

/**
  * @brief
  */
static void VecToPolar(vec3_t v, vec2_t a)
{
	a[0] = 180 / M_PI * atan2(v[1], v[0]);
	a[1] = 90 - 180 / M_PI * acos(v[2]);
}

/**
  * @brief
  */
extern void MAP_MapCalcLine(vec2_t start, vec2_t end, mapline_t* line)
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
	cosTrafo = cos(trafo[1] * M_PI / 180);
	sinTrafo = sin(trafo[1] * M_PI / 180);

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

	line->dist = fabs(phiEnd - phiStart) / n * 180 / M_PI;
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
  */
static void MAP_MapDrawLine(const menuNode_t* node, const mapline_t* line)
{
	vec4_t color;
	int pts[LINE_MAXPTS * 2];
	int *p;
	int i, start, old;

	/* set color */
	VectorSet(color, 1.0f, 0.5f, 0.5f);
	color[3] = 1.0f;
	re.DrawColor(color);

	/* draw */
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

/*
 * @brief
 */
static void MAP_Draw3DMapMarkers(const menuNode_t * node, float latitude, float longitude)
{
	aircraft_t *air;
	actMis_t *ms;
	int i, j, x, y;

	/* draw mission pics */
	Cvar_Set("mn_mapdaytime", "");
	for (i = 0; i < ccs.numMissions; i++) {
		ms = &ccs.mission[i];
		if (!MAP_MapToScreen(node, ms->realPos, &x, &y))
			continue;
		re.Draw3DMapMarkers(latitude, longitude, "cross");

		if (ms == selMis) {
			re.Draw3DMapMarkers(latitude, longitude, selMis->def->active ? "circleactive" : "circle");
/*			if ( CL_3DMapIsNight( ms->realPos ) ) Cvar_Set( "mn_mapdaytime", _("Night") );
			else Cvar_Set( "mn_mapdaytime", _("Day") ); */
		}
	}

	/* draw base pics */
	for (j = 0; j < gd.numBases; j++)
		if (gd.bases[j].founded) {
			if (!MAP_MapToScreen(node, gd.bases[j].pos, &x, &y))
				continue;
			re.Draw3DMapMarkers(latitude, longitude, "base");

			/* draw aircraft */
			for (i = 0, air = (aircraft_t *) gd.bases[j].aircraft; i < gd.bases[j].numAircraftInBase; i++, air++)
				if (air->status > AIR_HOME) {
					if (!MAP_MapToScreen(node, air->pos, &x, &y))
						continue;
					re.Draw3DMapMarkers(latitude, longitude, air->image);

					if (air->status >= AIR_TRANSIT) {
						mapline_t path;

						path.n = air->route.n - air->point;
						memcpy(path.p + 1, air->route.p + air->point + 1, (path.n - 1) * sizeof(vec2_t));
						memcpy(path.p, air->pos, sizeof(vec2_t));
						re.Draw3DMapLine(path.n, path.dist, path.p);
					}
				}
		}
}

/*
 * @brief
 */
static void MAP_DrawMapMarkers(const menuNode_t* node)
{
	aircraft_t *air, **ufos;
	actMis_t *ms;
	int i, j, x, y;

	assert(node);

	/* draw mission pics */
	Cvar_Set("mn_mapdaytime", "");
	for (i = 0; i < ccs.numMissions; i++) {
		ms = &ccs.mission[i];
		if (!MAP_MapToScreen(node, ms->realPos, &x, &y))
			continue;
		re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "cross");
		if (ms == selMis) {
			re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, selMis->def->active ? "circleactive" : "circle");
			Cvar_Set("mn_mapdaytime", CL_MapIsNight(ms->realPos) ? _("Night") : _("Day"));
		}
	}

	/* draw base pics */
	for (j = 0; j < gd.numBases; j++)
		if (gd.bases[j].founded) {
			if (!MAP_MapToScreen(node, gd.bases[j].pos, &x, &y))
				continue;

			re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "base");

			if (gd.bases[j].numSensoredAircraft > 0)
				re.DrawNormPic(x, y, i, i, 0, 0, 0, 0, ALIGN_CC, qtrue, "sensor");

			/* draw aircrafts */
			for (i = 0, air = (aircraft_t *) gd.bases[j].aircraft; i < gd.bases[j].numAircraftInBase; i++, air++)
				if (air->status > AIR_HOME) {
					if (!MAP_MapToScreen(node, air->pos, &x, &y))
						continue;
					re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, air->image);

					if (air->status >= AIR_TRANSIT) {
						mapline_t path;

						path.n = air->route.n - air->point;
						memcpy(path.p, air->pos, sizeof(vec2_t));
 						memcpy(path.p + 1, air->route.p + air->point + 1, (path.n - 1) * sizeof(vec2_t));
						MAP_MapDrawLine(node, &path);
					}

					if (air == selectedAircraft)
						re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "circle");
				}
		}

	/* draws ufos */
	for (UFO_GetUfosList(&ufos, &i) ; i > 0 ; i--, ufos++) {
		air = *ufos;

#ifdef DEBUG
		/* in debug mode you execute set showufos 1 to see the ufos on geoscape */
		if (Cvar_VariableValue("showufos")) {
			if (! MAP_MapToScreen(node, air->pos, &x, &y))
				continue;
			MAP_MapDrawLine(node, &(air->route)); /* Draw ufo route */
		} else
#endif
		if (! air->visible || ! MAP_MapToScreen(node, air->pos, &x, &y))
			continue;
		re.DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, air->image);
	}
}

/**
  * @brief Draw the geoscape
  */
extern void MAP_DrawMap(const menuNode_t* node, qboolean map3D)
{
	float q;

	/* Draw the map and markers */
	if (map3D) {
		q = (ccs.date.day % 365 + (float) (ccs.date.sec / (3600 * 6)) / 4) * 2 * M_PI / 365 - M_PI;
		re.Draw3DGlobe(node->pos[0], node->pos[1], node->size[0], node->size[1],
			(float) ccs.date.sec / (3600 * 24), q, ccs.center[0], ccs.center[1], 0.5 / ccs.zoom, curCampaign->map);

		MAP_Draw3DMapMarkers(node, 0.0, 0.0);	/* FIXME: */
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
		menuText[TEXT_STANDARD] = _("Select the desired location of the\nnew base on the map.\n");
		return;
	case MA_BASEATTACK:
		if (selMis)
			break;
		menuText[TEXT_STANDARD] = _("Aliens are attacking our base\nat this very moment.\n");
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
	}

	/* Nothing is displayed yet */
	if (selMis)
		menuText[TEXT_STANDARD] = va(_("Location: %s\nType: %s\nObjective: %s\n"), selMis->def->location, selMis->def->type, selMis->def->text);
	else if (selectedAircraft) {
		Com_sprintf(text_standard, sizeof(text_standard), va(_("Name:\t%s (%i/%i)\n"), selectedAircraft->name, *selectedAircraft->teamSize, selectedAircraft->size));
		Q_strcat(text_standard, va(_("Status:\t%s\n"), CL_AircraftStatusToName(selectedAircraft)), sizeof(text_standard));
		Q_strcat(text_standard, va(_("Speed:\t%.0f\n"), selectedAircraft->speed), sizeof(text_standard));
		Q_strcat(text_standard, va(_("Fuel:\t%i/%i\n"), selectedAircraft->fuel / 1000, selectedAircraft->fuelSize / 1000), sizeof(text_standard));
		menuText[TEXT_STANDARD] = text_standard;
	}
}

/**
  * @brief No more special action in geoscape
  */
extern void MAP_ResetAction(void)
{
	/* don't allow a reset when no base is set up */
	if (gd.numBases)
		gd.mapAction = MA_NONE;

	/* FIXME: Don't reset the selMis when we are in tactical mission and enter the geoscape via mn_push map */
	gd.interceptAircraft = -1;
	selMis = NULL;				/* reset selected mission */
	selectedAircraft = NULL;
}

/**
 * @brief Select the specified aircraft in geoscape
 */
extern void MAP_SelectAircraft(aircraft_t* aircraft) {
	MAP_ResetAction();
	selectedAircraft = aircraft;
}

/**
 * @brief Selected the specified mission
 */
extern void MAP_SelectMission(actMis_t* mission) {
	if (! mission || mission == selMis)
		return;
	MAP_ResetAction();
	gd.mapAction = MA_INTERCEPT;
	selMis = mission;
}

/**
 * @brief Notify that a mission has been removed
 * Unselect the current selected mission if its the same
 */
extern void MAP_NotifyMissionRemoved(const actMis_t* mission) {
	if (selMis == mission && (gd.mapAction == MA_BASEATTACK || gd.mapAction == MA_INTERCEPT))
		MAP_ResetAction();
	else if (selMis > mission)
		selMis--;
}

/**
  * @brief
  */
extern void MAP_Reset(void)
{
	MAP_ResetAction();

	cl_showCoords = Cvar_Get("cl_showcoords", "0", CVAR_ARCHIVE);

	Cmd_AddCommand("multi_select_click", MAP_MultiSelectExecuteAction_f);
	Cmd_AddCommand("popup_aircraft_action_click", CL_PopupAircraftClick_f);
}
