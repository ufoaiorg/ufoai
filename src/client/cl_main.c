/*
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
// cl_main.c  -- client main loop

#include "client.h"

cvar_t	*freelook;

cvar_t	*adr0;
cvar_t	*adr1;
cvar_t	*adr2;
cvar_t	*adr3;
cvar_t	*adr4;
cvar_t	*adr5;
cvar_t	*adr6;
cvar_t	*adr7;
cvar_t	*adr8;

cvar_t	*cl_stereo_separation;
cvar_t	*cl_stereo;

cvar_t	*rcon_client_password;
cvar_t	*rcon_address;

cvar_t	*cl_noskins;
cvar_t	*cl_autoskins;
cvar_t	*cl_footsteps;
cvar_t	*cl_timeout;
cvar_t	*cl_predict;
//cvar_t	*cl_minfps;
cvar_t	*cl_maxfps;
cvar_t	*cl_gun;
cvar_t	*cl_markactors;

cvar_t	*cl_add_particles;
cvar_t	*cl_add_lights;
cvar_t	*cl_add_entities;
cvar_t	*cl_add_blend;

cvar_t	*cl_fps;
cvar_t	*cl_shownet;
cvar_t	*cl_showmiss;
cvar_t	*cl_showclamp;
cvar_t	*cl_show_tooltips;

cvar_t	*cl_paused;
cvar_t	*cl_timedemo;

cvar_t	*cl_aviFrameRate;
cvar_t	*cl_aviMotionJpeg;

cvar_t	*lookspring;
cvar_t	*lookstrafe;
cvar_t	*sensitivity;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*cl_logevents;

cvar_t	*cl_centerview;

cvar_t	*cl_worldlevel;
cvar_t	*cl_selected;

cvar_t	*cl_lightlevel;

cvar_t	*cl_numnames;

cvar_t	*mn_main;
cvar_t	*mn_sequence;
cvar_t	*mn_active;
cvar_t	*mn_hud;
cvar_t	*mn_lastsave;

cvar_t	*difficulty;
cvar_t	*confirm_actions;

cvar_t	*cl_precachemenus;

//
// userinfo
//
cvar_t	*info_password;
cvar_t	*info_spectator;
cvar_t	*name;
cvar_t	*team;
cvar_t	*equip;
cvar_t	*teamnum;
cvar_t	*campaign;
cvar_t	*rate;
cvar_t	*msg;

cvar_t	*cl_vwep;

#ifndef _WIN32
cvar_t *soundsystem;
#endif

client_static_t	cls;
client_state_t	cl;

centity_t cl_entities[MAX_EDICTS];

//======================================================================

/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer (void)
{
	char	*cmd;

	cmd = Cmd_Argv(0);
	if (cls.state <= ca_connected || *cmd == '-' || *cmd == '+')
	{
		Com_Printf ("Unknown command \"%s\"\n", cmd);
		return;
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, cmd);
	if (Cmd_Argc() > 1)
	{
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}

void CL_Setenv_f( void )
{
	int argc = Cmd_Argc();

	if ( argc > 2 )
	{
		char buffer[1000];
		int i;

		Q_strncpyz( buffer, Cmd_Argv(1), sizeof(buffer) );
		Q_strcat( buffer, sizeof(buffer), "=" );

		for ( i = 2; i < argc; i++ )
		{
			Q_strcat( buffer, sizeof(buffer), Cmd_Argv( i ) );
			Q_strcat( buffer, sizeof(buffer), " " );
		}

		putenv( buffer );
	}
	else if ( argc == 2 )
	{
		char *env = getenv( Cmd_Argv(1) );

		if ( env )
		{
			Com_Printf( "%s=%s\n", Cmd_Argv(1), env );
		}
		else
		{
			Com_Printf( "%s undefined\n", Cmd_Argv(1), env );
		}
	}
}


/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f (void)
{
	if (cls.state != ca_connected && cls.state != ca_active)
	{
		Com_Printf ( "Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	// don't forward the first argument
	if (Cmd_Argc() > 1)
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}


/*
==================
CL_Pause_f
==================
*/
void CL_Pause_f (void)
{
	// never pause in multiplayer
	if ( !ccs.singleplayer || !Com_ServerState () )
	{
		Cvar_SetValue ("paused", 0);
		return;
	}

	Cvar_SetValue ("paused", !cl_paused->value);
}

/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
{
	CL_Disconnect ();
	Com_Quit ();
}

/*
================
CL_Drop

Called after an ERR_DROP was thrown
================
*/

void CL_Drop (void)
{
	if (cls.state == ca_uninitialized)
		return;
	if (cls.state == ca_disconnected)
		return;

	CL_Disconnect ();

	// drop loading plaque unless this is the initial game start
	if (cls.disable_servercount != -1)
		SCR_EndLoadingPlaque ();	// get rid of loading plaque
}


/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
void CL_SendConnectPacket (void)
{
	netadr_t	adr;
	int		port;

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ( "Bad server address: %s\n", cls.servername );
		cls.connect_time = 0;
		return;
	}
	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	port = Cvar_VariableValue ("qport");
	userinfo_modified = qfalse;

	Netchan_OutOfBandPrint (NS_CLIENT, adr, "connect %i %i %i \"%s\"\n",
		PROTOCOL_VERSION, port, cls.challenge, Cvar_Userinfo() );
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend (void)
{
	netadr_t	adr;

	// if the local server is running and we aren't
	// then connect
	if (cls.state == ca_disconnected && Com_ServerState() )
	{
		cls.state = ca_connecting;
		Q_strncpyz(cls.servername, "localhost", sizeof(cls.servername));
		// we don't need a challenge on the localhost
		CL_SendConnectPacket ();
		return;
//		cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
	}

	// resend if we haven't gotten a reply yet
	if (cls.state != ca_connecting)
		return;

	if (cls.realtime - cls.connect_time < 3000)
		return;

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ("Bad server address: %s\n", cls.servername);
		cls.state = ca_disconnected;
		return;
	}
	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	cls.connect_time = cls.realtime;	// for retransmit requests

	Com_Printf ("Connecting to %s...\n", cls.servername);

	Netchan_OutOfBandPrint (NS_CLIENT, adr, "getchallenge\n");
}


