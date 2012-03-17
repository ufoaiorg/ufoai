/**
 * @file cp_map.c
 * @brief Geoscape/Map management
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

#include "../../cl_shared.h"
#include "../../renderer/r_image.h"
#include "../../renderer/r_framebuffer.h"
#include "../../renderer/r_draw.h"
#include "../../renderer/r_geoscape.h"
#include "../../ui/ui_main.h"
#include "../../ui/ui_font.h" /* UI_GetFontFromNode */
#include "../../ui/ui_render.h" /* UI_DrawString */
#include "../../ui/node/ui_node_abstractnode.h" /* UI_GetNodeAbsPos */
#include "../../ui/node/ui_node_map.h" /* paddingRight */
#include "cp_overlay.h"
#include "cp_campaign.h"
#include "cp_popup.h"
#include "cp_mapfightequip.h"
#include "cp_map.h"
#include "cp_missions.h"
#include "cp_ufo.h"
#include "cp_time.h"
#include "cp_xvi.h"

cvar_t *cl_3dmap;				/**< 3D geoscape or flat geoscape */
static cvar_t *cl_3dmapAmbient;
cvar_t *cl_mapzoommax;
cvar_t *cl_mapzoommin;
cvar_t *cl_geoscape_overlay;

extern image_t *r_dayandnightTexture;
extern image_t *r_radarTexture;
extern image_t *r_xviTexture;

#ifdef DEBUG
static cvar_t *debug_showInterest;
#endif

#define ZOOM_LIMIT	2.5f

enum {
	GEOSCAPE_IMAGE_MISSION,
	GEOSCAPE_IMAGE_MISSION_SELECTED,
	GEOSCAPE_IMAGE_MISSION_ACTIVE,
	GEOSCAPE_IMAGE_BASE,
	GEOSCAPE_IMAGE_BASE_ATTACK,

	GEOSCAPE_IMAGE_MAX
};

static char const* const geoscapeImageNames[] = {
	"pics/geoscape/mission",
	"pics/geoscape/circle",
	"pics/geoscape/circleactive",
	"pics/geoscape/base",
	"pics/geoscape/baseattack"
};
CASSERT(lengthof(geoscapeImageNames) == GEOSCAPE_IMAGE_MAX);

static image_t *geoscapeImages[GEOSCAPE_IMAGE_MAX];

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
static qboolean MAP_IsMapPositionSelected(const uiNode_t* node, const vec2_t pos, int x, int y);
static void MAP3D_ScreenToMap(const uiNode_t* node, int x, int y, vec2_t pos);
static void MAP_ScreenToMap(const uiNode_t* node, int x, int y, vec2_t pos);

/* static variables */
static char textStandard[2048];		/**< Buffer to display standard text on the geoscape */
static int centerOnEventIdx;		/**< Current Event centered on 3D geoscape */

/* Colors */
static const vec4_t green = {0.0f, 1.0f, 0.0f, 0.8f};
static const vec4_t yellow = {1.0f, 0.874f, 0.294f, 1.0f};
static const vec4_t red = {1.0f, 0.0f, 0.0f, 0.8f};

static const float smoothAcceleration = 0.06f;		/**< the acceleration to use during a smooth motion (This affects the speed of the smooth motion) */
static const float defaultBaseAngle = -90.0f;	/**< Default angle value for 3D models like bases */

static byte *terrainPic;				/**< this is the terrain mask for separating the climate
										 * zone and water by different color values */
static int terrainWidth, terrainHeight;		/**< the width and height for the terrain pic. */

static byte *culturePic;				/**< this is the mask for separating the culture
										 * zone and water by different color values */
static int cultureWidth, cultureHeight;		/**< the width and height for the culture pic. */

static byte *populationPic;				/**< this is the mask for separating the population rate
										 * zone and water by different color values */
static int populationWidth, populationHeight;		/**< the width and height for the population pic. */

static byte *nationsPic;				/**< this is the nation mask - separated
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
static void MAP_MultiSelectListAddItem (multiSelectType_t itemType, int itemID,
	const char* itemDescription, const char* itemName)
{
	Q_strcat(multiSelect.popupText, va("%s\t%s\n", itemDescription, itemName), sizeof(multiSelect.popupText));
	multiSelect.selectType[multiSelect.nbSelect] = itemType;
	multiSelect.selectId[multiSelect.nbSelect] = itemID;
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
	qboolean multiSelection = qfalse;

	if (Cmd_Argc() < 2) {
		/* Direct call from code, not from a popup menu */
		selected = 0;
	} else {
		/* Call from a geoscape popup menu (popup_multi_selection) */
		UI_PopWindow(qfalse);
		selected = atoi(Cmd_Argv(1));
		multiSelection = qtrue;
	}

	if (selected < 0 || selected >= multiSelect.nbSelect)
		return;
	id = multiSelect.selectId[selected];

	/* Execute action on element */
	switch (multiSelect.selectType[selected]) {
	case MULTISELECT_TYPE_BASE:	/* Select a base */
		if (id >= B_GetCount())
			break;
		MAP_ResetAction();
		B_SelectBase(B_GetFoundedBaseByIDX(id));
		break;
	case MULTISELECT_TYPE_INSTALLATION: {
		/* Select an installation */
		installation_t *ins = INS_GetByIDX(id);
		if (ins) {
			MAP_ResetAction();
			INS_SelectInstallation(ins);
		}
		break;
	}
	case MULTISELECT_TYPE_MISSION: {
		mission_t *mission = MAP_GetSelectedMission();
		/* Select a mission */
		if (ccs.mapAction == MA_INTERCEPT && mission && mission == MAP_GetMissionByIDX(id)) {
			CL_DisplayPopupInterceptMission(mission);
			return;
		}
		mission = MAP_SelectMission(MAP_GetMissionByIDX(id));
		if (multiSelection) {
			/* if we come from a multiSelection menu, there is no need to open this popup twice to go to a mission */
			CL_DisplayPopupInterceptMission(mission);
			return;
		}
		break;
	}
	case MULTISELECT_TYPE_AIRCRAFT: /* Selection of an aircraft */
		aircraft = AIR_AircraftGetFromIDX(id);
		if (aircraft == NULL) {
			Com_DPrintf(DEBUG_CLIENT, "MAP_MultiSelectExecuteAction: selection of an unknown aircraft idx %i\n", id);
			return;
		}

		if (MAP_IsAircraftSelected(aircraft)) {
			/* Selection of an already selected aircraft */
			CL_DisplayPopupAircraft(aircraft);	/* Display popup_aircraft */
		} else {
			/* Selection of an unselected aircraft */
			MAP_SelectAircraft(aircraft);
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

		if (MAP_IsUFOSelected(aircraft)) {
			/* Selection of a already selected ufo */
			CL_DisplayPopupInterceptUFO(aircraft);
		} else {
			/* Selection of a unselected ufo */
			MAP_SelectUFO(aircraft);
			if (multiSelection)
				/* if we come from a multiSelection menu, there is no need to open this popup twice to choose an action */
				CL_DisplayPopupInterceptUFO(MAP_GetSelectedUFO());
		}
		break;
	case MULTISELECT_TYPE_NONE :	/* Selection of an element that has been removed */
		break;
	default:
		Com_DPrintf(DEBUG_CLIENT, "MAP_MultiSelectExecuteAction: selection of an unknown element type %i\n",
				multiSelect.selectType[selected]);
	}
}

/**
 * @brief Click on the map/geoscape
 * @param[in] node UI Node of the geoscape map
 * @param[in] x Mouse click X coordinate
 * @param[in] y Mouse click Y coordinate
 * @return True if the event is used for something
 */
qboolean MAP_MapClick (uiNode_t* node, int x, int y)
{
	aircraft_t *ufo;
	base_t *base;
	vec2_t pos;

	/* get map position */
	if (cl_3dmap->integer)
		MAP3D_ScreenToMap(node, x, y, pos);
	else
		MAP_ScreenToMap(node, x, y, pos);

	switch (ccs.mapAction) {
	case MA_NEWBASE:
		/* new base construction */
		/** @todo make this a function in cp_base.c - B_BuildBaseAtPos */
		if (!MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN, NULL))) {
			if (B_GetCount() < MAX_BASES) {
				Vector2Copy(pos, ccs.newBasePos);
				CP_GameTimeStop();
				Cmd_ExecuteString("mn_set_base_title");
				UI_PushWindow("popup_newbase", NULL, NULL);
				return qtrue;
			}
			return qfalse;
		}
		break;
	case MA_NEWINSTALLATION:
		if (!MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN, NULL))) {
			Vector2Copy(pos, ccs.newBasePos);
			CP_GameTimeStop();
			UI_PushWindow("popup_newinstallation", NULL, NULL);
			return qtrue;
		}
		break;
	case MA_UFORADAR:
		UI_PushWindow("popup_intercept_ufo", NULL, NULL);
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
		if (tempMission->pos && MAP_IsMapPositionSelected(node, tempMission->pos, x, y))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_MISSION, MAP_GetIDXByMission(tempMission),
				CP_MissionToTypeString(tempMission), _(tempMission->location));
	}

	/* Get selected aircraft which belong */
	AIR_Foreach(aircraft) {
		if (AIR_IsAircraftOnGeoscape(aircraft) && aircraft->fuel > 0 && MAP_IsMapPositionSelected(node, aircraft->pos, x, y))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_AIRCRAFT, aircraft->idx, _("Aircraft"), aircraft->name);
	}

	/* Get selected bases */
	base = NULL;
	while ((base = B_GetNext(base)) != NULL && multiSelect.nbSelect < MULTISELECT_MAXSELECT) {
		if (MAP_IsMapPositionSelected(node, base->pos, x, y))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_BASE, base->idx, _("Base"), base->name);
	}

	/* Get selected installations */
	INS_Foreach(installation) {
		if (multiSelect.nbSelect >= MULTISELECT_MAXSELECT)
			break;
		if (MAP_IsMapPositionSelected(node, installation->pos, x, y))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_INSTALLATION, installation->idx, _("Installation"), installation->name);
	}

	/* Get selected ufos */
	ufo = NULL;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != NULL)
		if (AIR_IsAircraftOnGeoscape(ufo) && MAP_IsMapPositionSelected(node, ufo->pos, x, y))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_UFO, UFO_GetGeoscapeIDX(ufo), _("UFO Sighting"), UFO_AircraftToIDOnGeoscape(ufo));

	if (multiSelect.nbSelect == 1) {
		/* Execute directly action for the only one element selected */
		Cmd_ExecuteString("multi_select_click");
		return qtrue;
	} else if (multiSelect.nbSelect > 1) {
		/* Display popup for multi selection */
		UI_RegisterText(TEXT_MULTISELECTION, multiSelect.popupText);
		CP_GameTimeStop();
		UI_PushWindow("popup_multi_selection", NULL, NULL);
		return qtrue;
	} else {
		aircraft_t *aircraft = MAP_GetSelectedAircraft();
		/* Nothing selected */
		if (!aircraft) {
			MAP_ResetAction();
			return qfalse;
		}
		else if (AIR_IsAircraftOnGeoscape(aircraft) && AIR_AircraftHasEnoughFuel(aircraft, pos)) {
			/* Move the selected aircraft to the position clicked */
			MAP_MapCalcLine(aircraft->pos, pos, &aircraft->route);
			aircraft->status = AIR_TRANSIT;
			aircraft->time = 0;
			aircraft->point = 0;
			return qtrue;
		}
	}
	return qfalse;
}


