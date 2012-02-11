#include "Clipper.h"

#include "radiant_i18n.h"
#include "iscenegraph.h"
#include "ieventmanager.h"
#include "iselection.h"
#include "preferencesystem.h"
#include "stringio.h"
#include "shaderlib.h"

#include "../ui/mainframe/mainframe.h"
#include "../brush/csg/csg.h"
#include "../sidebar/texturebrowser.h"

namespace {
const std::string RKEY_CLIPPER_CAULK_SHADER = "user/ui/clipper/caulkTexture";
const std::string RKEY_CLIPPER_USE_CAULK = "user/ui/clipper/useCaulk";
}

BrushClipper::BrushClipper () :
		_movingClip(NULL), _switch(false), _useCaulk(GlobalRegistry().getBool(RKEY_CLIPPER_USE_CAULK)), _caulkShader(
				GlobalRegistry().get(RKEY_CLIPPER_CAULK_SHADER))
{
	GlobalRegistry().addKeyObserver(this, RKEY_CLIPPER_USE_CAULK);
	GlobalRegistry().addKeyObserver(this, RKEY_CLIPPER_CAULK_SHADER);

	// greebo: Register this class in the preference system so that the
	// constructPreferencePage() gets called.
	GlobalPreferenceSystem().addConstructor(this);

	GlobalEventManager().addCommand("FlipClip", MemberCaller<BrushClipper, &BrushClipper::flipClip>(*this));
	GlobalEventManager().addCommand("ClipSelected", MemberCaller<BrushClipper, &BrushClipper::clip>(*this));
	GlobalEventManager().addCommand("SplitSelected", MemberCaller<BrushClipper, &BrushClipper::splitClip>(*this));
}

// Update the internally stored variables on registry key change
void BrushClipper::keyChanged (const std::string& changedKey, const std::string& newValue)
{
	_caulkShader = GlobalRegistry().get(RKEY_CLIPPER_CAULK_SHADER);
	_useCaulk = GlobalRegistry().getBool(RKEY_CLIPPER_USE_CAULK);
}

void BrushClipper::constructPreferencePage (PreferenceGroup& group)
{
	PreferencesPage* page = group.createPage(_("Clipper"), _("Clipper Tool Settings"));

	page->appendCheckBox("", _("Clipper tool uses caulk"), RKEY_CLIPPER_USE_CAULK);
	page->appendTextureEntry(_("Caulk texture name"), RKEY_CLIPPER_CAULK_SHADER);
}

EViewType BrushClipper::getViewType () const
{
	return _viewType;
}

void BrushClipper::setViewType (EViewType viewType)
{
	_viewType = viewType;
}

ClipPoint* BrushClipper::getMovingClip ()
{
	return _movingClip;
}

Vector3& BrushClipper::getMovingClipCoords() {
	// Check for NULL pointers, one never knows
	if (_movingClip != NULL) {
		return _movingClip->_coords;
	}

	// Return at least anything, i.e. the coordinates of the first clip point
	return _clipPoints[0]._coords;
}

void BrushClipper::setMovingClip (ClipPoint* clipPoint)
{
	_movingClip = clipPoint;
}

const std::string BrushClipper::getShader () const
{
	return (_useCaulk) ? GlobalTexturePrefix_get() + _caulkShader : GlobalTextureBrowser().getSelectedShader();
}

// greebo: Cycles through the three possible clip points and returns the nearest to point (for selectiontest)
ClipPoint* BrushClipper::find (const Vector3& point, EViewType viewtype, float scale)
{
	double bestDistance = FLT_MAX;

	ClipPoint* bestClip = NULL;

	for (unsigned int i = 0; i < NUM_CLIP_POINTS; i++) {
		_clipPoints[i].testSelect(point, viewtype, scale, bestDistance, bestClip);
	}

	return bestClip;
}

// Returns true if at least two clip points are set
bool BrushClipper::valid () const
{
	return _clipPoints[0].isSet() && _clipPoints[1].isSet();
}

void BrushClipper::draw (float scale)
{
	// Draw clip points
	for (unsigned int i = 0; i < NUM_CLIP_POINTS; i++) {
		if (_clipPoints[i].isSet())
			_clipPoints[i].Draw(i, scale);
	}
}

