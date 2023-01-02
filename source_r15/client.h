/*  Note that this file is likely not done yet

Versus Quakespasm:   Various.  Extensive ...

1. Dynamic light correct on moved/rotated brush models
2. Rotation support
3. Ping in scoreboard
4. Talk macro expansion "I have %h health and %a armor"
5. Demo rewind
6. Ping in scoreboard

Does not have: Quakespasm stufftext frame (???)
               Unbindall in config thing

*/

/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others

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

#ifndef _CLIENT_H_
#define _CLIENT_H_

// client.h

typedef struct
{
	int		length;
	char	map[MAX_STYLESTRING];
	char	average; //johnfitz
	char	peak; //johnfitz
} lightstyle_t;

typedef struct
{
	char	name[MAX_SCOREBOARDNAME];
	float	entertime;
	int		frags;
	int		colors;			// two 4 bit fields
	int		ping;
} scoreboard_t;

typedef struct
{
	int		destcolor[3];
	int		percent;		// 0-256
} cshift_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS		4

#define	NAME_LENGTH	64


//
// client_state_t should hold all pieces of the client state
//

#define	SIGNONS		4			// signon messages to receive before connected


typedef struct
{
	vec3_t	origin;
	vec3_t	transformed;
	float	radius;
	float	die;				// stop lighting after this time
	float	decay;				// drop this each second
	float	minlight;			// don't add when contributing less
	int		key;
	vec3_t	color;				//johnfitz -- lit support via lordhavoc
} dlight_t;


typedef struct
{
	int		entity;
	struct qmodel_s	*model;
	float	endtime;
	vec3_t	start, end;
} beam_t;

#define	MAX_MAPSTRING	2048
#define	MAX_DEMOS		8
#define	MAX_DEMONAME	16

typedef enum {
ca_dedicated, 		// a dedicated server with no ability to start a client
ca_disconnected, 	// full screen console with no connection
ca_connected		// valid netcon, talking to a server
} cactive_t;



//
// the client_static_t structure is persistant through an arbitrary number
// of server connections
//
typedef struct
{
	cactive_t	state;

// personalization data sent to server
	char		mapstring[MAX_QPATH];
	char		spawnparms[MAX_MAPSTRING];	// to restart a level

// demo loop control
	int			demonum;		// -1 = don't play demos
	char		demos[MAX_DEMOS][MAX_DEMONAME];		// when not playing

// demo recording info must be here, because record is started before
// entering a map (and clearing client_state_t)
	qboolean	demorecording;
	qboolean	demoplayback;
	qboolean	demorewind;
	float		demospeed;

	char		demoname[MAX_OSPATH];	// So we can print demo whatever completed.

	int			demo_file_length;		// VectorLength of file in bytes
	int			demo_offset_start;		// If in a pak, the offset into the file otherwise 0.
	int			demo_offset_current;	// Current offset into the file, updated as the demo is player

	float		demo_hosttime_start;	// For measuring capturedemo time completion estimates.
	float		demo_hosttime_elapsed;	// Does not advance if paused.
	float		demo_cltime_start;		// Above except cl.time
	float		demo_cltime_elapsed;	// Above except cl.time

	qboolean	titledemo;				// Does not display HUD, notify, centerprint or crosshair when played as part of startdemos

	qboolean	capturedemo;			// Indicates if we are capturing demo playback
	qboolean	capturedemo_and_exit;	// Quit after capturedemo

	qboolean	timedemo;

	int			forcetrack;			// -1 = use normal cd track
	FILE		*demofile;
	int			td_lastframe;		// to meter out one message a frame
	int			td_startframe;		// host_framecount at start
	float		td_starttime;		// realtime at second frame of timedemo

// connection information
	int			signon;			// 0 to SIGNONS
	struct qsocket_s	*netcon;
	sizebuf_t	message;		// writing buffer to send to server

} client_static_t;

extern client_static_t	cls;

