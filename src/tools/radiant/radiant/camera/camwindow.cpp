/**
 * @file camwindow.cpp
 * @author Leonardo Zide (leo@lokigames.com)
 * @brief Camera Window
 */

/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "camwindow.h"
#include "radiant_i18n.h"

#include "debugging/debugging.h"

#include "ientity.h"
#include "iscenegraph.h"
#include "irender.h"
#include "igl.h"
#include "cullable.h"
#include "renderable.h"
#include "preferencesystem.h"

#include "signal/signal.h"
#include "container/array.h"
#include "scenelib.h"
#include "render.h"
#include "math/frustum.h"

#include "gtkutil/widget.h"
#include "gtkutil/button.h"
#include "gtkutil/toolbar.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/GLWidgetSentry.h"
#include "gtkutil/xorrectangle.h"
#include "../gtkmisc.h"
#include "../selection.h"
#include "../mainframe.h"
#include "../settings/preferences.h"
#include "../commands.h"
#include "../xyview/GlobalXYWnd.h"
#include "../windowobservers.h"
#include "../ui/Icons.h"
#include "../render/RenderStatistics.h"


#include "../timer.h"

#include "CamRenderer.h"

Signal0 g_cameraMoved_callbacks;

void AddCameraMovedCallback (const SignalHandler& handler)
{
	g_cameraMoved_callbacks.connectLast(handler);
}

void CameraMovedNotify ()
{
	g_cameraMoved_callbacks();
}

struct camwindow_globals_private_t
{
		int m_nMoveSpeed;
		bool m_bCamLinkSpeed;
		int m_nAngleSpeed;
		bool m_bCamInverseMouse;
		bool m_bLightRadius;
		bool m_bCamDiscrete;
		bool m_bCubicClipping;
		bool m_showStats;
		int m_nStrafeMode;

		camwindow_globals_private_t () :
			m_nMoveSpeed(100), m_bCamLinkSpeed(true), m_nAngleSpeed(3), m_bCamInverseMouse(false),
					m_bLightRadius(false), m_bCamDiscrete(true), m_bCubicClipping(true), m_showStats(true),
					m_nStrafeMode(0)
		{
		}
};

camwindow_globals_private_t g_camwindow_globals_private;

const Matrix4 g_opengl2radiant(0, 0, -1, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1);

const Matrix4 g_radiant2opengl(0, -1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 1);

struct camera_t;
void Camera_mouseMove (camera_t& camera, int x, int y);

#include "Camera.h"

camera_draw_mode camera_t::draw_mode = cd_texture;

inline Matrix4 projection_for_camera (float near_z, float far_z, float fieldOfView, int width, int height)
{
	const float half_width = static_cast<float> (near_z * tan(degrees_to_radians(fieldOfView * 0.5)));
	const float half_height = half_width * (static_cast<float> (height) / static_cast<float> (width));

	return matrix4_frustum(-half_width, half_width, -half_height, half_height, near_z, far_z);
}

float Camera_getFarClipPlane (camera_t& camera)
{
	return (g_camwindow_globals_private.m_bCubicClipping) ? pow(2.0, (g_camwindow_globals.m_nCubicScale + 7) / 2.0)
			: 32768.0f;
}

void Camera_updateProjection (camera_t& camera)
{
	float farClip = Camera_getFarClipPlane(camera);
	camera.projection = projection_for_camera(farClip / 4096.0f, farClip, camera.fieldOfView, camera.width,
			camera.height);

	camera.m_view->Construct(camera.projection, camera.modelview, camera.width, camera.height);
}

void Camera_updateVectors (camera_t& camera)
{
	for (int i = 0; i < 3; i++) {
		camera.vright[i] = camera.modelview[(i << 2) + 0];
		camera.vup[i] = camera.modelview[(i << 2) + 1];
		camera.vpn[i] = camera.modelview[(i << 2) + 2];
	}
}

void Camera_updateModelview (camera_t& camera)
{
	camera.modelview = Matrix4::getIdentity();

	// roll, pitch, yaw
	Vector3 radiant_eulerXYZ(0, -camera.angles[CAMERA_PITCH], camera.angles[CAMERA_YAW]);

	camera.modelview.translateBy(camera.origin);
	matrix4_rotate_by_euler_xyz_degrees(camera.modelview, radiant_eulerXYZ);
	matrix4_multiply_by_matrix4(camera.modelview, g_radiant2opengl);
	matrix4_affine_invert(camera.modelview);

	Camera_updateVectors(camera);

	camera.m_view->Construct(camera.projection, camera.modelview, camera.width, camera.height);
}

void Camera_Move_updateAxes (camera_t& camera)
{
	double ya = degrees_to_radians(camera.angles[CAMERA_YAW]);

	// the movement matrix is kept 2d
	camera.forward[0] = static_cast<float> (cos(ya));
	camera.forward[1] = static_cast<float> (sin(ya));
	camera.forward[2] = 0;
	camera.right[0] = camera.forward[1];
	camera.right[1] = -camera.forward[0];
}

void Camera_Freemove_updateAxes (camera_t& camera)
{
	camera.right = camera.vright;
	camera.forward = -camera.vpn;
}

const Vector3& Camera_getOrigin (camera_t& camera)
{
	return camera.origin;
}

void Camera_setOrigin (camera_t& camera, const Vector3& origin)
{
	camera.origin = origin;
	Camera_updateModelview(camera);
	camera.m_update();
	CameraMovedNotify();
}

const Vector3& Camera_getAngles (camera_t& camera)
{
	return camera.angles;
}

void Camera_setAngles (camera_t& camera, const Vector3& angles)
{
	camera.angles = angles;
	Camera_updateModelview(camera);
	camera.m_update();
	CameraMovedNotify();
}

void Camera_FreeMove (camera_t& camera, int dx, int dy)
{
	// free strafe mode, toggled by the ctrl key with optional shift for forward movement
	if (camera.m_strafe) {
		float strafespeed = 0.65f;

		if (g_camwindow_globals_private.m_bCamLinkSpeed) {
			strafespeed = (float) g_camwindow_globals_private.m_nMoveSpeed / 100;
		}

		camera.origin -= camera.vright * strafespeed * dx;
		if (camera.m_strafe_forward)
			camera.origin += camera.vpn * strafespeed * dy;
		else
			camera.origin += camera.vup * strafespeed * dy;
	} else { // free rotation
		const float dtime = 0.1f;

		if (g_camwindow_globals_private.m_bCamInverseMouse)
			camera.angles[CAMERA_PITCH] -= dy * dtime * g_camwindow_globals_private.m_nAngleSpeed;
		else
			camera.angles[CAMERA_PITCH] += dy * dtime * g_camwindow_globals_private.m_nAngleSpeed;

		camera.angles[CAMERA_YAW] += dx * dtime * g_camwindow_globals_private.m_nAngleSpeed;

		if (camera.angles[CAMERA_PITCH] > 90)
			camera.angles[CAMERA_PITCH] = 90;
		else if (camera.angles[CAMERA_PITCH] < -90)
			camera.angles[CAMERA_PITCH] = -90;

		if (camera.angles[CAMERA_YAW] >= 360)
			camera.angles[CAMERA_YAW] -= 360;
		else if (camera.angles[CAMERA_YAW] <= 0)
			camera.angles[CAMERA_YAW] += 360;
	}

	Camera_updateModelview(camera);
	Camera_Freemove_updateAxes(camera);
}

void Cam_MouseControl (camera_t& camera, int x, int y)
{
	int xl, xh;
	int yl, yh;
	float xf, yf;

	xf = (float) (x - camera.width / 2) / (camera.width / 2);
	yf = (float) (y - camera.height / 2) / (camera.height / 2);

	xl = camera.width / 3;
	xh = xl * 2;
	yl = camera.height / 3;
	yh = yl * 2;

	xf *= 1.0f - fabsf(yf);
	if (xf < 0) {
		xf += 0.1f;
		if (xf > 0)
			xf = 0;
	} else {
		xf -= 0.1f;
		if (xf < 0)
			xf = 0;
	}

	camera.origin += camera.forward * (yf * 0.1f * g_camwindow_globals_private.m_nMoveSpeed);
	camera.angles[CAMERA_YAW] += xf * -0.1f * g_camwindow_globals_private.m_nAngleSpeed;

	Camera_updateModelview(camera);
}

void Camera_mouseMove (camera_t& camera, int x, int y)
{
	Camera_FreeMove(camera, -x, -y);
	camera.m_update();
	CameraMovedNotify();
}