/*
==============================================================
GEOSCAPE DRAWING AND COORDINATES
==============================================================
*/
/**
 * @brief maximum distance (in pixel) to get a valid mouse click
 * @note this is for a 1024 * 768 screen
 */
#define UI_MAP_DIST_SELECTION 15
/**
 * @brief Tell if the specified position is considered clicked
 */
static qboolean MAP_IsMapPositionSelected (const uiNode_t* node, const vec2_t pos, int x, int y)
{
	int msx, msy;

	if (MAP_AllMapToScreen(node, pos, &msx, &msy, NULL))
		if (x >= msx - UI_MAP_DIST_SELECTION && x <= msx + UI_MAP_DIST_SELECTION
		 && y >= msy - UI_MAP_DIST_SELECTION && y <= msy + UI_MAP_DIST_SELECTION)
			return qtrue;

	return qfalse;
}

/**
 * @brief Typical zoom to use on the 3D geoscape to use same zoom values for both 2D and 3D geoscape
 * @note Used to convert openGL coordinates of the sphere into screen coordinates
 * @sa GLOBE_RADIUS */
const float STANDARD_3D_ZOOM = 40.0f;

/** @brief radius of the globe in screen coordinates */
#define GLOBE_RADIUS EARTH_RADIUS * (ccs.zoom / STANDARD_3D_ZOOM)

/**
 * @brief Transform a 2D position on the map to screen coordinates.
 * @param[in] node Menu node
 * @param[in] pos vector that holds longitude and latitude
 * @param[out] x normalized (rotated and scaled) x value of mouseclick
 * @param[out] y normalized (rotated and scaled) y value of mouseclick
 * @param[out] z z value of the given latitude and longitude - might also be NULL if not needed
 * @sa MAP_MapToScreen
 * @sa MAP3D_ScreenToMap
 * @return qtrue if the point is visible, qfalse else (if it's outside the node or on the wrong side of the earth).
 * @note In the function, we do the opposite of MAP3D_ScreenToMap
 */
static qboolean MAP_3DMapToScreen (const uiNode_t* node, const vec2_t pos, int *x, int *y, int *z)
{
	vec2_t mid;
	vec3_t v, v1, rotationAxis;
	const float radius = GLOBE_RADIUS;

	PolarToVec(pos, v);

	/* rotate the vector to switch of reference frame.
	 * We switch from the static frame of the earth to the local frame of the player (opposite rotation of MAP3D_ScreenToMap) */
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v1, rotationAxis, v, - ccs.angles[PITCH]);

	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v, rotationAxis, v1, - ccs.angles[YAW]);

	/* set mid to the coordinates of the center of the globe */
	Vector2Set(mid, ccs.mapPos[0] + ccs.mapSize[0] / 2.0f, ccs.mapPos[1] + ccs.mapSize[1] / 2.0f);

	/* We now convert those coordinates relative to the center of the globe to coordinates of the screen
	 * (which are relative to the upper left side of the screen) */
	*x = (int) (mid[0] - radius * v[1]);
	*y = (int) (mid[1] - radius * v[0]);

	if (z)
		*z = (int) (radius * v[2]);

	/* if the point is on the wrong side of the earth, the player cannot see it */
	if (v[2] > 0)
		return qfalse;

	/* if the point is outside the screen, the player cannot see it */
	if (*x < ccs.mapPos[0] && *y < ccs.mapPos[1] && *x > ccs.mapPos[0] + ccs.mapSize[0] && *y > ccs.mapPos[1] + ccs.mapSize[1])
		return qfalse;

	return qtrue;
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
static qboolean MAP_MapToScreen (const uiNode_t* node, const vec2_t pos, int *x, int *y)
{
	float sx;

	/* get "raw" position */
	sx = pos[0] / 360 + ccs.center[0] - 0.5;

	/* shift it on screen */
	if (sx < -0.5)
		sx += 1.0;
	else if (sx > +0.5)
		sx -= 1.0;

	*x = ccs.mapPos[0] + 0.5 * ccs.mapSize[0] - sx * ccs.mapSize[0] * ccs.zoom;
	*y = ccs.mapPos[1] + 0.5 * ccs.mapSize[1] -
		(pos[1] / 180 + ccs.center[1] - 0.5) * ccs.mapSize[1] * ccs.zoom;

	if (*x < ccs.mapPos[0] && *y < ccs.mapPos[1] &&
		*x > ccs.mapPos[0] + ccs.mapSize[0] && *y > ccs.mapPos[1] + ccs.mapSize[1])
		return qfalse;
	return qtrue;
}

/**
 * @brief Call either MAP_MapToScreen or MAP_3DMapToScreen depending on the geoscape you're using.
 * @param[in] node Menu node
 * @param[in] pos Position on the map described by longitude and latitude
 * @param[out] x Pointer to the X coordinate on the screen
 * @param[out] y Pointer to the Y coordinate on the screen
 * @param[out] z Pointer to the Z coordinate on the screen (may be NULL if not needed)
 * @returns qtrue if pos corresponds to a point which is visible on the screen. Otherwise returns qfalse.
 * @sa MAP_MapToScreen
 * @sa MAP_3DMapToScreen
 */
qboolean MAP_AllMapToScreen (const uiNode_t* node, const vec2_t pos, int *x, int *y, int *z)
{
	if (cl_3dmap->integer)
		return MAP_3DMapToScreen(node, pos, x, y, z);
	else {
		if (z)
			*z = -10;
		return MAP_MapToScreen(node, pos, x, y);
	}
}

/**
 * @brief Draws a 3D marker on geoscape if the player can see it.
 * @param[in] node Menu node.
 * @param[in] pos Longitude and latitude of the marker to draw.
 * @param[in] theta Angle (degree) of the model to the horizontal.
 * @param[in] model The name of the model of the marker.
 * @param[in] skin Number of modelskin to draw on marker
 */
static void MAP_Draw3DMarkerIfVisible (const uiNode_t* node, const vec2_t pos, float theta, const char *model, int skin)
{
	if (cl_3dmap->integer) {
		R_Draw3DMapMarkers(ccs.mapPos[0], ccs.mapPos[1], ccs.mapSize[0], ccs.mapSize[1], ccs.angles, pos, theta, GLOBE_RADIUS, model, skin);
	} else {
		int x, y;
		vec3_t screenPos;

		MAP_AllMapToScreen(node, pos, &x, &y, NULL);
		VectorSet(screenPos, x, y, 0);
		/* models are used on 2D geoscape for aircraft */
		R_Draw2DMapMarkers(screenPos, theta, model, skin);
	}
}


/**
 * @brief Return longitude and latitude of a point of the screen for 2D geoscape
 * @param[in] node The current menuNode we have clicked on (3dmap or map)
 * @param[in] x X coordinate on the screen that was clicked on
 * @param[in] y Y coordinate on the screen that was clicked on
 * @param[out] pos vec2_t was filled with longitude and latitude
 */
static void MAP_ScreenToMap (const uiNode_t* node, int x, int y, vec2_t pos)
{
	pos[0] = (((ccs.mapPos[0] - x) / ccs.mapSize[0] + 0.5) / ccs.zoom - (ccs.center[0] - 0.5)) * 360.0;
	pos[1] = (((ccs.mapPos[1] - y) / ccs.mapSize[1] + 0.5) / ccs.zoom - (ccs.center[1] - 0.5)) * 180.0;

	while (pos[0] > 180.0)
		pos[0] -= 360.0;
	while (pos[0] < -180.0)
		pos[0] += 360.0;
}

/**
 * @brief Return longitude and latitude of a point of the screen for 3D geoscape (globe)
 * @param[in] node The current menuNode we have clicked on (3dmap or map)
 * @param[in] x X coordinate on the screen that was clicked on
 * @param[in] y Y coordinate on the screen that was clicked on
 * @param[out] pos vec2_t was filled with longitude and latitude
 * @sa MAP_3DMapToScreen
 */
static void MAP3D_ScreenToMap (const uiNode_t* node, int x, int y, vec2_t pos)
{
	vec2_t mid;
	vec3_t v, v1, rotationAxis;
	float dist;
	const float radius = GLOBE_RADIUS;

	/* set mid to the coordinates of the center of the globe */
	Vector2Set(mid, ccs.mapPos[0] + ccs.mapSize[0] / 2.0f, ccs.mapPos[1] + ccs.mapSize[1] / 2.0f);

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
	 *  (because the point is on the globe) */
	v[0] = - (y - mid[1]);
	v[1] = - (x - mid[0]);
	v[2] = - sqrt(radius * radius - (x - mid[0]) * (x - mid[0]) - (y - mid[1]) * (y - mid[1]));
	VectorNormalize(v);

	/* rotate the vector to switch of reference frame
	 * note the ccs.angles[ROLL] is always 0, so there is only 2 rotations and not 3
	 * and that GLOBE_ROTATE is already included in ccs.angles[YAW]
	 * first rotation is along the horizontal axis of the screen, to put north-south axis of the earth
	 * perpendicular to the screen */
	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v1, rotationAxis, v, ccs.angles[YAW]);

	/* second rotation is to rotate the earth around its north-south axis
	 * so that Greenwich meridian is along the vertical axis of the screen */
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v, rotationAxis, v1, ccs.angles[PITCH]);

	/* we therefore got in v the coordinates of the point in the static frame of the earth
	 * that we can convert in polar coordinates to get its latitude and longitude */
	VecToPolar(v, pos);
}

/**
 * @brief Calculate the shortest way to go from start to end on a sphere
 * @param[in] start The point you start from
 * @param[in] end The point you go to
 * @param[out] line Contains the shortest path to go from start to end
 * @sa MAP_MapDrawLine
 */
