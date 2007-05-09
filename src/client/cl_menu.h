/**
 * @file cl_menu.h
 * @brief Header for client menu implementation
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CLIENT_CL_MENU_H
#define CLIENT_CL_MENU_H

#define MAX_MENUS			128
#define MAX_MENUNODES		4096
#define MAX_MENUACTIONS		8192
#define MAX_MENUSTACK		32
#define MAX_MENUMODELS		128

/* one unit in the containers is 25x25 */
#define C_UNIT				25
#define C_UNDEFINED			0xFE

#define MAX_MENUTEXTLEN		32768
/* used to speed up buffer safe string copies */
#define MAX_SMALLMENUTEXTLEN	1024

#define MAX_EXLUDERECTS	4

/* max menuscale values */
#define MAX_MENUMODELS_SCALEMENUS 8

/** @brief Model that have more than one part (head, body) but may only use one menu node */
typedef struct menuModel_s {
	char id[MAX_VAR];
	char need[MAX_VAR];
	char anim[MAX_VAR];
	char parent[MAX_VAR];
	char tag[MAX_VAR];
	int skin;
	char model[MAX_QPATH];
	char menuScale[MAX_MENUMODELS_SCALEMENUS][MAX_VAR];	/**< the menu id to scale this model for */
	void *menuScaleMenusPtr[MAX_MENUMODELS_SCALEMENUS];	/**< linked after parsing for faster access */
	vec3_t menuScaleValue[MAX_MENUMODELS_SCALEMENUS];	/**< the scale values for the specific menu */
	int menuScaleCnt;			/**< parsed menuscale menus */
	animState_t animState;
	vec3_t origin, scale, angles, center;
	vec4_t color;				/**< rgba */
	struct menuModel_s *next;
} menuModel_t;

typedef struct menuAction_s {
	int type;
	void *data;
	struct menuAction_s *next;
} menuAction_t;

typedef struct excludeRect_s {
	vec2_t pos, size;
} excludeRect_t;

/** @brief menu node */
typedef struct menuNode_s {
	void *data[6];				/**< needs to be first */
	char name[MAX_VAR];
	char key[MAX_VAR];
	int type;
	vec3_t origin, scale, angles, center;
	vec2_t pos, size, texh, texl;
	menuModel_t *menuModel;		/**< pointer to menumodel definition from models.ufo */
	qboolean noMenuModel;		/**< if this is a model name and no menumodel definition we don't have
								 * to query this again and again */
	byte state;
	byte align;
	int border;					/**< border for this node - thickness in pixel - default 0 - also see bgcolor */
	int padding;				/**< padding for this node - default 3 - see bgcolor */
	qboolean invis, blend;
	int mousefx;
	int horizontalScroll;		/**< if text is too long, the text is horizontally scrolled */
	int textScroll;				/**< textfields - current scroll position */
	int textLines;				/**< How many lines there are (set by MN_DrawMenus)*/
	int timeOut;				/**< ms value until invis is set (see cl.time) */
	int timePushed;				/**< when a menu was pushed this value is set to cl.time */
	qboolean timeOutOnce;		/**< timeOut is decreased if this value is true */
	int num, height;			/**< textfields - num: menutexts-id; height: max. rows to show */
	vec4_t color;				/**< rgba */
	vec4_t bgcolor;				/**< rgba */
	vec4_t bordercolor;			/**< rgba - see border and padding */
	menuAction_t *click, *rclick, *mclick, *wheel, *mouseIn, *mouseOut, *wheelUp, *wheelDown;
	qboolean repeat;			/**< repeat action when "click" is holded */
	excludeRect_t exclude[MAX_EXLUDERECTS];	/**< exclude this for hover or click functions */
	int excludeNum;				/**< how many exclude rects defined? */
	menuDepends_t depends;
	struct menuNode_s *next;
} menuNode_t;

/** @brief menu with all it's nodes linked in */
typedef struct menu_s {
	char name[MAX_VAR];
	int eventTime;
	menuNode_t *firstNode, *initNode, *closeNode, *renderNode;
	menuNode_t *popupNode, *hoverNode, *eventNode, *leaveNode;
} menu_t;

