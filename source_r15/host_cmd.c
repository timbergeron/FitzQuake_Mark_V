/*  File isn't done, more cleanup to go ...

Versus Quakespasm:

1.  sv_cheats 0.  Disallow cheating in coop.
2.  freezeall
3.  Mark V enhanced gamedir switching
4.  MH save game angles fix
5.  MH save game speed fix (removes pointless fflush from loop)
6.  Ability to pause a demo.
7.  qcexec, some handling of demo playback tweaks

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

#include "quakedef.h"

extern cvar_t	pausable;

extern int com_nummissionpacks; //johnfitz




/*
==================
Host_Quit_f
==================
*/



void Host_Quit (void)
{
	CL_Disconnect ();
	Host_ShutdownServer(false);

	Sys_Quit ();
}

// This is the command
void Host_Quit_f (void)
{
	// Force shareware sell screen?
	Host_Quit ();
}





const char* gamedir_type_names[] =
{
	"", // id1 doesn't require one
	"-rogue" ,
	"-hipnotic",
	"-quoth",
};

const char* gamedir_type_read_names[] =
{
	"Standard Quake", // id1 doesn't require one
	"Rogue" ,
	"Hipnotic",
	"Quoth",
};


typedef enum
{
	gamedir_type_id1 = 0,
	gamedir_type_rogue = 1,
	gamedir_type_hipnotic = 2,
	gamedir_type_quoth = 3,
	MAX_GAMEDIR_TYPES =4,
	gamedir_type_shareware = 5,
	gamedir_type_relative_path_not_allowed = 6,
	gamedir_type_not_installed = 7,
} gamedir_type_t;

const char* fail_reason_strings[] =
{
	NULL, // id1 doesn't require one
	"Rogue is not installed.", // 2
	"Hipnotic is not installed.", // 2
	"Quoth is not installed.", // 3
	NULL, // 4
	"You must have the registered version to use modified games", // 5
	"Relative pathnames are not allowed.", // 6
	"Game not installed.", // 7
};

gamedir_type_t Gamedir_TypeNum (qboolean rogue_, qboolean hip_, qboolean quoth_)
{
	if (rogue_) return gamedir_type_rogue;
	if (quoth_) return gamedir_type_quoth;
	if (hipnotic) return gamedir_type_hipnotic;
	return gamedir_type_id1;
}

const char* Gamedir_TypeName (void)
{
	gamedir_type_t typenum = Gamedir_TypeNum(rogue, hipnotic, quoth);
	return gamedir_type_names[typenum];
}

int Game_Type_Flags (const char* gamedir_new, const char* identifier, qboolean *out_rogue, qboolean *out_hipnotic, qboolean *out_quoth)
{
	*out_rogue = false, *out_hipnotic = false, *out_quoth = false;
	if (identifier)
	{
		if (Q_strcasecmp (identifier, "-rogue") == 0)			*out_rogue = true;
		else if (Q_strcasecmp (identifier, "-hipnotic") == 0)	*out_hipnotic = true;
		else if (Q_strcasecmp (identifier, "-quoth") == 0)		*out_hipnotic = *out_quoth = true;
	}	

	if (Q_strcasecmp (gamedir_new, "rogue") == 0 && *out_rogue == false)
		*out_rogue = true; // Force it.
	if (Q_strcasecmp (gamedir_new, "hipnotic") == 0 && *out_hipnotic == false)
		*out_hipnotic = true; // Force it.
	if (Q_strcasecmp (gamedir_new, "quoth") == 0 && *out_quoth == false)
		*out_quoth = true; // Force it.	
	
	return 73834; // This isn't used obviously ;)
}

// returns 0 if everything is ok
int Gamedir_Available_Fails (const char* gamedir_new, qboolean wants_rogue, qboolean wants_hipnotic, qboolean wants_quoth)
{
	if (static_registered == false)
		return gamedir_type_shareware; // 5

	if (strstr(gamedir_new, ".."))
		return gamedir_type_relative_path_not_allowed;

	if (wants_rogue && Sys_FileExists (va ("%s/rogue", com_basedir)) == false)
		return gamedir_type_rogue; // 1

	if (wants_hipnotic && !wants_quoth && Sys_FileExists (va ("%s/hipnotic", com_basedir)) == false)
		return gamedir_type_hipnotic;

	if (wants_quoth && Sys_FileExists (va ("%s/quoth", com_basedir)) == false)
		return gamedir_type_quoth;

	if (Q_strcasecmp (gamedir_new, GAMENAME /* "id1*/) != 0 && Sys_FileExists (va ("%s/%s", com_basedir, gamedir_new)) == false)
		return gamedir_type_not_installed;
		
	return 0; // All clear!
}


// returns 1: change, return 0 no action required (no change), -1 failed doesn't exist

enum {game_change_fail = -1, game_no_change = 0, game_change_success = 1 };
int Host_Gamedir_Change (const char* gamedir_new, const char* new_hud_typestr, qboolean liveswitch, const char** info_string)
{
#pragma message ("Do no allow a user command in console to change gamedir while demo is playing")
	qboolean wants_rogue, wants_hipnotic, wants_quoth;

	qboolean	is_custom		= !!Q_strcasecmp(gamedir_new, GAMENAME); // GAMENAME is "id1"
		
	const char* gamedir_old		= COM_SkipPath(com_gamedir);
	qboolean	gamedir_change	= Q_strcasecmp (gamedir_old, gamedir_new ) != 0;
	
	int			unused			= Game_Type_Flags (gamedir_new, new_hud_typestr, &wants_rogue, &wants_hipnotic, &wants_quoth);
	int			hudstyle_old	= Gamedir_TypeNum (rogue, hipnotic, quoth);
	int			hudstyle_new	= Gamedir_TypeNum (wants_rogue, wants_hipnotic, wants_quoth);
	qboolean	hudstyle_change	= (hudstyle_new != hudstyle_old);

	qboolean	any_change		= (gamedir_change || hudstyle_change);
	int			change_fail		= Gamedir_Available_Fails (gamedir_new, wants_rogue, wants_hipnotic, wants_quoth);
	const char*	change_fail_str	= fail_reason_strings[change_fail];
	qboolean	can_change		= (change_fail == 0);

	const char*	new_hud_str		= gamedir_type_names[hudstyle_new];
	const char*	old_hud_str		= gamedir_type_names[hudstyle_old];
		
	Con_DPrintf ("New: %s Old: %s change %i\n",  gamedir_new, gamedir_old, gamedir_change);
	Con_DPrintf ("New: %s Old: %s change %i\n",  new_hud_str, old_hud_str, hudstyle_change);
	//Con_Printf ("Fail reason %s\n",  new_hud_str, old_hud_str, hudstyle_change);

	if (any_change == false)
	{
		Con_DPrintf ("Gamedir change is no change\n");
		return game_no_change;
	}

	if (can_change == false)
	{
		*info_string = change_fail_str;
		Con_DPrintf ("%s\n", change_fail_str);
		return game_change_fail;
	}

	// Everything ok now ....
	Con_DPrintf ("New game and/or hud style requested\n");
				
	rogue = wants_rogue, hipnotic = wants_hipnotic, quoth = wants_quoth;
	standard_quake = !rogue && !hipnotic;

	// If we aren't receiving this via connected signon switch
	// then kill the server.
	if (liveswitch == false)
	{
		//Kill the server
		CL_Disconnect ();
		Host_ShutdownServer(true);
	}

	//Write config file
	Host_WriteConfiguration ();

	//Kill the extra game if it is loaded. Note: RemoveAllPaths and COM_AddGameDirectory set com_gamedir
	COM_RemoveAllPaths ();

	com_modified = true;

	// add these in the same order as ID do (mission packs always get second-lowest priority)
	if (rogue)				COM_AddGameDirectory (va ("%s/rogue", com_basedir));
	if (hipnotic && !quoth) COM_AddGameDirectory (va ("%s/hipnotic", com_basedir));
	if (quoth)				COM_AddGameDirectory (va ("%s/quoth", com_basedir));
	if (is_custom)			COM_AddGameDirectory (va ("%s/%s", com_basedir, gamedir_new));

	//clear out and reload appropriate data
	Cache_Flush ();

	if (!isDedicated)
	{
		TexMgr_NewGame ();
		Draw_NewGame ();
		R_NewGame ();
	}

	Lists_NewGame ();

	// Return description of current setting
	*info_string = gamedir_new;
	return game_change_success;
}