/*
================
CL_Connect_f

FIXME: Spectator needs no team
================
*/
void CL_Connect_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: connect <server>\n");
		return;
	}

	if ( ! B_GetNumOnTeam() )
	{
		MN_Popup( _("Error"), _("Assemble a team first") );
		return;
	}

	if (Com_ServerState ())
	{	// if running a local server, kill it and reissue
		SV_Shutdown ( "Server quit\n", qfalse);
	}
	else
	{
		CL_Disconnect ();
	}

	server = Cmd_Argv (1);

	NET_Config (qtrue); // allow remote

	CL_Disconnect ();

	cls.state = ca_connecting;
	Q_strncpyz(cls.servername, server, sizeof(cls.servername));
	cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
}


/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f (void)
{
	char	message[1024];
	int		i;
	netadr_t	to;

	if (!rcon_client_password->string)
	{
		Com_Printf ("You must set 'rcon_password' before issuing an rcon command.\n");
		return;
	}

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;

	NET_Config (qtrue); // allow remote

	Q_strcat (message, sizeof(message), "rcon ");

	Q_strcat (message, sizeof(message), rcon_client_password->string);
	Q_strcat (message, sizeof(message), " ");

	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		Q_strcat (message, sizeof(message), Cmd_Argv(i));
		Q_strcat (message, sizeof(message), " ");
	}

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else
	{
		if (!strlen(rcon_address->string))
		{
			Com_Printf ( "You must either be connected, or set the 'rcon_address' cvar\nto issue rcon commands\n" );
			return;
		}
		NET_StringToAdr (rcon_address->string, &to);
		if (to.port == 0)
			to.port = BigShort (PORT_SERVER);
	}

	NET_SendPacket (NS_CLIENT, strlen(message)+1, message, to);
}


/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	S_StopAllSounds ();
	CL_ClearEffects ();
//	CL_ClearTEnts ();
	CL_InitEvents ();

	// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));
	memset (&cl_entities, 0, sizeof(cl_entities));

	numLEs = 0;
	numLMs = 0;
	numMPs = 0;
	numPtls = 0;

	SZ_Clear (&cls.netchan.message);
}

/*
=====================
CL_Disconnect

Goes from a connected state to full screen console state
Sends a disconnect message to the server
This is also called on Com_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
	byte	final[32];

	if (cls.state == ca_disconnected)
		return;

	if (cl_timedemo && cl_timedemo->value)
	{
		int	time;

		time = Sys_Milliseconds () - cl.timedemo_start;
		if (time > 0)
			Com_Printf ("%i frames, %3.1f seconds: %3.1f fps\n", cl.timedemo_frames,
			time/1000.0, cl.timedemo_frames*1000.0 / time);
	}

	VectorClear (cl.refdef.blend);

	// go to main menu and bring up console
	MN_PopMenu( qtrue );
	MN_PushMenu( mn_main->string );
// 	if ( ! ccs.singleplayer )
// 		cls.key_dest = key_console;

	cls.connect_time = 0;

	// send a disconnect message to the server
	final[0] = clc_stringcmd;
	Q_strncpyz((char *)final+1, "disconnect", sizeof(final)-1);
	Netchan_Transmit (&cls.netchan, strlen((char*)final), final);
	Netchan_Transmit (&cls.netchan, strlen((char*)final), final);
	Netchan_Transmit (&cls.netchan, strlen((char*)final), final);

	CL_ClearState ();

	// Stop recording any video
	if ( CL_VideoRecording() )
		CL_CloseAVI();

	cls.state = ca_disconnected;
}

void CL_Disconnect_f (void)
{
	Com_Drop();
}


/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
void CL_Packet_f (void)
{
	char	send[2048];

	int		i, l;
	char	*in, *out;
	netadr_t	adr;

	if (Cmd_Argc() != 3)
	{
		Com_Printf ("Usage: packet <destination> <contents>\n");
		return;
	}

	NET_Config (qtrue);		// allow remote

	if (!NET_StringToAdr (Cmd_Argv(1), &adr))
	{
		Com_Printf ("Bad address\n");
		return;
	}
	if (!adr.port)
		adr.port = BigShort (PORT_SERVER);

	in = Cmd_Argv(2);
	out = send+4;
	send[0] = send[1] = send[2] = send[3] = (char)0xff;

	l = strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket (NS_CLIENT, out-send, send, adr);
}

/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void CL_Changing_f (void)
{
	SCR_BeginLoadingPlaque ();
	cls.state = ca_connected;	// not active anymore, but not disconnected
	Com_Printf ("\nChanging map...\n");
}


/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f (void)
{
	S_StopAllSounds ();
	if (cls.state == ca_connected) {
		Com_Printf ("reconnecting...\n");
		cls.state = ca_connected;
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		return;
	}

	if (*cls.servername) {
		if (cls.state >= ca_connected) {
			CL_Disconnect();
			cls.connect_time = cls.realtime - 1500;
		} else
			cls.connect_time = -99999; // fire immediately

		cls.state = ca_connecting;
		Com_Printf ("reconnecting...\n");
	}
}

/*
=================
CL_ParseStatusMessage

Handle a reply from a ping
=================
*/
#define MAX_SERVERLIST 32

char		serverText[1024];
int			serverListLength;
netadr_t	serverList[MAX_SERVERLIST];

void CL_ParseStatusMessage (void)
{
	char *s = MSG_ReadString(&net_message);

	Com_DPrintf ("CL_ParseStatusMessage: %s\n", s);

	if ( serverListLength >= MAX_SERVERLIST )
		return;

	serverList[serverListLength++] = net_from;
	Q_strcat( serverText, sizeof(serverText), s );
	menuText[TEXT_LIST] = serverText;
}