const unsigned int MOVE_NONE = 0;
const unsigned int MOVE_FORWARD = 1 << 0;
const unsigned int MOVE_BACK = 1 << 1;
const unsigned int MOVE_ROTRIGHT = 1 << 2;
const unsigned int MOVE_ROTLEFT = 1 << 3;
const unsigned int MOVE_STRAFERIGHT = 1 << 4;
const unsigned int MOVE_STRAFELEFT = 1 << 5;
const unsigned int MOVE_UP = 1 << 6;
const unsigned int MOVE_DOWN = 1 << 7;
const unsigned int MOVE_PITCHUP = 1 << 8;
const unsigned int MOVE_PITCHDOWN = 1 << 9;
const unsigned int MOVE_ALL = MOVE_FORWARD | MOVE_BACK | MOVE_ROTRIGHT | MOVE_ROTLEFT | MOVE_STRAFERIGHT
		| MOVE_STRAFELEFT | MOVE_UP | MOVE_DOWN | MOVE_PITCHUP | MOVE_PITCHDOWN;

void Cam_KeyControl (camera_t& camera, float dtime)
{
	// Update angles
	if (camera.movementflags & MOVE_ROTLEFT)
		camera.angles[CAMERA_YAW] += 15 * dtime * g_camwindow_globals_private.m_nAngleSpeed;
	if (camera.movementflags & MOVE_ROTRIGHT)
		camera.angles[CAMERA_YAW] -= 15 * dtime * g_camwindow_globals_private.m_nAngleSpeed;
	if (camera.movementflags & MOVE_PITCHUP) {
		camera.angles[CAMERA_PITCH] += 15 * dtime * g_camwindow_globals_private.m_nAngleSpeed;
		if (camera.angles[CAMERA_PITCH] > 90)
			camera.angles[CAMERA_PITCH] = 90;
	}
	if (camera.movementflags & MOVE_PITCHDOWN) {
		camera.angles[CAMERA_PITCH] -= 15 * dtime * g_camwindow_globals_private.m_nAngleSpeed;
		if (camera.angles[CAMERA_PITCH] < -90)
			camera.angles[CAMERA_PITCH] = -90;
	}

	Camera_updateModelview(camera);
	Camera_Freemove_updateAxes(camera);

	// Update position
	if (camera.movementflags & MOVE_FORWARD)
		camera.origin += camera.forward * (dtime * g_camwindow_globals_private.m_nMoveSpeed);
	if (camera.movementflags & MOVE_BACK)
		camera.origin += camera.forward * (-dtime * g_camwindow_globals_private.m_nMoveSpeed);
	if (camera.movementflags & MOVE_STRAFELEFT)
		camera.origin += camera.right * (-dtime * g_camwindow_globals_private.m_nMoveSpeed);
	if (camera.movementflags & MOVE_STRAFERIGHT)
		camera.origin += camera.right * (dtime * g_camwindow_globals_private.m_nMoveSpeed);
	if (camera.movementflags & MOVE_UP)
		camera.origin += g_vector3_axis_z * (dtime * g_camwindow_globals_private.m_nMoveSpeed);
	if (camera.movementflags & MOVE_DOWN)
		camera.origin += g_vector3_axis_z * (-dtime * g_camwindow_globals_private.m_nMoveSpeed);

	Camera_updateModelview(camera);
}

void Camera_keyMove (camera_t& camera)
{
	camera.m_mouseMove.flush();

	float time_seconds = camera.m_keycontrol_timer.elapsed_msec() / static_cast<float> (msec_per_sec);
	camera.m_keycontrol_timer.start();
	if (time_seconds > 0.05f) {
		time_seconds = 0.05f; // 20fps
	}
	Cam_KeyControl(camera, time_seconds * 5.0f);

	camera.m_update();
	CameraMovedNotify();
}

gboolean camera_keymove (gpointer data)
{
	Camera_keyMove(*reinterpret_cast<camera_t*> (data));
	return TRUE;
}

void Camera_setMovementFlags (camera_t& camera, unsigned int mask)
{
	if ((~camera.movementflags & mask) != 0 && camera.movementflags == 0) {
		camera.m_keymove_handler = g_idle_add(camera_keymove, &camera);
	}
	camera.movementflags |= mask;
}
void Camera_clearMovementFlags (camera_t& camera, unsigned int mask)
{
	if ((camera.movementflags & ~mask) == 0 && camera.movementflags != 0) {
		g_source_remove(camera.m_keymove_handler);
		camera.m_keymove_handler = 0;
	}
	camera.movementflags &= ~mask;
}

void Camera_MoveForward_KeyDown (camera_t& camera)
{
	Camera_setMovementFlags(camera, MOVE_FORWARD);
}
void Camera_MoveForward_KeyUp (camera_t& camera)
{
	Camera_clearMovementFlags(camera, MOVE_FORWARD);
}
void Camera_MoveBack_KeyDown (camera_t& camera)
{
	Camera_setMovementFlags(camera, MOVE_BACK);
}
void Camera_MoveBack_KeyUp (camera_t& camera)
{
	Camera_clearMovementFlags(camera, MOVE_BACK);
}

void Camera_MoveLeft_KeyDown (camera_t& camera)
{
	Camera_setMovementFlags(camera, MOVE_STRAFELEFT);
}
void Camera_MoveLeft_KeyUp (camera_t& camera)
{
	Camera_clearMovementFlags(camera, MOVE_STRAFELEFT);
}
void Camera_MoveRight_KeyDown (camera_t& camera)
{
	Camera_setMovementFlags(camera, MOVE_STRAFERIGHT);
}
void Camera_MoveRight_KeyUp (camera_t& camera)
{
	Camera_clearMovementFlags(camera, MOVE_STRAFERIGHT);
}

void Camera_MoveUp_KeyDown (camera_t& camera)
{
	Camera_setMovementFlags(camera, MOVE_UP);
}
void Camera_MoveUp_KeyUp (camera_t& camera)
{
	Camera_clearMovementFlags(camera, MOVE_UP);
}
void Camera_MoveDown_KeyDown (camera_t& camera)
{
	Camera_setMovementFlags(camera, MOVE_DOWN);
}
void Camera_MoveDown_KeyUp (camera_t& camera)
{
	Camera_clearMovementFlags(camera, MOVE_DOWN);
}

void Camera_RotateLeft_KeyDown (camera_t& camera)
{
	Camera_setMovementFlags(camera, MOVE_ROTLEFT);
}
void Camera_RotateLeft_KeyUp (camera_t& camera)
{
	Camera_clearMovementFlags(camera, MOVE_ROTLEFT);
}
void Camera_RotateRight_KeyDown (camera_t& camera)
{
	Camera_setMovementFlags(camera, MOVE_ROTRIGHT);
}
void Camera_RotateRight_KeyUp (camera_t& camera)
{
	Camera_clearMovementFlags(camera, MOVE_ROTRIGHT);
}

void Camera_PitchUp_KeyDown (camera_t& camera)
{
	Camera_setMovementFlags(camera, MOVE_PITCHUP);
}
void Camera_PitchUp_KeyUp (camera_t& camera)
{
	Camera_clearMovementFlags(camera, MOVE_PITCHUP);
}
void Camera_PitchDown_KeyDown (camera_t& camera)
{
	Camera_setMovementFlags(camera, MOVE_PITCHDOWN);
}
void Camera_PitchDown_KeyUp (camera_t& camera)
{
	Camera_clearMovementFlags(camera, MOVE_PITCHDOWN);
}

typedef ReferenceCaller<camera_t, &Camera_MoveForward_KeyDown> FreeMoveCameraMoveForwardKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveForward_KeyUp> FreeMoveCameraMoveForwardKeyUpCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveBack_KeyDown> FreeMoveCameraMoveBackKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveBack_KeyUp> FreeMoveCameraMoveBackKeyUpCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveLeft_KeyDown> FreeMoveCameraMoveLeftKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveLeft_KeyUp> FreeMoveCameraMoveLeftKeyUpCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveRight_KeyDown> FreeMoveCameraMoveRightKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveRight_KeyUp> FreeMoveCameraMoveRightKeyUpCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveUp_KeyDown> FreeMoveCameraMoveUpKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveUp_KeyUp> FreeMoveCameraMoveUpKeyUpCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveDown_KeyDown> FreeMoveCameraMoveDownKeyDownCaller;
typedef ReferenceCaller<camera_t, &Camera_MoveDown_KeyUp> FreeMoveCameraMoveDownKeyUpCaller;

#define SPEED_MOVE 32
#define SPEED_TURN 22.5
#define MIN_CAM_SPEED 10
#define MAX_CAM_SPEED 610
#define CAM_SPEED_STEP 50

void Camera_MoveForward_Discrete (camera_t& camera)
{
	Camera_Move_updateAxes(camera);
	Camera_setOrigin(camera, Camera_getOrigin(camera) + camera.forward * SPEED_MOVE);
}
void Camera_MoveBack_Discrete (camera_t& camera)
{
	Camera_Move_updateAxes(camera);
	Camera_setOrigin(camera, Camera_getOrigin(camera) + camera.forward * (-SPEED_MOVE));
}

