/*
Copyright (C) 2009-2013 Baker

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
// vid.c -- common video

#include "quakedef.h"

viddef_t	vid; // global video state

cvar_t		vid_fullscreen		= {"vid_fullscreen", "1", CVAR_ARCHIVE};
cvar_t		vid_width			= {"vid_width", "640", CVAR_ARCHIVE};
cvar_t		vid_height			= {"vid_height", "480", CVAR_ARCHIVE};
#ifdef SUPPORTS_MULTISAMPLE // Baker change
cvar_t		vid_multisample		= {"vid_multisample", "0", CVAR_ARCHIVE};
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change
cvar_t		vid_vsync			= {"vid_vsync", "0", CVAR_ARCHIVE};

//
// set mode and restart
//

/*
================
VID_SetMode
================
*/
int VID_SetMode (int modenum)
{
// so Con_Printfs don't mess us up by forcing vid and snd updates
	int	temp = scr_disabled_for_loading;
	qboolean re_setupgl;

	scr_disabled_for_loading = true;
	vid.canalttab = false;

	// Stop Sounds
	S_BlockSound ();
	S_ClearBuffer ();
	CDAudio_Pause ();

	// Platform specific stuff
	re_setupgl = (VID_Local_SetMode (modenum) == false);

	// Assignment
	{
		vid.modenum_screen = modenum;
		vid.screen = vid.modelist[vid.modenum_screen];
		// keep cvars in line with actual mode
		VID_Cvars_Sync_To_Mode (&vid.modelist[vid.modenum_screen]);

		// Refresh console
		SCR_Conwidth_f (NULL);
		vid.recalc_refdef = 1;
		vid.numpages = 2;
	}

	// GL break-in
	GL_Evaluate_Renderer ();
	GL_SetupState ();

	// ensure swap settings right
	VID_Local_Vsync_f (NULL);
#ifdef HARDWARE_GAMMA_BUILD
	VID_Gamma_Think ();
	VID_Gamma_Clock_Set (); // Baker: This sets a clock to restore the gamma
#endif // HARDWARE_GAMMA_BUILD

	if (re_setupgl)
	{
		if (host_initialized)
			Con_Printf ("VID_SetMode: Couldn't reuse context.  Reupload images.\n");	
		TexMgr_ReloadImages (); // SAFE?
	} else Con_DPrintf ("Reused context no reupload images\n");

	TexMgr_RecalcWarpImageSize (); // SAFE?

	// Restore sound
	Input_Think ();
	S_UnblockSound ();
	CDAudio_Resume ();

	vid.canalttab = true;
	scr_disabled_for_loading = temp;
	
	return true;
}

qboolean VID_Restart (int flags /* favorite vs. temp*/)
{

	HDC			hdc = wglGetCurrentDC();
	HGLRC		hrc = wglGetCurrentContext();
	vmode_t		newmode = VID_Cvars_To_Mode ();
	vmode_t		oldmode = vid.screen;
	int			newmodenum;
	
	// No change scenario
	if ( memcmp (&newmode, &oldmode, sizeof(vmode_t)) == 0)
	{
		Con_Printf ("Video mode request is same as current mode.\n");
		return false;
	}

	// Fullscreen must check existing modes, window must set it instead.
	switch (newmode.type)
	{
	case MODE_WINDOWED:
		memcpy (&vid.modelist[MODE_WINDOWED], &newmode, sizeof (vmode_t) );
		newmodenum = 0;
		break;

	case MODE_FULLSCREEN:
		if (VID_Mode_Exists (&newmode, &newmodenum) == false)
		{
			Con_Printf ("%d x %d (%i) is not a valid fullscreen mode\n",
				(int)vid_width.value,
				(int)vid_height.value,
				(int)vid_fullscreen.value);
			return false;
		}
		break;
	}
	
	// Determine if the mode is invalid.

	VID_SetMode (newmodenum);

	if (flags == USER_SETTING_FAVORITE_MODE)
		vid.modenum_user_selected = vid.modenum_screen;

	return true;
}

