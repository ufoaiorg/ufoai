/**
 * @file cl_scrn.c
 * @brief Master for refresh, status bar, console, chat, notify, etc
 *
 * Full screen console.
 * Put up loading plaque.
 * Blanked background with loading plaque.
 * Blanked background with menu.
 * Full screen image for quit and victory.
 * End of unit intermissions.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "client.h"

static float scr_con_current;			/* aproaches scr_conlines at scr_conspeed */
static float scr_conlines;				/* 0.0 to 1.0 lines of console to display */

static qboolean scr_initialized = qfalse;/* ready to draw */

static int scr_draw_loading = 0;

vrect_t scr_vrect;				/* position of render window on screen */

static cvar_t *scr_conspeed;
static cvar_t *scr_consize;
static cvar_t *scr_centertime;
static cvar_t *scr_showpause;

static cvar_t *scr_netgraph;
static cvar_t *scr_timegraph;
static cvar_t *scr_debuggraph;
static cvar_t *scr_graphheight;
static cvar_t *scr_graphscale;
static cvar_t *scr_graphshift;
static cvar_t *scr_rspeed;

static char cursor_pic[MAX_QPATH];

static void SCR_TimeRefresh_f(void);
static void SCR_Loading_f(void);
static void SCR_DrawString(int x, int y, const char *string, qboolean bitmapFont);

/*
===============================================================================
BAR GRAPHS
===============================================================================
*/

/**
 * @brief A new packet was just parsed
 */
void CL_AddNetgraph (void)
{
	int i;

	/* if using the debuggraph for something else, don't */
	/* add the net lines */
	if (scr_debuggraph->integer || scr_timegraph->integer)
		return;

	for (i = 0; i < cls.netchan.dropped; i++)
		SCR_DebugGraph(30, 0x40);
}

/** @brief Graph color and value */
typedef struct {
	float value;
	int color;		/**< color value */
} graphsamp_t;

#define GRAPH_WIDTH 256

static int currentPos = 0;
static graphsamp_t values[GRAPH_WIDTH];

/**
 * @brief
 * @sa SCR_DrawDebugGraph
 */
void SCR_DebugGraph (float value, int color)
{
	values[currentPos & (GRAPH_WIDTH-1)].value = value;
	values[currentPos & (GRAPH_WIDTH-1)].color = color;
	currentPos++;
}

/**
 * @brief
 * @sa SCR_DebugGraph
 */
static void SCR_DrawDebugGraph (void)
{
	int a, x, y, w, i, h, min, max;
	float v;
	vec4_t color = {1, 0, 0, 0.5};
	static float lasttime = 0;
	static int fps;
	struct tm *now;
	time_t tnow;
	char timebuf[32], temp_map[32];
	int time_clock, map_time = 0, time_map, hour, mins, secs;

	tnow = time((time_t *) 0);
	now = localtime(&tnow);

	h = w = GRAPH_WIDTH;

	x = scr_vrect.width - (w + 2) - 1;
	y = scr_vrect.height - (h + 2) - 1;

	re.DrawFill(x, y, w, h, 0, color);

	for (a = 0; a < GRAPH_WIDTH; a++) {
		i = (currentPos - 1 - a + GRAPH_WIDTH) & (GRAPH_WIDTH-1);
		v = values[i].value;
		Vector4Set(color, values[i].color, values[i].color, values[i].color, 1.0f);
		v = v * scr_graphscale->value;

		if (v < 1)
			v += h * (1 + (int)(-v / h));

		max = (int)v % h + 1;
		min = y + h - max - scr_graphshift->integer;

		/* bind to box! */
		if (min < y + 1)
			min = y + 1;
		if (min > y + h)
			min = y + h;
		if (min + max > y + h)
			max = y + h - max;

		re.DrawFill(x + w - a, min, 1, max, 0, color);
	}

	if (cls.realtime - lasttime > 50) {
		lasttime = cls.realtime;
		fps = (cls.frametime) ? 1 / cls.frametime : 0;
	}

	if (scr_netgraph->integer == 3) {
		time_clock = strftime(timebuf, 32, "Time: %H:%M", now);
		time_map = cl.time / 1000;
		hour = time_map / 3600;
		mins = (time_map % 3600) / 60;
		secs = time_map % 60;

		if (hour > 0)
			Com_sprintf(temp_map, sizeof(temp_map), "Map:  %i:%02i:%02i", hour, mins, secs);
		else
			Com_sprintf(temp_map, sizeof(temp_map), "Map:  %i:%02i", mins, secs);

		SCR_DrawString(x + 5, y + con_fontHeight->integer, va("FPS:  %3i", fps), qtrue);
		SCR_DrawString(x + 5, y + con_fontHeight->integer * 2, va(timebuf, time_clock), qtrue);
		SCR_DrawString(x + 5, y + con_fontHeight->integer * 3, va(temp_map, map_time), qtrue);
	}
}