char serverInfoText[MAX_MESSAGE_TEXT];
void CL_ParseServerInfoMessage ( void )
{
	char *s = MSG_ReadString (&net_message);
	char *var = NULL;
	char *value = NULL;

	if ( ! s )
		return;

	var = strstr(s, "\n");
	*var = '\0';
	Com_DPrintf ("%s\n", s);
	Cvar_Set("mn_mappic", "maps/shots/na.jpg" );

	Com_sprintf( serverInfoText, MAX_MESSAGE_TEXT, _("IP\t%s\n\n"), NET_AdrToString (net_from) );

	s++; // first char is slash
	do
	{
		// var
		var = s;
		s = strstr(s, "\\");
		*s++ = '\0';

		// value
		value = s;
		s = strstr(s, "\\");
		// last?
		if ( s )
			*s++ = '\0';

		if ( !Q_strncmp(var, "mapname", 7 ) )
		{
			if ( FS_CheckFile( va("pics/maps/shots/%s.jpg", value) ) != -1 )
				Cvar_Set("mn_mappic", va("maps/shots/%s.jpg", value) );

			Cvar_ForceSet("mapname", value );
			Q_strcat( serverInfoText, MAX_MESSAGE_TEXT, va(_("Map:\t%s\n"), value) );
		}
		else if ( !Q_strncmp(var, "version", 7 ) )
			Q_strcat( serverInfoText, MAX_MESSAGE_TEXT, va(_("Version:\t%s\n"), value) );
		else if ( !Q_strncmp(var, "hostname", 8 ) )
			Q_strcat( serverInfoText, MAX_MESSAGE_TEXT, va(_("Servername:\t%s\n"), value) );
		else if ( !Q_strncmp(var, "sv_enablemorale", 15 ) )
			Q_strcat( serverInfoText, MAX_MESSAGE_TEXT, va(_("Moralestates:\t%s\n"), value) );
		else if ( !Q_strncmp(var, "sv_teamplay", 11 ) )
		{

			Q_strcat( serverInfoText, MAX_MESSAGE_TEXT, va(_("Teamplay:\t%s\n"), value) );
		}
		else if ( !Q_strncmp(var, "maxplayers", 10 ) )
			Q_strcat( serverInfoText, MAX_MESSAGE_TEXT, va(_("Max. players per team:\t%s\n"), value) );
		else if ( !Q_strncmp(var, "maxclients", 10 ) )
			Q_strcat( serverInfoText, MAX_MESSAGE_TEXT, va(_("Max. clients:\t%s\n"), value) );
		else if ( !Q_strncmp(var, "maxsoldiersperplayer", 20 ) )
			Q_strcat( serverInfoText, MAX_MESSAGE_TEXT, va(_("Max. soldiers per player:\t%s\n"), value) );
		else if ( !Q_strncmp(var, "maxsoldiers", 11 ) )
			Q_strcat( serverInfoText, MAX_MESSAGE_TEXT, va(_("Max. soldiers per team:\t%s\n"), value) );
#ifdef DEBUG
		else
			Q_strcat( serverInfoText, MAX_MESSAGE_TEXT, va("%s\t%s\n", var, value) );
#endif
	} while ( s != NULL );
	menuText[TEXT_STANDARD] = serverInfoText;
	MN_PushMenu("serverinfo");
}

/*=================
CL_ServerConnect_f

called via server_connect

FIXME: Spectator needs no team
=================*/
void CL_ServerConnect_f( void )
{
	char* ip = Cvar_VariableString("mn_server_ip");

	if ( ! B_GetNumOnTeam() )
	{
		MN_Popup( _("Error"), _("Assemble a team first") );
		return;
	}

	if ( ip )
	{
		Com_DPrintf("CL_ServerConnect_f: connect to %s\n", ip);
		Cbuf_AddText( va( "connect %s\n", ip ) );
	}
}

/*=================
CL_BookmarkPrint_f
=================*/
char bookmarkText[MAX_MESSAGE_TEXT];
void CL_BookmarkPrint_f ( void )
{
	int i;

	// clear list;
	bookmarkText[0] = '\0';

	for ( i = 0; i < 16; i++ )
	{
		Q_strcat( bookmarkText, MAX_MESSAGE_TEXT, va( "%s\n", Cvar_VariableString( va("adr%i", i) ) ) );
	}
	menuText[TEXT_LIST] = bookmarkText;
}

/*=================
CL_BookmarkAdd_f
=================*/
void CL_BookmarkAdd_f ( void )
{
	int	i;
	char	*bookmark = NULL;
	char	*newBookmark = NULL;
	netadr_t	adr;

	if ( Cmd_Argc() < 2 )
	{
		newBookmark = Cvar_VariableString("mn_server_ip");
		if ( ! newBookmark )
		{
			Com_Printf( "usage: bookmark_add <ip>\n" );
			return;
		}
	}
	else
		newBookmark = Cmd_Argv( 1 );

	if (!NET_StringToAdr (newBookmark, &adr))
	{
		Com_Printf("CL_BookmarkAdd_f: Invalid address %s\n", newBookmark );
		return;
	}

	for ( i = 0; i < 16; i++ )
	{
		bookmark = Cvar_VariableString( va("adr%i", i) );
		if ( !bookmark || !NET_StringToAdr (bookmark, &adr) )
		{
			Cvar_Set( va("adr%i", i), newBookmark );
			return;
		}
	}
	// bookmarks are full - overwrite the first entry
	Cvar_Set( "adr0", newBookmark );
}

/*=================
CL_BookmarkListClick_f
=================*/
void CL_BookmarkListClick_f ( void )
{
	int	num;
	char	*bookmark = NULL;
	netadr_t	adr;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "usage: bookmarks_click <num>\n" );
		return;
	}
	num = atoi( Cmd_Argv( 1 ) );
	bookmark = Cvar_VariableString( va("adr%i", num) );

	if ( bookmark )
	{
		if (!NET_StringToAdr (bookmark, &adr))
		{
			Com_Printf ("Bad address: %s\n", bookmark);
			return;
		}
		if ( adr.port == 0 )
			adr.port = BigShort(PORT_SERVER);

		Cvar_Set("mn_server_ip", bookmark );
		Netchan_OutOfBandPrint (NS_CLIENT, adr, "status %i", PROTOCOL_VERSION );
	}
}

