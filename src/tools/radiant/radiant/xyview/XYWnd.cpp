#include "XYWnd.h"

#include "../brush/TexDef.h"
#include "iregistry.h"
#include "iscenegraph.h"
#include "iundo.h"
#include "ifilesystem.h"
#include "ientity.h"
#include "ibrush.h"
#include "iimage.h"
#include "ieventmanager.h"
#include "ioverlay.h"

#include "gtkutil/glwidget.h"
#include "gtkutil/GLWidgetSentry.h"
#include "os/path.h"
#include "stream/stringstream.h"
#include "radiant_i18n.h"

#include "../plugin.h"
#include "../brush/brushmanip.h"
#include "../ui/mainframe/mainframe.h"
#include "../sidebar/texturebrowser.h"
#include "../select.h"
#include "../entity/entity.h"
#include "../renderer.h"
#include "../windowobservers.h"
#include "../camera/GlobalCamera.h"
#include "../ui/ortho/OrthoContextMenu.h"
#include "../selection/algorithm/General.h"

#include "GlobalXYWnd.h"
#include "XYRenderer.h"
#include "../camera/CameraSettings.h"
#include "../image.h"

inline float Betwixt (float f1, float f2)
{
	if (f1 > f2)
		return f2 + ((f1 - f2) / 2);
	else
		return f1 + ((f2 - f1) / 2);
}

inline double two_to_the_power (int power)
{
	return pow(2.0f, power);
}

inline float screen_normalised (int pos, unsigned int size)
{
	return ((2.0f * pos) / size) - 1.0f;
}

inline float normalised_to_world (float normalised, float world_origin, float normalised2world_scale)
{
	return world_origin + normalised * normalised2world_scale;
}

inline const char* ViewType_getTitle (EViewType viewtype)
{
	if (viewtype == XY) {
		return _("XY Top");
	}
	if (viewtype == XZ) {
		return _("XZ Front");
	}
	if (viewtype == YZ) {
		return _("YZ Side");
	}
	return "";
}