/*
===============================================================================
CENTER PRINTING
===============================================================================
*/

static char scr_centerstring[1024];
static float scr_centertime_start;		/* for slow victory printing */
static float scr_centertime_off;
static int scr_center_lines;
static int scr_erase_center;

/**
 * @brief Called for important messages that should stay in the center of the screen for a few moments
 */
void SCR_CenterPrint (const char *str)
{
	const char *s;
	char line[64];
	int i, j, l;

	strncpy(scr_centerstring, str, sizeof(scr_centerstring) - 1);
	scr_centertime_off = scr_centertime->value;
	scr_centertime_start = cl.time;

	/* count the number of lines for centering */
	scr_center_lines = 1;
	s = str;
	while (*s) {
		if (*s == '\n')
			scr_center_lines++;
		s++;
	}

	s = str;
	do {
		/* scan the width of the line */
		for (l = 0; l < 40; l++)
			if (s[l] == '\n' || !s[l])
				break;
		for (i = 0; i < (40 - l) / 2; i++)
			line[i] = ' ';

		for (j = 0; j < l; j++) {
			line[i++] = s[j];
		}

		line[i] = '\n';
		line[i + 1] = 0;

		Com_Printf("%s", line);

		while (*s && *s != '\n')
			s++;

		if (!*s)
			break;
		s++;					/* skip the \n */
	} while (1);
	Con_ClearNotify();
}

/**
 * @brief
 */
static void SCR_DrawCenterString (void)
{
	char *start;
	int l;
	int j;
	int x, y;
	int remaining;

	/* the finale prints the characters one at a time */
	remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = viddef.height * 0.35;
	else
		y = 48;

	do {
		/* scan the width of the line */
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (viddef.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8) {
			re.DrawChar(x, y, start[j]);
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;				/* skip the \n */
	} while (1);
}

/**
 * @brief
 */
static void SCR_CheckDrawCenterString (void)
{
	scr_centertime_off -= cls.frametime;

	if (scr_centertime_off <= 0)
		return;

	SCR_DrawCenterString();
}

/**
 * @brief
 */
void SCR_Init (void)
{
	scr_conspeed = Cvar_Get("scr_conspeed", "3", 0, NULL);
	scr_consize = Cvar_Get("scr_consize", "1.0", 0, NULL);
	scr_showpause = Cvar_Get("scr_showpause", "1", 0, "Show pause image when server is in paused mode"
		" set to 0 to deactivate it, useful for screenshots");
	scr_centertime = Cvar_Get("scr_centertime", "2.5", 0, NULL);
	scr_netgraph = Cvar_Get("netgraph", "0", 0, "Draw the netgraph");
	scr_timegraph = Cvar_Get("timegraph", "0", 0, NULL);
	scr_debuggraph = Cvar_Get("debuggraph", "0", 0, NULL);
	scr_graphheight = Cvar_Get("graphheight", "32", 0, NULL);
	scr_graphscale = Cvar_Get("graphscale", "1", 0, NULL);
	scr_graphshift = Cvar_Get("graphshift", "0", 0, NULL);
	scr_rspeed = Cvar_Get("r_speeds", "0", 0, NULL);

	/* register our commands */
	Cmd_AddCommand("timerefresh", SCR_TimeRefresh_f, NULL);
	Cmd_AddCommand("loading", SCR_Loading_f, NULL);

	SCR_TouchPics();

	scr_initialized = qtrue;
}


/**
 * @brief Draws a net-connection-problems icon
 */
static void SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP - 1)
		return;

	re.DrawPic(scr_vrect.x + 64, scr_vrect.y, "net");
}