//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct
{
	int			movemessages;	// since connecting to this server
								// throw out the first couple, so the player
								// doesn't accidentally do something the
								// first frame
	usercmd_t	cmd;			// last command sent to the server

// information for local display
	int			stats[MAX_CL_STATS];	// health, etc
	int			items;			// inventory bit flags
	float	item_gettime[32];	// cl.time of aquiring item, for blinking
	float		faceanimtime;	// use anim frame if cl.time < this

	cshift_t	cshifts[NUM_CSHIFTS];	// color shifts for damage, powerups
	cshift_t	prev_cshifts[NUM_CSHIFTS];	// and content types

// the client maintains its own idea of view angles, which are
// sent to the server each frame.  The server sets punchangle when
// the view is temporarliy offset, and an angle reset commands at the start
// of each level and after teleporting.
	vec3_t		mviewangles[2];	// during demo playback viewangles is lerped
								// between these
	vec3_t		viewangles;

	vec3_t		mvelocity[2];	// update by server, used for lean+bob
								// (0 is newest)
	vec3_t		velocity;		// lerped between mvelocity[0] and [1]

	vec3_t		punchangle;		// temporary offset

// pitch drifting vars
	float		idealpitch;
	float		pitchvel;
	qboolean	nodrift;
	float		driftmove;
	double		laststop;

	float		viewheight;
	float		crouch;			// local amount for smoothing stepups

	qboolean	paused;			// send over by server
	qboolean	onground;
	qboolean	inwater;

	int			intermission;	// don't change view angle, full screen, etc
	int			completed_time;	// latched at intermission start

	double		mtime[2];		// the timestamp of last two messages
	double		time;			// clients view of time, should be between
								// servertime and oldservertime to generate
								// a lerp point for other data
	double		ctime;			// inclusive of demo speed (can go backwards)
	double		oldtime;		// previous cl.time, time-oldtime is used
								// to decay light values and smooth step ups


	float		last_received_message;	// (realtime) for net trouble icon

//
// information that is static for the entire time connected to a server
//
	struct qmodel_s		*model_precache[MAX_FITZQUAKE_MODELS];
	struct sfx_s		*sound_precache[MAX_FITZQUAKE_SOUNDS];

	char		worldname[MAX_QPATH];
	char		levelname[128];	// for display on solo scoreboard //johnfitz -- was 40.
	int			viewentity;		// cl_entitites[cl.viewentity] = player
	int			maxclients;
	int			gametype;

// refresh related state
	struct qmodel_s	*worldmodel;	// cl_entitites[0].model
	struct efrag_s	*free_efrags;
	int			num_entities;	// held in cl_entities array
	int			num_statics;	// held in cl_staticentities array
	entity_t	viewent;			// the gun model

	int			cdtrack, looptrack;	// cd audio

// frag scoreboard
	scoreboard_t	*scores;		// [cl.maxclients]

	double			last_ping_time;
	qboolean		expecting_ping;
	qboolean		in_ping_parse;
	int				in_ping_parse_slot;

	vec3_t			death_location;		// used for %d formatting

	double			last_angle_time;
	vec3_t			lerpangles;

	qboolean		noclip_anglehack;
	unsigned		protocol; //johnfitz
	char			hintstrings[MAX_NUM_HINTS][MAX_HINT_BUF_64];
	int				skillhint;
} client_state_t;

//
// cvars
//
extern	cvar_t	cl_name;
extern	cvar_t	cl_color;

extern	cvar_t	cl_upspeed;
extern	cvar_t	cl_forwardspeed;
extern	cvar_t	cl_backspeed;
extern	cvar_t	cl_sidespeed;

extern	cvar_t	cl_movespeedkey;

extern	cvar_t	cl_yawspeed;
extern	cvar_t	cl_pitchspeed;

extern	cvar_t	cl_anglespeedkey;

extern	cvar_t	cl_autofire;

