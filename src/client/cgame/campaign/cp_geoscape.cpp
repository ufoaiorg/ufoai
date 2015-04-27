/**
 * @file
 * @brief Geoscape/Map management
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

#include "../../cl_shared.h"
#include "../../ui/ui_dataids.h"
#include "../../ui/node/ui_node_geoscape.h"
#include "cp_overlay.h"
#include "cp_campaign.h"
#include "cp_geoscape.h"
#include "cp_popup.h"
#include "cp_mapfightequip.h"
#include "cp_missions.h"
#include "cp_ufo.h"
#include "cp_time.h"
#include "cp_xvi.h"

static uiNode_t* geoscapeNode;

#ifdef DEBUG
static cvar_t* debug_showInterest;
#endif

#define GLOBE_ROTATE -90
#define ZOOM_LIMIT	2.5f

/*
==============================================================
MULTI SELECTION DEFINITION
==============================================================
*/

#define MULTISELECT_MAXSELECT 6	/**< Maximal count of elements that can be selected at once */

/**
 * @brief Types of elements that can be selected on the map
 */
typedef enum {
	MULTISELECT_TYPE_BASE,
	MULTISELECT_TYPE_INSTALLATION,
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
static bool GEO_IsPositionSelected(const uiNode_t* node, const vec2_t pos, int x, int y);

/* static variables */
static char textStandard[2048];		/**< Buffer to display standard text on the geoscape */
static int centerOnEventIdx;		/**< Current Event centered on 3D geoscape */

/* Colors */
static const vec4_t green = {0.0f, 1.0f, 0.0f, 0.8f};
static const vec4_t yellow = {1.0f, 0.874f, 0.294f, 1.0f};
static const vec4_t red = {1.0f, 0.0f, 0.0f, 0.8f};

static const float defaultBaseAngle = -90.0f;	/**< Default angle value for 3D models like bases */

static byte* terrainPic;				/**< this is the terrain mask for separating the climate
										 * zone and water by different color values */
static int terrainWidth, terrainHeight;		/**< the width and height for the terrain pic. */

static byte* culturePic;				/**< this is the mask for separating the culture
										 * zone and water by different color values */
static int cultureWidth, cultureHeight;		/**< the width and height for the culture pic. */

static byte* populationPic;				/**< this is the mask for separating the population rate
										 * zone and water by different color values */
static int populationWidth, populationHeight;		/**< the width and height for the population pic. */

static byte* nationsPic;				/**< this is the nation mask - separated
										 * by colors given in nations.ufo. */
static int nationsWidth, nationsHeight;	/**< the width and height for the nation pic. */


/*
==============================================================
CLICK ON MAP and MULTI SELECTION FUNCTIONS
==============================================================
*/

/**
 * @brief Add an element in the multiselection list
 */
static void GEO_MultiSelectListAddItem (multiSelectType_t itemType, int itemID,
	const char* itemDescription, const char* itemName)
{
	Q_strcat(multiSelect.popupText, sizeof(multiSelect.popupText), "%s\t%s\n", itemDescription, itemName);
	multiSelect.selectType[multiSelect.nbSelect] = itemType;
	multiSelect.selectId[multiSelect.nbSelect] = itemID;
	multiSelect.nbSelect++;
}

/**
 * @brief Execute action for 1 element of the multi selection
 * Param cgi->Cmd_Argv(1) is the element selected in the popup_multi_selection menu
 */
static void GEO_MultiSelectExecuteAction_f (void)
{
	int selected, id;
	aircraft_t* aircraft;
	bool multiSelection = false;

	if (cgi->Cmd_Argc() < 2) {
		/* Direct call from code, not from a popup menu */
		selected = 0;
	} else {
		/* Call from a geoscape popup menu (popup_multi_selection) */
		cgi->UI_PopWindow(false);
		selected = atoi(cgi->Cmd_Argv(1));
		multiSelection = true;
	}

	if (selected < 0 || selected >= multiSelect.nbSelect)
		return;
	id = multiSelect.selectId[selected];

	/* Execute action on element */
	switch (multiSelect.selectType[selected]) {
	case MULTISELECT_TYPE_BASE:	/* Select a base */
		if (id >= B_GetCount())
			break;
		GEO_ResetAction();
		B_SelectBase(B_GetFoundedBaseByIDX(id));
		break;
	case MULTISELECT_TYPE_INSTALLATION: {
		/* Select an installation */
		installation_t* ins = INS_GetByIDX(id);
		if (ins) {
			GEO_ResetAction();
			INS_SelectInstallation(ins);
		}
		break;
	}
	case MULTISELECT_TYPE_MISSION: {
		mission_t* mission = GEO_GetSelectedMission();
		/* Select a mission */
		if (ccs.mapAction == MA_INTERCEPT && mission && mission == MIS_GetByIdx(id)) {
			CL_DisplayPopupInterceptMission(mission);
			return;
		}
		mission = GEO_SelectMission(MIS_GetByIdx(id));
		if (multiSelection) {
			/* if we come from a multiSelection menu, there is no need to open this popup twice to go to a mission */
			CL_DisplayPopupInterceptMission(mission);
			return;
		}
		break;
	}
	case MULTISELECT_TYPE_AIRCRAFT: /* Selection of an aircraft */
		aircraft = AIR_AircraftGetFromIDX(id);
		if (aircraft == nullptr) {
			Com_DPrintf(DEBUG_CLIENT, "GEO_MultiSelectExecuteAction: selection of an unknown aircraft idx %i\n", id);
			return;
		}

		if (GEO_IsAircraftSelected(aircraft)) {
			/* Selection of an already selected aircraft */
			CL_DisplayPopupAircraft(aircraft);	/* Display popup_aircraft */
		} else {
			/* Selection of an unselected aircraft */
			GEO_SelectAircraft(aircraft);
			if (multiSelection)
				/* if we come from a multiSelection menu, there is no need to open this popup twice to choose an action */
				CL_DisplayPopupAircraft(aircraft);
		}
		break;
	case MULTISELECT_TYPE_UFO : /* Selection of a UFO */
		/* Get the ufo selected */
		if (id < 0 || id >= ccs.numUFOs)
			return;
		aircraft = UFO_GetByIDX(id);

		if (GEO_IsUFOSelected(aircraft)) {
			/* Selection of a already selected ufo */
			CL_DisplayPopupInterceptUFO(aircraft);
		} else {
			/* Selection of a unselected ufo */
			GEO_SelectUFO(aircraft);
			if (multiSelection)
				/* if we come from a multiSelection menu, there is no need to open this popup twice to choose an action */
				CL_DisplayPopupInterceptUFO(GEO_GetSelectedUFO());
		}
		break;
	case MULTISELECT_TYPE_NONE :	/* Selection of an element that has been removed */
		break;
	default:
		Com_DPrintf(DEBUG_CLIENT, "GEO_MultiSelectExecuteAction: selection of an unknown element type %i\n",
				multiSelect.selectType[selected]);
		break;
	}
}

bool GEO_IsRadarOverlayActivated (void)
{
	return cgi->Cvar_GetInteger("geo_overlay_radar");
}

static inline bool GEO_IsNationOverlayActivated (void)
{
	return cgi->Cvar_GetInteger("geo_overlay_nation");
}

static inline bool GEO_IsXVIOverlayActivated (void)
{
	return cgi->Cvar_GetInteger("geo_overlay_xvi");
}

/**
 * @brief Click on the map/geoscape
 * @param[in] node UI Node of the geoscape map
 * @param[in] x,y Mouse click coordinates
 * @param[in] pos Geoscape (longitude, latitude) coordinate of the click
 * @return True if the event is used for something
 */
bool GEO_Click (const uiNode_t* node, int x, int y, const vec2_t pos)
{
	aircraft_t* ufo;
	base_t* base;

	switch (ccs.mapAction) {
	case MA_NEWBASE:
		/* new base construction */
		/** @todo make this a function in cp_base.c - B_BuildBaseAtPos */
		if (!MapIsWater(GEO_GetColor(pos, MAPTYPE_TERRAIN, nullptr))) {
			if (B_GetCount() < MAX_BASES) {
				Vector2Copy(pos, ccs.newBasePos);
				CP_GameTimeStop();
				cgi->Cmd_ExecuteString("mn_set_base_title");
				cgi->UI_PushWindow("popup_newbase");
				return true;
			}
			return false;
		}
		break;
	case MA_NEWINSTALLATION:
		if (!MapIsWater(GEO_GetColor(pos, MAPTYPE_TERRAIN, nullptr))) {
			Vector2Copy(pos, ccs.newBasePos);
			CP_GameTimeStop();
			cgi->UI_PushWindow("popup_newinstallation");
			return true;
		}
		break;
	case MA_UFORADAR:
		cgi->UI_PushWindow("popup_intercept_ufo");
		break;
	default:
		break;
	}

	/* Init data for multi selection */
	multiSelect.nbSelect = 0;
	OBJZERO(multiSelect.popupText);
	/* Get selected missions */
	MIS_Foreach(tempMission) {
		if (multiSelect.nbSelect >= MULTISELECT_MAXSELECT)
			break;
		if (tempMission->stage == STAGE_NOT_ACTIVE || !tempMission->onGeoscape)
			continue;
		if (tempMission->pos && GEO_IsPositionSelected(node, tempMission->pos, x, y))
			GEO_MultiSelectListAddItem(MULTISELECT_TYPE_MISSION, MIS_GetIdx(tempMission), _("Mission"), MIS_GetName(tempMission));
	}

	/* Get selected aircraft which belong */
	AIR_Foreach(aircraft) {
		if (AIR_IsAircraftOnGeoscape(aircraft) && aircraft->fuel > 0 && GEO_IsPositionSelected(node, aircraft->pos, x, y))
			GEO_MultiSelectListAddItem(MULTISELECT_TYPE_AIRCRAFT, aircraft->idx, _("Aircraft"), aircraft->name);
	}

	/* Get selected bases */
	base = nullptr;
	while ((base = B_GetNext(base)) != nullptr && multiSelect.nbSelect < MULTISELECT_MAXSELECT) {
		if (GEO_IsPositionSelected(node, base->pos, x, y))
			GEO_MultiSelectListAddItem(MULTISELECT_TYPE_BASE, base->idx, _("Base"), base->name);
	}

	/* Get selected installations */
	INS_Foreach(installation) {
		if (multiSelect.nbSelect >= MULTISELECT_MAXSELECT)
			break;
		if (GEO_IsPositionSelected(node, installation->pos, x, y))
			GEO_MultiSelectListAddItem(MULTISELECT_TYPE_INSTALLATION, installation->idx, _("Installation"), installation->name);
	}

	/* Get selected ufos */
	ufo = nullptr;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != nullptr)
		if (AIR_IsAircraftOnGeoscape(ufo) && GEO_IsPositionSelected(node, ufo->pos, x, y))
			GEO_MultiSelectListAddItem(MULTISELECT_TYPE_UFO, UFO_GetGeoscapeIDX(ufo), _("UFO Sighting"), UFO_GetName(ufo));

	if (multiSelect.nbSelect == 1) {
		/* Execute directly action for the only one element selected */
		cgi->Cmd_ExecuteString("multi_select_click");
		return true;
	} else if (multiSelect.nbSelect > 1) {
		/* Display popup for multi selection */
		cgi->UI_RegisterText(TEXT_MULTISELECTION, multiSelect.popupText);
		CP_GameTimeStop();
		cgi->UI_PushWindow("popup_multi_selection");
		return true;
	} else {
		aircraft_t* aircraft = GEO_GetSelectedAircraft();
		/* Nothing selected */
		if (!aircraft) {
			GEO_ResetAction();
			return false;
		}

		if (AIR_IsAircraftOnGeoscape(aircraft) && AIR_AircraftHasEnoughFuel(aircraft, pos)) {
			/* Move the selected aircraft to the position clicked */
			GEO_CalcLine(aircraft->pos, pos, &aircraft->route);
			aircraft->status = AIR_TRANSIT;
			aircraft->aircraftTarget = nullptr;
			aircraft->time = 0;
			aircraft->point = 0;
			return true;
		}
	}
	return false;
}