void MAP_MapCalcLine (const vec2_t start, const vec2_t end, mapline_t* line)
{
	vec3_t s, e, v;
	vec3_t normal;
	vec2_t trafo, sa, ea;
	float cosTrafo, sinTrafo;
	float phiStart, phiEnd, dPhi, phi;
	float *p;
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
	p = NULL;
	for (phi = phiStart, i = 0; i <= n; phi += dPhi, i++) {
		const float *last = p;
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
 * @sa MAP_MapCalcLine
 */
static void MAP_MapDrawLine (const uiNode_t* node, const mapline_t* line)
{
	const vec4_t color = {1, 0.5, 0.5, 1};
	screenPoint_t pts[LINE_MAXPTS];
	screenPoint_t *p;
	int i, start, old;

	/* draw */
	R_Color(color);
	start = 0;
	old = ccs.mapSize[0] / 2;
	for (i = 0, p = pts; i < line->numPoints; i++, p++) {
		MAP_MapToScreen(node, line->point[i], &p->x, &p->y);

		/* If we cross longitude 180 degree (right/left edge of the screen), draw the first part of the path */
		if (i > start && abs(p->x - old) > ccs.mapSize[0] / 2) {
			/* shift last point */
			int diff;

			if (p->x - old > ccs.mapSize[0] / 2)
				diff = -ccs.mapSize[0] * ccs.zoom;
			else
				diff = ccs.mapSize[0] * ccs.zoom;
			p->x += diff;

			/* wrap around screen border */
			R_DrawLineStrip(i - start, (int*)(&pts));

			/* first path of the path is drawn, now we begin the second part of the path */
			/* shift first point, continue drawing */
			start = i;
			pts[0].x = p[-1].x - diff;
			pts[0].y = p[-1].y;
			p = pts;
		}
		old = p->x;
	}

	R_DrawLineStrip(i - start, (int*)(&pts));
	R_Color(NULL);
}

/**
 * @brief Draw a path on a menu node (usually the 3Dgeoscape map)
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the 3Dgeoscape menu node.
 * @param[in] line The path which is to be drawn
 * @sa MAP_MapCalcLine
 */
static void MAP_3DMapDrawLine (const uiNode_t* node, const mapline_t* line)
{
	const vec4_t color = {1, 0.5, 0.5, 1};
	screenPoint_t pts[LINE_MAXPTS];
	int i, numPoints, start;

	start = 0;

	/* draw only when the point of the path is visible */
	R_Color(color);
	for (i = 0, numPoints = 0; i < line->numPoints; i++) {
		if (MAP_3DMapToScreen(node, line->point[i], &pts[i].x, &pts[i].y, NULL))
			numPoints++;
		else if (!numPoints)
			/* the point which is not drawn is at the beginning of the path */
			start++;
	}

	R_DrawLineStrip(numPoints, (int*)(&pts[start]));
	R_Color(NULL);
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
void MAP_MapDrawEquidistantPoints (const uiNode_t* node, const vec2_t center, const float angle, const vec4_t color)
{
	int i, xCircle, yCircle;
	screenPoint_t pts[CIRCLE_DRAW_POINTS + 1];
	vec2_t posCircle;
	qboolean oldDraw = qfalse;
	int numPoints = 0;
	vec3_t initialVector, rotationAxis, currentPoint, centerPos;

	R_Color(color);

	/* Set centerPos corresponding to cartesian coordinates of the center point */
	PolarToVec(center, centerPos);

	/* Find a perpendicular vector to centerPos, and rotate centerPos around it to obtain one point distant of angle from centerPos */
	PerpendicularVector(rotationAxis, centerPos);
	RotatePointAroundVector(initialVector, rotationAxis, centerPos, angle);

	/* Now, each equidistant point is given by a rotation around centerPos */
	for (i = 0; i <= CIRCLE_DRAW_POINTS; i++) {
		qboolean draw = qfalse;
		const float degrees = i * 360.0f / (float)CIRCLE_DRAW_POINTS;
		RotatePointAroundVector(currentPoint, centerPos, initialVector, degrees);
		VecToPolar(currentPoint, posCircle);
		if (MAP_AllMapToScreen(node, posCircle, &xCircle, &yCircle, NULL)) {
			draw = qtrue;
			if (!cl_3dmap->integer && numPoints != 0 && abs(pts[numPoints - 1].x - xCircle) > 512)
				oldDraw = qfalse;
		}

		/* if moving from a point of the screen to a distant one, draw the path we already calculated, and begin a new path
		 * (to avoid unwanted lines) */
		if (draw != oldDraw && i != 0) {
			R_DrawLineStrip(numPoints, (int*)(&pts));
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
	R_DrawLineStrip(numPoints, (int*)(&pts));
	R_Color(NULL);
}

/**
 * @brief Return the angle of a model given its position and destination, on 3D geoscape.
 * @param[in] start Latitude and longitude of the position of the model.
 * @param[in] end Latitude and longitude of aimed point.
 * @param[in] direction vec3_t giving current direction of the model (NULL if the model is idle).
 * @param[out] ortVector If not NULL, this will be filled with the normalized vector around which rotation allows to go toward @c direction.
 * @return Angle (degrees) of rotation around the radius axis of earth for @c start going toward @c end. Zero value is the direction of North pole.
 */
static float MAP_AngleOfPath3D (const vec3_t start, const vec2_t end, vec3_t direction, vec3_t ortVector)
{
	float angle = 0.0f;
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
		float dist;
		VectorSubtract(ortToDest, direction, v);
		dist = VectorLength(v);
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
	angle = todeg * acos(DotProduct(ortToDest, ortToPole));
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
 * @param[in] direction vec3_t giving current direction of the model (NULL if the model is idle).
 * @param[out] ortVector If not NULL, this will be filled with the normalized vector around which rotation allows to go toward @c direction.
 * @return Angle (degrees) of rotation around the radius axis of earth for @c start going toward @c end. Zero value is the direction of North pole.
 */
static float MAP_AngleOfPath2D (const vec3_t start, const vec2_t end, vec3_t direction, vec3_t ortVector)
{
	float angle = 0.0f;
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
		float dist;
		VectorSubtract(tangentVector, direction, v);
		dist = VectorLength(v);
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
	angle = todeg * atan(tangentVector[0] / tangentVector[1]);
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
 * @param[in] direction vec3_t giving current direction of the model (NULL if the model is idle).
 * @param[out] ortVector If not NULL, this will be filled with the normalized vector around which rotation allows to go toward @c direction.
 * @return Angle (degrees) of rotation around the radius axis of earth for @c start going toward @c end. Zero value is the direction of North pole.
 */
float MAP_AngleOfPath (const vec3_t start, const vec2_t end, vec3_t direction, vec3_t ortVector)
{
	float angle;

	if (cl_3dmap->integer)
		angle = MAP_AngleOfPath3D(start, end, direction, ortVector);
	else
		angle = MAP_AngleOfPath2D(start, end, direction, ortVector);

	return angle;
}

/**
 * @brief Will set the vector for the geoscape position
 * @param[out] vector The output vector. A two-dim vector for the flat geoscape, and a three-dim vector for the 3d geoscape
 * @param[in] objectPos The position vector of the object to transform.
 */
static void MAP_ConvertObjectPositionToGeoscapePosition (float* vector, const vec2_t objectPos)
{
	if (cl_3dmap->integer)
		VectorSet(vector, objectPos[0], -objectPos[1], 0);
	else
		Vector2Set(vector, objectPos[0], objectPos[1]);
}

/**
 * @brief center to a mission
 */
static void MAP_GetMissionAngle (float *vector, int id)
{
	mission_t *mission = MAP_GetMissionByIDX(id);
	if (mission == NULL)
		return;
	MAP_ConvertObjectPositionToGeoscapePosition(vector, mission->pos);
	MAP_SelectMission(mission);
}

/**
 * @brief center to a ufo
 */
static void MAP_GetUFOAngle (float *vector, int idx)
{
	aircraft_t *ufo;

	/* Cycle through UFOs (only those visible on geoscape) */
	ufo = NULL;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != NULL) {
		if (ufo->idx != idx)
			continue;
		MAP_ConvertObjectPositionToGeoscapePosition(vector, ufo->pos);
		MAP_SelectUFO(ufo);
		return;
	}
}


/**
 * @brief Start center to the selected point
 * @sa MAP_GetGeoscapeAngle
 * @sa MAP_DrawMap
 * @sa MAP3D_SmoothRotate
 * @sa MAP_SmoothTranslate
 */
static void MAP_StartCenter (void)
{
	if (cl_3dmap->integer) {
		/* case 3D geoscape */
		vec3_t diff;

		ccs.smoothFinalGlobeAngle[1] += GLOBE_ROTATE;
		VectorSubtract(ccs.smoothFinalGlobeAngle, ccs.angles, diff);
		ccs.smoothDeltaLength = VectorLength(diff);
	} else {
		/* case 2D geoscape */
		vec2_t diff;

		Vector2Set(ccs.smoothFinal2DGeoscapeCenter, 0.5f - ccs.smoothFinal2DGeoscapeCenter[0] / 360.0f, 0.5f - ccs.smoothFinal2DGeoscapeCenter[1] / 180.0f);
		if (ccs.smoothFinal2DGeoscapeCenter[1] < 0.5 / ZOOM_LIMIT)
			ccs.smoothFinal2DGeoscapeCenter[1] = 0.5 / ZOOM_LIMIT;
		if (ccs.smoothFinal2DGeoscapeCenter[1] > 1.0 - 0.5 / ZOOM_LIMIT)
			ccs.smoothFinal2DGeoscapeCenter[1] = 1.0 - 0.5 / ZOOM_LIMIT;
		diff[0] = ccs.smoothFinal2DGeoscapeCenter[0] - ccs.center[0];
		diff[1] = ccs.smoothFinal2DGeoscapeCenter[1] - ccs.center[1];
		ccs.smoothDeltaLength = sqrt(diff[0] * diff[0] + diff[1] * diff[1]);
	}

	ccs.smoothFinalZoom = ZOOM_LIMIT;
	ccs.smoothDeltaZoom = fabs(ccs.smoothFinalZoom - ccs.zoom);
	ccs.smoothRotation = qtrue;
}

/**
 * @brief Center the view and select an object from the geoscape
 */
static void MAP_SelectObject_f (void)
{
	const char *type;
	int idx;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <mission|ufo> <id>\n", Cmd_Argv(0));
		return;
	}

	type = Cmd_Argv(1);
	idx = atoi(Cmd_Argv(2));

	if (cl_3dmap->integer) {
		if (Q_streq(type, "mission"))
			MAP_GetMissionAngle(ccs.smoothFinalGlobeAngle, idx);
		else if (Q_streq(type, "ufo"))
			MAP_GetUFOAngle(ccs.smoothFinalGlobeAngle, idx);
		else {
			Com_Printf("MAP_SelectObject_f: type %s unsupported.", type);
			return;
		}
	} else {
		if (Q_streq(type, "mission"))
			MAP_GetMissionAngle(ccs.smoothFinal2DGeoscapeCenter, idx);
		else if (Q_streq(type, "ufo"))
			MAP_GetUFOAngle(ccs.smoothFinal2DGeoscapeCenter, idx);
		else {
			Com_Printf("MAP_SelectObject_f: type %s unsupported.", type);
			return;
		}
	}
	MAP_StartCenter();
}

/**
 * @brief Returns position of the model corresponding to centerOnEventIdx.
 * @param[out] vector Latitude and longitude of the model (finalAngle[2] is always 0).
 * @note Vector is a vec3_t if cl_3dmap is true, and a vec2_t if cl_3dmap is false.
 * @sa MAP_CenterOnPoint
 */
static void MAP_GetGeoscapeAngle (float *vector)
{
	int counter = 0;
	int maxEventIdx;
	const int numMissions = CP_CountMissionOnGeoscape();
	aircraft_t *ufo;
	base_t *base;
	int numBases = B_GetCount();

	/* If the value of maxEventIdx is too big or to low, restart from beginning */
	maxEventIdx = numMissions + numBases + INS_GetCount() - 1;
	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		AIR_ForeachFromBase(aircraft, base) {
			if (AIR_IsAircraftOnGeoscape(aircraft))
				maxEventIdx++;
		}
	}
	ufo = NULL;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != NULL)
		maxEventIdx++;

	/* if there's nothing to center the view on, just go to 0,0 pos */
	if (maxEventIdx < 0) {
		MAP_ConvertObjectPositionToGeoscapePosition(vector, vec2_origin);
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
				MAP_ConvertObjectPositionToGeoscapePosition(vector, mission->pos);
				MAP_SelectMission(mission);
				return;
			}
			counter++;
		}
	}
	counter = numMissions;

	/* Cycle through bases */
	if (centerOnEventIdx < numBases + counter) {
		base = NULL;
		while ((base = B_GetNext(base)) != NULL) {
			if (counter == centerOnEventIdx) {
				MAP_ConvertObjectPositionToGeoscapePosition(vector, base->pos);
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
				MAP_ConvertObjectPositionToGeoscapePosition(vector, inst->pos);
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
				MAP_ConvertObjectPositionToGeoscapePosition(vector, aircraft->pos);
				MAP_SelectAircraft(aircraft);
				return;
			}
			counter++;
		}
	}

	/* Cycle through UFOs (only those visible on geoscape) */
	ufo = NULL;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != NULL) {
		if (centerOnEventIdx == counter) {
			MAP_ConvertObjectPositionToGeoscapePosition(vector, ufo->pos);
			MAP_SelectUFO(ufo);
			return;
		}
		counter++;
	}
}