/*
================
VID_Test -- johnfitz -- like vid_restart, but asks for confirmation after switching modes
================
*/
void VID_Test (void)
{
	vmode_t		newmode = VID_Cvars_To_Mode ();
	vmode_t		oldmode = vid.screen;
	qboolean	mode_changed = memcmp (&newmode, &vid.screen, sizeof(vmode_t) );
	
	if (!mode_changed)
		return;
//
// now try the switch
//
	VID_Restart (USER_SETTING_FAVORITE_MODE);

	//pop up confirmation dialog
	if (!SCR_ModalMessage("Would you like to keep this\nvideo mode? (y/n)\n", 5.0f))
	{
		// User rejected new mode: revert cvars and mode
		VID_Cvars_Sync_To_Mode (&oldmode);
		VID_Restart (USER_SETTING_FAVORITE_MODE);
	}
}

void VID_Alt_Enter_f (void)
{
	if (vid.modenum_screen != vid.modenum_user_selected) 
	{
		// Go to favorite mode
		VID_Cvars_Sync_To_Mode ( &vid.modelist[vid.modenum_user_selected] );
		VID_Restart (USER_SETTING_FAVORITE_MODE);
		return;
	}

// ALT-ENTER to a temp mode
	if (vid.screen.type == MODE_WINDOWED)
		VID_Cvars_Set_Autoselect_Temp_Fullscreen_Mode (vid.modenum_screen);
	else  VID_Cvars_Set_Autoselect_Temp_Windowed_Mode (vid.modenum_screen);

	VID_Restart (ALT_ENTER_TEMPMODE);
}


void VID_Restart_f (void)
{
	VID_Restart (USER_SETTING_FAVORITE_MODE);
}

//
// in-game
//


void VID_AppActivate(qboolean fActive, qboolean minimize)
{
	vid.ActiveApp = fActive;
	vid.Minimized = minimize;

//	Con_Printf ("App activate occurred %i\n", vid.ActiveApp);

	if (vid.ActiveApp)
	{
		if (!vid.sound_active)
		{
			S_UnblockSound ();
			vid.sound_active = true;
		}
		if (vid.screen.type == MODE_FULLSCREEN && vid.canalttab && vid.wassuspended) 
		{
			VID_Local_Suspend (false);
			vid.wassuspended = false;
		}
	}

	if (!vid.ActiveApp)
	{
		if (vid.screen.type == MODE_FULLSCREEN && vid.canalttab) 
		{
			VID_Local_Suspend (true);
			vid.wassuspended = true;
		}

		if (vid.sound_active)
		{
			S_BlockSound ();
			vid.sound_active = false;
		}
	}
}

void VID_SwapBuffers (void)
{
	VID_Local_SwapBuffers ();
}

//
// functions to match modes to cvars or reverse
//

void VID_Cvars_Sync_To_Mode (vmode_t* mymode)
{
	// Don't allow anything exiting to call this.  I think we are "ok"
	Cvar_SetValue (vid_width.name, (float)mymode->width);
	Cvar_SetValue (vid_height.name, (float)mymode->height);
	Cvar_SetValue (vid_fullscreen.name, mymode->type == MODE_FULLSCREEN ? 1 : 0);
}

vmode_t VID_Cvars_To_Mode (void)
{
	vmode_t retmode;

	retmode.type	= vid_fullscreen.value ? MODE_FULLSCREEN : MODE_WINDOWED;
	retmode.width	= (int)vid_width.value;
	retmode.height	= (int)vid_height.value;
	retmode.bpp		= vid.desktop.bpp;
	
	if (retmode.type == MODE_WINDOWED)
	{
		retmode.width	= CLAMP (MIN_WINDOWED_MODE_WIDTH, retmode.width, vid.desktop.width);
		retmode.height	= CLAMP (MIN_WINDOWED_MODE_HEIGHT, retmode.height, vid.desktop.height);
		retmode.bpp		= vid.desktop.bpp;
	}
	
	return retmode;
}


	

