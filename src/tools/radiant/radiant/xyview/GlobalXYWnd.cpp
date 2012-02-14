#include "GlobalXYWnd.h"

#include "ieventmanager.h"

#include "gtkutil/FramedTransientWidget.h"
#include "stringio.h"
#include "../select.h"
#include "../selection/algorithm/General.h"
#include "../ui/mainframe/mainframe.h"
#include "radiant_i18n.h"

// Constructor
XYWndManager::XYWndManager() :
	_activeXY(NULL), _globalParentWindow(NULL)
{
	// Connect self to the according registry keys
	GlobalRegistry().addKeyObserver(this, RKEY_CHASE_MOUSE);
	GlobalRegistry().addKeyObserver(this, RKEY_CAMERA_XY_UPDATE);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_CROSSHAIRS);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_GRID);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_SIZE_INFO);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_ENTITY_ANGLES);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_ENTITY_NAMES);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_BLOCKS);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_COORDINATES);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_OUTLINE);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_AXES);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_WORKZONE);
	GlobalRegistry().addKeyObserver(this, RKEY_DEFAULT_BLOCKSIZE);
	GlobalRegistry().addKeyObserver(this, RKEY_ALWAYS_CAULK_FOR_NEW_BRUSHES);
	GlobalRegistry().addKeyObserver(this, RKEY_CAULK_TEXTURE);

	// Trigger loading the values of the observed registry keys
	keyChanged("", "");

	// greebo: Register this class in the preference system so that the constructPreferencePage() gets called.
	GlobalPreferenceSystem().addConstructor(this);

	// Add the commands to the EventManager
	registerCommands();
}

// Destructor
XYWndManager::~XYWndManager() {
}

void XYWndManager::construct() {
	XYWnd::captureStates();
}

// Free the allocated XYViews from the heap
void XYWndManager::destroyViews() {
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); ++i) {
		// Free the view from the heap
		XYWnd* xyView = *i;

		delete xyView;
	}
	// Discard the whole list
	_XYViews.clear();
}

// Release the shader states
void XYWndManager::destroy() {
	XYWnd::releaseStates();
}

void XYWndManager::registerCommands() {
	GlobalEventManager().addCommand("NewOrthoView", MemberCaller<XYWndManager, &XYWndManager::createNewOrthoView> (
			*this));

	GlobalEventManager().addCommand("NextView", MemberCaller<XYWndManager, &XYWndManager::toggleActiveView>(*this));
	GlobalEventManager().addCommand("ZoomIn", MemberCaller<XYWndManager, &XYWndManager::zoomIn>(*this));
	GlobalEventManager().addCommand("ZoomOut", MemberCaller<XYWndManager, &XYWndManager::zoomOut>(*this));
	GlobalEventManager().addCommand("ViewTop", MemberCaller<XYWndManager, &XYWndManager::setActiveViewXY>(*this));
	GlobalEventManager().addCommand("ViewSide", MemberCaller<XYWndManager, &XYWndManager::setActiveViewXZ>(*this));
	GlobalEventManager().addCommand("ViewFront", MemberCaller<XYWndManager, &XYWndManager::setActiveViewYZ>(*this));
	GlobalEventManager().addCommand("CenterXYViews", MemberCaller<XYWndManager, &XYWndManager::splitViewFocus>(*this));
	GlobalEventManager().addCommand("CenterXYView", MemberCaller<XYWndManager, &XYWndManager::focusActiveView>(*this));
	GlobalEventManager().addCommand("Zoom100", MemberCaller<XYWndManager, &XYWndManager::zoom100>(*this));

	GlobalEventManager().addRegistryToggle("ToggleCrosshairs", RKEY_SHOW_CROSSHAIRS);
	GlobalEventManager().addRegistryToggle("ToggleGrid", RKEY_SHOW_GRID);
	GlobalEventManager().addRegistryToggle("ShowAngles", RKEY_SHOW_ENTITY_ANGLES);
	GlobalEventManager().addRegistryToggle("ShowNames", RKEY_SHOW_ENTITY_NAMES);
	GlobalEventManager().addRegistryToggle("ShowBlocks", RKEY_SHOW_BLOCKS);
	GlobalEventManager().addRegistryToggle("ShowCoordinates", RKEY_SHOW_COORDINATES);
	GlobalEventManager().addRegistryToggle("ShowWindowOutline", RKEY_SHOW_OUTLINE);
	GlobalEventManager().addRegistryToggle("ShowAxes", RKEY_SHOW_AXES);
	GlobalEventManager().addRegistryToggle("ShowWorkzone", RKEY_SHOW_WORKZONE);
	GlobalEventManager().addRegistryToggle("ToggleAlwaysCaulk", RKEY_ALWAYS_CAULK_FOR_NEW_BRUSHES);
}

