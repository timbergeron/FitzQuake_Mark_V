/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/*
================================================================================================================================

DIRECTSHOW INTERFACE

This runs supplementary to, and not instead of, the stock Q1 CD interface.  CD Audio is preferred but - if not initialized,
DirectShow will take over in it's place.

================================================================================================================================
*/

// README - README - README - README - README - README - README - README
// Baker note: In Visual C++ 6 the file has these attributes
//# ADD CPP /I "./dxsdk/sdk8/include"
//# SUBTRACT CPP /I ".\dxsdk\sdk\inc"
// README - README - README - README - README - README - README - README

#if _MSC_VER > 1200 // MSVC6 does not need
#define POINTER_64 __ptr64 // Vile Baker hack ... explanation ...

/*
Visual Studio C++ Express 2008 includes "Additional Include Paths" at higher priority than the standard include paths.  Some sort
of issue with one of the includes DirectX8 includes causes POINTER_64 to not get defined.  The accepted remedies
all suck.  I'll try to remember to list the link MSDN discussion on this here.

But what I did, was made the "Additional Include Paths" specific to this single file instead of the project on
a whole.
*/
#endif

#include "quakedef.h"
#ifdef SUPPORTS_MP3_MUSIC

#include "winquake.h"



#include <objbase.h>

#define COBJMACROS
#include <dshow.h>
#pragma comment (lib, "./dxsdk/sdk8/lib/strmiids.lib")

// this needs to be defined in gl_vidnt.c as well
IGraphBuilder *pGraph = NULL;
IMediaControl *pControl = NULL;
IMediaEventEx *pEvent = NULL;
IBasicAudio	  *pAudio = NULL;
IMediaSeeking *pSeek = NULL;

qboolean command_line_disabled;
qboolean Playing;
qboolean Initialized;
qboolean StayOff;
qboolean Paused;

float Volume_level;
char last_track [MAX_OSPATH];

void MediaPlayer_ChangeVolume (float newvolume);
void MediaPlayer_Stop (void);

void WaitForFilterState (OAFilterState DesiredState)
{
	OAFilterState MP3FS;
	HRESULT hr;

	while (1)
	{
        hr = IMediaControl_GetState(pControl, 1, &MP3FS);

		if (FAILED (hr)) continue;
		if (MP3FS == DesiredState) return;
	}
}


qboolean sMediaPlayer_Init (void)
{
	if (COM_CheckParm("-nosound") || StayOff)
		return false;
	else
	{
		HRESULT hr = CoInitialize (NULL);

		if (FAILED (hr))
		{
			Con_Printf ("ERROR - Could not initialize COM library");
			return false;
		}

		Initialized = true;
		Con_DPrintf ("Directshow Music initialized\n");
	}

	return true;
}


void MediaPlayer_Shutdown (void)
{
	if (!Initialized) return;

//	Con_DPrintf ("********************* SHUTDOWN MP3 TRACK *********************\n");
	// stop anything that's playing
	if (Playing)
	{

		IMediaEventEx_SetNotifyWindow(pEvent, (OAHWND) NULL, 0, 0);
		IMediaControl_Stop(pControl);
		WaitForFilterState (State_Stopped);
		IMediaControl_Release(pControl);
		IMediaEventEx_Release(pEvent);
		IBasicAudio_Release(pAudio);
		IMediaSeeking_Release(pSeek); // 1
		IGraphBuilder_Release(pGraph);

		// nothing playing now
		Playing = false;
		Con_DPrintf ("MP3 Tracks: Stop\n");
	}
//	return;

	CoUninitialize ();
	Con_DPrintf ("MP3 Tracks: Shutdown\n");

	// Reset stuff
	pGraph = NULL;
	pControl = NULL;
	pEvent = NULL;
	pAudio = NULL;
	pSeek = NULL;

	Initialized = Playing = Paused = false;

#if 0 //def _DEBUG
	Sys_WindowCaptionSet (NULL);
#endif
}

void MediaPlayer_Pause (void)
{

	if (!Initialized) return; // don't try anything if we couldn't start COM
	if (!Playing) return; // don' t try to pause if we're not even playing!!!
	if (Paused) return;

	// don't wait for the filter states here
    IMediaControl_Pause(pControl);

	Paused = true;
    Con_DPrintf ("MP3 Tracks: Pause\n");
}