/**
 * @brief
 * @sa SCR_DrawLoading
 */
void SCR_DrawLoadingBar (int x, int y, int w, int h, int percent)
{
	static vec4_t color = {0.3f, 0.3f, 0.3f, 0.7f};
	static vec4_t color_bar = {0.8f, 0.8f, 0.8f, 0.7f};

	re.DrawFill(x, y, w, h, ALIGN_UL, color);
	re.DrawColor(NULL);

	if (percent != 0) {
		re.DrawFill((int)(x+(h*0.2)), (int)(y+(h*0.2)), (int)((w-(h*0.4))*percent*0.01), (int)(h*0.6), ALIGN_UL, color_bar);
		re.DrawColor(NULL);
	}
}

/**
 * @brief Precache and loading screen at startup
 * @sa CL_InitAfter
 * @param[in] string Draw the loading string - if the scripts are not parsed, this is
 * not possible, so use qfalse for very early calls
 */
extern void SCR_DrawPrecacheScreen (qboolean string)
{
	re.BeginFrame(0);
	re.DrawNormPic(0, 0, VID_NORM_WIDTH, VID_NORM_HEIGHT, 0, 0, 0, 0, ALIGN_UL, qfalse, "loading");
	if (string)
		re.FontDrawString("f_menubig", ALIGN_UC,
			(int)(VID_NORM_WIDTH / 2),
			(int)(VID_NORM_HEIGHT / 2 - 60),
			0, 1, VID_NORM_WIDTH, VID_NORM_HEIGHT, 50, _("Loading game"), 0, 0, NULL, qfalse);
	SCR_DrawLoadingBar((int)(VID_NORM_WIDTH / 2) - 300, VID_NORM_HEIGHT - 30, 600, 20, (int)cls.loadingPercent);
	re.EndFrame();
}

/**
 * @brief Updates needed cvar for loading screens
 */
extern void SCR_SetLoadingBackground (const char *mapString)
{
	char loadingPic[MAX_QPATH];
	char tmpPicName[MAX_VAR];
	const char *mapname;

	if (!mapString)
		mapname = Cvar_VariableString("mapname");
	else {
		mapname = mapString;
		Cvar_Set("mapname", mapString);
	}

	if (*mapname != '+') {
		Q_strncpyz(tmpPicName, mapname, sizeof(tmpPicName));
		tmpPicName[strlen(tmpPicName)-1] = '\0'; /* strip of the day and night letter */
		if (FS_CheckFile(va("maps/loading/%s.jpg", tmpPicName)))
			Com_sprintf(loadingPic, sizeof(loadingPic), "maps/loading/%s.jpg", tmpPicName);
		else
			Q_strncpyz(loadingPic, "maps/loading/default.jpg", sizeof(loadingPic));
		Cvar_Set("mn_mappicbig", loadingPic);
	} else {
		Q_strncpyz(loadingPic, "maps/loading/default.jpg", sizeof(loadingPic));
		Cvar_Set("mn_mappicbig", loadingPic);
	}
}

/**
 * @brief Draws the current loading pic of the map from base/pics/maps/loading
 * @sa SCR_DrawLoadingBar
 */
