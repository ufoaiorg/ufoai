#include "XYWnd.h"

#include "../brush/TexDef.h"
#include "iregistry.h"
#include "iscenegraph.h"
#include "iundo.h"
#include "ifilesystem.h"
#include "ientity.h"
#include "ibrush.h"
#include "iimage.h"

#include "gtkutil/accelerator.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/GLWidgetSentry.h"

#include "os/path.h"

#include "stream/stringstream.h"

#include "../plugin.h"
#include "../brush/brushmanip.h"
#include "../mainframe.h"
#include "../sidebar/texturebrowser.h"
#include "../select.h"
#include "../entity.h"
#include "../renderer.h"
#include "../windowobservers.h"
#include "../camera/GlobalCamera.h"
#include "../ui/ortho/OrthoContextMenu.h"

#include "GlobalXYWnd.h"
#include "XYRenderer.h"
#include "../image.h"
#include "xywindow.h"

void LoadTextureRGBA (qtexture_t* q, unsigned char* pPixels, int nWidth, int nHeight);

extern bool g_bCrossHairs;
extern bool g_brush_always_nodraw;

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

XYWnd::XYWnd () :
	_glWidget(false), m_gl_widget(static_cast<GtkWidget*> (_glWidget)), m_deferredDraw(WidgetQueueDrawCaller(
			*m_gl_widget)), m_deferred_motion(callbackMouseMotion, this), _parent(0), m_window_observer(
			NewWindowObserver()), m_XORRectangle(m_gl_widget), _chaseMouseHandler(0)
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

	m_backgroundActivated = false;
	m_alpha = 1.0f;
	m_xmin = 0.0f;
	m_ymin = 0.0f;
	m_xmax = 0.0f;
	m_ymax = 0.0f;

	m_entityCreate = false;

	m_mnitem_connect = 0;
	m_mnitem_fitface = 0;
	m_mnitem_separator = 0;

	GlobalWindowObservers_add(m_window_observer);
	GlobalWindowObservers_connectWidget(m_gl_widget);

	m_window_observer->setRectangleDrawCallback(MemberCaller1<XYWnd, Rectangle, &XYWnd::updateXORRectangle> (*this));
	m_window_observer->setView(m_view);

	gtk_widget_ref(m_gl_widget);

	gtk_widget_set_events(m_gl_widget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
	GTK_WIDGET_SET_FLAGS(m_gl_widget, GTK_CAN_FOCUS);
	gtk_widget_set_size_request(m_gl_widget, XYWND_MINSIZE_X, XYWND_MINSIZE_Y);

	m_sizeHandler = g_signal_connect(G_OBJECT(m_gl_widget), "size_allocate", G_CALLBACK(callbackSizeAllocate), this);
	m_exposeHandler = g_signal_connect(G_OBJECT(m_gl_widget), "expose_event", G_CALLBACK(callbackExpose), this);

	g_signal_connect(G_OBJECT(m_gl_widget), "button_press_event", G_CALLBACK(callbackButtonPress), this);
	g_signal_connect(G_OBJECT(m_gl_widget), "button_release_event", G_CALLBACK(callbackButtonRelease), this);
	g_signal_connect(G_OBJECT(m_gl_widget), "motion_notify_event", G_CALLBACK(DeferredMotion::gtk_motion), &m_deferred_motion);

	g_signal_connect(G_OBJECT(m_gl_widget), "scroll_event", G_CALLBACK(callbackMouseWheelScroll), this);

	Map_addValidCallback(g_map, DeferredDrawOnMapValidChangedCaller(m_deferredDraw));

	updateProjection();
	updateModelview();

	AddSceneChangeCallback(MemberCaller<XYWnd, &XYWnd::queueDraw> (*this));
	GlobalCamera().addCameraObserver(this);

	PressedButtons_connect(g_pressedButtons, m_gl_widget);
}