qboolean VID_Read_Early_Cvars (void)
{

	// Any of these found and we bail
	char *video_override_commandline_params[] = {"-window", "-width", "-height", "-current", NULL } ;
	const cvar_t* cvarslist[] = {
		&vid_fullscreen, 
		&vid_width, 
		&vid_height, 
#ifdef SUPPORTS_MULTISAMPLE // Baker change
		&vid_multisample, 
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change
		&scr_brightness, 
	NULL};
	qboolean found_in_config, found_in_autoexec;
	int i;

	for (i = 0; video_override_commandline_params[i]; i++)
		if (COM_CheckParm (video_override_commandline_params[i]))
			return false;

	found_in_config = Read_Early_Cvars_For_File (CONFIG_CFG, cvarslist);
	found_in_autoexec = Read_Early_Cvars_For_File (AUTOEXEC_CFG, cvarslist);

	return (found_in_config || found_in_autoexec);
}


qboolean VID_Mode_Exists (vmode_t* test, int *outmodenum)
{
	int i;

	for (i = 0; i < vid.nummodes; i++)
	{
		if (memcmp (&vid.modelist[i], test, sizeof(vmode_t)))
			continue; // No match

		// dup
		if (outmodenum) *outmodenum = i;
		return true; // Duplicate
	}
	return false;
}

void VID_MakeMode (modestate_t mode_type, vmode_t *new_mode)
{
	new_mode->type	= mode_type;
	new_mode->width	= 640;
	new_mode->height= 480;
	new_mode->bpp	= vid.desktop.bpp;

	// !!(int)vid_fullscreen.value --> turn into an int and "NOT" it twice 
	// so must have 0 or 1 value.  In case vid_fullscreen is 2 or something weird
	if ( (!!(int)vid_fullscreen.value) == mode_type)
	{
		new_mode->width = (int)vid_width.value;
		new_mode->height= (int)vid_height.value;
	} 
	vid.nummodes ++;
}

void VID_Cvars_Set_Autoselect_Temp_Windowed_Mode (int favoritemode)
{
	// Pencil in last windowed mode, but set the bpp to the desktop bpp
	VID_Cvars_Sync_To_Mode (&vid.modelist[MODE_WINDOWED]);
}

void VID_Cvars_Set_Autoselect_Temp_Fullscreen_Mode (int favoritemode)
{
	vmode_t *fave = &vid.modelist[favoritemode];
	int best = -1;
	int	bestscore = -1;
	int i;

	// Look through the video modes.

	// If an exact matching fullscreen mode of same resolution
	// exists, pick that.  Try to go for same bpp.

	// Attempt 1: Match resolution

	// Baker: Locate matching mode
	for (i = 1; i < vid.nummodes; i ++)
	{
		vmode_t *mode = &vid.modelist[i];
		int size_match		= (mode->width == fave->width && mode->height == fave->height);

		int score = size_match * 20;

		if (score <= bestscore)
			continue;  // Didn't beat best
		
		// New best
		best = i;
		bestscore = score;
	}

	if (bestscore < 20)
	{
		// No size match ... try again
		// If fails, pick something with width/height both divisble by 8
		// so charset doesn't look stretched.  Go for largest mode with
		// desktop bpp and desktop refreshrate and desktop width/height
		// Unless those are stupid.

		best = -1;
		bestscore = -1;

		for (i = 1; i < vid.nummodes; i ++)
		{
			vmode_t *mode = &vid.modelist[i];

			if (mode->width & 7)
				continue; // Skip stupid resolutions
			if (mode->height & 7)
				continue; // Skip stupid resolutions

			if (mode->width >= vid.desktop.width - 7)
				if (mode->height >= vid.desktop.height - 7)
				{
					// Take it
					best = i;
					break;
				}

			// Not an automatic winner.  If largest ...
			if (mode->width >= vid.modelist[best].width || mode->height >= vid.modelist[best].height)
			{
				best = i;
			}
		}

		if (best == -1)
			Sys_Error ("Couldn't find suitable video mode");

	}

	// Set cvars
	VID_Cvars_Sync_To_Mode (&vid.modelist[best]);

	// Ok ... cvars are ready !
}


//
// startup / shutdown
//

