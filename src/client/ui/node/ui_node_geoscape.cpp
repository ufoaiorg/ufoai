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

#include "../ui_nodes.h"
#include "../ui_input.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_actions.h"
#include "../ui_render.h"
#include "ui_node_geoscape.h"
#include "../../renderer/r_framebuffer.h"
#include "../../renderer/r_geoscape.h"
#include "../../cl_shared.h"
#include "../../cgame/cl_game.h"
#include "../../input/cl_input.h"
#include "../../input/cl_keys.h"

#define EXTRADATA_TYPE mapExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

/**
 * Status of the node
 */
enum mapDragMode_t {
	/**
	 * No interaction
	 */
	MODE_NULL,
	/**
	 * Mouse captured to move the 2D geoscape
	 */
	MODE_SHIFT2DMAP,
	/**
	 * Mouse captured to move the 3D geoscape
	 */
	MODE_SHIFT3DMAP,
	/**
	 * Mouse captured to zoom on the geoscape
	 */
	MODE_ZOOMMAP
};

/**
 * Old X-location of the mouse, when the node is captured.
 * It is related to a captured node (only one at a time), that why it is global.
 */
static int oldMousePosX = 0;

/**
 * Old y-location of the mouse, when the node is captured.
 * It is related to a captured node (only one at a time), that why it is global.
 */
static int oldMousePosY = 0;

/**
 * Status of the node.
 * It is related to a captured node (only one at a time), that why it is global.
 */
static mapDragMode_t mode = MODE_NULL;
static const float ROTATE_SPEED	= 0.5f;
static const float GLOBE_ROTATE = -90.0f;
static const float SMOOTHING_STEP_2D = 0.02f;
static const float SMOOTHACCELERATION = 0.06f;		/**< the acceleration to use during a smooth motion (This affects the speed of the smooth motion) */

static cvar_t* cl_3dmap;						/**< 3D geoscape or flat geoscape */
static cvar_t* cl_3dmapAmbient;
static cvar_t* cl_mapzoommax;
static cvar_t* cl_mapzoommin;

// FIXME: don't make this static
static uiNode_t* geoscapeNode;

/** this is the mask that is used to display day/night on (2d-)geoscape */
image_t* r_dayandnightTexture;

image_t* r_radarTexture;				/**< radar texture */
image_t* r_xviTexture;					/**< XVI alpha mask texture */

#define DAWN		0.03

/**
 * @brief smooth rotation of the 3D geoscape
 * @note updates slowly values of @c angles and @c zoom so that its value goes to @c smoothFinalGlobeAngle
 */
