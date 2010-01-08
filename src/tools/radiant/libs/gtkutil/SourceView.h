#ifndef GTKUTIL_SOURCEVIEW_H_
#define GTKUTIL_SOURCEVIEW_H_

#include <string>
#include "gtkutil/ifc/Widget.h"
#include <stdexcept>

typedef struct _GtkSourceBuffer GtkSourceBuffer;
typedef struct _GtkSourceView GtkSourceView;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkSourceLanguageManager GtkSourceLanguageManager;

namespace gtkutil
{

	class SourceView: public Widget
	{
			// The top-level widget
			GtkWidget* _widget;

			GtkSourceView* _view;
			GtkSourceBuffer* _buffer;

			GtkSourceLanguageManager* _langManager;

		public:
			/**
			 * Constructs a new sourceview with the given language ID as specified
			 * in the .lang files (e.g. "python").
			 *
			 * @param language the language definition to create a source view for.
			 * @param[in] readOnly: Set this to TRUE to disallow editing of the text buffer.
			 *
			 * @throw runtime_error if the given language file wasn't found in the sourceviewer/ subdirectory
			 */
			SourceView (const std::string& language, bool readOnly);

			virtual ~SourceView ();

			void setContents (const std::string& newContents);

			// Returns the contents of the source buffer
			std::string getContents ();

			// place the cursor at the given linenumber
			void goToLine (int lineNumber);

			// Clears the contents of the buffer
			void clear ();

		protected:
			// gtkutil::Widget implementation
			GtkWidget* _getWidget () const
			{
				return _widget;
			}
	};

} // namespace gtkutil

#endif /* GTKUTIL_SOURCEVIEW_H_ */