void	VID_Init (void)
{
	int		i;

	qboolean videos_cvars_read;

	vid.desktop = VID_Local_GetDesktopProperties (); // Good time to get them.

	Cvar_RegisterVariable (&vid_fullscreen);
	Cvar_RegisterVariable (&vid_width);
	Cvar_RegisterVariable (&vid_height);
#ifdef SUPPORTS_MULTISAMPLE // Baker change
	Cvar_RegisterVariableWithCallback (&vid_multisample, VID_Local_Multisample_f);
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change
	Cvar_RegisterVariableWithCallback (&vid_vsync, VID_Local_Vsync_f);
	Cvar_RegisterVariable (&scr_brightness);

	Cmd_AddCommand ("vid_restart", VID_Restart_f);
	Cmd_AddCommand ("vid_test", VID_Test);
	Cmd_AddCommand ("gl_info", GL_Info_f);

	// Now, if we have -window or anything we don't bother to read the cvars early
	// But we will still read them later.

	videos_cvars_read = VID_Read_Early_Cvars ();

	if ((i = COM_CheckParm("-width")) && i + 1 < com_argc)
		Cvar_SetValue (vid_width.name, (float)atoi(com_argv[i+1]) );

	if ((i = COM_CheckParm("-height")) && i + 1 < com_argc)
		Cvar_SetValue (vid_height.name, (float)atoi(com_argv[i+1]) );

	if (COM_CheckParm("-current"))
	{
		VID_Cvars_Sync_To_Mode (&vid.desktop); // Use desktop sizes.
	}

	if (COM_CheckParm("-window"))
		Cvar_SetValue (vid_fullscreen.name, 0);

	VID_Local_Window_PreSetup ();

// Add the default windowed and default fullscreen modes.
// This does allow a user to do an invalid mode in the a command line, but that's on them.
	VID_MakeMode (MODE_WINDOWED,   &vid.modelist[MODE_WINDOWED]);
	VID_MakeMode (MODE_FULLSCREEN, &vid.modelist[MODE_FULLSCREEN]);

// Add the fullscreen modes
	VID_Local_AddFullscreenModes ();

	if (COM_CheckParm("-fullsbar"))
		vid.fullsbardraw = true;

	vid.initialized = true;

// Now we set the video mode

	VID_SetMode (vid_fullscreen.value ? MODE_FULLSCREEN : MODE_WINDOWED);

	vid.modenum_user_selected = vid.modenum_screen; // Default choice	

#ifdef HARDWARE_GAMMA_BUILD
	VID_Gamma_Init ();
#endif // HARDWARE_GAMMA_BUILD

	VID_Menu_Init(); //johnfitz
}


void VID_Shutdown (void)
{
   	if (!vid.initialized)
		return;

	VID_Local_Window_Renderer_Teardown (TEARDOWN_FULL);

#ifdef HARDWARE_GAMMA_BUILD
	VID_Gamma_Shutdown ();
#endif // HARDWARE_GAMMA_BUILD

	vid.canalttab = false;
}

#ifdef HARDWARE_GAMMA_BUILD
static void VID_Gamma_Table_Make (float gamma, float contrast, unsigned short* ramps)
{
	int	i, c;
	for (i = 0 ; i < 256 ; i++)		// For each color intensity 0-255
	{
		// apply contrast; limit contrast effect to 255 since this is a byte percent
		c = i * contrast;  if (c > 255)	c = 255;

		// apply gamma
		c = 255 * pow((c + 0.5)/255.5, gamma) + 0.5;  c = CLAMP (0, c, 255);

		ramps[i + 0] = ramps[i + 256] = ramps[i + 512] = c << 8;	// Divide by 256
	}
}


cvar_t		vid_hardware_contrast	= {"contrast", "1", CVAR_ARCHIVE};
qboolean gamma_available = false;
unsigned short	systemgammaramp[768]; // restore gamma on exit and/or alt-tab

void VID_Gamma_Shutdown (void)
{
	VID_Local_Gamma_Reset ();

}

void VID_Gamma_Init (void)
{
	gamma_available = VID_Local_IsGammaAvailable (systemgammaramp);
	if (!gamma_available)
	{
		Con_Warning ("Hardware gamma not available (GetDeviceGammaRamp failed)\n");
		return;
	}

	VID_Local_Gamma_Reset ();

	Cvar_RegisterVariable (&vid_hardware_contrast);

	Con_SafePrintf ("Hardware gamma enabled\n");
}



static double hardware_active_enforcer_time_clock = 0;
static int hardware_enforcer_count = 0;