/**
 * @brief Switch to next model on 2D and 3D geoscape.
 * @note Set @c smoothRotation to @c qtrue to allow a smooth rotation in MAP_DrawMap.
 * @note This function sets the value of smoothFinalGlobeAngle (for 3D) or smoothFinal2DGeoscapeCenter (for 2D),
 *  which contains the final value that ccs.angles or ccs.centre must respectively take.
 * @sa MAP_GetGeoscapeAngle
 * @sa MAP_DrawMap
 * @sa MAP3D_SmoothRotate
 * @sa MAP_SmoothTranslate
 */
void MAP_CenterOnPoint_f (void)
{
	if (!Q_streq(UI_GetActiveWindowName(), "geoscape"))
		return;

	centerOnEventIdx++;

	if (cl_3dmap->integer)
		MAP_GetGeoscapeAngle(ccs.smoothFinalGlobeAngle);
	else
		MAP_GetGeoscapeAngle(ccs.smoothFinal2DGeoscapeCenter);
	MAP_StartCenter();
}

/**
 * @brief smooth rotation of the 3D geoscape
 * @note updates slowly values of ccs.angles and ccs.zoom so that its value goes to smoothFinalGlobeAngle
 * @sa MAP_DrawMap
 * @sa MAP_CenterOnPoint
 */
static void MAP3D_SmoothRotate (void)
{
	vec3_t diff;
	const float diffZoom = ccs.smoothFinalZoom - ccs.zoom;

	VectorSubtract(ccs.smoothFinalGlobeAngle, ccs.angles, diff);

	if (ccs.smoothDeltaLength > ccs.smoothDeltaZoom) {
		/* when we rotate (and zoom) */
		const float diffAngle = VectorLength(diff);
		const float epsilon = 0.1f;
		if (diffAngle > epsilon) {
			float rotationSpeed;
			/* Append the old speed to the new speed if this is the first half of a new rotation, but never exceed the max speed.
			 * This allows the globe to rotate at maximum speed when the button is held down. */
			if (diffAngle / ccs.smoothDeltaLength > 0.5)
				rotationSpeed = min(diffAngle, ccs.curRotationSpeed + sin(3.05f * diffAngle / ccs.smoothDeltaLength) * diffAngle * 0.5);
			else
				rotationSpeed = sin(3.05f * diffAngle / ccs.smoothDeltaLength) * diffAngle;
			ccs.curRotationSpeed = rotationSpeed;
			VectorScale(diff, smoothAcceleration / diffAngle * rotationSpeed, diff);
			VectorAdd(ccs.angles, diff, ccs.angles);
			ccs.zoom = ccs.zoom + smoothAcceleration * diffZoom / diffAngle * rotationSpeed;
			return;
		}
	} else {
		const float epsilonZoom = 0.01f;
		/* when we zoom only */
		if (fabs(diffZoom) > epsilonZoom) {
			float speed;
			/* Append the old speed to the new speed if this is the first half of a new zoom operation, but never exceed the max speed.
			 * This allows the globe to zoom at maximum speed when the button is held down. */
			if (fabs(diffZoom) / ccs.smoothDeltaZoom > 0.5)
				speed = min(smoothAcceleration * 2.0, ccs.curZoomSpeed + sin(3.05f * (fabs(diffZoom) / ccs.smoothDeltaZoom)) * smoothAcceleration);
			else {
				speed = sin(3.05f * (fabs(diffZoom) / ccs.smoothDeltaZoom)) * smoothAcceleration * 2.0;
			}
			ccs.curZoomSpeed = speed;
			ccs.zoom = ccs.zoom + diffZoom * speed;
			return;
		}
	}

	/* if we reach this point, that means that movement is over */
	VectorCopy(ccs.smoothFinalGlobeAngle, ccs.angles);
	ccs.smoothRotation = qfalse;
	ccs.zoom = ccs.smoothFinalZoom;
}

/**
 * @brief stop smooth translation on geoscape
 * @sa UI_RightClick
 * @sa UI_MouseWheel
 */
void MAP_StopSmoothMovement (void)
{
	ccs.smoothRotation = qfalse;
}

#define SMOOTHING_STEP_2D	0.02f
/**
 * @brief smooth translation of the 2D geoscape
 * @note updates slowly values of ccs.center so that its value goes to smoothFinal2DGeoscapeCenter
 * @note updates slowly values of ccs.zoom so that its value goes to ZOOM_LIMIT
 * @sa MAP_DrawMap
 * @sa MAP_CenterOnPoint
 */
static void MAP_SmoothTranslate (void)
{
	const float dist1 = ccs.smoothFinal2DGeoscapeCenter[0] - ccs.center[0];
	const float dist2 = ccs.smoothFinal2DGeoscapeCenter[1] - ccs.center[1];
	const float length = sqrt(dist1 * dist1 + dist2 * dist2);

	if (length < SMOOTHING_STEP_2D) {
		ccs.center[0] = ccs.smoothFinal2DGeoscapeCenter[0];
		ccs.center[1] = ccs.smoothFinal2DGeoscapeCenter[1];
		ccs.zoom = ccs.smoothFinalZoom;
		ccs.smoothRotation = qfalse;
	} else {
		const float diffZoom = ccs.smoothFinalZoom - ccs.zoom;
		ccs.center[0] = ccs.center[0] + SMOOTHING_STEP_2D * dist1 / length;
		ccs.center[1] = ccs.center[1] + SMOOTHING_STEP_2D * dist2 / length;
		ccs.zoom = ccs.zoom + SMOOTHING_STEP_2D * diffZoom;
	}
}

#define BULLET_SIZE	1
/**
 * @brief Draws a bunch of bullets on the geoscape map
 * @param[in] node Pointer to the node in which you want to draw the bullets.
 * @param[in] pos
 * @sa MAP_DrawMap
 */
static void MAP_DrawBullets (const uiNode_t* node, const vec3_t pos)
{
	int x, y;

	if (MAP_AllMapToScreen(node, pos, &x, &y, NULL))
		R_DrawFill(x, y, BULLET_SIZE, BULLET_SIZE, yellow);
}

/**
 * @brief Draws a energy beam on the geoscape map (laser/particle)
 * @param[in] node Pointer to the node in which you want to draw.
 * @param[in] start Start position of the shot (on geoscape)
 * @param[in] end End position of the shot (on geoscape)
 * @param[in] color color of the beam
 * @sa MAP_DrawMap
 */
static void MAP_DrawBeam (const uiNode_t* node, const vec3_t start, const vec3_t end, const vec4_t color)
{
	int points[4];

	if (!MAP_AllMapToScreen(node, start, &(points[0]), &(points[1]), NULL))
		return;
	if (!MAP_AllMapToScreen(node, end, &(points[2]), &(points[3]), NULL))
		return;

	R_Color(color);
	R_DrawLine(points, 2.0);
	R_Color(NULL);
}

#define SELECT_CIRCLE_RADIUS	1.5f + 3.0f / ccs.zoom

/**
 * @brief Draws one mission on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] ms Pointer to the mission to draw.
 */
static void MAP_DrawMapOneMission (const uiNode_t* node, const mission_t *ms)
{
	int x, y;
	const qboolean isCurrentSelectedMission = MAP_IsMissionSelected(ms);

	if (isCurrentSelectedMission)
		Cvar_Set("mn_mapdaytime", MAP_IsNight(ms->pos) ? _("Night") : _("Day"));

	if (!MAP_AllMapToScreen(node, ms->pos, &x, &y, NULL))
		return;

	if (isCurrentSelectedMission) {
		/* Draw circle around the mission */
		if (cl_3dmap->integer) {
			if (!ms->active)
				MAP_MapDrawEquidistantPoints(node, ms->pos, SELECT_CIRCLE_RADIUS, yellow);
		} else {
			const image_t *image;
			if (ms->active) {
				image = geoscapeImages[GEOSCAPE_IMAGE_MISSION_ACTIVE];
			} else {
				image = geoscapeImages[GEOSCAPE_IMAGE_MISSION_SELECTED];
			}
			R_DrawImage(x - image->width / 2, y - image->height / 2, image);
		}
	}

	/* Draw mission model (this must be called after drawing the selection circle so that the model is rendered on top of it)*/
	if (cl_3dmap->integer) {
		MAP_Draw3DMarkerIfVisible(node, ms->pos, defaultBaseAngle, MAP_GetMissionModel(ms), 0);
	} else {
		const image_t *image = geoscapeImages[GEOSCAPE_IMAGE_MISSION];
		R_DrawImage(x - image->width / 2, y - image->height / 2, image);
	}

	UI_DrawString("f_verysmall", ALIGN_UL, x + 10, y, 0, 0, 0,  _(ms->location), 0, 0, NULL, qfalse, 0);
}

/**
 * @brief Draws one installation on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] installation Pointer to the installation to draw.
 * @param[in] oneUFOVisible Is there at least one UFO visible on the geoscape?
 * @param[in] font Default font.
 * @pre installation is not NULL.
 */
static void MAP_DrawMapOneInstallation (const uiNode_t* node, const installation_t *installation,
	qboolean oneUFOVisible, const char* font)
{
	const installationTemplate_t *tpl = installation->installationTemplate;
	int x, y;

	/* Draw weapon range if at least one UFO is visible */
	if (oneUFOVisible && AII_InstallationCanShoot(installation)) {
		int i;
		for (i = 0; i < tpl->maxBatteries; i++) {
			const aircraftSlot_t const *slot = &installation->batteries[i].slot;
			if (slot->item && slot->ammoLeft != 0 && slot->installationTime == 0) {
				MAP_MapDrawEquidistantPoints(node, installation->pos,
					slot->ammo->craftitem.stats[AIR_STATS_WRANGE], red);
			}
		}
	}

	/* Draw installation radar (only the "wire" style part) */
	if (MAP_IsRadarOverlayActivated())
		RADAR_DrawInMap(node, &installation->radar, installation->pos);

	/* Draw installation */
	if (cl_3dmap->integer) {
		MAP_Draw3DMarkerIfVisible(node, installation->pos, defaultBaseAngle, tpl->model, 0);
	} else if (MAP_MapToScreen(node, installation->pos, &x, &y)) {
		const image_t *image = R_FindImage(tpl->image, it_pic);
		if (image)
			R_DrawImage(x - image->width / 2, y - image->height / 2, image);
	}

	/* Draw installation names */
	if (MAP_AllMapToScreen(node, installation->pos, &x, &y, NULL))
		UI_DrawString(font, ALIGN_UL, x, y + 10, 0, 0, 0, installation->name, 0, 0, NULL, qfalse, 0);
}

/**
 * @brief Draws one base on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] base Pointer to the base to draw.
 * @param[in] oneUFOVisible Is there at least one UFO visible on the geoscape?
 * @param[in] font Default font.
 */
