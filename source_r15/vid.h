/* Versus Quakespasm: 

1. Removed software renderer only stuff
2. Desktop width, height, bpp

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

#ifndef __VID_DEFS_H
#define __VID_DEFS_H

// vid.h -- video driver defs

// moved here for global use -- kristian
typedef enum { MODE_UNINIT = -1, MODE_WINDOWED = 0, MODE_FULLSCREEN = 1 } modestate_t;

typedef struct vrect_s
{
	int				x,y,width,height;
//	struct vrect_s	*pnext;
} vrect_t;

enum {TEARDOWN_NO_DELETE_GL_CONTEXT = 0, TEARDOWN_FULL = 1};

enum {USER_SETTING_FAVORITE_MODE = 0, ALT_ENTER_TEMPMODE = 1};

#define MAX_MODE_LIST				600
#define MAX_MODE_WIDTH				10000
#define MAX_MODE_HEIGHT				10000
#define MIN_MODE_WIDTH				640
#define MIN_MODE_HEIGHT				400

#define MIN_WINDOWED_MODE_WIDTH		320
#define MIN_WINDOWED_MODE_HEIGHT	200

typedef struct 
{
	modestate_t	type;

	int			width;
	int			height;
	int			bpp;
} vmode_t;

extern cvar_t vid_fullscreen;
extern cvar_t vid_width;
extern cvar_t vid_height;
#ifdef SUPPORTS_MULTISAMPLE // Baker change
extern cvar_t vid_multisample;
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change
extern cvar_t vid_vsync;

typedef struct
{
	vmode_t			modelist[MAX_MODE_LIST];
	int				nummodes; // The number of them filled in

	vmode_t			screen;
	int				modenum_screen;		// mode # on-screen now
	int				modenum_user_selected;	// mode # user intentionally selected (i.e. not an ALT-ENTER toggle)

	int				numpages;
	int				recalc_refdef;	// if true, recalc vid-based stuff
	int				conwidth;
	int				conheight;

	vmode_t			desktop;
	qboolean		canalttab;
	qboolean		wassuspended;	
	qboolean		fullsbardraw;
	qboolean		ActiveApp;
	qboolean		Minimized;
	qboolean		sound_active;
	qboolean		initialized;

} viddef_t;

extern	viddef_t	vid;				// global video state


void VID_Local_Vsync_f (cvar_t *var);
#ifdef SUPPORTS_MULTISAMPLE // Baker change
void VID_Local_Multisample_f (cvar_t *var);
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change
void VID_Test (void);
void VID_Restart_f (void);
void VID_Alt_Enter_f (void);


// During run ...
void VID_AppActivate(qboolean fActive, qboolean minimize);
void VID_Local_Suspend (qboolean bSuspend);
void VID_SwapBuffers (void);
void VID_Local_SwapBuffers (void);

// Platform localized video setup ...
vmode_t VID_Local_GetDesktopProperties (void);
void VID_Local_Window_PreSetup (void);
qboolean VID_Local_Vsync_Init (const char* gl_extensions_str);

// Main
void VID_Init (void);
int VID_SetMode (int modenum);
qboolean VID_Local_SetMode (int modenum);
void VID_Shutdown (void);
void VID_Local_Window_Renderer_Teardown (int full);


// Video modes
qboolean VID_Mode_Exists (vmode_t* test, int *outmodenum);
void VID_Local_AddFullscreenModes (void);


// Cvars and modes
vmode_t VID_Cvars_To_Mode (void);
void VID_Cvars_Sync_To_Mode (vmode_t* mymode);
void VID_Cvars_Set_Autoselect_Temp_Fullscreen_Mode (int favoritemode);
void VID_Cvars_Set_Autoselect_Temp_Windowed_Mode (int favoritemode);

#ifdef HARDWARE_GAMMA_BUILD
	// Gamma Table
	void VID_Gamma_Init (void);
	void VID_Gamma_Think (void);
	void VID_Gamma_Shutdown (void);
	qboolean VID_Local_IsGammaAvailable (unsigned short* ramps);
	void VID_Local_Gamma_Set (unsigned short* ramps);
	int VID_Local_Gamma_Reset (void);
	void VID_Gamma_Clock_Set (void); // Initiates a "timer" to ensure gamma is good in fullscreen
#endif // HARDWARE_GAMMA_BUILD

#endif	/* __VID_DEFS_H */