XYWnd::~XYWnd (void)
{
	g_signal_handler_disconnect(G_OBJECT(m_gl_widget), m_sizeHandler);
	g_signal_handler_disconnect(G_OBJECT(m_gl_widget), m_exposeHandler);

	gtk_widget_unref(m_gl_widget);

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

void XYWnd::setParent(GtkWindow* parent) {
	_parent = parent;
}

GtkWindow* XYWnd::getParent() const {
	return _parent;
}

EViewType XYWnd::getViewType ()
{
	return m_viewType;
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

	Vector3 mid;
	Select_GetMid(mid);
	GlobalClipPoints()->setViewType(getViewType());
	int nDim = (GlobalClipPoints()->getViewType() == YZ) ? 0 : ((GlobalClipPoints()->getViewType() == XZ) ? 1 : 2);
	point[nDim] = mid[nDim];
	vector3_snap(point, GetGridSize());
	GlobalClipPoints()->newClipPoint(point);
}

void XYWnd::Clipper_OnLButtonDown (int x, int y)
{
	Vector3 mousePosition = g_vector3_identity;
	convertXYToWorld(x, y, mousePosition);
	ClipPoint* foundClipPoint = GlobalClipPoints()->find(mousePosition, m_viewType, m_fScale);
	GlobalClipPoints()->setMovingClip(foundClipPoint);
	if (foundClipPoint == NULL) {
		DropClipPoint(x, y);
	}
}

void XYWnd::Clipper_OnLButtonUp (int x, int y)
{
	GlobalClipPoints()->setMovingClip(NULL);
}

void XYWnd::Clipper_OnMouseMoved (int x, int y)
{
	ClipPoint* movingClip = GlobalClipPoints()->getMovingClip();
	if (movingClip != NULL) {
		convertXYToWorld(x, y, movingClip->_coords);
		snapToGrid(movingClip->_coords);
		GlobalClipPoints()->update();
		ClipperChangeNotify();
	}
}

void XYWnd::Clipper_Crosshair_OnMouseMoved (int x, int y)
{
	Vector3 mousePosition = g_vector3_identity;
	convertXYToWorld(x, y, mousePosition);
	if (GlobalClipPoints()->clipMode() && GlobalClipPoints()->find(mousePosition, m_viewType, m_fScale) != 0) {
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
	if (g_xywindow_globals_private.m_bCamXYUpdate) {
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

void XYWnd::focus ()
{
	Vector3 position;
	getFocusPosition(position);
	positionView(position);
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
	float min_scale = MIN(getWidth(), getHeight()) / (1.1f * (g_MaxWorldCoord - g_MinWorldCoord));
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
 * The call is triggered by a timer, that gets start in XYWnd::chaseMouseMotion();
 */
void XYWnd::chaseMouse (void)
{
	const float multiplier = _chaseMouseTimer.elapsed_msec() / 10.0f;
	scroll(float_to_integer(multiplier * m_chasemouse_delta_x), float_to_integer(multiplier * -m_chasemouse_delta_y));

	mouseMoved(m_chasemouse_current_x, m_chasemouse_current_y, _event->state);
	// greebo: Restart the timer, so that it can trigger again
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

	// These are the events that are allowed
	bool isAllowedEvent = GlobalEventMapper().stateMatchesXYViewEvent(ui::xySelect, state)
			|| GlobalEventMapper().stateMatchesXYViewEvent(ui::xyNewBrushDrag, state);

	// greebo: The mouse chase is only active when the according global is set to true and if we
	// are in the right state
	if (g_xywindow_globals_private.m_bChaseMouse && isAllowedEvent) {
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
	if (Map_Valid(g_map) && ScreenUpdates_Enabled()) {
		xywnd->draw();
		xywnd->m_XORRectangle.set(rectangle_t());
	}
	return FALSE;
}

void XYWnd::CameraMoved ()
{
	if (g_xywindow_globals_private.m_bCamXYUpdate) {
		queueDraw();
	}
}

inline const std::string NewBrushDragGetTexture (void)
{
	const std::string& selectedTexture = GlobalTextureBrowser().getSelectedShader();
	if (g_brush_always_nodraw)
		return "textures/tex_common/nodraw";
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

	mins[nDim] = float_snapped(Select_getWorkZone().min[nDim], GetGridSize());
	maxs[nDim] = float_snapped(Select_getWorkZone().max[nDim], GetGridSize());

	if (maxs[nDim] <= mins[nDim])
		maxs[nDim] = mins[nDim] + GetGridSize();

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
		Node_getTraversable(GlobalRadiant().getMapWorldEntity())->insert(node);

		scene::Path brushpath(makeReference(GlobalSceneGraph().root()));
		brushpath.push(makeReference(*Map_GetWorldspawn(g_map)));
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
	const float fWorkMid = float_mid(Select_getWorkZone().min[nDim], Select_getWorkZone().max[nDim]);
	point[nDim] = float_snapped(fWorkMid, GetGridSize());
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
	_freezePointer.freeze_pointer(_parent != 0 ? _parent : GlobalRadiant().getMainWindow(), callbackMoveDelta,
			this);
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
	_freezePointer.freeze_pointer(_parent != 0 ? _parent : GlobalRadiant().getMainWindow(), zoomDelta,
			this);
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
	if (GlobalEventMapper().stateMatchesXYViewEvent(ui::xyMoveView, event)) {
		beginMove();
		EntityCreate_MouseDown(x, y);
	}

	if (GlobalEventMapper().stateMatchesXYViewEvent(ui::xyZoom, event)) {
		beginZoom();
	}

	if (GlobalEventMapper().stateMatchesXYViewEvent(ui::xyCameraMove, event)) {
		positionCamera(x, y, *g_pParentWnd->GetCamWnd());
	}

	if (GlobalEventMapper().stateMatchesXYViewEvent(ui::xyCameraAngle, event)) {
		orientCamera(x, y, *g_pParentWnd->GetCamWnd());
	}

	// Only start a NewBrushDrag operation, if not other elements are selected
	if (GlobalSelectionSystem().countSelected() == 0 && GlobalEventMapper().stateMatchesXYViewEvent(ui::xyNewBrushDrag,
			event)) {
		NewBrushDrag_Begin(x, y);
		return; // Prevent the call from being passed to the windowobserver
	}

	if (GlobalEventMapper().stateMatchesXYViewEvent(ui::xySelect, event)) {
		// There are two possibilites for the "select" click: Clip or Select
		if (GlobalClipPoints()->clipMode()) {
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
	if (GlobalEventMapper().stateMatchesXYViewEvent(ui::xyCameraMove, state)) {
		positionCamera(x, y, *g_pParentWnd->GetCamWnd());
	}

	if (GlobalEventMapper().stateMatchesXYViewEvent(ui::xyCameraAngle, state)) {
		orientCamera(x, y, *g_pParentWnd->GetCamWnd());
	}

	// Check, if we are in a NewBrushDrag operation and continue it
	if (m_bNewBrushDrag && GlobalEventMapper().stateMatchesXYViewEvent(ui::xyNewBrushDrag, state)) {
		NewBrushDrag(x, y);
		return; // Prevent the call from being passed to the windowobserver
	}

	if (GlobalEventMapper().stateMatchesXYViewEvent(ui::xySelect, state)) {
		// Check, if we have a clip point operation running
		if (GlobalClipPoints()->clipMode() && GlobalClipPoints()->getMovingClip() != 0) {
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

	if (g_bCrossHairs) {
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

	if (GlobalClipPoints()->clipMode() && GlobalEventMapper().stateMatchesXYViewEvent(ui::xySelect, event)) {
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
		point[0] = float_snapped(point[0], GetGridSize());
		point[1] = float_snapped(point[1], GetGridSize());
	} else if (m_viewType == YZ) {
		point[1] = float_snapped(point[1], GetGridSize());
		point[2] = float_snapped(point[2], GetGridSize());
	} else {
		point[0] = float_snapped(point[0], GetGridSize());
		point[2] = float_snapped(point[2], GetGridSize());
	}
}

/**
 * @todo Use @code m_image = GlobalTexturesCache().capture(name) @endcode
 */
void XYWnd::loadBackgroundImage (const std::string& name)
{
	std::string relative = GlobalFileSystem().getRelative(name);
	if (relative == name)
		g_warning("Could not extract the relative path, using full path instead\n");

	std::string fileNameWithoutExt = os::stripExtension(relative);
	Image *image = QERApp_LoadImage(0, fileNameWithoutExt);
	if (!image) {
		g_warning("Could not load texture %s\n", fileNameWithoutExt.c_str());
		return;
	}

	XYWnd *xy = GlobalXYWnd().getActiveXY();
	xy->m_tex = (qtexture_t*) malloc(sizeof(qtexture_t));
	LoadTextureRGBA(xy->m_tex, image->getRGBAPixels(), image->getWidth(),
			image->getHeight());
	g_message("Loaded background texture %s\n", relative.c_str());
	xy->m_backgroundActivated = true;

	int m_ix, m_iy;
	switch (xy->getViewType()) {
	default:
	case XY:
		m_ix = 0;
		m_iy = 1;
		break;
	case XZ:
		m_ix = 0;
		m_iy = 2;
		break;
	case YZ:
		m_ix = 1;
		m_iy = 2;
		break;
	}

	Vector3 min, max;
	Select_GetBounds(min, max);
	xy->m_xmin = min[m_ix];
	xy->m_ymin = min[m_iy];
	xy->m_xmax = max[m_ix];
	xy->m_ymax = max[m_iy];
}

void XYWnd::disableBackground (void)
{
	XYWnd *xy = GlobalXYWnd().getActiveXY();
	xy->m_backgroundActivated = false;
	if (xy->m_tex)
		free(xy->m_tex);
	xy->m_tex = NULL;
}

void XYWnd::drawAxis (void)
{
	if (g_xywindow_globals_private.show_axis) {
		const char g_AxisName[3] = { 'X', 'Y', 'Z' };
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
		GlobalOpenGL().drawChar(g_AxisName[nDim1]);
		glRasterPos2f(28 / m_fScale, -10 / m_fScale);
		GlobalOpenGL().drawChar(g_AxisName[nDim1]);
		glColor3fv(colourY);
		glRasterPos2f(m_vOrigin[nDim1] - w + 25 / m_fScale, m_vOrigin[nDim2] + h - 30 / m_fScale);
		GlobalOpenGL().drawChar(g_AxisName[nDim2]);
		glRasterPos2f(-10 / m_fScale, 28 / m_fScale);
		GlobalOpenGL().drawChar(g_AxisName[nDim2]);
	}
}

void XYWnd::drawBackground (void)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glPolygonMode(GL_FRONT, GL_FILL);

	glBindTexture(GL_TEXTURE_2D, m_tex->texture_number);
	glBegin(GL_QUADS);

	glColor4f(1.0, 1.0, 1.0, m_alpha);
	glTexCoord2f(0.0, 1.0);
	glVertex2f(m_xmin, m_ymin);

	glTexCoord2f(1.0, 1.0);
	glVertex2f(m_xmax, m_ymin);

	glTexCoord2f(1.0, 0.0);
	glVertex2f(m_xmax, m_ymax);

	glTexCoord2f(0.0, 0.0);
	glVertex2f(m_xmin, m_ymax);

	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);

	glPopAttrib();
}

void XYWnd::drawGrid (void)
{
	float x, y, xb, xe, yb, ye;
	float w, h;
	char text[32];
	float step, minor_step, stepx, stepy;
	step = minor_step = stepx = stepy = GetGridSize();

	int minor_power = Grid_getPower();
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

	const int nDim1 = (m_viewType == YZ) ? 1 : 0;
	const int nDim2 = (m_viewType == XY) ? 1 : 2;

	xb = m_vOrigin[nDim1] - w;
	if (xb < region_mins[nDim1])
		xb = region_mins[nDim1];
	xb = step * floor(xb / step);

	xe = m_vOrigin[nDim1] + w;
	if (xe > region_maxs[nDim1])
		xe = region_maxs[nDim1];
	xe = step * ceil(xe / step);

	yb = m_vOrigin[nDim2] - h;
	if (yb < region_mins[nDim2])
		yb = region_mins[nDim2];
	yb = step * floor(yb / step);

	ye = m_vOrigin[nDim2] + h;
	if (ye > region_maxs[nDim2])
		ye = region_maxs[nDim2];
	ye = step * ceil(ye / step);

	// djbob
	// draw minor blocks
	if (g_xywindow_globals_private.d_showgrid) {
		ui::ColourItem colourGridBack = ColourSchemes().getColour("grid_background");
		ui::ColourItem colourGridMinor = ColourSchemes().getColour("grid_minor");
		ui::ColourItem colourGridMajor = ColourSchemes().getColour("grid_major");
		if (colourGridMinor != colourGridBack) {
			glColor3fv(colourGridMinor);

			glBegin(GL_LINES);
			int i = 0;
			for (x = xb; x < xe; x += minor_step, ++i) {
				if ((i & mask) != 0) {
					glVertex2f(x, yb);
					glVertex2f(x, ye);
				}
			}
			i = 0;
			for (y = yb; y < ye; y += minor_step, ++i) {
				if ((i & mask) != 0) {
					glVertex2f(xb, y);
					glVertex2f(xe, y);
				}
			}
			glEnd();
		}

		// draw major blocks
		if (colourGridMajor != colourGridBack) {
			glColor3fv(colourGridMajor);

			glBegin(GL_LINES);
			for (x = xb; x <= xe; x += step) {
				glVertex2f(x, yb);
				glVertex2f(x, ye);
			}
			for (y = yb; y <= ye; y += step) {
				glVertex2f(xb, y);
				glVertex2f(xe, y);
			}
			glEnd();
		}
	}

	// draw coordinate text if needed
	if (g_xywindow_globals_private.show_coordinates) {
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
		if (!g_xywindow_globals_private.show_axis) {
			glRasterPos2f(m_vOrigin[nDim1] - w + 35 / m_fScale, m_vOrigin[nDim2] + h - 20 / m_fScale);

			GlobalOpenGL().drawString(ViewType_getTitle(m_viewType));
		}
	}

	XYWnd::drawAxis();

	// show current work zone?
	// the work zone is used to place dropped points and brushes
	if (g_xywindow_globals_private.d_show_work) {
		glColor3f(1.0f, 0.0f, 0.0f);
		glBegin(GL_LINES);
		glVertex2f(xb, Select_getWorkZone().min[nDim2]);
		glVertex2f(xe, Select_getWorkZone().min[nDim2]);
		glVertex2f(xb, Select_getWorkZone().max[nDim2]);
		glVertex2f(xe, Select_getWorkZone().max[nDim2]);
		glVertex2f(Select_getWorkZone().min[nDim1], yb);
		glVertex2f(Select_getWorkZone().min[nDim1], ye);
		glVertex2f(Select_getWorkZone().max[nDim1], yb);
		glVertex2f(Select_getWorkZone().max[nDim1], ye);
		glEnd();
	}
}

void XYWnd::drawBlockGrid (void)
{
	if (Map_FindWorldspawn(g_map) == 0)
		return;

	const char *value = Node_getEntity(*Map_GetWorldspawn(g_map))->getKeyValue("_blocksize");
	if (strlen(value))
		sscanf(value, "%i", &g_xywindow_globals_private.blockSize);

	if (!g_xywindow_globals_private.blockSize || g_xywindow_globals_private.blockSize > 65536
			|| g_xywindow_globals_private.blockSize < 1024)
		// don't use custom blocksize if it is less than the default, or greater than the maximum world coordinate
		g_xywindow_globals_private.blockSize = 1024;

	float x, y, xb, xe, yb, ye;
	float w, h;
	char text[32];

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	w = (m_nWidth / 2 / m_fScale);
	h = (m_nHeight / 2 / m_fScale);

	const int nDim1 = (m_viewType == YZ) ? 1 : 0;
	const int nDim2 = (m_viewType == XY) ? 1 : 2;

	xb = m_vOrigin[nDim1] - w;
	if (xb < region_mins[nDim1])
		xb = region_mins[nDim1];
	xb = static_cast<float> (g_xywindow_globals_private.blockSize * floor(xb / g_xywindow_globals_private.blockSize));

	xe = m_vOrigin[nDim1] + w;
	if (xe > region_maxs[nDim1])
		xe = region_maxs[nDim1];
	xe = static_cast<float> (g_xywindow_globals_private.blockSize * ceil(xe / g_xywindow_globals_private.blockSize));

	yb = m_vOrigin[nDim2] - h;
	if (yb < region_mins[nDim2])
		yb = region_mins[nDim2];
	yb = static_cast<float> (g_xywindow_globals_private.blockSize * floor(yb / g_xywindow_globals_private.blockSize));

	ye = m_vOrigin[nDim2] + h;
	if (ye > region_maxs[nDim2])
		ye = region_maxs[nDim2];
	ye = static_cast<float> (g_xywindow_globals_private.blockSize * ceil(ye / g_xywindow_globals_private.blockSize));

	// draw major blocks

	glColor3fv(ColourSchemes().getColourVector3("grid_block"));
	glLineWidth(2);

	glBegin(GL_LINES);

	for (x = xb; x <= xe; x += g_xywindow_globals_private.blockSize) {
		glVertex2f(x, yb);
		glVertex2f(x, ye);
	}

	if (m_viewType == XY) {
		for (y = yb; y <= ye; y += g_xywindow_globals_private.blockSize) {
			glVertex2f(xb, y);
			glVertex2f(xe, y);
		}
	}

	glEnd();
	glLineWidth(1);

	// draw coordinate text if needed
	if (m_viewType == XY && m_fScale > .1) {
		for (x = xb; x < xe; x += g_xywindow_globals_private.blockSize)
			for (y = yb; y < ye; y += g_xywindow_globals_private.blockSize) {
				glRasterPos2f(x + (g_xywindow_globals_private.blockSize / 2), y + (g_xywindow_globals_private.blockSize
						/ 2));
				sprintf(text, "%i,%i", (int) floor(x / g_xywindow_globals_private.blockSize), (int) floor(y
						/ g_xywindow_globals_private.blockSize));
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

	glColor3f(0.0, 0.0, 1.0);
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
	m_projection[10] = 1.0f / (g_MaxWorldCoord * m_fScale);

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
	m_modelview[14] = g_MaxWorldCoord * m_fScale;

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

	glDisable(GL_LINE_STIPPLE);
	glLineWidth(1);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_DEPTH_TEST);

	if (m_backgroundActivated)
		drawBackground();
	drawGrid();

	if (g_xywindow_globals_private.show_blocks)
		drawBlockGrid();

	glLoadMatrixf(m_modelview);

	unsigned int globalstate = RENDER_COLOURARRAY | RENDER_COLOURWRITE;
	if (!g_xywindow_globals.m_bNoStipple) {
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

	// greebo: Check, if the brush/patch size info should be displayed (if there are any items selected)
	if (GlobalRegistry().get("user/ui/showSizeInfo") == "1" && GlobalSelectionSystem().countSelected() != 0) {
		Vector3 min, max;
		Select_GetBounds(min, max);
		drawSizeInfo(nDim1, nDim2, min, max);
	}

	if (g_bCrossHairs) {
		glColor4f(0.2f, 0.9f, 0.2f, 0.8f);
		glBegin(GL_LINES);
		if (m_viewType == XY) {
			glVertex2f(2.0f * g_MinWorldCoord, m_mousePosition[1]);
			glVertex2f(2.0f * g_MaxWorldCoord, m_mousePosition[1]);
			glVertex2f(m_mousePosition[0], 2.0f * g_MinWorldCoord);
			glVertex2f(m_mousePosition[0], 2.0f * g_MaxWorldCoord);
		} else if (m_viewType == YZ) {
			glVertex3f(m_mousePosition[0], 2.0f * g_MinWorldCoord, m_mousePosition[2]);
			glVertex3f(m_mousePosition[0], 2.0f * g_MaxWorldCoord, m_mousePosition[2]);
			glVertex3f(m_mousePosition[0], m_mousePosition[1], 2.0f * g_MinWorldCoord);
			glVertex3f(m_mousePosition[0], m_mousePosition[1], 2.0f * g_MaxWorldCoord);
		} else {
			glVertex3f(2.0f * g_MinWorldCoord, m_mousePosition[1], m_mousePosition[2]);
			glVertex3f(2.0f * g_MaxWorldCoord, m_mousePosition[1], m_mousePosition[2]);
			glVertex3f(m_mousePosition[0], m_mousePosition[1], 2.0f * g_MinWorldCoord);
			glVertex3f(m_mousePosition[0], m_mousePosition[1], 2.0f * g_MaxWorldCoord);
		}
		glEnd();
	}

	if (GlobalClipPoints()->clipMode()) {
		GlobalClipPoints()->draw(m_fScale);
	}

	// reset modelview
	glLoadIdentity();
	glScalef(m_fScale, m_fScale, 1);
	glTranslatef(-m_vOrigin[nDim1], -m_vOrigin[nDim2], 0);

	drawCameraIcon(g_pParentWnd->GetCamWnd()->getCameraOrigin(), g_pParentWnd->GetCamWnd()->getCameraAngles());

	if (g_xywindow_globals_private.show_outline) {
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
