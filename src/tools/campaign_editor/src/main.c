/**
 * @file main.c
 * @brief Main file for UFO:AI campaign editor
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "interface.h"
#include "support.h"

int main (int argc, char *argv[])
{
#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  gtk_set_locale ();
  gtk_init (&argc, &argv);

  add_pixmap_directory ("base/pics/menu");
  add_pixmap_directory (PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");

  campaign_editor = create_campaign_editor ();
  gtk_widget_show (campaign_editor);
  mis_txt = create_mis_txt();
  mission_dialog = create_mission_dialog ();
  gtk_main ();
  return 0;
}

