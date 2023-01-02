/*  This file is likely not final.

Versus Quakespasm:

1. Movie capture
2. Demo rewind/fast forward
3. host_maxedicts is automatic guessing
4. Mousewheel check when not connected to server for dinput console scroll
5. Freezeall stops particles too

NOT yet ...

A. Quakespasm
B. Elimination of host_time.  Hmmm.  How?

C. Quakespasm does unusual stuff with "stuffcmds". I get what they are trying
   to do, but no other engine has extensively rewritten stuffcmds handling
   so I think it is a bit heavy handed for whatever their goals are.  I get what
   they are trying to address, but I'm not certain they understand the relationship
   to quake.rc, etc.

D. Quakespasm background music.

E. Quakespasm loads colormap.lmp; only WinQuake uses that file.

F. Quakespasm alternate log file handling.

G. I am not sure what the Quakespasm log handling does or what problem it is
   meant to solve or if it does it correctly within the context of several
   possible uses of a Quake engine (including several dedicated servers running
   different mods, like what is often done with ProQuake).  I care about this
   mainly because I do not see a need for separate ProQuake vs. Mark V in the
   future.  Let's have one classic engine that can run client and server and
   Func Messageboard releases and mods.

H. Figure out Quakespasm's input scheme.  It looks simpler.


*/

/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske

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
// host.c -- coordinates spawning and killing of local servers

#include "quakedef.h"
#include <setjmp.h>

/*

A server can allways be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/

quakeparms_t *host_parms;

qboolean	host_initialized;		// true if into command execution
qboolean	host_post_initialized;	// true if completed initial config execution

double		host_frametime;
double		host_time;
double		realtime;				// without any filtering or bounding
double		oldrealtime;			// last frame run

int			host_framecount;

int			host_hunklevel;

int			minimum_memory;

client_t	*host_client;			// current client

jmp_buf 	host_abortserver;


cvar_t	host_framerate = {"host_framerate","0",CVAR_NONE};	// set for slow motion
cvar_t	host_speeds = {"host_speeds","0", CVAR_NONE};			// set for running times
cvar_t	host_maxfps = {"host_maxfps", "72", CVAR_ARCHIVE}; //johnfitz
cvar_t	host_sleep = {"host_sleep", "0", CVAR_NONE }; //johnfitz
cvar_t	host_timescale = {"host_timescale", "0",CVAR_NONE}; //johnfitz
cvar_t	max_edicts = {"host_maxedicts", "0",CVAR_NONE}; //Baker: 0 = automatic
cvar_t	model_external_vis = { "external_vis", "1", CVAR_NONE };


cvar_t	sys_ticrate = {"sys_ticrate","0.05",CVAR_NONE}; // dedicated server
cvar_t	serverprofile = {"serverprofile","0",CVAR_NONE};

cvar_t	fraglimit = {"fraglimit","0",CVAR_NOTIFY|CVAR_SERVERINFO};
cvar_t	timelimit = {"timelimit","0",CVAR_NOTIFY|CVAR_SERVERINFO};
cvar_t	teamplay = {"teamplay","0",CVAR_NOTIFY|CVAR_SERVERINFO};
cvar_t	samelevel = {"samelevel","0",CVAR_NONE};
cvar_t	noexit = {"noexit","0",CVAR_NOTIFY|CVAR_SERVERINFO};
cvar_t	skill = {"skill","1",CVAR_NONE};						// 0 - 3
cvar_t	deathmatch = {"deathmatch","0",CVAR_NONE};			// 0, 1, or 2
cvar_t	coop = {"coop","0",CVAR_NONE};			// 0 or 1

cvar_t	pausable = {"pausable","1",CVAR_NONE};

cvar_t	developer = {"developer","0",CVAR_NONE};

cvar_t	temp1 = {"temp1","0",CVAR_NONE};

cvar_t devstats = {"devstats","0",CVAR_NONE}; //johnfitz -- track developer statistics that vary every frame

devstats_t dev_stats, dev_peakstats;
overflowtimes_t dev_overflows; //this stores the last time overflow messages were displayed, not the last time overflows occured

/*
================
Max_Edicts_f -- johnfitz
================
*/
static void Max_Edicts_f (cvar_t *var)
{
	static float oldval = 1024; //must match the default value for max_edicts

	//TODO: clamp it here?

	if (max_edicts.value == oldval)
		return;

	if (cls.state == ca_connected || sv.active)
		Con_Printf ("Changes will not take effect until the next level load.\n");

	oldval = max_edicts.value;
}

