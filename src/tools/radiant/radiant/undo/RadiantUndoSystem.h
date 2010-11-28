#include "iundo.h"
#include "iregistry.h"
#include "preferencesystem.h"

#include <map>
#include <set>
#include <string>

#include "SnapShot.h"
#include "Operation.h"
#include "Stack.h"
#include "StackFiller.h"

namespace undo {

class RadiantUndoSystem: public UndoSystem, public PreferenceConstructor, public RegistryKeyObserver
{
	public:
		// Radiant Module stuff
		typedef UndoSystem Type;
		STRING_CONSTANT(Name, "*");

		// Return the static instance
		UndoSystem* getTable ()
		{
			return this;
		}

	private:

		static const std::size_t MAX_UNDO_LEVELS = 1024;

		// The undo and redo stacks
		UndoStack _undoStack;
		UndoStack _redoStack;

		typedef std::map<Undoable*, UndoStackFiller> UndoablesMap;
		UndoablesMap _undoables;

		// The operation Observers which get notified on certain events
		typedef std::set<Observer*> ObserverSet;
		ObserverSet _observers;

		std::size_t _undoLevels;

		typedef std::set<UndoTracker*> Trackers;
		Trackers _trackers;
	public:

		// Constructor
		RadiantUndoSystem ();

		~RadiantUndoSystem ();

		// Gets called as soon as the observed registry keys get changed
		void keyChanged (const std::string& changedKey, const std::string& newValue);

		UndoObserver* observer (Undoable* undoable);

		void release (Undoable* undoable);

		// Sets the size of the undoStack
		void setLevels (std::size_t levels);

		std::size_t getLevels () const;

		std::size_t size () const;

		void startUndo ();

		bool finishUndo (const std::string& command);

		void startRedo ();

		bool finishRedo (const std::string& command);

		void start ();

		void finish (const std::string& command);

		void undo ();

		void redo ();

		void clear ();

		void addObserver (Observer* observer);

		void removeObserver (Observer* observer);

		void clearRedo ();

		void trackerAttach (UndoTracker& tracker);

		void trackerDetach (UndoTracker& tracker);

		void trackersClear () const;

		void trackersClearRedo () const;

		void trackersBegin () const;

		void trackersUndo () const;

		void trackersRedo () const;

		// Gets called by the PreferenceSystem as request to create the according settings page
		void constructPreferencePage (PreferenceGroup& group);

	private:
		// Assigns the given stack to all of the Undoables listed in the map
		void mark_undoables (undo::UndoStack* stack);
}; // class RadiantUndoSystem

} // namespace undo
