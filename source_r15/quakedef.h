/* This file is NOT DONE

Versus Quakespasm:

1. Quakespasm supports a user directory.  Thinking about this ...

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

#ifndef __QUAKEDEFS_H
#define __QUAKEDEFS_H

// quakedef.h -- primary header for client

#define	QUAKE_GAME			// as opposed to utilities

#define	QUAKE_VERSION		1.09
#define ENGINE_NAME			"FitzQuake"
#define	FITZQUAKE_VERSION	0.941
#define	GAMENAME			"id1"			// directory to look in by default
#define CONFIG_CFG			"config.cfg"	// config name
#define AUTOEXEC_CFG		"autoexec.cfg"	// autoexec.cfg name

#if 1 // Baker change base is now Mark V - Revision 10 platform neutral parts.

#define SUPPORTS_MULTISAMPLE
//#define SUPPORTS_MIRRORS
//	#define HARDWARE_GAMMA_BUILD
	#define SUPPORTS_TEXTURE_POINTER

	#define SUPPORTS_MH_NOCLIP_NOTRIGGER
	#define SUPPORTS_WHITE_ROOM_FIX
//	#define SUPPORTS_SHIFT_SLOW_ALWAYS_RUN (do not like)
	#define CLIPBOARD_IMAGE
	#define FIX_LUMA_TEXTURE
	#define ADD_DEC_CVAR
	#define CHECK_SOUND_BIT_WIDTH_8_16
	//#define SUPPORTS_64_BIT

	// Revision 11: Give, unalias fix, os path to 256 chars, modal sleep, vs2012
	// Increased default console width to 78.  640 width as "normal" for startup.
	// Baker: -noipx is implied

	// FitzQuake Mark V - Revision 10 = New Base.
//	#define SUPPORTS_GHOSTING_AUTOMAP				// ghostdemo plays the map
//	#define SUPPORTS_GHOSTING_VIA_DEMOS				// Create a ghost from demos

	// Platform specific:   #ifdef _WIN32
//	#define SUPPORTS_WINDOWS_MENUS					// Convenience menus.
	#define SUPPORTS_MP3_MUSIC						// Plays id1/music/track00.mp3 or rather <gamedir + search paths>/music/track<xx>.mp3 (Does not play in-pak mp3 files)
	#define SUPPORTS_AVI_CAPTURE					// capturedemo command and friends
	// #endif

	#if _MSC_VER > 1200 // At the moment the include order is wrong so cannot make this work
			#undef SUPPORTS_MP3_MUSIC
			#define WANTED_MP3_MUSIC // But ... can't have it.
	#endif

	#ifdef __GNUC__ // Baker:  I have never been able to make this work under MinGW + CodeBlocks
		#ifdef SUPPORTS_MP3_MUSIC
			#undef SUPPORTS_MP3_MUSIC
			#define WANTED_MP3_MUSIC // But ... can't have it.
		#endif
		#ifdef SUPPORTS_AVI_CAPTURE
			#undef SUPPORTS_AVI_CAPTURE // Must disable AVI capture :(
			#define WANTED_AVI_CAPTURE // But ... can't have it.
		#endif
	#endif


#endif // Baker change +



#include "q_stdinc.h"


#define	MINIMUM_MEMORY			0x550000
#define	MINIMUM_MEMORY_LEVELPAK	(MINIMUM_MEMORY + 0x100000)

#define MAX_NUM_ARGVS	50

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


#define	MAX_QPATH		64			// max length of a quake game pathname
#define	MAX_OSPATH		256			// max length of a filesystem pathname

#define	ON_EPSILON		0.1			// point on plane side epsilon

#define	DIST_EPSILON		(0.03125)		// 1/32 epsilon to keep floating point happy (moved from world.c)



typedef enum
{
//	FitzQuake limit					 // Standard limit (WinQuake)
	MAX_EDICTS_PROTOCOL_666			= 32000,

	MIN_SANE_EDICTS_512				=  512,
	MAX_SANE_EDICTS_8192			=  8192,

	MAX_FITZQUAKE_MSGLEN			= 32000,  MAX_WINQUAKE_MSGLEN			=  8000,
	NET_FITZQUAKE_MAXMESSAGE		= 32000,  MAX_WINQUAKE_MAXMESSAGE		=  8192,
	MAX_FITZQUAKE_DATAGRAM			= 32000,  MAX_WINQUAKE_DATAGRAM			=  1024,

	MAX_FITZQUAKE_DATAGRAM_MTU		= 1400,
	// ^^ FIX ME!!!!!  It is intended to be 1400

// per-level limits
	MAX_FITZQUAKE_BEAMS				=    32,  MAX_WINQUAKE_BEAMS			=    24,
	MAX_FITZQUAKE_EFRAGS			=  2048,  MAX_WINQUAKE_EFRAGS			=   600,
	MAX_FITZQUAKE_DLIGHTS			=   128,  MAX_WINQUAKE_DLIGHTS			=    32,
	MAX_FITZQUAKE_STATIC_ENTITIES	=   512,  MAX_WINQUAKE_STATIC_ENTITIES	=   128,
	MAX_FITZQUAKE_TEMP_ENTITIES		=   256,  MAX_WINQUAKE_TEMP_ENTITIES	=    64,
	MAX_FITZQUAKE_VISEDICTS			=  1024,  MAX_WINQUAKE_VISEDICTS		=   256,

	MAX_FITZQUAKE_LIGHTMAPS			=   256,  MAX_WINQUAKE_LIGHTMAPS		=    64,

	MAX_FITZQUAKE_MAX_EDICTS		= 32000,  MAX_WINQUAKE_EDICTS			=   600,
	MAX_FITZQUAKE_MODELS			=  2048,  MAX_WINQUAKE_MODELS			=   256,
	MAX_FITZQUAKE_SOUNDS			=  2048,  MAX_WINQUAKE_SOUNDS			=   256,

	MAX_FITZQUAKE_SURFACE_EXTENTS	=  2000,  MAX_WINQUAKE_SURFACE_EXTENTS  =   256,

} engine_limits;
//johnfitz -- ents past 8192 can't play sounds in the standard protocol

#define	MAX_LIGHTSTYLES	64
#define	MAX_STYLESTRING	64
#define	SAVEGAME_COMMENT_LENGTH	39

//
// stats are integers communicated to the client by the server
//
#define	MAX_CL_STATS		32
#define	STAT_HEALTH			0
#define	STAT_FRAGS			1
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_ARMOR			4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS			6
#define	STAT_NAILS			7
#define	STAT_ROCKETS		8
#define	STAT_CELLS			9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster

// stock defines

#define	IT_SHOTGUN				1
#define	IT_SUPER_SHOTGUN		2
#define	IT_NAILGUN				4
#define	IT_SUPER_NAILGUN		8
#define	IT_GRENADE_LAUNCHER		16
#define	IT_ROCKET_LAUNCHER		32
#define	IT_LIGHTNING			64
#define IT_SUPER_LIGHTNING      128
#define IT_SHELLS               256
#define IT_NAILS                512
#define IT_ROCKETS              1024
#define IT_CELLS                2048
#define IT_AXE                  4096
#define IT_ARMOR1               8192
#define IT_ARMOR2               16384
#define IT_ARMOR3               32768
#define IT_SUPERHEALTH          65536
#define IT_KEY1                 131072
#define IT_KEY2                 262144
#define	IT_INVISIBILITY			524288
#define	IT_INVULNERABILITY		1048576
#define	IT_SUIT					2097152
#define	IT_QUAD					4194304
#define IT_SIGIL1               (1<<28)
#define IT_SIGIL2               (1<<29)
#define IT_SIGIL3               (1<<30)
#define IT_SIGIL4               (1<<31)

//===========================================
//rogue changed and added defines

#define RIT_SHELLS              128
#define RIT_NAILS               256
#define RIT_ROCKETS             512
#define RIT_CELLS               1024
#define RIT_AXE                 2048
#define RIT_LAVA_NAILGUN        4096
#define RIT_LAVA_SUPER_NAILGUN  8192
#define RIT_MULTI_GRENADE       16384
#define RIT_MULTI_ROCKET        32768
#define RIT_PLASMA_GUN          65536
#define RIT_ARMOR1              8388608
#define RIT_ARMOR2              16777216
#define RIT_ARMOR3              33554432
#define RIT_LAVA_NAILS          67108864
#define RIT_PLASMA_AMMO         134217728
#define RIT_MULTI_ROCKETS       268435456
#define RIT_SHIELD              536870912
#define RIT_ANTIGRAV            1073741824
#define RIT_SUPERHEALTH         2147483648

//MED 01/04/97 added hipnotic defines
//===========================================
//hipnotic added defines
#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT       7
#define HIT_LASER_CANNON_BIT  23
#define HIT_PROXIMITY_GUN   (1<<HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR         (1<<HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON    (1<<HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT         (1<<(23+2))
#define HIT_EMPATHY_SHIELDS (1<<(23+3))

//===========================================

#define	MAX_SCOREBOARD		16
#define	MAX_SCOREBOARDNAME	32

#define	SOUND_CHANNELS		8

typedef struct
{
	const char	*basedir; // This can be overriden with -basedir, this is the current working directory (cwd command fills this)
	int		argc;
	char	**argv;
	void	*membase;
	int		memsize;
} quakeparms_t;

#include "common.h"
#include "strings.h"  // Baker
#include "common_ez.h" // Baker
#include "file.h"
#include "bspfile.h"
#include "sys.h"
#include "zone.h"
#include "mathlib.h"
#include "cvar.h"

#include "protocol.h"
#include "net.h"

#include "cmd.h"
#include "crc.h"

#include "progs.h"
#include "server.h"

#include "console.h"
#include "wad.h"
#include "vid.h"
#include "screen.h"
#include "draw.h"
#include "render.h"
#include "view.h"
#include "sbar.h"
#include "q_sound.h"
#include "client.h"

#include "gl_model.h"
#include "world.h"

#include "image.h" //johnfitz
#include "gl_texmgr.h" //johnfitz
#include "input.h"
#include "keys.h"
#include "menu.h"
#include "cdaudio.h"
#include "glquake.h"

#include "movie.h" // Baker
#include "talk_macro.h" // Baker
#include "lists.h"
#include "location.h" // Baker
#include "tool_inspector.h" // Baker
#include "tool_texturepointer.h" // Baker
#include "recent_file.h" // Baker
#include "downloads.h" // Baker
//#include "sys_win_menu.h"



//=============================================================================

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

//
// host
//
extern	quakeparms_t *host_parms;

extern	cvar_t		sys_ticrate;
extern	cvar_t		sys_nostdout;
extern	cvar_t		developer;
extern	cvar_t		max_edicts; //johnfitz

extern	qboolean	host_initialized;		// true if into command execution
extern	qboolean	host_post_initialized;	// true if beyond initial command execution (config.cfg already run, etc.)
extern	double		host_frametime;
extern	int			host_framecount;	// incremented every frame, never reset
extern	double		realtime;			// not bounded in any way, changed at
										// start of every frame, never reset

void Host_ClearMemory (void);
void Host_ServerFrame (void);
void Host_InitCommands (void);
void Host_Init (void);
void Host_Shutdown(void);
void Host_Error (const char *error, ...) __attribute__((__format__(__printf__,1,2), __noreturn__));
void Host_EndGame (const char *message, ...) __attribute__((__format__(__printf__,1,2), __noreturn__));
void Host_Frame (float time);
void Host_Quit_f (void);
void Host_Quit (void);
void Host_ClientCommands (const char *fmt, ...) __attribute__((__format__(__printf__,1,2)));
void Host_ShutdownServer (qboolean crash);
void Host_WriteConfiguration (void);

void Host_Stopdemo_f (void);

void Host_Changelevel_Required_Msg (cvar_t* var);
qboolean Read_Early_Cvars_For_File (const char* config_file_name, const cvar_t* list[]);
int Host_Gamedir_Change (const char* gamedir_new, const char* new_hud_typestr, qboolean liveswitch, const char** info_string);

extern qboolean		isDedicated;

extern int			minimum_memory;

#endif	/* __QUAKEDEFS_H */

