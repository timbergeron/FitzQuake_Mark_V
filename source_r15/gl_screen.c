/* Versus Quakespasm:

1. Different method of widescreen correction than Quakespasm.  End result
   is probably identical.
2. Title demos, 
3. Does not default alpha sbar.
4. viddef_t vid lives here in Mark V.  In Quakespasm, it lives in gl_vidsdl.c
   I have it live here because I plan to have a SDL and non-SDL builds for
   at least Windows.  May move to a vid common like file.
5. Quakespasm's "set quick" uses a pointer to the cvar.
6. Some older Intel video cards hate gl_ztrick.  Quakespasm removed my fix.
   See if is a problem. (TODO)
7. Movie capture

Quakespasm has some console background drawing with scr_drawdialog
for new game confirm.  Does this address a situation that can really 
happen?  How can we be disconnected and need the new game confirm dialog???

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

// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net
turn off messages option

the refresh is allways rendered, unless the console is full screen


console is:
	notify lines
	half
	full


*/


int			glx, gly, glwidth, glheight;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

float		oldscreensize, oldfov, oldsbarscale, oldsbaralpha; //johnfitz -- added oldsbarscale and oldsbaralpha

//johnfitz -- new cvars
cvar_t		scr_menuscale = {"scr_menuscale", "1", CVAR_ARCHIVE};
cvar_t		scr_sbarscale = {"scr_sbarscale", "1", CVAR_ARCHIVE};
cvar_t		scr_sbaralpha = {"scr_sbaralpha", "1", CVAR_ARCHIVE};
cvar_t		scr_conwidth = {"scr_conwidth", "0", CVAR_ARCHIVE};
cvar_t		scr_conscale = {"scr_conscale", "1", CVAR_ARCHIVE};
cvar_t		scr_crosshairscale = {"scr_crosshairscale", "1", CVAR_ARCHIVE};
cvar_t		scr_showfps = {"scr_showfps", "0", CVAR_NONE};
cvar_t		sbar_clock = {"scr_clock", "0", CVAR_NONE};
//johnfitz

cvar_t		scr_viewsize = {"viewsize","100", CVAR_ARCHIVE};
cvar_t		scr_fov = {"fov","90",CVAR_ARCHIVE};	// 10 - 170
//cvar_t		scr_fov_adapt = {"fov_adapt","1",CVAR_ARCHIVE};	// 10 - 170
cvar_t		scr_conspeed = {"scr_conspeed","300",CVAR_ARCHIVE};
cvar_t		scr_centertime = {"scr_centertime","2",CVAR_NONE};
cvar_t		scr_showram = {"showram","1",CVAR_NONE};
cvar_t		scr_showturtle = {"showturtle","0",CVAR_NONE};
cvar_t		scr_showpause = {"showpause","1",CVAR_NONE};
cvar_t		scr_printspeed = {"scr_printspeed","8",CVAR_NONE};
cvar_t		gl_triplebuffer = {"gl_triplebuffer", "1", CVAR_ARCHIVE };
cvar_t		scr_brightness = {"gamma", "1", CVAR_ARCHIVE};

qboolean		scr_skipupdate;

extern	cvar_t	crosshair;

qboolean	scr_initialized;		// ready to draw

qpic_t		*scr_ram;
qpic_t		*scr_net;
qpic_t		*scr_turtle;

int			clearconsole;
int			clearnotify;


vrect_t		scr_vrect;

qboolean	scr_disabled_for_loading;
qboolean	scr_drawloading;
float		scr_disabled_time;

int	scr_tileclear_updates = 0; //johnfitz

