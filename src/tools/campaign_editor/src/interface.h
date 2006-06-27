#define VERSION "0.1"
#define WEBSITE "http://www.ufoai.net"
#define NAME "Campaign editor for UFO:AI"

GtkWidget* create_campaign_editor (void);
GtkWidget* create_mission_dialog (void);
GtkWidget* create_mis_txt (void);
void create_about_box (void);

GtkWidget *mis_txt;
GtkWidget *campaign_editor;
GtkWidget *mission_dialog;