static void MAP_DrawMapOneBase (const uiNode_t* node, const base_t *base,
	qboolean oneUFOVisible, const char* font)
{
	int x, y;

	/* Draw weapon range if at least one UFO is visible */
	if (oneUFOVisible && AII_BaseCanShoot(base)) {
		int i;
		for (i = 0; i < base->numBatteries; i++) {
			const aircraftSlot_t const *slot = &base->batteries[i].slot;
			if (slot->item && slot->ammoLeft != 0 && slot->installationTime == 0) {
				MAP_MapDrawEquidistantPoints(node, base->pos,
					slot->ammo->craftitem.stats[AIR_STATS_WRANGE], red);
			}
		}
		for (i = 0; i < base->numLasers; i++) {
			const aircraftSlot_t const *slot = &base->lasers[i].slot;
			if (slot->item && slot->ammoLeft != 0 && slot->installationTime == 0) {
				MAP_MapDrawEquidistantPoints(node, base->pos,
					slot->ammo->craftitem.stats[AIR_STATS_WRANGE], red);
			}
		}
	}

	/* Draw base radar (only the "wire" style part) */
	if (MAP_IsRadarOverlayActivated())
		RADAR_DrawInMap(node, &base->radar, base->pos);

	/* Draw base */
	if (cl_3dmap->integer) {
		if (B_IsUnderAttack(base))
			/* two skins - second skin is for baseattack */
			MAP_Draw3DMarkerIfVisible(node, base->pos, defaultBaseAngle, "geoscape/base", 1);
		else
			MAP_Draw3DMarkerIfVisible(node, base->pos, defaultBaseAngle, "geoscape/base", 0);
	} else if (MAP_MapToScreen(node, base->pos, &x, &y)) {
		const image_t *image;
		if (B_IsUnderAttack(base))
			image = geoscapeImages[GEOSCAPE_IMAGE_BASE_ATTACK];
		else
			image = geoscapeImages[GEOSCAPE_IMAGE_BASE];

		R_DrawImage(x - image->width / 2, y - image->height / 2, image);
	}

	/* Draw base names */
	if (MAP_AllMapToScreen(node, base->pos, &x, &y, NULL))
		UI_DrawString(font, ALIGN_UL, x, y + 10, 0, 0, 0, base->name, 0, 0, NULL, qfalse, 0);
}

/**
 * @brief Draws health bar for an aircraft (either phalanx or ufo)
 * @param[in] node Pointer to the meunode to draw in
 * @param[in] aircraft Pointer to the aircraft to draw for
 * @note if max health (AIR_STATS_DAMAGE) <= 0 no healthbar drawn
 */
static void MAP_DrawAircraftHealthBar (const uiNode_t* node, const aircraft_t *aircraft)
{
	const vec4_t bordercolor = {1, 1, 1, 1};
	const int width = 8 * ccs.zoom;
	const int height = 1 * ccs.zoom * 0.9;
	vec4_t color;
	int centerX;
	int centerY;
	qboolean visible;

	if (!aircraft)
		return;
	if (aircraft->stats[AIR_STATS_DAMAGE] <= 0)
		return;

	if (((float)aircraft->damage / aircraft->stats[AIR_STATS_DAMAGE]) <= .33) {
		Vector4Copy(red, color);
	} else if (((float)aircraft->damage / aircraft->stats[AIR_STATS_DAMAGE]) <= .75) {
		Vector4Copy(yellow, color);
	} else {
		Vector4Copy(green, color);
	}

	if (cl_3dmap->integer)
		visible = MAP_3DMapToScreen(node, aircraft->pos, &centerX, &centerY, NULL);
	else
		visible = MAP_AllMapToScreen(node, aircraft->pos, &centerX, &centerY, NULL);

	if (visible) {
		R_DrawFill(centerX - width / 2 , centerY - 5 * ccs.zoom, round(width * ((float)aircraft->damage / aircraft->stats[AIR_STATS_DAMAGE])), height, color);
		R_DrawRect(centerX - width / 2, centerY - 5 * ccs.zoom, width, height, bordercolor, 1.0, 1);
	}
}

/**
 * @brief Draws one Phalanx aircraft on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] aircraft Pointer to the aircraft to draw.
 * @param[in] oneUFOVisible Is there at least one UFO visible on the geoscape?
 */
static void MAP_DrawMapOnePhalanxAircraft (const uiNode_t* node, aircraft_t *aircraft, qboolean oneUFOVisible)
{
	float angle;

	/* Draw aircraft radar (only the "wire" style part) */
	if (MAP_IsRadarOverlayActivated())
		RADAR_DrawInMap(node, &aircraft->radar, aircraft->pos);

	/* Draw only the bigger weapon range on geoscape: more detail will be given on airfight map */
	if (oneUFOVisible)
		MAP_MapDrawEquidistantPoints(node, aircraft->pos, aircraft->stats[AIR_STATS_WRANGE] / 1000.0f, red);

	/* Draw aircraft route */
	if (aircraft->status >= AIR_TRANSIT) {
		/* aircraft is moving */
		mapline_t path;

		path.numPoints = aircraft->route.numPoints - aircraft->point;
		/** @todo : check why path.numPoints can be sometime equal to -1 */
		if (path.numPoints > 1) {
			memcpy(path.point, aircraft->pos, sizeof(vec2_t));
			memcpy(path.point + 1, aircraft->route.point + aircraft->point + 1, (path.numPoints - 1) * sizeof(vec2_t));
			if (cl_3dmap->integer)
				MAP_3DMapDrawLine(node, &path);
			else
				MAP_MapDrawLine(node, &path);
		}
		angle = MAP_AngleOfPath(aircraft->pos, aircraft->route.point[aircraft->route.numPoints - 1], aircraft->direction, NULL);
	} else {
		/* aircraft is idle */
		angle = 0.0f;
	}

	/* Draw a circle around selected aircraft */
	if (MAP_IsAircraftSelected(aircraft)) {
		const image_t *image = geoscapeImages[GEOSCAPE_IMAGE_MISSION_SELECTED];
		int x;
		int y;

		if (cl_3dmap->integer)
			MAP_MapDrawEquidistantPoints(node, aircraft->pos, SELECT_CIRCLE_RADIUS, yellow);
		else {
			MAP_AllMapToScreen(node, aircraft->pos, &x, &y, NULL);
			R_DrawImage(x - image->width / 2, y - image->height / 2, image);
		}

		/* Draw a circle around the ufo pursued by selected aircraft */
		if (aircraft->status == AIR_UFO && MAP_AllMapToScreen(node, aircraft->aircraftTarget->pos, &x, &y, NULL)) {
			if (cl_3dmap->integer)
				MAP_MapDrawEquidistantPoints(node, aircraft->aircraftTarget->pos, SELECT_CIRCLE_RADIUS, yellow);
			else
				R_DrawImage(x - image->width / 2, y - image->height / 2, image);
		}
	}

	/* Draw aircraft (this must be called after drawing the selection circle so that the aircraft is drawn on top of it)*/
	MAP_Draw3DMarkerIfVisible(node, aircraft->pos, angle, aircraft->model, 0);

	/** @todo we should only show healthbar if the aircraft is fighting but it's a slow algo */
	if (oneUFOVisible || Cvar_GetInteger("debug_showcrafthealth") >= 1)
		MAP_DrawAircraftHealthBar(node, aircraft);
}

/**
 * @brief Assembles a string for a mission that is on the geoscape
 * @param[in] mission The mission to get the description for
 * @param[out] buffer The target buffer to store the text in
 * @param[in] size The size of the target buffer
 * @return A pointer to the buffer that was given to this function
 * @sa MAP_GetAircraftText
 * @sa MAP_GetUFOText
 */
static const char *MAP_GetMissionText (char *buffer, size_t size, const mission_t *mission)
{
	assert(mission);
	Com_sprintf(buffer, size, _("Location: %s\nType: %s\nObjective: %s"), mission->location,
			CP_MissionToTypeString(mission), (mission->mapDef) ? _(mission->mapDef->description) : _("Unknown"));
	return buffer;
}

/**
 * @brief Assembles a short string for a mission that is on the geoscape
 * @param[in] mission The mission to get the description for
 * @param[out] buffer The target buffer to store the text in
 * @param[in] size The size of the target buffer
 * @return A pointer to the buffer that was given to this function
 * @sa MAP_GetAircraftText
 * @sa MAP_GetUFOText
 */
static const char *MAP_GetShortMissionText (char *buffer, size_t size, const mission_t *mission)
{
	assert(mission);
	Com_sprintf(buffer, size, _("%s (%s)\n%s"),
			CP_MissionToTypeString(mission),
			mission->location,
			(mission->mapDef) ? _(mission->mapDef->description) : _("Unknown"));
	return buffer;
}

/**
 * @brief Assembles a string for an aircraft that is on the geoscape
 * @param[in] aircraft The aircraft to get the description for
 * @param[out] buffer The target buffer to store the text in
 * @param[in] size The size of the target buffer
 * @return A pointer to the buffer that was given to this function
 * @sa MAP_GetUFOText
 * @sa MAP_GetMissionText
 */
static const char *MAP_GetAircraftText (char *buffer, size_t size, const aircraft_t *aircraft)
{
	if (aircraft->status == AIR_UFO) {
		const float distance = GetDistanceOnGlobe(aircraft->pos, aircraft->aircraftTarget->pos);
		Com_sprintf(buffer, size, _("Name:\t%s (%i/%i)\n"), aircraft->name, AIR_GetTeamSize(aircraft), aircraft->maxTeamSize);
		Q_strcat(buffer, va(_("Status:\t%s\n"), AIR_AircraftStatusToName(aircraft)), size);
		if (aircraft->stats[AIR_STATS_DAMAGE] > 0)
			Q_strcat(buffer, va(_("Health:\t%3.0f%%\n"), (double)aircraft->damage * 100 / aircraft->stats[AIR_STATS_DAMAGE]), size);
		Q_strcat(buffer, va(_("Distance to target:\t\t%.0f\n"), distance), size);
		Q_strcat(buffer, va(_("Speed:\t%i km/h\n"), AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED)), size);
		Q_strcat(buffer, va(_("Fuel:\t%i/%i\n"), AIR_AircraftMenuStatsValues(aircraft->fuel, AIR_STATS_FUELSIZE),
			AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE)), size);
		Q_strcat(buffer, va(_("ETA:\t%sh\n"), CP_SecondConvert((float)SECONDS_PER_HOUR * distance / aircraft->stats[AIR_STATS_SPEED])), size);
	} else {
		Com_sprintf(buffer, size, _("Name:\t%s (%i/%i)\n"), aircraft->name, AIR_GetTeamSize(aircraft), aircraft->maxTeamSize);
		Q_strcat(buffer, va(_("Status:\t%s\n"), AIR_AircraftStatusToName(aircraft)), size);
		if (aircraft->stats[AIR_STATS_DAMAGE] > 0)
			Q_strcat(buffer, va(_("Health:\t%3.0f%%\n"), (double)aircraft->damage * 100 / aircraft->stats[AIR_STATS_DAMAGE]), size);
		Q_strcat(buffer, va(_("Speed:\t%i km/h\n"), AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED)), size);
		Q_strcat(buffer, va(_("Fuel:\t%i/%i\n"), AIR_AircraftMenuStatsValues(aircraft->fuel, AIR_STATS_FUELSIZE),
			AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE)), size);
		if (aircraft->status != AIR_IDLE) {
			const float distance = GetDistanceOnGlobe(aircraft->pos,
					aircraft->route.point[aircraft->route.numPoints - 1]);
			Q_strcat(buffer, va(_("ETA:\t%sh\n"), CP_SecondConvert((float)SECONDS_PER_HOUR * distance / aircraft->stats[AIR_STATS_SPEED])), size);
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
 * @sa MAP_GetAircraftText
 * @sa MAP_GetMissionText
 */
static const char *MAP_GetUFOText (char *buffer, size_t size, const aircraft_t* ufo)
{
	Com_sprintf(buffer, size, "%s\n", UFO_AircraftToIDOnGeoscape(ufo));
	Q_strcat(buffer, va(_("Speed: %i km/h\n"), AIR_AircraftMenuStatsValues(ufo->stats[AIR_STATS_SPEED], AIR_STATS_SPEED)), size);
	return buffer;
}

/**
 * @brief Will add missions and UFOs to the geoscape dock panel
 */
void MAP_UpdateGeoscapeDock (void)
{
	char buf[512];
	aircraft_t *ufo;

	UI_ExecuteConfunc("clean_geoscape_object");

	/* draw mission pics */
	MIS_Foreach(mission) {
		if (!mission->onGeoscape)
			continue;
		UI_ExecuteConfunc("add_geoscape_object mission %i \"%s\" %s \"%s\"",
				mission->idx, mission->location, MAP_GetMissionModel(mission), MAP_GetShortMissionText(buf, sizeof(buf), mission));
	}

	/* draws ufos */
	ufo = NULL;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != NULL) {
		const unsigned int ufoIDX = UFO_GetGeoscapeIDX(ufo);
		UI_ExecuteConfunc("add_geoscape_object ufo %i %i %s \"%s\"",
				ufoIDX, ufoIDX, ufo->model, MAP_GetUFOText(buf, sizeof(buf), ufo));
	}
}