void SCR_ScreenShot_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (const char *str) //update centerprint data
{
	q_strlcpy (scr_centerstring, str, sizeof(scr_centerstring));
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	str = scr_centerstring;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString (void) //actually do the drawing
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

	GL_SetCanvas (CANVAS_MENU); //johnfitz

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = 200*0.35;	//johnfitz -- 320x200 coordinate system
	else
		y = 48;

	do
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (320 - l*8)/2;	//johnfitz -- 320x200 coordinate system

// Baker: detect crosshair scenario		
		
		for (j=0 ; j<l ; j++, x+=8)
		{
			Draw_Character (x, y, start[j]);	//johnfitz -- stretch overlays
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;
	if (cl.paused) //johnfitz -- don't show centerprint during a pause
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFovy
====================
*/
float CalcFovy (float fov_x, float width, float height)
{
    float   a, x;

    if (fov_x < 1 || fov_x > 179)
            Host_Error ("Bad fov: %f", fov_x);

    x = width/tan(fov_x/360*M_PI);
    a = atan (height/x);
    a = a*360/M_PI;
    return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
	float		size, scale; //johnfitz -- scale

	float		fov_base;

	vid.recalc_refdef = 0;

// force the status bar to redraw
	Sbar_Changed ();

	scr_tileclear_updates = 0; //johnfitz

// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set ("viewsize","30");
	if (scr_viewsize.value > 120)
		Cvar_Set ("viewsize","120");

// bound fov
	if (scr_fov.value < 10)
		Cvar_Set ("fov","10");
	if (scr_fov.value > 170)
		Cvar_Set ("fov","170");

	//johnfitz -- rewrote this section
	size = scr_viewsize.value;
	scale = CLAMP (1.0, scr_sbarscale.value, (float)glwidth / 320.0);

	if (size >= 120 || cl.intermission || scr_sbaralpha.value < 1) //johnfitz -- scr_sbaralpha.value
		sb_lines = 0;
	else if (cls.titledemo)
		sb_lines = 0;
	else if (size >= 110)
		sb_lines = 24 * scale;
	else
		sb_lines = 48 * scale;

	if (cls.titledemo)
		size = 1; // Title demos are maximum viewsize
	else
		size = q_min(scr_viewsize.value, 100) / 100;
	//johnfitz

	if (developer.value > 10000)
	{
		// Baker: Sadly this little block of code is necessary
		// to prevent the compiler I am using from performing an optimization below that is 
		// detrimental!  I don't know why.  Otherwise a calculation below will be off by 1
		// for 640x480 resolution --- as an example.  All resolutions are affected.
		Con_Printf ("Size is %g\n", size);
	}
	//johnfitz -- rewrote this section
	r_refdef.vrect.width = q_max(glwidth * size, 96); //no smaller than 96, for icons
	r_refdef.vrect.height = q_min(glheight * size, glheight - sb_lines); //make room for sbar
	r_refdef.vrect.x = (glwidth - r_refdef.vrect.width)/2;
	r_refdef.vrect.y = (glheight - sb_lines - r_refdef.vrect.height)/2;
	//johnfitz

	if (cls.titledemo)
		fov_base = 90.0f;
	else
		fov_base = scr_fov.value;

	#define SCREEN_CORRECTION_ASPECT 1.33333f
//	if (scr_fov_adapt.value)
//	{
		r_refdef.fov_y = CalcFovy (fov_base, r_refdef.vrect.height * SCREEN_CORRECTION_ASPECT, r_refdef.vrect.height);
		r_refdef.fov_x = CalcFovy (r_refdef.fov_y, r_refdef.vrect.height, r_refdef.vrect.width);	
//	}
//	else
//	{
		r_refdef.fov_x = scr_fov.value;
		r_refdef.fov_y = CalcFovy (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);
//	}



	scr_vrect = r_refdef.vrect;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize.value+10);
	vid.recalc_refdef = 1;
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize.value-10);
	vid.recalc_refdef = 1;
}

/*
==================
SCR_Conwidth_f -- johnfitz -- called when scr_conwidth or scr_conscale changes
==================
*/
void SCR_Conwidth_f (cvar_t *var)
{
	vid.conwidth = (scr_conwidth.value > 0) ? (int)scr_conwidth.value : (scr_conscale.value > 0) ? (int)(vid.screen.width/scr_conscale.value) : vid.screen.width;
	vid.conwidth = CLAMP (320, vid.conwidth, vid.screen.width);
	vid.conwidth &= 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.screen.height / vid.screen.width;
}

//============================================================================

/*
==================
SCR_LoadPics -- johnfitz
==================
*/
void SCR_LoadPics (void)
{
	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");
}
/*
==================
SCR_Init
==================
*/