void uiGeoscapeNode::smoothRotate (uiNode_t* node)
{
	vec3_t diff;
	const float diffZoom = UI_MAPEXTRADATACONST(node).smoothFinalZoom - UI_MAPEXTRADATACONST(node).zoom;

	VectorSubtract(UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle, UI_MAPEXTRADATACONST(node).angles, diff);

	if (UI_MAPEXTRADATACONST(node).smoothDeltaLength > UI_MAPEXTRADATACONST(node).smoothDeltaZoom) {
		/* when we rotate (and zoom) */
		const float diffAngle = VectorLength(diff);
		const float epsilon = 0.1f;
		if (diffAngle > epsilon) {
			float rotationSpeed;
			/* Append the old speed to the new speed if this is the first half of a new rotation, but never exceed the max speed.
			 * This allows the globe to rotate at maximum speed when the button is held down. */
			rotationSpeed = sin(3.05f * diffAngle / UI_MAPEXTRADATACONST(node).smoothDeltaLength) * diffAngle;
			if (diffAngle / UI_MAPEXTRADATACONST(node).smoothDeltaLength > 0.5)
				rotationSpeed = std::min(diffAngle, UI_MAPEXTRADATACONST(node).curRotationSpeed + rotationSpeed * 0.5f);

			UI_MAPEXTRADATA(node).curRotationSpeed = rotationSpeed;
			VectorScale(diff, SMOOTHACCELERATION / diffAngle * rotationSpeed, diff);
			VectorAdd(UI_MAPEXTRADATACONST(node).angles, diff, UI_MAPEXTRADATA(node).angles);
			UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).zoom + SMOOTHACCELERATION * diffZoom / diffAngle * rotationSpeed;
			return;
		}
	} else {
		const float epsilonZoom = 0.01f;
		/* when we zoom only */
		if (fabsf(diffZoom) > epsilonZoom) {
			float speed;
			/* Append the old speed to the new speed if this is the first half of a new zoom operation, but never exceed the max speed.
			 * This allows the globe to zoom at maximum speed when the button is held down. */
			if (fabsf(diffZoom) / UI_MAPEXTRADATACONST(node).smoothDeltaZoom > 0.5f) {
				const float maxSpeed = SMOOTHACCELERATION * 2.0f;
				const float newSpeed = UI_MAPEXTRADATACONST(node).curZoomSpeed + sin(3.05 * (fabs(diffZoom) / UI_MAPEXTRADATACONST(node).smoothDeltaZoom)) * SMOOTHACCELERATION;
				speed = std::min(maxSpeed, newSpeed);
			} else {
				speed = sin(3.05 * (fabs(diffZoom) / UI_MAPEXTRADATACONST(node).smoothDeltaZoom)) * SMOOTHACCELERATION * 2.0;
			}
			UI_MAPEXTRADATA(node).curZoomSpeed = speed;
			UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).zoom + diffZoom * speed;
			return;
		}
	}

	/* if we reach this point, that means that movement is over */
	VectorCopy(UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle, UI_MAPEXTRADATA(node).angles);
	UI_MAPEXTRADATA(node).smoothRotation = false;
	UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).smoothFinalZoom;
}

/**
 * @brief smooth translation of the 2D geoscape
 * @note updates slowly values of @c center so that its value goes to @c smoothFinal2DGeoscapeCenter
 * @note updates slowly values of @c zoom so that its value goes to @c ZOOM_LIMIT
 */
void uiGeoscapeNode::smoothTranslate (uiNode_t* node)
{
	const float dist1 = UI_MAPEXTRADATACONST(node).smoothFinal2DGeoscapeCenter[0] - UI_MAPEXTRADATACONST(node).center[0];
	const float dist2 = UI_MAPEXTRADATACONST(node).smoothFinal2DGeoscapeCenter[1] - UI_MAPEXTRADATACONST(node).center[1];
	const float length = sqrt(dist1 * dist1 + dist2 * dist2);

	if (length < SMOOTHING_STEP_2D) {
		UI_MAPEXTRADATA(node).center[0] = UI_MAPEXTRADATACONST(node).smoothFinal2DGeoscapeCenter[0];
		UI_MAPEXTRADATA(node).center[1] = UI_MAPEXTRADATACONST(node).smoothFinal2DGeoscapeCenter[1];
		UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).smoothFinalZoom;
		UI_MAPEXTRADATA(node).smoothRotation = false;
	} else {
		const float diffZoom = UI_MAPEXTRADATACONST(node).smoothFinalZoom - UI_MAPEXTRADATACONST(node).zoom;
		UI_MAPEXTRADATA(node).center[0] = UI_MAPEXTRADATACONST(node).center[0] + SMOOTHING_STEP_2D * dist1 / length;
		UI_MAPEXTRADATA(node).center[1] = UI_MAPEXTRADATACONST(node).center[1] + SMOOTHING_STEP_2D * dist2 / length;
		UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).zoom + SMOOTHING_STEP_2D * diffZoom;
	}
}

/**
 * @brief Applies alpha values to the night overlay image for 2d geoscape
 * @param[in] node The current menuNode we have clicked on (3dmap or map)
 * @param[in] q The angle the sun is standing against the equator on earth
 */
