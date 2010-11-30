#include "UndoStateTracker.h"

/**
 * Updates the sensitivity of save, undo and redo buttons and menu items according to actual state.
 */
void UndoSaveStateTracker::UpdateSensitiveStates (void)
{
}

/**
 * This stores actual step as the saved step.
 */
void UndoSaveStateTracker::storeState (void)
{
	m_savedStep = m_undoSteps;
	UpdateSensitiveStates();
}

/**
 * increase redo steps if undo level supports it
 */
void UndoSaveStateTracker::increaseRedo (void)
{
	if (m_redoSteps < GlobalUndoSystem().getLevels()) {
		m_redoSteps++;
	}
}

/**
 * adjusts undo level if needed (e.g. if undo queue size was lowered)
 */
void UndoSaveStateTracker::checkUndoLevel (void)
{
	const unsigned int currentLevel = GlobalUndoSystem().getLevels();
	if (m_undoSteps > currentLevel)
		m_undoSteps = currentLevel;
}

/**
 * Increase undo steps if undo level supports it. If undo exceeds current undo level,
 * lower saved steps to ensure that save state is handled correctly.
 */
void UndoSaveStateTracker::increaseUndo (void)
{
	if (m_undoSteps < GlobalUndoSystem().getLevels()) {
		m_undoSteps++;
	} else {
		checkUndoLevel();
		m_savedStep--;
	}
}

/**
 * Begin a new step, invalidates all other redo states
 */
void UndoSaveStateTracker::begin (void)
{
	increaseUndo();
	m_redoSteps = 0;
	UpdateSensitiveStates();
}

/**
 * Reset all states, no save, redo and undo is available
 */
void UndoSaveStateTracker::clear (void)
{
	m_redoSteps = 0;
	m_undoSteps = 0;
	m_savedStep = 0;
	UpdateSensitiveStates();
}

/**
 * Reset redo states
 */
void UndoSaveStateTracker::clearRedo (void)
{
	m_redoSteps = 0;
	UpdateSensitiveStates();
}

/**
 * Redo a step, one more undo is available
 */
void UndoSaveStateTracker::redo (void)
{
	m_redoSteps--;
	increaseUndo();
	UpdateSensitiveStates();
}

/**
 * Undo a step, on more redo is available
 */
void UndoSaveStateTracker::undo (void)
{
	increaseRedo();
	checkUndoLevel();
	m_undoSteps--;
	UpdateSensitiveStates();
}