void SCR_Init (void)
{
	//johnfitz -- new cvars
	Cvar_RegisterVariable (&scr_menuscale);
	Cvar_RegisterVariable (&scr_sbarscale);
	Cvar_RegisterVariable (&scr_sbaralpha);
	Cvar_RegisterVariableWithCallback (&scr_conwidth, SCR_Conwidth_f);
	Cvar_RegisterVariableWithCallback (&scr_conscale, SCR_Conwidth_f);
	Cvar_RegisterVariable (&scr_crosshairscale);
	Cvar_RegisterVariable (&scr_showfps);
	Cvar_RegisterVariable (&sbar_clock);
	//johnfitz
	Cvar_RegisterVariable (&scr_fov);
//	Cvar_RegisterVariable (&scr_fov_adapt);
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_showram);
	Cvar_RegisterVariable (&scr_showturtle);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&gl_triplebuffer);

	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);
#ifdef SUPPORTS_WINDOWS_MENUS // Baker change
// To be able to invoke the loading plague via a command.
	Cmd_AddCommand ("_showloading", SCR_ShowLoading_f);
#endif // Baker change +

	SCR_LoadPics (); //johnfitz
//#pragma message ("I think dedicated no longer reaches here ... fixme?")
#ifdef SUPPORTS_AVI_CAPTURE // Baker change
	if (!isDedicated)
		Movie_Init ();
#endif // Baker change +
	scr_initialized = true;
}

//============================================================================

/*
==============
SCR_DrawFPS -- johnfitz
==============
*/
void SCR_DrawFPS (void)
{
	static double	oldtime = 0;
	static double	lastfps = 0;
	static int		oldframecount = 0;
	double	elapsed_time;
	int	frames;

	elapsed_time = realtime - oldtime;
	frames = r_framecount - oldframecount;

	if (elapsed_time < 0 || frames < 0)
	{
		oldtime = realtime;
		oldframecount = r_framecount;
		return;
	}
	// update value every 3/4 second
	if (elapsed_time > 0.75)
	{
		lastfps = frames / elapsed_time;
		oldtime = realtime;
		oldframecount = r_framecount;
	}

	if (scr_showfps.value)
	{
		char			st[16];
		int				x, y;
		q_snprintf (st, sizeof(st), "%4.0f fps", lastfps);
		x = 320 - (strlen(st)<<3);
		y = 200 - 8;
		if (sbar_clock.value) y -= 8; //make room for clock
		GL_SetCanvas (CANVAS_BOTTOMRIGHT);
		Draw_String (x, y, st);
		scr_tileclear_updates = 0;
	}

}

/*
==============
SCR_DrawDevStats
==============
*/
void SCR_DrawDevStats (void)
{
	char	str[40];
	int		y = 25-9; //9=number of lines to print
	int		x = 0; //margin

	if (!devstats.value)
		return;

	GL_SetCanvas (CANVAS_BOTTOMLEFT);

	Draw_Fill (x, y*8, 19*8, 9*8, 0, 0.5); //dark rectangle

	sprintf (str, "devstats |Curr Peak");
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "---------+---------");
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Edicts   |%4i %4i", dev_stats.edicts, dev_peakstats.edicts);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Packet   |%4i %4i", dev_stats.packetsize, dev_peakstats.packetsize);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Visedicts|%4i %4i", dev_stats.visedicts, dev_peakstats.visedicts);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Efrags   |%4i %4i", dev_stats.efrags, dev_peakstats.efrags);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Dlights  |%4i %4i", dev_stats.dlights, dev_peakstats.dlights);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Beams    |%4i %4i", dev_stats.beams, dev_peakstats.beams);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Tempents |%4i %4i", dev_stats.tempents, dev_peakstats.tempents);
	Draw_String (x, (y++)*8-x, str);
}

/*
==============
SCR_DrawRam
==============
*/
void SCR_DrawRam (void)
{
	if (!scr_showram.value)
		return;

// Baker: r_cache_thrash shows RAM.  r_cache_thrash is WinQuake only.
// Result: GLQuake never shows RAM since condition can never be true.

	//if (!r_cache_thrash)
	return;

	GL_SetCanvas (CANVAS_DEFAULT); //johnfitz

	Draw_Pic (scr_vrect.x+32, scr_vrect.y, scr_ram);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int	count;

	if (!scr_showturtle.value)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	GL_SetCanvas (CANVAS_DEFAULT); //johnfitz

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (cl.paused && sv.active) // Don't do this for active server in pause.
		return;

	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	GL_SetCanvas (CANVAS_DEFAULT); //johnfitz

	Draw_Pic (scr_vrect.x+64, scr_vrect.y, scr_net);
}

