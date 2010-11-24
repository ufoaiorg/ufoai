#ifndef GAMEDIALOG_H_
#define GAMEDIALOG_H_

#include "iregistry.h"
#include "igamemanager.h"

#include "preferencesystem.h"
#include "moduleobservers.h"

class GameDescription;

namespace ui {

/*!
 standalone dialog for games selection, and more generally global settings
 */
class GameManager: public IGameManager, public PreferenceConstructor, public RegistryKeyObserver // Observes the engine path
{
	public:
		// Radiant Module stuff
		typedef IGameManager Type;
		STRING_CONSTANT(Name, "*");

		// Return the static instance
		IGameManager* getTable ()
		{
			return this;
		}

	private:

		GameDescription* _currentGameDescription;
		std::string _enginePath;
		std::string _cleanedEnginePath;
		const std::string _emptyString;

		ModuleObservers _enginePathObservers;
		std::size_t _enginepathUnrealised;

	private:

		bool settingsValid () const;

		void initialise ();

	public:

		GameManager ();

		virtual ~GameManager ();

		const std::string& getKeyValue (const std::string& key) const;

		const std::string& getEnginePath () const;

		void init();
		void destroy();

		/** greebo: RegistryKeyObserver implementation, gets notified
		 * 			upon engine path changes.
		 */
		void keyChanged (const std::string& key, const std::string& val);

		void constructPreferencePage (PreferenceGroup& group);
};

} // namespace ui

#endif