/*
================
Host_EndGame
================
*/
void Host_EndGame (const char *message, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,message);
	q_vsnprintf (string, sizeof(string), message, argptr);
	va_end (argptr);
	Con_DPrintf ("Host_EndGame: %s\n",string);

	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_EndGame: %s\n",string);	// dedicated servers exit

	if (cls.demonum != -1)
		CL_NextDemo ();
	else
	{
//		SCR_EndLoadingPlaque (); // Baker: any disconnect state should end the loading plague, right?
//	if (key_dest == key_game)
//		key_dest = key_console;

		CL_Disconnect ();
	}

	longjmp (host_abortserver, 1);
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (const char *error, ...)
{
	va_list		argptr;
	char		string[1024];
	static	qboolean inerror = false;

	if (inerror)
		Sys_Error ("Host_Error: recursively entered");
	inerror = true;

	SCR_EndLoadingPlaque ();		// reenable screen updates

	va_start (argptr,error);
	q_vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);
	Con_Printf ("Host_Error: %s\n",string);

	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_Error: %s\n",string);	// dedicated servers exit

	CL_Disconnect ();
	cls.demonum = -1;
	cl.intermission = 0; //johnfitz -- for errors during intermissions (changelevel with no map found, etc.)

	inerror = false;

	longjmp (host_abortserver, 1);
}

/*
================
Host_FindMaxClients
================
*/
void	Host_FindMaxClients (void)
{
	int		i;

	svs.maxclients = 1;

	i = COM_CheckParm ("-dedicated");
	if (i)
	{
		cls.state = ca_dedicated;
		if (i != (com_argc - 1))
		{
			svs.maxclients = Q_atoi (com_argv[i+1]);
		}
		else
			svs.maxclients = 8;
	}
	else
		cls.state = ca_disconnected;

	i = COM_CheckParm ("-listen");
	if (i)
	{
		if (cls.state == ca_dedicated)
			Sys_Error ("Only one of -dedicated or -listen can be specified");
		if (i != (com_argc - 1))
			svs.maxclients = Q_atoi (com_argv[i+1]);
		else
			svs.maxclients = 8;
	}
	if (svs.maxclients < 1)
		svs.maxclients = 8;
	else if (svs.maxclients > MAX_SCOREBOARD)
		svs.maxclients = MAX_SCOREBOARD;

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclientslimit < 4)
		svs.maxclientslimit = 4;
	svs.clients = (struct client_s *)Hunk_AllocName (svs.maxclientslimit*sizeof(client_t), "clients");

	if (svs.maxclients > 1)
		Cvar_SetQuick (&deathmatch, "1");
	else
		Cvar_SetQuick (&deathmatch, "0");
}

void Host_Version_f (void)
{
	Con_Printf ("Quake Version %1.2f\n", QUAKE_VERSION);
	Con_Printf ("%s Version %1.2f\n", ENGINE_NAME, FITZQUAKE_VERSION);
	Con_Printf ("Exe: "__TIME__" "__DATE__"\n");
}

/* cvar callback functions : */
void Host_Callback_Notify (cvar_t *var)
{
	if (sv.active)
		SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name, var->string);
}

// Baker:  Hint to tell dedicated server we are after initial execution of configs.
void Host_Post_Initialization_f (void)
{
	host_post_initialized = true;
}