/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	qpic_t	*pic;

	if (!cl.paused)
		return;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	GL_SetCanvas (CANVAS_MENU); //johnfitz

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ( (320 - pic->width)/2, (240 - 48 - pic->height)/2, pic); //johnfitz -- stretched menus

	scr_tileclear_updates = 0; //johnfitz
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	qpic_t	*pic;

	if (!scr_drawloading)
		return;

	GL_SetCanvas (CANVAS_MENU); //johnfitz

	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ( (320 - pic->width)/2, (240 - 48 - pic->height)/2, pic); //johnfitz -- stretched menus

	scr_tileclear_updates = 0; //johnfitz
}

/*
==============
SCR_DrawCrosshair -- johnfitz
==============
*/
void SCR_DrawCrosshair (void)
{
	if (!crosshair.value)
		return;
	if (cls.titledemo)
		return;

	GL_SetCanvas (CANVAS_CROSSHAIR);
	
	if (!scr_crosshairscale.value) // Dot crosshair
		Draw_Character (-4, -4, 0); //0,0 is center of viewport
	else Draw_Character (-4, -4, '+'); // Standard Quake crosshair
}



//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	//johnfitz -- let's hack away the problem of slow console when host_timescale is <0
	extern float frame_timescale;

	//johnfitz
	extern float scr_con_size;


	Con_CheckResize ();

	if (scr_drawloading)
		return;		// never a console with loading plaque

// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = glheight; //full screen //johnfitz -- glheight instead of vid.height
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
	{
		scr_conlines = glheight * scr_con_size;
		scr_conlines = CLAMP (30, scr_conlines, glheight);
	}
	else scr_conlines = 0; //none visible


	if (scr_conlines < scr_con_current)
	{ // Retract console
		scr_con_current -= scr_conspeed.value*host_frametime/frame_timescale; //johnfitz -- timescale
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{ // Deploy console
		scr_con_current += scr_conspeed.value*host_frametime/frame_timescale; //johnfitz -- timescale
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
		Sbar_Changed ();

	if (!con_forcedup && scr_con_current)
		scr_tileclear_updates = 0; //johnfitz
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{
		Con_DrawConsole (scr_con_current, true);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			if (!cls.titledemo) // Title demos do not get notify text
				Con_DrawNotify ();	// only draw notify in game
	}
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

/*
==================
SCR_ScreenShot_f -- johnfitz -- rewritten to use Image_WriteTGA
==================
*/
void SCR_ScreenShot_f (void)
{
	byte	*buffer;
	char	tganame[16];  //johnfitz -- was [80]
	char	checkname[MAX_OSPATH];
	int		i;

	if (Cmd_Argc () == 2 && Q_strcasecmp(Cmd_Argv(1), "copy") == 0)
	{
		SCR_ScreenShot_Clipboard_f ();	// Copy to clipboard instead
		return;
	}

	if (Cmd_Argc () == 2) // Explicitly named
	{
		q_snprintf (tganame, sizeof(tganame), "%s.tga", Cmd_Argv (1));
		goto takeshot;
	}

// find a file name to save it to
	for (i=0; i<10000; i++)
	{
		q_snprintf (tganame, sizeof(tganame), "fitz%04i.tga", i);
		q_snprintf (checkname, sizeof(checkname), "%s/%s", com_gamedir, tganame);
		if (Sys_FileTime(checkname) == -1)
			break;	// file doesn't exist
	}
	if (i == 10000)
	{
		Con_Printf ("SCR_ScreenShot_f: Couldn't find an unused filename\n");
		return;
 	}

takeshot:
//get data
	buffer = (byte *)malloc(glwidth*glheight*3);
	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);

// now write the file
	if (Image_WriteTGA (tganame, buffer, glwidth, glheight, 24, false))
	{
		Recent_File_Set_RelativePath (tganame);
		Con_Printf ("Wrote %s\n", tganame);
	}
	else
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n");

	free (buffer);
}


static void FlipBuffer (byte *buffer, const int columns, const int rows, const int BytesPerPixel)	// Flip the image because of GL's up = positive-y
{
    int		bufsize = columns * BytesPerPixel; // bufsize=widthBytes;

	byte	*tb1 = malloc (bufsize);
	byte	*tb2 = malloc (bufsize);
    int		i, offset1, offset2;

    for (i = 0; i < (rows + 1) / 2;i++)
    {
        offset1 = i * bufsize;
        offset2 =((rows - 1) - i) * bufsize;

        memcpy (tb1, buffer + offset1, bufsize);
        memcpy (tb2, buffer + offset2, bufsize);
        memcpy (buffer + offset1, tb2, bufsize);
        memcpy( buffer + offset2, tb1, bufsize);
    }

	free (tb1);
	free (tb2);
    return;
}



void SCR_ScreenShot_Clipboard_f (void)
{
	int		buffersize = glwidth * glheight * 4; // 4 bytes per pixel
	byte	*buffer = malloc (buffersize);
//	int		i;

//get data
	glReadPixels (glx, gly, glwidth, glheight, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buffer);

// We are upside down flip it.
	FlipBuffer (buffer, glwidth, glheight, 4 /* bytes per pixel */);

// FIXME: No gamma correction of screenshots in Fitz?
	Sys_Image_BGRA_To_Clipboard (buffer, glwidth, glheight, buffersize);

	Con_Printf("Screenshot to clipboard\n");

	free (buffer);

}


//=============================================================================


/*
===============
SCR_BeginLoadingPlaque

================
*/

void SCR_BeginLoadingPlaque_Force_NoTransition (void)
{
	S_StopAllSounds (true);
	CDAudio_Stop (); // Stop the CD music

	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
}

// for live play level to level and reconnect
// where it should draw the loading plaque
void SCR_BeginLoadingPlaque_Force_Transition (void)
{
	S_StopAllSounds (true);
	CDAudio_Stop (); // Stop the CD music

	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	Sbar_Changed ();
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
}


void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);
	CDAudio_Stop (); // Stop the CD music

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;

