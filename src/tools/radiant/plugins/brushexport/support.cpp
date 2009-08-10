#include "support.h"

GtkWidget* lookup_widget (GtkWidget *widget, const gchar *widgetName)
{
	GtkWidget *parent, *foundWidget;

	for (;;) {
		if (GTK_IS_MENU (widget))
			parent = gtk_menu_get_attach_widget(GTK_MENU (widget));
		else
			parent = widget->parent;
		if (!parent)
			parent = (GtkWidget*) g_object_get_data(G_OBJECT (widget), "GladeParentKey");
		if (parent == NULL)
			break;
		widget = parent;
	}

	foundWidget = (GtkWidget*) g_object_get_data(G_OBJECT (widget), widgetName);
	if (!foundWidget)
		g_warning("Widget not found: %s", widgetName);
	return foundWidget;
}
