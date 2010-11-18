#ifndef GAMEDIALOG_H_
#define GAMEDIALOG_H_

#include "../dialog.h"

class GameDescription;
typedef struct _GtkWindow GtkWindow;

namespace ui {

/*!
 standalone dialog for games selection, and more generally global settings
 */
class CGameDialog: public Dialog
{
	private:

		GameDescription* _currentGameDescription;

	public:

		virtual ~CGameDialog ();

		static CGameDialog& Instance ();

		void initialise ();

		void setGameDescription (GameDescription* newGameDescription);
		GameDescription* getGameDescription ();

		GtkWindow* BuildDialog ();
};

} // namespace ui

#endif