void Camera_MoveUp_Discrete (camera_t& camera)
{
	Vector3 origin(Camera_getOrigin(camera));
	origin[2] += SPEED_MOVE;
	Camera_setOrigin(camera, origin);
}
void Camera_MoveDown_Discrete (camera_t& camera)
{
	Vector3 origin(Camera_getOrigin(camera));
	origin[2] -= SPEED_MOVE;
	Camera_setOrigin(camera, origin);
}

void Camera_MoveLeft_Discrete (camera_t& camera)
{
	Camera_Move_updateAxes(camera);
	Camera_setOrigin(camera, Camera_getOrigin(camera) + camera.right * (-SPEED_MOVE));
}
void Camera_MoveRight_Discrete (camera_t& camera)
{
	Camera_Move_updateAxes(camera);
	Camera_setOrigin(camera, Camera_getOrigin(camera) + camera.right * (SPEED_MOVE));
}

void Camera_RotateLeft_Discrete (camera_t& camera)
{
	Vector3 angles(Camera_getAngles(camera));
	angles[CAMERA_YAW] += SPEED_TURN;
	Camera_setAngles(camera, angles);
}
void Camera_RotateRight_Discrete (camera_t& camera)
{
	Vector3 angles(Camera_getAngles(camera));
	angles[CAMERA_YAW] -= SPEED_TURN;
	Camera_setAngles(camera, angles);
}

void Camera_PitchUp_Discrete (camera_t& camera)
{
	Vector3 angles(Camera_getAngles(camera));
	angles[CAMERA_PITCH] += SPEED_TURN;
	if (angles[CAMERA_PITCH] > 90)
		angles[CAMERA_PITCH] = 90;
	Camera_setAngles(camera, angles);
}
void Camera_PitchDown_Discrete (camera_t& camera)
{
	Vector3 angles(Camera_getAngles(camera));
	angles[CAMERA_PITCH] -= SPEED_TURN;
	if (angles[CAMERA_PITCH] < -90)
		angles[CAMERA_PITCH] = -90;
	Camera_setAngles(camera, angles);
}

class CameraView
{
		camera_t& m_camera;
		View* m_view;
		Callback m_update;
	public:
		CameraView (camera_t& camera, View* view, const Callback& update) :
			m_camera(camera), m_view(view), m_update(update)
		{
		}
		void update ()
		{
			m_view->Construct(m_camera.projection, m_camera.modelview, m_camera.width, m_camera.height);
			m_update();
		}
		void setModelview (const Matrix4& modelview)
		{
			m_camera.modelview = modelview;
			matrix4_multiply_by_matrix4(m_camera.modelview, g_radiant2opengl);
			matrix4_affine_invert(m_camera.modelview);
			Camera_updateVectors(m_camera);
			update();
		}
		void setFieldOfView (float fieldOfView)
		{
			float farClip = Camera_getFarClipPlane(m_camera);
			m_camera.projection = projection_for_camera(farClip / 4096.0f, farClip, fieldOfView, m_camera.width,
					m_camera.height);
			update();
		}
};

void Camera_motionDelta (int x, int y, unsigned int state, void* data)
{
	camera_t* cam = reinterpret_cast<camera_t*> (data);

	cam->m_mouseMove.motion_delta(x, y, state);

	switch (g_camwindow_globals_private.m_nStrafeMode) {
	case 0:
		cam->m_strafe = (state & GDK_CONTROL_MASK) != 0;
		if (cam->m_strafe)
			cam->m_strafe_forward = (state & GDK_SHIFT_MASK) != 0;
		else
			cam->m_strafe_forward = false;
		break;
	case 1:
		cam->m_strafe = (state & GDK_CONTROL_MASK) != 0 && (state & GDK_SHIFT_MASK) == 0;
		cam->m_strafe_forward = false;
		break;
	case 2:
		cam->m_strafe = (state & GDK_CONTROL_MASK) != 0 && (state & GDK_SHIFT_MASK) == 0;
		cam->m_strafe_forward = cam->m_strafe;
		break;
	}
}
#include "CamWnd.h"

Shader* CamWnd::m_state_select1 = 0;
Shader* CamWnd::m_state_select2 = 0;

CamWnd* NewCamWnd ()
{
	return new CamWnd;
}
void DeleteCamWnd (CamWnd* camwnd)
{
	delete camwnd;
}

void CamWnd_constructStatic ()
{
	CamWnd::captureStates();
}

void CamWnd_destroyStatic ()
{
	CamWnd::releaseStates();
}

static CamWnd* g_camwnd = 0;

void GlobalCamera_setCamWnd (CamWnd& camwnd)
{
	g_camwnd = &camwnd;
}

GtkWidget* CamWnd_getWidget (CamWnd& camwnd)
{
	return camwnd.m_gl_widget;
}

GtkWindow* CamWnd_getParent (CamWnd& camwnd)
{
	return camwnd.m_parent;
}

ToggleShown g_camera_shown(true);

void CamWnd_setParent (CamWnd& camwnd, GtkWindow* parent)
{
	camwnd.m_parent = parent;
	g_camera_shown.connect(GTK_WIDGET(camwnd.m_parent));
}

void CamWnd_Update (CamWnd& camwnd)
{
	camwnd.queue_draw();
}

camwindow_globals_t g_camwindow_globals;

const Vector3& Camera_getOrigin (CamWnd& camwnd)
{
	return Camera_getOrigin(camwnd.getCamera());
}

void Camera_setOrigin (CamWnd& camwnd, const Vector3& origin)
{
	Camera_setOrigin(camwnd.getCamera(), origin);
}

const Vector3& Camera_getAngles (CamWnd& camwnd)
{
	return Camera_getAngles(camwnd.getCamera());
}

void Camera_setAngles (CamWnd& camwnd, const Vector3& angles)
{
	Camera_setAngles(camwnd.getCamera(), angles);
}

// =============================================================================
// CamWnd class

gboolean enable_freelook_button_press (GtkWidget* widget, GdkEventButton* event, CamWnd* camwnd)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		camwnd->EnableFreeMove();
		return TRUE;
	}
	return FALSE;
}

gboolean disable_freelook_button_press (GtkWidget* widget, GdkEventButton* event, CamWnd* camwnd)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		camwnd->DisableFreeMove();
		return TRUE;
	}
	return FALSE;
}

void camwnd_update_xor_rectangle (CamWnd& self, rect_t area)
{
	if (GTK_WIDGET_VISIBLE(self.m_gl_widget)) {
		self.m_XORRectangle.set(
				rectangle_from_area(area.min, area.max, self.getCamera().width, self.getCamera().height));
	}
}

gboolean selection_button_press (GtkWidget* widget, GdkEventButton* event, WindowObserver* observer)
{
	if (event->type == GDK_BUTTON_PRESS) {
		observer->onMouseDown(WindowVector_forDouble(event->x, event->y), button_for_button(event->button),
				modifiers_for_state(event->state));
	}
	return FALSE;
}

/* greebo: GTK Callback: This gets called on "button_release_event" and basically just passes the call on
 * to the according window observer. */
gboolean selection_button_release (GtkWidget* widget, GdkEventButton* event, WindowObserver* observer)
{
	if (event->type == GDK_BUTTON_RELEASE) {
		observer->onMouseUp(WindowVector_forDouble(event->x, event->y), button_for_button(event->button),
				modifiers_for_state(event->state));
	}
	return FALSE;
}

void selection_motion (gdouble x, gdouble y, guint state, void* data)
{
	reinterpret_cast<WindowObserver*> (data)->onMouseMotion(WindowVector_forDouble(x, y), modifiers_for_state(state));
}

inline WindowVector windowvector_for_widget_centre (GtkWidget* widget)
{
	return WindowVector(static_cast<float> (widget->allocation.width / 2),
			static_cast<float> (widget->allocation.height / 2));
}

// greebo: The GTK Callback during freemove mode for mouseDown. Passes the call on to the Windowobserver
gboolean selection_button_press_freemove (GtkWidget* widget, GdkEventButton* event, WindowObserver* observer)
{
	if (event->type == GDK_BUTTON_PRESS) {
		observer->onMouseDown(windowvector_for_widget_centre(widget), button_for_button(event->button),
				modifiers_for_state(event->state));
	}
	return FALSE;
}

// greebo: The GTK Callback during freemove mode for mouseUp. Passes the call on to the Windowobserver
gboolean selection_button_release_freemove (GtkWidget* widget, GdkEventButton* event, WindowObserver* observer)
{
	if (event->type == GDK_BUTTON_RELEASE) {
		observer->onMouseUp(windowvector_for_widget_centre(widget), button_for_button(event->button),
				modifiers_for_state(event->state));
	}
	return FALSE;
}