/*
==============================================================
GEOSCAPE DRAWING AND COORDINATES
==============================================================
*/

/**
 * @brief Transform a 2D position on the map to screen coordinates.
 * @param[in] node Menu node
 * @param[in] pos vector that holds longitude and latitude
 * @param[out] x normalized (rotated and scaled) x value of mouseclick
 * @param[out] y normalized (rotated and scaled) y value of mouseclick
 * @param[out] z z value of the given latitude and longitude - might also be nullptr if not needed
 * @return true if the point is visible, false else (if it's outside the node or on the wrong side of the earth).
 */
static bool GEO_3DMapToScreen (const uiNode_t* node, const vec2_t pos, int* x, int* y, int* z)
{
	vec2_t mid;
	vec3_t v, v1, rotationAxis;
	const float radius = GLOBE_RADIUS;

	PolarToVec(pos, v);

	/* rotate the vector to switch of reference frame.
	 * We switch from the static frame of the earth to the local frame of the player */
	VectorSet(rotationAxis, 0, 0, 1);
	const mapExtraData_t& data = UI_MAPEXTRADATACONST(node);
	RotatePointAroundVector(v1, rotationAxis, v, - data.angles[PITCH]);

	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v, rotationAxis, v1, - data.angles[YAW]);

	/* set mid to the coordinates of the center of the globe */
	Vector2Set(mid, data.mapPos[0] + data.mapSize[0] / 2.0f, data.mapPos[1] + data.mapSize[1] / 2.0f);

	/* We now convert those coordinates relative to the center of the globe to coordinates of the screen
	 * (which are relative to the upper left side of the screen) */
	*x = (int) (mid[0] - radius * v[1]);
	*y = (int) (mid[1] - radius * v[0]);

	if (z)
		*z = (int) (radius * v[2]);

	/* if the point is on the wrong side of the earth, the player cannot see it */
	if (v[2] > 0)
		return false;

	/* if the point is outside the screen, the player cannot see it */
	if (*x < data.mapPos[0] && *y < data.mapPos[1]
			&& *x > data.mapPos[0] + data.mapSize[0]
			&& *y > data.mapPos[1] + data.mapSize[1])
		return false;

	return true;
}

/**
 * @brief Transform a 2D position on the map to screen coordinates.
 * @param[in] node Menu node
 * @param[in] pos Position on the map described by longitude and latitude
 * @param[out] x X coordinate on the screen
 * @param[out] y Y coordinate on the screen
 * @returns true if the screen position is within the boundaries of the menu
 * node. Otherwise returns false.
 */
static bool GEO_MapToScreen (const uiNode_t* node, const vec2_t pos, int* x, int* y)
{
	const mapExtraData_t& data = UI_MAPEXTRADATACONST(node);
	/* get "raw" position */
	float sx = pos[0] / 360 + data.center[0] - 0.5;

	/* shift it on screen */
	if (sx < -0.5f)
		sx += 1.0f;
	else if (sx > +0.5f)
		sx -= 1.0f;

	*x = data.mapPos[0] + 0.5f * data.mapSize[0] - sx * data.mapSize[0] * data.zoom;
	*y = data.mapPos[1] + 0.5f * data.mapSize[1] - (pos[1] / 180.0f + data.center[1] - 0.5f) * data.mapSize[1] * data.zoom;

	if (*x < data.mapPos[0] && *y < data.mapPos[1]
			&& *x > data.mapPos[0] + data.mapSize[0]
			&& *y > data.mapPos[1] + data.mapSize[1])
		return false;
	return true;
}

/**
 * @brief Call either GEO_MapToScreen or GEO_3DMapToScreen depending on the geoscape you're using.
 * @param[in] node Menu node
 * @param[in] pos Position on the map described by longitude and latitude
 * @param[out] x Pointer to the X coordinate on the screen
 * @param[out] y Pointer to the Y coordinate on the screen
 * @param[out] z Pointer to the Z coordinate on the screen (may be nullptr if not needed)
 * @returns true if pos corresponds to a point which is visible on the screen. Otherwise returns false.
 */
static bool GEO_AllMapToScreen (const uiNode_t* node, const vec2_t pos, int* x, int* y, int* z)
{
	const mapExtraData_t& data = UI_MAPEXTRADATACONST(node);
	if (!data.flatgeoscape)
		return GEO_3DMapToScreen(node, pos, x, y, z);

	if (z)
		*z = -10;
	return GEO_MapToScreen(node, pos, x, y);
}

/**
 * @brief maximum distance (in pixel) to get a valid mouse click
 * @note this is for a 1024 * 768 screen
 */
#define UI_MAP_DIST_SELECTION 15
/**
 * @brief Tell if the specified position is considered clicked
 */
static bool GEO_IsPositionSelected (const uiNode_t* node, const vec2_t pos, int x, int y)
{
	int msx, msy;

	if (GEO_AllMapToScreen(node, pos, &msx, &msy, nullptr))
		if (x >= msx - UI_MAP_DIST_SELECTION && x <= msx + UI_MAP_DIST_SELECTION
		 && y >= msy - UI_MAP_DIST_SELECTION && y <= msy + UI_MAP_DIST_SELECTION)
			return true;

	return false;
}

/**
 * @brief Draws a 3D marker on geoscape if the player can see it.
 * @param[in] node Menu node.
 * @param[in] pos Longitude and latitude of the marker to draw.
 * @param[in] theta Angle (degree) of the model to the horizontal.
 * @param[in] model The name of the model of the marker.
 * @param[in] skin Number of modelskin to draw on marker
 */
static void GEO_Draw3DMarkerIfVisible (const uiNode_t* node, const vec2_t pos, float theta, const char* model, int skin)
{
	const mapExtraData_t& data = UI_MAPEXTRADATACONST(node);
	if (data.flatgeoscape) {
		int x, y;
		vec3_t screenPos;

		GEO_AllMapToScreen(node, pos, &x, &y, nullptr);
		VectorSet(screenPos, x, y, 0);
		/* models are used on 2D geoscape for aircraft */
		cgi->R_Draw2DMapMarkers(screenPos, theta, model, skin);
	} else {
		cgi->R_Draw3DMapMarkers(data.mapPos, data.mapSize, data.angles, pos, theta, GLOBE_RADIUS, model, skin);
	}
}

/**
 * @brief Calculate the shortest way to go from start to end on a sphere
 * @param[in] start The point you start from
 * @param[in] end The point you go to
 * @param[out] line Contains the shortest path to go from start to end
 * @sa GEO_MapDrawLine
 */