/*=================
CL_ServerListClick_f
=================*/
char serverInfoText[MAX_MESSAGE_TEXT];
void CL_ServerListClick_f (void)
{
	int num;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "usage: servers_click <num>\n" );
		return;
	}
	num = atoi( Cmd_Argv( 1 ) );

	menuText[TEXT_STANDARD] = serverInfoText;
	if ( num >= 0 && num < serverListLength )
	{
		Netchan_OutOfBandPrint (NS_CLIENT, serverList[num], "status %i", PROTOCOL_VERSION );
		Cvar_Set("mn_server_ip", NET_AdrToString( serverList[num] ) );
	}
}

/*=================
CL_PingServers_f
=================*/
void CL_PingServers_f (void)
{
	netadr_t	adr;
	cvar_t		*noudp;
	cvar_t		*noipx;

	menuText[TEXT_LIST] = NULL;

	NET_Config (qtrue);		// allow remote

	// send a broadcast packet
	Com_DPrintf ("pinging broadcast...\n");
	serverText[0] = 0;
	serverListLength = 0;

	noudp = Cvar_Get ("noudp", "0", CVAR_NOSET);
	if (!noudp->value)
	{
		adr.type = NA_BROADCAST;
		adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, adr, "info %i", PROTOCOL_VERSION );
	}

	noipx = Cvar_Get ("noipx", "0", CVAR_NOSET);
	if (!noipx->value)
	{
		adr.type = NA_BROADCAST_IPX;
		adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, adr, "info %i", PROTOCOL_VERSION );
	}
}


/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket (void)
{
	char	*s;
	char	*c;

	MSG_BeginReading (&net_message);
	MSG_ReadLong (&net_message);	// skip the -1

	s = MSG_ReadStringLine (&net_message);

	Cmd_TokenizeString (s, qfalse);

	c = Cmd_Argv(0);

	Com_Printf ("%s: %s\n", NET_AdrToString (net_from), c);

	// server connection
	if (!Q_strncmp(c, "client_connect", 13))
	{
		if (cls.state == ca_connected)
		{
			Com_Printf ("Dup connect received. Ignored.\n");
			return;
		}
		Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.quakePort);
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		cls.state = ca_connected;
		return;
	}

	// server responding to a status broadcast
	if (!Q_strncmp(c, "info", 4))
	{
		CL_ParseStatusMessage ();
		return;
	}

	// remote command from gui front end
	if (!Q_strncmp(c, "cmd", 3))
	{
		if (!NET_IsLocalAddress(net_from))
		{
			Com_Printf ("Command packet from remote host. Ignored.\n");
			return;
		}
		Sys_AppActivate ();
		s = MSG_ReadString (&net_message);
		Cbuf_AddText (s);
		Cbuf_AddText ("\n");
		return;
	}
	// print command from somewhere
	if (!Q_strncmp(c, "print", 5))
	{
		CL_ParseServerInfoMessage ();
		return;
	}

	// ping from somewhere
	if (!Q_strncmp(c, "ping", 4))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, net_from, "ack");
		return;
	}

	// challenge from the server we are connecting to
	if (!Q_strncmp(c, "challenge", 9))
	{
		cls.challenge = atoi(Cmd_Argv(1));
		CL_SendConnectPacket ();
		return;
	}

	// echo request from server
	if (!Q_strncmp(c, "echo", 4))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, net_from, "%s", Cmd_Argv(1) );
		return;
	}

	Com_Printf ("Unknown command.\n");
}


/*
=================
CL_DumpPackets

A vain attempt to help bad TCP stacks that cause problems
when they overflow
=================
*/
void CL_DumpPackets (void)
{
	while (NET_GetPacket (NS_CLIENT, &net_from, &net_message))
	{
		Com_Printf ("dumnping a packet\n");
	}
}

/*
=================
CL_ReadPackets
=================
*/
void CL_ReadPackets (void)
{
	while (NET_GetPacket (NS_CLIENT, &net_from, &net_message))
	{
//	Com_Printf ("packet\n");
		//
		// remote command packet
		//
		if (*(int *)net_message.data == -1)
		{
			CL_ConnectionlessPacket ();
			continue;
		}

		if (cls.state == ca_disconnected || cls.state == ca_connecting)
			continue;		// dump it if not connected

		if (net_message.cursize < 8)
		{
			Com_Printf ("%s: Runt packet\n",NET_AdrToString(net_from));
			continue;
		}

		//
		// packet from server
		//
		if (!NET_CompareAdr (net_from, cls.netchan.remote_address))
		{
			Com_DPrintf ("%s:sequenced packet without connection\n",NET_AdrToString(net_from));
			continue;
		}
		if (!Netchan_Process(&cls.netchan, &net_message))
			continue;		// wasn't accepted for some reason
		CL_ParseServerMessage ();
	}

	//
	// check timeout
	//
	if (cls.state >= ca_connected
	 && cls.realtime - cls.netchan.last_received > cl_timeout->value*1000)
	{
		if (++cl.timeoutcount > 5)	// timeoutcount saves debugger
		{
			Com_Printf ("\nServer connection timed out.\n");
			CL_Disconnect ();
			return;
		}
	}
	else
		cl.timeoutcount = 0;

}


//=============================================================================

/*
==============
CL_Userinfo_f
==============
*/
void CL_Userinfo_f (void)
{
	Com_Printf ("User info settings:\n");
	Info_Print (Cvar_Userinfo());
}