void MediaPlayer_Resume (void)
{
	if (!Initialized) return; // don't try anything if we couldn't start COM
	if (!Playing) return; // don' t try to pause if we're not even playing!!!
	if (!Paused) return;

	// don't wait for the filter states here
    IMediaControl_Run(pControl);

	Paused = false;
    Con_DPrintf ("MP3 Tracks: Resume\n");
}

void MediaPlayer_RefreshVolume (void)
{
	long db = 0;

	if (!Initialized) return; // don't try anything if we couldn't start COM
	if (!Playing) return; // don' t try to pause if we're not even playing!!!


	if (Volume_level <= 0)
		db = -10000;
	else if (Volume_level >= 1)
		db = 0;
	else db = log10 (Volume_level) * 2000;

	IBasicAudio_put_Volume	(pAudio, db);
	Con_DPrintf ("MP3 Tracks: Refresh volume\n");
}


void MediaPlayer_Command_Line_Disabled (void)
{
	command_line_disabled = true;


}


qboolean MediaPlayer_Play (int tracknum, qboolean looping)
{

	char track_file[MAX_QPATH];
	char *absolute_filename;
	wchar_t W_absolute_filename[1024];
//	int   i;

	// Disabled by a command line option like -nosound, -nocdaudio or "-dedicated server".
	if (command_line_disabled)
		return false;

	Playing = false;

	if (!Initialized)
	{
		sMediaPlayer_Init (); // Try to turn it on
		if (!Initialized)
		{
			Con_Printf ("Couldn't turn it on\n");
			return false; // Couldn't turn it on
		}
	}

	// Build relative file name
	q_snprintf (track_file, sizeof(track_file), "music/track%02i", tracknum);
	COM_DefaultExtension (track_file, ".mp3", sizeof(track_file));

	// Build absolute file name
	absolute_filename = COM_FindFile_NoPak (track_file);

	if (!absolute_filename)
	{
		Con_Printf ("Track: %s not found\n", track_file);
		MediaPlayer_Shutdown ();
		return false; // Doesn't exist
	}

	strcpy (last_track, absolute_filename);
	Con_Printf ("Current music track: %s\n", track_file);

	mbstowcs (W_absolute_filename, absolute_filename, MAX_OSPATH);

	do
	{
		#define FAILED_SETUP(x) { Con_Printf (x); return false; }
		HRESULT hr = S_OK;

		// Create the filter graph manager and query for interfaces.
		if (FAILED (hr = CoCreateInstance (&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **) &pGraph)))
			FAILED_SETUP ("CoCreateInstance Failed");
		if (FAILED (hr =IGraphBuilder_QueryInterface(pGraph, &IID_IMediaControl, (void **) &pControl)))
			FAILED_SETUP ("IID_IMediaControl Failed");
		if (FAILED (hr =IGraphBuilder_QueryInterface(pGraph, &IID_IMediaEventEx, (void **) &pEvent)))
			FAILED_SETUP ("IID_IMediaEventEx Failed");
		if (FAILED (hr =IGraphBuilder_QueryInterface(pGraph, &IID_IBasicAudio, (void **) &pAudio)))
			FAILED_SETUP ("IID_IBasicAudio Failed");
		if (FAILED (hr =IGraphBuilder_QueryInterface(pGraph, &IID_IMediaSeeking, (void **) &pSeek)))
			FAILED_SETUP ("IID_IMediaSeeking Failed");

		if (FAILED (hr =IGraphBuilder_RenderFile(pGraph, W_absolute_filename, NULL))) // Baker _Render can point into a pak?
			FAILED_SETUP ("IGraphBuilder_RenderFile Failed");

		// send events through the standard window event handler
	   if (FAILED (hr = IMediaEventEx_SetNotifyWindow(pEvent, (OAHWND) wplat.mainwindow, WM_GRAPHNOTIFY, 0)))
			FAILED_SETUP ("IMediaEventEx_SetNotifyWindow Failed");

		// Run the graph.
	   if (FAILED (hr = IMediaControl_Run(pControl) ))
			FAILED_SETUP ("IMediaControl_Run Failed");

		#undef FAILED_SETUP
	} while (0);

	// tell us globally that we can play OK
	Playing = true;
	Paused = false;

	// wait until it reports playing
	WaitForFilterState (State_Running);

	MediaPlayer_RefreshVolume ();
	// examples in the SDK will wait for the event to complete here, but this is totally inappropriate for a game engine.

	Con_DPrintf ("MP3 Tracks: Playing\n");