void GEO_CalcLine (const vec2_t start, const vec2_t end, mapline_t* line)
{
	vec3_t s, e, v;
	vec3_t normal;
	vec2_t trafo, sa, ea;
	float cosTrafo, sinTrafo;
	float phiStart, phiEnd, dPhi, phi;
	float* p;
	int i, n;

	/* get plane normal */
	PolarToVec(start, s);
	PolarToVec(end, e);
	/* Procedure below won't work if start is the same like end */
	if (VectorEqual(s, e)) {
		line->distance = 0;
		line->numPoints = 2;
		Vector2Set(line->point[0], end[0], end[1]);
		Vector2Set(line->point[1], end[0], end[1]);
		return;
	}

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

	line->distance = fabs(phiEnd - phiStart) / n * todeg;
	line->numPoints = n + 1;
	/* make sure we do not exceed route array size */
	assert(line->numPoints <= LINE_MAXPTS);
	dPhi = (phiEnd - phiStart) / n;
	p = nullptr;
	for (phi = phiStart, i = 0; i <= n; phi += dPhi, i++) {
		const float* last = p;
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
 * @brief Draw a path on a menu node (usually the 2D geoscape map)
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the geoscape menu node.
 * @param[in] line The path which is to be drawn
 * @sa GEO_CalcLine
 */
static void GEO_MapDrawLine (const uiNode_t* node, const mapline_t* line)
{
	const vec4_t color = {1, 0.5, 0.5, 1};
	screenPoint_t pts[LINE_MAXPTS];
	screenPoint_t* p;
	int i, start, old;

	/* draw */
	cgi->R_Color(color);
	start = 0;
	const mapExtraData_t& data = UI_MAPEXTRADATACONST(node);
	old = data.mapSize[0] / 2;
	for (i = 0, p = pts; i < line->numPoints; i++, p++) {
		GEO_MapToScreen(node, line->point[i], &p->x, &p->y);

		/* If we cross longitude 180 degree (right/left edge of the screen), draw the first part of the path */
		if (i > start && abs(p->x - old) > data.mapSize[0] / 2) {
			/* shift last point */
			int diff;

			if (p->x - old > data.mapSize[0] / 2)
				diff = -data.mapSize[0] * data.zoom;
			else
				diff = data.mapSize[0] * data.zoom;
			p->x += diff;

			/* wrap around screen border */
			cgi->R_DrawLineStrip(i - start, (int*)(&pts));

			/* first path of the path is drawn, now we begin the second part of the path */
			/* shift first point, continue drawing */
			start = i;
			pts[0].x = p[-1].x - diff;
			pts[0].y = p[-1].y;
			p = pts;
		}
		old = p->x;
	}

	cgi->R_DrawLineStrip(i - start, (int*)(&pts));
	cgi->R_Color(nullptr);
}

/**
 * @brief Draw a path on a menu node (usually the 3Dgeoscape map)
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the 3Dgeoscape menu node.
 * @param[in] line The path which is to be drawn
 * @sa GEO_CalcLine
 */
static void GEO_3DMapDrawLine (const uiNode_t* node, const mapline_t* line)
{
	const vec4_t color = {1, 0.5, 0.5, 1};
	screenPoint_t pts[LINE_MAXPTS];
	int numPoints, start;

	start = 0;
	numPoints = 0;

	/* draw only when the point of the path is visible */
	cgi->R_Color(color);
	for (int i = 0; i < line->numPoints; i++) {
		if (GEO_3DMapToScreen(node, line->point[i], &pts[i].x, &pts[i].y, nullptr))
			numPoints++;
		else if (!numPoints)
			/* the point which is not drawn is at the beginning of the path */
			start++;
	}

	cgi->R_DrawLineStrip(numPoints, (int*)(&pts[start]));
	cgi->R_Color(nullptr);
}

#define CIRCLE_DRAW_POINTS	60
/**
 * @brief Draw equidistant points from a given point on a menu node
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the geoscape menu node.
 * @param[in] center The latitude and longitude of center point
 * @param[in] angle The angle defining the distance of the equidistant points to center
 * @param[in] color The color for drawing
 * @sa RADAR_DrawCoverage
 */
static void GEO_MapDrawEquidistantPoints (const uiNode_t* node, const vec2_t center, const float angle, const vec4_t color)
{
	screenPoint_t pts[CIRCLE_DRAW_POINTS + 1];
	int numPoints = 0;
	vec3_t initialVector, rotationAxis, currentPoint, centerPos;

	cgi->R_Color(color);

	/* Set centerPos corresponding to cartesian coordinates of the center point */
	PolarToVec(center, centerPos);

	/* Find a perpendicular vector to centerPos, and rotate centerPos around it to obtain one point distant of angle from centerPos */
	PerpendicularVector(rotationAxis, centerPos);
	RotatePointAroundVector(initialVector, rotationAxis, centerPos, angle);

	bool draw = false;
	bool oldDraw = false;
	const mapExtraData_t& data = UI_MAPEXTRADATACONST(node);
	/* Now, each equidistant point is given by a rotation around centerPos */
	for (int i = 0; i <= CIRCLE_DRAW_POINTS; i++) {
		int xCircle, yCircle;
		vec2_t posCircle;
		const float degrees = i * 360.0f / (float)CIRCLE_DRAW_POINTS;
		RotatePointAroundVector(currentPoint, centerPos, initialVector, degrees);
		VecToPolar(currentPoint, posCircle);
		if (GEO_AllMapToScreen(node, posCircle, &xCircle, &yCircle, nullptr)) {
			draw = true;
			if (data.flatgeoscape && numPoints != 0 && abs(pts[numPoints - 1].x - xCircle) > 512)
				oldDraw = false;
		}

		/* if moving from a point of the screen to a distant one, draw the path we already calculated, and begin a new path
		 * (to avoid unwanted lines) */
		if (draw != oldDraw && i != 0) {
			cgi->R_DrawLineStrip(numPoints, (int*)(&pts));
			numPoints = 0;
		}
		/* if the current point is to be drawn, add it to the path */
		if (draw) {
			pts[numPoints].x = xCircle;
			pts[numPoints].y = yCircle;
			numPoints++;
		}
		/* update value of oldDraw */
		oldDraw = draw;
	}

	/* Draw the last path */
	cgi->R_DrawLineStrip(numPoints, (int*)(&pts));
	cgi->R_Color(nullptr);
}

/**
 * @brief Return the angle of a model given its position and destination, on 3D geoscape.
 * @param[in] start Latitude and longitude of the position of the model.
 * @param[in] end Latitude and longitude of aimed point.
 * @param[in] direction vec3_t giving current direction of the model (nullptr if the model is idle).
 * @param[out] ortVector If not nullptr, this will be filled with the normalized vector around which rotation allows to go toward @c direction.
 * @return Angle (degrees) of rotation around the radius axis of earth for @c start going toward @c end. Zero value is the direction of North pole.
 */
static float GEO_AngleOfPath3D (const vec2_t start, const vec2_t end, vec3_t direction, vec3_t ortVector)
{
	vec3_t start3D, end3D, north3D, ortToDest, ortToPole, v;
	const vec2_t northPole = {0.0f, 90.0f};	/**< Position of the north pole (used to know where the 'up' side is) */

	PolarToVec(start, start3D);
	PolarToVec(end, end3D);
	PolarToVec(northPole, north3D);

	/* calculate the vector othogonal to movement */
	CrossProduct(start3D, end3D, ortToDest);
	VectorNormalize(ortToDest);
	if (ortVector) {
		VectorCopy(ortToDest, ortVector);
	}

	/* calculate the vector othogonal to north pole (from model location) */
	CrossProduct(start3D, north3D, ortToPole);
	VectorNormalize(ortToPole);

	/**
	 * @todo Save the value angle instead of direction: we don't need a vector here,
	 * we could just compare angle to current angle.
	 */
	/* smooth change of direction if the model is not idle */
	if (direction) {
		VectorSubtract(ortToDest, direction, v);
		const float dist = VectorLength(v);
		if (dist > 0.01) {
			vec3_t rotationAxis;
			CrossProduct(direction, ortToDest, rotationAxis);
			VectorNormalize(rotationAxis);
			RotatePointAroundVector(v, rotationAxis, direction, 5.0);
			VectorCopy(v, direction);
			VectorSubtract(ortToDest, direction, v);
			if (VectorLength(v) < dist)
				VectorCopy(direction, ortToDest);
			else
				VectorCopy(ortToDest, direction);
		}
	}

	/* calculate the angle the model is making at earth surface with north pole direction */
	float angle = todeg * acos(DotProduct(ortToDest, ortToPole));
	/* with arcos, you only get the absolute value of the angle: get the sign */
	CrossProduct(ortToDest, ortToPole, v);
	if (DotProduct(start3D, v) < 0)
		angle = - angle;

	return angle;
}

/**
 * @brief Return the angle of a model given its position and destination, on 2D geoscape.
 * @param[in] start Latitude and longitude of the position of the model.
 * @param[in] end Latitude and longitude of aimed point.
 * @param[in] direction vec3_t giving current direction of the model (nullptr if the model is idle).
 * @param[out] ortVector If not nullptr, this will be filled with the normalized vector around which rotation allows to go toward @c direction.
 * @return Angle (degrees) of rotation around the radius axis of earth for @c start going toward @c end. Zero value is the direction of North pole.
 */
static float GEO_AngleOfPath2D (const vec2_t start, const vec2_t end, vec3_t direction, vec3_t ortVector)
{
	vec3_t start3D, end3D, tangentVector, v, rotationAxis;

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
		const float dist = VectorLength(v);
		if (dist > 0.01) {
			CrossProduct(direction, tangentVector, rotationAxis);
			VectorNormalize(rotationAxis);
			RotatePointAroundVector(v, rotationAxis, direction, 5.0);
			VectorSubtract(tangentVector, direction, v);
			if (VectorLength(v) < dist)
				VectorCopy(direction, tangentVector);
			else
				VectorCopy(tangentVector, direction);
		}
	}

	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v, rotationAxis, tangentVector, - start[0]);
	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(tangentVector, rotationAxis, v, start[1] + 90.0f);

	/* calculate the orientation angle of the model around axis perpendicular to the screen */
	float angle = todeg * atan(tangentVector[0] / tangentVector[1]);
	if (tangentVector[1] > 0)
		angle -= 90.0f;
	else
		angle += 90.0f;

	return angle;
}

/**
 * @brief Select which function should be used for calculating the direction of model on 2D or 3D geoscape.
 * @param[in] start Latitude and longitude of the position of the model.
 * @param[in] end Latitude and longitude of aimed point.
 * @param[in] direction vec3_t giving current direction of the model (nullptr if the model is idle).
 * @param[out] ortVector If not nullptr, this will be filled with the normalized vector around which rotation allows to go toward @c direction.
 * @return Angle (degrees) of rotation around the radius axis of earth for @c start going toward @c end. Zero value is the direction of North pole.
 */
float GEO_AngleOfPath (const vec2_t start, const vec2_t end, vec3_t direction, vec3_t ortVector)
{
	uiNode_t* node = geoscapeNode;
	if (!node)
		return 0.0f;

	const mapExtraData_t& data = UI_MAPEXTRADATA(node);
	if (!data.flatgeoscape)
		return GEO_AngleOfPath3D(start, end, direction, ortVector);
	return GEO_AngleOfPath2D(start, end, direction, ortVector);
}

/**
 * @brief Will set the vector for the geoscape position
 * @param[in] flatgeoscape True for 2D geoscape
 * @param[out] vector The output vector. A two-dim vector for the flat geoscape, and a three-dim vector for the 3d geoscape
 * @param[in] objectPos The position vector of the object to transform.
 */
static void GEO_ConvertObjectPositionToGeoscapePosition (bool flatgeoscape, float* vector, const vec2_t objectPos)
{
	if (flatgeoscape)
		Vector2Set(vector, objectPos[0], objectPos[1]);
	else
		VectorSet(vector, objectPos[0], -objectPos[1], 0);
}

/**
 * @brief center to a mission
 */
static void GEO_GetMissionAngle (bool flatgeoscape, float* vector, int id)
{
	mission_t* mission = MIS_GetByIdx(id);
	if (mission == nullptr)
		return;
	GEO_ConvertObjectPositionToGeoscapePosition(flatgeoscape, vector, mission->pos);
	GEO_SelectMission(mission);
}

/**
 * @brief center to a ufo
 */
static void GEO_GetUFOAngle (bool flatgeoscape, float* vector, int idx)
{
	aircraft_t* ufo;

	/* Cycle through UFOs (only those visible on geoscape) */
	ufo = nullptr;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != nullptr) {
		if (ufo->idx != idx)
			continue;
		GEO_ConvertObjectPositionToGeoscapePosition(flatgeoscape, vector, ufo->pos);
		GEO_SelectUFO(ufo);
		return;
	}
}


/**
 * @brief Start center to the selected point
 */
static void GEO_StartCenter (uiNode_t* node)
{
	mapExtraData_t& data = UI_MAPEXTRADATA(node);
	if (data.flatgeoscape) {
		/* case 2D geoscape */
		vec2_t diff;

		Vector2Set(data.smoothFinal2DGeoscapeCenter, 0.5f - data.smoothFinal2DGeoscapeCenter[0] / 360.0f,
				0.5f - data.smoothFinal2DGeoscapeCenter[1] / 180.0f);
		if (data.smoothFinal2DGeoscapeCenter[1] < 0.5 / ZOOM_LIMIT)
			data.smoothFinal2DGeoscapeCenter[1] = 0.5 / ZOOM_LIMIT;
		if (data.smoothFinal2DGeoscapeCenter[1] > 1.0 - 0.5 / ZOOM_LIMIT)
			data.smoothFinal2DGeoscapeCenter[1] = 1.0 - 0.5 / ZOOM_LIMIT;
		diff[0] = data.smoothFinal2DGeoscapeCenter[0] - data.center[0];
		diff[1] = data.smoothFinal2DGeoscapeCenter[1] - data.center[1];
		data.smoothDeltaLength = sqrt(diff[0] * diff[0] + diff[1] * diff[1]);
	} else {
		/* case 3D geoscape */
		vec3_t diff;

		data.smoothFinalGlobeAngle[1] += GLOBE_ROTATE;
		VectorSubtract(data.smoothFinalGlobeAngle, data.angles, diff);
		data.smoothDeltaLength = VectorLength(diff);
	}

	data.smoothFinalZoom = ZOOM_LIMIT;
	data.smoothDeltaZoom = fabs(data.smoothFinalZoom - data.zoom);
	data.smoothRotation = true;
}