/*
=================
CL_Snd_Restart_f

Restart the sound subsystem so it can pick up
new parameters and flush all sounds
=================
*/
void CL_Snd_Restart_f (void)
{
	S_Shutdown ();
	S_Init ();
	CL_RegisterSounds ();
}

/*
=================
CL_Precache_f

The server will send this command right
before allowing the client into the server
=================
*/
void CL_Precache_f (void)
{
	// stop sound, back to the console
	S_StopAllSounds();
	MN_PopMenu( qtrue );

	CL_RegisterSounds();
	CL_PrepRefresh();

	// send team info
	CL_SendTeamInfo( &cls.netchan.message, baseCurrent->curTeam, B_GetNumOnTeam() );

	// send begin
	MSG_WriteByte( &cls.netchan.message, clc_stringcmd);
	MSG_WriteString( &cls.netchan.message, va("begin %i\n", atoi(Cmd_Argv(1))) );
}


/*=================
CL_ParseScriptFirst

parsed if we are no dedicated server
first stage parses all the main data into their struct
see CL_ParseScriptSecond for more details about parsing stages
=================*/
void CL_ParseScriptFirst( char *type, char *name, char **text )
{
	// check for client interpretable scripts
	if ( !Q_strncmp( type, "menu_model", 10 ) ) MN_ParseMenuModel( name, text );
	else if ( !Q_strncmp( type, "menu", 4 ) ) MN_ParseMenu( name, text );
	else if ( !Q_strncmp( type, "particle", 8 ) ) CL_ParseParticle( name, text );
	else if ( !Q_strncmp( type, "mission", 7 ) ) CL_ParseMission( name, text );
	else if ( !Q_strncmp( type, "sequence", 8 ) ) CL_ParseSequence( name, text );
	else if ( !Q_strncmp( type, "up_chapters", 11 ) ) UP_ParseUpChapters( name, text );
	else if ( !Q_strncmp( type, "building", 8 ) ) B_ParseBuildings( name, text, qfalse );
	else if ( !Q_strncmp( type, "tech", 4 ) ) RS_ParseTechnologies( name, text );
	else if ( !Q_strncmp( type, "aircraft", 8 ) ) CL_ParseAircraft( name, text );
	else if ( !Q_strncmp( type, "base", 4 ) ) B_ParseBases( name, text );
	else if ( !Q_strncmp( type, "nation", 6 ) ) CL_ParseNations( name, text );
	else if ( !Q_strncmp( type, "shader", 6 ) ) CL_ParseShaders( name, text );
	else if ( !Q_strncmp( type, "font", 4 ) ) CL_ParseFont( name, text );
	else if ( !Q_strncmp( type, "tutorial", 8 ) ) MN_ParseTutorials( name, text );
}

/*=================
CL_ParseScriptSecond

parsed if we are no dedicated server
second stage links all the parsed data from first stage
example: we need a techpointer in a building - in the second stage the buildings and the
         techs are already parsed - so now we can link them
=================*/
void CL_ParseScriptSecond( char *type, char *name, char **text )
{
	// check for client interpretable scripts
	if ( !Q_strncmp( type, "campaign", 8 ) ) CL_ParseCampaign( name, text );
	else if ( !Q_strncmp( type, "stage", 5 ) ) CL_ParseStage( name, text );
	else if ( !Q_strncmp( type, "building", 8 ) ) B_ParseBuildings( name, text, qtrue );
}


/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal (void)
{
	cls.state = ca_disconnected;
	cls.realtime = Sys_Milliseconds ();

	Con_CheckResize ();
	Com_InitInventory( invList );
	CL_InitInput();
	CL_InitMessageSystem();

	MN_ResetMenus();
	CL_ResetParticles();
	CL_ResetCampaign();
	CL_ResetMarket();
	CL_ResetSequences();
	CL_ResetTeams();

	adr0 = Cvar_Get( "adr0", "", CVAR_ARCHIVE );
	adr1 = Cvar_Get( "adr1", "", CVAR_ARCHIVE );
	adr2 = Cvar_Get( "adr2", "", CVAR_ARCHIVE );
	adr3 = Cvar_Get( "adr3", "", CVAR_ARCHIVE );
	adr4 = Cvar_Get( "adr4", "", CVAR_ARCHIVE );
	adr5 = Cvar_Get( "adr5", "", CVAR_ARCHIVE );
	adr6 = Cvar_Get( "adr6", "", CVAR_ARCHIVE );
	adr7 = Cvar_Get( "adr7", "", CVAR_ARCHIVE );
	adr8 = Cvar_Get( "adr8", "", CVAR_ARCHIVE );

	//
	// register our variables
	//
	cl_stereo_separation = Cvar_Get( "cl_stereo_separation", "0.4", CVAR_ARCHIVE );
	cl_stereo = Cvar_Get( "cl_stereo", "0", 0 );

	cl_add_blend = Cvar_Get ("cl_blend", "1", 0);
	cl_add_lights = Cvar_Get ("cl_lights", "1", 0);
	cl_add_particles = Cvar_Get ("cl_particles", "1", 0);
	cl_add_entities = Cvar_Get ("cl_entities", "1", 0);
	cl_gun = Cvar_Get ("cl_gun", "1", 0);
	cl_footsteps = Cvar_Get ("cl_footsteps", "1", 0);
	cl_noskins = Cvar_Get ("cl_noskins", "0", 0);
	cl_autoskins = Cvar_Get ("cl_autoskins", "0", 0);
	cl_predict = Cvar_Get ("cl_predict", "1", 0);
//	cl_minfps = Cvar_Get ("cl_minfps", "5", 0);
	cl_maxfps = Cvar_Get ("cl_maxfps", "90", 0);
	cl_show_tooltips = Cvar_Get ("cl_show_tooltips", "1", CVAR_ARCHIVE);

	cl_camrotspeed = Cvar_Get ("cl_camrotspeed", "250", 0);
	cl_camrotaccel = Cvar_Get ("cl_camrotaccel", "400", 0);
	cl_cammovespeed = Cvar_Get ("cl_cammovespeed", "750", 0);
	cl_cammoveaccel = Cvar_Get ("cl_cammoveaccel", "1250", 0);
	cl_camyawspeed = Cvar_Get ("cl_camyawspeed", "160", 0);
	cl_campitchspeed = Cvar_Get ("cl_campitchspeed", "0.5", 0);
	cl_camzoomquant = Cvar_Get ("cl_camzoomquant", "0.25", 0);
	cl_centerview = Cvar_Get ("cl_centerview", "1", 0);

	cl_run = Cvar_Get ("cl_run", "0", CVAR_ARCHIVE);
	freelook = Cvar_Get( "freelook", "0", CVAR_ARCHIVE );
	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE);
	sensitivity = Cvar_Get ("sensitivity", "2", CVAR_ARCHIVE);
	cl_markactors = Cvar_Get ("cl_markactors", "1", CVAR_ARCHIVE);

	cl_precachemenus = Cvar_Get ("cl_precachemenus", "0", CVAR_ARCHIVE);

	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE);
	m_yaw = Cvar_Get ("m_yaw", "0.022", 0);
	m_forward = Cvar_Get ("m_forward", "1", 0);
	m_side = Cvar_Get ("m_side", "1", 0);

	cl_aviFrameRate = Cvar_Get ("cl_aviFrameRate", "25", CVAR_ARCHIVE);
	cl_aviMotionJpeg = Cvar_Get ("cl_aviMotionJpeg", "1", CVAR_ARCHIVE);

	cl_fps = Cvar_Get ("cl_fps", "0", CVAR_ARCHIVE);
	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_ARCHIVE);
	cl_showmiss = Cvar_Get ("cl_showmiss", "0", 0);
	cl_showclamp = Cvar_Get ("showclamp", "0", 0);
	cl_timeout = Cvar_Get ("cl_timeout", "120", 0);
	cl_paused = Cvar_Get ("paused", "0", 0);
	cl_timedemo = Cvar_Get ("timedemo", "0", 0);

	rcon_client_password = Cvar_Get ("rcon_password", "", 0);
	rcon_address = Cvar_Get ("rcon_address", "", 0);

	cl_logevents = Cvar_Get ("cl_logevents", "0", 0);

	cl_worldlevel = Cvar_Get ("cl_worldlevel", "0", 0);
	cl_selected = Cvar_Get ("cl_selected", "0", CVAR_NOSET);

