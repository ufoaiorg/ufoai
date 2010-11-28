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

#include "RadiantUndoSystem.h"

#include "string/string.h"
#include "radiant_i18n.h"

namespace undo {

namespace {
const std::string RKEY_UNDO_QUEUE_SIZE = "user/ui/undo/queueSize";
}
RadiantUndoSystem::RadiantUndoSystem () :
	_undoLevels(GlobalRegistry().getInt(RKEY_UNDO_QUEUE_SIZE))
{
	// Add self to the key observers to get notified on change
	GlobalRegistry().addKeyObserver(this, RKEY_UNDO_QUEUE_SIZE);

	// greebo: Register this class in the preference system so that the constructPreferencePage() gets called.
	GlobalPreferenceSystem().addConstructor(this);
}

RadiantUndoSystem::~RadiantUndoSystem ()
{
	clear();
}

// Gets called as soon as the observed registry keys get changed
void RadiantUndoSystem::keyChanged (const std::string& changedKey, const std::string& newValue)
{
	_undoLevels = GlobalRegistry().getInt(RKEY_UNDO_QUEUE_SIZE);
}

UndoObserver* RadiantUndoSystem::observer (Undoable* undoable)
{
	return &_undoables[undoable];
}

void RadiantUndoSystem::release (Undoable* undoable)
{
	_undoables.erase(undoable);
}

// Sets the size of the undoStack
void RadiantUndoSystem::setLevels (std::size_t levels)
{
	if (levels > MAX_UNDO_LEVELS) {
		levels = MAX_UNDO_LEVELS;
	}

	while (_undoStack.size() > levels) {
		_undoStack.pop_front();
	}
	_undoLevels = levels;
}

std::size_t RadiantUndoSystem::getLevels () const
{
	return _undoLevels;
}

std::size_t RadiantUndoSystem::size () const
{
	return _undoStack.size();
}

void RadiantUndoSystem::startUndo ()
{
	_undoStack.start("unnamedCommand");
	mark_undoables(&_undoStack);
}

bool RadiantUndoSystem::finishUndo (const std::string& command)
{
	bool changed = _undoStack.finish(command);
	mark_undoables(0);
	return changed;
}

void RadiantUndoSystem::startRedo ()
{
	_redoStack.start("unnamedCommand");
	mark_undoables(&_redoStack);
}

bool RadiantUndoSystem::finishRedo (const std::string& command)
{
	bool changed = _redoStack.finish(command);
	mark_undoables(0);
	return changed;
}

void RadiantUndoSystem::start ()
{
	_redoStack.clear();
	if (_undoStack.size() == _undoLevels) {
		_undoStack.pop_front();
	}
	startUndo();
	trackersBegin();
}

void RadiantUndoSystem::finish (const std::string& command)
{
	if (finishUndo(command)) {
		globalOutputStream() << command << "\n";
	}
}

void RadiantUndoSystem::undo ()
{
	if (_undoStack.empty()) {
		globalOutputStream() << "Undo: no undo available\n";
	} else {
		undo::Operation* operation = _undoStack.back();
		globalOutputStream() << "Undo: " << operation->_command << "\n";

		startRedo();
		trackersUndo();
		operation->_snapshot.restore();
		finishRedo(operation->_command);
		_undoStack.pop_back();

		for (ObserverSet::iterator i = _observers.begin(); i != _observers.end(); /* in-loop */) {
			Observer* observer = *(i++);
			observer->postUndo();
		}
	}
}

void RadiantUndoSystem::redo ()
{
	if (_redoStack.empty()) {
		globalOutputStream() << "Redo: no redo available\n";
	} else {
		undo::Operation* operation = _redoStack.back();
		globalOutputStream() << "Redo: " << operation->_command << "\n";

		startUndo();
		trackersRedo();
		operation->_snapshot.restore();
		finishUndo(operation->_command);
		_redoStack.pop_back();

		for (ObserverSet::iterator i = _observers.begin(); i != _observers.end(); /* in-loop */) {
			Observer* observer = *(i++);
			observer->postRedo();
		}
	}
}

void RadiantUndoSystem::clear ()
{
	mark_undoables(0);
	_undoStack.clear();
	_redoStack.clear();
	trackersClear();
	_observers.clear();
}

void RadiantUndoSystem::addObserver (Observer* observer)
{
	_observers.insert(observer);
}

void RadiantUndoSystem::removeObserver (Observer* observer)
{
	_observers.erase(observer);
}

void RadiantUndoSystem::clearRedo ()
{
	_redoStack.clear();
	trackersClearRedo();
}

void RadiantUndoSystem::trackerAttach (UndoTracker& tracker)
{
	_trackers.insert(&tracker);
}

void RadiantUndoSystem::trackerDetach (UndoTracker& tracker)
{
	_trackers.erase(&tracker);
}

void RadiantUndoSystem::trackersClear () const
{
	for (Trackers::const_iterator i = _trackers.begin(); i != _trackers.end(); ++i) {
		(*i)->clear();
	}
}

void RadiantUndoSystem::trackersClearRedo () const
{
	for (Trackers::const_iterator i = _trackers.begin(); i != _trackers.end(); ++i) {
		(*i)->clearRedo();
	}
}

void RadiantUndoSystem::trackersBegin () const
{
	for (Trackers::const_iterator i = _trackers.begin(); i != _trackers.end(); ++i) {
		(*i)->begin();
	}
}

void RadiantUndoSystem::trackersUndo () const
{
	for (Trackers::const_iterator i = _trackers.begin(); i != _trackers.end(); ++i) {
		(*i)->undo();
	}
}

void RadiantUndoSystem::trackersRedo () const
{
	for (Trackers::const_iterator i = _trackers.begin(); i != _trackers.end(); ++i) {
		(*i)->redo();
	}
}

// Gets called by the PreferenceSystem as request to create the according settings page
void RadiantUndoSystem::constructPreferencePage (PreferenceGroup& group)
{
	PreferencesPage* page(group.createPage(_("Undo"), _("Undo Queue Settings")));
	page->appendSpinner(_("Undo Queue Size"), RKEY_UNDO_QUEUE_SIZE, 0, 1024, 1);
}

// Assigns the given stack to all of the Undoables listed in the map
void RadiantUndoSystem::mark_undoables (undo::UndoStack* stack)
{
	for (UndoablesMap::iterator i = _undoables.begin(); i != _undoables.end(); ++i) {
		i->second.setStack(stack);
	}
}

}
