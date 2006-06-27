#include <gtk/gtk.h>

void on_beenden1_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_info1_activate (GtkMenuItem *menuitem, gpointer user_data);
void mission_save (GtkButton *button, gpointer user_data);
void motion_notify_event (GtkWidget *widget, GdkEventMotion *event);
void button_press_event (GtkWidget *widget, GdkEventButton *event);
void button_mis_txt_cancel (GtkWidget *widget, GdkEventButton *event);
void button_mission_dialog_cancel (GtkWidget *widget, GdkEventButton *event);

