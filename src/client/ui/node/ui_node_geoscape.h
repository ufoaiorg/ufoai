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

#pragma once

#include "../ui_nodes.h"
#include "ui_node_abstractnode.h"
#include "../../cl_shared.h"

class uiGeoscapeNode : public uiLocatedNode {
protected:
	void smoothTranslate (uiNode_t* node);
	void smoothRotate (uiNode_t* node);
	void screenTo3DMap (const uiNode_t* node, int x, int y, vec2_t pos);
	void screenToMap (const uiNode_t* node, int x, int y, vec2_t pos);
	void calcAndUploadDayAndNightTexture (uiNode_t* node, float q);
public:
	void draw(uiNode_t* node) override;
	void onMouseUp(uiNode_t* node, int x, int y, int button) override;
	void onCapturedMouseMove(uiNode_t* node, int x, int y) override;
	void onCapturedMouseLost(uiNode_t* node) override;
	void onLoading(uiNode_t* node) override;
	bool onScroll(uiNode_t* node, int deltaX, int deltaY) override;
	void onLeftClick(uiNode_t* node, int x, int y) override;
	bool onStartDragging(uiNode_t* node, int startX, int startY, int currentX, int currentY, int button) override;
	void zoom(uiNode_t* node, bool out);
	void startMouseShifting(uiNode_t* node, int x, int y);
};

#define UI_MAPEXTRADATA_TYPE mapExtraData_t
#define UI_MAPEXTRADATA(node) UI_EXTRADATA(node, UI_MAPEXTRADATA_TYPE)
#define UI_MAPEXTRADATACONST(node) UI_EXTRADATACONST(node, UI_MAPEXTRADATA_TYPE)

#define DAN_WIDTH		2048
#define DAN_HEIGHT		1024

/**
 * @brief Typical zoom to use on the 3D geoscape to use same zoom values for both 2D and 3D geoscape
 * @note Used to convert openGL coordinates of the sphere into screen coordinates
 * @sa GLOBE_RADIUS */
#define STANDARD_3D_ZOOM 40.0f

#define EARTH_RADIUS 8192.0f
#define MOON_RADIUS 1024.0f
#define SUN_RADIUS 1024.0f

/** @brief radius of the globe in screen coordinates */
#define GLOBE_RADIUS EARTH_RADIUS * (UI_MAPEXTRADATACONST(node).zoom / STANDARD_3D_ZOOM)

typedef struct mapExtraData_s {
	/* Smoothing variables */
	bool smoothRotation;			/**< @c true if the rotation of 3D geoscape must me smooth */
	vec3_t smoothFinalGlobeAngle;	/**< value of final angles for a smooth change of angle (see MAP_CenterOnPoint)*/
	vec2_t smoothFinal2DGeoscapeCenter; /**< value of center for a smooth change of position (see MAP_CenterOnPoint) */
	float smoothDeltaLength;		/**< angle/position difference that we need to change when smoothing */
	float smoothFinalZoom;			/**< value of final zoom for a smooth change of angle (see MAP_CenterOnPoint)*/
	float smoothDeltaZoom;			/**< zoom difference that we need to change when smoothing */
	float curZoomSpeed;				/**< The current zooming speed. Used for smooth zooming. */
	float curRotationSpeed;			/**< The current rotation speed. Used for smooth rotating.*/

	vec2_t mapSize;
	vec2_t mapPos;
	vec3_t angles;	/**< 3d geoscape rotation */
	vec2_t center;	/**< latitude and longitude of the point we're looking at on earth */
	float zoom;		/**< zoom used when looking at earth */
	bool flatgeoscape;
	float ambientLightFactor;
	float mapzoommin;
	float mapzoommax;
	float paddingRight;
	int radarOverlay;
	int nationOverlay;
	int xviOverlay;
	geoscapeData_t* geoscapeData;
	byte* r_dayandnightAlpha;
} mapExtraData_t;

void UI_RegisterGeoscapeNode(uiBehaviour_t* behaviour);