static void SCR_DrawLoading (void)
{
	char loadingPic[MAX_QPATH];
	const vec4_t color = {0.0, 0.7, 0.0, 0.8};
	char *mapmsg;

	if (!scr_draw_loading)
		return;

	if (!ccs.singleplayer || !selMis) {
		Com_sprintf(loadingPic, MAX_QPATH, "maps/loading/default.jpg");
	} else {
		if (FS_CheckFile(va("maps/loading/%s.jpg", selMis->def->map)))
			Com_sprintf(loadingPic, MAX_QPATH, "maps/loading/%s.jpg", selMis->def->map);
		else
			Com_sprintf(loadingPic, MAX_QPATH, "maps/loading/default.jpg");
	}
	re.DrawNormPic(0, 0, VID_NORM_WIDTH, VID_NORM_HEIGHT, 0, 0, 0, 0, ALIGN_UL, qfalse, loadingPic);
	re.DrawColor(color);

	if (cl.configstrings[CS_TILES][0]) {
		mapmsg = va(_("Loading Map [%s]"), _(cl.configstrings[CS_MAPTITLE]));
		re.FontDrawString("f_menubig", ALIGN_UC,
			(int)(VID_NORM_WIDTH / 2),
			(int)(VID_NORM_HEIGHT / 2 - 60),
			0, 1, VID_NORM_WIDTH, VID_NORM_HEIGHT, 50, mapmsg, 0, 0, NULL, qfalse);
	}

	re.FontDrawString("f_menu", ALIGN_UC,
		(int)(VID_NORM_WIDTH / 2),
		(int)(VID_NORM_HEIGHT / 2),
		0, 1, VID_NORM_WIDTH, VID_NORM_HEIGHT, 50, cls.loadingMessages, 0, 0, NULL, qfalse);

	SCR_DrawLoadingBar((int)(VID_NORM_WIDTH / 2) - 300, VID_NORM_HEIGHT - 30, 600, 20, (int)cls.loadingPercent);
}

static const vec4_t cursorBG = { 0.0f, 0.0f, 0.0f, 0.7f };
/**
 * @brief Draws the 3D-cursor in battlemode and the icons/info next to it.
 */
static void SCR_DrawCursor (void)
{
	int icon_offset_x = 16;	/* Offset of the first icon on the x-axis. */
	int icon_offset_y = 16;	/* Offset of the first icon on the y-axis. */
	int icon_spacing = 2;	/* the space between different icons. */

	if (!cursor->value)
		return;

	if (cursor->modified) {
		cursor->modified = qfalse;
		SCR_TouchPics();
	}

	if (!cursor_pic[0])
		return;

	if (mouseSpace != MS_DRAG) {
		if (cls.state == ca_active && cls.team != cl.actTeam)
			re.DrawNormPic(mx, my, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "wait");
		else
			re.DrawNormPic(mx, my, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, cursor_pic);

		if (cls.state == ca_active && mouseSpace == MS_WORLD) {
			if (selActor) {
				/* Display 'crouch' icon if actor is crouched. */
				if (selActor->state & STATE_CROUCHED)
					re.DrawNormPic(mx + icon_offset_x, my + icon_offset_y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "ducked");
				icon_offset_y += 16;	/* Height of 'crouched' icon. */
				icon_offset_y += icon_spacing;

				/* Display 'Reaction shot' icon if actor has it activated. */
				if (selActor->state & STATE_REACTION_ONCE)
					re.DrawNormPic(mx + icon_offset_x, my + icon_offset_y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "reactionfire");
				else if (selActor->state & STATE_REACTION_MANY)
					re.DrawNormPic(mx + icon_offset_x, my + icon_offset_y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "reactionfiremany");
				icon_offset_y += 16;	/* Height of 'reaction fire' icon. ... just in case we add further icons below.*/
				icon_offset_y += icon_spacing;

				/* Display weaponmode (text) here. */
				if (menuText[TEXT_MOUSECURSOR_RIGHT] && cl_show_cursor_tooltips->value)
					SCR_DrawString(mx + icon_offset_x,my - 16, menuText[TEXT_MOUSECURSOR_RIGHT], qfalse);
			}

			/* playernames */
			if (menuText[TEXT_MOUSECURSOR_PLAYERNAMES] && cl_show_cursor_tooltips->value) {
				/*@todo: activate this:
				re.DrawFill(mx + icon_offset_x - 1, my - 33, 20, 128, 0, cursorBG);
				re.DrawColor(NULL);
				*/
				SCR_DrawString(mx + icon_offset_x, my - 32, menuText[TEXT_MOUSECURSOR_PLAYERNAMES], qfalse);
				menuText[TEXT_MOUSECURSOR_PLAYERNAMES] = NULL;
			}
		}
	} else {
		vec3_t scale = { 3.5, 3.5, 3.5 };
		vec3_t org;
		vec4_t color;

		org[0] = mx;
		org[1] = my;
		org[2] = -50;
		color[0] = color[1] = color[2] = 0.5;
		color[3] = 1.0;
		MN_DrawItem(org, dragItem, 0, 0, 0, 0, scale, color);
	}
}