/**
 * @brief Start to rotate or shift the globe to the given position
 * @param[in] pos Longitude and latitude of the position to center on
 */
void GEO_CenterPosition (const vec2_t pos)
{
	uiNode_t* node = geoscapeNode;
	if (!node)
		return;
	mapExtraData_t& data = UI_MAPEXTRADATA(node);
	const bool flatgeoscape = data.flatgeoscape;
	float* vector;
	if (flatgeoscape)
		vector = data.smoothFinal2DGeoscapeCenter;
	else
		vector = data.smoothFinalGlobeAngle;

	GEO_ConvertObjectPositionToGeoscapePosition(flatgeoscape, vector, pos);
	GEO_StartCenter(node);
}

/**
 * @brief Center the view and select an object from the geoscape
 */
static void GEO_SelectObject_f (void)
{
	uiNode_t* node = geoscapeNode;
	if (!node)
		return;

	if (cgi->Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <mission|ufo> <id>\n", cgi->Cmd_Argv(0));
		return;
	}

	const char* type = cgi->Cmd_Argv(1);
	const int idx = atoi(cgi->Cmd_Argv(2));
	mapExtraData_t& data = UI_MAPEXTRADATA(node);
	const bool flatgeoscape = data.flatgeoscape;

	float* vector;
	if (flatgeoscape)
		vector = data.smoothFinal2DGeoscapeCenter;
	else
		vector = data.smoothFinalGlobeAngle;

	if (Q_streq(type, "mission"))
		GEO_GetMissionAngle(flatgeoscape, vector, idx);
	else if (Q_streq(type, "ufo"))
		GEO_GetUFOAngle(flatgeoscape, vector, idx);
	else {
		Com_Printf("GEO_SelectObject_f: type %s unsupported.", type);
		return;
	}
	GEO_StartCenter(node);
}

/**
 * @brief Returns position of the model corresponding to centerOnEventIdx.
 * @param[out] pos the position of the object
 */
static void GEO_GetGeoscapeAngle (vec2_t pos)
{
	int counter = 0;
	int maxEventIdx;
	const int numMissions = CP_CountMissionOnGeoscape();
	aircraft_t* ufo;
	base_t* base;
	int numBases = B_GetCount();

	/* If the value of maxEventIdx is too big or to low, restart from beginning */
	maxEventIdx = numMissions + numBases + INS_GetCount() - 1;
	base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		AIR_ForeachFromBase(aircraft, base) {
			if (AIR_IsAircraftOnGeoscape(aircraft))
				maxEventIdx++;
		}
	}
	ufo = nullptr;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != nullptr)
		maxEventIdx++;

	/* if there's nothing to center the view on, just go to 0,0 pos */
	if (maxEventIdx < 0) {
		Vector2Copy(vec2_origin, pos);
		return;
	}

	/* check centerOnEventIdx is within the bounds */
	if (centerOnEventIdx < 0)
		centerOnEventIdx = maxEventIdx;
	if (centerOnEventIdx > maxEventIdx)
		centerOnEventIdx = 0;

	/* Cycle through missions */
	if (centerOnEventIdx < numMissions) {
		MIS_Foreach(mission) {
			if (!mission->onGeoscape)
				continue;
			if (counter == centerOnEventIdx) {
				Vector2Copy(mission->pos, pos);
				GEO_SelectMission(mission);
				return;
			}
			counter++;
		}
	}
	counter = numMissions;

	/* Cycle through bases */
	if (centerOnEventIdx < numBases + counter) {
		base = nullptr;
		while ((base = B_GetNext(base)) != nullptr) {
			if (counter == centerOnEventIdx) {
				Vector2Copy(base->pos, pos);
				return;
			}
			counter++;
		}
	}
	counter += numBases;

	/* Cycle through installations */
	if (centerOnEventIdx < INS_GetCount() + counter) {
		INS_Foreach(inst) {
			if (counter == centerOnEventIdx) {
				Vector2Copy(inst->pos, pos);
				return;
			}
			counter++;
		}
	}
	counter += INS_GetCount();

	/* Cycle through aircraft (only those present on geoscape) */
	AIR_Foreach(aircraft) {
		if (AIR_IsAircraftOnGeoscape(aircraft)) {
			if (centerOnEventIdx == counter) {
				Vector2Copy(aircraft->pos, pos);
				GEO_SelectAircraft(aircraft);
				return;
			}
			counter++;
		}
	}

	/* Cycle through UFOs (only those visible on geoscape) */
	ufo = nullptr;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != nullptr) {
		if (centerOnEventIdx == counter) {
			Vector2Copy(ufo->pos, pos);
			GEO_SelectUFO(ufo);
			return;
		}
		counter++;
	}
}

/**
 * @brief Switch to next model on 2D and 3D geoscape.
 * @note Set @c smoothRotation to @c true to allow a smooth rotation in GEO_Draw.
 * @note This function sets the value of smoothFinalGlobeAngle (for 3D) or smoothFinal2DGeoscapeCenter (for 2D),
 *  which contains the final value that ccs.angles or ccs.centre must respectively take.
 */
void GEO_CenterOnPoint_f (void)
{
	if (!Q_streq(cgi->UI_GetActiveWindowName(), "geoscape"))
		return;

	centerOnEventIdx++;

	uiNode_t* node = geoscapeNode;
	if (!node)
		return;

	vec2_t pos;
	GEO_GetGeoscapeAngle(pos);
	GEO_CenterPosition(pos);
}

#define BULLET_SIZE	1
/**
 * @brief Draws a bunch of bullets on the geoscape map
 * @param[in] node Pointer to the node in which you want to draw the bullets.
 * @param[in] pos
 * @sa GEO_Draw
 */
static void GEO_DrawBullets (const uiNode_t* node, const vec3_t pos)
{
	int x, y;

	if (GEO_AllMapToScreen(node, pos, &x, &y, nullptr))
		cgi->R_DrawFill(x, y, BULLET_SIZE, BULLET_SIZE, yellow);
}

/**
 * @brief Draws a energy beam on the geoscape map (laser/particle)
 * @param[in] node Pointer to the node in which you want to draw.
 * @param[in] start Start position of the shot (on geoscape)
 * @param[in] end End position of the shot (on geoscape)
 * @param[in] color color of the beam
 * @sa GEO_Draw
 */
static void GEO_DrawBeam (const uiNode_t* node, const vec3_t start, const vec3_t end, const vec4_t color)
{
	int points[4];

	if (!GEO_AllMapToScreen(node, start, &(points[0]), &(points[1]), nullptr))
		return;
	if (!GEO_AllMapToScreen(node, end, &(points[2]), &(points[3]), nullptr))
		return;

	cgi->R_Color(color);
	cgi->R_DrawLine(points, 2.0);
	cgi->R_Color(nullptr);
}

static inline void GEO_RenderImage (int x, int y, const char* image)
{
	cgi->R_DrawImageCentered(x, y, image);
}

#define SELECT_CIRCLE_RADIUS	1.5f + 3.0f / UI_MAPEXTRADATACONST(node).zoom

/**
 * @brief Draws one mission on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] mission Pointer to the mission to draw.
 */
static void GEO_DrawMapOneMission (const uiNode_t* node, const mission_t* mission)
{
	int x, y;
	const bool isCurrentSelectedMission = GEO_IsMissionSelected(mission);

	if (isCurrentSelectedMission)
		cgi->Cvar_Set("mn_mapdaytime", GEO_IsNight(mission->pos) ? _("Night") : _("Day"));

	if (!GEO_AllMapToScreen(node, mission->pos, &x, &y, nullptr))
		return;

	const mapExtraData_t& data = UI_MAPEXTRADATACONST(node);
	if (isCurrentSelectedMission) {
		/* Draw circle around the mission */
		if (data.flatgeoscape) {
			if (mission->active) {
				GEO_RenderImage(x, y, "pics/geoscape/circleactive");
			} else {
				GEO_RenderImage(x, y, "pics/geoscape/circle");
			}
		} else {
			if (!mission->active)
				GEO_MapDrawEquidistantPoints(node, mission->pos, SELECT_CIRCLE_RADIUS, yellow);
		}
	}

	/* Draw mission model (this must be called after drawing the selection circle so that the model is rendered on top of it)*/
	if (data.flatgeoscape) {
		GEO_RenderImage(x, y, "pics/geoscape/mission");
	} else {
		GEO_Draw3DMarkerIfVisible(node, mission->pos, defaultBaseAngle, MIS_GetModel(mission), 0);
	}

	cgi->UI_DrawString("f_verysmall", ALIGN_UL, x + 10, y, MIS_GetName(mission));
}

/**
 * @brief Draw only the "wire" Radar coverage.
 * @param[in] node The menu node where radar coverage will be drawn.
 * @param[in] radar Pointer to the radar that will be drawn.
 * @param[in] pos Position of the radar.
 */
static void GEO_DrawRadarLineCoverage (const uiNode_t* node, const radar_t* radar, const vec2_t pos)
{
	const vec4_t color = {1., 1., 1., .4};
	GEO_MapDrawEquidistantPoints(node, pos, radar->range, color);
	GEO_MapDrawEquidistantPoints(node, pos, radar->trackingRange, color);
}

/**
 * @brief Draw only the "wire" part of the radar coverage in geoscape
 * @param[in] node The menu node where radar coverage will be drawn.
 * @param[in] radar Pointer to the radar that will be drawn.
 * @param[in] pos Position of the radar.
 */
static void GEO_DrawRadarInMap (const uiNode_t* node, const radar_t* radar, const vec2_t pos)
{
	/* Show radar range zones */
	GEO_DrawRadarLineCoverage(node, radar, pos);

	/* everything below is drawn only if there is at least one detected UFO */
	if (!radar->numUFOs)
		return;

	/* Draw lines from radar to ufos sensored */
	int x, y;
	const bool display = GEO_AllMapToScreen(node, pos, &x, &y, nullptr);
	if (!display)
		return;

	screenPoint_t pts[2];
	pts[0].x = x;
	pts[0].y = y;

	/* Set color */
	const vec4_t color = {1., 1., 1., .3};
	cgi->R_Color(color);
	for (int i = 0; i < radar->numUFOs; i++) {
		const aircraft_t* ufo = radar->ufos[i];
		if (UFO_IsUFOSeenOnGeoscape(ufo) && GEO_AllMapToScreen(node, ufo->pos, &x, &y, nullptr)) {
			pts[1].x = x;
			pts[1].y = y;
			cgi->R_DrawLineStrip(2, (int*)pts);
		}
	}
	cgi->R_Color(nullptr);
}

