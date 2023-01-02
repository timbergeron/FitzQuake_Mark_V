/* Versus Quakespasm:

1. demo rewind and fast forward
2. inherent demo progress estimation (filesize of demo, offset of read)
3. AVI capture
4. Title demos
5. Clearing of demo queue when an intentional user action occurs

*/

/*
Copyright (C) 1996-2001 Id Software, Inc.

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

#include "quakedef.h"

static void CL_FinishTimeDemo (void);



typedef struct framepos_s
{
	long				baz;
	struct framepos_s	*next;
} framepos_t;

framepos_t	*dem_framepos = NULL;
qboolean	start_of_demo = false;
qboolean	bumper_on = false;

/*
==============================================================================

DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

// from ProQuake: space to fill out the demo header for record at any time
static byte	demo_head[3][MAX_FITZQUAKE_MSGLEN];
static int		demo_head_size[2];

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback (void)
{
	if (!cls.demoplayback)
		return;

	FS_fclose (cls.demofile);

	cls.demoplayback = false;
	cls.demofile = NULL;
	cls.state = ca_disconnected;


	if (cls.timedemo)
		CL_FinishTimeDemo ();

#ifdef SUPPORTS_AVI_CAPTURE
	if (cls.capturedemo)
		Sys_WindowCaptionSet (NULL); // Restores it to default of "FitzQuake"

	Movie_StopPlayback ();
#endif

		if (cls.titledemo)
		{
			vid.recalc_refdef = true; // also triggers sbar refresh
			cls.titledemo = 0;
		}
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
static void CL_WriteDemoMessage (void)
{
	int		len;
	int		i;
	float	f;

	len = LittleLong (net_message.cursize);
	fwrite (&len, 4, 1, cls.demofile);
	for (i=0 ; i<3 ; i++)
	{
		f = LittleFloat (cl.viewangles[i]);
		fwrite (&f, 4, 1, cls.demofile);
	}
	fwrite (net_message.data, net_message.cursize, 1, cls.demofile);
	fflush (cls.demofile);
}

void PushFrameposEntry (long fbaz)
{
	framepos_t	*newf;

	newf = malloc (sizeof(framepos_t)); // Demo rewind
	newf->baz = fbaz;

	if (!dem_framepos)
	{
		newf->next = NULL;
		start_of_demo = false;
	}
	else
	{
		newf->next = dem_framepos;
	}
	dem_framepos = newf;
}

static void EraseTopEntry (void)
{
	framepos_t	*top;

	top = dem_framepos;
	dem_framepos = dem_framepos->next;
	free (top);
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
int CL_GetMessage (void)
{
	int		r, i;
	float	f;

	if (cl.paused & 2)
		return 0;

	if	(cls.demoplayback)
	{
		if (start_of_demo && cls.demorewind)
			return 0;

		if (cls.signon < SIGNONS)	// clear stuffs if new demo
			while (dem_framepos)
				EraseTopEntry ();

	// decide if it is time to grab the next message
		if (cls.signon == SIGNONS)	// allways grab until fully connected
		{
			if (cls.timedemo)
			{
				if (host_framecount == cls.td_lastframe)
					return 0;		// allready read this frame's message
				cls.td_lastframe = host_framecount;
			// if this is the second frame, grab the real td_starttime
			// so the bogus time on the first frame doesn't count
				if (host_framecount == cls.td_startframe + 1)
					cls.td_starttime = realtime;
			}
			else if (!cls.demorewind && cl.ctime <= cl.mtime[0])
				return 0;		// don't need another message yet
			else if (cls.demorewind && cl.ctime >= cl.mtime[0])
				return 0;

			// joe: fill in the stack of frames' positions
			// enable on intermission or not...?
			// NOTE: it can't handle fixed intermission views!
			if (!cls.demorewind /*&& !cl.intermission*/)
				PushFrameposEntry (ftell(cls.demofile));

		}

	// get the next message
		cls.demo_offset_current = ftell(cls.demofile);
		fread (&net_message.cursize, 4, 1, cls.demofile);
		VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
		for (i=0 ; i<3 ; i++)
		{
			r = fread (&f, 4, 1, cls.demofile);
			cl.mviewangles[0][i] = LittleFloat (f);
		}

		net_message.cursize = LittleLong (net_message.cursize);
		if (net_message.cursize > MAX_FITZQUAKE_MSGLEN)
			Host_Error ("Demo message > MAX_MSGLEN");
		r = fread (net_message.data, net_message.cursize, 1, cls.demofile);
		if (r != 1)
		{
			CL_StopPlayback ();
			return 0;
		}

		// joe: get out framestack's top entry
		if (cls.demorewind /*&& !cl.intermission*/)
		{
			if (dem_framepos/* && dem_framepos->baz*/)	// Baker: in theory, if this occurs we ARE at the start of the demo with demo rewind on
			{
				fseek (cls.demofile, dem_framepos->baz, SEEK_SET);
				EraseTopEntry (); // Baker: we might be able to improve this better but not right now.
			}
			if (!dem_framepos)
				bumper_on = start_of_demo = true;
		}

		return 1;
	}

	while (1)
	{
		r = NET_GetMessage (cls.netcon);

		if (r != 1 && r != 2)
			return r;

	// discard nop keepalive message
		if (net_message.cursize == 1 && net_message.data[0] == svc_nop)
			Con_Printf ("<-- server to client keepalive\n");
		else
			break;
	}

	if (cls.demorecording)
		CL_WriteDemoMessage ();

	if (cls.signon < 2)
	{
	// record messages before full connection, so that a
	// demo record can happen after connection is done
		memcpy(demo_head[cls.signon], net_message.data, net_message.cursize);
		demo_head_size[cls.signon] = net_message.cursize;
	}

	return r;
}


