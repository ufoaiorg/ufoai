#include "igrid.h"

#include <map>
#include "iradiant.h"
#include "ieventmanager.h"
#include "iregistry.h"
#include "preferencesystem.h"
#include "signal/signal.h"
#include "radiant_i18n.h"

#include <iostream>

#include "GridItem.h"

namespace {
const std::string RKEY_DEFAULT_GRID_SIZE = "user/ui/grid/defaultGridPower";
const std::string RKEY_GRID_LOOK_MAJOR = "user/ui/grid/majorGridLook";
const std::string RKEY_GRID_LOOK_MINOR = "user/ui/grid/minorGridLook";
}

class GridManager: public IGridManager, public RegistryKeyObserver, public PreferenceConstructor
{
	public:
		// Radiant Module stuff
		typedef IGridManager Type;
		STRING_CONSTANT(Name, "*");

		// Return the static instance
		IGridManager* getTable ()
		{
			return this;
		}

	private:
		typedef std::map<const std::string, GridItem> GridItemMap;

		GridItemMap _gridItems;

		// The currently active grid size
		GridSize _activeGridSize;

		Signal0 _gridChangeCallbacks;

	public:
		GridManager () :
			_activeGridSize(GRID_8)
		{
			populateGridItems();
			registerCommands();

			// Connect self to the according registry keys
			GlobalRegistry().addKeyObserver(this, RKEY_DEFAULT_GRID_SIZE);

			// greebo: Register this class in the preference system so that the constructPreferencePage() gets called.
			GlobalPreferenceSystem().addConstructor(this);

			// Load the default value from the registry
			keyChanged("", "");
		}

		void keyChanged (const std::string& changedKey, const std::string& newValue)
		{
			// Get the registry value
			int registryValue = GlobalRegistry().getInt(RKEY_DEFAULT_GRID_SIZE);

			// Constrain the values to the allowed ones
			if (registryValue < GRID_0125) {
				registryValue = static_cast<int> (GRID_0125);
			}

			if (registryValue > GRID_256) {
				registryValue = static_cast<int> (GRID_256);
			}

			_activeGridSize = static_cast<GridSize> (registryValue);

			// Notify the world about the grid change
			gridChangeNotify();
		}

		void populateGridItems ()
		{
			// Populate the GridItem map
			_gridItems["0.125"] = GridItem(GRID_0125, this);
			_gridItems["0.25"] = GridItem(GRID_025, this);
			_gridItems["0.5"] = GridItem(GRID_05, this);
			_gridItems["1"] = GridItem(GRID_1, this);
			_gridItems["2"] = GridItem(GRID_2, this);
			_gridItems["4"] = GridItem(GRID_4, this);
			_gridItems["8"] = GridItem(GRID_8, this);
			_gridItems["16"] = GridItem(GRID_16, this);
			_gridItems["32"] = GridItem(GRID_32, this);
			_gridItems["64"] = GridItem(GRID_64, this);
			_gridItems["128"] = GridItem(GRID_128, this);
			_gridItems["256"] = GridItem(GRID_256, this);
		}

		void registerCommands ()
		{
			for (GridItemMap::iterator i = _gridItems.begin(); i != _gridItems.end(); ++i) {
				std::string toggleName = "SetGrid";
				toggleName += i->first; // Makes "SetGrid" to "SetGrid64", for example
				GridItem& gridItem = i->second;

				GlobalEventManager().addToggle(toggleName, GridItem::ActivateCaller(gridItem));
			}

			GlobalEventManager().addCommand("GridDown", MemberCaller<GridManager, &GridManager::gridDown> (*this));
			GlobalEventManager().addCommand("GridUp", MemberCaller<GridManager, &GridManager::gridUp> (*this));
		}

		ComboBoxValueList getGridList ()
		{
			ComboBoxValueList returnValue;

			for (GridItemMap::iterator i = _gridItems.begin(); i != _gridItems.end(); ++i) {
				returnValue.push_back(i->first);
			}

			return returnValue;
		}

		void constructPreferencePage (PreferenceGroup& group)
		{
			PreferencesPage* page(group.createPage(_("Grid"), _("Default Grid Settings")));
			page->appendCombo("Default Grid Size", RKEY_DEFAULT_GRID_SIZE, getGridList());

			ComboBoxValueList looks;

			looks.push_back(_("Lines"));
			looks.push_back(_("Dotted Lines"));
			looks.push_back(_("More Dotted Lines"));
			looks.push_back(_("Crosses"));
			looks.push_back(_("Dots"));
			looks.push_back(_("Big Dots"));
			looks.push_back(_("Squares"));

			page->appendCombo(_("Major Grid Style"), RKEY_GRID_LOOK_MAJOR, looks);
			page->appendCombo(_("Minor Grid Style"), RKEY_GRID_LOOK_MINOR, looks);
		}

		void addGridChangeCallback (const SignalHandler& handler)
		{
			_gridChangeCallbacks.connectLast(handler);
			handler();
		}

		void gridChangeNotify ()
		{
			_gridChangeCallbacks();
		}

		void gridDown ()
		{
			if (_activeGridSize > GRID_0125) {
				int _activeGridIndex = static_cast<int> (_activeGridSize);
				_activeGridIndex--;
				setGridSize(static_cast<GridSize> (_activeGridIndex));
			}
		}

		void gridUp ()
		{
			if (_activeGridSize < GRID_256) {
				int _activeGridIndex = static_cast<int> (_activeGridSize);
				_activeGridIndex++;
				setGridSize(static_cast<GridSize> (_activeGridIndex));
			}
		}

		void setGridSize (GridSize gridSize)
		{
			_activeGridSize = gridSize;

			gridChanged();
		}

		float getGridSize () const
		{
			return pow(2.0f, static_cast<int> (_activeGridSize));
		}

		int getGridPower () const
		{
			return static_cast<int> (_activeGridSize);
		}

		void gridChanged ()
		{
			for (GridItemMap::iterator i = _gridItems.begin(); i != _gridItems.end(); ++i) {
				std::string toggleName = "SetGrid";
				toggleName += i->first; // Makes "SetGrid" to "SetGrid64", for example
				GridItem& gridItem = i->second;

				GlobalEventManager().setToggled(toggleName, _activeGridSize == gridItem.getGridSize());
			}

			gridChangeNotify();
		}

		static GridLook getLookFromNumber (int i)
		{
			if (i >= GRIDLOOK_LINES && i <= GRIDLOOK_SQUARES) {
				return static_cast<GridLook> (i);
			}

			return GRIDLOOK_LINES;
		}

		GridLook getMajorLook () const
		{
			return getLookFromNumber(GlobalRegistry().getInt(RKEY_GRID_LOOK_MAJOR));
		}

		GridLook getMinorLook () const
		{
			return getLookFromNumber(GlobalRegistry().getInt(RKEY_GRID_LOOK_MINOR));
		}
}; // class GridManager

/* GridManager dependencies class.
 */
class GridManagerDependencies: public GlobalRadiantModuleRef,
		public GlobalRegistryModuleRef,
		public GlobalEventManagerModuleRef,
		public GlobalPreferenceSystemModuleRef
{
};

/* Required code to register the module with the ModuleServer.
 */

#include "modulesystem/singletonmodule.h"

typedef SingletonModule<GridManager, GridManagerDependencies> GridManagerModule;

typedef Static<GridManagerModule> StaticGridManagerSystemModule;
StaticRegisterModule staticRegisterGridManagerSystem(StaticGridManagerSystemModule::instance());