//	cl_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);

	cl_numnames = Cvar_Get ("cl_numnames", "19", CVAR_NOSET);

	difficulty = Cvar_Get ("difficulty", "3", CVAR_ARCHIVE | CVAR_LATCH);
	difficulty->modified = qfalse;

	confirm_actions = Cvar_Get ("confirm_actions", "0", CVAR_ARCHIVE );

	Cvar_Set( "music", "" );

	mn_main = Cvar_Get ("mn_main", "main", 0);
	mn_sequence = Cvar_Get ("mn_sequence", "sequence", 0);
	mn_active = Cvar_Get ("mn_active", "", 0);
	mn_hud = Cvar_Get ("mn_hud", "hud", CVAR_ARCHIVE);
	mn_lastsave = Cvar_Get ("mn_lastsave", "", CVAR_ARCHIVE);

	//
	// userinfo
	//
	info_password = Cvar_Get ("password", "", CVAR_USERINFO);
	info_spectator = Cvar_Get ("spectator", "0", CVAR_USERINFO);
	name = Cvar_Get ("name", _("unnamed"), CVAR_USERINFO | CVAR_ARCHIVE);
#ifndef _WIN32
	// set alsa as default
	s_system = Cvar_Get ("s_system", "2", CVAR_USERINFO | CVAR_ARCHIVE);