/*
=======================
Host_InitLocal
======================
*/
void Host_InitLocal (void)
{
	Cmd_AddCommand ("version", Host_Version_f);
	Cmd_AddCommand ("_host_post_initialized", Host_Post_Initialization_f);

	Host_InitCommands ();

	Cvar_RegisterVariable (&host_framerate);
	Cvar_RegisterVariable (&host_speeds);
	Cvar_RegisterVariable (&host_maxfps); //johnfitz
	Cvar_RegisterVariable (&host_timescale); //johnfitz
	Cvar_RegisterVariable (&host_sleep); //Baker

	Cvar_RegisterVariableWithCallback (&max_edicts, Max_Edicts_f);
	Cvar_RegisterVariable (&devstats); //johnfitz

	Cvar_RegisterVariable (&model_external_vis);

	Cvar_RegisterVariable (&sys_ticrate);
	Cvar_RegisterVariable (&serverprofile);

	Cvar_RegisterVariableWithCallback (&fraglimit, Host_Callback_Notify);
	Cvar_RegisterVariableWithCallback (&timelimit, Host_Callback_Notify);
	Cvar_RegisterVariableWithCallback (&teamplay, Host_Callback_Notify);
	Cvar_RegisterVariableWithCallback (&noexit, Host_Callback_Notify);

	Cvar_RegisterVariable (&samelevel);
	
	Cvar_RegisterVariable (&skill);
	Cvar_RegisterVariable (&developer);
	Cvar_RegisterVariable (&coop);
	Cvar_RegisterVariable (&deathmatch);

	Cvar_RegisterVariable (&pausable);

	Cvar_RegisterVariable (&temp1);

	Host_FindMaxClients ();

	host_time = 1.0;		// so a think at time 0 won't get called
}


/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_WriteConfiguration (void)
{
	FILE	*f;

// dedicated servers initialize the host but don't parse and set the
// config.cfg cvars
	if (host_initialized && !isDedicated)
	{
		f = FS_fopen_write (va("%s/%s", com_gamedir, CONFIG_CFG), "w");
		if (f)
		{
			VID_Cvars_Sync_To_Mode (&vid.modelist[vid.modenum_user_selected]); //johnfitz -- write actual current mode to config file, in case cvars were messed with

			Key_WriteBindings (f);
			Cvar_WriteVariables (f);

			FS_fclose (f);
			Recent_File_Set_RelativePath (CONFIG_CFG);			
		} Con_Printf ("Couldn't write %s.\n", CONFIG_CFG);

	}
}


/*
=================
SV_ClientPrintf

Sends text across to be displayed
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	q_vsnprintf (string, sizeof(string), fmt,argptr);
	va_end (argptr);

	MSG_WriteByte (&host_client->message, svc_print);
	MSG_WriteString (&host_client->message, string);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	int			i;

	va_start (argptr,fmt);
	q_vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active && svs.clients[i].spawned)
		{
			MSG_WriteByte (&svs.clients[i].message, svc_print);
			MSG_WriteString (&svs.clients[i].message, string);
		}
	}
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void Host_ClientCommands (const char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	q_vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&host_client->message, svc_stufftext);
	MSG_WriteString (&host_client->message, string);
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void SV_DropClient (qboolean crash)
{
	int		saveSelf;
	int		i;
	client_t *client;

	if (!crash)
	{
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage (host_client->netconnection))
		{
			MSG_WriteByte (&host_client->message, svc_disconnect);
			NET_SendMessage (host_client->netconnection, &host_client->message);
		}

		if (host_client->edict && host_client->spawned)
		{
		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
			saveSelf = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			PR_ExecuteProgram (pr_global_struct->ClientDisconnect);
			pr_global_struct->self = saveSelf;
		}

		Sys_Printf ("Client %s removed\n",host_client->name);
	}

// break the net connection
	NET_Close (host_client->netconnection);
	host_client->netconnection = NULL;

// free the client (the body stays around)
	host_client->active = false;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	net_activeconnections--;

// send notification to all clients
	for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
	{
		if (!client->active)
			continue;
		MSG_WriteByte (&client->message, svc_updatename);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteString (&client->message, "");
		MSG_WriteByte (&client->message, svc_updatefrags);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteShort (&client->message, 0);
		MSG_WriteByte (&client->message, svc_updatecolors);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteByte (&client->message, 0);
	}
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer(qboolean crash)
{
	int		i;
	int		count;
	sizebuf_t	buf;
	byte		message[4];
	double	start;

	if (!sv.active)
		return;

	sv.active = false;

// stop all client sounds immediately
	if (cls.state == ca_connected)
		CL_Disconnect ();

// flush any pending messages - like the score!!!
	start = Sys_DoubleTime();
	do
	{
		count = 0;
		for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		{
			if (host_client->active && host_client->message.cursize)
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					NET_SendMessage(host_client->netconnection, &host_client->message);
					SZ_Clear (&host_client->message);
				}
				else
				{
					NET_GetMessage(host_client->netconnection);
					count++;
				}
			}
		}
		if ((Sys_DoubleTime() - start) > 3.0)
			break;
	}
	while (count);

// make sure all the clients know we're disconnecting
	buf.data = message;
	buf.maxsize = 4;
	buf.cursize = 0;
	MSG_WriteByte(&buf, svc_disconnect);
	count = NET_SendToAll(&buf, 5.0);
	if (count)
		Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", count);

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_DropClient(crash);

//
// clear structures
//
// Baker --- this is redundant, but I want this function to do as expected
	memset (&sv, 0, sizeof(sv)); // ServerSpawn already do this by Host_ClearMemory
	memset (svs.clients, 0, svs.maxclientslimit*sizeof(client_t));
}


/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void Host_ClearMemory (void)
{
	Con_DPrintf ("Clearing memory\n");
	Mod_ClearAll ();
/* host_hunklevel MUST be set at this point */
	Hunk_FreeToLowMark (host_hunklevel);
	cls.signon = 0;
	memset (&sv, 0, sizeof(sv));
	memset (&cl, 0, sizeof(cl));
}