XYWnd::XYWnd () :
	_glWidget(false), m_gl_widget(static_cast<GtkWidget*> (_glWidget)), m_deferredDraw(WidgetQueueDrawCaller(
			*m_gl_widget)), m_deferred_motion(callbackMouseMotion, this), _parent(0), _minWorldCoord(
			GlobalRegistry().getFloat("game/defaults/minWorldCoord")), _maxWorldCoord(GlobalRegistry().getFloat(
			"game/defaults/maxWorldCoord")), m_window_observer(NewWindowObserver()), m_XORRectangle(m_gl_widget),
			_chaseMouseHandler(0)
{
	m_bActive = false;
	m_buttonstate = 0;

	m_bNewBrushDrag = false;
	_moveStarted = false;
	_zoomStarted = false;

	m_nWidth = 0;
	m_nHeight = 0;

	/* we need this initialized */
	m_modelview = Matrix4::getIdentity();
	m_projection = Matrix4::getIdentity();

	m_vOrigin[0] = 0;
	m_vOrigin[1] = 20;
	m_vOrigin[2] = 46;
	m_fScale = 1;
	m_viewType = XY;

	m_alpha = 1.0f;
	m_xmin = 0.0f;
	m_ymin = 0.0f;
	m_xmax = 0.0f;
	m_ymax = 0.0f;

	m_entityCreate = false;

	GlobalWindowObservers_add(m_window_observer);
	GlobalWindowObservers_connectWidget(m_gl_widget);

	m_window_observer->setRectangleDrawCallback(MemberCaller1<XYWnd, Rectangle, &XYWnd::updateXORRectangle> (*this));
	m_window_observer->setView(m_view);

	gtk_widget_ref(m_gl_widget);

	gtk_widget_set_events(m_gl_widget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
	GTK_WIDGET_SET_FLAGS(m_gl_widget, GTK_CAN_FOCUS);
	gtk_widget_set_size_request(m_gl_widget, XYWND_MINSIZE_X, XYWND_MINSIZE_Y);
	g_object_set(m_gl_widget, "can-focus", TRUE, NULL);

	m_sizeHandler = g_signal_connect(G_OBJECT(m_gl_widget), "size_allocate", G_CALLBACK(callbackSizeAllocate), this);
	m_exposeHandler = g_signal_connect(G_OBJECT(m_gl_widget), "expose_event", G_CALLBACK(callbackExpose), this);

	g_signal_connect(G_OBJECT(m_gl_widget), "button_press_event", G_CALLBACK(callbackButtonPress), this);
	g_signal_connect(G_OBJECT(m_gl_widget), "button_release_event", G_CALLBACK(callbackButtonRelease), this);
	g_signal_connect(G_OBJECT(m_gl_widget), "motion_notify_event", G_CALLBACK(DeferredMotion::gtk_motion), &m_deferred_motion);

	g_signal_connect(G_OBJECT(m_gl_widget), "scroll_event", G_CALLBACK(callbackMouseWheelScroll), this);

	GlobalMap().addValidCallback(DeferredDrawOnMapValidChangedCaller(m_deferredDraw));

	updateProjection();
	updateModelview();

	GlobalSceneGraph().addSceneObserver(this);
	GlobalCamera().addCameraObserver(this);

	GlobalEventManager().connect(GTK_OBJECT(m_gl_widget));
}

void XYWnd::onSceneGraphChange ()
{
	queueDraw();
}

XYWnd::~XYWnd (void)
{
	// Remove <self> from the scene change callback list
	GlobalSceneGraph().removeSceneObserver(this);

	// greebo: Remove <self> as CameraObserver to the CamWindow.
	GlobalCamera().removeCameraObserver(this);

	GlobalEventManager().disconnect(GTK_OBJECT(m_gl_widget));

	g_signal_handler_disconnect(G_OBJECT(m_gl_widget), m_sizeHandler);
	g_signal_handler_disconnect(G_OBJECT(m_gl_widget), m_exposeHandler);

	gtk_widget_hide(m_gl_widget);

	// greebo: Unregister the allocated window observer from the global list, before destroying it
	GlobalWindowObservers_remove(m_window_observer);

    // This deletes the RadiantWindowObserver from the heap
	delete m_window_observer;
}

void XYWnd::captureStates (void)
{
	m_state_selected = GlobalShaderCache().capture("$XY_OVERLAY");
}

void XYWnd::setEvent (GdkEventButton* event)
{
	_event = event;
}

void XYWnd::releaseStates (void)
{
	GlobalShaderCache().release("$XY_OVERLAY");
}

void XYWnd::setParent (GtkWindow* parent)
{
	_parent = parent;
}

GtkWindow* XYWnd::getParent () const
{
	return _parent;
}

EViewType XYWnd::getViewType ()
{
	return m_viewType;
}

const std::string XYWnd::getViewTypeTitle(EViewType viewtype) {
	if (viewtype == XY) {
		return _("XY Top");
	}
	if (viewtype == XZ) {
		return _("XZ Front");
	}
	if (viewtype == YZ) {
		return _("YZ Side");
	}
	return "";
}

float XYWnd::getScale () const
{
	return m_fScale;
}
int XYWnd::getWidth () const
{
	return m_nWidth;
}
int XYWnd::getHeight () const
{
	return m_nHeight;
}

const Vector3& XYWnd::getOrigin (void) const
{
	return m_vOrigin;
}

void XYWnd::setOrigin (const Vector3& origin)
{
	m_vOrigin = origin;
	updateModelview();
}

void XYWnd::scroll (int x, int y)
{
	const int nDim1 = (m_viewType == YZ) ? 1 : 0;
	const int nDim2 = (m_viewType == XY) ? 1 : 2;
	m_vOrigin[nDim1] += x / m_fScale;
	m_vOrigin[nDim2] += y / m_fScale;
	updateModelview();
	queueDraw();
}

void XYWnd::DropClipPoint (int pointx, int pointy)
{
	Vector3 point = g_vector3_identity;

	convertXYToWorld(pointx, pointy, point);

	Vector3 mid = selection::algorithm::getCurrentSelectionCenter();
	GlobalClipper().setViewType(getViewType());
	int nDim = (GlobalClipper().getViewType() == YZ) ? 0 : ((GlobalClipper().getViewType() == XZ) ? 1 : 2);
	point[nDim] = mid[nDim];
	vector3_snap(point, GlobalGrid().getGridSize());
	GlobalClipper().newClipPoint(point);
}

void XYWnd::Clipper_OnLButtonDown (int x, int y)
{
	Vector3 mousePosition = g_vector3_identity;
	convertXYToWorld(x, y, mousePosition);
	ClipPoint* foundClipPoint = GlobalClipper().find(mousePosition, m_viewType, m_fScale);
	GlobalClipper().setMovingClip(foundClipPoint);
	if (foundClipPoint == NULL) {
		DropClipPoint(x, y);
	}
}

void XYWnd::Clipper_OnLButtonUp (int x, int y)
{
	GlobalClipper().setMovingClip(NULL);
}

void XYWnd::Clipper_OnMouseMoved (int x, int y)
{
	ClipPoint* movingClip = GlobalClipper().getMovingClip();
	if (movingClip != NULL) {
		convertXYToWorld(x, y, movingClip->_coords);
		snapToGrid(movingClip->_coords);
		GlobalClipper().update();
		ClipperChangeNotify();
	}
}

void XYWnd::Clipper_Crosshair_OnMouseMoved (int x, int y)
{
	Vector3 mousePosition = g_vector3_identity;
	convertXYToWorld(x, y, mousePosition);
	if (GlobalClipper().clipMode() && GlobalClipper().find(mousePosition, m_viewType, m_fScale) != 0) {
		GdkCursor *cursor = gdk_cursor_new(GDK_CROSSHAIR);
		gdk_window_set_cursor(m_gl_widget->window, cursor);
		gdk_cursor_unref(cursor);
	} else {
		gdk_window_set_cursor(m_gl_widget->window, 0);
	}
}

void XYWnd::positionCamera (int x, int y, CamWnd& camwnd)
{
	Vector3 origin(camwnd.getCameraOrigin());
	convertXYToWorld(x, y, origin);
	snapToGrid(origin);
	camwnd.setCameraOrigin(origin);
}

void XYWnd::orientCamera (int x, int y, CamWnd& camwnd)
{
	Vector3 point = g_vector3_identity;
	convertXYToWorld(x, y, point);
	snapToGrid(point);
	point -= camwnd.getCameraOrigin();

	const int n1 = (getViewType() == XY) ? 1 : 2;
	const int n2 = (getViewType() == YZ) ? 1 : 0;
	const int nAngle = (getViewType() == XY) ? CAMERA_YAW : CAMERA_PITCH;
	if (point[n1] || point[n2]) {
		Vector3 angles(camwnd.getCameraAngles());
		angles[nAngle] = static_cast<float> (radians_to_degrees(atan2(point[n1], point[n2])));
		camwnd.setCameraAngles(angles);
	}
}

// Callback that gets invoked on camera move
void XYWnd::cameraMoved ()
{
	if (GlobalXYWnd().camXYUpdate()) {
		queueDraw();
	}
}

// This is the chase mouse handler that gets connected by XYWnd::chaseMouseMotion()
// It passes te call on to the XYWnd::ChaseMouse() method.
gboolean XYWnd::xywnd_chasemouse (gpointer data)
{
	reinterpret_cast<XYWnd*> (data)->chaseMouse();
	return TRUE;
}

void XYWnd::NewBrushDrag_Begin (int x, int y)
{
	m_NewBrushDrag = 0;
	m_nNewBrushPressx = x;
	m_nNewBrushPressy = y;

	m_bNewBrushDrag = true;
	GlobalUndoSystem().start();
}

void XYWnd::NewBrushDrag_End (int x, int y)
{
	if (m_NewBrushDrag != 0) {
		GlobalUndoSystem().finish("brushDragNew");
	}
}

void XYWnd::setScale (float f)
{
	m_fScale = f;
	updateProjection();
	updateModelview();
	queueDraw();
}

void XYWnd::zoomIn ()
{
	const float max_scale = 64.0f;
	const float scale = getScale() * 5.0f / 4.0f;
	if (scale > max_scale) {
		if (getScale() != max_scale) {
			setScale(max_scale);
		}
	} else {
		setScale(scale);
	}
}

/**
 * @note the zoom out factor is 4/5, we could think about customizing it
 * we don't go below a zoom factor corresponding to 10% of the max world size
 * (this has to be computed against the window size)
 */
void XYWnd::zoomOut ()
{
	float min_scale = MIN(getWidth(), getHeight()) / (1.1f * (_maxWorldCoord - _minWorldCoord));
	float scale = getScale() * 4.0f / 5.0f;
	if (scale < min_scale) {
		if (getScale() != min_scale) {
			setScale(min_scale);
		}
	} else {
		setScale(scale);
	}
}

void XYWnd::zoomDelta (int x, int y, unsigned int state, void* data)
{
	if (y != 0) {
		XYWnd *xywnd = reinterpret_cast<XYWnd*> (data);

		xywnd->_dragZoom += y;

		while (abs(xywnd->_dragZoom) > 8) {
			if (xywnd->_dragZoom > 0) {
				xywnd->zoomOut();
				xywnd->_dragZoom -= 8;
			} else {
				xywnd->zoomIn();
				xywnd->_dragZoom += 8;
			}
		}
	}
}

/* greebo: This gets repeatedly called during a mouse chase operation.
 * The method is making use of a timer to determine the amount of time that has
 * passed since the chaseMouse has been started
 */
void XYWnd::chaseMouse (void)
{
	const float multiplier = _chaseMouseTimer.elapsed_msec() / 10.0f;
	scroll(float_to_integer(multiplier * m_chasemouse_delta_x), float_to_integer(multiplier * -m_chasemouse_delta_y));

	mouseMoved(m_chasemouse_current_x, m_chasemouse_current_y, _event->state);
	// greebo: Restart the timer
	_chaseMouseTimer.start();
}

/* greebo: This handles the "chase mouse" behaviour, if the user drags something
 * beyond the XY window boundaries. If the chaseMouse option (currently a global)
 * is set true, the view origin gets relocated along with the mouse movements.
 *
 * @returns: true, if the mousechase has been performed, false if no mouse chase was necessary
 */
bool XYWnd::chaseMouseMotion (int pointx, int pointy, const unsigned int& state)
{
	m_chasemouse_delta_x = 0;
	m_chasemouse_delta_y = 0;

	IMouseEvents& mouseEvents = GlobalEventManager().MouseEvents();

	// These are the events that are allowed
	bool isAllowedEvent = mouseEvents.stateMatchesXYViewEvent(ui::xySelect, state)
			|| mouseEvents.stateMatchesXYViewEvent(ui::xyNewBrushDrag, state);

	// greebo: The mouse chase is only active when the according global is set to true and if we
	// are in the right state
	if (GlobalXYWnd().chaseMouse() && isAllowedEvent) {
		const int epsilon = 16;

		// Calculate the X delta
		if (pointx < epsilon) {
			m_chasemouse_delta_x = std::max(pointx, 0) - epsilon;
		} else if ((pointx - m_nWidth) > -epsilon) {
			m_chasemouse_delta_x = std::min((pointx - m_nWidth), 0) + epsilon;
		}

		// Calculate the Y delta
		if (pointy < epsilon) {
			m_chasemouse_delta_y = std::max(pointy, 0) - epsilon;
		} else if ((pointy - m_nHeight) > -epsilon) {
			m_chasemouse_delta_y = std::min((pointy - m_nHeight), 0) + epsilon;
		}

		// If any of the deltas is uneqal to zero the mouse chase is to be performed
		if (m_chasemouse_delta_y != 0 || m_chasemouse_delta_x != 0) {
			m_chasemouse_current_x = pointx;
			m_chasemouse_current_y = pointy;

			// Start the timer, if there isn't one connected already
			if (_chaseMouseHandler == 0) {
				_chaseMouseTimer.start();

				// Add the chase mouse handler to the idle callbacks, so it gets called as
				// soon as there is nothing more important to do. The callback queries the timer
				// and takes the according window movement actions
				_chaseMouseHandler = g_idle_add(xywnd_chasemouse, this);
			}

			// Return true to signal that there are no other mouseMotion handlers to be performed
			// see xywnd_motion() callback function
			return true;
		} else {
			// All deltas are zero, so there is no more mouse chasing necessary, remove the handlers
			if (_chaseMouseHandler != 0) {
				g_source_remove(_chaseMouseHandler);
				_chaseMouseHandler = 0;
			}
		}
	} else {
		// Remove the handlers, the user has probably released the mouse button during chase
		if (_chaseMouseHandler != 0) {
			g_source_remove(_chaseMouseHandler);
			_chaseMouseHandler = 0;
		}
	}

	// No mouse chasing has been performed, return false
	return false;
}

// =============================================================================
// XYWnd class
Shader* XYWnd::m_state_selected = 0;

/* greebo: This is the callback for the mouse_press event that is invoked by GTK
 * it checks for the correct event type and passes the call to the according xy view window.
 *
 * Note: I think these should be static members of the XYWnd class, shouldn't they?
 */
gboolean XYWnd::callbackButtonPress (GtkWidget* widget, GdkEventButton* event, XYWnd* xywnd)
{
	// Move the focus to this GL widget
	gtk_widget_grab_focus(widget);

	if (event->type == GDK_BUTTON_PRESS) {
		GlobalXYWnd().setActiveXY(xywnd);

		xywnd->setEvent(event);

		// Pass the GdkEventButton* to the XYWnd class, the boolean <true> is passed but never used
		xywnd->mouseDown(static_cast<int> (event->x), static_cast<int> (event->y), event);
	}
	return FALSE;
}

// greebo: This is the GTK callback for mouseUp.
gboolean XYWnd::callbackButtonRelease (GtkWidget* widget, GdkEventButton* event, XYWnd* xywnd)
{
	// greebo: Check for the correct event type (redundant?)
	if (event->type == GDK_BUTTON_RELEASE) {
		// Call the according mouseUp method
		xywnd->mouseUp(static_cast<int> (event->x), static_cast<int> (event->y), event);

		// Clear the buttons that the button_release has been called with
		xywnd->setEvent(event);
	}
	return FALSE;
}

// greebo: This is the GTK callback for mouse movement.
void XYWnd::callbackMouseMotion (gdouble x, gdouble y, guint state, void* data)
{
	// Convert the passed pointer into a XYWnd pointer
	XYWnd* xywnd = reinterpret_cast<XYWnd*> (data);

	// Call the chaseMouse method
	if (xywnd->chaseMouseMotion(static_cast<int> (x), static_cast<int> (y), state)) {
		return;
	}

	// This gets executed, if the above chaseMouse call returned false, i.e. no mouse chase has been performed
	xywnd->mouseMoved(static_cast<int> (x), static_cast<int> (y), state);
}

gboolean XYWnd::callbackMouseWheelScroll (GtkWidget* widget, GdkEventScroll* event, XYWnd* xywnd)
{
	if (event->direction == GDK_SCROLL_UP) {
		xywnd->zoomIn();
	} else if (event->direction == GDK_SCROLL_DOWN) {
		xywnd->zoomOut();
	}
	return FALSE;
}

gboolean XYWnd::callbackSizeAllocate (GtkWidget* widget, GtkAllocation* allocation, XYWnd* xywnd)
{
	xywnd->m_nWidth = allocation->width;
	xywnd->m_nHeight = allocation->height;
	xywnd->updateProjection();
	xywnd->m_window_observer->onSizeChanged(xywnd->getWidth(), xywnd->getHeight());
	return FALSE;
}

gboolean XYWnd::callbackExpose (GtkWidget* widget, GdkEventExpose* event, XYWnd* xywnd)
{
	gtkutil::GLWidgetSentry sentry(xywnd->getWidget());
	if (GlobalMap().isValid() && ScreenUpdates_Enabled()) {
		xywnd->draw();
		xywnd->m_XORRectangle.set(rectangle_t());
	}
	return FALSE;
}

void XYWnd::CameraMoved ()
{
	if (GlobalXYWnd().camXYUpdate()) {
		queueDraw();
	}
}

inline const std::string NewBrushDragGetTexture (void)
{
	const std::string& selectedTexture = GlobalTextureBrowser().getSelectedShader();
	if (GlobalXYWnd().alwaysCaulkForNewBrushes())
		return GlobalTexturePrefix_get() + GlobalXYWnd().getCaulkTexture();
	return selectedTexture;
}

/** @brief Left mouse button was clicked without any selection active */
void XYWnd::NewBrushDrag (int x, int y)
{
	Vector3 mins = g_vector3_identity;
	Vector3 maxs = g_vector3_identity;
	convertXYToWorld(m_nNewBrushPressx, m_nNewBrushPressy, mins);
	snapToGrid(mins);
	convertXYToWorld(x, y, maxs);
	snapToGrid(maxs);

	const int nDim = (m_viewType == XY) ? 2 : (m_viewType == YZ) ? 0 : 1;

	mins[nDim] = float_snapped(GlobalSelectionSystem().getWorkZone().min[nDim], GlobalGrid().getGridSize());
	maxs[nDim] = float_snapped(GlobalSelectionSystem().getWorkZone().max[nDim], GlobalGrid().getGridSize());

	if (maxs[nDim] <= mins[nDim])
		maxs[nDim] = mins[nDim] + GlobalGrid().getGridSize();

	for (int i = 0; i < 3; i++) {
		if (mins[i] == maxs[i])
			return; // don't create a degenerate brush
		if (mins[i] > maxs[i]) {
			const float temp = mins[i];
			mins[i] = maxs[i];
			maxs[i] = temp;
		}
	}

	if (m_NewBrushDrag == 0) {
		NodeSmartReference node(GlobalBrushCreator().createBrush());
		Node_getTraversable(GlobalMap().findOrInsertWorldspawn())->insert(node);

		scene::Path brushpath(makeReference(GlobalSceneGraph().root()));
		brushpath.push(makeReference(*GlobalMap().getWorldspawn()));
		brushpath.push(makeReference(node.get()));
		selectPath(brushpath, true);

		m_NewBrushDrag = node.get_pointer();
	}

	Scene_BrushResize_Selected(GlobalSceneGraph(), AABB::createFromMinMax(mins, maxs), NewBrushDragGetTexture());
}

void XYWnd::mouseToPoint (int x, int y, Vector3& point)
{
	convertXYToWorld(x, y, point);
	snapToGrid(point);

	const int nDim = (getViewType() == XY) ? 2 : (getViewType() == YZ) ? 0 : 1;
	const float fWorkMid = float_mid(GlobalSelectionSystem().getWorkZone().min[nDim], GlobalSelectionSystem().getWorkZone().max[nDim]);
	point[nDim] = float_snapped(fWorkMid, GlobalGrid().getGridSize());
}

/**
 * @brief Context menu for the right click in the views
 */
void XYWnd::onContextMenu (void)
{
	// Get the click point in 3D space
	Vector3 point = g_vector3_identity;
	mouseToPoint(m_entityCreate_x, m_entityCreate_y, point);
	// Display the menu, passing the coordinates for creation
	ui::OrthoContextMenu::displayInstance(point);
}

void XYWnd::callbackMoveDelta (int x, int y, unsigned int state, void* data)
{
	reinterpret_cast<XYWnd*> (data)->EntityCreate_MouseMove(x, y);
	reinterpret_cast<XYWnd*> (data)->scroll(-x, y);
}

gboolean XYWnd::callbackMoveFocusOut (GtkWidget* widget, GdkEventFocus* event, XYWnd* xywnd)
{
	xywnd->endMove();
	return FALSE;
}

void XYWnd::beginMove (void)
{
	if (_moveStarted) {
		endMove();
	}
	_moveStarted = true;
	_freezePointer.freeze_pointer(_parent != 0 ? _parent : GlobalRadiant().getMainWindow(), callbackMoveDelta, this);
	m_move_focusOut
			= g_signal_connect(G_OBJECT(m_gl_widget), "focus_out_event", G_CALLBACK(callbackMoveFocusOut), this);
}

void XYWnd::endMove (void)
{
	_moveStarted = false;
	_freezePointer.unfreeze_pointer(_parent != 0 ? _parent : GlobalRadiant().getMainWindow());
	g_signal_handler_disconnect(G_OBJECT(m_gl_widget), m_move_focusOut);
}

gboolean XYWnd::onFocusOut (GtkWidget* widget, GdkEventFocus* event, XYWnd* xywnd)
{
	xywnd->endZoom();
	return FALSE;
}

void XYWnd::beginZoom (void)
{
	if (_zoomStarted) {
		endZoom();
	}
	_zoomStarted = true;
	_dragZoom = 0;
	_freezePointer.freeze_pointer(_parent != 0 ? _parent : GlobalRadiant().getMainWindow(), zoomDelta, this);
	m_zoom_focusOut = g_signal_connect(G_OBJECT(m_gl_widget), "focus_out_event", G_CALLBACK(onFocusOut), this);
}

void XYWnd::endZoom (void)
{
	_zoomStarted = false;
	_freezePointer.unfreeze_pointer(_parent != 0 ? _parent : GlobalRadiant().getMainWindow());
	g_signal_handler_disconnect(G_OBJECT(m_gl_widget), m_zoom_focusOut);
}

/**
 * @brief makes sure the selected brush or camera is in view
 */
void XYWnd::positionView (const Vector3& position)
{
	const int nDim1 = (m_viewType == YZ) ? 1 : 0;
	const int nDim2 = (m_viewType == XY) ? 1 : 2;

	m_vOrigin[nDim1] = position[nDim1];
	m_vOrigin[nDim2] = position[nDim2];

	updateModelview();

	queueDraw();
}

void XYWnd::setViewType (EViewType viewType)
{
	m_viewType = viewType;
	updateModelview();

	if (_parent != 0) {
		gtk_window_set_title(_parent, ViewType_getTitle(m_viewType));
	}
}

/* This gets called by the GTK callback function.
 */
void XYWnd::mouseDown (int x, int y, GdkEventButton* event)
{
	IMouseEvents& mouseEvents = GlobalEventManager().MouseEvents();

	if (mouseEvents.stateMatchesXYViewEvent(ui::xyMoveView, event)) {
		beginMove();
		EntityCreate_MouseDown(x, y);
	}

	if (mouseEvents.stateMatchesXYViewEvent(ui::xyZoom, event)) {
		beginZoom();
	}

	if (mouseEvents.stateMatchesXYViewEvent(ui::xyCameraMove, event)) {
		positionCamera(x, y, *g_pParentWnd->GetCamWnd());
	}

	if (mouseEvents.stateMatchesXYViewEvent(ui::xyCameraAngle, event)) {
		orientCamera(x, y, *g_pParentWnd->GetCamWnd());
	}

	// Only start a NewBrushDrag operation, if not other elements are selected
	if (GlobalSelectionSystem().countSelected() == 0 && mouseEvents.stateMatchesXYViewEvent(ui::xyNewBrushDrag,
			event)) {
		NewBrushDrag_Begin(x, y);
		return; // Prevent the call from being passed to the windowobserver
	}

	if (mouseEvents.stateMatchesXYViewEvent(ui::xySelect, event)) {
		// There are two possibilites for the "select" click: Clip or Select
		if (GlobalClipper().clipMode()) {
			Clipper_OnLButtonDown(x, y);
			return; // Prevent the call from being passed to the windowobserver
		}
	}

	// Pass the call to the window observer
	m_window_observer->onMouseDown(WindowVector(x, y), event);
}

// This gets called by either the GTK Callback or the method that is triggered by the mousechase timer
void XYWnd::mouseMoved (int x, int y, const unsigned int& state)
{
	IMouseEvents& mouseEvents = GlobalEventManager().MouseEvents();

	if (mouseEvents.stateMatchesXYViewEvent(ui::xyCameraMove, state)) {
		positionCamera(x, y, *g_pParentWnd->GetCamWnd());
	}

	if (mouseEvents.stateMatchesXYViewEvent(ui::xyCameraAngle, state)) {
		orientCamera(x, y, *g_pParentWnd->GetCamWnd());
	}

	// Check, if we are in a NewBrushDrag operation and continue it
	if (m_bNewBrushDrag && mouseEvents.stateMatchesXYViewEvent(ui::xyNewBrushDrag, state)) {
		NewBrushDrag(x, y);
		return; // Prevent the call from being passed to the windowobserver
	}

	if (mouseEvents.stateMatchesXYViewEvent(ui::xySelect, state)) {
		// Check, if we have a clip point operation running
		if (GlobalClipper().clipMode() && GlobalClipper().getMovingClip() != 0) {
			Clipper_OnMouseMoved(x, y);
			return; // Prevent the call from being passed to the windowobserver
		}
	}

	// default windowobserver::mouseMotion call, if no other clauses called "return" till now
	m_window_observer->onMouseMotion(WindowVector(x, y), state);

	m_mousePosition[0] = m_mousePosition[1] = m_mousePosition[2] = 0.0;
	convertXYToWorld(x, y, m_mousePosition);
	snapToGrid(m_mousePosition);

	StringOutputStream status(64);
	status << "x:: " << FloatFormat(m_mousePosition[0], 6, 1) << "  y:: " << FloatFormat(m_mousePosition[1], 6, 1)
			<< "  z:: " << FloatFormat(m_mousePosition[2], 6, 1);
	g_pParentWnd->SetStatusText(g_pParentWnd->m_position_status, status.toString());

	if (GlobalXYWnd().showCrossHairs()) {
		queueDraw();
	}

	Clipper_Crosshair_OnMouseMoved(x, y);
}

// greebo: The mouseUp method gets called by the GTK callback above
void XYWnd::mouseUp (int x, int y, GdkEventButton* event)
{
	// End move
	if (_moveStarted) {
		endMove();
		EntityCreate_MouseUp(x, y);
	}

	// End zoom
	if (_zoomStarted) {
		endZoom();
	}

	// Finish any pending NewBrushDrag operations
	if (m_bNewBrushDrag) {
		// End the NewBrushDrag operation
		m_bNewBrushDrag = false;
		NewBrushDrag_End(x, y);
		return; // Prevent the call from being passed to the windowobserver
	}

	IMouseEvents& mouseEvents = GlobalEventManager().MouseEvents();
	if (GlobalClipper().clipMode() && mouseEvents.stateMatchesXYViewEvent(ui::xySelect, event)) {
		// End the clip operation
		Clipper_OnLButtonUp(x, y);
		return; // Prevent the call from being passed to the windowobserver
	}

	// Pass the call to the window observer
	m_window_observer->onMouseUp(WindowVector(x, y), event);
}

void XYWnd::EntityCreate_MouseDown (int x, int y)
{
	m_entityCreate = true;
	m_entityCreate_x = x;
	m_entityCreate_y = y;
}

void XYWnd::EntityCreate_MouseMove (int x, int y)
{
	if (m_entityCreate && (m_entityCreate_x != x || m_entityCreate_y != y)) {
		m_entityCreate = false;
	}
}

void XYWnd::EntityCreate_MouseUp (int x, int y)
{
	if (m_entityCreate) {
		m_entityCreate = false;
		onContextMenu();
	}
}

// TTimo: watch it, this doesn't init one of the 3 coords
void XYWnd::convertXYToWorld (int x, int y, Vector3& point)
{
	float normalised2world_scale_x = m_nWidth / 2 / m_fScale;
	float normalised2world_scale_y = m_nHeight / 2 / m_fScale;
	if (m_viewType == XY) {
		point[0] = normalised_to_world(screen_normalised(x, m_nWidth), m_vOrigin[0], normalised2world_scale_x);
		point[1] = normalised_to_world(-screen_normalised(y, m_nHeight), m_vOrigin[1], normalised2world_scale_y);
	} else if (m_viewType == YZ) {
		point[1] = normalised_to_world(screen_normalised(x, m_nWidth), m_vOrigin[1], normalised2world_scale_x);
		point[2] = normalised_to_world(-screen_normalised(y, m_nHeight), m_vOrigin[2], normalised2world_scale_y);
	} else {
		point[0] = normalised_to_world(screen_normalised(x, m_nWidth), m_vOrigin[0], normalised2world_scale_x);
		point[2] = normalised_to_world(-screen_normalised(y, m_nHeight), m_vOrigin[2], normalised2world_scale_y);
	}
}

void XYWnd::snapToGrid (Vector3& point)
{
	if (m_viewType == XY) {
		point[0] = float_snapped(point[0], GlobalGrid().getGridSize());
		point[1] = float_snapped(point[1], GlobalGrid().getGridSize());
	} else if (m_viewType == YZ) {
		point[1] = float_snapped(point[1], GlobalGrid().getGridSize());
		point[2] = float_snapped(point[2], GlobalGrid().getGridSize());
	} else {
		point[0] = float_snapped(point[0], GlobalGrid().getGridSize());
		point[2] = float_snapped(point[2], GlobalGrid().getGridSize());
	}
}

void XYWnd::drawAxis (void)
{
	if (GlobalXYWnd().showAxes()) {
		const char* g_AxisName[3] = { "X", "Y", "Z" };
		const int nDim1 = (m_viewType == YZ) ? 1 : 0;
		const int nDim2 = (m_viewType == XY) ? 1 : 2;
		const int w = (int) (m_nWidth / 2 / m_fScale);
		const int h = (int) (m_nHeight / 2 / m_fScale);

		const Vector3 colourX = (m_viewType == YZ) ? ColourSchemes().getColourVector3("axis_y")
				: ColourSchemes().getColourVector3("axis_x");
		const Vector3 colourY = (m_viewType == XY) ? ColourSchemes().getColourVector3("axis_y")
				: ColourSchemes().getColourVector3("axis_z");

		// draw two lines with corresponding axis colors to highlight current view
		// horizontal line: nDim1 color
		glLineWidth(2);
		glBegin(GL_LINES);
		glColor3fv(colourX);
		glVertex2f(m_vOrigin[nDim1] - w + 40 / m_fScale, m_vOrigin[nDim2] + h - 45 / m_fScale);
		glVertex2f(m_vOrigin[nDim1] - w + 65 / m_fScale, m_vOrigin[nDim2] + h - 45 / m_fScale);
		glVertex2f(0, 0);
		glVertex2f(32 / m_fScale, 0);
		glColor3fv(colourY);
		glVertex2f(m_vOrigin[nDim1] - w + 40 / m_fScale, m_vOrigin[nDim2] + h - 45 / m_fScale);
		glVertex2f(m_vOrigin[nDim1] - w + 40 / m_fScale, m_vOrigin[nDim2] + h - 20 / m_fScale);
		glVertex2f(0, 0);
		glVertex2f(0, 32 / m_fScale);
		glEnd();
		glLineWidth(1);
		// now print axis symbols
		glColor3fv(colourX);
		glRasterPos2f(m_vOrigin[nDim1] - w + 55 / m_fScale, m_vOrigin[nDim2] + h - 55 / m_fScale);
		GlobalOpenGL().drawString(g_AxisName[nDim1]);
		glRasterPos2f(28 / m_fScale, -10 / m_fScale);
		GlobalOpenGL().drawString(g_AxisName[nDim1]);
		glColor3fv(colourY);
		glRasterPos2f(m_vOrigin[nDim1] - w + 25 / m_fScale, m_vOrigin[nDim2] + h - 30 / m_fScale);
		GlobalOpenGL().drawString(g_AxisName[nDim2]);
		glRasterPos2f(-10 / m_fScale, 28 / m_fScale);
		GlobalOpenGL().drawString(g_AxisName[nDim2]);
	}
}

/* greebo: This calculates the coordinates of the xy view window corners.
 *
 * @returns: Vector4( xbegin, xend, ybegin, yend);
 */
Vector4 XYWnd::getWindowCoordinates() {
	int nDim1 = (m_viewType == YZ) ? 1 : 0;
	int nDim2 = (m_viewType == XY) ? 1 : 2;

	float w = (m_nWidth / 2 / m_fScale);
	float h = (m_nHeight / 2 / m_fScale);

	float xb = m_vOrigin[nDim1] - w;
	if (xb < GlobalMap().getRegionMins()[nDim1])
		xb = GlobalMap().getRegionMins()[nDim1];

	float xe = m_vOrigin[nDim1] + w;
	if (xe > GlobalMap().getRegionMaxs()[nDim1])
		xe = GlobalMap().getRegionMaxs()[nDim1];

	float yb = m_vOrigin[nDim2] - h;
	if (yb < GlobalMap().getRegionMins()[nDim2])
		yb = GlobalMap().getRegionMins()[nDim2];

	float ye = m_vOrigin[nDim2] + h;
	if (ye > GlobalMap().getRegionMaxs()[nDim2])
		ye = GlobalMap().getRegionMaxs()[nDim2];

	return Vector4(xb, xe, yb, ye);
}

void XYWnd::drawGrid (void)
{
	float x, y, xb, xe, yb, ye;
	float w, h;
	char text[32];
	float step, minor_step, stepx, stepy;
	step = minor_step = stepx = stepy = GlobalGrid().getGridSize();

	int minor_power = GlobalGrid().getGridPower();
	int mask;

	while ((minor_step * m_fScale) <= 4.0f) { // make sure minor grid spacing is at least 4 pixels on the screen
		++minor_power;
		minor_step *= 2;
	}
	int power = minor_power;
	while ((power % 3) != 0 || (step * m_fScale) <= 32.0f) { // make sure major grid spacing is at least 32 pixels on the screen
		++power;
		step = float(two_to_the_power(power));
	}
	mask = (1 << (power - minor_power)) - 1;
	while ((stepx * m_fScale) <= 32.0f)
		// text step x must be at least 32
		stepx *= 2;
	while ((stepy * m_fScale) <= 32.0f)
		// text step y must be at least 32
		stepy *= 2;

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glLineWidth(1);

	w = (m_nWidth / 2 / m_fScale);
	h = (m_nHeight / 2 / m_fScale);

	Vector4 windowCoords = getWindowCoordinates();

	xb = step * floor(windowCoords[0] / step);
	xe = step * ceil(windowCoords[1] / step);
	yb = step * floor(windowCoords[2] / step);
	ye = step * ceil(windowCoords[3] / step);

	if (GlobalXYWnd().showGrid()) {
		ui::ColourItem colourGridBack = ColourSchemes().getColour("grid_background");
		ui::ColourItem colourGridMinor = ColourSchemes().getColour("grid_minor");
		ui::ColourItem colourGridMajor = ColourSchemes().getColour("grid_major");

		// run grid rendering twice, first run is minor grid, then major
		// NOTE: with a bit more work, we can have variable number of grids
		for (int gf = 0; gf < 2; ++gf) {
			double cur_step, density, sizeFactor;
			GridLook look;

			if (gf) {
				// major grid
				if (colourGridMajor == colourGridBack)
					continue;

				glColor3fv(colourGridMajor);
				look = GlobalGrid().getMajorLook();
				cur_step = step;
				density = 4;
				// slightly bigger crosses
				sizeFactor = 1.95;
			} else {
				// minor grid (rendered first)
				if (colourGridMinor == colourGridBack)
					continue;
				glColor3fv(colourGridMinor);
				look = GlobalGrid().getMinorLook();
				cur_step = minor_step;
				density = 4;
				sizeFactor = 0.95;
			}

			switch (look) {
			case GRIDLOOK_DOTS:
				glBegin(GL_POINTS);
				for (x = xb; x < xe; x += cur_step) {
					for (y = yb; y < ye; y += cur_step) {
						glVertex2d(x, y);
					}
				}
				glEnd();
				break;
			case GRIDLOOK_BIGDOTS:
				glPointSize(3);
				glEnable(GL_POINT_SMOOTH);
				glBegin(GL_POINTS);
				for (x = xb; x < xe; x += cur_step) {
					for (y = yb; y < ye; y += cur_step) {
						glVertex2d(x, y);
					}
				}
				glEnd();
				glDisable(GL_POINT_SMOOTH);
				glPointSize(1);
				break;
			case GRIDLOOK_SQUARES:
				glPointSize(3);
				glBegin(GL_POINTS);
				for (x = xb; x < xe; x += cur_step) {
					for (y = yb; y < ye; y += cur_step) {
						glVertex2d(x, y);
					}
				}
				glEnd();
				glPointSize(1);
				break;
			case GRIDLOOK_MOREDOTLINES:
				density = 8;
			case GRIDLOOK_DOTLINES:
				glBegin(GL_POINTS);
				for (x = xb; x < xe; x += cur_step) {
					for (y = yb; y < ye; y += minor_step / density) {
						glVertex2d(x, y);
					}
				}

				for (y = yb; y < ye; y += cur_step) {
					for (x = xb; x < xe; x += minor_step / density) {
						glVertex2d(x, y);
					}
				}
				glEnd();
				break;
			case GRIDLOOK_CROSSES:
				glBegin(GL_LINES);
				for (x = xb; x <= xe; x += cur_step) {
					for (y = yb; y <= ye; y += cur_step) {
						glVertex2d(x - sizeFactor / m_fScale, y);
						glVertex2d(x + sizeFactor / m_fScale, y);
						glVertex2d(x, y - sizeFactor / m_fScale);
						glVertex2d(x, y + sizeFactor / m_fScale);
					}
				}
				glEnd();
				break;
			case GRIDLOOK_LINES:
			default:
				glBegin(GL_LINES);
				int i = 0;
				for (x = xb; x < xe; x += cur_step, ++i) {
					if (gf == 1 || (i & mask) != 0) { // greebo: No mask check for major grid
						glVertex2d(x, yb);
						glVertex2d(x, ye);
					}
				}
				i = 0;
				for (y = yb; y < ye; y += cur_step, ++i) {
					if (gf == 1 || (i & mask) != 0) { // greebo: No mask check for major grid
						glVertex2d(xb, y);
						glVertex2d(xe, y);
					}
				}
				glEnd();
				break;
			}
		}
	}

	int nDim1 = (m_viewType == YZ) ? 1 : 0;
	int nDim2 = (m_viewType == XY) ? 1 : 2;

	// draw coordinate text if needed
	if (GlobalXYWnd().showCoordinates()) {
		ui::ColourItem colourGridText = ColourSchemes().getColour("grid_text");
		glColor3fv(colourGridText);
		const float offx = m_vOrigin[nDim2] + h - GlobalOpenGL().m_fontHeight / m_fScale;
		const float offy = m_vOrigin[nDim1] - w + 1 / m_fScale;
		for (x = xb - fmod(xb, stepx); x <= xe; x += stepx) {
			glRasterPos2f(x, offx);
			sprintf(text, "%g", x);
			GlobalOpenGL().drawString(text);
		}
		for (y = yb - fmod(yb, stepy); y <= ye; y += stepy) {
			glRasterPos2f(offy, y);
			sprintf(text, "%g", y);
			GlobalOpenGL().drawString(text);
		}

		if (Active()) {
			ui::ColourItem colourActiveViewName = ColourSchemes().getColour("active_view_name");
			glColor3fv(colourActiveViewName);
		}

		// we do this part (the old way) only if show_axis is disabled
		if (!GlobalXYWnd().showAxes()) {
			glRasterPos2f(m_vOrigin[nDim1] - w + 35 / m_fScale, m_vOrigin[nDim2] + h - 20 / m_fScale);

			GlobalOpenGL().drawString(ViewType_getTitle(m_viewType));
		}
	}

	XYWnd::drawAxis();

	// show current work zone?
	// the work zone is used to place dropped points and brushes
	if (GlobalXYWnd().showWorkzone()) {
		ui::ColourItem colourWorkzone = ColourSchemes().getColour("workzone");
		glColor3fv(colourWorkzone);
		glBegin(GL_LINES);
		const selection::WorkZone& workZone = GlobalSelectionSystem().getWorkZone();
		glVertex2f(xb, workZone.min[nDim2]);
		glVertex2f(xe, workZone.min[nDim2]);
		glVertex2f(xb, workZone.max[nDim2]);
		glVertex2f(xe, workZone.max[nDim2]);
		glVertex2f(workZone.min[nDim1], yb);
		glVertex2f(workZone.min[nDim1], ye);
		glVertex2f(workZone.max[nDim1], yb);
		glVertex2f(workZone.max[nDim1], ye);
		glEnd();
	}
}

void XYWnd::drawBlockGrid (void)
{
	if (GlobalMap().findWorldspawn() == 0)
		return;

	int blockSize = GlobalXYWnd().defaultBlockSize();
	const std::string value = Node_getEntity(*GlobalMap().getWorldspawn())->getKeyValue("_blocksize");
	if (!value.empty())
		blockSize = string::toInt(value);

	float x, y, xb, xe, yb, ye;
	char text[32];

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	Vector4 windowCoords = getWindowCoordinates();

	xb = static_cast<float> (blockSize * floor(windowCoords[0] / blockSize));
	xe = static_cast<float> (blockSize * ceil(windowCoords[1] / blockSize));
	yb = static_cast<float> (blockSize * floor(windowCoords[2] / blockSize));
	ye = static_cast<float> (blockSize * ceil(windowCoords[3] / blockSize));

	// draw major blocks

	glColor3fv(ColourSchemes().getColourVector3("grid_block"));
	glLineWidth(2);

	glBegin(GL_LINES);

	for (x = xb; x <= xe; x += blockSize) {
		glVertex2f(x, yb);
		glVertex2f(x, ye);
	}

	if (m_viewType == XY) {
		for (y = yb; y <= ye; y += blockSize) {
			glVertex2f(xb, y);
			glVertex2f(xe, y);
		}
	}

	glEnd();
	glLineWidth(1);

	// draw coordinate text if needed
	if (m_viewType == XY && m_fScale > .1) {
		for (x = xb; x < xe; x += blockSize)
			for (y = yb; y < ye; y += blockSize) {
				glRasterPos2f(x + (blockSize / 2), y + (blockSize / 2));
				sprintf(text, "%i,%i", (int) floor(x / blockSize), (int) floor(y / blockSize));
				GlobalOpenGL().drawString(text);
			}
	}

	glColor4f(0, 0, 0, 0);
}

void XYWnd::drawCameraIcon (const Vector3& origin, const Vector3& angles)
{
	float x, y;
	double a;

	const float fov = 48.0f / m_fScale;
	const float box = 16.0f / m_fScale;

	if (m_viewType == XY) {
		x = origin[0];
		y = origin[1];
		a = degrees_to_radians(angles[CAMERA_YAW]);
	} else if (m_viewType == YZ) {
		x = origin[1];
		y = origin[2];
		a = degrees_to_radians(angles[CAMERA_PITCH]);
	} else {
		x = origin[0];
		y = origin[2];
		a = degrees_to_radians(angles[CAMERA_PITCH]);
	}

	ui::ColourItem colourCameraIcon = ColourSchemes().getColour("camera_icon");
	glColor3fv(colourCameraIcon);
	glBegin(GL_LINE_STRIP);
	glVertex3f(x - box, y, 0);
	glVertex3f(x, y + (box / 2), 0);
	glVertex3f(x + box, y, 0);
	glVertex3f(x, y - (box / 2), 0);
	glVertex3f(x - box, y, 0);
	glVertex3f(x + box, y, 0);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glVertex3f(x + static_cast<float> (fov * cos(a + c_pi / 4)), y + static_cast<float> (fov * sin(a + c_pi / 4)), 0);
	glVertex3f(x, y, 0);
	glVertex3f(x + static_cast<float> (fov * cos(a - c_pi / 4)), y + static_cast<float> (fov * sin(a - c_pi / 4)), 0);
	glEnd();
}

/// \todo can be greatly simplified but per usual i am in a hurry
/// which is not an excuse, just a fact
void XYWnd::drawSizeInfo (int nDim1, int nDim2, Vector3& vMinBounds, Vector3& vMaxBounds)
{
	if (vMinBounds == vMaxBounds)
		return;

	const char* g_pDimStrings[] = { "x:", "y:", "z:" };
	typedef const char* OrgStrings[2];
	const OrgStrings g_pOrgStrings[] = { { "x:", "y:", }, { "x:", "z:", }, { "y:", "z:", } };

	Vector3 vSize(vMaxBounds - vMinBounds);

	Vector3 colourSelectedBrush = ColourSchemes().getColourVector3("selected_brush");
	glColor3f(colourSelectedBrush[0] * .65f, colourSelectedBrush[1] * .65f, colourSelectedBrush[2] * .65f);

	StringOutputStream dimensions(16);

	if (m_viewType == XY) {
		glBegin(GL_LINES);

		glVertex3f(vMinBounds[nDim1], vMinBounds[nDim2] - 6.0f / m_fScale, 0.0f);
		glVertex3f(vMinBounds[nDim1], vMinBounds[nDim2] - 10.0f / m_fScale, 0.0f);

		glVertex3f(vMinBounds[nDim1], vMinBounds[nDim2] - 10.0f / m_fScale, 0.0f);
		glVertex3f(vMaxBounds[nDim1], vMinBounds[nDim2] - 10.0f / m_fScale, 0.0f);

		glVertex3f(vMaxBounds[nDim1], vMinBounds[nDim2] - 6.0f / m_fScale, 0.0f);
		glVertex3f(vMaxBounds[nDim1], vMinBounds[nDim2] - 10.0f / m_fScale, 0.0f);

		glVertex3f(vMaxBounds[nDim1] + 6.0f / m_fScale, vMinBounds[nDim2], 0.0f);
		glVertex3f(vMaxBounds[nDim1] + 10.0f / m_fScale, vMinBounds[nDim2], 0.0f);

		glVertex3f(vMaxBounds[nDim1] + 10.0f / m_fScale, vMinBounds[nDim2], 0.0f);
		glVertex3f(vMaxBounds[nDim1] + 10.0f / m_fScale, vMaxBounds[nDim2], 0.0f);

		glVertex3f(vMaxBounds[nDim1] + 6.0f / m_fScale, vMaxBounds[nDim2], 0.0f);
		glVertex3f(vMaxBounds[nDim1] + 10.0f / m_fScale, vMaxBounds[nDim2], 0.0f);

		glEnd();

		glRasterPos3f(Betwixt(vMinBounds[nDim1], vMaxBounds[nDim1]), vMinBounds[nDim2] - 20.0f / m_fScale, 0.0f);
		dimensions << g_pDimStrings[nDim1] << vSize[nDim1];
		GlobalOpenGL().drawString(dimensions.toString());
		dimensions.clear();

		glRasterPos3f(vMaxBounds[nDim1] + 16.0f / m_fScale, Betwixt(vMinBounds[nDim2], vMaxBounds[nDim2]), 0.0f);
		dimensions << g_pDimStrings[nDim2] << vSize[nDim2];
		GlobalOpenGL().drawString(dimensions.toString());
		dimensions.clear();

		glRasterPos3f(vMinBounds[nDim1] + 4, vMaxBounds[nDim2] + 8 / m_fScale, 0.0f);
		dimensions << "(" << g_pOrgStrings[0][0] << vMinBounds[nDim1] << "  " << g_pOrgStrings[0][1]
				<< vMaxBounds[nDim2] << ")";
		GlobalOpenGL().drawString(dimensions.toString());
	} else if (m_viewType == XZ) {
		glBegin(GL_LINES);

		glVertex3f(vMinBounds[nDim1], 0, vMinBounds[nDim2] - 6.0f / m_fScale);
		glVertex3f(vMinBounds[nDim1], 0, vMinBounds[nDim2] - 10.0f / m_fScale);

		glVertex3f(vMinBounds[nDim1], 0, vMinBounds[nDim2] - 10.0f / m_fScale);
		glVertex3f(vMaxBounds[nDim1], 0, vMinBounds[nDim2] - 10.0f / m_fScale);

		glVertex3f(vMaxBounds[nDim1], 0, vMinBounds[nDim2] - 6.0f / m_fScale);
		glVertex3f(vMaxBounds[nDim1], 0, vMinBounds[nDim2] - 10.0f / m_fScale);

		glVertex3f(vMaxBounds[nDim1] + 6.0f / m_fScale, 0, vMinBounds[nDim2]);
		glVertex3f(vMaxBounds[nDim1] + 10.0f / m_fScale, 0, vMinBounds[nDim2]);

		glVertex3f(vMaxBounds[nDim1] + 10.0f / m_fScale, 0, vMinBounds[nDim2]);
		glVertex3f(vMaxBounds[nDim1] + 10.0f / m_fScale, 0, vMaxBounds[nDim2]);

		glVertex3f(vMaxBounds[nDim1] + 6.0f / m_fScale, 0, vMaxBounds[nDim2]);
		glVertex3f(vMaxBounds[nDim1] + 10.0f / m_fScale, 0, vMaxBounds[nDim2]);

		glEnd();

		glRasterPos3f(Betwixt(vMinBounds[nDim1], vMaxBounds[nDim1]), 0, vMinBounds[nDim2] - 20.0f / m_fScale);
		dimensions << g_pDimStrings[nDim1] << vSize[nDim1];
		GlobalOpenGL().drawString(dimensions.toString());
		dimensions.clear();

		glRasterPos3f(vMaxBounds[nDim1] + 16.0f / m_fScale, 0, Betwixt(vMinBounds[nDim2], vMaxBounds[nDim2]));
		dimensions << g_pDimStrings[nDim2] << vSize[nDim2];
		GlobalOpenGL().drawString(dimensions.toString());
		dimensions.clear();

		glRasterPos3f(vMinBounds[nDim1] + 4, 0, vMaxBounds[nDim2] + 8 / m_fScale);
		dimensions << "(" << g_pOrgStrings[1][0] << vMinBounds[nDim1] << "  " << g_pOrgStrings[1][1]
				<< vMaxBounds[nDim2] << ")";
		GlobalOpenGL().drawString(dimensions.toString());
	} else {
		glBegin(GL_LINES);

		glVertex3f(0, vMinBounds[nDim1], vMinBounds[nDim2] - 6.0f / m_fScale);
		glVertex3f(0, vMinBounds[nDim1], vMinBounds[nDim2] - 10.0f / m_fScale);

		glVertex3f(0, vMinBounds[nDim1], vMinBounds[nDim2] - 10.0f / m_fScale);
		glVertex3f(0, vMaxBounds[nDim1], vMinBounds[nDim2] - 10.0f / m_fScale);

		glVertex3f(0, vMaxBounds[nDim1], vMinBounds[nDim2] - 6.0f / m_fScale);
		glVertex3f(0, vMaxBounds[nDim1], vMinBounds[nDim2] - 10.0f / m_fScale);

		glVertex3f(0, vMaxBounds[nDim1] + 6.0f / m_fScale, vMinBounds[nDim2]);
		glVertex3f(0, vMaxBounds[nDim1] + 10.0f / m_fScale, vMinBounds[nDim2]);

		glVertex3f(0, vMaxBounds[nDim1] + 10.0f / m_fScale, vMinBounds[nDim2]);
		glVertex3f(0, vMaxBounds[nDim1] + 10.0f / m_fScale, vMaxBounds[nDim2]);

		glVertex3f(0, vMaxBounds[nDim1] + 6.0f / m_fScale, vMaxBounds[nDim2]);
		glVertex3f(0, vMaxBounds[nDim1] + 10.0f / m_fScale, vMaxBounds[nDim2]);

		glEnd();

		glRasterPos3f(0, Betwixt(vMinBounds[nDim1], vMaxBounds[nDim1]), vMinBounds[nDim2] - 20.0f / m_fScale);
		dimensions << g_pDimStrings[nDim1] << vSize[nDim1];
		GlobalOpenGL().drawString(dimensions.toString());
		dimensions.clear();

		glRasterPos3f(0, vMaxBounds[nDim1] + 16.0f / m_fScale, Betwixt(vMinBounds[nDim2], vMaxBounds[nDim2]));
		dimensions << g_pDimStrings[nDim2] << vSize[nDim2];
		GlobalOpenGL().drawString(dimensions.toString());
		dimensions.clear();

		glRasterPos3f(0, vMinBounds[nDim1] + 4.0f, vMaxBounds[nDim2] + 8 / m_fScale);
		dimensions << "(" << g_pOrgStrings[2][0] << vMinBounds[nDim1] << "  " << g_pOrgStrings[2][1]
				<< vMaxBounds[nDim2] << ")";
		GlobalOpenGL().drawString(dimensions.toString());
	}
}

void XYWnd::updateProjection ()
{
	if (m_nWidth == 0 || m_nHeight == 0)
		return;

	m_projection[0] = 1.0f / static_cast<float> (m_nWidth / 2);
	m_projection[5] = 1.0f / static_cast<float> (m_nHeight / 2);
	m_projection[10] = 1.0f / (_maxWorldCoord * m_fScale);

	m_projection[12] = 0.0f;
	m_projection[13] = 0.0f;
	m_projection[14] = -1.0f;

	m_projection[1] = 0.0f;
	m_projection[2] = 0.0f;
	m_projection[3] = 0.0f;

	m_projection[4] = 0.0f;
	m_projection[6] = 0.0f;
	m_projection[7] = 0.0f;

	m_projection[8] = 0.0f;
	m_projection[9] = 0.0f;
	m_projection[11] = 0.0f;

	m_projection[15] = 1.0f;

	m_view.Construct(m_projection, m_modelview, m_nWidth, m_nHeight);
}

/**
 * @note modelview matrix must have a uniform scale, otherwise strange things happen when
 * rendering the rotation manipulator.
 */
void XYWnd::updateModelview ()
{
	const int nDim1 = (m_viewType == YZ) ? 1 : 0;
	const int nDim2 = (m_viewType == XY) ? 1 : 2;

	// translation
	m_modelview[12] = -m_vOrigin[nDim1] * m_fScale;
	m_modelview[13] = -m_vOrigin[nDim2] * m_fScale;
	m_modelview[14] = _maxWorldCoord * m_fScale;

	// axis base
	switch (m_viewType) {
	case XY:
		m_modelview[0] = m_fScale;
		m_modelview[1] = 0;
		m_modelview[2] = 0;

		m_modelview[4] = 0;
		m_modelview[5] = m_fScale;
		m_modelview[6] = 0;

		m_modelview[8] = 0;
		m_modelview[9] = 0;
		m_modelview[10] = -1.0;
		break;
	case XZ:
		m_modelview[0] = m_fScale;
		m_modelview[1] = 0;
		m_modelview[2] = 0;

		m_modelview[4] = 0;
		m_modelview[5] = 0;
		m_modelview[6] = 1.0;

		m_modelview[8] = 0;
		m_modelview[9] = m_fScale;
		m_modelview[10] = 0;
		break;
	case YZ:
		m_modelview[0] = 0;
		m_modelview[1] = 0;
		m_modelview[2] = -1.0;

		m_modelview[4] = m_fScale;
		m_modelview[5] = 0;
		m_modelview[6] = 0;

		m_modelview[8] = 0;
		m_modelview[9] = m_fScale;
		m_modelview[10] = 0;
		break;
	}

	m_modelview[3] = m_modelview[7] = m_modelview[11] = 0;
	m_modelview[15] = 1;

	m_view.Construct(m_projection, m_modelview, m_nWidth, m_nHeight);
}

void XYWnd::draw ()
{
	// clear
	glViewport(0, 0, m_nWidth, m_nHeight);
	Vector3 colourGridBack = ColourSchemes().getColourVector3("grid_background");
	glClearColor(colourGridBack[0], colourGridBack[1], colourGridBack[2], 0);

	glClear(GL_COLOR_BUFFER_BIT);

	// set up viewpoint
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(m_projection);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(m_fScale, m_fScale, 1);
	const int nDim1 = (m_viewType == YZ) ? 1 : 0;
	const int nDim2 = (m_viewType == XY) ? 1 : 2;
	glTranslatef(-m_vOrigin[nDim1], -m_vOrigin[nDim2], 0);

	// Call the image overlay draw method with the window coordinates
	Vector4 windowCoords = getWindowCoordinates();
	GlobalOverlay().draw(windowCoords[0], windowCoords[1], windowCoords[2], windowCoords[3], m_fScale);

	glDisable(GL_LINE_STIPPLE);
	glLineWidth(1);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_DEPTH_TEST);

	drawGrid();

	if (GlobalXYWnd().showBlocks())
		drawBlockGrid();

	glLoadMatrixf(m_modelview);

	unsigned int globalstate = RENDER_COLOURARRAY | RENDER_COLOURWRITE;
	if (!getCameraSettings()->solidSelectionBoxes()) {
		globalstate |= RENDER_LINESTIPPLE;
	}

	{
		XYRenderer renderer(globalstate, m_state_selected);
		Scene_Render(renderer, m_view);
		renderer.render(m_modelview, m_projection);
	}

	glDepthMask(GL_FALSE);

	glLoadMatrixf(m_modelview);

	glDisable(GL_LINE_STIPPLE);

	glLineWidth(1);

	if (GlobalOpenGL().GL_1_3()) {
		glActiveTexture(GL_TEXTURE0);
		glClientActiveTexture(GL_TEXTURE0);
	}

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);

	// greebo: Check, if the brush size info should be displayed (if there are any items selected)
	if (GlobalXYWnd().showSizeInfo() && GlobalSelectionSystem().countSelected() != 0) {
		AABB aabb = selection::algorithm::getCurrentSelectionBounds();
		Vector3 min = aabb.getMins();
		Vector3 max = aabb.getMaxs();
		drawSizeInfo(nDim1, nDim2, min, max);
	}

	if (GlobalXYWnd().showCrossHairs()) {
		Vector3 colour = ColourSchemes().getColourVector3("xyview_crosshairs");
		glColor4f(colour[0], colour[1], colour[2], 0.8f);
		glBegin(GL_LINES);
		if (m_viewType == XY) {
			glVertex2f(2.0f * _minWorldCoord, m_mousePosition[1]);
			glVertex2f(2.0f * _maxWorldCoord, m_mousePosition[1]);
			glVertex2f(m_mousePosition[0], 2.0f * _minWorldCoord);
			glVertex2f(m_mousePosition[0], 2.0f * _maxWorldCoord);
		} else if (m_viewType == YZ) {
			glVertex3f(m_mousePosition[0], 2.0f * _minWorldCoord, m_mousePosition[2]);
			glVertex3f(m_mousePosition[0], 2.0f * _maxWorldCoord, m_mousePosition[2]);
			glVertex3f(m_mousePosition[0], m_mousePosition[1], 2.0f * _minWorldCoord);
			glVertex3f(m_mousePosition[0], m_mousePosition[1], 2.0f * _maxWorldCoord);
		} else {
			glVertex3f(2.0f * _minWorldCoord, m_mousePosition[1], m_mousePosition[2]);
			glVertex3f(2.0f * _maxWorldCoord, m_mousePosition[1], m_mousePosition[2]);
			glVertex3f(m_mousePosition[0], m_mousePosition[1], 2.0f * _minWorldCoord);
			glVertex3f(m_mousePosition[0], m_mousePosition[1], 2.0f * _maxWorldCoord);
		}
		glEnd();
	}

	// greebo: Draw the clipper's control points
	{
		glColor3fv(ColourSchemes().getColour("clipper"));

		glPointSize(4);

		if (GlobalClipper().clipMode()) {
			GlobalClipper().draw(m_fScale);
		}

		glPointSize(1);
	}

	// reset modelview
	glLoadIdentity();
	glScalef(m_fScale, m_fScale, 1);
	glTranslatef(-m_vOrigin[nDim1], -m_vOrigin[nDim2], 0);

	drawCameraIcon(g_pParentWnd->GetCamWnd()->getCameraOrigin(), g_pParentWnd->GetCamWnd()->getCameraAngles());

	if (GlobalXYWnd().showOutline()) {
		if (Active()) {
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, m_nWidth, 0, m_nHeight, 0, 1);

			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();

			// four view mode doesn't colorize
			if (g_pParentWnd->CurrentStyle() == MainFrame::eSplit) {
				ui::ColourItem colourActiveViewName = ColourSchemes().getColour("active_view_name");
				glColor3fv(colourActiveViewName);
			} else {
				switch (m_viewType) {
				case YZ:
					glColor3fv(ColourSchemes().getColourVector3("axis_x"));
					break;
				case XZ:
					glColor3fv(ColourSchemes().getColourVector3("axis_y"));
					break;
				case XY:
					glColor3fv(ColourSchemes().getColourVector3("axis_z"));
					break;
				}
			}
			glBegin(GL_LINE_LOOP);
			glVertex2i(0, 0);
			glVertex2i(m_nWidth - 1, 0);
			glVertex2i(m_nWidth - 1, m_nHeight - 1);
			glVertex2i(0, m_nHeight - 1);
			glEnd();
		}
	}

	glFinish();
}

void XYWnd::updateXORRectangle (Rectangle area)
{
	if (GTK_WIDGET_VISIBLE(getWidget())) {
		m_XORRectangle.set(rectangle_from_area(area.min, area.max, getWidth(), getHeight()));
	}
}