void uiGeoscapeNode::calcAndUploadDayAndNightTexture (uiNode_t* node, float q)
{
	const float dphi = (float) 2 * M_PI / DAN_WIDTH;
	const float da = M_PI / 2 * (HIGH_LAT - LOW_LAT) / DAN_HEIGHT;
	const float sin_q = sin(q);
	const float cos_q = cos(q);
	float sin_phi[DAN_WIDTH], cos_phi[DAN_WIDTH];
	byte* px;

	for (int x = 0; x < DAN_WIDTH; x++) {
		const float phi = x * dphi - q;
		sin_phi[x] = sin(phi);
		cos_phi[x] = cos(phi);
	}

	/* calculate */
	px = UI_MAPEXTRADATA(node).r_dayandnightAlpha;
	for (int y = 0; y < DAN_HEIGHT; y++) {
		const float a = sin(M_PI / 2 * HIGH_LAT - y * da);
		const float root = sqrt(1 - a * a);
		for (int x = 0; x < DAN_WIDTH; x++) {
			const float pos = sin_phi[x] * root * sin_q - (a * SIN_ALPHA + cos_phi[x] * root * COS_ALPHA) * cos_q;

			if (pos >= DAWN)
				*px++ = 255;
			else if (pos <= -DAWN)
				*px++ = 0;
			else
				*px++ = (byte) (128.0 * (pos / DAWN + 1));
		}
	}

	/* upload alpha map into the r_dayandnighttexture */
	R_UploadAlpha(r_dayandnightTexture, UI_MAPEXTRADATA(node).r_dayandnightAlpha);
}

void uiGeoscapeNode::draw (uiNode_t* node)
{
	vec2_t screenPos;

	geoscapeNode = node;
	UI_MAPEXTRADATA(node).flatgeoscape = cl_3dmap->integer == 0;
	UI_MAPEXTRADATA(node).radarOverlay = Cvar_GetValue("geo_overlay_radar");
	UI_MAPEXTRADATA(node).nationOverlay = Cvar_GetValue("geo_overlay_nation");
	UI_MAPEXTRADATA(node).xviOverlay = Cvar_GetValue("geo_overlay_xvi");
	UI_MAPEXTRADATA(node).ambientLightFactor = cl_3dmapAmbient->value;
	UI_MAPEXTRADATA(node).mapzoommin = cl_mapzoommin->value;
	UI_MAPEXTRADATA(node).mapzoommax = cl_mapzoommax->value;

	UI_GetNodeAbsPos(node, UI_MAPEXTRADATA(node).mapPos);
	Vector2Copy(node->box.size, UI_MAPEXTRADATA(node).mapSize);
	if (!UI_MAPEXTRADATACONST(node).flatgeoscape) {
		/* remove the left padding */
		UI_MAPEXTRADATA(node).mapSize[0] -= UI_MAPEXTRADATACONST(node).paddingRight;
	}

	/* Draw geoscape */
	UI_GetNodeScreenPos(node, screenPos);
	UI_PushClipRect(screenPos[0], screenPos[1], node->box.size[0], node->box.size[1]);

	if (UI_MAPEXTRADATACONST(node).smoothRotation) {
		if (UI_MAPEXTRADATACONST(node).flatgeoscape)
			smoothTranslate(node);
		else
			smoothRotate(node);
	}

	geoscapeData_t& data = *UI_MAPEXTRADATA(node).geoscapeData;
	data.geoscapeNode = node;
	GAME_DrawMap(&data);
	if (!data.active)
		return;

	const char* map = data.map;
	date_t& date = data.date;

	/* Draw the map and markers */
	if (UI_MAPEXTRADATACONST(node).flatgeoscape) {
		/* the last q value for the 2d geoscape night overlay */
		static float lastQ = 0.0f;

		/* the sun is not always in the plane of the equator on earth - calculate the angle the sun is at */
		const float q = (date.day % DAYS_PER_YEAR + (float)(date.sec / (SECONDS_PER_HOUR * 6)) / 4) * 2 * M_PI / DAYS_PER_YEAR - M_PI;
		if (lastQ != q) {
			calcAndUploadDayAndNightTexture(node, q);
			lastQ = q;
		}
		R_DrawFlatGeoscape(UI_MAPEXTRADATACONST(node).mapPos, UI_MAPEXTRADATACONST(node).mapSize, (float) date.sec / SECONDS_PER_DAY,
				UI_MAPEXTRADATACONST(node).center[0], UI_MAPEXTRADATACONST(node).center[1], 0.5 / UI_MAPEXTRADATACONST(node).zoom, map,
				data.nationOverlay, data.xviOverlay, data.radarOverlay, r_dayandnightTexture, r_xviTexture, r_radarTexture);

		GAME_DrawMapMarkers(node);
	} else {
		bool disableSolarRender = false;
		if (UI_MAPEXTRADATACONST(node).zoom > 3.3)
			disableSolarRender = true;

		R_EnableRenderbuffer(true);

		R_Draw3DGlobe(UI_MAPEXTRADATACONST(node).mapPos, UI_MAPEXTRADATACONST(node).mapSize, date.day, date.sec,
				UI_MAPEXTRADATACONST(node).angles, UI_MAPEXTRADATACONST(node).zoom, map, disableSolarRender,
				UI_MAPEXTRADATACONST(node).ambientLightFactor, UI_MAPEXTRADATA(node).nationOverlay,
				UI_MAPEXTRADATA(node).xviOverlay, UI_MAPEXTRADATA(node).radarOverlay, r_xviTexture, r_radarTexture,
				true);

		GAME_DrawMapMarkers(node);

		R_DrawBloom();
		R_EnableRenderbuffer(false);
	}

	UI_PopClipRect();
}