/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Printf ("Not recording a demo.\n");
		return;
	}

// write a disconnect message to the demo file
	SZ_Clear (&net_message);
	MSG_WriteByte (&net_message, svc_disconnect);
	CL_WriteDemoMessage ();

// finish up
	FS_fclose (cls.demofile);
	Recent_File_Set_FullPath (cls.demoname); // Close demo instance #2 (record end)

	cls.demofile = NULL;
	cls.demorecording = false;
	Lists_Update_Demolist ();
	Con_Printf ("Completed demo %s\n", COM_SkipPath(cls.demoname) );
}

void CL_Clear_Demos_Queue (void)
{
	int i;
	for (i = 0;i < MAX_DEMOS; i ++)	// Clear demo loop queue
		cls.demos[i][0] = 0;
	cls.demonum = -1;				// Set next demo to none
}

/*
====================
CL_Record_f

record <demoname> <map> [cd track]
====================
*/
void CL_Record_f (void)
{
	int		c;
	char	name[MAX_OSPATH];
	int		track;

	if (cmd_source != src_command)
		return;

	if (cls.demoplayback)
	{
		Con_Printf ("Can't record during demo playback\n");
		return;
	}

	if (cls.demorecording)
		CL_Stop_f();

	c = Cmd_Argc();
	if (c != 2 && c != 3 && c != 4)
	{
		Con_Printf ("record <demoname> [<map> [cd track]]\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	if (c == 2 && cls.state == ca_connected)
	{
#if 0
		Con_Printf("Can not record - already connected to server\nClient demo recording must be started before connecting\n");
		return;
#endif
		if (cls.signon < 2)
		{
			Con_Printf("Can't record - try again when connected\n");
			return;
		}
	}

// write the forced cd track number, or -1
	if (c == 4)
	{
		track = atoi(Cmd_Argv(3));
		Con_Printf ("Forcing CD track to %i\n", cls.forcetrack);
	}
	else
	{
		track = -1;
	}

	q_snprintf (name, sizeof(name), "%s/%s", com_gamedir, Cmd_Argv(1));

	CL_Clear_Demos_Queue (); // timedemo is a very intentional action
// start the map up
	if (c > 2)
	{
		Cmd_ExecuteString ( va("map %s", Cmd_Argv(2)), src_command);
		if (cls.state != ca_connected)
			return;
	}

// open the demo file
	COM_ForceExtension (name, ".dem", sizeof(name));

	Con_Printf ("recording to %s.\n", name);
	cls.demofile = FS_fopen_write (name, "wb");
	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't create %s\n", name);
		return;
	}

	// Officially recording ... copy the name for reference
	q_strlcpy (cls.demoname, name, sizeof(cls.demoname));

	cls.forcetrack = track;
	fprintf (cls.demofile, "%i\n", cls.forcetrack);

	cls.demorecording = true;

	// from ProQuake: initialize the demo file if we're already connected
	if (c == 2 && cls.state == ca_connected)
	{
		byte *data = net_message.data;
		int cursize = net_message.cursize;
		int i;

		for (i = 0 ; i < 2 ; i++)
		{
			net_message.data = demo_head[i];
			net_message.cursize = demo_head_size[i];
			CL_WriteDemoMessage();
		}

		net_message.data = demo_head[2];
		SZ_Clear (&net_message);

		// current names, colors, and frag counts
		for (i=0 ; i < cl.maxclients ; i++)
		{
			MSG_WriteByte (&net_message, svc_updatename);
			MSG_WriteByte (&net_message, i);
			MSG_WriteString (&net_message, cl.scores[i].name);
			MSG_WriteByte (&net_message, svc_updatefrags);
			MSG_WriteByte (&net_message, i);
			MSG_WriteShort (&net_message, cl.scores[i].frags);
			MSG_WriteByte (&net_message, svc_updatecolors);
			MSG_WriteByte (&net_message, i);
			MSG_WriteByte (&net_message, cl.scores[i].colors);
		}

		// send all current light styles
		for (i = 0 ; i < MAX_LIGHTSTYLES ; i++)
		{
			MSG_WriteByte (&net_message, svc_lightstyle);
			MSG_WriteByte (&net_message, i);
			MSG_WriteString (&net_message, cl_lightstyle[i].map);
		}

		// what about the CD track or SVC fog... future consideration.
		MSG_WriteByte (&net_message, svc_updatestat);
		MSG_WriteByte (&net_message, STAT_TOTALSECRETS);
		MSG_WriteLong (&net_message, cl.stats[STAT_TOTALSECRETS]);

		MSG_WriteByte (&net_message, svc_updatestat);
		MSG_WriteByte (&net_message, STAT_TOTALMONSTERS);
		MSG_WriteLong (&net_message, cl.stats[STAT_TOTALMONSTERS]);

		MSG_WriteByte (&net_message, svc_updatestat);
		MSG_WriteByte (&net_message, STAT_SECRETS);
		MSG_WriteLong (&net_message, cl.stats[STAT_SECRETS]);

		MSG_WriteByte (&net_message, svc_updatestat);
		MSG_WriteByte (&net_message, STAT_MONSTERS);
		MSG_WriteLong (&net_message, cl.stats[STAT_MONSTERS]);

		// view entity
		MSG_WriteByte (&net_message, svc_setview);
		MSG_WriteShort (&net_message, cl.viewentity);

		// signon
		MSG_WriteByte (&net_message, svc_signonnum);
		MSG_WriteByte (&net_message, 3);

		CL_WriteDemoMessage();

		// restore net_message
		net_message.data = data;
		net_message.cursize = cursize;
	}
}


/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/

cvar_t cl_titledemos_list = {"cl_titledemos_list", ""};

// Baker: So we know this is a real start demo
qboolean play_as_start_demo = false;
void CL_PlayDemo_NextStartDemo_f (void)
{
	play_as_start_demo = true;
	CL_PlayDemo_f (); // Inherits the cmd_argc and cmd_argv
	play_as_start_demo = false;
}

void CL_PlayDemo_f (void)
{
	char	name[MAX_OSPATH];
	int c;
	qboolean neg = false;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("playdemo <demoname> : plays a demo\n");
		return;
	}

// disconnect from server
	CL_Disconnect ();

	// Revert
	cls.demorewind = false;
	cls.demospeed = 0; // 0 = Don't use
	bumper_on = false;

// open the demo file
	q_strlcpy (name, Cmd_Argv(1), sizeof(name));
	COM_DefaultExtension (name, ".dem", sizeof(name));

	Con_Printf ("Playing demo from %s.\n", name);

	COM_FOpenFile (name, &cls.demofile);
	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't open %s\n", name);
		cls.demonum = -1;		// stop demo loop
		return;
	}

	q_strlcpy (cls.demoname, name, sizeof(cls.demoname));
	cls.demo_offset_start = ftell (cls.demofile);	// qfs_lastload.offset instead?
	cls.demo_file_length = com_filesize;
	cls.demo_hosttime_start	= cls.demo_hosttime_elapsed = 0; // Fill this in ... host_time;
	cls.demo_cltime_start = cls.demo_cltime_elapsed = 0; // Fill this in

	// Title demos get no HUD, no crosshair, no con_notify, FOV 90, viewsize 120
	if (COM_ListMatch (cl_titledemos_list.string, cls.demoname))
	{
		vid.recalc_refdef = true;
		cls.titledemo = true;
	}

	if (!play_as_start_demo)
	{
		CL_Clear_Demos_Queue ();
		key_dest = key_game;
		SCR_BeginLoadingPlaque_Force_NoTransition ();	
	}

	cls.demoplayback = true;
	cls.state = ca_connected;
	cls.forcetrack = 0;

	while ((c = getc(cls.demofile)) != '\n')
		if (c == '-')
			neg = true;
		else
			cls.forcetrack = cls.forcetrack * 10 + (c - '0');

	if (neg)
		cls.forcetrack = -cls.forcetrack;
// ZOID, fscanf is evil
//	fscanf (cls.demofile, "%i\n", &cls.forcetrack);
}

/*
====================
CL_FinishTimeDemo

====================
*/
static void CL_FinishTimeDemo (void)
{
	int		frames;
	float	time;

	cls.timedemo = false;

// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = realtime - cls.td_starttime;
	if (!time)
		time = 1;
	Con_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames/time);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	// Baker: This is a performance benchmark.  No reason to have console up.
	if (key_dest != key_game)
		key_dest = key_game;

	CL_Clear_Demos_Queue (); // timedemo is a very intentional action

	CL_PlayDemo_f ();

	// don't trigger timedemo mode if playdemo fails
	if (!cls.demofile) 
		return;

// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted

	cls.timedemo = true;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;		// get a new message this frame

}