/**
 * @brief Draws one installation on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] installation Pointer to the installation to draw.
 * @param[in] oneUFOVisible Is there at least one UFO visible on the geoscape?
 * @param[in] font Default font.
 * @pre installation is not nullptr.
 */
static void GEO_DrawMapOneInstallation (const uiNode_t* node, const installation_t* installation,
	bool oneUFOVisible, const char* font)
{
	const installationTemplate_t* tpl = installation->installationTemplate;

	/* Draw weapon range if at least one UFO is visible */
	if (oneUFOVisible && AII_InstallationCanShoot(installation)) {
		for (int i = 0; i < tpl->maxBatteries; i++) {
			const aircraftSlot_t* slot = &installation->batteries[i].slot;
			if (slot->item && slot->ammoLeft != 0 && slot->installationTime == 0) {
				GEO_MapDrawEquidistantPoints(node, installation->pos,
					slot->ammo->craftitem.stats[AIR_STATS_WRANGE], red);
			}
		}
	}

	/* Draw installation radar (only the "wire" style part) */
	if (GEO_IsRadarOverlayActivated())
		GEO_DrawRadarInMap(node, &installation->radar, installation->pos);

	int x, y;
	/* Draw installation */
	if (!UI_MAPEXTRADATACONST(node).flatgeoscape) {
		GEO_Draw3DMarkerIfVisible(node, installation->pos, defaultBaseAngle, tpl->model, 0);
	} else if (GEO_MapToScreen(node, installation->pos, &x, &y)) {
		GEO_RenderImage(x, y, tpl->image);
	}

	/* Draw installation names */
	if (GEO_AllMapToScreen(node, installation->pos, &x, &y, nullptr))
		cgi->UI_DrawString(font, ALIGN_UL, x, y + 10, installation->name);
}

/**
 * @brief Draws one base on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] base Pointer to the base to draw.
 * @param[in] oneUFOVisible Is there at least one UFO visible on the geoscape?
 * @param[in] font Default font.
 */
static void GEO_DrawMapOneBase (const uiNode_t* node, const base_t* base,
	bool oneUFOVisible, const char* font)
{
	/* Draw weapon range if at least one UFO is visible */
	if (oneUFOVisible && AII_BaseCanShoot(base)) {
		int i;
		for (i = 0; i < base->numBatteries; i++) {
			const aircraftSlot_t* slot = &base->batteries[i].slot;
			if (slot->item && slot->ammoLeft != 0 && slot->installationTime == 0) {
				GEO_MapDrawEquidistantPoints(node, base->pos,
					slot->ammo->craftitem.stats[AIR_STATS_WRANGE], red);
			}
		}
		for (i = 0; i < base->numLasers; i++) {
			const aircraftSlot_t* slot = &base->lasers[i].slot;
			if (slot->item && slot->ammoLeft != 0 && slot->installationTime == 0) {
				GEO_MapDrawEquidistantPoints(node, base->pos,
					slot->ammo->craftitem.stats[AIR_STATS_WRANGE], red);
			}
		}
	}

	/* Draw base radar (only the "wire" style part) */
	if (GEO_IsRadarOverlayActivated())
		GEO_DrawRadarInMap(node, &base->radar, base->pos);

	int x, y;
	/* Draw base */
	if (!UI_MAPEXTRADATACONST(node).flatgeoscape) {
		if (B_IsUnderAttack(base))
			/* two skins - second skin is for baseattack */
			GEO_Draw3DMarkerIfVisible(node, base->pos, defaultBaseAngle, "geoscape/base", 1);
		else
			GEO_Draw3DMarkerIfVisible(node, base->pos, defaultBaseAngle, "geoscape/base", 0);
	} else if (GEO_MapToScreen(node, base->pos, &x, &y)) {
		if (B_IsUnderAttack(base))
			GEO_RenderImage(x, y, "pics/geoscape/baseattack");
		else
			GEO_RenderImage(x, y, "pics/geoscape/base");
	}

	/* Draw base names */
	if (GEO_AllMapToScreen(node, base->pos, &x, &y, nullptr))
		cgi->UI_DrawString(font, ALIGN_UL, x, y + 10, base->name);
}

/**
 * @brief Draws health bar for an aircraft (either phalanx or ufo)
 * @param[in] node Pointer to the meunode to draw in
 * @param[in] aircraft Pointer to the aircraft to draw for
 * @note if max health (AIR_STATS_DAMAGE) <= 0 no healthbar drawn
 */
static void GEO_DrawAircraftHealthBar (const uiNode_t* node, const aircraft_t* aircraft)
{
	const mapExtraData_t& data = UI_MAPEXTRADATACONST(node);
	const int width = 8 * data.zoom;
	const int height = 1 * data.zoom * 0.9f;
	vec4_t color;
	int centerX;
	int centerY;
	bool visible;

	if (!aircraft)
		return;
	if (aircraft->stats[AIR_STATS_DAMAGE] <= 0)
		return;

	if (((float)aircraft->damage / aircraft->stats[AIR_STATS_DAMAGE]) <= .33f) {
		Vector4Copy(red, color);
	} else if (((float)aircraft->damage / aircraft->stats[AIR_STATS_DAMAGE]) <= .75f) {
		Vector4Copy(yellow, color);
	} else {
		Vector4Copy(green, color);
	}

	if (!data.flatgeoscape)
		visible = GEO_3DMapToScreen(node, aircraft->pos, &centerX, &centerY, nullptr);
	else
		visible = GEO_AllMapToScreen(node, aircraft->pos, &centerX, &centerY, nullptr);

	if (visible) {
		const vec4_t bordercolor = {1, 1, 1, 1};
		cgi->R_DrawFill(centerX - width / 2 , centerY - 5 * data.zoom, round(width * ((float)aircraft->damage / aircraft->stats[AIR_STATS_DAMAGE])), height, color);
		cgi->R_DrawRect(centerX - width / 2, centerY - 5 * data.zoom, width, height, bordercolor, 1.0, 1);
	}
}

/**
 * @brief Draws one Phalanx aircraft on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] aircraft Pointer to the aircraft to draw.
 * @param[in] oneUFOVisible Is there at least one UFO visible on the geoscape?
 */
static void GEO_DrawMapOnePhalanxAircraft (const uiNode_t* node, aircraft_t* aircraft, bool oneUFOVisible)
{
	float angle;

	/* Draw aircraft radar (only the "wire" style part) */
	if (GEO_IsRadarOverlayActivated())
		GEO_DrawRadarInMap(node, &aircraft->radar, aircraft->pos);

	/* Draw only the bigger weapon range on geoscape: more detail will be given on airfight map */
	if (oneUFOVisible)
		GEO_MapDrawEquidistantPoints(node, aircraft->pos, aircraft->stats[AIR_STATS_WRANGE] / 1000.0f, red);

	const mapExtraData_t& data = UI_MAPEXTRADATACONST(node);
	/* Draw aircraft route */
	if (aircraft->status >= AIR_TRANSIT) {
		/* aircraft is moving */
		mapline_t path;

		path.numPoints = aircraft->route.numPoints - aircraft->point;
		/** @todo : check why path.numPoints can be sometime equal to -1 */
		if (path.numPoints > 1) {
			memcpy(path.point, aircraft->pos, sizeof(vec2_t));
			memcpy(path.point + 1, aircraft->route.point + aircraft->point + 1, (path.numPoints - 1) * sizeof(vec2_t));
			if (!data.flatgeoscape)
				GEO_3DMapDrawLine(node, &path);
			else
				GEO_MapDrawLine(node, &path);
		}
		angle = GEO_AngleOfPath(aircraft->pos, aircraft->route.point[aircraft->route.numPoints - 1], aircraft->direction, nullptr);
	} else {
		/* aircraft is idle */
		angle = 0.0f;
	}

	/* Draw a circle around selected aircraft */
	if (GEO_IsAircraftSelected(aircraft)) {
		int x;
		int y;

		if (!data.flatgeoscape)
			GEO_MapDrawEquidistantPoints(node, aircraft->pos, SELECT_CIRCLE_RADIUS, yellow);
		else {
			GEO_AllMapToScreen(node, aircraft->pos, &x, &y, nullptr);
			GEO_RenderImage(x, y, "pics/geoscape/circleactive");
		}

		/* Draw a circle around the ufo pursued by selected aircraft */
		if (aircraft->status == AIR_UFO && GEO_AllMapToScreen(node, aircraft->aircraftTarget->pos, &x, &y, nullptr)) {
			if (!data.flatgeoscape)
				GEO_MapDrawEquidistantPoints(node, aircraft->aircraftTarget->pos, SELECT_CIRCLE_RADIUS, yellow);
			else
				GEO_RenderImage(x, y, "pics/geoscape/circleactive");
		}
	}

	/* Draw aircraft (this must be called after drawing the selection circle so that the aircraft is drawn on top of it)*/
	GEO_Draw3DMarkerIfVisible(node, aircraft->pos, angle, aircraft->model, 0);

	/** @todo we should only show healthbar if the aircraft is fighting but it's a slow algo */
	if (oneUFOVisible || cgi->Cvar_GetInteger("debug_showcrafthealth") >= 1)
		GEO_DrawAircraftHealthBar(node, aircraft);
}

/**
 * @brief Assembles a string for a mission that is on the geoscape
 * @param[in] mission The mission to get the description for
 * @param[out] buffer The target buffer to store the text in
 * @param[in] size The size of the target buffer
 * @return A pointer to the buffer that was given to this function
 */
static const char* GEO_GetMissionText (char* buffer, size_t size, const mission_t* mission)
{
	assert(mission);
	Com_sprintf(buffer, size, _("Name: %s\nObjective: %s"),
		MIS_GetName(mission), (mission->mapDef) ? _(mission->mapDef->description) : _("Unknown"));
	return buffer;
}

/**
 * @brief Assembles a string for an aircraft that is on the geoscape
 * @param[in] aircraft The aircraft to get the description for
 * @param[out] buffer The target buffer to store the text in
 * @param[in] size The size of the target buffer
 * @return A pointer to the buffer that was given to this function
 */