void uiGeoscapeNode::onCapturedMouseMove (uiNode_t* node, int x, int y)
{
	switch (mode) {
	case MODE_SHIFT2DMAP:
	{
		const float zoom = 0.5 / UI_MAPEXTRADATACONST(node).zoom;
		/* shift the map */
		UI_MAPEXTRADATA(node).center[0] -= (float) (mousePosX - oldMousePosX) / (node->box.size[0] * UI_MAPEXTRADATACONST(node).zoom);
		UI_MAPEXTRADATA(node).center[1] -= (float) (mousePosY - oldMousePosY) / (node->box.size[1] * UI_MAPEXTRADATACONST(node).zoom);
		for (int i = 0; i < 2; i++) {
			/* clamp to min/max values */
			while (UI_MAPEXTRADATACONST(node).center[i] < 0.0)
				UI_MAPEXTRADATA(node).center[i] += 1.0;
			while (UI_MAPEXTRADATACONST(node).center[i] > 1.0)
				UI_MAPEXTRADATA(node).center[i] -= 1.0;
		}
		if (UI_MAPEXTRADATACONST(node).center[1] < zoom)
			UI_MAPEXTRADATA(node).center[1] = zoom;
		if (UI_MAPEXTRADATACONST(node).center[1] > 1.0 - zoom)
			UI_MAPEXTRADATA(node).center[1] = 1.0 - zoom;
		break;
	}

	case MODE_SHIFT3DMAP:
		/* rotate a model */
		UI_MAPEXTRADATA(node).angles[PITCH] += ROTATE_SPEED * (mousePosX - oldMousePosX) / UI_MAPEXTRADATACONST(node).zoom;
		UI_MAPEXTRADATA(node).angles[YAW] -= ROTATE_SPEED * (mousePosY - oldMousePosY) / UI_MAPEXTRADATACONST(node).zoom;

		/* clamp the UI_MAPEXTRADATACONST(node).angles */
		while (UI_MAPEXTRADATACONST(node).angles[YAW] > 0.0)
			UI_MAPEXTRADATA(node).angles[YAW] = 0.0;
		while (UI_MAPEXTRADATACONST(node).angles[YAW] < -180.0)
			UI_MAPEXTRADATA(node).angles[YAW] = -180.0;

		while (UI_MAPEXTRADATACONST(node).angles[PITCH] > 180.0)
			UI_MAPEXTRADATA(node).angles[PITCH] -= 360.0;
		while (UI_MAPEXTRADATACONST(node).angles[PITCH] < -180.0)
			UI_MAPEXTRADATA(node).angles[PITCH] += 360.0;
		break;
	case MODE_ZOOMMAP:
	{
		const float zoom = 0.5 / UI_MAPEXTRADATACONST(node).zoom;
		/* zoom the map */
		UI_MAPEXTRADATA(node).zoom *= pow(0.995, mousePosY - oldMousePosY);
		if (UI_MAPEXTRADATACONST(node).zoom < UI_MAPEXTRADATACONST(node).mapzoommin)
			UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).mapzoommin;
		else if (UI_MAPEXTRADATACONST(node).zoom > UI_MAPEXTRADATACONST(node).mapzoommax)
			UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).mapzoommax;

		if (UI_MAPEXTRADATACONST(node).center[1] < zoom)
			UI_MAPEXTRADATA(node).center[1] = zoom;
		if (UI_MAPEXTRADATACONST(node).center[1] > 1.0 - zoom)
			UI_MAPEXTRADATA(node).center[1] = 1.0 - zoom;
		break;
	}
	default:
		assert(false);
		break;
	}
	oldMousePosX = x;
	oldMousePosY = y;
}