// greebo: The GTK Callback during freemove mode for mouseMoved. Passes the call on to the Windowobserver
gboolean selection_motion_freemove (GtkWidget *widget, GdkEventMotion *event, WindowObserver* observer)
{
	observer->onMouseMotion(windowvector_for_widget_centre(widget), modifiers_for_state(event->state));
	return FALSE;
}

// greebo: The GTK Callback during freemove mode for scroll events.
gboolean wheelmove_scroll (GtkWidget* widget, GdkEventScroll* event, CamWnd* camwnd)
{
	// Determine the direction we are moving.
	if (event->direction == GDK_SCROLL_UP) {
		Camera_Freemove_updateAxes(camwnd->getCamera());
		Camera_setOrigin(*camwnd, Camera_getOrigin(*camwnd) + camwnd->getCamera().forward
				* static_cast<float> (g_camwindow_globals_private.m_nMoveSpeed));
	} else if (event->direction == GDK_SCROLL_DOWN) {
		Camera_Freemove_updateAxes(camwnd->getCamera());
		Camera_setOrigin(*camwnd, Camera_getOrigin(*camwnd) + camwnd->getCamera().forward
				* (-static_cast<float> (g_camwindow_globals_private.m_nMoveSpeed)));
	}

	return FALSE;
}

gboolean camera_size_allocate (GtkWidget* widget, GtkAllocation* allocation, CamWnd* camwnd)
{
	camwnd->getCamera().width = allocation->width;
	camwnd->getCamera().height = allocation->height;
	Camera_updateProjection(camwnd->getCamera());
	camwnd->m_window_observer->onSizeChanged(camwnd->getCamera().width, camwnd->getCamera().height);
	camwnd->queue_draw();
	return FALSE;
}

gboolean camera_expose (GtkWidget* widget, GdkEventExpose* event, gpointer data)
{
	reinterpret_cast<CamWnd*> (data)->draw();
	return FALSE;
}

void KeyEvent_connect (const char* name)
{
	const KeyEvent& keyEvent = GlobalKeyEvents_find(name);
	keydown_accelerators_add(keyEvent.m_accelerator, keyEvent.m_keyDown);
	keyup_accelerators_add(keyEvent.m_accelerator, keyEvent.m_keyUp);
}

void KeyEvent_disconnect (const char* name)
{
	const KeyEvent& keyEvent = GlobalKeyEvents_find(name);
	keydown_accelerators_remove(keyEvent.m_accelerator);
	keyup_accelerators_remove(keyEvent.m_accelerator);
}