void XYWndManager::constructPreferencePage(PreferenceGroup& group) {
	PreferencesPage* page(group.createPage(_("Orthographic"), _("Orthographic View Settings")));

	page->appendCheckBox("", _("View chases mouse cursor during drags"), RKEY_CHASE_MOUSE);
	page->appendCheckBox("", _("Update views on camera move"), RKEY_CAMERA_XY_UPDATE);
	page->appendCheckBox("", _("Show Crosshairs"), RKEY_SHOW_CROSSHAIRS);
	page->appendCheckBox("", _("Show Grid"), RKEY_SHOW_GRID);
	page->appendCheckBox("", _("Show Size Info"), RKEY_SHOW_SIZE_INFO);
	page->appendCheckBox("", _("Show Entity Angle Arrow"), RKEY_SHOW_ENTITY_ANGLES);
	page->appendCheckBox("", _("Show Entity Names"), RKEY_SHOW_ENTITY_NAMES);
	page->appendCheckBox("", _("Show Blocks"), RKEY_SHOW_BLOCKS);
	page->appendCheckBox("", _("Show Coordinates"), RKEY_SHOW_COORDINATES);
	page->appendCheckBox("", _("Show Axes"), RKEY_SHOW_AXES);
	page->appendCheckBox("", _("Show Window Outline"), RKEY_SHOW_OUTLINE);
	page->appendCheckBox("", _("Show Workzone"), RKEY_SHOW_WORKZONE);
	page->appendCheckBox("", _("Always caulk for new brushes"), RKEY_ALWAYS_CAULK_FOR_NEW_BRUSHES);
	page->appendTextureEntry(_("Caulk texture name"), RKEY_CAULK_TEXTURE);
}

// Load/Reload the values from the registry
void XYWndManager::keyChanged(const std::string& changedKey, const std::string& newValue) {
	_chaseMouse = (GlobalRegistry().get(RKEY_CHASE_MOUSE) == "1");
	_camXYUpdate = (GlobalRegistry().get(RKEY_CAMERA_XY_UPDATE) == "1");
	_showCrossHairs = (GlobalRegistry().get(RKEY_SHOW_CROSSHAIRS) == "1");
	_showGrid = (GlobalRegistry().get(RKEY_SHOW_GRID) == "1");
	_showSizeInfo = (GlobalRegistry().get(RKEY_SHOW_SIZE_INFO) == "1");
	_showBlocks = (GlobalRegistry().get(RKEY_SHOW_BLOCKS) == "1");
	_showCoordinates = (GlobalRegistry().get(RKEY_SHOW_COORDINATES) == "1");
	_showOutline = (GlobalRegistry().get(RKEY_SHOW_OUTLINE) == "1");
	_showAxes = (GlobalRegistry().get(RKEY_SHOW_AXES) == "1");
	_showWorkzone = (GlobalRegistry().get(RKEY_SHOW_WORKZONE) == "1");
	_defaultBlockSize = (GlobalRegistry().getInt(RKEY_DEFAULT_BLOCKSIZE));
	_alwaysCaulkForNewBrushes = (GlobalRegistry().get(RKEY_ALWAYS_CAULK_FOR_NEW_BRUSHES) == "1");
	_caulkTexture = GlobalRegistry().get(RKEY_CAULK_TEXTURE);

	updateAllViews();
}