/**
 * @brief Scroll it up or down
 */
void SCR_RunConsole (void)
{
	/* decide on the height of the console */
	if (cls.key_dest == key_console)
		scr_conlines = scr_consize->value;	/* half screen */
	else
		scr_conlines = 0;		/* none visible */

	if (scr_conlines < scr_con_current) {
		scr_con_current -= scr_conspeed->value * cls.frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed->value * cls.frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

}

/**
 * @brief
 * @sa SCR_UpdateScreen
 * @sa Con_DrawConsole
 * @sa Con_DrawNotify
 */
static void SCR_DrawConsole (void)
{
	Con_CheckResize();

	if (!scr_vrect.width || !scr_vrect.height) {
		/* active full screen menu */
		/* draw the console like in game */
		if (scr_con_current)
			Con_DrawConsole(scr_con_current);
		/* allow chat in waiting dialoges */
		if (cls.key_dest == key_message)
			Con_DrawNotify(); /* only draw notify in game */
		return;
	}

#if 0
	if (cls.state == ca_connecting || cls.state == ca_connected) {	/* forced full screen console */
		Con_DrawConsole(1.0);
		return;
	}
#endif

	if (scr_con_current) {
		Con_DrawConsole(scr_con_current);
	} else {
		if ((cls.key_dest == key_game || cls.key_dest == key_message) && cls.state != ca_sequence)
			Con_DrawNotify(); /* only draw notify in game */
	}
}

/**
 * @brief
 * @sa SCR_UpdateScreen
 * @sa SCR_EndLoadingPlaque
 * @sa SCR_DrawLoading
 */
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds();
	cl.sound_prepped = qfalse;	/* don't play ambients */
	CDAudio_Stop();
	if (developer->value)
		return;

	scr_draw_loading = 1;

	SCR_UpdateScreen();
	cls.disable_screen = Sys_Milliseconds();
	cls.disable_servercount = cl.servercount;
}

/**
 * @brief
 * @sa SCR_BeginLoadingPlaque
 */
void SCR_EndLoadingPlaque (void)
{
	cls.disable_screen = 0;
	scr_draw_loading = 0;
	/* clear any lines of console text */
	Con_ClearNotify();
}

/**
 * @brief
 * @sa SCR_BeginLoadingPlaque
 * @sa SCR_EndLoadingPlaque
 */
static void SCR_Loading_f (void)
{
	SCR_BeginLoadingPlaque();
}

/**
 * @brief
 */
static void SCR_TimeRefresh_f (void)
{
	int i;
	int start, stop;
	float time;

	if (cls.state != ca_active)
		return;

	start = Sys_Milliseconds();

	if (Cmd_Argc() == 2) {		/* run without page flipping */
		re.BeginFrame(0);
		for (i = 0; i < 128; i++) {
			cl.refdef.viewangles[1] = i / 128.0 * 360.0;
			re.RenderFrame(&cl.refdef);
		}
		re.EndFrame();
	} else {
		for (i = 0; i < 128; i++) {
			cl.refdef.viewangles[1] = i / 128.0 * 360.0;

			re.BeginFrame(0);
			re.RenderFrame(&cl.refdef);
			re.EndFrame();
		}
	}

	stop = Sys_Milliseconds();
	time = (stop - start) / 1000.0;
	Com_Printf("%f seconds (%f fps)\n", time, 128 / time);
}