static const char* GEO_GetAircraftText (char* buffer, size_t size, const aircraft_t* aircraft)
{
	if (aircraft->status == AIR_UFO) {
		const float distance = GetDistanceOnGlobe(aircraft->pos, aircraft->aircraftTarget->pos);
		Com_sprintf(buffer, size, _("Name:\t%s (%i/%i)\n"), aircraft->name, AIR_GetTeamSize(aircraft), aircraft->maxTeamSize);
		Q_strcat(buffer, size, _("Status:\t%s\n"), AIR_AircraftStatusToName(aircraft));
		if (aircraft->stats[AIR_STATS_DAMAGE] > 0)
			Q_strcat(buffer, size, _("Health:\t%3.0f%%\n"), (double)aircraft->damage * 100 / aircraft->stats[AIR_STATS_DAMAGE]);
		Q_strcat(buffer, size, _("Distance to target:\t\t%.0f\n"), distance);
		Q_strcat(buffer, size, _("Speed:\t%i km/h\n"), AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED));
		Q_strcat(buffer, size, _("Fuel:\t%i/%i\n"), AIR_AircraftMenuStatsValues(aircraft->fuel, AIR_STATS_FUELSIZE),
			AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE));
		Q_strcat(buffer, size, _("ETA:\t%sh\n"), CP_SecondConvert((float)SECONDS_PER_HOUR * distance / aircraft->stats[AIR_STATS_SPEED]));
	} else {
		Com_sprintf(buffer, size, _("Name:\t%s (%i/%i)\n"), aircraft->name, AIR_GetTeamSize(aircraft), aircraft->maxTeamSize);
		Q_strcat(buffer, size, _("Status:\t%s\n"), AIR_AircraftStatusToName(aircraft));
		if (aircraft->stats[AIR_STATS_DAMAGE] > 0)
			Q_strcat(buffer, size, _("Health:\t%3.0f%%\n"), (double)aircraft->damage * 100 / aircraft->stats[AIR_STATS_DAMAGE]);
		Q_strcat(buffer, size, _("Speed:\t%i km/h\n"), AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED));
		Q_strcat(buffer, size, _("Fuel:\t%i/%i\n"), AIR_AircraftMenuStatsValues(aircraft->fuel, AIR_STATS_FUELSIZE),
			AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE));
		if (aircraft->status != AIR_IDLE) {
			const float distance = GetDistanceOnGlobe(aircraft->pos,
					aircraft->route.point[aircraft->route.numPoints - 1]);
			Q_strcat(buffer, size, _("ETA:\t%sh\n"), CP_SecondConvert((float)SECONDS_PER_HOUR * distance / aircraft->stats[AIR_STATS_SPEED]));
		}
	}
	return buffer;
}

/**
 * @brief Assembles a string for a UFO that is on the geoscape
 * @param[in] ufo The UFO to get the description for
 * @param[out] buffer The target buffer to store the text in
 * @param[in] size The size of the target buffer
 * @return A pointer to the buffer that was given to this function
 */
static const char* GEO_GetUFOText (char* buffer, size_t size, const aircraft_t* ufo)
{
	Com_sprintf(buffer, size, "%s\n", UFO_GetName(ufo));
	Q_strcat(buffer, size, _("Speed: %i km/h\n"), AIR_AircraftMenuStatsValues(ufo->stats[AIR_STATS_SPEED], AIR_STATS_SPEED));
	return buffer;
}

/**
 * @brief Will add missions and UFOs to the geoscape dock panel
 */
void GEO_UpdateGeoscapeDock (void)
{
	char buf[512];
	aircraft_t* ufo;

	cgi->UI_ExecuteConfunc("clean_geoscape_object");

	/* draw mission pics */
	MIS_Foreach(mission) {
		if (!mission->onGeoscape)
			continue;
		cgi->UI_ExecuteConfunc("add_geoscape_object mission %i \"%s\" \"%s\n%s\"",
			mission->idx, MIS_GetModel(mission), MIS_GetName(mission),
			(mission->mapDef) ? _(mission->mapDef->description) : "");
	}

	/* draws ufos */
	ufo = nullptr;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != nullptr) {
		const unsigned int ufoIDX = UFO_GetGeoscapeIDX(ufo);
		cgi->UI_ExecuteConfunc("add_geoscape_object ufo %i %s \"%s\"",
				ufoIDX, ufo->model, GEO_GetUFOText(buf, sizeof(buf), ufo));
	}
}

/**
 * @brief Draws all ufos, aircraft, bases and so on to the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @note This is a drawing function only, called each time a frame is drawn. Therefore
 * you should not use this function to calculate eg. the distance between 2 items on the geoscape
 * (you should instead calculate it just after one of the items moved -- distance is not
 * going to change when you rotate the earth around itself and the time is stopped eg.).
 * @sa GEO_Draw
 */
void GEO_DrawMarkers (const uiNode_t* node)
{
	int i;
	const char* font;
	aircraft_t* ufo;
	base_t* base;

	const vec4_t white = {1.f, 1.f, 1.f, 0.7f};
	int maxInterpolationPoints;

	assert(node);

	/* font color on geoscape */
	cgi->R_Color(node->color);
	/* default font */
	font = cgi->UI_GetFontFromNode(node);

	/* check if at least 1 UFO is visible */
	const bool oneUFOVisible = UFO_GetNextOnGeoscape(nullptr) != nullptr;

	/* draw mission pics */
	MIS_Foreach(mission) {
		if (!mission->onGeoscape)
			continue;
		GEO_DrawMapOneMission(node, mission);
	}

	/* draw installations */
	INS_Foreach(installation) {
		GEO_DrawMapOneInstallation(node, installation, oneUFOVisible, font);
	}

	/* draw bases */
	base = nullptr;
	while ((base = B_GetNext(base)) != nullptr)
		GEO_DrawMapOneBase(node, base, oneUFOVisible, font);

	/* draw all aircraft */
	AIR_Foreach(aircraft) {
		if (AIR_IsAircraftOnGeoscape(aircraft))
			GEO_DrawMapOnePhalanxAircraft(node, aircraft, oneUFOVisible);
	}

	/* draws ufos */
	ufo = nullptr;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != nullptr) {
#ifdef DEBUG
		/* in debug mode you execute set showufos 1 to see the ufos on geoscape */
		if (cgi->Cvar_GetInteger("debug_showufos")) {
			/* Draw ufo route */
			if (!UI_MAPEXTRADATACONST(node).flatgeoscape)
				GEO_3DMapDrawLine(node, &ufo->route);
			else
				GEO_MapDrawLine(node, &ufo->route);
		} else
#endif
		{
			const float angle = GEO_AngleOfPath(ufo->pos, ufo->route.point[ufo->route.numPoints - 1], ufo->direction, nullptr);
			const mapExtraData_t& data = UI_MAPEXTRADATACONST(node);

			if (!data.flatgeoscape)
				GEO_MapDrawEquidistantPoints(node, ufo->pos, SELECT_CIRCLE_RADIUS, white);

			if (GEO_IsUFOSelected(ufo)) {
				if (!data.flatgeoscape) {
					GEO_MapDrawEquidistantPoints(node, ufo->pos, SELECT_CIRCLE_RADIUS, yellow);
				} else {
					int x, y;
					GEO_AllMapToScreen(node, ufo->pos, &x, &y, nullptr);
					GEO_RenderImage(x, y, "pics/geoscape/circleactive");
				}
			}
			GEO_Draw3DMarkerIfVisible(node, ufo->pos, angle, ufo->model, 0);

			/** @todo we should only show healthbar if aircraft is fighting but it's a slow algo */
			if (RS_IsResearched_ptr(ufo->tech)
			 || cgi->Cvar_GetInteger("debug_showcrafthealth") >= 1)
				GEO_DrawAircraftHealthBar(node, ufo);
		}
	}

	if (ccs.gameTimeScale > 0)
		maxInterpolationPoints = floor(1.0f / (ccs.frametime * (float)ccs.gameTimeScale));
	else
		maxInterpolationPoints = 0;

	/* draws projectiles */
	for (i = 0; i < ccs.numProjectiles; i++) {
		aircraftProjectile_t* projectile = &ccs.projectiles[i];
		vec3_t drawPos = {0, 0, 0};

		if (projectile->hasMoved) {
			projectile->hasMoved = false;
			VectorCopy(projectile->pos[0], drawPos);
		} else {
			if (maxInterpolationPoints > 2 && projectile->numInterpolationPoints < maxInterpolationPoints) {
				/* If a new point hasn't been given and there is at least 3 points need to be filled in then
				 * use linear interpolation to draw the points until a new projectile point is provided.
				 * The reason you need at least 3 points is that acceptable results can be achieved with 2 or less
				 * gaps in points so don't add the overhead of interpolation. */
				const float xInterpolStep = (projectile->projectedPos[0][0] - projectile->pos[0][0]) / (float)maxInterpolationPoints;
				projectile->numInterpolationPoints += 1;
				drawPos[0] = projectile->pos[0][0] + (xInterpolStep * projectile->numInterpolationPoints);
				LinearInterpolation(projectile->pos[0], projectile->projectedPos[0], drawPos[0], drawPos[1]);
			} else {
				VectorCopy(projectile->pos[0], drawPos);
			}
		}

		if (projectile->bullets) {
			GEO_DrawBullets(node, drawPos);
		} else if (projectile->beam) {
			vec3_t start;
			vec3_t end;

			if (projectile->attackingAircraft)
				VectorCopy(projectile->attackingAircraft->pos, start);
			else
				VectorCopy(projectile->attackerPos, start);

			if (projectile->aimedAircraft)
				VectorCopy(projectile->aimedAircraft->pos, end);
			else
				VectorCopy(projectile->idleTarget, end);

			GEO_DrawBeam(node, start, end, projectile->aircraftItem->craftitem.beamColor);
		} else {
			GEO_Draw3DMarkerIfVisible(node, drawPos, projectile->angle, projectile->aircraftItem->model, 0);
		}
	}

	const bool showXVI = CP_IsXVIVisible();
	static char buffer[512];

	/* Draw nation names */
	buffer[0] = 0;
	for (i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = NAT_GetNationByIDX(i);
		int x, y;
		if (GEO_AllMapToScreen(node, nation->pos, &x, &y, nullptr))
			cgi->UI_DrawString("f_verysmall", ALIGN_UC, x , y, _(nation->name));
		if (showXVI) {
			const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
			Q_strcat(buffer, sizeof(buffer), _("%s\t%i%%\n"), _(nation->name), stats->xviInfection);
		}
	}

	if (showXVI)
		cgi->UI_RegisterText(TEXT_XVI, buffer);
	else
		cgi->UI_ResetData(TEXT_XVI);

	cgi->R_Color(nullptr);
}

/**
 * @brief Draw the geoscape
 * @param[in] data Geoscape status data structure
 */