// redraw with no console and the loading plaque
	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	Sbar_Changed ();
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;

}



/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = false;
	Con_ClearNotify ();
}

//=============================================================================

const char	*scr_notifystring;
qboolean	scr_drawdialog;

void SCR_DrawNotifyString (void)
{
	const char	*start;
	int		l;
	int		j;
	int		x, y;

	GL_SetCanvas (CANVAS_MENU); //johnfitz

	start = scr_notifystring;

	y = 200 * 0.35; //johnfitz -- stretched overlays

	do
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (320 - l*8)/2; //johnfitz -- stretched overlays
		for (j=0 ; j<l ; j++, x+=8)
			Draw_Character (x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.
==================
*/
int SCR_ModalMessage (const char *text, float timeout) //johnfitz -- timeout
{
	double time1, time2; //johnfitz -- timeout

	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;

// draw a fresh screen
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;

	S_ClearBuffer ();		// so dma doesn't loop current sound

	time1 = Sys_DoubleTime () + timeout; //johnfitz -- timeout
	time2 = 0.0f; //johnfitz -- timeout

	do
	{
		key_count = -1;		// wait for a key down and up
		Sys_SendKeyEvents ();
		Sys_Sleep(16);
		if (timeout) time2 = Sys_DoubleTime (); //johnfitz -- zero timeout means wait forever.
	} while (key_lastpress != 'y' &&
			 key_lastpress != 'n' &&
			 key_lastpress != K_ESCAPE &&
			 time2 <= time1);

	// make sure we don't ignore the next keypress
	if (key_count < 0)
		key_count = 0;

//	SCR_UpdateScreen (); //johnfitz -- commented out

	//johnfitz -- timeout
	if (time2 > time1)
		return false;
	//johnfitz

	return key_lastpress == 'y';
}


//=============================================================================

//johnfitz -- deleted SCR_BringDownConsole


/*
==================
SCR_TileClear -- johnfitz -- modified to use glwidth/glheight instead of vid.width/vid.height
                             also fixed the dimentions of right and top panels
							 also added scr_tileclear_updates
==================
*/
void SCR_TileClear (void)
{
	if (scr_tileclear_updates >= vid.numpages && !gl_clear.value && !renderer.isIntelVideo) //intel video workarounds from Baker
		return;
	scr_tileclear_updates++;

	if (r_refdef.vrect.x > 0)
	{
		// left
		Draw_TileClear (0,
						0,
						r_refdef.vrect.x,
						glheight - sb_lines);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width,
						0,
						glwidth - r_refdef.vrect.x - r_refdef.vrect.width,
						glheight - sb_lines);
	}

	if (r_refdef.vrect.y > 0)
	{
		// top
		Draw_TileClear (r_refdef.vrect.x,
						0,
						r_refdef.vrect.width,
						r_refdef.vrect.y);
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
						r_refdef.vrect.y + r_refdef.vrect.height,
						r_refdef.vrect.width,
						glheight - r_refdef.vrect.y - r_refdef.vrect.height - sb_lines);
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen (void)
{
	vid.numpages = (gl_triplebuffer.value) ? 3 : 2;

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = false;
			Con_Printf ("load failed.\n");
		}
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet


	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);

	//
	// determine size of refresh window
	//

	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

//	if (oldfovadapt != scr_fov_adapt.value)
//	{
//		oldfovadapt = scr_fov_adapt.value;
//		vid.recalc_refdef = true;
//	}

	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	//johnfitz -- added oldsbarscale and oldsbaralpha
	if (oldsbarscale != scr_sbarscale.value)
	{
		oldsbarscale = scr_sbarscale.value;
		vid.recalc_refdef = true;
	}

	if (oldsbaralpha != scr_sbaralpha.value)
	{
		oldsbaralpha = scr_sbaralpha.value;
		vid.recalc_refdef = true;
	}
	//johnfitz

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();

	V_RenderView ();

	GL_Set2D ();

	//FIXME: only call this when needed
	SCR_TileClear ();

	if (scr_drawdialog) //new game confirm
	{
		Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
	}
	else if (scr_drawloading) //loading
	{
		SCR_DrawLoading ();
		Sbar_Draw ();
		M_Draw (); // drawloading and draw menu can happen now
	}
	else if (cl.intermission == 1 && key_dest == key_game) //end of level
	{
		Sbar_IntermissionOverlay ();
	}
	else if (cl.intermission == 2 && key_dest == key_game) //end of episode
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	}
	else
	{
		TexturePointer_Draw ();
		SCR_DrawRam ();
		SCR_DrawNet ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		SCR_CheckDrawCenterString ();
		SCR_DrawCrosshair (); // Baker:moved
		Sbar_Draw ();
		SCR_DrawDevStats (); //johnfitz
		SCR_DrawFPS (); //johnfitz
//		SCR_DrawClock (); //johnfitz
		SCR_DrawConsole ();
		M_Draw ();
	}

	V_UpdateBlend (); //johnfitz -- V_UpdatePalette cleaned up and renamed