#endif
	team = Cvar_Get ("team", "human", CVAR_USERINFO | CVAR_ARCHIVE);
	equip = Cvar_Get ("equip", "standard", CVAR_USERINFO | CVAR_ARCHIVE);
	teamnum = Cvar_Get ("teamnum", "1", CVAR_USERINFO | CVAR_ARCHIVE);
	campaign = Cvar_Get ("campaign", "main", 0);
	rate = Cvar_Get ("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE);	// FIXME
	msg = Cvar_Get ("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE);

	cl_vwep = Cvar_Get ("cl_vwep", "1", CVAR_ARCHIVE);

	sv_maxclients = Cvar_Get ("maxclients", "1", CVAR_SERVERINFO );

	//
	// register our commands
	//
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("pause", CL_Pause_f);
	Cmd_AddCommand ("pingservers", CL_PingServers_f);

	// text id is servers in menu_multiplayer.ufo
	Cmd_AddCommand ("servers_click", CL_ServerListClick_f);
	Cmd_AddCommand ("server_connect", CL_ServerConnect_f);
	Cmd_AddCommand ("bookmarks_click", CL_BookmarkListClick_f);
	Cmd_AddCommand ("bookmarks_print", CL_BookmarkPrint_f);
	Cmd_AddCommand ("bookmark_add", CL_BookmarkAdd_f );

	// text id is ships in menu_geoscape.ufo
	Cmd_AddCommand ("ships_click", CL_SelectAircraft_f);
	Cmd_AddCommand ("ships_rclick", CL_OpenAircraft_f);

	Cmd_AddCommand ("userinfo", CL_Userinfo_f);
	Cmd_AddCommand ("snd_restart", CL_Snd_Restart_f);

	Cmd_AddCommand ("changing", CL_Changing_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);

	Cmd_AddCommand ("quit", CL_Quit_f);

	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddCommand ("rcon", CL_Rcon_f);

// 	Cmd_AddCommand ("packet", CL_Packet_f); // this is dangerous to leave in

	Cmd_AddCommand ("setenv", CL_Setenv_f );

	Cmd_AddCommand ("precache", CL_Precache_f);

	Cmd_AddCommand ("seq_start", CL_SequenceStart_f );
	Cmd_AddCommand ("seq_end", CL_SequenceEnd_f );

	Cmd_AddCommand ("video", CL_Video_f );
	Cmd_AddCommand ("stopvideo", CL_StopVideo_f );

	//
	// forward to server commands
	//
	// the only thing this does is allow command completion
	// to work -- all unknown commands are automatically
	// forwarded to the server
	Cmd_AddCommand ("wave", NULL);
	Cmd_AddCommand ("inven", NULL);
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("use", NULL);
	Cmd_AddCommand ("drop", NULL);
	Cmd_AddCommand ("say", NULL);
	Cmd_AddCommand ("say_team", NULL);
	Cmd_AddCommand ("info", NULL);
	Cmd_AddCommand ("prog", NULL);
	Cmd_AddCommand ("give", NULL);
	Cmd_AddCommand ("god", NULL);
	Cmd_AddCommand ("notarget", NULL);
	Cmd_AddCommand ("noclip", NULL);
	Cmd_AddCommand ("invuse", NULL);
	Cmd_AddCommand ("invprev", NULL);
	Cmd_AddCommand ("invnext", NULL);
	Cmd_AddCommand ("invdrop", NULL);
	Cmd_AddCommand ("weapnext", NULL);
	Cmd_AddCommand ("weapprev", NULL);
	Cmd_AddCommand ("playerlist", NULL);
	Cmd_AddCommand ("players", NULL);
}



/*
===============
CL_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void CL_WriteConfiguration (void)
{
//	FILE	*f;
	char	path[MAX_QPATH];

	if (cls.state == ca_uninitialized)
		return;

	Com_sprintf (path, sizeof(path), "%s/config.cfg", FS_Gamedir());
	Cvar_WriteVariables (path);
}


/*
==================
CL_FixCvarCheats

==================
*/

typedef struct
{
	char	*name;
	char	*value;
	cvar_t	*var;
} cheatvar_t;

cheatvar_t	cheatvars[] = {
//	{"timescale", "1"},
	{"timedemo", "0"},
	{"r_drawworld", "1"},
	{"cl_testlights", "0"},
	{"r_fullbright", "0"},
//	{"r_drawflat", "0"},
	{"paused", "0"},
	{"fixedtime", "0"},
//	{"sw_draworder", "0"},
	{"gl_lightmap", "0"},
	{"gl_wire", "0"},
	{"gl_saturatelighting", "0"},
	{NULL, NULL}
};

int		numcheatvars;

void CL_FixCvarCheats (void)
{
	int			i;
	cheatvar_t	*var;

	if ( !Q_strncmp(cl.configstrings[CS_MAXCLIENTS], "1", MAX_TOKEN_CHARS)
		|| !cl.configstrings[CS_MAXCLIENTS][0] )
		return;		// single player can cheat

	// find all the cvars if we haven't done it yet
	if (!numcheatvars)
	{
		while (cheatvars[numcheatvars].name)
		{
			cheatvars[numcheatvars].var = Cvar_Get (cheatvars[numcheatvars].name,
					cheatvars[numcheatvars].value, 0);
			numcheatvars++;
		}
	}

	// make sure they are all set to the proper values
	for (i=0, var = cheatvars ; i<numcheatvars ; i++, var++)
	{
		if ( Q_strcmp (var->var->string, var->value) )
		{
			Cvar_Set (var->name, var->value);
		}
	}
}

//============================================================================

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
//	sizebuf_t	buf;
//	byte		data[128];

	// send a userinfo update if needed
	if ( cls.state >= ca_connected)
	{
		if (userinfo_modified)
		{
			userinfo_modified = qfalse;
			MSG_WriteByte (&cls.netchan.message, clc_userinfo);
			MSG_WriteString (&cls.netchan.message, Cvar_Userinfo() );
		}

		if (cls.netchan.message.cursize	|| curtime - cls.netchan.last_sent > 1000 )
			Netchan_Transmit (&cls.netchan, 0, NULL);
	}
}


/*
==================
CL_SendCommand
==================
*/
void CL_SendCommand (void)
{
	// get new key events
	Sys_SendKeyEvents ();

	// allow mice or other external controllers to add commands
	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

	// send intentions now
	CL_SendCmd ();

	// fix any cheating cvars
	CL_FixCvarCheats ();

	// resend a connection request if necessary
	CL_CheckForResend ();
}


/*
==================
CL_AddMapParticle
==================
*/
void CL_AddMapParticle (char *ptl, vec3_t origin, vec2_t wait, char *info, int levelflags)
{
	mp_t	*mp;

	mp = &MPs[numMPs++];

	if ( numMPs >= MAX_MAPPARTICLES )
	{
		Sys_Error("Too many map particles\n");
		return;
	}

	Q_strncpyz( mp->ptl, ptl, MAX_QPATH );
	VectorCopy( origin, mp->origin );
	mp->info = info;
	mp->levelflags = levelflags;
	mp->wait[0] = wait[0]*1000;
	mp->wait[1] = wait[1]*1000;
	mp->nextTime = cl.time + wait[0] + wait[1]*frand() + 1;

	Com_DPrintf( "Adding map particle %s (%i) with levelflags %i\n", ptl, numMPs, levelflags );
}

// FIXME: Howto mark them for gettext?
char *difficulty_names[] =
{
	"Chickenhearted",
	"Very Easy",
	"Easy",
	"Normal",
	"Hard",
	"Very Hard",
	"Insane"
};