void uiGeoscapeNode::startMouseShifting (uiNode_t* node, int x, int y)
{
	UI_SetMouseCapture(node);
	if (UI_MAPEXTRADATACONST(node).flatgeoscape)
		mode = MODE_SHIFT2DMAP;
	else
		mode = MODE_SHIFT3DMAP;
	UI_MAPEXTRADATA(node).smoothRotation = false;
	oldMousePosX = x;
	oldMousePosY = y;
}

/**
 * @brief Return longitude and latitude of a point of the screen for 2D geoscape
 * @param[in] node The current menuNode we have clicked on (3dmap or map)
 * @param[in] x,y Coordinates on the screen that were clicked on
 * @param[out] pos vec2_t was filled with longitude and latitude
 */
void uiGeoscapeNode::screenToMap (const uiNode_t* node, int x, int y, vec2_t pos)
{
	pos[0] = (((UI_MAPEXTRADATACONST(node).mapPos[0] - x) / UI_MAPEXTRADATACONST(node).mapSize[0] + 0.5) / UI_MAPEXTRADATACONST(node).zoom
			- (UI_MAPEXTRADATACONST(node).center[0] - 0.5)) * 360.0;
	pos[1] = (((UI_MAPEXTRADATACONST(node).mapPos[1] - y) / UI_MAPEXTRADATACONST(node).mapSize[1] + 0.5) / UI_MAPEXTRADATACONST(node).zoom
			- (UI_MAPEXTRADATACONST(node).center[1] - 0.5)) * 180.0;

	while (pos[0] > 180.0)
		pos[0] -= 360.0;
	while (pos[0] < -180.0)
		pos[0] += 360.0;
}

/**
 * @brief Return longitude and latitude of a point of the screen for 3D geoscape (globe)
 * @param[in] node The current menuNode we have clicked on (3dmap or map)
 * @param[in] x,y Coordinates on the screen that were clicked on
 * @param[out] pos vec2_t was filled with longitude and latitude
 * @sa MAP_3DMapToScreen
 */
void uiGeoscapeNode::screenTo3DMap (const uiNode_t* node, int x, int y, vec2_t pos)
{
	vec2_t mid;
	vec3_t v, v1, rotationAxis;
	float dist;
	const float radius = GLOBE_RADIUS;

	/* set mid to the coordinates of the center of the globe */
	Vector2Set(mid, UI_MAPEXTRADATACONST(node).mapPos[0] + UI_MAPEXTRADATACONST(node).mapSize[0] / 2.0f,
			UI_MAPEXTRADATACONST(node).mapPos[1] + UI_MAPEXTRADATACONST(node).mapSize[1] / 2.0f);

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
	RotatePointAroundVector(v1, rotationAxis, v, UI_MAPEXTRADATACONST(node).angles[YAW]);

	/* second rotation is to rotate the earth around its north-south axis
	 * so that Greenwich meridian is along the vertical axis of the screen */
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v, rotationAxis, v1, UI_MAPEXTRADATACONST(node).angles[PITCH]);

	/* we therefore got in v the coordinates of the point in the static frame of the earth
	 * that we can convert in polar coordinates to get its latitude and longitude */
	VecToPolar(v, pos);
}