#ifndef HARDWARE_GAMMA_BUILD
	SCR_BrightenScreen ();
#endif

#ifdef SUPPORTS_AVI_CAPTURE
	Movie_UpdateScreen ();
#endif
	GL_EndRendering ();
}


#ifndef HARDWARE_GAMMA_BUILD

#if 0
/*
===================
SCR_BrightenScreen

Baker: From Reckless who had it from MH.

Enables setting of brightness without having to do any mondo fancy shite.
It's assumed that we're in the 2D view for this...

Basically, what it does is multiply framebuffer colours by an incoming constant between 0 and 2
===================
*/

// host_initialized == true  && host_post_initialized == false, we ignore any gamma change.
// because quake.rc's default.cfg is going to default us.  But we already read it early.
void SCR_BrightenScreen (void)
{
	static float lastbright;
	qboolean ignore_change = (host_initialized == true  && host_post_initialized == false);

	float brightfactor_base = 0.5 + (1 - scr_brightness.value)/2; // Baker: translate 1 to 0.5 range to 0.5 to 1.5 range
	float brightfactor0 = CLAMP(0.5, brightfactor_base, 1.0);
	
	float brightfactor = ignore_change ? lastbright : brightfactor0;

    // divide by 2 cos the blendfunc will sum src and dst
    const GLfloat brightblendcolour[4] = {0, 0, 0, 0.5f * brightfactor};
    const GLfloat constantwhite[4] = {1, 1, 1, 1};

	if (lastbright != brightfactor)
	{
		lastbright = brightfactor;
	} 

	// MH: don't trust == with floats, don;t bother if it's 1 cos it does nothing to the end result!!!
    if (brightfactor > 0.49 && brightfactor < 0.51)
    {
        return;
    }

	GL_DisableMultitexture ();
	GL_SetCanvas (CANVAS_DEFAULT);

	glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
    glColor4fv (constantwhite);
    glEnable (GL_BLEND);
    glDisable (GL_ALPHA_TEST);
    glBlendFunc (GL_DST_COLOR, GL_SRC_COLOR);

	// MH: combine hack...
    // this is weird cos it uses a texture but actually doesn't - the parameters of the
    // combiner function only use the incoming fragment colour and a constant colour...
    // you could actually bind any texture you care to mention and get the very same result...
    // i've decided not to bind any at all...
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT); // 1
    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE); // 2
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_CONSTANT_EXT); // 3
    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_ALPHA); // 4
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT); // 5
    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR); // 6
    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, brightblendcolour); // 7

    glBegin (GL_QUADS);
		glTexCoord2f (0, 0); glVertex2f (glx, gly);
    	glTexCoord2f (0, 1); glVertex2f (glx, glheight);
		glTexCoord2f (1, 1); glVertex2f (glwidth, glheight);
    	glTexCoord2f (1, 0); glVertex2f (glwidth, gly);

    glEnd ();

	// Restore

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); // 1
    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE); // 2
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE); // 3
    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR); // 4
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT); // 5
    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR); // 6
    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constantwhite); // 7
    
	glDisable (GL_BLEND); //
    glEnable (GL_ALPHA_TEST);
    glColor4fv (constantwhite);
}