/*
==================
Host_Game_f
==================
*/

void Host_Game_f (void)
{
	const char* gametypename;
	const char* gamedir_directory;
	const char* hudstyle = NULL;
	const char* feedback_string;
	int result;

	switch (Cmd_Argc())
	{
	case 3:
		hudstyle = Cmd_Argv(2);
		// Falls through ...
	case 2:
		gamedir_directory = Cmd_Argv(1);
		result = Host_Gamedir_Change (Cmd_Argv(1), Cmd_Argv(2), false, &feedback_string);
		
		switch (result)
		{
		case game_change_fail:
			Con_Printf ("%s\n", feedback_string);
			break;
		case game_no_change:
			Con_Printf ("Game already set!\n");
			break;
		case game_change_success:
			Recent_File_NewGame ();	
			Con_Printf("\"game\" changed to \"%s\"\n", feedback_string);
			break;
		}
		break;

	default:
		//Diplay the current gamedir
		gametypename = Gamedir_TypeName ();
		Con_Printf("\"game\" is \"%s%s\"\n", COM_SkipPath(com_gamedir), gametypename[0] ? 
					va(" %s", gametypename) : "" );
		break;
	}

}

/*
=============
Host_Mapname_f -- johnfitz
=============
*/
void Host_Mapname_f (void)
{
	if (sv.active)
	{
		Con_Printf ("\"mapname\" is \"%s\"\n", sv.name);
		return;
	}

	if (cls.state == ca_connected)
	{
		Con_Printf ("\"mapname\" is \"%s\"\n", cl.worldname);
		return;
	}

	Con_Printf ("no map loaded\n");
}

/*
==================
Host_Status_f
==================
*/
void Host_Status_f (void)
{
	client_t	*client;
	int			seconds;
	int			minutes;
	int			hours = 0;
	int			j;
	void		(*print_fn) (const char *fmt, ...) __fp_attribute__((__format__(__printf__,1,2)));

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print_fn = Con_Printf;
	}
	else
		print_fn = SV_ClientPrintf;

	print_fn ("host:    %s\n", Cvar_VariableString ("hostname"));
	print_fn ("version: %4.2f\n", QUAKE_VERSION);
	if (tcpipAvailable)
		print_fn ("tcp/ip:  %s\n", my_tcpip_address);
	print_fn ("map:     %s\n", sv.name);
	print_fn ("players: %i active (%i max)\n\n", net_activeconnections, svs.maxclients);
	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
	{
		if (!client->active)
			continue;
		seconds = (int)(net_time - NET_QSocketGetTime(client->netconnection) );
		minutes = seconds / 60;
		if (minutes)
		{
			seconds -= (minutes * 60);
			hours = minutes / 60;
			if (hours)
				minutes -= (hours * 60);
		}
		else
			hours = 0;
		print_fn ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j+1, client->name, (int)client->edict->v.frags, hours, minutes, seconds);
		print_fn ("   %s\n", NET_QSocketGetAddressString(client->netconnection));
	}
}

/*
==================
Host_QC_Exec

Execute QC commands from the console
==================
*/
void Host_QC_Exec (void)
{
	dfunction_t *f;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (!sv.active)
	{
		Con_Printf ("Not running local game\n");
		return;
	};

	if (!developer.value)
	{
		Con_Printf ("Only available in developer mode\n");
		return;
	};

	f = 0;
	if ((f = ED_FindFunction(Cmd_Argv(1))) != NULL)
	{

		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram ((func_t)(f - pr_functions));
	}
	else
		Con_Printf("bad function\n");

}

/*
============
ED_FindFunction
============
*/
void ED_Listfunctions_f (void)
{
	if (sv.active)
	{
		int				i;
		Con_Printf ("QuakeC Functions:\n");
		for (i = 0 ; i < progs->numfunctions ; i++)
		{
			dfunction_t	*func = &pr_functions[i];
			const char* func_name = PR_GetString(func->s_name);

			if ('A' <= func_name[0] && func_name[0] <= 'Z')
				Con_Printf ("%i: %s\n", i, PR_GetString(func->s_name) );
		}

	} else Con_Printf ("No active server\n");
}



