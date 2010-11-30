#ifndef UNDOSTATETRACKER_H
#define UNDOSTATETRACKER_H

#include "iundo.h"

class UndoSaveStateTracker: public UndoTracker
{
	private:
		unsigned int m_undoSteps;
		unsigned int m_redoSteps;
		int m_savedStep;

		void UpdateSensitiveStates (void);
		void increaseUndo ();
		void increaseRedo ();
		void checkUndoLevel ();
	public:
		UndoSaveStateTracker () :
			m_undoSteps(0), m_redoSteps(0), m_savedStep(0)
		{
		}
		void clear ();
		void clearRedo ();
		void begin ();
		void undo ();
		void redo ();
		void storeState (void);
};

#endif