bool XYWndManager::chaseMouse() const {
	return _chaseMouse;
}

bool XYWndManager::camXYUpdate() const {
	return _camXYUpdate;
}

bool XYWndManager::showCrossHairs() const {
	return _showCrossHairs;
}

bool XYWndManager::showBlocks() const {
	return _showBlocks;
}

unsigned int XYWndManager::defaultBlockSize() const {
	return _defaultBlockSize;
}

bool XYWndManager::showCoordinates() const {
	return _showCoordinates;
}

bool XYWndManager::showOutline() const  {
	return _showOutline;
}

bool XYWndManager::showAxes() const {
	return _showAxes;
}

bool XYWndManager::showWorkzone() const {
	return _showWorkzone;
}

bool XYWndManager::showGrid() const {
	return _showGrid;
}

bool XYWndManager::showSizeInfo() const {
	return _showSizeInfo;
}

std::string XYWndManager::getCaulkTexture() const {
	return _caulkTexture;
}

bool XYWndManager::alwaysCaulkForNewBrushes() const {
	return _alwaysCaulkForNewBrushes;
}

void XYWndManager::updateAllViews() {
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); ++i) {
		XYWnd* xyview = *i;

		// Pass the call
		xyview->queueDraw();
	}
}

void XYWndManager::zoomIn() {
	if (_activeXY != NULL) {
		_activeXY->zoomIn();
	}
}

void XYWndManager::zoomOut() {
	if (_activeXY != NULL) {
		_activeXY->zoomOut();
	}
}

void XYWndManager::setActiveViewXY() {
	setActiveViewType(XY);
	positionView(getFocusPosition());
}

void XYWndManager::setActiveViewXZ() {
	setActiveViewType(XZ);
	positionView(getFocusPosition());
}

void XYWndManager::setActiveViewYZ() {
	setActiveViewType(YZ);
	positionView(getFocusPosition());
}

void XYWndManager::splitViewFocus() {
	positionAllViews(getFocusPosition());
}

void XYWndManager::zoom100() {
	GlobalXYWnd().setScale(1);
}

void XYWndManager::focusActiveView() {
	positionView(getFocusPosition());
}

XYWnd* XYWndManager::getActiveXY() const {
	return _activeXY;
}

void XYWndManager::setOrigin(const Vector3& origin) {
	// Cycle through the list of views and set the origin
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); ++i) {
		XYWnd* xyView = *i;

		if (xyView != NULL) {
			// Pass the call
			xyView->setOrigin(origin);
		}
	}
}

void XYWndManager::setScale(float scale) {
	// Cycle through the list of views and set the origin
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); ++i) {
		XYWnd* xyView = *i;

		if (xyView != NULL) {
			// Pass the call
			xyView->setScale(scale);
		}
	}
}

void XYWndManager::positionAllViews(const Vector3& origin) {
	// Cycle through the list of views and set the origin
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); ++i) {
		XYWnd* xyView = *i;

		if (xyView != NULL) {
			// Pass the call
			xyView->positionView(origin);
		}
	}
}

void XYWndManager::positionView(const Vector3& origin) {
	if (_activeXY != NULL) {
		return _activeXY->positionView(origin);
	}
}

EViewType XYWndManager::getActiveViewType() const {
	if (_activeXY != NULL) {
		return _activeXY->getViewType();
	}
	// Return at least anything
	return XY;
}

void XYWndManager::setActiveViewType(EViewType viewType) {
	if (_activeXY != NULL) {
		return _activeXY->setViewType(viewType);
	}
}

void XYWndManager::toggleActiveView() {
	if (_activeXY != NULL) {
		if (_activeXY->getViewType() == XY) {
			_activeXY->setViewType(XZ);
		}
		else if (_activeXY->getViewType() == XZ) {
			_activeXY->setViewType(YZ);
		}
		else {
			_activeXY->setViewType(XY);
		}
	}

	positionView(getFocusPosition());
}