void uiGeoscapeNode::onLeftClick (uiNode_t* node, int x, int y)
{
	if (mode != MODE_NULL)
		return;

	vec2_t pos;

	/* get map position */
	if (!UI_MAPEXTRADATACONST(node).flatgeoscape)
		screenTo3DMap(node, x, y, pos);
	else
		screenToMap(node, x, y, pos);

	GAME_MapClick(node, x, y, pos);
}

bool uiGeoscapeNode::onStartDragging (uiNode_t* node, int startX, int startY, int x, int y, int button)
{
	switch (button) {
	case K_MOUSE1:
	case K_MOUSE3:
		startMouseShifting(node, startX, startY);
		return true;
	case K_MOUSE2:
		UI_SetMouseCapture(node);
		mode = MODE_ZOOMMAP;
		oldMousePosX = startX;
		oldMousePosY = startY;
		return true;
	}
	return false;
}

void uiGeoscapeNode::onMouseUp (uiNode_t* node, int x, int y, int button)
{
	if (mode != MODE_NULL) {
		UI_MouseRelease();
		mode = MODE_NULL;
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
void uiGeoscapeNode::onCapturedMouseLost (uiNode_t* node)
{
	mode = MODE_NULL;
}

/**
 * Zoom on the node
 * @todo it should use an int param for smooth zoom
 */
void uiGeoscapeNode::zoom (uiNode_t* node, bool out)
{
	UI_MAPEXTRADATA(node).zoom *= pow(0.995, (out ? 10: -10));
	if (UI_MAPEXTRADATACONST(node).zoom < UI_MAPEXTRADATACONST(node).mapzoommin)
		UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).mapzoommin;
	else if (UI_MAPEXTRADATACONST(node).zoom > UI_MAPEXTRADATACONST(node).mapzoommax)
		UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).mapzoommax;

	if (UI_MAPEXTRADATACONST(node).flatgeoscape) {
		if (UI_MAPEXTRADATACONST(node).center[1] < 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 0.5 / UI_MAPEXTRADATACONST(node).zoom;
		if (UI_MAPEXTRADATACONST(node).center[1] > 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom;
	}
	UI_MAPEXTRADATA(node).smoothRotation = false;
}

bool uiGeoscapeNode::onScroll (uiNode_t* node, int deltaX, int deltaY)
{
	bool down = deltaY > 0;
	if (deltaY == 0)
		return false;
	zoom(node, down);
	return true;
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
void uiGeoscapeNode::onLoading (uiNode_t* node)
{
	Vector4Set(node->color, 1, 1, 1, 1);

	OBJZERO(EXTRADATA(node));
	EXTRADATA(node).angles[YAW] = GLOBE_ROTATE;
	EXTRADATA(node).center[0] = EXTRADATA(node).center[1] = 0.5;
	EXTRADATA(node).zoom = 1.0;
	Vector2Set(EXTRADATA(node).smoothFinal2DGeoscapeCenter, 0.5, 0.5);
	VectorSet(EXTRADATA(node).smoothFinalGlobeAngle, 0, GLOBE_ROTATE, 0);

	/* @todo: allocate this on a per node basis - and remove the global variable geoscapeData */
	EXTRADATA(node).geoscapeData = &geoscapeData;
	/* EXTRADATA(node).geoscapeData = Mem_AllocType(geoscapeData_t); */

	/** this is the data that is used with r_dayandnightTexture */
	EXTRADATA(node).r_dayandnightAlpha = Mem_AllocTypeN(byte, DAN_WIDTH * DAN_HEIGHT);

	r_dayandnightTexture = R_LoadImageData("***r_dayandnighttexture***", nullptr, DAN_WIDTH, DAN_HEIGHT, it_effect);
	r_radarTexture = R_LoadImageData("***r_radarTexture***", nullptr, RADAR_WIDTH, RADAR_HEIGHT, it_effect);
	r_xviTexture = R_LoadImageData("***r_xvitexture***", nullptr, XVI_WIDTH, XVI_HEIGHT, it_effect);
}

static void UI_GeoscapeNodeZoomIn (uiNode_t* node, const uiCallContext_t* context)
{
	uiGeoscapeNode* m = static_cast<uiGeoscapeNode*>(node->behaviour->manager.get());
	m->zoom(node, false);
}

static void UI_GeoscapeNodeZoomOut (uiNode_t* node, const uiCallContext_t* context)
{
	uiGeoscapeNode* m = static_cast<uiGeoscapeNode*>(node->behaviour->manager.get());
	m->zoom(node, true);
}

/**
 * @brief Command binding for map zooming
 * @todo convert into node method
 */
static void UI_GeoscapeNodeZoom_f (void)
{
	const char* cmd;
	const float zoomAmount = 50.0f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <in|out>\n", Cmd_Argv(0));
		return;
	}

	cmd = Cmd_Argv(1);
	uiNode_t* node = geoscapeNode;
	if (!node)
		return;

	switch (cmd[0]) {
	case 'i':
		UI_MAPEXTRADATA(node).smoothFinalZoom = UI_MAPEXTRADATACONST(node).zoom * powf(0.995, -zoomAmount);
		break;
	case 'o':
		UI_MAPEXTRADATA(node).smoothFinalZoom = UI_MAPEXTRADATACONST(node).zoom * powf(0.995, zoomAmount);
		break;
	default:
		Com_Printf("UI_GeoscapeNodeZoom_f: Invalid parameter: %s\n", cmd);
		return;
	}

	if (UI_MAPEXTRADATACONST(node).smoothFinalZoom < UI_MAPEXTRADATACONST(node).mapzoommin)
		UI_MAPEXTRADATA(node).smoothFinalZoom = UI_MAPEXTRADATACONST(node).mapzoommin;
	else if (UI_MAPEXTRADATACONST(node).smoothFinalZoom > UI_MAPEXTRADATACONST(node).mapzoommax)
		UI_MAPEXTRADATA(node).smoothFinalZoom = UI_MAPEXTRADATACONST(node).mapzoommax;

	if (UI_MAPEXTRADATACONST(node).flatgeoscape) {
		UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).smoothFinalZoom;
		if (UI_MAPEXTRADATACONST(node).center[1] < 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 0.5 / UI_MAPEXTRADATACONST(node).zoom;
		if (UI_MAPEXTRADATACONST(node).center[1] > 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom;
	} else {
		VectorCopy(UI_MAPEXTRADATACONST(node).angles, UI_MAPEXTRADATA(node).smoothFinalGlobeAngle);
		UI_MAPEXTRADATA(node).smoothDeltaLength = 0;
		UI_MAPEXTRADATA(node).smoothRotation = true;
		UI_MAPEXTRADATA(node).smoothDeltaZoom = fabs(UI_MAPEXTRADATACONST(node).smoothFinalZoom - UI_MAPEXTRADATACONST(node).zoom);
	}
}

/**
 * @brief Command binding for map scrolling
 * @todo convert into node method
 */
static void UI_GeoscapeNodeScroll_f (void)
{
	const char* cmd;
	float scrollX = 0.0f, scrollY = 0.0f;
	const float scrollAmount = 80.0f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <up|down|left|right>\n", Cmd_Argv(0));
		return;
	}

	cmd = Cmd_Argv(1);

	uiNode_t* node = geoscapeNode;
	if (!node)
		return;

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
		Com_Printf("UI_GeoscapeNodeScroll_f: Invalid parameter\n");
		return;
	}

	if (!UI_MAPEXTRADATACONST(node).flatgeoscape) {
		/* case 3D geoscape */
		vec3_t diff;

		VectorCopy(UI_MAPEXTRADATACONST(node).angles, UI_MAPEXTRADATA(node).smoothFinalGlobeAngle);

		/* rotate a model */
		UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[PITCH] += ROTATE_SPEED * (scrollX) / UI_MAPEXTRADATACONST(node).zoom;
		UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[YAW] -= ROTATE_SPEED * (scrollY) / UI_MAPEXTRADATACONST(node).zoom;

		while (UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle[YAW] < -180.0) {
			UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[YAW] = -180.0;
		}
		while (UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle[YAW] > 0.0) {
			UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[YAW] = 0.0;
		}

		while (UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle[PITCH] > 180.0) {
			UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[PITCH] -= 360.0;
			UI_MAPEXTRADATA(node).angles[PITCH] -= 360.0;
		}
		while (UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle[PITCH] < -180.0) {
			UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[PITCH] += 360.0;
			UI_MAPEXTRADATA(node).angles[PITCH] += 360.0;
		}
		VectorSubtract(UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle, UI_MAPEXTRADATACONST(node).angles, diff);
		UI_MAPEXTRADATA(node).smoothDeltaLength = VectorLength(diff);

		UI_MAPEXTRADATA(node).smoothFinalZoom = UI_MAPEXTRADATACONST(node).zoom;
		UI_MAPEXTRADATA(node).smoothDeltaZoom = 0.0f;
		UI_MAPEXTRADATA(node).smoothRotation = true;
	} else {
		/* shift the map */
		UI_MAPEXTRADATA(node).center[0] -= (float) (scrollX) / (UI_MAPEXTRADATACONST(node).mapSize[0] * UI_MAPEXTRADATACONST(node).zoom);
		UI_MAPEXTRADATA(node).center[1] -= (float) (scrollY) / (UI_MAPEXTRADATACONST(node).mapSize[1] * UI_MAPEXTRADATACONST(node).zoom);
		for (int i = 0; i < 2; i++) {
			while (UI_MAPEXTRADATACONST(node).center[i] < 0.0)
				UI_MAPEXTRADATA(node).center[i] += 1.0;
			while (UI_MAPEXTRADATACONST(node).center[i] > 1.0)
				UI_MAPEXTRADATA(node).center[i] -= 1.0;
		}
		if (UI_MAPEXTRADATACONST(node).center[1] < 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 0.5 / UI_MAPEXTRADATACONST(node).zoom;
		if (UI_MAPEXTRADATACONST(node).center[1] > 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom;
	}
}

void UI_RegisterGeoscapeNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "geoscape";
	behaviour->manager = UINodePtr(new uiGeoscapeNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Use a right padding. */
	UI_RegisterExtradataNodeProperty(behaviour, "padding-right", V_FLOAT, EXTRADATA_TYPE, paddingRight);
	/* Call it to zoom out of the map */
	UI_RegisterNodeMethod(behaviour, "zoomin", UI_GeoscapeNodeZoomIn);
	/* Call it to zoom into the map */
	UI_RegisterNodeMethod(behaviour, "zoomout", UI_GeoscapeNodeZoomOut);

	Cmd_AddCommand("map_zoom", UI_GeoscapeNodeZoom_f);
	Cmd_AddCommand("map_scroll", UI_GeoscapeNodeScroll_f);

	cl_3dmap = Cvar_Get("cl_3dmap", "1", CVAR_ARCHIVE, "3D geoscape or flat geoscape");
	cl_3dmapAmbient = Cvar_Get("cl_3dmapAmbient", "0", CVAR_ARCHIVE, "3D geoscape ambient lighting factor");
	cl_mapzoommax = Cvar_Get("cl_mapzoommax", "6.0", CVAR_ARCHIVE, "Maximum geoscape zooming value");
	cl_mapzoommin = Cvar_Get("cl_mapzoommin", "1.0", CVAR_ARCHIVE, "Minimum geoscape zooming value");
}