//==============================================================================
//
// Host Frame
//
//==============================================================================

/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/
float frame_timescale = 1.0f;
qboolean Host_FilterTime (float time)
{
#ifdef SUPPORTS_AVI_CAPTURE // Baker change
	double Movie_FrameTime (void);
	qboolean Movie_IsActive (void);
#endif // Baker change +

	float maxfps; //johnfitz

	realtime += time;

	//johnfitz -- max fps cvar
	maxfps = CLAMP (10.0, host_maxfps.value, 1000.0);

	// Baker: Don't sleep during a capturedemo (CPU intensive!) or during a timedemo (performance testing)
	if (!cls.capturedemo && !cls.timedemo && realtime - oldrealtime < 1.0/maxfps)
	{
		if (host_sleep.value)
			Sys_Sleep (1); // Lower cpu
		return false; // framerate is too high
	}

	//johnfitz
#ifdef SUPPORTS_AVI_CAPTURE // Baker change
	if (Movie_IsActive())
		host_frametime = Movie_FrameTime ();
	else
#endif // Baker change +

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	//johnfitz -- host_timescale is more intuitive than host_framerate
	frame_timescale = 1;
	if (cls.demoplayback && cls.demospeed && !cls.timedemo && !cls.capturedemo && cls.demonum == -1)
	{
		host_frametime *= cls.demospeed;
		frame_timescale = cls.demospeed;
	}
	else
	if (host_timescale.value > 0 && !(cls.demoplayback && cls.demospeed && !cls.timedemo && !cls.capturedemo && cls.demonum == -1) )
	{
		host_frametime *= host_timescale.value;
		frame_timescale = host_timescale.value;
	}
	//johnfitz
	else if (host_framerate.value > 0)
		host_frametime = host_framerate.value;
	else // don't allow really long or short frames
		host_frametime = CLAMP (0.001, host_frametime, 0.1); //johnfitz -- use CLAMP

	return true;
}

/*
===================
Host_GetConsoleCommands

Add them exactly as if they had been typed at the console
===================
*/
void Host_GetConsoleCommands (void)
{
	const char	*cmd;

// Baker: Native windows dedicated server needs this
//	if (cls.state == ca_dedicated)
//		return;	// no stdin necessary in graphical mode
	
	while (1)
	{
		cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Cbuf_AddText (cmd);
	}
}