/**
 * @brief Draws all ufos, aircraft, bases and so on to the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @note This is a drawing function only, called each time a frame is drawn. Therefore
 * you should not use this function to calculate eg. the distance between 2 items on the geoscape
 * (you should instead calculate it just after one of the items moved -- distance is not
 * going to change when you rotate the earth around itself and the time is stopped eg.).
 * @sa MAP_DrawMap
 */
static void MAP_DrawMapMarkers (const uiNode_t* node)
{
	int x, y, i, idx;
	const char* font;
	aircraft_t *ufo;
	base_t *base;

	const vec4_t white = {1.f, 1.f, 1.f, 0.7f};
	qboolean showXVI = qfalse;
	qboolean oneUFOVisible = qfalse;
	static char buffer[512] = "";
	int maxInterpolationPoints;

	assert(node);

	/* font color on geoscape */
	R_Color(node->color);
	/* default font */
	font = UI_GetFontFromNode(node);

	/* check if at least 1 UFO is visible */
	oneUFOVisible = UFO_GetNextOnGeoscape(NULL) != NULL;

	/* draw mission pics */
	Cvar_Set("mn_mapdaytime", "");
	MIS_Foreach(mission) {
		if (!mission->onGeoscape)
			continue;
		MAP_DrawMapOneMission(node, mission);
	}

	/* draw installations */
	INS_Foreach(installation) {
		MAP_DrawMapOneInstallation(node, installation, oneUFOVisible, font);
	}

	/* draw bases */
	base = NULL;
	while ((base = B_GetNext(base)) != NULL)
		MAP_DrawMapOneBase(node, base, oneUFOVisible, font);

	/* draw all aircraft */
	AIR_Foreach(aircraft) {
		if (AIR_IsAircraftOnGeoscape(aircraft))
			MAP_DrawMapOnePhalanxAircraft(node, aircraft, oneUFOVisible);
	}

	/* draws ufos */
	ufo = NULL;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != NULL) {
#ifdef DEBUG
		/* in debug mode you execute set showufos 1 to see the ufos on geoscape */
		if (Cvar_GetInteger("debug_showufos")) {
			/* Draw ufo route */
			if (cl_3dmap->integer)
				MAP_3DMapDrawLine(node, &ufo->route);
			else
				MAP_MapDrawLine(node, &ufo->route);
		} else
#endif
		{
			const float angle = MAP_AngleOfPath(ufo->pos, ufo->route.point[ufo->route.numPoints - 1], ufo->direction, NULL);

			if (cl_3dmap->integer)
				MAP_MapDrawEquidistantPoints(node, ufo->pos, SELECT_CIRCLE_RADIUS, white);
			if (MAP_IsUFOSelected(ufo)) {
				if (cl_3dmap->integer)
					MAP_MapDrawEquidistantPoints(node, ufo->pos, SELECT_CIRCLE_RADIUS, yellow);
				else {
					const image_t *image = geoscapeImages[GEOSCAPE_IMAGE_MISSION_SELECTED];
					MAP_AllMapToScreen(node, ufo->pos, &x, &y, NULL);
					R_DrawImage(x - image->width / 2, y - image->height / 2, image);
				}
			}
			MAP_Draw3DMarkerIfVisible(node, ufo->pos, angle, ufo->model, 0);

			/** @todo we should only show healthbar if aircraft is fighting but it's a slow algo */
			if (RS_IsResearched_ptr(ufo->tech)
			 || Cvar_GetInteger("debug_showcrafthealth") >= 1)
				MAP_DrawAircraftHealthBar(node, ufo);
		}
	}

	if (ccs.gameTimeScale > 0)
		maxInterpolationPoints = floor(1.0f / (ccs.frametime * (float)ccs.gameTimeScale));
	else
		maxInterpolationPoints = 0;

	/* draws projectiles */
	for (idx = 0; idx < ccs.numProjectiles; idx++) {
		aircraftProjectile_t *projectile = &ccs.projectiles[idx];
		vec3_t drawPos = {0, 0, 0};

		if (projectile->hasMoved) {
			projectile->hasMoved = qfalse;
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
			MAP_DrawBullets(node, drawPos);
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

			MAP_DrawBeam(node, start, end, projectile->aircraftItem->craftitem.beamColor);
		} else {
			MAP_Draw3DMarkerIfVisible(node, drawPos, projectile->angle, projectile->aircraftItem->model, 0);
		}
	}

	showXVI = CP_IsXVIResearched();

	/* Draw nation names */
	for (i = 0; i < ccs.numNations; i++) {
		const nation_t *nation = NAT_GetNationByIDX(i);
		if (MAP_AllMapToScreen(node, nation->pos, &x, &y, NULL))
			UI_DrawString("f_verysmall", ALIGN_UC, x , y, 0, 0, 0, _(nation->name), 0, 0, NULL, qfalse, 0);
		if (showXVI) {
			const nationInfo_t *stats = NAT_GetCurrentMonthInfo(nation);
			Q_strcat(buffer, va(_("%s\t%i%%\n"), _(nation->name), stats->xviInfection), sizeof(buffer));
		}
	}
	if (showXVI)
		UI_RegisterText(TEXT_XVI, buffer);
	else
		UI_ResetData(TEXT_XVI);

	R_Color(NULL);
}

/**
 * @brief Draw the geoscape
 * @param[in] node The map menu node
 * @param[in,out] campaign The campaign data structure
 * @sa MAP_DrawMapMarkers
 */
void MAP_DrawMap (const uiNode_t* node, const campaign_t *campaign)
{
	vec2_t pos;
	mission_t *mission;

	/* store these values in ccs struct to be able to handle this even in the input code */
	UI_GetNodeAbsPos(node, pos);
	Vector2Copy(pos, ccs.mapPos);
	Vector2Copy(node->size, ccs.mapSize);
	if (cl_3dmap->integer) {
		/* remove the left padding */
		ccs.mapSize[0] -= UI_MAPEXTRADATACONST(node).paddingRight;
	}

	/* Draw the map and markers */
	if (cl_3dmap->integer) {
		qboolean disableSolarRender = qfalse;
		/** @todo I think this check is wrong; isn't zoom clamped to this value already?
		 *  A value of 3.3 seems about right for me, but this should probably be fixed...*/
#if 0
		if (ccs.zoom > cl_mapzoommax->value)
#else
		if (ccs.zoom > 3.3)
#endif
			disableSolarRender = qtrue;

		R_EnableRenderbuffer(qtrue);

		if (ccs.smoothRotation)
			MAP3D_SmoothRotate();
		R_Draw3DGlobe(ccs.mapPos[0], ccs.mapPos[1], ccs.mapSize[0], ccs.mapSize[1],
				ccs.date.day, ccs.date.sec, ccs.angles, ccs.zoom, campaign->map, disableSolarRender,
				cl_3dmapAmbient->value, MAP_IsNationOverlayActivated(), MAP_IsXVIOverlayActivated(),
				MAP_IsRadarOverlayActivated(), r_xviTexture, r_radarTexture, cl_3dmap->integer != 2);

		MAP_DrawMapMarkers(node);

		R_DrawBloom();
		R_EnableRenderbuffer(qfalse);
	} else {
		/* the last q value for the 2d geoscape night overlay */
		static float lastQ = 0.0f;

		/* the sun is not always in the plane of the equator on earth - calculate the angle the sun is at */
		const float q = (ccs.date.day % DAYS_PER_YEAR + (float)(ccs.date.sec / (SECONDS_PER_HOUR * 6)) / 4) * 2 * M_PI / DAYS_PER_YEAR - M_PI;
		if (ccs.smoothRotation)
			MAP_SmoothTranslate();
		if (lastQ != q) {
			CP_CalcAndUploadDayAndNightTexture(q);
			lastQ = q;
		}
		R_DrawFlatGeoscape(ccs.mapPos[0], ccs.mapPos[1], ccs.mapSize[0], ccs.mapSize[1], (float) ccs.date.sec / SECONDS_PER_DAY,
			ccs.center[0], ccs.center[1], 0.5 / ccs.zoom, campaign->map, MAP_IsNationOverlayActivated(),
			MAP_IsXVIOverlayActivated(), MAP_IsRadarOverlayActivated(), r_dayandnightTexture, r_xviTexture, r_radarTexture);
		MAP_DrawMapMarkers(node);
	}

	mission = MAP_GetSelectedMission();
	/* display text */
	UI_ResetData(TEXT_STANDARD);
	switch (ccs.mapAction) {
	case MA_NEWBASE:
		UI_RegisterText(TEXT_STANDARD, _("Select the desired location of the new base on the map.\n"));
		return;
	case MA_NEWINSTALLATION:
		UI_RegisterText(TEXT_STANDARD, _("Select the desired location of the new installation on the map.\n"));
		return;
	case MA_BASEATTACK:
		if (mission)
			break;
		UI_RegisterText(TEXT_STANDARD, _("Aliens are attacking our base at this very moment.\n"));
		return;
	case MA_INTERCEPT:
		if (mission)
			break;
		UI_RegisterText(TEXT_STANDARD, _("Select ufo or mission on map\n"));
		return;
	case MA_UFORADAR:
		if (mission)
			break;
		UI_RegisterText(TEXT_STANDARD, _("UFO in radar range\n"));
		return;
	case MA_NONE:
		break;
	}

	/* Nothing is displayed yet */
	if (mission) {
		UI_RegisterText(TEXT_STANDARD, MAP_GetMissionText(textStandard, sizeof(textStandard), mission));
	} else if (MAP_GetSelectedAircraft() != NULL) {
		const aircraft_t *aircraft = MAP_GetSelectedAircraft();
		if (AIR_IsAircraftInBase(aircraft)) {
			UI_RegisterText(TEXT_STANDARD, NULL);
			MAP_ResetAction();
			return;
		}
		UI_RegisterText(TEXT_STANDARD, MAP_GetAircraftText(textStandard, sizeof(textStandard), aircraft));
	} else if (MAP_GetSelectedUFO() != NULL) {
		UI_RegisterText(TEXT_STANDARD, MAP_GetUFOText(textStandard, sizeof(textStandard), MAP_GetSelectedUFO()));
	} else {
#ifdef DEBUG
		if (debug_showInterest->integer) {
			static char t[64];
			Com_sprintf(t, lengthof(t), "Interest level: %i\n", ccs.overallInterest);
			UI_RegisterText(TEXT_STANDARD, t);
		} else
#endif
		UI_RegisterText(TEXT_STANDARD, "");
	}
}