#else

void SCR_BrightenScreen (void)
{

	float		f;

	// Baker: We don't care if it is enabled, just if we are using it?
//	if (scr_brightness.value <= 1)	return;		// Because it won't do anything, 1 to any power is 1
	float current_contrast0 = CLAMP (0.5f, scr_brightness.value, 1.0f);
	float current_contrast1 = ((1-current_contrast0)*2); // now is 0 to 1 where 1 is full brightness.
	float current_contrast = 1 + current_contrast1; // Low must be one


	{	// Range limit cvars
//		float effective_gamma =		CLAMP (vid_min_gamma, vid_brightness_gamma.floater, vid_max_gamma);
#define vid_min_contrast 1.0f
#define vid_max_contrast 4.0f
		float effective_contrast =	CLAMP (vid_min_contrast, current_contrast, vid_max_contrast);

//		if (!(vid_brightness_gamma.floater == effective_gamma))			Cvar_SetFloatByRef (&vid_brightness_gamma, effective_gamma);
//		if (!(vid_brightness_contrast.floater == effective_contrast))	Cvar_SetFloatByRef (&vid_brightness_contrast, effective_contrast);

		// Translate gamma to range

//		effective_gamma = (effective_gamma-vid_min_gamma)/(vid_max_gamma-vid_min_gamma)*0.95 + 0.05;

		f = current_contrast;
		f = pow	(f, 1); // Baker: change to vid_hwgamma


	}

	GL_SetCanvas (CANVAS_DEFAULT);
	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glBlendFunc (GL_DST_COLOR, GL_ONE);


	glBegin (GL_QUADS);
	while (f > 1)
	{
		if (f >= 2)
			glColor3f (1,1,1);
		else
			glColor3f (f - 1, f - 1, f - 1);

		// Baker: Remember, this is using whatever canvas we have setup!!!!!!
		glVertex2f (0, 0);
		glVertex2f (vid.screen.width, 0);				// Baker: shouldn't this be glwidth and glx gly and so forth? NO.  Canvas!
		glVertex2f (vid.screen.width, vid.screen.height);	// Baker: This does need to be the entire screen.  Why did you think canvas?
		glVertex2f (0, vid.screen.height);			// Baker: However we need to set the viewport to full client area here
		f *= 0.5;
	}
	glEnd ();

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glColor4f (1,1,1,1);
}

#endif

#endif // !HARDWARE_GAMMA_BUILD