/** @brief linked into menuText - defined in menu scripts via num */
typedef enum {
	TEXT_STANDARD,
	TEXT_LIST,
	TEXT_UFOPEDIA,
	TEXT_BUILDINGS,
	TEXT_BUILDING_INFO,
	TEXT_RESEARCH = 5,
	TEXT_RESEARCH_INFO,
	TEXT_POPUP,
	TEXT_POPUP_INFO,
	TEXT_AIRCRAFT_LIST,
	TEXT_AIRCRAFT = 10,
	TEXT_AIRCRAFT_INFO,
	TEXT_MESSAGESYSTEM,			/**< just a dummy for messagesystem - we use the stack */
	TEXT_CAMPAIGN_LIST,
	TEXT_MULTISELECTION,
	TEXT_PRODUCTION_LIST = 15,
	TEXT_PRODUCTION_AMOUNT,
	TEXT_PRODUCTION_INFO,
	TEXT_EMPLOYEE,
	TEXT_MOUSECURSOR_RIGHT,
	TEXT_PRODUCTION_QUEUED = 20,
	TEXT_HOSPITAL,
	TEXT_STATS_1,
	TEXT_STATS_2,
	TEXT_STATS_3,
	TEXT_STATS_4 = 25,
	TEXT_STATS_5,
	TEXT_BASE_LIST,
	TEXT_BASE_INFO,
	TEXT_TRANSFER_LIST,
	TEXT_MOUSECURSOR_PLAYERNAMES = 30,
	TEXT_CARGO_LIST,
	TEXT_UFOPEDIA_MAILHEADER,
	TEXT_UFOPEDIA_MAIL,
	TEXT_MARKET_NAMES,
	TEXT_MARKET_STORAGE = 35,
	TEXT_MARKET_MARKET,
	TEXT_MARKET_PRICES,
	TEXT_CHAT_WINDOW,

	MAX_MENUTEXTS
} texts_t;

extern inventory_t *menuInventory;
extern const char *menuText[MAX_MENUTEXTS];
extern char popupText[MAX_SMALLMENUTEXTLEN];

/* this is the function where all the sdl_ttf fonts are parsed */
void CL_ParseFont(const char *name, char **text);

/* here they get reinit after a vid_restart */
void CL_InitFonts(void);

typedef struct font_s {
	char name[MAX_VAR];
	int size;
	char style[MAX_VAR];
	char path[MAX_QPATH];
} font_t;

extern font_t *fontSmall;
extern font_t *fontBig;

/* will return the size and the path for each font */
void CL_GetFontData(const char *name, int *size, char *path);

qboolean MN_CursorOnMenu(int x, int y);
void MN_Click(int x, int y);
void MN_RightClick(int x, int y);
void MN_MiddleClick(int x, int y);
void MN_MouseWheel(qboolean down, int x, int y);
void MN_SetViewRect(const menu_t* menu);
void MN_DrawMenus(void);
void MN_DrawItem(vec3_t org, item_t item, int sx, int sy, int x, int y, vec3_t scale, vec4_t color);
void MN_ShutdownMessageSystem(void);
void MN_UnHideNode(menuNode_t* node);
void MN_HideNode(menuNode_t* node);
menuNode_t* MN_GetNodeFromCurrentMenu(const char *name);
void MN_SetNewNodePos(menuNode_t* node, int x, int y);
menuNode_t *MN_GetNode(const menu_t* const menu, const char *name);
menu_t *MN_GetMenu(const char *name);
const char *MN_GetFont(const menu_t *m, const menuNode_t *const n);
void MN_TextScrollBottom(const char* nodeName);
void MN_ExecuteActions(const menu_t* const menu, menuAction_t* const first);
void MN_LinkMenuModels(void);

void MN_ResetMenus(void);
void MN_Shutdown(void);
void MN_ParseMenu(const char *name, char **text);
void MN_ParseMenuModel(const char *name, char **text);
menu_t* MN_PushMenu(const char *name);
void MN_PopMenu(qboolean all);
menu_t* MN_ActiveMenu(void);
void MN_Popup(const char *title, const char *text);
void MN_ParseTutorials(const char *title, char **text);

#endif /* CLIENT_CL_MENU_H */