/**
 * @brief No more special action on the geoscape
 */
void MAP_ResetAction (void)
{
	/* don't allow a reset when no base is set up */
	if (B_AtLeastOneExists())
		ccs.mapAction = MA_NONE;

	MAP_SetInterceptorAircraft(NULL);
	MAP_SetSelectedMission(NULL);
	MAP_SetSelectedAircraft(NULL);
	MAP_SetSelectedUFO(NULL);

	if (!radarOverlayWasSet)
		MAP_DeactivateOverlay("radar");
}

/**
 * @brief Select the specified ufo on the geoscape
 */
void MAP_SelectUFO (aircraft_t* ufo)
{
	MAP_ResetAction();
	MAP_SetSelectedUFO(ufo);
}

/**
 * @brief Select the specified aircraft on the geoscape
 */
void MAP_SelectAircraft (aircraft_t* aircraft)
{
	MAP_ResetAction();
	MAP_SetSelectedAircraft(aircraft);
}

/**
 * @brief Select the specified mission
 * @param[in] mission Pointer to the mission to select
 * @return pointer to the selected mission
 */
mission_t* MAP_SelectMission (mission_t* mission)
{
	if (!mission || MAP_IsMissionSelected(mission))
		return MAP_GetSelectedMission();
	MAP_ResetAction();
	ccs.mapAction = MA_INTERCEPT;
	MAP_SetSelectedMission(mission);
	return MAP_GetSelectedMission();
}

/**
 * @brief Notify that a mission has been removed
 */
void MAP_NotifyMissionRemoved (const mission_t* mission)
{
	/* Unselect the current selected mission if it's the same */
	if (MAP_IsMissionSelected(mission))
		MAP_ResetAction();

	MAP_UpdateGeoscapeDock();
}

/**
 * @brief Notify that a UFO has been removed
 * @param[in] ufo Pointer to the ufo has been removed
 * @param[in] destroyed True if the UFO has been destroyed, false if it's been only set invisible (landed)
 */
void MAP_NotifyUFORemoved (const aircraft_t* ufo, qboolean destroyed)
{
	MAP_UpdateGeoscapeDock();

	if (MAP_GetSelectedUFO() == NULL)
		return;

	/* Unselect the current selected ufo if it's the same */
	if (MAP_IsUFOSelected(ufo))
		MAP_ResetAction();
	else if (destroyed && ccs.geoscape.selectedUFO > ufo)
		/** @todo convert to linked list */
		ccs.geoscape.selectedUFO--;
}

/**
 * @brief Notify that an aircraft has been removed from game
 * @param[in] aircraft Pointer to the aircraft that has been removed
 */
void MAP_NotifyAircraftRemoved (const aircraft_t* aircraft)
{
	/* Unselect the current selected ufo if its the same */
	if (MAP_IsAircraftSelected(aircraft) || MAP_IsInterceptorSelected(aircraft))
		MAP_ResetAction();
}

/**
 * @brief Translate nation map color to nation
 * @sa MAP_GetColor
 * @param[in] pos Map Coordinates to get the nation from
 * @return returns the nation pointer with the given color on nationPic at given pos
 * @return NULL if no nation with the given color value was found
 * @note The coodinates already have to be transfored to map coordinates via MAP_ScreenToMap
 */
nation_t* MAP_GetNation (const vec2_t pos)
{
	int i;
	const byte* color = MAP_GetColor(pos, MAPTYPE_NATIONS, NULL);
#ifdef PARANOID
	Com_DPrintf(DEBUG_CLIENT, "MAP_GetNation: color value for %.0f:%.0f is r:%i, g:%i, b: %i\n", pos[0], pos[1], color[0], color[1], color[2]);
#endif
	for (i = 0; i < ccs.numNations; i++) {
		nation_t *nation = NAT_GetNationByIDX(i);
		/* compare the first three color values with color value at pos */
		if (VectorCompare(nation->color, color))
			return nation;
	}
	Com_DPrintf(DEBUG_CLIENT, "MAP_GetNation: No nation found at %.0f:%.0f - color: %i:%i:%i\n", pos[0], pos[1], color[0], color[1], color[2]);
	return NULL;
}


/**
 * @brief Translate color value to terrain type
 * @sa MAP_GetColor
 * @param[in] color the color value from the terrain mask
 * @return returns the zone name
 * @note never may return a null pointer or an empty string
 * @note Make sure, that there are textures with the same name in base/textures/tex_terrain
 */
const char* MAP_GetTerrainType (const byte* const color)
{
	if (MapIsDesert(color))
		return "desert";
	else if (MapIsArctic(color))
		return "arctic";
	else if (MapIsWater(color))
		return "water";
	else if (MapIsMountain(color))
		return "mountain";
	else if (MapIsTropical(color))
		return "tropical";
	else if (MapIsCold(color))
		return "cold";
	else if (MapIsWasted(color))
		return "wasted";
	else
		return "grass";
}

/**
 * @brief Translate color value to culture type
 * @sa MAP_GetColor
 * @param[in] color the color value from the culture mask
 * @return returns the zone name
 * @note never may return a null pointer or an empty string
 */
static const char* MAP_GetCultureType (const byte* color)
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
	else
		return "western";
}

/**
 * @brief Translate color value to population type
 * @sa MAP_GetColor
 * @param[in] color the color value from the population mask
 * @return returns the zone name
 * @note never may return a null pointer or an empty string
 */
static const char* MAP_GetPopulationType (const byte* color)
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
	else if (MapIsNopopulation(color))
		return "nopopulation";
	else
		return "nopopulation";
}

/**
 * @brief Determine the terrain type under a given position
 * @sa MAP_GetColor
 * @param[in] pos Map Coordinates to get the terrain type from
 * @return returns the zone name
 */
static inline const char* MAP_GetTerrainTypeByPos (const vec2_t pos, qboolean *coast)
{
	const byte* color = MAP_GetColor(pos, MAPTYPE_TERRAIN, coast);
	return MAP_GetTerrainType(color);
}

/**
 * @brief Determine the culture type under a given position
 * @sa MAP_GetColor
 * @param[in] pos Map Coordinates to get the culture type from
 * @return returns the zone name
 */
static inline const char* MAP_GetCultureTypeByPos (const vec2_t pos)
{
	const byte* color = MAP_GetColor(pos, MAPTYPE_CULTURE, NULL);
	return MAP_GetCultureType(color);
}

/**
 * @brief Determine the population type under a given position
 * @sa MAP_GetColor
 * @param[in] pos Map Coordinates to get the population type from
 * @return returns the zone name
 */
static inline const char* MAP_GetPopulationTypeByPos (const vec2_t pos)
{
	const byte* color = MAP_GetColor(pos, MAPTYPE_POPULATION, NULL);
	return MAP_GetPopulationType(color);
}

/**
 * @brief Get number of civilian on a map at given position.
 * @param[in] pos Position where the mission takes place.
 * @return Number of civilian.
 * @sa CP_CreateCivilianTeam
 */
int MAP_GetCivilianNumberByPosition (const vec2_t pos)
{
	const byte* color = MAP_GetColor(pos, MAPTYPE_POPULATION, NULL);

	if (MapIsWater(color))
		Com_Error(ERR_DROP, "MAP_GetPopulationType: Trying to get number of civilian in a position on water");
	else if (MapIsUrban(color))
		return 10;
	else if (MapIsSuburban(color))
		return 8;
	else if (MapIsVillage(color))
		return 6;
	else if (MapIsRural(color))
		return 4;
	else if (MapIsNopopulation(color))
		return 2;
	else
		return 0;
}

/**
 * @brief Prints positions parameter in console.
 * @param[in] pos Location (latitude, longitude) where you want to check parameters.
 * @note Used for printing in console, do not translate.
 * @sa NAT_ScriptSanityCheck
 */
void MAP_PrintParameterStringByPos (const vec2_t pos)
{
	qboolean coast = qfalse;
	const char *terrainType = MAP_GetTerrainTypeByPos(pos, &coast);
	const char *cultureType = MAP_GetCultureTypeByPos(pos);
	const char *populationType = MAP_GetPopulationTypeByPos(pos);

	Com_Printf ("      (Terrain: %s, Culture: %s, Population: %s, Coast: %s)\n",
			terrainType, cultureType, populationType, coast ? "true" : "false");
}

/**
 * @brief Check that a position (in latitude / longitude) is within boundaries.
 * @param[in,out] pos Pointer to the 2 elements vector giving the position.
 */
void MAP_CheckPositionBoundaries (float *pos)
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
qboolean MAP_IsNight (const vec2_t pos)
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
 * This can be @c NULL if you are not interested in this fact.
 * @return Returns the color value at given position.
 * @note terrainPic, culturePic and populationPic are pointers to an rgba image in memory
 * @sa MAP_GetTerrainType
 * @sa MAP_GetCultureType
 * @sa MAP_GetPopulationType
 */