/*
==================
Host_ServerFrame
==================
*/
void Host_ServerFrame (void)
{
	int		i, active; //johnfitz
	edict_t	*ent; //johnfitz

// run the world state
	pr_global_struct->frametime = host_frametime;

// set the time and clear the general datagram
	SV_ClearDatagram ();

// check for new clients
	SV_CheckForNewClients ();

// read client messages
	SV_RunClients ();

// move things around and think
// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game) )
		SV_Physics ();

//johnfitz -- devstats
	if (cls.signon == SIGNONS)
	{
		for (i=0, active=0; i<sv.num_edicts; i++)
		{
			ent = EDICT_NUM(i);
			if (!ent->free)
				active++;
		}
		if (active > MAX_WINQUAKE_EDICTS && dev_peakstats.edicts <= MAX_WINQUAKE_EDICTS)
			Con_Warning ("%i edicts exceeds standard limit of %d.\n", active, MAX_WINQUAKE_EDICTS); // 600
		dev_stats.edicts = active;
		dev_peakstats.edicts = q_max(active, dev_peakstats.edicts);
	}
//johnfitz

// send all messages to the clients
	SV_SendClientMessages ();
}

/*
==================
Host_Frame

Runs all active servers
==================
*/
void _Host_Frame (float time)
{
	static double		time1 = 0;
	static double		time2 = 0;
	static double		time3 = 0;
	int			pass1, pass2, pass3;

	if (setjmp (host_abortserver) )
		return;			// something bad happened, or the server disconnected

// keep the random time dependent
	rand ();

// decide the simulation time
	if (!Host_FilterTime (time))
		return;			// don't run too fast, or packets will flood out

// get new key events
#if !defined (__APPLE__) && !defined (MACOSX)
	Sys_SendKeyEvents ();
#else
    {
        void IN_SendKeyEvents (void);

        IN_SendKeyEvents ();
    }
#endif // !__APPLE__ && !MACOSX

// allow mice or other external controllers to add commands
//	IN_Commands ();  Was just joystick

// process console commands
	Cbuf_Execute ();

	NET_Poll();

// if running the server locally, make intentions now
	if (sv.active)
		CL_SendCmd ();

// DINPUT
//	else if (con_forcedup && key_dest == key_game) // Allows console scrolling when con_forcedup
//		IN_MouseWheel ();	// Grab mouse wheel input for dinput (contrary to the name, IN_MouseWheel is only for DirectInput

//-------------------
//
// server operations
//
//-------------------

// check for commands typed to the host
	Host_GetConsoleCommands ();

	if (sv.active)
		Host_ServerFrame ();

//-------------------
//
// client operations
//
//-------------------

// if running the server remotely, send intentions now after
// the incoming messages have been read
	if (!sv.active)
		CL_SendCmd ();

	host_time += host_frametime;

// fetch results from server
	if (cls.state == ca_connected)
		CL_ReadFromServer ();

// update video
	if (host_speeds.value)
		time1 = Sys_DoubleTime ();

	{
		SCR_UpdateScreen ();
		if (!sv.frozen)
			CL_RunParticles (); //johnfitz -- seperated from rendering
	}

	if (host_speeds.value)
		time2 = Sys_DoubleTime ();

// update audio
	if (cls.signon == SIGNONS)
	{
		S_Update (r_origin, vpn, vright, vup);
		if (!sv.frozen)
			CL_DecayLights ();
	}
	else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	CDAudio_Update();

	if (host_speeds.value)
	{
		pass1 = (time1 - time3)*1000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1)*1000;
		pass3 = (time3 - time2)*1000;
		Con_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
					pass1+pass2+pass3, pass1, pass2, pass3);
	}

	host_framecount++;

}

void Host_Frame (float time)
{
	double	time1, time2;
	static double	timetotal;
	static int		timecount;
	int		i, c, m;

	if (!serverprofile.value)
	{
		_Host_Frame (time);
		return;
	}

	time1 = Sys_DoubleTime ();
	_Host_Frame (time);
	time2 = Sys_DoubleTime ();

	timetotal += time2 - time1;
	timecount++;

	if (timecount < 1000)
		return;

	m = timetotal*1000/timecount;
	timecount = 0;
	timetotal = 0;
	c = 0;
	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active)
			c++;
	}

	Con_Printf ("serverprofile: %2i clients %2i msec\n",  c,  m);
}

