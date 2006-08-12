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
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

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

float scr_con_current;			/* aproaches scr_conlines at scr_conspeed */
float scr_conlines;				/* 0.0 to 1.0 lines of console to display */

qboolean scr_initialized;		/* ready to draw */

int scr_draw_loading;

vrect_t scr_vrect;				/* position of render window on screen */

cvar_t *scr_viewsize;
cvar_t *scr_conspeed;
cvar_t *scr_consize;
cvar_t *scr_centertime;
cvar_t *scr_showpause;

cvar_t *scr_netgraph;
cvar_t *scr_timegraph;
cvar_t *scr_debuggraph;
cvar_t *scr_graphheight;
cvar_t *scr_graphscale;
cvar_t *scr_graphshift;
cvar_t *scr_drawall;

typedef struct {
	int x1, y1, x2, y2;
} dirty_t;

dirty_t scr_dirty, scr_old_dirty[2];

char cursor_pic[MAX_QPATH];

void SCR_TimeRefresh_f(void);
void SCR_Loading_f(void);


/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
==============
CL_AddNetgraph

A new packet was just parsed
==============
*/
void CL_AddNetgraph(void)
{
	int i;
	int in;
	int ping;

	/* if using the debuggraph for something else, don't */
	/* add the net lines */
	if (scr_debuggraph->value || scr_timegraph->value)
		return;

	for (i = 0; i < cls.netchan.dropped; i++)
		SCR_DebugGraph(30, 0x40);

	for (i = 0; i < cl.surpressCount; i++)
		SCR_DebugGraph(30, 0xdf);

	/* see what the latency was on this packet */
	in = cls.netchan.incoming_acknowledged & (CMD_BACKUP - 1);
/*	ping = cls.realtime - cl.cmd_time[in]; */
	ping = 100;
	ping /= 30;
	if (ping > 30)
		ping = 30;
	SCR_DebugGraph(ping, 0xd0);
}


typedef struct {
	float value;
	int color;
} graphsamp_t;