//	Con_DPrintf ("********************* PLAYING MP3 TRACK *********************\n");
	return Playing;

}

void MediaPlayer_Update (void)
{
#if 0 //def _DEBUG
	if (!Initialized) return; // don't try anything if we couldn't start COM
	if (!Playing) return; // don't try to stop if we're not even playing!!!

	{
		static int k;
		LONGLONG pos1, pos2;
		k++;
		IMediaSeeking_GetPositions (pSeek, &pos1, &pos2);
		Sys_WindowCaptionSet (   va("%i - CD track position: pos1 %i pos2 %i", k, (int)pos1/10000000, (int)(pos2/10000000) ) );
	}
#endif
}


qboolean MediaPlayer_Force_On (void)
{
	if (command_line_disabled)
		return false;

	if (COM_CheckParm("-nosound"))
		return false;

	if (COM_CheckParm("-nocdaudio"))
		return false;

	StayOff = false;
	Con_Printf ("MP3 Tracks: Forced on\n");
	return true;
}


void MediaPlayer_Force_Off (void)
{
	StayOff = true;
	Con_Printf ("MP3 Tracks: Forced off\n");
}

void MediaPlayer_ChangeVolume (float newvolume)
{
	if (!Initialized) return; // don't try anything if we couldn't start COM
	if (!Playing) return; // don' t try to pause if we're not even playing!!!

	// put_Volume uses an exponential decibel-based scale going from -10000 (no sound) to 0 (full volume)
	// each 100 represents 1 db.  i could do the maths, but this is faster and more maintainable.
	// whoever in ms designed this must have really prided themselves on being so technically correct.  bastards.
	// decibels are great if you're an audio engineer, but if you're just writing a simple sliding volume control...

	Volume_level = newvolume;
	MediaPlayer_RefreshVolume ();

	Con_DPrintf ("MP3 Tracks: Effective volume is %f\n", newvolume);
}

/*
===================
MesgMP3DShow

MP3 Message handler.  The only one we're interested in is a stop message.
Everything else is handled within the engine code.
===================
*/
void MediaPlayer_Message (int looping)
{
	LONG      evCode;
	LONG_PTR   evParam1, evParam2;
    HRESULT      hr = S_OK;

	if (!Initialized) return; // don't try anything if we couldn't start COM
	if (!Playing) return; // don' t try to pause if we're not even playing!!!

#if 0 //def _DEBUG
	{
		static int k;
		LONGLONG pos1, pos2;
		k++;
		IMediaSeeking_GetPositions (pSeek, &pos1, &pos2);
		Sys_WindowCaptionSet (   va("%i - CD track position: pos1 %i pos2 %i", k, (int)pos1, (int)pos2 ) );
	}


#pragma message ("Baker: In Engine X I altered this a bit")
#endif
    // Process all queued events
   while (SUCCEEDED (IMediaEventEx_GetEvent (pEvent, &evCode, &evParam1, &evParam2, 0)))
    {

        // Free memory associated with callback, since we're not using it
        hr = IMediaEventEx_FreeEventParams(pEvent, evCode, evParam1, evParam2);

        // If this is the end of the clip, reset to beginning
        if (evCode == EC_COMPLETE && looping)
        {
            LONGLONG pos = 0;

            // Reset to first frame of movie
			 hr = IMediaSeeking_SetPositions(pSeek, &pos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
			 Con_DPrintf ("MP3 Tracks: Looped\n");
        }
		else if (evCode == EC_COMPLETE)
		{
			// have to explicitly stop it when it completes otherwise the interfaces will remain open
			// when the next MP3 is played, and both will play simultaneously...!
			MediaPlayer_Shutdown ();
		}
    }
    return;
}

#endif