void VID_Gamma_Think (void)
{
	static qboolean	hardware_is_active = false;
	qboolean clock_fire = false;
	static unsigned short ramps[768]; // restore gamma on exit and/or alt-tab
	static	float	previous_gamma = 1.0f, previous_contrast = 1.0f;

	float current_gamma = CLAMP (0.5f, scr_brightness.value, 1.0f);
	float current_contrast = CLAMP (0.5f, vid_hardware_contrast.value, 1.5f);

//	float current_contrast1 = ((1-current_contrast0)*2); // now is 0 to 1 where 1 is full brightness.
//	float current_contrast = 1 + current_contrast1; // Low must be one
	//float current_contrast1 = 0.5 + (1 - current_contrast0); // Baker: translate 1 to 0.5 range to 0.5 to 1.5 range
	//float current_contrast = CLAMP(0.5, current_contrast1, 2.0);


	qboolean hardware_should_be_active = gamma_available && vid.ActiveApp && !vid.Minimized; // && !shutdown;
	qboolean hardware_change = (hardware_is_active != hardware_should_be_active);
	
	// Because quake.rc is going to be read and it will falsely default away from user preference
	qboolean ignore_cvar_change = (host_initialized == true  && host_post_initialized == false);

	// First update the gamma table
	qboolean table_change = (current_gamma != previous_gamma || current_contrast != previous_contrast) && !ignore_cvar_change;
	qboolean actionable_change = hardware_change || (table_change && hardware_should_be_active);

	// bound cvars to menu range
	if (scr_brightness.value < 0.5) Cvar_SetValueQuick (&scr_brightness, 0.5);
	if (scr_brightness.value > 1.0) Cvar_SetValueQuick (&scr_brightness, 1.0);
	if (vid_hardware_contrast.value < 0.5) Cvar_SetValueQuick (&vid_hardware_contrast, 0.5);
	if (vid_hardware_contrast.value > 1.5) Cvar_SetValueQuick (&vid_hardware_contrast, 1.5);


	// Baker: We will set the gamma IF
	// 1) There is a hardware change
	// 2) There is a color change *AND* hardware is active
	// 3) If hardware is active and no color change but timer fires.

	if (!hardware_change && hardware_should_be_active && hardware_active_enforcer_time_clock)
	{
		if (realtime > hardware_active_enforcer_time_clock)
		{
			clock_fire = true;
			hardware_enforcer_count --;
			if (hardware_enforcer_count)
				hardware_active_enforcer_time_clock = realtime + 1;
			else hardware_active_enforcer_time_clock = 0; // Disable
			Con_DPrintf ("Gamma protector clock fired  ... ");
		}
	}

	if (hardware_change)
		Con_DPrintf ("Hardware change detected ... ");

	if (!actionable_change && !clock_fire)
		return;

	if (!hardware_change && table_change)
		Con_DPrintf ("Color change with hardware active ... ");
	
	// If we hit here, a change occurred.
	switch (hardware_should_be_active)
	{
	case true:
		{
			previous_gamma = current_gamma;
			previous_contrast = current_contrast;
			
			if (clock_fire) // Nudge table a little
			{
				static unsigned short tempramps[768]; // restore gamma on exit and/or alt-tab
				VID_Gamma_Table_Make (current_gamma+0.01, current_contrast -0.01, tempramps);
				VID_Local_Gamma_Set (tempramps); // Table niggle?
			}
			VID_Gamma_Table_Make (current_gamma, current_contrast, ramps);
			VID_Local_Gamma_Set (ramps);
		}
		Con_DPrintf ("Custom Gamma set: bright = %g\n", current_contrast);
		hardware_is_active = true;
		break;

	case false:
		VID_Local_Gamma_Set (systemgammaramp);
		Con_DPrintf ("Gamma system set\n");
		hardware_is_active = false;
		hardware_enforcer_count = 0;
		hardware_active_enforcer_time_clock = 0;
		break;
	}

	
}

void VID_Gamma_Clock_Set (void)
{
	Con_DPrintf ("Gamma protector set\n");
	hardware_enforcer_count = 2;
	hardware_active_enforcer_time_clock = realtime + 3;
}

#endif // HARDWARE_GAMMA_BUILD
