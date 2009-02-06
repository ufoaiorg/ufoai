/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

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

#if !defined(INCLUDED_IUNDO_H)
#define INCLUDED_IUNDO_H

/// \file
/// \brief The undo-system interface. Uses the 'memento' pattern.

#include <cstddef>
#include "generic/constant.h"
#include "generic/callbackfwd.h"

/**
 * @brief abstract object that should be stored to preserve actual state of an object
 */
class UndoMemento {
public:
	virtual ~UndoMemento() {}
};


/**
 * @brief interface to implement to get an UndoMemento for the object implementing this interface to get its current state.
 */
class Undoable {
public:
	virtual ~Undoable() {}
	/**
	 * @brief export the current state via UndoMemento
	 * @return a memento that represents current state
	 */
	virtual UndoMemento* exportState() const = 0;
	/**
	 * @brief restore a previously stored memento
	 * @param state state that should be restored
	 */
	virtual void importState(const UndoMemento* state) = 0;
};

class UndoObserver {
public:
	virtual ~UndoObserver() {}
	virtual void save(Undoable* undoable) = 0;
};

class UndoTracker {
public:
	virtual ~UndoTracker() {}
	/**
	 * @brief called whenever tracker system should be reseted
	 */
	virtual void clear() = 0;
	/**
	 * @brief called right before a new action is called, should be used to save current state (e.g. in an UndoMememto)
	 */
	virtual void begin() = 0;
	/**
	 * @brief called whenever an older state should be retrieved
	 */
	virtual void undo() = 0;

	/**
	 * @brief called whenever a previously recorded action is about to be replayed to regenerate a state after undo
	 */
	virtual void redo() = 0;
};

class UndoSystem {
public:
	INTEGER_CONSTANT(Version, 1);
	STRING_CONSTANT(Name, "undo");

	virtual ~UndoSystem() {}
	virtual UndoObserver* observer(Undoable* undoable) = 0;
	virtual void release(Undoable* undoable) = 0;

	virtual std::size_t size() const = 0;
	virtual void start() = 0;
	virtual void finish(const char* command) = 0;
	virtual void undo() = 0;
	virtual void redo() = 0;
	virtual void clear() = 0;

	virtual void trackerAttach(UndoTracker& tracker) = 0;
	virtual void trackerDetach(UndoTracker& tracker) = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<UndoSystem> GlobalUndoModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<UndoSystem> GlobalUndoModuleRef;

inline UndoSystem& GlobalUndoSystem() {
	return GlobalUndoModule::getTable();
}

class UndoableCommand {
	const char* m_command;
public:
	UndoableCommand(const char* command) : m_command(command) {
		GlobalUndoSystem().start();
	}
	~UndoableCommand() {
		GlobalUndoSystem().finish(m_command);
	}
};


#endif