XYWnd* XYWndManager::getView(EViewType viewType) {
	// Cycle through the list of views and get the one matching the type
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); ++i) {
		XYWnd* xyView = *i;

		if (xyView != NULL) {
			// If the view matches, return the pointer
			if (xyView->getViewType() == viewType) {
				return xyView;
			}
		}
	}

	return NULL;
}

void XYWndManager::setActiveXY(XYWnd* wnd) {
	// Notify the currently active XYView that is has been deactivated
	if (_activeXY != NULL) {
		_activeXY->setActive(false);
	}

	// Update the pointer
	_activeXY = wnd;

	// Notify the new active XYView about its activation
	if (_activeXY != NULL) {
		_activeXY->setActive(true);
	}
}

XYWnd* XYWndManager::createXY() {
	// Allocate a new window
	XYWnd* newWnd = new XYWnd();

	// Add it to the internal list and return the pointer
	_XYViews.push_back(newWnd);

	// Tag the new view as active, if there is no active view yet
	if (_activeXY == NULL) {
		_activeXY = newWnd;
	}

	return newWnd;
}

void XYWndManager::setGlobalParentWindow(GtkWindow* globalParentWindow) {
	_globalParentWindow = globalParentWindow;
}

void XYWndManager::destroyOrthoView(XYWnd* xyWnd) {
	if (xyWnd != NULL) {

		// Remove the pointer from the list
		for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); ++i) {
			XYWnd* listItem = (*i);

			// If the view is found, remove it from the list
			if (listItem == xyWnd) {
				// Retrieve the parent from the view (for later destruction)
				GtkWindow* parent = xyWnd->getParent();
				GtkWidget* glWidget = xyWnd->getWidget();

				if (_activeXY == xyWnd) {
					_activeXY = NULL;
				}

				// Destroy the window
				delete xyWnd;

				// Remove it from the list
				_XYViews.erase(i);

				// Destroy the parent window (and the contained frame) as well
				if (parent != NULL) {
					gtk_widget_destroy(GTK_WIDGET(glWidget));
					gtk_widget_destroy(GTK_WIDGET(parent));
				}

				break;
			}
		}
	}
}

gboolean XYWndManager::onDeleteOrthoView(GtkWidget *widget, GdkEvent *event, gpointer data) {
	// Get the pointer to the deleted XY view from data
	XYWnd* deletedView = reinterpret_cast<XYWnd*>(data);

	GlobalXYWnd().destroyOrthoView(deletedView);

	return false;
}

void XYWndManager::createOrthoView(EViewType viewType) {

	// Allocate a new XYWindow
	XYWnd* newWnd = createXY();

	// Add the new XYView GL widget to a framed window
	GtkWidget* window = gtkutil::FramedTransientWidget(XYWnd::getViewTypeTitle(viewType), _globalParentWindow,
			newWnd->getWidget());

	// Connect the destroyed signal to the callback of this class
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(onDeleteOrthoView), newWnd);

	newWnd->setParent(GTK_WINDOW(window));

	// Set the viewtype (and with it the window title)
	newWnd->setViewType(viewType);
}

// Shortcut method for connecting to a GlobalEventManager command
void XYWndManager::createNewOrthoView() {
	createOrthoView(XY);
}

/* greebo: This function determines the point currently being "looked" at, it is used for toggling the ortho views
 * If something is selected the center of the selection is taken as new origin, otherwise the camera
 * position is considered to be the new origin of the toggled orthoview.
*/
Vector3 XYWndManager::getFocusPosition() {
	Vector3 position(0,0,0);

	if (GlobalSelectionSystem().countSelected() != 0) {
		position = selection::algorithm::getCurrentSelectionCenter();
	}
	else {
		position = g_pParentWnd->GetCamWnd()->getCameraOrigin();
	}

	return position;
}

// Accessor function returning a reference to the static instance
XYWndManager& GlobalXYWnd() {
	static XYWndManager _xyWndManager;
	return _xyWndManager;
}