/*
====================
Host_Init
====================
*/
void Host_Init (void)
{
	if (standard_quake)
		minimum_memory = MINIMUM_MEMORY;
	else minimum_memory = MINIMUM_MEMORY_LEVELPAK;

	if (COM_CheckParm ("-minmemory"))
		host_parms->memsize = minimum_memory;

	if (host_parms->memsize < minimum_memory)
		Sys_Error ("Only %4.1f megs of memory available, can't execute game", host_parms->memsize / (float)0x100000);

	com_argc = host_parms->argc;
	com_argv = host_parms->argv;

	Memory_Init (host_parms->membase, host_parms->memsize);
	Cbuf_Init ();
	Cmd_Init ();
	Cvar_Init (); //johnfitz
	COM_Init ();
	COM_InitFilesystem ();
	Host_InitLocal ();
	W_LoadWadFile (); //johnfitz -- filename is now hard-coded for honesty
	if (cls.state != ca_dedicated)
	{
		Key_Init ();
		Con_Init ();
	}
	PR_Init ();
	Mod_Init ();
	NET_Init ();
	SV_Init ();

	Con_SafePrintf ("Exe: "__TIME__" "__DATE__"\n");
	Con_SafePrintf ("%4.1f megabyte heap\n",host_parms->memsize/ (1024*1024.0));

	if (cls.state != ca_dedicated)
	{
		// Baker: In WinQuake, colormap.lmp would be loaded here.
		V_Init ();
		Chase_Init ();		
		M_Init ();
		Lists_Init ();
		VID_Init ();
		Input_Init ();
		TexMgr_Init (); //johnfitz
		Draw_Init ();
		SCR_Init ();
		R_Init ();
		S_Init ();
		CDAudio_Init ();
		Sbar_Init ();
		CL_Init ();
		Recent_File_Init (); // Baker
		Zip_Init ();	// Baker
		Downloads_Init (); // Baker
	}

	Cbuf_InsertText ("exec quake.rc\n");
//	Cbuf_InsertText ("exec fitzquake.rc\n"); //johnfitz (inserted second so it'll be executed first)


	Cbuf_AddText ("\n_host_post_initialized\n");  // Baker -- hint to dedicated server.

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;
	Con_Printf ("\n========= Quake Initialized =========\n\n");
	{
		extern int community_check;
		if (community_check == 1)
			Con_Printf ("\nCommunity Registered Check\n");
	}
}


/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown(void)
{
	static qboolean isdown = false;

	if (isdown)
	{
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

// keep Con_Printf from trying to update the screen
	scr_disabled_for_loading = true;

	Host_WriteConfiguration ();

	NET_Shutdown ();

	if (cls.state != ca_dedicated)
	{
		if (con_initialized)
			History_Shutdown ();	
		CDAudio_Shutdown ();
		S_Shutdown();
		Input_Shutdown ();
		VID_Shutdown();
	}
}

qboolean Read_Early_Cvars_For_File (const char* config_file_name, const cvar_t* list[])
{
	qboolean found_any_cvars = false;
	const cvar_t* var;
	char	config_buffer[8192];
	FILE	*f;
	int		bytes_size = COM_FOpenFile (config_file_name, &f);
	int i;

	// Read the file into the buffer.  Read the first 8000 bytes (if longer, tough cookies)
	// Because it is pretty likely that size of file will get a "SZ_GetSpace: overflow without allowoverflow set"
	// During command execution

	if (bytes_size !=-1)
	{
		int	bytes_in = q_min (bytes_size, 8000); // Cap at 8000
		int bytes_read = fread (config_buffer, 1, bytes_in, f);
		config_buffer [bytes_read + 1] = 0; // Null terminate just in case

		FS_fclose (f);
	} else return false;

	for (i = 0, var = list[i]; var; i++, var = list[i])
	{
		float value;
		qboolean found = Parse_Float_From_String (&value, config_buffer, var->name);

//		MessageBox (NULL, va("Cvar %s was %s and is %g", video_cvars[i]->name, found ? "Found" : "Not found", found ? value : 0), "", MB_OK);

		if (found == false)
			continue;

		Cvar_SetValue (var->name, value);
		found_any_cvars = true;
	}

	return found_any_cvars;
}

