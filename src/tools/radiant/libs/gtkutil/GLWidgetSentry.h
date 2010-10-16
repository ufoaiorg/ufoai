#ifndef GLWIDGETSENTRY_H_
#define GLWIDGETSENTRY_H_

#include "glwidget.h"

namespace gtkutil
{
	/** Sentry class that calls makeCurrent() on construction and
	 * swapBuffers() on destruction at the end of a scope. This avoids
	 * the need to manually call these functions and use branches to make sure
	 * they are executed.
	 */
	class GLWidgetSentry
	{
			// The GL widget
			GtkWidget* _widget;

		public:

			/** Constructor calls makeCurrent().
			 */
			GLWidgetSentry (GtkWidget* w) :
				_widget(w)
			{
				GLWidget::makeCurrent(_widget);
			}

			/* Destructor swaps the buffers with swapBuffers().
			 */
			~GLWidgetSentry ()
			{
				GLWidget::swapBuffers(_widget);
			}
	};
}

#endif /*GLWIDGETSENTRY_H_*/
