#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

static int x, y;

void on_quit_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	exit(0);
}

void on_info_activate (GtkMenuItem *menuitem, gpointer user_data)
{
  aboutdialog = create_about_box();
}

void mission_save (GtkButton *button, gpointer user_data)
{
  gtk_widget_hide( mission_dialog );
  gtk_widget_show( mis_txt );
}

void button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  float xf, yf;

  xf = (0.0f-(float)event->x / 2048 + 0.5) * 360.0;
  yf = (0.0f-(float)event->x / 1024 + 0.5) * 180.0;

  while (xf > 180.0)
    xf -= 360.0;
  while (yf < -180.0)
    yf += 360.0;

  x = xf;
  y = yf;

  gtk_widget_show (mission_dialog);

//  g_print("click: long: %i - lat: %i, %i\n", x, y, event->state);
}

void motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
}

void button_mission_dialog_cancel (GtkWidget *widget, GdkEventButton *event)
{
  gtk_widget_hide( mission_dialog );
}

void button_mis_txt_cancel (GtkWidget *widget, GdkEventButton *event)
{
  gtk_widget_hide( mis_txt );
}