void BrushClipper::getPlanePoints (Vector3 planepts[3], const AABB& bounds) const
{
	ASSERT_MESSAGE(valid(), "clipper points not initialised");

	planepts[0] = _clipPoints[0]._coords;
	planepts[1] = _clipPoints[1]._coords;
	planepts[2] = _clipPoints[2]._coords;

	Vector3 maxs(bounds.origin + bounds.extents);
	Vector3 mins(bounds.origin - bounds.extents);

	if (!_clipPoints[2].isSet()) {
		int n = (_viewType == XY) ? 2 : (_viewType == YZ) ? 0 : 1;
		int x = (n == 0) ? 1 : 0;
		int y = (n == 2) ? 1 : 2;

		// on viewtype XZ, flip clip points
		if (n == 1) {
			planepts[0][n] = maxs[n];
			planepts[1][n] = maxs[n];
			planepts[2][x] = _clipPoints[0]._coords[x];
			planepts[2][y] = _clipPoints[0]._coords[y];
			planepts[2][n] = mins[n];
		} else {
			planepts[0][n] = mins[n];
			planepts[1][n] = mins[n];
			planepts[2][x] = _clipPoints[0]._coords[x];
			planepts[2][y] = _clipPoints[0]._coords[y];
			planepts[2][n] = maxs[n];
		}
	}
}

void BrushClipper::update ()
{
	Vector3 planepts[3];
	if (!valid()) {
		planepts[0] = Vector3(0, 0, 0);
		planepts[1] = Vector3(0, 0, 0);
		planepts[2] = Vector3(0, 0, 0);
		Scene_BrushSetClipPlane(GlobalSceneGraph(), Plane3(0, 0, 0, 0));
	} else {
		AABB bounds(Vector3(0, 0, 0), Vector3(64, 64, 64));
		getPlanePoints(planepts, bounds);
		if (_switch) {
			std::swap(planepts[0], planepts[1]);
		}
		Scene_BrushSetClipPlane(GlobalSceneGraph(), Plane3(planepts));
	}
	ClipperChangeNotify();
}

void BrushClipper::flipClip ()
{
	_switch = !_switch;
	update();
	ClipperChangeNotify();
}

void BrushClipper::reset ()
{
	for (unsigned int i = 0; i < NUM_CLIP_POINTS; i++) {
		_clipPoints[i].reset();
	}
}

void BrushClipper::clip ()
{
	if (clipMode() && valid()) {
		UndoableCommand undo("clipperClip");
		Vector3 planepts[3];
		AABB bounds(Vector3(0, 0, 0), Vector3(64, 64, 64));
		getPlanePoints(planepts, bounds);

		Scene_BrushSplitByPlane(GlobalSceneGraph(), planepts[0], planepts[1], planepts[2], getShader(),
				(_switch) ? eFront : eBack);

		reset();
		update();
		ClipperChangeNotify();
	}
}

void BrushClipper::splitClip ()
{
	if (clipMode() && valid()) {
		UndoableCommand undo("clipperSplit");
		Vector3 planepts[3];
		AABB bounds(Vector3(0, 0, 0), Vector3(64, 64, 64));
		getPlanePoints(planepts, bounds);

		Scene_BrushSplitByPlane(GlobalSceneGraph(), planepts[0], planepts[1], planepts[2], getShader(), eFrontAndBack);

		reset();
		update();
		ClipperChangeNotify();
	}
}

bool BrushClipper::clipMode () const
{
	return GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eClip;
}

void BrushClipper::onClipMode (bool enabled)
{
	// Revert all clip points to <0,0,0> values
	reset();

	// Revert the _movingClip pointer, if clip mode to be disabled
	if (!enabled && _movingClip) {
		_movingClip = 0;
	}

	update();
	ClipperChangeNotify();
}

void BrushClipper::newClipPoint (const Vector3& point)
{
	if (_clipPoints[0].isSet() == false) {
		_clipPoints[0]._coords = point;
		_clipPoints[0].Set(true);
	} else if (_clipPoints[1].isSet() == false) {
		_clipPoints[1]._coords = point;
		_clipPoints[1].Set(true);
	} else if (_clipPoints[2].isSet() == false) {
		_clipPoints[2]._coords = point;
		_clipPoints[2].Set(true);
	} else {
		// All three clip points were already set, restart with the first one
		reset();
		_clipPoints[0]._coords = point;
		_clipPoints[0].Set(true);
	}

	update();
	ClipperChangeNotify();
}

/* BrushClipper dependencies class.
 */

class BrushClipperDependencies: public GlobalRegistryModuleRef,
		public GlobalRadiantModuleRef,
		public GlobalPreferenceSystemModuleRef,
		public GlobalSelectionModuleRef,
		public GlobalSceneGraphModuleRef
{
};

/* Required code to register the module with the ModuleServer.
 */

#include "modulesystem/singletonmodule.h"

typedef SingletonModule<BrushClipper, BrushClipperDependencies> BrushClipperModule;

typedef Static<BrushClipperModule> StaticBrushClipperSystemModule;
StaticRegisterModule staticRegisterBrushClipperSystem(StaticBrushClipperSystemModule::instance());
