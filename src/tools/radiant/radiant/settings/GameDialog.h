#ifndef GAMEDIALOG_H_
#define GAMEDIALOG_H_

#include "../dialog.h"

typedef struct _GtkWindow GtkWindow;

/*!
 standalone dialog for games selection, and more generally global settings
 */
class CGameDialog: public Dialog
{
	public:

		/*!
		 those settings are saved in the global prefs file
		 I'm too lazy to wrap behind protected access, not sure this needs to be public
		 NOTE: those are preference settings. if you change them it is likely that you would
		 have to restart the editor for them to take effect
		 */

		CGameDialog ()
		{
		}
		virtual ~CGameDialog ();

		void Init ();

		/*!
		 reset the global settings by removing the file
		 */
		void Reset ();

		/*!
		 Dialog API
		 this is only called when the dialog is built at startup for main engine select
		 */
		GtkWindow* BuildDialog ();
};

#endif