void GEO_Draw (geoscapeData_t* data)
{
	if (!CP_IsRunning()) {
		data->active = false;
		return;
	}

	data->active = true;
	data->map = ccs.curCampaign->map;
	data->nationOverlay = GEO_IsNationOverlayActivated();
	data->xviOverlay = GEO_IsXVIOverlayActivated();
	data->radarOverlay = GEO_IsRadarOverlayActivated();
	data->date = ccs.date;

	geoscapeNode = static_cast<uiNode_t* >(data->geoscapeNode);

	mission_t* mission = GEO_GetSelectedMission();
	/* display text */
	cgi->UI_ResetData(TEXT_STANDARD);
	switch (ccs.mapAction) {
	case MA_NEWBASE:
		cgi->UI_RegisterText(TEXT_STANDARD, _("Select the desired location of the new base on the map.\n"));
		return;
	case MA_NEWINSTALLATION:
		cgi->UI_RegisterText(TEXT_STANDARD, _("Select the desired location of the new installation on the map.\n"));
		return;
	case MA_BASEATTACK:
		if (mission)
			break;
		cgi->UI_RegisterText(TEXT_STANDARD, _("Aliens are attacking our base at this very moment.\n"));
		return;
	case MA_INTERCEPT:
		if (mission)
			break;
		cgi->UI_RegisterText(TEXT_STANDARD, _("Select ufo or mission on map\n"));
		return;
	case MA_UFORADAR:
		if (mission)
			break;
		cgi->UI_RegisterText(TEXT_STANDARD, _("UFO in radar range\n"));
		return;
	case MA_NONE:
		break;
	}

	/* Nothing is displayed yet */
	if (mission) {
		cgi->UI_RegisterText(TEXT_STANDARD, GEO_GetMissionText(textStandard, sizeof(textStandard), mission));
	} else if (GEO_GetSelectedAircraft() != nullptr) {
		const aircraft_t* aircraft = GEO_GetSelectedAircraft();
		if (AIR_IsAircraftInBase(aircraft)) {
			cgi->UI_RegisterText(TEXT_STANDARD, nullptr);
			GEO_ResetAction();
			return;
		}
		cgi->UI_RegisterText(TEXT_STANDARD, GEO_GetAircraftText(textStandard, sizeof(textStandard), aircraft));
	} else if (GEO_GetSelectedUFO() != nullptr) {
		cgi->UI_RegisterText(TEXT_STANDARD, GEO_GetUFOText(textStandard, sizeof(textStandard), GEO_GetSelectedUFO()));
	} else {
#ifdef DEBUG
		if (debug_showInterest->integer) {
			static char t[64];
			Com_sprintf(t, lengthof(t), "Interest level: %i\n", ccs.overallInterest);
			cgi->UI_RegisterText(TEXT_STANDARD, t);
		} else
#endif
		cgi->UI_RegisterText(TEXT_STANDARD, "");
	}
}

/**
 * @brief No more special action on the geoscape
 */
void GEO_ResetAction (void)
{
	/* don't allow a reset when no base is set up */
	if (B_AtLeastOneExists())
		ccs.mapAction = MA_NONE;

	GEO_SetInterceptorAircraft(nullptr);
	GEO_SetSelectedMission(nullptr);
	GEO_SetSelectedAircraft(nullptr);
	GEO_SetSelectedUFO(nullptr);

	if (!radarOverlayWasSet)
		GEO_SetOverlay("radar", 0);
}

/**
 * @brief Select the specified ufo on the geoscape
 */
void GEO_SelectUFO (aircraft_t* ufo)
{
	GEO_ResetAction();
	GEO_SetSelectedUFO(ufo);
}

/**
 * @brief Select the specified aircraft on the geoscape
 */
void GEO_SelectAircraft (aircraft_t* aircraft)
{
	GEO_ResetAction();
	GEO_SetSelectedAircraft(aircraft);
}

/**
 * @brief Select the specified mission
 * @param[in] mission Pointer to the mission to select
 * @return pointer to the selected mission
 */
mission_t* GEO_SelectMission (mission_t* mission)
{
	if (!mission || GEO_IsMissionSelected(mission))
		return GEO_GetSelectedMission();
	GEO_ResetAction();
	ccs.mapAction = MA_INTERCEPT;
	GEO_SetSelectedMission(mission);
	return GEO_GetSelectedMission();
}

/**
 * @brief Notify that a mission has been removed
 */
void GEO_NotifyMissionRemoved (const mission_t* mission)
{
	/* Unselect the current selected mission if it's the same */
	if (GEO_IsMissionSelected(mission))
		GEO_ResetAction();

	GEO_UpdateGeoscapeDock();
}

/**
 * @brief Notify that a UFO has been removed
 * @param[in] ufo Pointer to the ufo has been removed
 * @param[in] destroyed True if the UFO has been destroyed, false if it's been only set invisible (landed)
 */
void GEO_NotifyUFORemoved (const aircraft_t* ufo, bool destroyed)
{
	GEO_UpdateGeoscapeDock();

	if (GEO_GetSelectedUFO() == nullptr)
		return;

	/* Unselect the current selected ufo if it's the same */
	if (GEO_IsUFOSelected(ufo))
		GEO_ResetAction();
	else if (destroyed && ccs.geoscape.selectedUFO > ufo)
		/** @todo convert to linked list */
		ccs.geoscape.selectedUFO--;
}

/**
 * @brief Notify that an aircraft has been removed from game
 * @param[in] aircraft Pointer to the aircraft that has been removed
 */
void GEO_NotifyAircraftRemoved (const aircraft_t* aircraft)
{
	/* Unselect the current selected ufo if its the same */
	if (GEO_IsAircraftSelected(aircraft) || GEO_IsInterceptorSelected(aircraft))
		GEO_ResetAction();
}

/**
 * @brief Translate nation map color to nation
 * @sa GEO_GetColor
 * @param[in] pos Map Coordinates to get the nation from
 * @return returns the nation pointer with the given color on nationPic at given pos
 * @return nullptr if no nation with the given color value was found
 * @note The coordinates already have to be transformed to map coordinates via GEO_ScreenToMap
 */
nation_t* GEO_GetNation (const vec2_t pos)
{
	const byte* color = GEO_GetColor(pos, MAPTYPE_NATIONS, nullptr);
	const vec3_t fcolor = {color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f};
#ifdef PARANOID
	Com_DPrintf(DEBUG_CLIENT, "GEO_GetNation: color value for %.0f:%.0f is r:%i, g:%i, b: %i\n", pos[0], pos[1], color[0], color[1], color[2]);
#endif
	for (int i = 0; i < ccs.numNations; i++) {
		nation_t* nation = NAT_GetNationByIDX(i);
		/* compare the first three color values with color value at pos */
		/* 0.02 x 255 = 5.1, which allow a variation of +-5 for each color components */
		if (VectorEqualEpsilon(nation->color, fcolor, 0.02))
			return nation;
	}
	Com_DPrintf(DEBUG_CLIENT, "GEO_GetNation: No nation found at %.0f:%.0f - color: %i:%i:%i\n", pos[0], pos[1], color[0], color[1], color[2]);
	return nullptr;
}

/**
 * @brief Translate color value to culture type
 * @sa GEO_GetColor
 * @param[in] color the color value from the culture mask
 * @return returns the zone name
 * @note never may return a null pointer or an empty string
 */
static const char* GEO_GetCultureType (const byte* color)
{
	if (MapIsWater(color))
		return "water";
	else if (MapIsEastern(color))
		return "eastern";
	else if (MapIsWestern(color))
		return "western";
	else if (MapIsOriental(color))
		return "oriental";
	else if (MapIsAfrican(color))
		return "african";
	return "western";
}

/**
 * @brief Translate color value to population type
 * @sa GEO_GetColor
 * @param[in] color the color value from the population mask
 * @return returns the zone name
 * @note never may return a null pointer or an empty string
 */
static const char* GEO_GetPopulationType (const byte* color)
{
	if (MapIsWater(color))
		return "water";
	else if (MapIsUrban(color))
		return "urban";
	else if (MapIsSuburban(color))
		return "suburban";
	else if (MapIsVillage(color))
		return "village";
	else if (MapIsRural(color))
		return "rural";
	return "nopopulation";
}

/**
 * @brief Determine the terrain type under a given position
 * @sa GEO_GetColor
 * @param[in] pos Map Coordinates to get the terrain type from
 * @param[out] coast GEO_GetColor will set this to true if the given position is a coast line.
 * @return returns the zone name
 */
static inline const char* GEO_GetTerrainTypeByPos (const vec2_t pos, bool* coast)
{
	const byte* color = GEO_GetColor(pos, MAPTYPE_TERRAIN, coast);
	return cgi->csi->terrainDefs.getTerrainName(color);
}

/**
 * @brief Determine the culture type under a given position
 * @sa GEO_GetColor
 * @param[in] pos Map Coordinates to get the culture type from
 * @return returns the zone name
 */
static inline const char* GEO_GetCultureTypeByPos (const vec2_t pos)
{
	const byte* color = GEO_GetColor(pos, MAPTYPE_CULTURE, nullptr);
	return GEO_GetCultureType(color);
}

/**
 * @brief Determine the population type under a given position
 * @sa GEO_GetColor
 * @param[in] pos Map Coordinates to get the population type from
 * @return returns the zone name
 */
static inline const char* GEO_GetPopulationTypeByPos (const vec2_t pos)
{
	const byte* color = GEO_GetColor(pos, MAPTYPE_POPULATION, nullptr);
	return GEO_GetPopulationType(color);
}

/**
 * @brief Get number of civilian on a map at given position.
 * @param[in] pos Position where the mission takes place.
 * @return Number of civilian.
 * @sa CP_CreateCivilianTeam
 */
int GEO_GetCivilianNumberByPosition (const vec2_t pos)
{
	const byte* color = GEO_GetColor(pos, MAPTYPE_POPULATION, nullptr);

	if (MapIsWater(color))
		cgi->Com_Error(ERR_DROP, "GEO_GetPopulationType: Trying to get number of civilian in a position on water");

	if (MapIsUrban(color))
		return 10;
	else if (MapIsSuburban(color))
		return 8;
	else if (MapIsVillage(color))
		return 6;
	else if (MapIsRural(color))
		return 4;
	else if (MapIsNopopulation(color))
		return 2;

	return 0;
}

/**
 * @brief Prints positions parameter in console.
 * @param[in] pos Location (latitude, longitude) where you want to check
 * @note Used for printing in console, do not translate.
 * @sa NAT_ScriptSanityCheck
 */
void GEO_PrintParameterStringByPos (const vec2_t pos)
{
	bool coast = false;
	const char* terrainType = GEO_GetTerrainTypeByPos(pos, &coast);
	const char* cultureType = GEO_GetCultureTypeByPos(pos);
	const char* populationType = GEO_GetPopulationTypeByPos(pos);

	Com_Printf ("      (Terrain: %s, Culture: %s, Population: %s, Coast: %s)\n",
			terrainType, cultureType, populationType, coast ? "true" : "false");
}

/**
 * @brief Check that a position (in latitude / longitude) is within boundaries.
 * @param[in,out] pos Pointer to the 2 elements vector giving the position.
 */