/**
 * @brief Allows rendering code to cache all needed sbar graphics
 */
void SCR_TouchPics (void)
{
	if (cursor->integer) {
		if (cursor->integer > 9 || cursor->integer < 0)
			Cvar_SetValue("cursor", 1);

		re.RegisterPic("wait");
		re.RegisterPic("ducked");
		Com_sprintf(cursor_pic, sizeof(cursor_pic), "cursor%i", cursor->integer);
		if (!re.RegisterPic(cursor_pic)) {
			Com_Printf("SCR_TouchPics: Could not register cursor: %s\n", cursor_pic);
			cursor_pic[0] = 0;
		}
	}
}

/**
 * @brief
 * @sa Font_DrawString
 */
static void SCR_DrawString (int x, int y, const char *string, qboolean bitmapFont)
{
	if (!string || !*string)
		return;

	if (bitmapFont) {
		while (*string) {
			re.DrawChar(x, y, *string);
			x += con_fontWidth->integer;
			string++;
		}
	} else
		re.FontDrawString("f_verysmall", ALIGN_UL, x, y, 0, 0, VID_NORM_WIDTH, VID_NORM_HEIGHT, 12, string, 0, 0, NULL, qfalse);
}

/**
 * @brief This is called every frame, and can also be called explicitly to flush text to the screen
 * @sa MN_DrawMenus
 * @sa V_RenderView
 * @sa SCR_DrawConsole
 * @sa SCR_DrawCursor
 */
void SCR_UpdateScreen (void)
{
	int numframes;
	int i;
	float separation[2] = { 0, 0 };

	/* if the screen is disabled (loading plaque is up, or vid mode changing) */
	/* do nothing at all */
	if (cls.disable_screen) {
		if (Sys_Milliseconds() - cls.disable_screen > 120000
			&& cl.refresh_prepped) {
			cls.disable_screen = 0;
			Com_Printf("Loading plaque timed out.\n");
			return;
		}
	}

	/* not initialized yet */
	if (!scr_initialized || !con.initialized)
		return;

	/*
	 ** range check cl_camera_separation so we don't inadvertently fry someone's
	 ** brain
	 */
	if (cl_stereo_separation->integer > 1.0)
		Cvar_SetValue("cl_stereo_separation", 1.0);
	else if (cl_stereo_separation->integer < 0)
		Cvar_SetValue("cl_stereo_separation", 0.0);

	if (cl_stereo->integer) {
		numframes = 2;
		separation[0] = -cl_stereo_separation->value / 2;
		separation[1] = cl_stereo_separation->value / 2;
	} else {
		separation[0] = 0;
		separation[1] = 0;
		numframes = 1;
	}

	for (i = 0; i < numframes; i++) {
		re.BeginFrame(separation[i]);

		if (cls.state == ca_disconnected && !scr_draw_loading)
			SCR_EndLoadingPlaque();

		MN_SetViewRect(MN_ActiveMenu());

		/* draw scene */
		V_RenderView(separation[i]);

		/* draw the menus on top of the render view (for hud and so on) */
		MN_DrawMenus();

		SCR_DrawNet();
		SCR_CheckDrawCenterString();

		if (cl_fps->integer)
			SCR_DrawString(viddef.width - con_fontWidth->integer * 10, 4, va("fps: %3.1f", cls.framerate), qtrue);
		if (cls.state == ca_active && scr_rspeed->integer)
			SCR_DrawString(viddef.width - con_fontWidth->integer * 30, 20, va("brushes: %6i alias: %6i\n", cl.refdef.c_brush_polys, cl.refdef.c_alias_polys), qtrue);

		if (scr_timegraph->integer)
			SCR_DebugGraph(cls.frametime * 300, 0);

		if (scr_debuggraph->integer || scr_timegraph->integer || scr_netgraph->integer)
			SCR_DrawDebugGraph();

		SCR_DrawConsole();

		if (cls.state != ca_sequence)
			SCR_DrawCursor();

		SCR_DrawLoading();
	}

	re.EndFrame();
}