/*
==================
Host_God_f

Sets client to godmode
==================
*/
void Host_God_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	if (sv.disallow_cheats && !host_client->privileged)
	{
		SV_ClientPrintf ("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	//johnfitz -- allow user to explicitly set god mode to on or off
	switch (Cmd_Argc())
	{
	case 1:
		sv_player->v.flags = (int)sv_player->v.flags ^ FL_GODMODE;
		if (!((int)sv_player->v.flags & FL_GODMODE) )
			SV_ClientPrintf ("godmode OFF\n");
		else
			SV_ClientPrintf ("godmode ON\n");
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			sv_player->v.flags = (int)sv_player->v.flags | FL_GODMODE;
			SV_ClientPrintf ("godmode ON\n");
		}
		else
		{
			sv_player->v.flags = (int)sv_player->v.flags & ~FL_GODMODE;
			SV_ClientPrintf ("godmode OFF\n");
		}
		break;
	default:
		Con_Printf("god [value] : toggle god mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

/*
==================
Host_Notarget_f
==================
*/
void Host_Notarget_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	if (sv.disallow_cheats && !host_client->privileged)
	{
		SV_ClientPrintf ("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	//johnfitz -- allow user to explicitly set notarget to on or off
	switch (Cmd_Argc())
	{
	case 1:
		sv_player->v.flags = (int)sv_player->v.flags ^ FL_NOTARGET;
		if (!((int)sv_player->v.flags & FL_NOTARGET) )
			SV_ClientPrintf ("notarget OFF\n");
		else
			SV_ClientPrintf ("notarget ON\n");
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			sv_player->v.flags = (int)sv_player->v.flags | FL_NOTARGET;
			SV_ClientPrintf ("notarget ON\n");
		}
		else
		{
			sv_player->v.flags = (int)sv_player->v.flags & ~FL_NOTARGET;
			SV_ClientPrintf ("notarget OFF\n");
		}
		break;
	default:
		Con_Printf("notarget [value] : toggle notarget mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}


/*
==================
Host_Noclip_f
==================
*/
void Host_Noclip_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	if (sv.disallow_cheats && !host_client->privileged)
	{
		SV_ClientPrintf ("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	//johnfitz -- allow user to explicitly set noclip to on or off
	switch (Cmd_Argc())
	{
	case 1:
		if (sv_player->v.movetype != MOVETYPE_NOCLIP)
		{
			cl.noclip_anglehack = true;
			sv_player->v.movetype = MOVETYPE_NOCLIP;
			SV_ClientPrintf ("noclip ON\n");
		}
		else
		{
			cl.noclip_anglehack = false;
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("noclip OFF\n");
		}
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			cl.noclip_anglehack = true;
			sv_player->v.movetype = MOVETYPE_NOCLIP;
			SV_ClientPrintf ("noclip ON\n");
		}
		else
		{
			cl.noclip_anglehack = false;
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("noclip OFF\n");
		}
		break;
	default:
		Con_Printf("noclip [value] : toggle noclip mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

/*
==================
Host_Fly_f

Sets client to flymode
==================
*/
void Host_Fly_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	if (sv.disallow_cheats && !host_client->privileged)
	{
		SV_ClientPrintf ("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	//johnfitz -- allow user to explicitly set noclip to on or off
	switch (Cmd_Argc())
	{
	case 1:
		if (sv_player->v.movetype != MOVETYPE_FLY)
		{
			sv_player->v.movetype = MOVETYPE_FLY;
			SV_ClientPrintf ("flymode ON\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("flymode OFF\n");
		}
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			sv_player->v.movetype = MOVETYPE_FLY;
			SV_ClientPrintf ("flymode ON\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("flymode OFF\n");
		}
		break;
	default:
		Con_Printf("fly [value] : toggle fly mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

void SV_Freezeall_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	if (sv.disallow_cheats && !host_client->privileged)
	{
		SV_ClientPrintf ("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	switch (Cmd_Argc())
	{
	case 1:
		sv.frozen = !sv.frozen;

		if (sv.frozen)
			SV_ClientPrintf ("freeze mode ON\n");
		else
			SV_ClientPrintf ("freeze mode OFF\n");

		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			sv.frozen = true;
			SV_ClientPrintf ("freeze mode ON\n");
		}
		else
		{
			sv.frozen = false;
			SV_ClientPrintf ("freeze mode OFF\n");
		}
		break;
	default:
		Con_Printf("freezeall [value] : toggle freeze mode. values: 0 = off, 1 = on\n");
		break;
	}

}

/*
==================
Host_Ping_f

==================
*/
void Host_Ping_f (void)
{
	int		i, j;
	float	total;
	client_t	*client;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	SV_ClientPrintf ("Client ping times:\n");
	for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
	{
		if (!client->active)
			continue;
		total = 0;
		for (j=0 ; j<NUM_PING_TIMES ; j++)
			total+=client->ping_times[j];
		total /= NUM_PING_TIMES;
		SV_ClientPrintf ("%4i %s\n", (int)(total*1000), client->name);
	}
}


void Host_Changelevel_Required_Msg (cvar_t* var)
{
	if (host_initialized)
		Con_Printf ("%s change takes effect on map restart/change.\n", var->name);
}


/*
===============================================================================

SERVER TRANSITIONS

===============================================================================
*/


/*
======================
Host_Map_f

handle a
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
void Host_Map_f (void)
{
	int		i;
	char	name[MAX_QPATH];

	// Quakespasm: map with no parameter says name
	if (Cmd_Argc() < 2)	//no map name given
	{
		if (cls.state == ca_dedicated)
		{
			if (sv.active)
				Con_Printf ("Current map: %s\n", sv.name);
			else
				Con_Printf ("Server not active\n");
		}
		else if (cls.state == ca_connected)
		{
			Con_Printf ("Current map: %s ( %s )\n", cl.levelname, cl.worldname);
		}
		else
		{
			Con_Printf ("map <levelname>: start a new server\n");
		}
		return;
	}


	if (cmd_source != src_command)
		return;

	CL_Clear_Demos_Queue (); // timedemo is a very intentional action

	CL_Disconnect ();
	Host_ShutdownServer(false);

	key_dest = key_game;			// remove console or menu
	SCR_BeginLoadingPlaque_Force_NoTransition ();

	cls.mapstring[0] = 0;
	for (i=0 ; i<Cmd_Argc() ; i++)
	{
		strcat (cls.mapstring, Cmd_Argv(i));
		strcat (cls.mapstring, " ");
	}
	strcat (cls.mapstring, "\n");

	svs.serverflags = 0;			// haven't completed an episode yet
	q_strlcpy (name, Cmd_Argv(1), sizeof(name));
	SV_SpawnServer (name);
	if (!sv.active)
		return;

	if (cls.state != ca_dedicated)
	{
		memset (cls.spawnparms, 0, sizeof(cls.spawnparms));
		for (i=2 ; i<Cmd_Argc() ; i++)
		{
			q_strlcat (cls.spawnparms, Cmd_Argv(i), sizeof(cls.spawnparms));
			q_strlcat (cls.spawnparms, " ", sizeof(cls.spawnparms));
		}

		Cmd_ExecuteString ("connect local", src_command);
	}
}

/*
==================
Host_Changelevel_f

Goes to a new map, taking all clients along
==================
*/
void Host_Changelevel_f (void)
{
	char	level[MAX_QPATH];
	int		i; //johnfitz

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("changelevel <levelname> : continue game on a new level\n");
		return;
	}
	if (!sv.active || cls.demoplayback)
	{
		Con_Printf ("Only the server may changelevel\n");
		return;
	}

	//johnfitz -- check for client having map before anything else
	q_snprintf (level, sizeof(level), "maps/%s.bsp", Cmd_Argv(1));
	if (COM_OpenFile (level, &i) == -1)
		Host_Error ("cannot find map %s", level);
	//johnfitz

	SV_SaveSpawnparms ();
	q_strlcpy (level, Cmd_Argv(1), sizeof(level));
	SV_SpawnServer (level);
}

/*
==================
Host_Restart_f

Restarts the current server for a dead player
==================
*/
void Host_Restart_f (void)
{
	char	mapname[MAX_QPATH];

	if (cls.demoplayback || !sv.active)
		return;

	if (cmd_source != src_command)
		return;
	q_strlcpy (mapname, sv.name, sizeof(mapname));	// mapname gets cleared in spawnserver
	SV_SpawnServer (mapname);
}

/*
==================
Host_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
==================
*/
// This can have 2 different scenarios.  Entry might be at 0, in that case 4 should clear plaque
//If signon is 4, that is death or changelevel.  What do we do?  Clear immediately?  But in 0 case, don't
void Host_Reconnect_f (void)
{
	// Consider stopping sound here?
	SCR_BeginLoadingPlaque_Force_Transition ();
	if (cls.demoplayback)
	{
		Con_DPrintf("Demo playing; ignoring reconnect\n");
		// Baker: Wrong solution but at least we can see demo
		SCR_EndLoadingPlaque (); // reconnect happens before signon reply #4?
		return;
	}	
	cls.signon = 0;		// need new connection messages
}

/*
=====================
Host_Connect_f

User command to connect to server
=====================
*/
void Host_Connect_f (void)
{
	char	name[MAX_QPATH];

	cls.demonum = -1;		// stop demo loop in case this fails
	if (cls.demoplayback)
	{
		CL_StopPlayback ();

		CL_Clear_Demos_Queue (); // timedemo is a very intentional action
		CL_Disconnect ();
	}
	key_dest = key_game;
	SCR_BeginLoadingPlaque_Force_NoTransition ();

	q_strlcpy (name, Cmd_Argv(1), sizeof(name));
	CL_EstablishConnection (name);
	Host_Reconnect_f ();
}

keyvalue_t hintnames[MAX_NUM_HINTS] =
{
	{ "game", hint_game },
	{ "skill", hint_skill },
};


/*
===============================================================================

LOAD / SAVE GAME

===============================================================================
*/

#define	SAVEGAME_VERSION	5

/*
===============
Host_SavegameComment

Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current
===============
*/
void Host_SavegameComment (char *text)
{
	int		i;
	char	kills[20];

	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
		text[i] = ' ';
	memcpy (text, cl.levelname, q_min(strlen(cl.levelname),22)); //johnfitz -- only copy 22 chars.
	sprintf (kills,"kills:%3i/%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	memcpy (text+22, kills, strlen(kills));
// convert space to _ to make stdio happy
	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
	{
		if (text[i] == ' ')
			text[i] = '_';
	}
	text[SAVEGAME_COMMENT_LENGTH] = '\0';
}


/*
===============
Host_Savegame_f
===============
*/
void Host_Savegame_f (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;
	int		i;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];

	if (cmd_source != src_command)
		return;

	if (!sv.active)
	{
		Con_Printf ("Not playing a local game.\n");
		return;
	}

	if (cl.intermission)
	{
		Con_Printf ("Can't save in intermission.\n");
		return;
	}

	if (svs.maxclients != 1)
	{
		Con_Printf ("Can't save multiplayer games.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("save <savename> : save a game\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active && (svs.clients[i].edict->v.health <= 0) )
		{
			Con_Printf ("Can't savegame with a dead player\n");
			return;
		}
	}

	q_snprintf (name, sizeof(name), "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_ForceExtension (name, ".sav", sizeof(name));

	Con_Printf ("Saving game to %s...\n", name);
	f = FS_fopen_write (name, "w");
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	fprintf (f, "%i\n", SAVEGAME_VERSION);
	Host_SavegameComment (comment);
	fprintf (f, "%s\n", comment);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		fprintf (f, "%f\n", svs.clients->spawn_parms[i]);
	fprintf (f, "%d\n", sv.current_skill);
	fprintf (f, "%s\n", sv.name);
	fprintf (f, "%f\n",sv.time);

// write the light styles

	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (sv.lightstyles[i])
			fprintf (f, "%s\n", sv.lightstyles[i]);
		else
			fprintf (f,"m\n");
	}


	ED_WriteGlobals (f);
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ED_Write (f, EDICT_NUM(i));
		// fflush (f); // Baker: This makes save games slow as hell.  Fix from M
	}
	FS_fclose (f);
	Lists_Update_Savelist ();
	Recent_File_Set_FullPath (name);
	Con_Printf ("done.\n");
}


/*
===============
Host_Loadgame_f
===============
*/
void Host_Loadgame_f (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;
	char	mapname[MAX_QPATH];
	float	time, tfloat;
	char	str[32768];
	const char *start;
	int		i, r;
	edict_t	*ent;
	int		entnum;
	int		version;
	float			spawn_parms[NUM_SPAWN_PARMS];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("load <savename> : load a game\n");
		return;
	}

	cls.demonum = -1;		// stop demo loop in case this fails

	q_snprintf (name, sizeof(name), "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (name, ".sav", sizeof(name));

// we can't call SCR_BeginLoadingPlaque, because too much stack space has
// been used.  The menu calls it before stuffing loadgame command
//	SCR_BeginLoadingPlaque ();

	Con_Printf ("Loading game from %s...\n", name);
	f = FS_fopen_read (name, "r");
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	fscanf (f, "%i\n", &version);
	if (version != SAVEGAME_VERSION)
	{
		FS_fclose (f);
		Con_Printf ("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}
	fscanf (f, "%s\n", str);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		fscanf (f, "%f\n", &spawn_parms[i]);
// this silliness is so we can load 1.06 save files, which have float skill values
	fscanf (f, "%f\n", &tfloat);
	sv.current_skill = (int)(tfloat + 0.1);
	Cvar_SetValue ("skill", (float)sv.current_skill);

	fscanf (f, "%s\n",mapname);
	fscanf (f, "%f\n",&time);

	CL_Disconnect_f ();

	SV_SpawnServer (mapname);

	if (!sv.active)
	{
		FS_fclose (f);
		Con_Printf ("Couldn't load map\n");
		return;
	}
	sv.paused = true;		// pause until all clients connect
	sv.loadgame = true;

// load the light styles

	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		fscanf (f, "%s\n", str);
		sv.lightstyles[i] = (const char *)Hunk_Strdup (str, "lightstyles");
	}

// load the edicts out of the savegame file
	entnum = -1;		// -1 is the globals
	while (!feof(f))
	{
		for (i = 0 ; i < (int) sizeof(str) - 1 ; i ++)
		{
			r = fgetc (f);
			if (r == EOF || !r)
				break;
			str[i] = r;
			if (r == '}')
			{
				i++;
				break;
			}
		}
		if (i ==  (int) sizeof(str)-1)
		{
			FS_fclose (f);
			Sys_Error ("Loadgame buffer overflow");
		}
		str[i] = 0;
		start = str;
		start = COM_Parse(str);
		if (!com_token[0])
			break;		// end of file
		if (strcmp(com_token,"{"))
		{
			FS_fclose (f);
			Sys_Error ("First token isn't a brace");
		}

		if (entnum == -1)
		{	// parse the global vars
			ED_ParseGlobals (start);
		}
		else
		{	// parse an edict

			ent = EDICT_NUM(entnum);
			memset (&ent->v, 0, progs->entityfields * 4);
			ent->free = false;
			ED_ParseEdict (start, ent);

		// link it into the bsp tree
			if (!ent->free)
				SV_LinkEdict (ent, false);
		}

		entnum++;
	}

	sv.num_edicts = entnum;
	sv.time = time;

	FS_fclose (f);

	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];

	if (cls.state != ca_dedicated)
	{
		CL_EstablishConnection ("local");
		Host_Reconnect_f ();
	}
}

//============================================================================

/*
======================
Host_Name_f
======================
*/
void Host_Name_f (void)
{
	char	newName[32];

	if (Cmd_Argc () == 1)
	{
		Con_Printf ("\"name\" is \"%s\"\n", cl_name.string);
		return;
	}
	if (Cmd_Argc () == 2)
		q_strlcpy(newName, Cmd_Argv(1), sizeof(newName));
	else
		q_strlcpy(newName, Cmd_Args(), sizeof(newName));
	newName[15] = 0;	// client_t structure actually says name[32].

	if (cmd_source == src_command)
	{
		if (Q_strcmp(cl_name.string, newName) == 0)
			return;
		Cvar_Set ("_cl_name", newName);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	if (host_client->name[0] && strcmp(host_client->name, "unconnected") )
	{
		if (Q_strcmp(host_client->name, newName) != 0)
			Con_Printf ("%s renamed to %s\n", host_client->name, newName);
	}
	Q_strcpy (host_client->name, newName);
	host_client->edict->v.netname = PR_SetEngineString(host_client->name);

// send notification to all clients

	MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString (&sv.reliable_datagram, host_client->name);
}

void Host_Say(qboolean teamonly)
{
	int		j;
	client_t *client;
	client_t *save;
	char	*p;
	unsigned char	text[64];
	qboolean	fromServer = false;

	if (cmd_source == src_command)
	{
		if (cls.state == ca_dedicated)
		{
			fromServer = true;
			teamonly = false;
		}
		else
		{
			Cmd_ForwardToServer ();
			return;
		}
	}

	if (Cmd_Argc () < 2)
		return;

	save = host_client;

	p = (char*)Cmd_Args(); // evil
// remove quotes if present
	if (*p == '\"')
	{
		p++;
		p[Q_strlen(p)-1] = 0;
	}
// turn on color set 1
	if (!fromServer)
		q_snprintf (text, sizeof(text), "\001%s: ", save->name);
	else
		q_snprintf (text, sizeof(text), "\001<%s> ", hostname.string);

	j = sizeof(text) - 2 - Q_strlen(text);  // -2 for /n and null terminator
	if (Q_strlen(p) > j)
		p[j] = 0;

	strcat (text, p);
	strcat (text, "\n");

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client || !client->active || !client->spawned)
			continue;
		if (teamplay.value && teamonly && client->edict->v.team != save->edict->v.team)
			continue;
		host_client = client;
		SV_ClientPrintf("%s", text);
	}
	host_client = save;

	Sys_Printf("%s", &text[1]);
}


void Host_Say_f(void)
{
	Host_Say(false);
}


void Host_Say_Team_f(void)
{
	Host_Say(true);
}


void Host_Tell_f(void)
{
	int		j;
	client_t *client;
	client_t *save;
	char	*p;
	char	text[64];

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (Cmd_Argc () < 3)
		return;

	Q_strcpy(text, host_client->name);
	Q_strcat(text, ": ");

	p = (char*)Cmd_Args(); // evil

// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[Q_strlen(p)-1] = 0;
	}

// check length & truncate if necessary
	j = sizeof(text) - 2 - Q_strlen(text);  // -2 for /n and null terminator
	if (Q_strlen(p) > j)
		p[j] = 0;

	strcat (text, p);
	strcat (text, "\n");

	save = host_client;
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client->active || !client->spawned)
			continue;
		if (Q_strcasecmp(client->name, Cmd_Argv(1)))
			continue;
		host_client = client;
		SV_ClientPrintf("%s", text);
		break;
	}
	host_client = save;
}


/*
==================
Host_Color_f
==================
*/
void Host_Color_f(void)
{
	int		top, bottom;
	int		playercolor;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"color\" is \"%i %i\"\n", ((int)cl_color.value) >> 4, ((int)cl_color.value) & 0x0f);
		Con_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
	else
	{
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	playercolor = top*16 + bottom;

	if (cmd_source == src_command)
	{
		Cvar_SetValue ("_cl_color", playercolor);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	host_client->colors = playercolor;
	host_client->edict->v.team = bottom + 1;

// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
}

/*
==================
Host_Kill_f
==================
*/
void Host_Kill_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (sv_player->v.health <= 0)
	{
		SV_ClientPrintf ("Can't suicide -- allready dead!\n");
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ExecuteProgram (pr_global_struct->ClientKill);
}


/*
==================
Host_Pause_f
==================
*/
void Host_Pause_f (void)
{
	// If playing back a demo, we pause now
	if (cls.demoplayback && cls.demonum == -1) // Don't allow startdemos to be paused
		cl.paused ^= 2;		// to handle demo-pause
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (!pausable.value)
		SV_ClientPrintf ("Pause not allowed.\n");
	else
	{
		// If not playing back a demo, we pause here 
		if (!cls.demoplayback) // Don't allow startdemos to be paused
			cl.paused ^= 2;		// to handle demo-pause
		sv.paused ^= 1;

		if (sv.paused)
		{
			SV_BroadcastPrintf ("%s paused the game\n", PR_GetString( sv_player->v.netname));
		}
		else
		{
			SV_BroadcastPrintf ("%s unpaused the game\n",PR_GetString( sv_player->v.netname));
		}

	// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_setpause);
		MSG_WriteByte (&sv.reliable_datagram, sv.paused);
	}
}

//===========================================================================


/*
==================
Host_PreSpawn_f
==================
*/
void Host_PreSpawn_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Printf ("prespawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf ("prespawn not valid -- allready spawned\n");
		return;
	}

	SZ_Write (&host_client->message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 2);
	host_client->sendsignon = true;
}

/*
==================
Host_Spawn_f
==================
*/
void Host_Spawn_f (void)
{
	int		i;
	client_t	*client;
	edict_t	*ent;

	if (cmd_source == src_command)
	{
		Con_Printf ("spawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf ("Spawn not valid -- allready spawned\n");
		return;
	}

// run the entrance script
	if (sv.loadgame)
	{	// loaded games are fully inited allready
		// if this is the last client to be connected, unpause
		sv.paused = false;
	}
	else
	{
		// set up the edict
		ent = host_client->edict;

		memset (&ent->v, 0, progs->entityfields * 4);
		ent->v.colormap = NUM_FOR_EDICT(ent);
		ent->v.team = (host_client->colors & 15) + 1;
		ent->v.netname = PR_SetEngineString(host_client->name);

		// copy spawn parms out of the client_t
		for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];
		// call the spawn function
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram (pr_global_struct->ClientConnect);

		if ((Sys_DoubleTime() - NET_QSocketGetTime(host_client->netconnection) ) <= sv.time)
			Sys_Printf ("%s entered the game\n", host_client->name);

		PR_ExecuteProgram (pr_global_struct->PutClientInServer);
	}


// send all current names, colors, and frag counts
	SZ_Clear (&host_client->message);

// send time of update
	MSG_WriteByte (&host_client->message, svc_time);
	MSG_WriteFloat (&host_client->message, sv.time);

	for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
	{
		MSG_WriteByte (&host_client->message, svc_updatename);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteString (&host_client->message, client->name);
		MSG_WriteByte (&host_client->message, svc_updatefrags);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteShort (&host_client->message, client->old_frags);
		MSG_WriteByte (&host_client->message, svc_updatecolors);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteByte (&host_client->message, client->colors);
	}

// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		MSG_WriteByte (&host_client->message, svc_lightstyle);
		MSG_WriteByte (&host_client->message, (char)i);
		MSG_WriteString (&host_client->message, sv.lightstyles[i]);
	}

//
// send some stats
//
	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_monsters);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_SECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->found_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_MONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->killed_monsters);

//
// send a fixangle
// Never send a roll angle, because savegames can catch the server
// in a state where it is expecting the client to correct the angle
// and it won't happen if the game was just loaded, so you wind up
// with a permanent head tilt
	ent = EDICT_NUM( 1 + (host_client - svs.clients) );
	MSG_WriteByte (&host_client->message, svc_setangle);

	if (sv.loadgame) // MH load game angles fix ...
	{
		MSG_WriteAngle (&host_client->message, ent->v.v_angle[0]);
		MSG_WriteAngle (&host_client->message, ent->v.v_angle[1]);
		MSG_WriteAngle (&host_client->message, 0 );
	}
	else
	{
		MSG_WriteAngle (&host_client->message, ent->v.angles[0] );
		MSG_WriteAngle (&host_client->message, ent->v.angles[1] );
		MSG_WriteAngle (&host_client->message, 0 );
	}


	SV_WriteClientdataToMessage (sv_player, &host_client->message);

	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 3);
	host_client->sendsignon = true;
}

/*
==================
Host_Begin_f
==================
*/
void Host_Begin_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Printf ("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = true;
}

//===========================================================================


/*
==================
Host_Kick_f

Kicks a user off of the server
==================
*/
void Host_Kick_f (void)
{
	const char		*who;
	const char		*message = NULL;
	client_t	*save;
	int			i;
	qboolean	byNumber = false;

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
	}
	else if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	save = host_client;

	if (Cmd_Argc() > 2 && Q_strcmp(Cmd_Argv(1), "#") == 0)
	{
		i = Q_atof(Cmd_Argv(2)) - 1;
		if (i < 0 || i >= svs.maxclients)
			return;
		if (!svs.clients[i].active)
			return;
		host_client = &svs.clients[i];
		byNumber = true;
	}
	else
	{
		for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
		{
			if (!host_client->active)
				continue;
			if (Q_strcasecmp(host_client->name, Cmd_Argv(1)) == 0)
				break;
		}
	}

	if (i < svs.maxclients)
	{
		if (cmd_source == src_command)
			if (cls.state == ca_dedicated)
				who = "Console";
			else
				who = cl_name.string;
		else
			who = save->name;

		// can't kick yourself!
		if (host_client == save)
			return;

		if (Cmd_Argc() > 2)
		{
			message = COM_Parse(Cmd_Args());
			if (byNumber)
			{
				message++;							// skip the #
				while (*message == ' ')				// skip white space
					message++;
				message += strlen(Cmd_Argv(2));	// skip the number
			}
			while (*message && *message == ' ')
				message++;
		}
		if (message)
			SV_ClientPrintf ("Kicked by %s: %s\n", who, message);
		else
			SV_ClientPrintf ("Kicked by %s\n", who);
		SV_DropClient (false);
	}

	host_client = save;
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/

/*
==================
Host_Give_f
==================
*/
void Host_Give_f (void)
{
	const char	*t;
	int		v;
	eval_t	*val;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;
#pragma message ("sv_cheats disable give ...")
	if (Cmd_Argc() == 1)
	{
		// Help
		Con_Printf ("usage: give <item> <quantity>\n");
		Con_Printf (" 1-8 = weapon, a = armor\n");
		Con_Printf (" h = health, ks or kg = key\n");
		Con_Printf (" s,n,r,c = ammo, q1-q4 = sigil\n");
		return;
	}

	t = Cmd_Argv(1);
	v = atoi (Cmd_Argv(2));

	switch (t[0])
	{
#if 1
	case 'k':
		if (t[1] && t[1] == 'g')
			sv_player->v.items = (int)sv_player->v.items | IT_KEY2;
		else sv_player->v.items = (int)sv_player->v.items | IT_KEY1;
		break;

	case 'q':
		if (t[0])
		{
			switch (t[1])
			{
			case '1': sv_player->v.items = (int)sv_player->v.items | IT_SIGIL1; break;
			case '2': sv_player->v.items = (int)sv_player->v.items | IT_SIGIL2; break;
			case '3': sv_player->v.items = (int)sv_player->v.items | IT_SIGIL3; break;
			case '4': sv_player->v.items = (int)sv_player->v.items | IT_SIGIL4; break;
			default:
				break;
			}
		}
		break;

#endif
   case '0':
   case '1':
   case '2':
   case '3':
   case '4':
   case '5':
   case '6':
   case '7':
   case '8':
   case '9':
      // MED 01/04/97 added hipnotic give stuff
      if (hipnotic)
      {
         if (t[0] == '6')
         {
            if (t[1] == 'a')
               sv_player->v.items = (int)sv_player->v.items | HIT_PROXIMITY_GUN;
            else
               sv_player->v.items = (int)sv_player->v.items | IT_GRENADE_LAUNCHER;
         }
         else if (t[0] == '9')
            sv_player->v.items = (int)sv_player->v.items | HIT_LASER_CANNON;
         else if (t[0] == '0')
            sv_player->v.items = (int)sv_player->v.items | HIT_MJOLNIR;
         else if (t[0] >= '2')
            sv_player->v.items = (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
      }
      else
      {
         if (t[0] >= '2')
            sv_player->v.items = (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
      }
		break;

    case 's':
		if (rogue)
		{
			val = GETEDICTFIELDVALUE(sv_player, eval_ammo_shells1);
		    if (val)
			    val->_float = v;
		}
        sv_player->v.ammo_shells = v;
        break;

    case 'n':
		if (rogue)
		{
			val = GETEDICTFIELDVALUE(sv_player, eval_ammo_nails1);
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_nails = v;
			}
		}
		else
		{
			sv_player->v.ammo_nails = v;
		}
        break;

    case 'l':
		if (rogue)
		{
			val = GETEDICTFIELDVALUE(sv_player, eval_ammo_lava_nails);
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_nails = v;
			}
		}
        break;

    case 'r':
		if (rogue)
		{
			val = GETEDICTFIELDVALUE(sv_player, eval_ammo_rockets1);
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_rockets = v;
			}
		}
		else
		{
			sv_player->v.ammo_rockets = v;
		}
        break;

    case 'm':
		if (rogue)
		{
			val = GETEDICTFIELDVALUE(sv_player, eval_ammo_multi_rockets);
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_rockets = v;
			}
		}
        break;

    case 'h':
        sv_player->v.health = v;
        break;

    case 'c':
		if (rogue)
		{
			val = GETEDICTFIELDVALUE (sv_player, eval_ammo_cells1);
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_cells = v;
			}
		}
		else
		{
			sv_player->v.ammo_cells = v;
		}
        break;

    case 'p':
		if (rogue)
		{
			val = GETEDICTFIELDVALUE (sv_player, eval_ammo_plasma);
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_cells = v;
			}
		}
        break;

	//johnfitz -- give armour
    case 'a':
		if (v > 150)
		{
		    sv_player->v.armortype = 0.8;
	        sv_player->v.armorvalue = v;
			sv_player->v.items = sv_player->v.items - 
				((int)(sv_player->v.items) & (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + 
				IT_ARMOR3;
		}
		else if (v > 100)
		{
		    sv_player->v.armortype = 0.6;
	        sv_player->v.armorvalue = v;
			sv_player->v.items = sv_player->v.items - 
				((int)(sv_player->v.items) & (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + 
					IT_ARMOR2;
		}
		else if (v >= 0)
		{
		    sv_player->v.armortype = 0.3;
	        sv_player->v.armorvalue = v;
			sv_player->v.items = sv_player->v.items -
				 ((int)(sv_player->v.items) & (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + 
				IT_ARMOR1;
		}
		break;
	//johnfitz
    }

	//johnfitz -- update currentammo to match new ammo (so statusbar updates correctly)
	switch ((int)(sv_player->v.weapon))
	{
	case IT_SHOTGUN:
	case IT_SUPER_SHOTGUN:
		sv_player->v.currentammo = sv_player->v.ammo_shells;
		break;
	case IT_NAILGUN:
	case IT_SUPER_NAILGUN:
	case RIT_LAVA_SUPER_NAILGUN:
		sv_player->v.currentammo = sv_player->v.ammo_nails;
		break;
	case IT_GRENADE_LAUNCHER:
	case IT_ROCKET_LAUNCHER:
	case RIT_MULTI_GRENADE:
	case RIT_MULTI_ROCKET:
		sv_player->v.currentammo = sv_player->v.ammo_rockets;
		break;
	case IT_LIGHTNING:
	case HIT_LASER_CANNON:
	case HIT_MJOLNIR:
		sv_player->v.currentammo = sv_player->v.ammo_cells;
		break;
	case RIT_LAVA_NAILGUN: //same as IT_AXE
		if (rogue)
			sv_player->v.currentammo = sv_player->v.ammo_nails;
		break;
	case RIT_PLASMA_GUN: //same as HIT_PROXIMITY_GUN
		if (rogue)
			sv_player->v.currentammo = sv_player->v.ammo_cells;
		if (hipnotic)
			sv_player->v.currentammo = sv_player->v.ammo_rockets;
		break;
	}
	//johnfitz
}

edict_t	*FindViewthing (void)
{
	int		i;
	edict_t	*e;

	for (i=0 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		if ( !strcmp (PR_GetString( e->v.classname), "viewthing") )
			return e;
	}
	Con_Printf ("No viewthing on map\n");
	return NULL;
}

/*
==================
Host_Viewmodel_f
==================
*/
void Host_Viewmodel_f (void)
{
	edict_t	*e;
	qmodel_t	*m;

	e = FindViewthing ();
	if (!e)
		return;

	m = Mod_ForName (Cmd_Argv(1), false);
	if (!m)
	{
		Con_Printf ("Can't load %s\n", Cmd_Argv(1));
		return;
	}

	e->v.frame = 0;
	cl.model_precache[(int)e->v.modelindex] = m;
}

/*
==================
Host_Viewframe_f
==================
*/
void Host_Viewframe_f (void)
{
	edict_t	*e;
	int		f;
	qmodel_t	*m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int)e->v.modelindex];

	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes-1;

	e->v.frame = f;
}


void PrintFrameName (qmodel_t *m, int frame)
{
	aliashdr_t 			*hdr;
	maliasframedesc_t	*pframedesc;

	hdr = (aliashdr_t *)Mod_Extradata (m);
	if (!hdr)
		return;
	pframedesc = &hdr->frames[frame];

	Con_Printf ("frame %i: %s\n", frame, pframedesc->name);
}

/*
==================
Host_Viewnext_f
==================
*/
void Host_Viewnext_f (void)
{
	edict_t	*e;
	qmodel_t	*m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame + 1;
	if (e->v.frame >= m->numframes)
		e->v.frame = m->numframes - 1;

	PrintFrameName (m, e->v.frame);
}

/*
==================
Host_Viewprev_f
==================
*/
void Host_Viewprev_f (void)
{
	edict_t	*e;
	qmodel_t	*m;

	e = FindViewthing ();
	if (!e)
		return;

	m = cl.model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame - 1;
	if (e->v.frame < 0)
		e->v.frame = 0;

	PrintFrameName (m, e->v.frame);
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/


/*
==================
Host_Startdemos_f
==================
*/
void Host_Startdemos_f (void)
{
	int		i, c;

	if (cls.state == ca_dedicated)
	{
		if (!sv.active)
			Cbuf_AddText ("map start\n");
		return;
	}

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		Con_Printf ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}

	if (Cmd_Argc() != 1)
	{
		cls.demonum = 0;

	}
	Con_Printf ("%i demo(s) in loop\n", c);

	for (i=1 ; i<c+1 ; i++)
		q_strlcpy (cls.demos[i-1], Cmd_Argv(i), sizeof(cls.demos[0]));

	if (!sv.active && cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
	{
		cls.demonum = 0;
		CL_NextDemo ();
	}
	else
	{
		cls.demonum = -1;
	}
}



/*
==================
Host_Stopdemo_f

Return to looping demos
==================
*/
void Host_Stopdemo_f (void)
{
	if (cls.state == ca_dedicated)
		return;

	if (!cls.demoplayback)
		return;
	CL_StopPlayback ();

// Baker :Since this is an intentional user action,
// clear the demos queue.
	CL_Clear_Demos_Queue ();

	CL_Disconnect ();
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands (void)
{
#if 0
	Cmd_AddCommand ("gamezip", GameZip_f);
	Cmd_AddCommand ("dir", Dir_f);
	Cmd_AddCommand ("cache", Cache_Dir_f);
#endif
	Cmd_AddCommand ("game", Host_Game_f); //johnfitz
	Cmd_AddCommand ("mapname", Host_Mapname_f); //johnfitz
	Cmd_AddCommand ("status", Host_Status_f);
	Cmd_AddCommand ("quit", Host_Quit_f);
	Cmd_AddCommand ("god", Host_God_f);
	Cmd_AddCommand ("notarget", Host_Notarget_f);
	Cmd_AddCommand ("fly", Host_Fly_f);
	Cmd_AddCommand ("map", Host_Map_f);
	Cmd_AddCommand ("restart", Host_Restart_f);
	Cmd_AddCommand ("changelevel", Host_Changelevel_f);
	Cmd_AddCommand ("connect", Host_Connect_f);
	Cmd_AddCommand ("reconnect", Host_Reconnect_f);
	Cmd_AddCommand ("name", Host_Name_f);
	Cmd_AddCommand ("noclip", Host_Noclip_f);

	Cmd_AddCommand ("say", Host_Say_f);
	Cmd_AddCommand ("say_team", Host_Say_Team_f);
	Cmd_AddCommand ("tell", Host_Tell_f);
	Cmd_AddCommand ("color", Host_Color_f);
	Cmd_AddCommand ("kill", Host_Kill_f);
	Cmd_AddCommand ("pause", Host_Pause_f);
	Cmd_AddCommand ("spawn", Host_Spawn_f);
	Cmd_AddCommand ("begin", Host_Begin_f);
	Cmd_AddCommand ("prespawn", Host_PreSpawn_f);
	Cmd_AddCommand ("kick", Host_Kick_f);
	Cmd_AddCommand ("ping", Host_Ping_f);
	Cmd_AddCommand ("load", Host_Loadgame_f);
	Cmd_AddCommand ("save", Host_Savegame_f);
	Cmd_AddCommand ("give", Host_Give_f);

	Cmd_AddCommand ("startdemos", Host_Startdemos_f);

// Baker --- I've never once used the command nor seen it used by a mod. Going to make it list demos.
	//	Cmd_AddCommand ("demos", Host_Demos_f); 
	Cmd_AddCommand ("stopdemo", Host_Stopdemo_f);

	Cmd_AddCommand ("viewmodel", Host_Viewmodel_f);
	Cmd_AddCommand ("viewframe", Host_Viewframe_f);
	Cmd_AddCommand ("viewnext", Host_Viewnext_f);
	Cmd_AddCommand ("viewprev", Host_Viewprev_f);

	Cmd_AddCommand ("qcexec", Host_QC_Exec);
	Cmd_AddCommand ("qcfuncs", ED_Listfunctions_f);

	Cmd_AddCommand ("mcache", Mod_Print);
	Cmd_AddCommand ("models", Mod_PrintEx); // Baker -- another way to get to this


	if (COM_CheckParm("+capturedemo"))
		cls.capturedemo_and_exit = true;
}




#if 0


#define StringStartsWith(bigstr, lilstr)		!strncmp(bigstr, lilstr, strlen(lilstr))
#define StringStartsWithCaseLess(bigstr, lilstr)!strncasecmp(bigstr, lilstr, strlen(lilstr))

void Recursive_List_Files (const char* path)
{
	Con_Printf ("Recursive List: %s\n", path);
	{
		DIR		*dir_p = opendir(path);
		struct dirent	*dir_t;

		if (dir_p /* == NULL*/)
		{
			while ((dir_t = readdir(dir_p)) != NULL)
			{
				char full_name[MAX_OSPATH];
				_snprintf (full_name, sizeof(full_name), "%s/%s", path, dir_t->d_name) ;

				if (dir_t->d_name[0] == '.') // No . or ..
					continue;

				Con_Printf ("%s", full_name /*dir_t->d_name*/);
				if (System_File_Is_Folder ( full_name ) )
				{
					Con_Printf (" (dir)\n");
					Recursive_List_Files (full_name);
				} else Con_Printf ("\n");
			}
			closedir(dir_p);
		}
	}
}



	const char *_System_Folder_AppData (void)
	{
		static path1024	appdatapath;
		char *_appdata = getenv("APPDATA");

		StringLCopy (appdatapath, _appdata);
		String_Edit_SlashesForward_Like_Unix (appdatapath);

		return appdatapath;
	}

	const char *System_Folder_Caches_By_AppName (const char* appname)
	{
	#define CACHES_DIR_SUBPATH_OF_APPDATA_WINDOWS "caches"
		static path1024	cachesfolder;

		snprintf (cachesfolder, sizeof(cachesfolder), "%s/%s/%s", _System_Folder_AppData (), appname, CACHES_DIR_SUBPATH_OF_APPDATA_WINDOWS);
		return cachesfolder;
	}


// Now what?
void Cache_Dir_f (void)
{
	Con_Printf ("Caches folder: %s\n", APPLICATION_CACHES_FOLDER );
}


void Unzip_File (const char* extract_to, const char* zipfile)
{




}

void Recursive_Delete (const char* path)
{
	blah
}

GetTempPathA (ANSI) + UuidCreateSequential



// "Gamezip travail"
void GameZip_f (void)
{
	if (Cmd_Argc () == 2)
	{
		path1024 full_name;
		path1024 shortpath;
		path1024 destname;
		snprintf (shortpath, sizeof(shortpath), "%s.zip", Cmd_Argv(1) );
		snprintf (full_name, sizeof(full_name), "%s/%s", com_modszippath, shortpath );
		snprintf (destname, sizeof(destname), "%s/%s", APPLICATION_CACHES_FOLDER, shortpath );

		Con_Printf ("Mod zip %s goes into %s (%u)\n", full_name, destname, strlen(destname) );

	}
}

// SetCurrentDirectory("c:\\docs\\stuff");
// HZIP hz = OpenZip("c:\\stuff.zip",0);
// ZIPENTRY ze; GetZipItem(hz,-1,&ze); int numitems=ze.index;
// for (int i=0; i<numitems; i++)
// { GetZipItem(hz,i,&ze);
//   UnzipItem(hz,i,ze.name);
// }
// CloseZip(hz);


void Dir_f (void)
{
	if (Cmd_Argc () == 2)
	{
		char* path = Cmd_Argv (1);
		String_Edit_SlashesForward_Like_Unix(path);
		Recursive_List_Files (path);
	} else Con_Printf ("usage: dir <path>\n");
}
#endif