/*
==================
CL_CvarCheck
==================
*/
void CL_CvarCheck( void )
{
	int v;

	// worldlevel
	if ( cl_worldlevel->modified )
	{
		int i;
		if ( (int)cl_worldlevel->value >= map_maxlevel-1 ) Cvar_SetValue( "cl_worldlevel", map_maxlevel-1 );
		if ( (int)cl_worldlevel->value < 0 ) Cvar_Set( "cl_worldlevel", "0" );
		for ( i = 0; i < map_maxlevel; i++ ) Cbuf_AddText( va( "deselfloor%i\n", i ) );
		for ( ; i < 8; i++ ) Cbuf_AddText( va( "disfloor%i\n", i ) );
		Cbuf_AddText( va( "selfloor%i\n", (int)cl_worldlevel->value ) );
		cl_worldlevel->modified = qfalse;
	}

	// difficulty
	if ( difficulty->modified )
	{
		v = (int)difficulty->value;

		if ( v < 0 ) v = 0;
		else if ( v > 6 ) v = 6;
		Cvar_Set( "mn_difficulty", _(difficulty_names[v]) );
	}

	// gl_mode and fullscreen
	v = Cvar_VariableValue( "mn_glmode" );
	// FIXME: Check whether this mode exists
	Cvar_Set( "mn_glmodestr", va( "%i*%i", vid_modes[v].width, vid_modes[v].height ) );
}


/*
==================
CL_Frame
==================
*/
#define NUM_DELTA_FRAMES	20

void CL_Frame (int msec)
{
	static int	extratime;
	static int  lasttimecalled;

	if (dedicated->value)
		return;

	if ( sv_maxclients->modified )
	{
		if ( (int)sv_maxclients->value > 1 )
		{
			ccs.singleplayer = qfalse;
			Com_Printf("Changing to Multiplayer\n");
		}
		else
		{
			ccs.singleplayer = qtrue;
			Com_Printf("Changing to Singleplayer\n");
		}
		sv_maxclients->modified = qfalse;
	}

	extratime += msec;

	if (!cl_timedemo->value)
	{
		if (cls.state == ca_connected && extratime < 100)
			return;			// don't flood packets out while connecting
		if (extratime < 1000/cl_maxfps->value)
			return;			// framerate is too high
	}

	// decide the simulation time
	cls.frametime = extratime/1000.0;
	cls.realtime = curtime;
	cl.time += extratime;
	if ( !blockEvents ) cl.eventTime += extratime;

	// let the mouse activate or deactivate
	IN_Frame ();

	// if recording an avi, lock to a fixed fps
	if ( CL_VideoRecording() && cl_aviFrameRate->value && msec) {
		// save the current screen
		if ( cls.state == ca_active )
		{
			CL_TakeVideoFrame();

			// fixed time for next frame'
			msec = (int)ceil( (1000.0f / cl_aviFrameRate->value) );
			if (msec == 0)
				msec = 1;
		}
	}

	// frame rate calculation
	if ( !(cls.framecount % NUM_DELTA_FRAMES) )
	{
		cls.framerate = NUM_DELTA_FRAMES * 1000.0 / (cls.framedelta + extratime);
		cls.framedelta = 0;
	}
	else cls.framedelta += extratime;

	extratime = 0;
#if 0
	if (cls.frametime > (1.0 / cl_minfps->value))
		cls.frametime = (1.0 / cl_minfps->value);
#else
	if (cls.frametime > (1.0 / 5))
		cls.frametime = (1.0 / 5);
#endif

	// if in the debugger last frame, don't timeout
	if (msec > 5000)
		cls.netchan.last_received = Sys_Milliseconds ();

	// fetch results from server
	CL_ReadPackets ();

	// event and LE updates
	CL_CvarCheck();
	CL_Events();
	LE_Think();
	// end the rounds when no soldier is alive
//	LE_Status();
	CL_RunMapParticles ();
	CL_ParticleRun();

	// send a new command message to the server
	CL_SendCommand ();

	// update camera position
	CL_CameraMove ();
	CL_ParseInput ();
	CL_ActorUpdateCVars();

	// allow rendering DLL change
	VID_CheckChanges ();
	if (!cl.refresh_prepped && cls.state == ca_active)
		CL_PrepRefresh ();

	// update the screen
	if (host_speeds->value)
		time_before_ref = Sys_Milliseconds ();
	SCR_UpdateScreen ();
	if (host_speeds->value)
		time_after_ref = Sys_Milliseconds ();

	// update audio
	S_Update (cl.refdef.vieworg, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);

	CDAudio_Update();

	// advance local effects for next frame
	CL_RunDLights ();
	CL_RunLightStyles ();
	SCR_RunConsole ();

	cls.framecount++;

	if ( log_stats->value )
	{
		if ( cls.state == ca_active )
		{
			if ( !lasttimecalled )
			{
				lasttimecalled = Sys_Milliseconds();
				if ( log_stats_file )
					fprintf( log_stats_file, "0\n" );
			}
			else
			{
				int now = Sys_Milliseconds();

				if ( log_stats_file )
					fprintf( log_stats_file, "%d\n", now - lasttimecalled );
				lasttimecalled = now;
			}
		}
	}
}


//============================================================================

/*
====================
CL_Init
====================
*/
void CL_Init (void)
{
	if (dedicated->value)
		return;		// nothing running on the client

	// all archived variables will now be loaded
	Con_Init ();
#ifdef _WIN32
	// sound must be initialized after window is created
	VID_Init ();
	S_Init ();
#else
	S_Init ();
	VID_Init ();
#endif

	V_Init ();

	net_message.data = net_message_buffer;
	net_message.maxsize = sizeof(net_message_buffer);

	SCR_Init ();

	CDAudio_Init ();
	CL_InitLocal ();
	IN_Init ();

	Cbuf_AddText ("exec autoexec.cfg\n");
	// FIXME: Maybe we should activate this again when all savegames issues are solved
//	Cbuf_AddText( "loadteam current\n" );
	FS_ExecAutoexec ();
	Cbuf_Execute ();
}


/*
===============
CL_Shutdown

FIXME: this is a callback from Sys_Quit and Com_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void CL_Shutdown(void)
{
	static qboolean isdown = qfalse;

	if (isdown)
	{
		printf ("recursive shutdown\n");
		return;
	}
	isdown = qtrue;

	CL_WriteConfiguration ();

	CDAudio_Shutdown();
	S_Shutdown();
	IN_Shutdown();
	VID_Shutdown();
	MN_Shutdown();
}