void CamWnd_registerCommands (CamWnd& camwnd)
{
	GlobalKeyEvents_insert("CameraForward", Accelerator(GDK_Up),
			ReferenceCaller<camera_t, Camera_MoveForward_KeyDown> (camwnd.getCamera()), ReferenceCaller<camera_t,
					Camera_MoveForward_KeyUp> (camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraBack", Accelerator(GDK_Down), ReferenceCaller<camera_t, Camera_MoveBack_KeyDown> (
			camwnd.getCamera()), ReferenceCaller<camera_t, Camera_MoveBack_KeyUp> (camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraLeft", Accelerator(GDK_Left), ReferenceCaller<camera_t, Camera_RotateLeft_KeyDown> (
			camwnd.getCamera()), ReferenceCaller<camera_t, Camera_RotateLeft_KeyUp> (camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraRight", Accelerator(GDK_Right),
			ReferenceCaller<camera_t, Camera_RotateRight_KeyDown> (camwnd.getCamera()), ReferenceCaller<camera_t,
					Camera_RotateRight_KeyUp> (camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraStrafeRight", Accelerator(GDK_period), ReferenceCaller<camera_t,
			Camera_MoveRight_KeyDown> (camwnd.getCamera()), ReferenceCaller<camera_t, Camera_MoveRight_KeyUp> (
			camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraStrafeLeft", Accelerator(GDK_comma), ReferenceCaller<camera_t,
			Camera_MoveLeft_KeyDown> (camwnd.getCamera()), ReferenceCaller<camera_t, Camera_MoveLeft_KeyUp> (
			camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraUp", Accelerator('D'), ReferenceCaller<camera_t, Camera_MoveUp_KeyDown> (
			camwnd.getCamera()), ReferenceCaller<camera_t, Camera_MoveUp_KeyUp> (camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraDown", Accelerator('C'), ReferenceCaller<camera_t, Camera_MoveDown_KeyDown> (
			camwnd.getCamera()), ReferenceCaller<camera_t, Camera_MoveDown_KeyUp> (camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraAngleDown", Accelerator('A'), ReferenceCaller<camera_t, Camera_PitchDown_KeyDown> (
			camwnd.getCamera()), ReferenceCaller<camera_t, Camera_PitchDown_KeyUp> (camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraAngleUp", Accelerator('Z'), ReferenceCaller<camera_t, Camera_PitchUp_KeyDown> (
			camwnd.getCamera()), ReferenceCaller<camera_t, Camera_PitchUp_KeyUp> (camwnd.getCamera()));

	GlobalKeyEvents_insert("CameraFreeMoveForward", Accelerator(GDK_Up), FreeMoveCameraMoveForwardKeyDownCaller(
			camwnd.getCamera()), FreeMoveCameraMoveForwardKeyUpCaller(camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraFreeMoveBack", Accelerator(GDK_Down), FreeMoveCameraMoveBackKeyDownCaller(
			camwnd.getCamera()), FreeMoveCameraMoveBackKeyUpCaller(camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraFreeMoveLeft", Accelerator(GDK_Left), FreeMoveCameraMoveLeftKeyDownCaller(
			camwnd.getCamera()), FreeMoveCameraMoveLeftKeyUpCaller(camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraFreeMoveRight", Accelerator(GDK_Right), FreeMoveCameraMoveRightKeyDownCaller(
			camwnd.getCamera()), FreeMoveCameraMoveRightKeyUpCaller(camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraFreeMoveUp", Accelerator('D'), FreeMoveCameraMoveUpKeyDownCaller(camwnd.getCamera()),
			FreeMoveCameraMoveUpKeyUpCaller(camwnd.getCamera()));
	GlobalKeyEvents_insert("CameraFreeMoveDown", Accelerator('C'), FreeMoveCameraMoveDownKeyDownCaller(
			camwnd.getCamera()), FreeMoveCameraMoveDownKeyUpCaller(camwnd.getCamera()));

	GlobalRadiant().commandInsert("CameraForward", ReferenceCaller<camera_t, Camera_MoveForward_Discrete> (
			camwnd.getCamera()), Accelerator(GDK_Up));
	GlobalRadiant().commandInsert("CameraBack",
			ReferenceCaller<camera_t, Camera_MoveBack_Discrete> (camwnd.getCamera()), Accelerator(GDK_Down));
	GlobalRadiant().commandInsert("CameraLeft", ReferenceCaller<camera_t, Camera_RotateLeft_Discrete> (
			camwnd.getCamera()), Accelerator(GDK_Left));
	GlobalRadiant().commandInsert("CameraRight", ReferenceCaller<camera_t, Camera_RotateRight_Discrete> (
			camwnd.getCamera()), Accelerator(GDK_Right));
	GlobalRadiant().commandInsert("CameraStrafeRight", ReferenceCaller<camera_t, Camera_MoveRight_Discrete> (
			camwnd.getCamera()), Accelerator(GDK_period));
	GlobalRadiant().commandInsert("CameraStrafeLeft", ReferenceCaller<camera_t, Camera_MoveLeft_Discrete> (
			camwnd.getCamera()), Accelerator(GDK_comma));

	GlobalRadiant().commandInsert("CameraUp", ReferenceCaller<camera_t, Camera_MoveUp_Discrete> (camwnd.getCamera()),
			Accelerator('D'));
	GlobalRadiant().commandInsert("CameraDown",
			ReferenceCaller<camera_t, Camera_MoveDown_Discrete> (camwnd.getCamera()), Accelerator('C'));
	GlobalRadiant().commandInsert("CameraAngleUp", ReferenceCaller<camera_t, Camera_PitchUp_Discrete> (
			camwnd.getCamera()), Accelerator('A'));
	GlobalRadiant().commandInsert("CameraAngleDown", ReferenceCaller<camera_t, Camera_PitchDown_Discrete> (
			camwnd.getCamera()), Accelerator('Z'));
}

void CamWnd_Move_Enable (CamWnd& camwnd)
{
	KeyEvent_connect("CameraForward");
	KeyEvent_connect("CameraBack");
	KeyEvent_connect("CameraLeft");
	KeyEvent_connect("CameraRight");
	KeyEvent_connect("CameraStrafeRight");
	KeyEvent_connect("CameraStrafeLeft");
	KeyEvent_connect("CameraUp");
	KeyEvent_connect("CameraDown");
	KeyEvent_connect("CameraAngleUp");
	KeyEvent_connect("CameraAngleDown");
}

void CamWnd_Move_Disable (CamWnd& camwnd)
{
	KeyEvent_disconnect("CameraForward");
	KeyEvent_disconnect("CameraBack");
	KeyEvent_disconnect("CameraLeft");
	KeyEvent_disconnect("CameraRight");
	KeyEvent_disconnect("CameraStrafeRight");
	KeyEvent_disconnect("CameraStrafeLeft");
	KeyEvent_disconnect("CameraUp");
	KeyEvent_disconnect("CameraDown");
	KeyEvent_disconnect("CameraAngleUp");
	KeyEvent_disconnect("CameraAngleDown");
}

void CamWnd_Move_Discrete_Enable (CamWnd& camwnd)
{
	command_connect_accelerator("CameraForward");
	command_connect_accelerator("CameraBack");
	command_connect_accelerator("CameraLeft");
	command_connect_accelerator("CameraStrafeLeft");
	command_connect_accelerator("CameraRight");
	command_connect_accelerator("CameraStrafeRight");
	command_connect_accelerator("CameraUp");
	command_connect_accelerator("CameraDown");
	command_connect_accelerator("CameraAngleUp");
	command_connect_accelerator("CameraAngleDown");
#if 0
	command_connect_accelerator("CameraFreeMoveForward");
	command_connect_accelerator("CameraFreeMoveBack");
	command_connect_accelerator("CameraFreeMoveLeft");
	command_connect_accelerator("CameraFreeMoveRight");
	command_connect_accelerator("CameraFreeMoveUp");
	command_connect_accelerator("CameraFreeMoveDown");
	command_connect_accelerator("ToggleCamera");
#endif
}

void CamWnd_Move_Discrete_Disable (CamWnd& camwnd)
{
	command_disconnect_accelerator("CameraForward");
	command_disconnect_accelerator("CameraBack");
	command_disconnect_accelerator("CameraLeft");
	command_disconnect_accelerator("CameraRight");
	command_disconnect_accelerator("CameraStrafeRight");
	command_disconnect_accelerator("CameraStrafeLeft");
	command_disconnect_accelerator("CameraUp");
	command_disconnect_accelerator("CameraDown");
	command_disconnect_accelerator("CameraAngleUp");
	command_disconnect_accelerator("CameraAngleDown");
}

void CamWnd_Move_Discrete_Import (CamWnd& camwnd, bool value)
{
	if (g_camwindow_globals_private.m_bCamDiscrete) {
		CamWnd_Move_Discrete_Disable(camwnd);
	} else {
		CamWnd_Move_Disable(camwnd);
	}

	g_camwindow_globals_private.m_bCamDiscrete = value;

	if (g_camwindow_globals_private.m_bCamDiscrete) {
		CamWnd_Move_Discrete_Enable(camwnd);
	} else {
		CamWnd_Move_Enable(camwnd);
	}
}

void CamWnd_Move_Discrete_Import (bool value)
{
	if (g_camwnd != 0) {
		CamWnd_Move_Discrete_Import(*g_camwnd, value);
	} else {
		g_camwindow_globals_private.m_bCamDiscrete = value;
	}
}

/**
 * @sa CamWnd_Remove_Handlers_Move
 * @sa CamWnd_Add_Handlers_FreeMove
 */
static void CamWnd_Add_Handlers_Move (CamWnd& camwnd)
{
	camwnd.m_selection_button_press_handler = g_signal_connect(G_OBJECT(camwnd.m_gl_widget), "button_press_event",
			G_CALLBACK(selection_button_press), camwnd.m_window_observer);
	camwnd.m_selection_button_release_handler = g_signal_connect(G_OBJECT(camwnd.m_gl_widget), "button_release_event",
			G_CALLBACK(selection_button_release), camwnd.m_window_observer);
	camwnd.m_selection_motion_handler = g_signal_connect(G_OBJECT(camwnd.m_gl_widget), "motion_notify_event",
			G_CALLBACK(DeferredMotion::gtk_motion), &camwnd.m_deferred_motion);

	camwnd.m_freelook_button_press_handler = g_signal_connect(G_OBJECT(camwnd.m_gl_widget), "button_press_event",
			G_CALLBACK(enable_freelook_button_press), &camwnd);

	if (g_camwindow_globals_private.m_bCamDiscrete) {
		CamWnd_Move_Discrete_Enable(camwnd);
	} else {
		CamWnd_Move_Enable(camwnd);
	}
}

/**
 * @sa CamWnd_Add_Handlers_Move
 */
static void CamWnd_Remove_Handlers_Move (CamWnd& camwnd)
{
	g_signal_handler_disconnect(G_OBJECT(camwnd.m_gl_widget), camwnd.m_selection_button_press_handler);
	g_signal_handler_disconnect(G_OBJECT(camwnd.m_gl_widget), camwnd.m_selection_button_release_handler);
	g_signal_handler_disconnect(G_OBJECT(camwnd.m_gl_widget), camwnd.m_selection_motion_handler);

	g_signal_handler_disconnect(G_OBJECT(camwnd.m_gl_widget), camwnd.m_freelook_button_press_handler);

	if (g_camwindow_globals_private.m_bCamDiscrete) {
		CamWnd_Move_Discrete_Disable(camwnd);
	} else {
		CamWnd_Move_Disable(camwnd);
	}
}

/**
 * @sa CamWnd_Remove_Handlers_FreeMove
 * @sa CamWnd_Add_Handlers_Move
 */
static void CamWnd_Add_Handlers_FreeMove (CamWnd& camwnd)
{
	camwnd.m_selection_button_press_handler = g_signal_connect(G_OBJECT(camwnd.m_gl_widget), "button_press_event",
			G_CALLBACK(selection_button_press_freemove), camwnd.m_window_observer);
	camwnd.m_selection_button_release_handler = g_signal_connect(G_OBJECT(camwnd.m_gl_widget), "button_release_event",
			G_CALLBACK(selection_button_release_freemove), camwnd.m_window_observer);
	camwnd.m_selection_motion_handler = g_signal_connect(G_OBJECT(camwnd.m_gl_widget), "motion_notify_event",
			G_CALLBACK(selection_motion_freemove), camwnd.m_window_observer);

	camwnd.m_freelook_button_press_handler = g_signal_connect(G_OBJECT(camwnd.m_gl_widget), "button_press_event",
			G_CALLBACK(disable_freelook_button_press), &camwnd);

	KeyEvent_connect("CameraFreeMoveForward");
	KeyEvent_connect("CameraFreeMoveBack");
	KeyEvent_connect("CameraFreeMoveLeft");
	KeyEvent_connect("CameraFreeMoveRight");
	KeyEvent_connect("CameraFreeMoveUp");
	KeyEvent_connect("CameraFreeMoveDown");
}

/**
 * @sa CamWnd_Add_Handlers_FreeMove
 */
static void CamWnd_Remove_Handlers_FreeMove (CamWnd& camwnd)
{
	KeyEvent_disconnect("CameraFreeMoveForward");
	KeyEvent_disconnect("CameraFreeMoveBack");
	KeyEvent_disconnect("CameraFreeMoveLeft");
	KeyEvent_disconnect("CameraFreeMoveRight");
	KeyEvent_disconnect("CameraFreeMoveUp");
	KeyEvent_disconnect("CameraFreeMoveDown");

	g_signal_handler_disconnect(G_OBJECT(camwnd.m_gl_widget), camwnd.m_selection_button_press_handler);
	g_signal_handler_disconnect(G_OBJECT(camwnd.m_gl_widget), camwnd.m_selection_button_release_handler);
	g_signal_handler_disconnect(G_OBJECT(camwnd.m_gl_widget), camwnd.m_selection_motion_handler);

	g_signal_handler_disconnect(G_OBJECT(camwnd.m_gl_widget), camwnd.m_freelook_button_press_handler);
}

CamWnd::CamWnd () :
	m_view(true), m_Camera(&m_view, CamWndQueueDraw(*this)), m_cameraview(m_Camera, &m_view, ReferenceCaller<CamWnd,
			CamWnd_Update> (*this)), _glWidget(true), m_gl_widget(static_cast<GtkWidget*>(_glWidget)), m_window_observer(NewWindowObserver()),
			m_XORRectangle(m_gl_widget), m_deferredDraw(WidgetQueueDrawCaller(*m_gl_widget)), m_deferred_motion(
					selection_motion, m_window_observer), m_selection_button_press_handler(0),
			m_selection_button_release_handler(0), m_selection_motion_handler(0), m_freelook_button_press_handler(0),
			m_drawing(false)
{
	m_bFreeMove = false;

	GlobalWindowObservers_add(m_window_observer);
	GlobalWindowObservers_connectWidget(m_gl_widget);

	m_window_observer->setRectangleDrawCallback(ReferenceCaller1<CamWnd, rect_t, camwnd_update_xor_rectangle> (*this));
	m_window_observer->setView(m_view);

	gtk_widget_ref(m_gl_widget);

	gtk_widget_set_events(m_gl_widget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
	GTK_WIDGET_SET_FLAGS(m_gl_widget, GTK_CAN_FOCUS);
	gtk_widget_set_size_request(m_gl_widget, CAMWND_MINSIZE_X, CAMWND_MINSIZE_Y);

	m_sizeHandler = g_signal_connect(G_OBJECT(m_gl_widget), "size_allocate", G_CALLBACK(camera_size_allocate), this);
	m_exposeHandler = g_signal_connect(G_OBJECT(m_gl_widget), "expose_event", G_CALLBACK(camera_expose), this);

	Map_addValidCallback(g_map, DeferredDrawOnMapValidChangedCaller(m_deferredDraw));

	CamWnd_registerCommands(*this);

	CamWnd_Add_Handlers_Move(*this);

	g_signal_connect(G_OBJECT(m_gl_widget), "scroll_event", G_CALLBACK(wheelmove_scroll), this);

	AddSceneChangeCallback(ReferenceCaller<CamWnd, CamWnd_Update> (*this));

	PressedButtons_connect(g_pressedButtons, m_gl_widget);
}

CamWnd::~CamWnd ()
{
	if (m_bFreeMove) {
		DisableFreeMove();
	}

	CamWnd_Remove_Handlers_Move(*this);

	g_signal_handler_disconnect(G_OBJECT(m_gl_widget), m_sizeHandler);
	g_signal_handler_disconnect(G_OBJECT(m_gl_widget), m_exposeHandler);

	gtk_widget_unref(m_gl_widget);

	delete m_window_observer;
}

void CamWnd::Cam_ChangeFloor (bool up)
{
	if (up) {
		m_Camera.origin[2] += 64;
	} else {
		m_Camera.origin[2] -= 64;
	}

	if (m_Camera.origin[2] < 0)
		m_Camera.origin[2] = 0;

	Camera_updateModelview(getCamera());
	CamWnd_Update(*this);
	CameraMovedNotify();
}

// NOTE TTimo if there's an OS-level focus out of the application
//   then we can release the camera cursor grab
static gboolean camwindow_freemove_focusout (GtkWidget* widget, GdkEventFocus* event, gpointer data)
{
	reinterpret_cast<CamWnd*> (data)->DisableFreeMove();
	return FALSE;
}

void CamWnd::EnableFreeMove ()
{
	ASSERT_MESSAGE(!m_bFreeMove, "EnableFreeMove: free-move was already enabled");
	m_bFreeMove = true;
	Camera_clearMovementFlags(getCamera(), MOVE_ALL);

	CamWnd_Remove_Handlers_Move(*this);
	CamWnd_Add_Handlers_FreeMove(*this);

	gtk_window_set_focus(m_parent, m_gl_widget);
	m_freemove_handle_focusout = g_signal_connect(G_OBJECT(m_gl_widget), "focus_out_event", G_CALLBACK(
					camwindow_freemove_focusout), this);
	m_freezePointer.freeze_pointer(m_parent, Camera_motionDelta, &m_Camera);

	CamWnd_Update(*this);
}

void CamWnd::DisableFreeMove ()
{
	ASSERT_MESSAGE(m_bFreeMove, "DisableFreeMove: free-move was not enabled");
	m_bFreeMove = false;
	Camera_clearMovementFlags(getCamera(), MOVE_ALL);

	CamWnd_Remove_Handlers_FreeMove(*this);
	CamWnd_Add_Handlers_Move(*this);

	m_freezePointer.unfreeze_pointer(m_parent);
	g_signal_handler_disconnect(G_OBJECT(m_gl_widget), m_freemove_handle_focusout);

	CamWnd_Update(*this);
}

/*
 ==============
 Cam_Draw
 ==============
 */

void Camera_SetStats (bool value)
{
	g_camwindow_globals_private.m_showStats = value;
}
void ShowStatsToggle ()
{
	g_camwindow_globals_private.m_showStats ^= 1;
}
typedef FreeCaller<ShowStatsToggle> ShowStatsToggleCaller;

void ShowStatsExport (const BoolImportCallback& importer)
{
	importer(g_camwindow_globals_private.m_showStats);
}
typedef FreeCaller1<const BoolImportCallback&, ShowStatsExport> ShowStatsExportCaller;

ShowStatsExportCaller g_show_stats_caller;
BoolExportCallback g_show_stats_callback(g_show_stats_caller);
ToggleItem g_show_stats(g_show_stats_callback);

void CamWnd::Cam_Draw ()
{
	glViewport(0, 0, m_Camera.width, m_Camera.height);

	// enable depth buffer writes
	glDepthMask(GL_TRUE);

	Vector3 clearColour(0, 0, 0);
	if (m_Camera.draw_mode != cd_lighting) {
		clearColour = g_camwindow_globals.color_cameraback;
	}

	glClearColor(clearColour[0], clearColour[1], clearColour[2], 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	render::RenderStatistics::Instance().resetStats();

	extern void Cull_ResetStats ();
	Cull_ResetStats();

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(m_Camera.projection);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(m_Camera.modelview);

	// one directional light source directly behind the viewer
	{
		GLfloat inverse_cam_dir[4], ambient[4], diffuse[4];//, material[4];

		ambient[0] = ambient[1] = ambient[2] = 0.4f;
		ambient[3] = 1.0f;
		diffuse[0] = diffuse[1] = diffuse[2] = 0.4f;
		diffuse[3] = 1.0f;

		inverse_cam_dir[0] = m_Camera.vpn[0];
		inverse_cam_dir[1] = m_Camera.vpn[1];
		inverse_cam_dir[2] = m_Camera.vpn[2];
		inverse_cam_dir[3] = 0;

		glLightfv(GL_LIGHT0, GL_POSITION, inverse_cam_dir);

		glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);

		glEnable(GL_LIGHT0);
	}

	unsigned int globalstate = RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_ALPHATEST
			| RENDER_BLEND | RENDER_CULLFACE | RENDER_COLOURARRAY | RENDER_COLOURCHANGE;
	switch (m_Camera.draw_mode) {
	case cd_wire:
		break;
	case cd_solid:
		globalstate |= (RENDER_FILL | RENDER_LIGHTING | RENDER_SMOOTH | RENDER_SCALED);
		break;
	case cd_texture:
		globalstate |= (RENDER_FILL | RENDER_LIGHTING | RENDER_TEXTURE_2D | RENDER_SMOOTH | RENDER_SCALED);
		break;
	case cd_lighting:
		globalstate |= (RENDER_FILL | RENDER_LIGHTING | RENDER_TEXTURE_2D | RENDER_SMOOTH | RENDER_SCALED | RENDER_SCREEN);
		break;
	default:
		globalstate = 0;
		break;
	}

	if (!g_xywindow_globals.m_bNoStipple) {
		globalstate |= RENDER_LINESTIPPLE;
	}

	{
		CamRenderer renderer(globalstate, m_state_select2, m_state_select1, m_view.getViewer());

		Scene_Render(renderer, m_view);

		renderer.render(m_Camera.modelview, m_Camera.projection);
	}

	// prepare for 2d stuff
	glColor4f(1, 1, 1, 1);
	glDisable(GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, (float) m_Camera.width, 0, (float) m_Camera.height, -100, 100);
	glScalef(1, -1, 1);
	glTranslatef(0, -(float) m_Camera.height, 0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if (GlobalOpenGL().GL_1_3()) {
		glClientActiveTexture(GL_TEXTURE0);
		glActiveTexture(GL_TEXTURE0);
	}

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_DEPTH_TEST);
	glColor3f(1.f, 1.f, 1.f);
	glLineWidth(1);

	// draw the crosshair
	if (m_bFreeMove) {
		glBegin(GL_LINES);
		glVertex2f((float) m_Camera.width / 2.f, (float) m_Camera.height / 2.f + 6);
		glVertex2f((float) m_Camera.width / 2.f, (float) m_Camera.height / 2.f + 2);
		glVertex2f((float) m_Camera.width / 2.f, (float) m_Camera.height / 2.f - 6);
		glVertex2f((float) m_Camera.width / 2.f, (float) m_Camera.height / 2.f - 2);
		glVertex2f((float) m_Camera.width / 2.f + 6, (float) m_Camera.height / 2.f);
		glVertex2f((float) m_Camera.width / 2.f + 2, (float) m_Camera.height / 2.f);
		glVertex2f((float) m_Camera.width / 2.f - 6, (float) m_Camera.height / 2.f);
		glVertex2f((float) m_Camera.width / 2.f - 2, (float) m_Camera.height / 2.f);
		glEnd();
	}

	if (g_camwindow_globals_private.m_showStats) {
		glRasterPos3f(1.0f, static_cast<float> (m_Camera.height) - 1.0f, 0.0f);
		GlobalOpenGL().drawString(render::RenderStatistics::Instance().getStatString());

		glRasterPos3f(1.0f, static_cast<float> (m_Camera.height) - 11.0f, 0.0f);
		extern const char* Cull_GetStats ();
		GlobalOpenGL().drawString(Cull_GetStats());
	}

	// bind back to the default texture so that we don't have problems
	// elsewhere using/modifying texture maps between contexts
	glBindTexture(GL_TEXTURE_2D, 0);
}

void CamWnd::draw ()
{
	m_drawing = true;

	gtkutil::GLWidgetSentry sentry(m_gl_widget);
	if (Map_Valid(g_map) && ScreenUpdates_Enabled()) {
		Cam_Draw();

		m_XORRectangle.set(Rectangle());
	}

	m_drawing = false;
}

void CamWnd::BenchMark ()
{
	const double dStart = clock() / 1000.0;
	for (int i = 0; i < 100; i++) {
		Vector3 angles;
		angles[CAMERA_ROLL] = 0;
		angles[CAMERA_PITCH] = 0;
		angles[CAMERA_YAW] = static_cast<float> (i * (360.0 / 100.0));
		Camera_setAngles(*this, angles);
	}
	const double dEnd = clock() / 1000.0;
	globalOutputStream() << FloatFormat(dEnd - dStart, 5, 2) << " seconds\n";
}

void GlobalCamera_ResetAngles ()
{
	CamWnd& camwnd = *g_camwnd;
	Vector3 angles;
	angles[CAMERA_ROLL] = angles[CAMERA_PITCH] = 0;
	angles[CAMERA_YAW] = static_cast<float> (22.5 * floor((Camera_getAngles(camwnd)[CAMERA_YAW] + 11) / 22.5));
	Camera_setAngles(camwnd, angles);
}

void Camera_ChangeFloorUp ()
{
	CamWnd& camwnd = *g_camwnd;
	camwnd.Cam_ChangeFloor(true);
}

void Camera_ChangeFloorDown ()
{
	CamWnd& camwnd = *g_camwnd;
	camwnd.Cam_ChangeFloor(false);
}

void Camera_CubeIn ()
{
	CamWnd& camwnd = *g_camwnd;
	g_camwindow_globals.m_nCubicScale--;
	if (g_camwindow_globals.m_nCubicScale < 1)
		g_camwindow_globals.m_nCubicScale = 1;
	Camera_updateProjection(camwnd.getCamera());
	CamWnd_Update(camwnd);
}

void Camera_CubeOut ()
{
	CamWnd& camwnd = *g_camwnd;
	g_camwindow_globals.m_nCubicScale++;
	if (g_camwindow_globals.m_nCubicScale > 23)
		g_camwindow_globals.m_nCubicScale = 23;
	Camera_updateProjection(camwnd.getCamera());
	CamWnd_Update(camwnd);
}

bool Camera_GetFarClip ()
{
	return g_camwindow_globals_private.m_bCubicClipping;
}

BoolExportCaller g_getfarclip_caller(g_camwindow_globals_private.m_bCubicClipping);
ToggleItem g_getfarclip_item(g_getfarclip_caller);

void Camera_SetFarClip (bool value)
{
	CamWnd& camwnd = *g_camwnd;
	g_camwindow_globals_private.m_bCubicClipping = value;
	g_getfarclip_item.update();
	Camera_updateProjection(camwnd.getCamera());
	CamWnd_Update(camwnd);
}

void Camera_ToggleFarClip ()
{
	Camera_SetFarClip(!Camera_GetFarClip());
}

void CamWnd_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, _("Cubic clip the camera view (\\)"), ui::icons::ICON_VIEW_CLIPPING,
			"ToggleCubicClip");
}

void CamWnd_registerShortcuts ()
{
	toggle_add_accelerator("ToggleCubicClip");

	command_connect_accelerator("CameraSpeedInc");
	command_connect_accelerator("CameraSpeedDec");
}

void GlobalCamera_Benchmark ()
{
	CamWnd& camwnd = *g_camwnd;
	camwnd.BenchMark();
}

void GlobalCamera_Update ()
{
	CamWnd& camwnd = *g_camwnd;
	CamWnd_Update(camwnd);
}

camera_draw_mode CamWnd_GetMode ()
{
	return camera_t::draw_mode;
}
void CamWnd_SetMode (camera_draw_mode mode)
{
	camera_t::draw_mode = mode;
	if (g_camwnd != 0) {
		CamWnd_Update(*g_camwnd);
	}
}

class CameraModel
{
	public:
		STRING_CONSTANT(Name, "CameraModel");
		virtual ~CameraModel ()
		{
		}
		virtual void setCameraView (CameraView* view, const Callback& disconnect) = 0;
};

static CameraModel* g_camera_model = 0;

void CamWnd_LookThroughCamera (CamWnd& camwnd)
{
	if (g_camera_model != 0) {
		CamWnd_Add_Handlers_Move(camwnd);
		g_camera_model->setCameraView(0, Callback());
		g_camera_model = 0;
		Camera_updateModelview(camwnd.getCamera());
		Camera_updateProjection(camwnd.getCamera());
		CamWnd_Update(camwnd);
	}
}

inline CameraModel* Instance_getCameraModel (scene::Instance& instance)
{
	return dynamic_cast<CameraModel*>(&instance);
}

void CamWnd_LookThroughSelected (CamWnd& camwnd)
{
	if (g_camera_model != 0) {
		CamWnd_LookThroughCamera(camwnd);
	}

	if (GlobalSelectionSystem().countSelected() != 0) {
		scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
		CameraModel* cameraModel = Instance_getCameraModel(instance);
		if (cameraModel != 0) {
			CamWnd_Remove_Handlers_Move(camwnd);
			g_camera_model = cameraModel;
			g_camera_model->setCameraView(&camwnd.getCameraView(), ReferenceCaller<CamWnd, CamWnd_LookThroughCamera> (
					camwnd));
		}
	}
}

void GlobalCamera_LookThroughSelected ()
{
	CamWnd_LookThroughSelected(*g_camwnd);
}

void GlobalCamera_LookThroughCamera ()
{
	CamWnd_LookThroughCamera(*g_camwnd);
}

void RenderModeImport (int value)
{
	switch (value) {
	case 0:
		CamWnd_SetMode(cd_wire);
		break;
	case 1:
		CamWnd_SetMode(cd_solid);
		break;
	case 2:
		CamWnd_SetMode(cd_texture);
		break;
	case 3:
		CamWnd_SetMode(cd_lighting);
		break;
	default:
		CamWnd_SetMode(cd_texture);
	}
}
typedef FreeCaller1<int, RenderModeImport> RenderModeImportCaller;

void RenderModeExport (const IntImportCallback& importer)
{
	switch (CamWnd_GetMode()) {
	case cd_wire:
		importer(0);
		break;
	case cd_solid:
		importer(1);
		break;
	case cd_texture:
		importer(2);
		break;
	case cd_lighting:
		importer(3);
		break;
	}
}
typedef FreeCaller1<const IntImportCallback&, RenderModeExport> RenderModeExportCaller;

void Camera_constructPreferences (PreferencesPage& page)
{
	page.appendSlider(_("Movement Speed"), g_camwindow_globals_private.m_nMoveSpeed, TRUE, 0, 0, 100, MIN_CAM_SPEED,
			MAX_CAM_SPEED, 1, 10, 10);
	page.appendCheckBox("", _("Link strafe speed to movement speed"), g_camwindow_globals_private.m_bCamLinkSpeed);
	page.appendSlider(_("Rotation Speed"), g_camwindow_globals_private.m_nAngleSpeed, TRUE, 0, 0, 3, 1, 180, 1, 10, 10);
	page.appendCheckBox("", _("Invert mouse vertical axis"), g_camwindow_globals_private.m_bCamInverseMouse);
	page.appendCheckBox("", _("Discrete movement"), FreeCaller1<bool, CamWnd_Move_Discrete_Import> (),
			BoolExportCaller(g_camwindow_globals_private.m_bCamDiscrete));
	page.appendCheckBox("", _("Enable far-clip plane"), FreeCaller1<bool, Camera_SetFarClip> (), BoolExportCaller(
			g_camwindow_globals_private.m_bCubicClipping));
	page.appendCheckBox("", _("Enable statistics"), FreeCaller1<bool, Camera_SetStats> (), BoolExportCaller(
			g_camwindow_globals_private.m_showStats));

	const char* render_mode[] = { C_("Render Mode", "Wireframe"), C_("Render Mode", "Flatshade"),
			C_("Render Mode", "Textured") };
	page.appendCombo(_("Render Mode"), STRING_ARRAY_RANGE(render_mode), IntImportCallback(RenderModeImportCaller()),
			IntExportCallback(RenderModeExportCaller()));

	const char* strafe_mode[] = { C_("Strafe Mode", "Both"), C_("Strafe Mode", "Forward"), C_("Strafe Mode", "Up") };
	page.appendCombo(_("Strafe Mode"), g_camwindow_globals_private.m_nStrafeMode, STRING_ARRAY_RANGE(strafe_mode));
}

void Camera_constructPage (PreferenceGroup& group)
{
	PreferencesPage page(group.createPage(_("Camera"), _("Camera View Preferences")));
	Camera_constructPreferences(page);
}
void Camera_registerPreferencesPage ()
{
	PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, Camera_constructPage> ());
}

#include "preferencesystem.h"
#include "stringio.h"
#include "../dialog.h"

typedef FreeCaller1<bool, CamWnd_Move_Discrete_Import> CamWndMoveDiscreteImportCaller;

void CameraSpeed_increase ()
{
	if (g_camwindow_globals_private.m_nMoveSpeed <= (MAX_CAM_SPEED - CAM_SPEED_STEP - 10)) {
		g_camwindow_globals_private.m_nMoveSpeed += CAM_SPEED_STEP;
	} else {
		g_camwindow_globals_private.m_nMoveSpeed = MAX_CAM_SPEED - 10;
	}
}

void CameraSpeed_decrease ()
{
	if (g_camwindow_globals_private.m_nMoveSpeed >= (MIN_CAM_SPEED + CAM_SPEED_STEP)) {
		g_camwindow_globals_private.m_nMoveSpeed -= CAM_SPEED_STEP;
	} else {
		g_camwindow_globals_private.m_nMoveSpeed = MIN_CAM_SPEED;
	}
}

/// \brief Initialisation for things that have the same lifespan as this module.
void CamWnd_Construct ()
{
	GlobalRadiant().commandInsert("CenterView", FreeCaller<GlobalCamera_ResetAngles> (), Accelerator(GDK_End));

	GlobalToggles_insert("ToggleCubicClip", FreeCaller<Camera_ToggleFarClip> (), ToggleItem::AddCallbackCaller(
			g_getfarclip_item), Accelerator('\\', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("CubicClipZoomIn", FreeCaller<Camera_CubeIn> (), Accelerator('[',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("CubicClipZoomOut", FreeCaller<Camera_CubeOut> (), Accelerator(']',
			(GdkModifierType) GDK_CONTROL_MASK));

	GlobalRadiant().commandInsert("UpFloor", FreeCaller<Camera_ChangeFloorUp> (), Accelerator(GDK_Prior));
	GlobalRadiant().commandInsert("DownFloor", FreeCaller<Camera_ChangeFloorDown> (), Accelerator(GDK_Next));

	GlobalToggles_insert("ToggleCamera", ToggleShown::ToggleCaller(g_camera_shown), ToggleItem::AddCallbackCaller(
			g_camera_shown.m_item), Accelerator('C', (GdkModifierType) (GDK_SHIFT_MASK | GDK_CONTROL_MASK)));
	GlobalCommands_insert("LookThroughSelected", FreeCaller<GlobalCamera_LookThroughSelected> ());
	GlobalCommands_insert("LookThroughCamera", FreeCaller<GlobalCamera_LookThroughCamera> ());

	GlobalRadiant().commandInsert("CameraSpeedInc", FreeCaller<CameraSpeed_increase> (), Accelerator(GDK_KP_Add,
			(GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("CameraSpeedDec", FreeCaller<CameraSpeed_decrease> (), Accelerator(GDK_KP_Subtract,
			(GdkModifierType) GDK_SHIFT_MASK));

	GlobalShortcuts_insert("CameraForward", Accelerator(GDK_Up));
	GlobalShortcuts_insert("CameraBack", Accelerator(GDK_Down));
	GlobalShortcuts_insert("CameraLeft", Accelerator(GDK_Left));
	GlobalShortcuts_insert("CameraRight", Accelerator(GDK_Right));
	GlobalShortcuts_insert("CameraStrafeRight", Accelerator(GDK_period));
	GlobalShortcuts_insert("CameraStrafeLeft", Accelerator(GDK_comma));

	GlobalShortcuts_insert("CameraUp", Accelerator('D'));
	GlobalShortcuts_insert("CameraDown", Accelerator('C'));
	GlobalShortcuts_insert("CameraAngleUp", Accelerator('A'));
	GlobalShortcuts_insert("CameraAngleDown", Accelerator('Z'));

	GlobalShortcuts_insert("CameraFreeMoveForward", Accelerator(GDK_Up));
	GlobalShortcuts_insert("CameraFreeMoveBack", Accelerator(GDK_Down));
	GlobalShortcuts_insert("CameraFreeMoveLeft", Accelerator(GDK_Left));
	GlobalShortcuts_insert("CameraFreeMoveRight", Accelerator(GDK_Right));

	GlobalToggles_insert("ShowStats", ShowStatsToggleCaller(), ToggleItem::AddCallbackCaller(g_show_stats));

	GlobalPreferenceSystem().registerPreference("ShowStats", BoolImportStringCaller(
			g_camwindow_globals_private.m_showStats), BoolExportStringCaller(g_camwindow_globals_private.m_showStats));
	GlobalPreferenceSystem().registerPreference("MoveSpeed", IntImportStringCaller(
			g_camwindow_globals_private.m_nMoveSpeed), IntExportStringCaller(g_camwindow_globals_private.m_nMoveSpeed));
	GlobalPreferenceSystem().registerPreference("CamLinkSpeed", BoolImportStringCaller(
			g_camwindow_globals_private.m_bCamLinkSpeed), BoolExportStringCaller(
			g_camwindow_globals_private.m_bCamLinkSpeed));
	GlobalPreferenceSystem().registerPreference("AngleSpeed", IntImportStringCaller(
			g_camwindow_globals_private.m_nAngleSpeed),
			IntExportStringCaller(g_camwindow_globals_private.m_nAngleSpeed));
	GlobalPreferenceSystem().registerPreference("CamInverseMouse", BoolImportStringCaller(
			g_camwindow_globals_private.m_bCamInverseMouse), BoolExportStringCaller(
			g_camwindow_globals_private.m_bCamInverseMouse));
	GlobalPreferenceSystem().registerPreference("CamDiscrete", makeBoolStringImportCallback(
			CamWndMoveDiscreteImportCaller()), BoolExportStringCaller(g_camwindow_globals_private.m_bCamDiscrete));
	GlobalPreferenceSystem().registerPreference("CubicClipping", BoolImportStringCaller(
			g_camwindow_globals_private.m_bCubicClipping), BoolExportStringCaller(
			g_camwindow_globals_private.m_bCubicClipping));
	GlobalPreferenceSystem().registerPreference("CubicScale", IntImportStringCaller(g_camwindow_globals.m_nCubicScale),
			IntExportStringCaller(g_camwindow_globals.m_nCubicScale));
	GlobalPreferenceSystem().registerPreference("SI_Colors4", Vector3ImportStringCaller(
			g_camwindow_globals.color_cameraback), Vector3ExportStringCaller(g_camwindow_globals.color_cameraback));
	GlobalPreferenceSystem().registerPreference("SI_Colors12", Vector3ImportStringCaller(
			g_camwindow_globals.color_selbrushes3d), Vector3ExportStringCaller(g_camwindow_globals.color_selbrushes3d));
	GlobalPreferenceSystem().registerPreference("CameraRenderMode", makeIntStringImportCallback(
			RenderModeImportCaller()), makeIntStringExportCallback(RenderModeExportCaller()));
	GlobalPreferenceSystem().registerPreference("StrafeMode", IntImportStringCaller(
			g_camwindow_globals_private.m_nStrafeMode),
			IntExportStringCaller(g_camwindow_globals_private.m_nStrafeMode));

	CamWnd_constructStatic();

	Camera_registerPreferencesPage();
}
void CamWnd_Destroy ()
{
	CamWnd_destroyStatic();
}