extern	cvar_t	cl_shownet;
extern	cvar_t	cl_nolerp;

extern	cvar_t	cfg_unbindall;

extern	cvar_t	cl_pitchdriftspeed;
extern	cvar_t	lookspring;
extern	cvar_t	lookstrafe;
extern	cvar_t	sensitivity;

extern	cvar_t	m_pitch;
extern	cvar_t	m_yaw;
extern	cvar_t	m_forward;
extern	cvar_t	m_side;


extern cvar_t cl_maxpitch; //johnfitz -- variable pitch clamping
extern cvar_t cl_minpitch; //johnfitz -- variable pitch clamping
extern cvar_t cl_titledemos_list;


extern	client_state_t	cl;

// FIXME, allocate dynamically
extern	beam_t			cl_beams[MAX_FITZQUAKE_BEAMS];
extern	dlight_t		cl_dlights[MAX_FITZQUAKE_DLIGHTS];
extern	efrag_t			cl_efrags[MAX_FITZQUAKE_EFRAGS];
extern	entity_t		cl_static_entities[MAX_FITZQUAKE_STATIC_ENTITIES];
extern	entity_t		cl_temp_entities[MAX_FITZQUAKE_TEMP_ENTITIES];

extern	entity_t		*cl_entities; //johnfitz -- was a static array, now on hunk
extern	int				cl_max_edicts; //johnfitz -- only changes when new map loads

extern	entity_t		*cl_visedicts[MAX_FITZQUAKE_VISEDICTS];
extern	int				cl_numvisedicts;

extern	lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];





//=============================================================================

//
// cl_main
//
dlight_t *CL_AllocDlight (int key);
void	CL_DecayLights (void);

void CL_Init (void);

void CL_EstablishConnection (const char *host);
void CL_Signon1 (void);
void CL_Signon2 (void);
void CL_Signon3 (void);
void CL_Signon4 (void);

void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_NextDemo (void);

//
// cl_input
//
typedef struct
{
	int		down[2];		// key nums holding it down
	int		state;			// low bit is down state
} kbutton_t;

extern	kbutton_t	in_mlook, in_klook;
extern 	kbutton_t 	in_strafe;
extern 	kbutton_t 	in_speed;
extern	kbutton_t	in_attack;

void CL_InitInput (void);
void CL_SendCmd (void);
void CL_SendMove (const usercmd_t *cmd);
int  CL_ReadFromServer (void);
void CL_BaseMove (usercmd_t *cmd);

void CL_ParseTEnt (void);
void CL_UpdateTEnts (void);

void CL_ClearState (unsigned int protocol_num);

//
// cl_demo.c
//
void CL_StopPlayback (void);
int CL_GetMessage (void);

void CL_Stop_f (void);
void CL_Record_f (void);
void CL_PlayDemo_f (void);
void CL_PlayDemo_NextStartDemo_f (void);
void CL_TimeDemo_f (void);
void CL_Clear_Demos_Queue (void);

//
// cl_parse.c
//
void CL_ParseServerMessage (void);
void CL_NewTranslation (int slot);
void CL_Hints_List_f (void);

//
// view
//
void V_StartPitchDrift (void);
void V_StopPitchDrift (void);

void V_RenderView (void);
//void V_UpdatePalette (void); //johnfitz
void V_Register (void);
void V_ParseDamage (void);
void V_SetContentsColor (int contents);

//
// cl_tent
//
void CL_InitTEnts (void);
void CL_SignonReply (void);

//
// chase
//
extern	cvar_t	chase_active;
extern int chase_mode;

void Chase_Init (void);
qboolean TraceLine (vec3_t start, vec3_t end, vec3_t impact); // Baker: qboolean = was traceline blocked?
void Chase_UpdateForClient (void); //johnfitz
void Chase_UpdateForDrawing (void); //johnfitz

#endif	/* _CLIENT_H_ */