const byte *MAP_GetColor (const vec2_t pos, mapType_t type, qboolean *coast)
{
	int x, y;
	int width, height;
	const byte *mask;
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
		Com_Error(ERR_DROP, "Unknown maptype %i\n", type);
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
	if (coast != NULL) {
		if (MapIsWater(color)) {
			*coast = qfalse;
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
 * @return Pointer to the base if one base is closer than MIN_DIST_BASE from pos, NULL else
 */
base_t* MAP_PositionCloseToBase (const vec2_t pos)
{
	base_t *base = NULL;
	while ((base = B_GetNext(base)) != NULL)
		if (GetDistanceOnGlobe(pos, base->pos) < MIN_DIST_BASE)
			return base;

	return NULL;
}

/**
 * @brief Checks for a given location, if it fulfills all criteria given via parameters (terrain, culture, population, nation type)
 * @param[in] pos Location to be tested
 * @param[in] terrainTypes A linkedList_t containing a list of strings determining the terrain types to be tested for (e.g. "grass") may be NULL
 * @param[in] cultureTypes A linkedList_t containing a list of strings determining the culture types to be tested for (e.g. "western") may be NULL
 * @param[in] populationTypes A linkedList_t containing a list of strings determining the population types to be tested for (e.g. "suburban") may be NULL
 * @param[in] nations A linkedList_t containing a list of strings determining the nations to be tested for (e.g. "asia") may be NULL
 * @return true if a location was found, otherwise false. If the map is over water, return false
 * @note The name TCPNTypes comes from terrain, culture, population, nation types
 */
qboolean MAP_PositionFitsTCPNTypes (const vec2_t pos, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations)
{
	qboolean coast = qfalse;
	const char *terrainType = MAP_GetTerrainTypeByPos(pos, &coast);
	const char *cultureType = MAP_GetCultureTypeByPos(pos);
	const char *populationType = MAP_GetPopulationTypeByPos(pos);

	if (MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN, NULL)))
		return qfalse;

	if (!terrainTypes || LIST_ContainsString(terrainTypes, terrainType) || (coast && LIST_ContainsString(terrainTypes, "coast"))) {
		if (!cultureTypes || LIST_ContainsString(cultureTypes, cultureType)) {
			if (!populationTypes || LIST_ContainsString(populationTypes, populationType)) {
				const nation_t *nationAtPos = MAP_GetNation(pos);
				if (!nations)
					return qtrue;
				if (nationAtPos && (!nations || LIST_ContainsString(nations, nationAtPos->id))) {
					return qtrue;
				}
			}
		}
	}

	return qfalse;
}

void MAP_Shutdown (void)
{
	if (terrainPic) {
		Mem_Free(terrainPic);
		terrainPic = NULL;
	}

	if (culturePic) {
		Mem_Free(culturePic);
		culturePic = NULL;
	}

	if (populationPic) {
		Mem_Free(populationPic);
		populationPic = NULL;
	}

	if (nationsPic) {
		Mem_Free(nationsPic);
		nationsPic = NULL;
	}
}

void MAP_Init (const char *map)
{
	/* load terrain mask */
	R_LoadImage(va("pics/geoscape/%s_terrain", map), &terrainPic, &terrainWidth, &terrainHeight);
	if (!terrainPic || !terrainWidth || !terrainHeight)
		Com_Error(ERR_DROP, "Couldn't load map mask %s_terrain in pics/geoscape", map);

	/* load culture mask */
	R_LoadImage(va("pics/geoscape/%s_culture", map), &culturePic, &cultureWidth, &cultureHeight);
	if (!culturePic || !cultureWidth || !cultureHeight)
		Com_Error(ERR_DROP, "Couldn't load map mask %s_culture in pics/geoscape", map);

	/* load population mask */
	R_LoadImage(va("pics/geoscape/%s_population", map), &populationPic, &populationWidth, &populationHeight);
	if (!populationPic || !populationWidth || !populationHeight)
		Com_Error(ERR_DROP, "Couldn't load map mask %s_population in pics/geoscape", map);

	/* load nations mask */
	R_LoadImage(va("pics/geoscape/%s_nations", map), &nationsPic, &nationsWidth, &nationsHeight);
	if (!nationsPic || !nationsWidth || !nationsHeight)
		Com_Error(ERR_DROP, "Couldn't load map mask %s_nations in pics/geoscape", map);
}

void MAP_Reset (const char *map)
{
	MAP_Shutdown();
	MAP_Init(map);
	MAP_ResetAction();
	MAP_UpdateGeoscapeDock();
}

/**
 * @brief Notify that a UFO disappears on radars
 */
void MAP_NotifyUFODisappear (const aircraft_t* ufo)
{
	/* Unselect the currently selected ufo if it's the same */
	if (MAP_IsUFOSelected(ufo))
		MAP_ResetAction();

	MAP_UpdateGeoscapeDock();
}

/**
 * @brief Command binding for map zooming
 */
void MAP_Zoom_f (void)
{
	const char *cmd;
	const float zoomAmount = 50.0f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <in|out>\n", Cmd_Argv(0));
		return;
	}

	cmd = Cmd_Argv(1);
	switch (cmd[0]) {
	case 'i':
		ccs.smoothFinalZoom = ccs.zoom * pow(0.995, -zoomAmount);
		break;
	case 'o':
		ccs.smoothFinalZoom = ccs.zoom * pow(0.995, zoomAmount);
		break;
	default:
		Com_Printf("MAP_Zoom_f: Invalid parameter: %s\n", cmd);
		return;
	}

	if (ccs.smoothFinalZoom < cl_mapzoommin->value)
		ccs.smoothFinalZoom = cl_mapzoommin->value;
	else if (ccs.smoothFinalZoom > cl_mapzoommax->value)
		ccs.smoothFinalZoom = cl_mapzoommax->value;

	if (!cl_3dmap->integer) {
		ccs.zoom = ccs.smoothFinalZoom;
		if (ccs.center[1] < 0.5 / ccs.zoom)
			ccs.center[1] = 0.5 / ccs.zoom;
		if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
			ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
	} else {
		VectorCopy(ccs.angles, ccs.smoothFinalGlobeAngle);
		ccs.smoothDeltaLength = 0;
		ccs.smoothRotation = qtrue;
		ccs.smoothDeltaZoom = fabs(ccs.smoothFinalZoom - ccs.zoom);
	}
}

/**
 * @brief Command binding for map scrolling
 */
void MAP_Scroll_f (void)
{
	const char *cmd;
	float scrollX = 0.0f, scrollY = 0.0f;
	const float scrollAmount = 80.0f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <up|down|left|right>\n", Cmd_Argv(0));
		return;
	}

	cmd = Cmd_Argv(1);
	switch (cmd[0]) {
	case 'l':
		scrollX = scrollAmount;
		break;
	case 'r':
		scrollX = -scrollAmount;
		break;
	case 'u':
		scrollY = scrollAmount;
		break;
	case 'd':
		scrollY = -scrollAmount;
		break;
	default:
		Com_Printf("MAP_Scroll_f: Invalid parameter\n");
		return;
	}
	if (cl_3dmap->integer) {
		/* case 3D geoscape */
		vec3_t diff;

		VectorCopy(ccs.angles, ccs.smoothFinalGlobeAngle);

		/* rotate a model */
		ccs.smoothFinalGlobeAngle[PITCH] += ROTATE_SPEED * (scrollX) / ccs.zoom;
		ccs.smoothFinalGlobeAngle[YAW] -= ROTATE_SPEED * (scrollY) / ccs.zoom;

		while (ccs.smoothFinalGlobeAngle[YAW] < -180.0) {
			ccs.smoothFinalGlobeAngle[YAW] = -180.0;
		}
		while (ccs.smoothFinalGlobeAngle[YAW] > 0.0) {
			ccs.smoothFinalGlobeAngle[YAW] = 0.0;
		}

		while (ccs.smoothFinalGlobeAngle[PITCH] > 180.0) {
			ccs.smoothFinalGlobeAngle[PITCH] -= 360.0;
			ccs.angles[PITCH] -= 360.0;
		}
		while (ccs.smoothFinalGlobeAngle[PITCH] < -180.0) {
			ccs.smoothFinalGlobeAngle[PITCH] += 360.0;
			ccs.angles[PITCH] += 360.0;
		}
		VectorSubtract(ccs.smoothFinalGlobeAngle, ccs.angles, diff);
		ccs.smoothDeltaLength = VectorLength(diff);

		ccs.smoothFinalZoom = ccs.zoom;
		ccs.smoothDeltaZoom = 0.0f;
		ccs.smoothRotation = qtrue;
	} else {
		int i;
		/* shift the map */
		ccs.center[0] -= (float) (scrollX) / (ccs.mapSize[0] * ccs.zoom);
		ccs.center[1] -= (float) (scrollY) / (ccs.mapSize[1] * ccs.zoom);
		for (i = 0; i < 2; i++) {
			while (ccs.center[i] < 0.0)
				ccs.center[i] += 1.0;
			while (ccs.center[i] > 1.0)
				ccs.center[i] -= 1.0;
		}
		if (ccs.center[1] < 0.5 / ccs.zoom)
			ccs.center[1] = 0.5 / ccs.zoom;
		if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
			ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
	}
}

/**
 * @brief Switch overlay (turn on / off)
 * @param[in] overlayID Name of the overlay you want to switch.
 */
void MAP_SetOverlay (const char *overlayID)
{
	if (Q_streq(overlayID, "nations")) {
		if (MAP_IsNationOverlayActivated())
			cl_geoscape_overlay->integer ^= OVERLAY_NATION;
		else
			cl_geoscape_overlay->integer |= OVERLAY_NATION;
	}

	/* do nothing while the first base is not build */
	if (!B_AtLeastOneExists())
		return;

	if (Q_streq(overlayID, "xvi")) {
		if (cl_geoscape_overlay->integer & OVERLAY_XVI)
			cl_geoscape_overlay->integer ^= OVERLAY_XVI;
		else
			cl_geoscape_overlay->integer |= OVERLAY_XVI;
	} else if (Q_streq(overlayID, "radar")) {
		if (MAP_IsRadarOverlayActivated())
			cl_geoscape_overlay->integer ^= OVERLAY_RADAR;
		else {
			cl_geoscape_overlay->integer |= OVERLAY_RADAR;
			RADAR_UpdateWholeRadarOverlay();
		}
	}
}

/**
 * @brief Console command to call MAP_SetOverlay.
 */
static void MAP_SetOverlay_f (void)
{
	const char *arg;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <nations|xvi|radar>\n", Cmd_Argv(0));
		return;
	}

	arg = Cmd_Argv(1);
	MAP_SetOverlay(arg);

	/* save last decision player took on radar display, in order to be able to restore it later */
	if (Q_streq(arg, "radar"))
		radarOverlayWasSet = MAP_IsRadarOverlayActivated();
}

/**
 * @brief Remove overlay.
 * @param[in] overlayID Name of the overlay you want to turn off.
 */
void MAP_DeactivateOverlay (const char *overlayID)
{
	if (Q_streq(overlayID, "nations")) {
		if (MAP_IsNationOverlayActivated())
			MAP_SetOverlay("nations");
	} else if (Q_streq(overlayID, "xvi")) {
		if (MAP_IsXVIOverlayActivated())
			MAP_SetOverlay("xvi");
	} else if (Q_streq(overlayID, "radar")) {
		if (MAP_IsRadarOverlayActivated())
			MAP_SetOverlay("radar");
	}
}

/**
 * @brief Console command to call MAP_DeactivateOverlay.
 */
static void MAP_DeactivateOverlay_f (void)
{
	const char *arg;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <nations|xvi|radar>\n", Cmd_Argv(0));
		return;
	}

	arg = Cmd_Argv(1);
	MAP_DeactivateOverlay(arg);
}

qboolean MAP_IsRadarOverlayActivated (void)
{
	return cl_geoscape_overlay->integer & OVERLAY_RADAR;
}

qboolean MAP_IsNationOverlayActivated (void)
{
	return cl_geoscape_overlay->integer & OVERLAY_NATION;
}

qboolean MAP_IsXVIOverlayActivated (void)
{
	return cl_geoscape_overlay->integer & OVERLAY_XVI;
}

/**
 * @brief Initialise MAP/Geoscape
 */
void MAP_InitStartup (void)
{
	int i;

	Cmd_AddCommand("multi_select_click", MAP_MultiSelectExecuteAction_f, NULL);
	Cmd_AddCommand("map_overlay", MAP_SetOverlay_f, "Set the geoscape overlay");
	Cmd_AddCommand("map_deactivateoverlay", MAP_DeactivateOverlay_f, "Deactivate overlay");
	Cmd_AddCommand("map_selectobject", MAP_SelectObject_f, "Select an object and center on it");
	Cmd_AddCommand("mn_mapaction_reset", MAP_ResetAction, NULL);

	cl_geoscape_overlay = Cvar_Get("cl_geoscape_overlay", "0", 0, "Geoscape overlays - Bitmask");
	cl_3dmap = Cvar_Get("cl_3dmap", "1", CVAR_ARCHIVE, "3D geoscape or flat geoscape");
	cl_3dmapAmbient = Cvar_Get("cl_3dmapAmbient", "0", CVAR_ARCHIVE, "3D geoscape ambient lighting factor");
	cl_mapzoommax = Cvar_Get("cl_mapzoommax", "6.0", CVAR_ARCHIVE, "Maximum geoscape zooming value");
	cl_mapzoommin = Cvar_Get("cl_mapzoommin", "1.0", CVAR_ARCHIVE, "Minimum geoscape zooming value");
#ifdef DEBUG
	debug_showInterest = Cvar_Get("debug_showinterest", "0", CVAR_DEVELOPER, "Shows the global interest value on geoscape");
#endif

	for (i = 0; i < GEOSCAPE_IMAGE_MAX; i++) {
		geoscapeImages[i] = R_FindImage(geoscapeImageNames[i], it_pic);
		if (geoscapeImages[i] == r_noTexture)
			Com_Error(ERR_DROP, "Could not find image: %s", geoscapeImageNames[i]);
	}
}