void GEO_CheckPositionBoundaries (float* pos)
{
	while (pos[0] > 180.0)
		pos[0] -= 360.0;
	while (pos[0] < -180.0)
		pos[0] += 360.0;
	while (pos[1] > 90.0)
		pos[1] -= 180.0;
	while (pos[1] < -90.0)
		pos[1] += 180.0;
}

/**
 * @brief Check whether given position is Day or Night.
 * @param[in] pos Given position.
 * @return True if given position is Night.
 */
bool GEO_IsNight (const vec2_t pos)
{
	float p, q, a, root, x;

	/* set p to hours (we don't use ccs.day here because we need a float value) */
	p = (float) ccs.date.sec / SECONDS_PER_DAY;
	/* convert current day to angle (-pi on 1st january, pi on 31 december) */
	q = (ccs.date.day + p) * (2 * M_PI / DAYS_PER_YEAR_AVG) - M_PI;
	p = (0.5 + pos[0] / 360 - p) * (2 * M_PI) - q;
	a = -sin(pos[1] * torad);
	root = sqrt(1.0 - a * a);
	x = sin(p) * root * sin(q) - (a * SIN_ALPHA + cos(p) * root * COS_ALPHA) * cos(q);
	return (x > 0);
}

/**
 * @brief Returns the color value from geoscape of a certain mask (terrain, culture or population) at a given position.
 * @param[in] pos vec2_t Value of position on map to get the color value from.
 * pos is longitude and latitude
 * @param[in] type determine the map to get the color from (there are different masks)
 * one for the climatezone (bases made use of this - there are grass, ice and desert
 * base tiles available) and one for the nations
 * @param[out] coast The function will set this to true if the given position is a coast line.
 * This can be @c nullptr if you are not interested in this fact.
 * @return Returns the color value at given position.
 * @note terrainPic, culturePic and populationPic are pointers to an rgba image in memory
 */
const byte* GEO_GetColor (const vec2_t pos, mapType_t type, bool* coast)
{
	int x, y;
	int width, height;
	const byte* mask;
	const byte* color;

	switch (type) {
	case MAPTYPE_TERRAIN:
		mask = terrainPic;
		width = terrainWidth;
		height = terrainHeight;
		break;
	case MAPTYPE_CULTURE:
		mask = culturePic;
		width = cultureWidth;
		height = cultureHeight;
		break;
	case MAPTYPE_POPULATION:
		mask = populationPic;
		width = populationWidth;
		height = populationHeight;
		break;
	case MAPTYPE_NATIONS:
		mask = nationsPic;
		width = nationsWidth;
		height = nationsHeight;
		break;
	default:
		cgi->Com_Error(ERR_DROP, "Unknown maptype %i\n", type);
	}

	assert(mask);

	/** @todo add EQUAL_EPSILON here? */
	assert(pos[0] >= -180);
	assert(pos[0] <= 180);
	assert(pos[1] >= -90);
	assert(pos[1] <= 90);

	/* get coordinates */
	x = (180 - pos[0]) / 360 * width;
	x--; /* we start from 0 */
	y = (90 - pos[1]) / 180 * height;
	y--; /* we start from 0 */
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	/* 4 => RGBA */
	/* terrainWidth is the width of the image */
	/* this calculation returns the pixel in col x and in row y */
	assert(4 * (x + y * width) < width * height * 4);
	color = mask + 4 * (x + y * width);
	if (coast != nullptr) {
		if (MapIsWater(color)) {
			*coast = false;
		} else {
			/* only check four directions */
			const int gap = 4;
			if (x > gap) {
				const byte* coastCheck = mask + 4 * ((x - gap) + y * width);
				*coast = MapIsWater(coastCheck);
			}
			if (!*coast && x < width - 1 - gap) {
				const byte* coastCheck = mask + 4 * ((x + gap) + y * width);
				*coast = MapIsWater(coastCheck);
			}

			if (!*coast) {
				if (y > gap) {
					const byte* coastCheck = mask + 4 * (x + (y - gap) * width);
					*coast = MapIsWater(coastCheck);
				}
				if (!*coast && y < height - 1 - gap) {
					const byte* coastCheck = mask + 4 * (x + (y + gap) * width);
					*coast = MapIsWater(coastCheck);
				}
			}
		}
	}

	return color;
}

/**
 * @brief Minimum distance between a new mission and an existing base.
 */
static const float MIN_DIST_BASE = 4.0f;

/**
 * @brief Check if given pos is close to an existing base.
 * @return Pointer to the base if one base is closer than MIN_DIST_BASE from pos, nullptr else
 */
base_t* GEO_PositionCloseToBase (const vec2_t pos)
{
	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr)
		if (GetDistanceOnGlobe(pos, base->pos) < MIN_DIST_BASE)
			return base;

	return nullptr;
}

/**
 * @brief Checks for a given location, if it fulfills all criteria given via parameters (terrain, culture, population, nation type)
 * @param[in] pos Location to be tested
 * @param[in] terrainTypes A linkedList_t containing a list of strings determining the terrain types to be tested for (e.g. "grass") may be nullptr
 * @param[in] cultureTypes A linkedList_t containing a list of strings determining the culture types to be tested for (e.g. "western") may be nullptr
 * @param[in] populationTypes A linkedList_t containing a list of strings determining the population types to be tested for (e.g. "suburban") may be nullptr
 * @param[in] nations A linkedList_t containing a list of strings determining the nations to be tested for (e.g. "asia") may be nullptr
 * @return true if a location was found, otherwise false. If the map is over water, return false
 * @note The name TCPNTypes comes from terrain, culture, population, nation types
 */
bool GEO_PositionFitsTCPNTypes (const vec2_t pos, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations)
{
	bool coast = false;
	const char* terrainType = GEO_GetTerrainTypeByPos(pos, &coast);
	const char* cultureType = GEO_GetCultureTypeByPos(pos);
	const char* populationType = GEO_GetPopulationTypeByPos(pos);

	if (MapIsWater(GEO_GetColor(pos, MAPTYPE_TERRAIN, nullptr)))
		return false;

	if (!terrainTypes || cgi->LIST_ContainsString(terrainTypes, terrainType) || (coast && cgi->LIST_ContainsString(terrainTypes, "coast"))) {
		if (!cultureTypes || cgi->LIST_ContainsString(cultureTypes, cultureType)) {
			if (!populationTypes || cgi->LIST_ContainsString(populationTypes, populationType)) {
				const nation_t* nationAtPos = GEO_GetNation(pos);
				if (!nations)
					return true;
				if (nationAtPos && (!nations || cgi->LIST_ContainsString(nations, nationAtPos->id))) {
					return true;
				}
			}
		}
	}

	return false;
}

void GEO_Shutdown (void)
{
	cgi->Free(terrainPic);
	terrainPic = nullptr;

	cgi->Free(culturePic);
	culturePic = nullptr;

	cgi->Free(populationPic);
	populationPic = nullptr;

	cgi->Free(nationsPic);
	nationsPic = nullptr;
}

void GEO_Init (const char* map)
{
	/* load terrain mask */
	cgi->R_LoadImage(va("pics/geoscape/%s_terrain", map), &terrainPic, &terrainWidth, &terrainHeight);
	if (!terrainPic || !terrainWidth || !terrainHeight)
		cgi->Com_Error(ERR_DROP, "Couldn't load map mask %s_terrain in pics/geoscape", map);

	/* load culture mask */
	cgi->R_LoadImage(va("pics/geoscape/%s_culture", map), &culturePic, &cultureWidth, &cultureHeight);
	if (!culturePic || !cultureWidth || !cultureHeight)
		cgi->Com_Error(ERR_DROP, "Couldn't load map mask %s_culture in pics/geoscape", map);

	/* load population mask */
	cgi->R_LoadImage(va("pics/geoscape/%s_population", map), &populationPic, &populationWidth, &populationHeight);
	if (!populationPic || !populationWidth || !populationHeight)
		cgi->Com_Error(ERR_DROP, "Couldn't load map mask %s_population in pics/geoscape", map);

	/* load nations mask */
	cgi->R_LoadImage(va("pics/geoscape/%s_nations", map), &nationsPic, &nationsWidth, &nationsHeight);
	if (!nationsPic || !nationsWidth || !nationsHeight)
		cgi->Com_Error(ERR_DROP, "Couldn't load map mask %s_nations in pics/geoscape", map);
}

void GEO_Reset (const char* map)
{
	GEO_Shutdown();
	GEO_Init(map);
	GEO_ResetAction();
	GEO_UpdateGeoscapeDock();
}

/**
 * @brief Notify that a UFO disappears on radars
 */
void GEO_NotifyUFODisappear (const aircraft_t* ufo)
{
	/* Unselect the currently selected ufo if it's the same */
	if (GEO_IsUFOSelected(ufo))
		GEO_ResetAction();

	GEO_UpdateGeoscapeDock();
}

/**
 * @brief Turn overlay on/off
 * @param[in] overlayID Name of the overlay you want to switch.
 * @param[in] status On/Off status to set
 */
void GEO_SetOverlay (const char* overlayID, int status)
{
	if (Q_streq(overlayID, "nation")) {
		cgi->Cvar_SetValue("geo_overlay_nation", status);
		return;
	}

	/* do nothing while the first base is not build */
	if (!B_AtLeastOneExists())
		return;

	if (Q_streq(overlayID, "xvi")) {
		cgi->Cvar_SetValue("geo_overlay_xvi", status);
	}
	if (Q_streq(overlayID, "radar")) {
		cgi->Cvar_SetValue("geo_overlay_radar", status);
		if (GEO_IsRadarOverlayActivated())
			RADAR_UpdateWholeRadarOverlay();
	}
}

/**
 * @brief Console command to call GEO_SetOverlay.
 */
static void GEO_SetOverlay_f (void)
{
	const char* overlay;
	int status;

	if (cgi->Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <nation|xvi|radar> <1|0>\n", cgi->Cmd_Argv(0));
		return;
	}

	overlay = cgi->Cmd_Argv(1);
	status = atoi(cgi->Cmd_Argv(2));
	GEO_SetOverlay(overlay, status);

	/* save last decision player took on radar display, in order to be able to restore it later */
	if (Q_streq(overlay, "radar"))
		radarOverlayWasSet = GEO_IsRadarOverlayActivated();
}

/**
 * @brief Initialise MAP/Geoscape
 */
void GEO_InitStartup (void)
{
	cgi->Cmd_AddCommand("multi_select_click", GEO_MultiSelectExecuteAction_f, nullptr);
	cgi->Cmd_AddCommand("geo_setoverlay", GEO_SetOverlay_f, "Set the geoscape overlay");
	cgi->Cmd_AddCommand("map_selectobject", GEO_SelectObject_f, "Select an object and center on it");
	cgi->Cmd_AddCommand("mn_mapaction_reset", GEO_ResetAction, nullptr);

#ifdef DEBUG
	debug_showInterest = cgi->Cvar_Get("debug_showinterest", "0", CVAR_DEVELOPER, "Shows the global interest value on geoscape");
#endif
}
