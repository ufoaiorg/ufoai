#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

void on_beenden1_activate (GtkMenuItem *menuitem, gpointer user_data)
{

}

void on_info1_activate (GtkMenuItem *menuitem, gpointer user_data)
{

}

void mission_save (GtkButton *button, gpointer user_data)
{
  gtk_widget_show( mis_txt );
  gtk_widget_hide( mission_dialog );
}

void button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  GtkWidget *mission_dialog;
  mission_dialog = create_mission_dialog ();
  gtk_widget_show (mission_dialog);
}

void motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
  int x, y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &x, &y, &state);
  else {
    x = event->x;
    y = event->y;
    state = event->state;
  }

/*  if (state & GDK_BUTTON1_MASK)
    print_coordinates(widget, x, y);*/
}

void button_mission_dialog_cancel (GtkWidget *widget, GdkEventButton *event)
{
  gtk_widget_hide( mission_dialog );
}

void button_mis_txt_cancel (GtkWidget *widget, GdkEventButton *event)
{
  gtk_widget_hide( mis_txt );
}