static int current;
static graphsamp_t values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph(float value, int color)
{
	values[current & 1023].value = value;
	values[current & 1023].color = color;
	current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph(void)
{
	int a, x, y, w, i, h;
	float v;
	vec4_t color = { 0.0, 0.0, 0.0, 1.0 };

	/* draw the graph */
	w = scr_vrect.width;

	x = scr_vrect.x;
	y = scr_vrect.y + scr_vrect.height;
	re.DrawFill(x, y - scr_graphheight->value, w, scr_graphheight->value, 0, color);

	for (a = 0; a < w; a++) {
		i = (current - 1 - a + 1024) & 1023;
		v = values[i].value;
		color[0] = color[1] = color[2] = values[i].color;
		v = v * scr_graphscale->value + scr_graphshift->value;

		if (v < 0)
			v += scr_graphheight->value * (1 + (int) (-v / scr_graphheight->value));
		h = (int) v % (int) scr_graphheight->value;
		re.DrawFill(x + w - 1 - a, y - h, 1, h, 0, color);
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

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint(char *str)
{
	char *s;
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

	/* echo it to the console */
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");

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
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_ClearNotify();
}


void SCR_DrawCenterString(void)
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
		SCR_AddDirtyPoint(x, y);
		for (j = 0; j < l; j++, x += 8) {
			re.DrawChar(x, y, start[j]);
			if (!remaining--)
				return;
		}
		SCR_AddDirtyPoint(x, y + 8);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;				/* skip the \n */
	} while (1);
}

void SCR_CheckDrawCenterString(void)
{
	scr_centertime_off -= cls.frametime;

	if (scr_centertime_off <= 0)
		return;

	SCR_DrawCenterString();
}

/*============================================================================= */

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f(void)
{
	Cvar_SetValue("viewsize", scr_viewsize->value + 10);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f(void)
{
	Cvar_SetValue("viewsize", scr_viewsize->value - 10);
}

/*============================================================================ */

/*
==================
SCR_Init
==================
*/
void SCR_Init(void)
{
	scr_viewsize = Cvar_Get("viewsize", "100", CVAR_ARCHIVE);
	scr_conspeed = Cvar_Get("scr_conspeed", "3", 0);
	scr_consize = Cvar_Get("scr_consize", "1.0", 0);
	scr_showpause = Cvar_Get("scr_showpause", "1", 0);
	scr_centertime = Cvar_Get("scr_centertime", "2.5", 0);
	scr_netgraph = Cvar_Get("netgraph", "0", 0);
	scr_timegraph = Cvar_Get("timegraph", "0", 0);
	scr_debuggraph = Cvar_Get("debuggraph", "0", 0);
	scr_graphheight = Cvar_Get("graphheight", "32", 0);
	scr_graphscale = Cvar_Get("graphscale", "1", 0);
	scr_graphshift = Cvar_Get("graphshift", "0", 0);
	scr_drawall = Cvar_Get("scr_drawall", "0", 0);

	/* register our commands */
	Cmd_AddCommand("timerefresh", SCR_TimeRefresh_f);
	Cmd_AddCommand("loading", SCR_Loading_f);
	Cmd_AddCommand("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand("sizedown", SCR_SizeDown_f);

	SCR_TouchPics();

	scr_initialized = qtrue;
}


/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet(void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP - 1)
		return;

	re.DrawPic(scr_vrect.x + 64, scr_vrect.y, "net");
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause(void)
{
	int w, h;

	/* turn off for screenshots */
	if (!scr_showpause->value)
		return;

	if (!cl_paused->value)
		return;

	re.DrawGetPicSize(&w, &h, "pause");
	re.DrawPic((viddef.width - w) / 2, viddef.height / 2 + 8, "pause");
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading(void)
{
	int w, h;

	if (!scr_draw_loading)
		return;

	scr_draw_loading = qfalse;
	re.DrawGetPicSize(&w, &h, "loading");
	re.DrawPic((viddef.width - w) / 2, (viddef.height - h) / 2, "loading");
}


/*
=================
SCR_DrawCursor
=================
*/
void SCR_DrawCursor(void)
{
	if (!cursor->value)
		return;

	if (cursor->modified) {
		cursor->modified = qfalse;
		SCR_TouchPics();
	}

	if (!cursor_pic[0])
		return;

	if (mouseSpace != MS_DRAG) {
		re.DrawNormPic(mx, my, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, cursor_pic);

		if (cls.state == ca_active && mouseSpace == MS_WORLD) {
			if (cls.team != cl.actTeam)
				re.DrawNormPic(mx + 16, my + 16, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "wait");
			else if (selActor && selActor->state & STATE_CROUCHED)
				re.DrawNormPic(mx + 16, my + 16, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "ducked");
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


/*============================================================================= */

/*
==================
SCR_RunConsole

Scroll it up or down
==================
*/
void SCR_RunConsole(void)
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
void SCR_DrawConsole(void)
{
	Con_CheckResize();

	if (!scr_vrect.width || !scr_vrect.height) {
		/* active full screen menu */
		/* draw the console like in game */
		if (scr_con_current)
			Con_DrawConsole(scr_con_current);
		return;
	}

	if (cls.state == ca_connecting || cls.state == ca_connected) {	/* forced full screen console */
		Con_DrawConsole(1.0);
		return;
	}

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
 */
void SCR_BeginLoadingPlaque(void)
{
	S_StopAllSounds();
	cl.sound_prepped = qfalse;	/* don't play ambients */
	CDAudio_Stop();
	if (developer->value)
		return;
	if (cls.state == ca_disconnected)
		return;					/* if at console, don't bring up the plaque */
	if (cls.key_dest == key_console)
		return;

	scr_draw_loading = 2;		/* clear to black first */

	SCR_UpdateScreen();
	cls.disable_screen = Sys_Milliseconds();
	cls.disable_servercount = cl.servercount;
}

/**
 * @brief
 * @sa SCR_BeginLoadingPlaque
 */
void SCR_EndLoadingPlaque(void)
{
	cls.disable_screen = 0;
	Con_ClearNotify();
}

/**
 * @brief
 * @sa SCR_BeginLoadingPlaque
 * @sa SCR_EndLoadingPlaque
 */
void SCR_Loading_f(void)
{
	SCR_BeginLoadingPlaque();
}

/**
 * @brief
 */
void SCR_TimeRefresh_f(void)
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
 * @brief
 * @sa SCR_DirtyScreen
 */
void SCR_AddDirtyPoint(int x, int y)
{
	if (x < scr_dirty.x1)
		scr_dirty.x1 = x;
	if (x > scr_dirty.x2)
		scr_dirty.x2 = x;
	if (y < scr_dirty.y1)
		scr_dirty.y1 = y;
	if (y > scr_dirty.y2)
		scr_dirty.y2 = y;
}

/**
 * @brief
 * @sa SCR_AddDirtyPoint
 */
void SCR_DirtyScreen(void)
{
	SCR_AddDirtyPoint(0, 0);
	SCR_AddDirtyPoint(viddef.width - 1, viddef.height - 1);
}

/**
 * @brief Allows rendering code to cache all needed sbar graphics
 */
void SCR_TouchPics(void)
{
	if (cursor->value) {
		if (cursor->value > 9 || cursor->value < 0)
			cursor->value = 1;

		re.RegisterPic("wait");
		re.RegisterPic("ducked");
		Com_sprintf(cursor_pic, sizeof(cursor_pic), "cursor%i", (int) (cursor->value));
		if (!re.RegisterPic(cursor_pic))
			cursor_pic[0] = 0;
	}
}

/**
 * @brief
 */
void SCR_DrawString(int x, int y, char *string)
{
	while (*string) {
		re.DrawChar(x, y, *string);
		x += 8;
		string++;
	}
}

/**
 * @brief This is called every frame, and can also be called explicitly to flush text to the screen
 * @sa MN_DrawMenus
 * @sa V_RenderView
 * @sa SCR_DrawConsole
 * @sa SCR_DrawCursor
 */
void SCR_UpdateScreen(void)
{
	int numframes;
	int i;
	float separation[2] = { 0, 0 };

	/* if the screen is disabled (loading plaque is up, or vid mode changing) */
	/* do nothing at all */
	if (cls.disable_screen) {
		if (Sys_Milliseconds() - cls.disable_screen > 120000) {
			cls.disable_screen = 0;
			Com_Printf("Loading plaque timed out.\n");
		}
		return;
	}

	/* not initialized yet */
	if (!scr_initialized || !con.initialized)
		return;

	/*
	 ** range check cl_camera_separation so we don't inadvertently fry someone's
	 ** brain
	 */
	if (cl_stereo_separation->value > 1.0)
		Cvar_SetValue("cl_stereo_separation", 1.0);
	else if (cl_stereo_separation->value < 0)
		Cvar_SetValue("cl_stereo_separation", 0.0);

	if (cl_stereo->value) {
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

		if (scr_draw_loading) {
			SCR_DrawLoading();
			continue;
		} else {
			MN_SetViewRect(MN_ActiveMenu());

			/* draw scene */
			V_RenderView(separation[i]);

			/* draw the menus on top of the render view (for hud and so on) */
			MN_DrawMenus();

			SCR_DrawNet();
			SCR_CheckDrawCenterString();

			if (cl_fps->value)
				SCR_DrawString(viddef.width - 80, 4, va("fps: %3.1f", cls.framerate));

			if (scr_timegraph->value)
				SCR_DebugGraph(cls.frametime * 300, 0);

			if (scr_debuggraph->value || scr_timegraph->value || scr_netgraph->value)
				SCR_DrawDebugGraph();

			SCR_DrawPause();

			SCR_DrawConsole();

			if (cls.state != ca_sequence)
				SCR_DrawCursor();
		}
	}

	re.EndFrame();
}
