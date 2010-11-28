#include "ipathfinding.h"
#include "iregistry.h"
#include "preferencesystem.h"

#include "Routing.h"

namespace routing {

class Pathfinding: public IPathfindingSystem, public PreferenceConstructor, public RegistryKeyObserver
{
	public:
		// Radiant Module stuff
		typedef IPathfindingSystem Type;
		STRING_CONSTANT(Name, "*");

		// Return the static instance
		IPathfindingSystem* getTable ()
		{
			return this;
		}

	private:

		bool _showPathfinding;

		bool _showIn2D;

		Routing *_routingRender;

		bool _showAllLowerLevels;

	public:

		Pathfinding ();

		// Update the internally stored variables on registry key change
		void keyChanged (const std::string& changedKey, const std::string& newValue);

		~Pathfinding ();

		void init ();

		/**
		 * @brief callback function for map changes to update routing data.
		 */
		void onMapValidChanged (void);

		void setShowAllLowerLevels (bool showAllLowerLevels);

		void setShowIn2D (bool showIn2D);

		void setShow (bool show);

		void toggleShowPathfindingIn2D ();

		void toggleShowLowerLevelsForPathfinding ();

		void showPathfinding ();

		/**
		 * @todo Maybe also use the ufo2map output directly
		 * @sa ToolsCompile
		 */
		void toggleShowPathfinding ();

		void constructPreferencePage (PreferenceGroup& group);
};

} // namespace routing
