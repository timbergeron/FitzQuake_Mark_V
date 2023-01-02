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
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include "quakedef.h"
#ifdef WANTED_MP3_MUSIC // Baker change
#pragma message ("Note: MP3 Music disabled; I've never had any luck with DX8 and MinGW + CodeBlocks / GCC --- Baker :(")
#endif

#include "winquake.h"


extern	cvar_t	bgmvolume;

static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	initialized = false;
static qboolean	enabled = false;
static qboolean playLooping = false;
static float	cdvolume;
static byte 	remap[100];
static byte		cdrom;
static byte		playTrack;
static byte		maxTrack;
#ifdef SUPPORTS_MP3_MUSIC // Baker change
static qboolean		using_directshow = false;
#endif // Baker change +

UINT	wDeviceID;


static void CDAudio_Eject(void)
{
	DWORD	dwReturn;

    if (dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, (DWORD)NULL))
		Con_DPrintf("MCI_SET_DOOR_OPEN failed (%i)\n", dwReturn);
}


static void CDAudio_CloseDoor(void)
{
	DWORD	dwReturn;

    if (dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_DOOR_CLOSED, (DWORD)NULL))
		Con_DPrintf("MCI_SET_DOOR_CLOSED failed (%i)\n", dwReturn);
}


static int CDAudio_GetAudioDiskInfo(void)
{
	DWORD				dwReturn;
	MCI_STATUS_PARMS	mciStatusParms;

	cdValid = false;

	mciStatusParms.dwItem = MCI_STATUS_READY;
    dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD) (LPVOID) &mciStatusParms);

	if (dwReturn)
	{
		Con_DPrintf("CDAudio: drive ready test - get status failed\n");
		return -1;
	}

	if (!mciStatusParms.dwReturn)
	{
		Con_DPrintf("CDAudio: drive not ready\n");
		return -1;
	}

	mciStatusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
    dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD) (LPVOID) &mciStatusParms);

	if (dwReturn)
	{
		Con_DPrintf("CDAudio: get tracks - status failed\n");
		return -1;
	}

	if (mciStatusParms.dwReturn < 1)
	{
		Con_DPrintf("CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
	maxTrack = mciStatusParms.dwReturn;

	return 0;
}


void CDAudio_Play(byte track, qboolean looping)
{
	DWORD				dwReturn;
    MCI_PLAY_PARMS		mciPlayParms;
	MCI_STATUS_PARMS	mciStatusParms;


#ifdef SUPPORTS_MP3_MUSIC // Baker change
	using_directshow = false;
#endif // Baker change +

	if (!enabled)
	{
#ifdef SUPPORTS_MP3_MUSIC // Baker change
		// try directshow
		using_directshow = MediaPlayer_Play (track, looping);
		if (using_directshow)
#endif // Baker change +
		return;
	}

	if (!cdValid)
	{
		// try one more time
		CDAudio_GetAudioDiskInfo();

		// didn't work
		if (!cdValid)
		{
#ifdef SUPPORTS_MP3_MUSIC // Baker change
			// play with directshow instead
			using_directshow = MediaPlayer_Play (track, looping);
#endif // Baker change +
			return;
		}
	}

	track = remap[track];

	if (track < 1 || track > maxTrack)
	{
		Con_DPrintf("CDAudio: Bad track number %u.\n", track);
		return;
	}

	// don't try to play a non-audio track
	mciStatusParms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
	mciStatusParms.dwTrack = track;
    dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD) (LPVOID) &mciStatusParms);

	if (dwReturn)
	{
		Con_DPrintf("MCI_STATUS failed (%i)\n", dwReturn);
		return;
	}

	if (mciStatusParms.dwReturn != MCI_CDA_TRACK_AUDIO)
	{
		Con_Printf("CDAudio: track %i is not audio\n", track);
		return;
	}

	// get the length of the track to be played
	mciStatusParms.dwItem = MCI_STATUS_LENGTH;
	mciStatusParms.dwTrack = track;
    dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD) (LPVOID) &mciStatusParms);

	if (dwReturn)
	{
		Con_DPrintf("MCI_STATUS failed (%i)\n", dwReturn);
		return;
	}

	if (playing)
	{
		if (playTrack == track)
			return;

		CDAudio_Stop();
	}

    mciPlayParms.dwFrom = MCI_MAKE_TMSF(track, 0, 0, 0);
	mciPlayParms.dwTo = (mciStatusParms.dwReturn << 8) | track;
    mciPlayParms.dwCallback = (DWORD)wplat.mainwindow;
    dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_FROM | MCI_TO, (DWORD)(LPVOID) &mciPlayParms);

	if (dwReturn)
	{
		Con_DPrintf("CDAudio: MCI_PLAY failed (%i)\n", dwReturn);
		return;
	}

	playLooping = looping;
	playTrack = track;
	playing = true;

	if (cdvolume == 0.0)
		CDAudio_Pause ();
}


void CDAudio_Stop(void)
{
	DWORD	dwReturn;

#ifdef SUPPORTS_MP3_MUSIC // Baker change
	if (using_directshow)
	{
//		MediaPlayer_Stop ();
		MediaPlayer_Shutdown ();
		using_directshow = false;
		return;
	}
#endif // Baker change +

	if (!enabled)
		return;

	if (!playing)
		return;

    if (dwReturn = mciSendCommand(wDeviceID, MCI_STOP, 0, (DWORD)NULL))
		Con_DPrintf("MCI_STOP failed (%i)", dwReturn);

	wasPlaying = false;
	playing = false;
}


void CDAudio_Pause(void)
{
	DWORD				dwReturn;
	MCI_GENERIC_PARMS	mciGenericParms;

#ifdef SUPPORTS_MP3_MUSIC // Baker change
	if (using_directshow)
	{
		MediaPlayer_Pause ();
		return;
	}
#endif // Baker change +

	if (!enabled)
		return;

	if (!playing)
		return;

	mciGenericParms.dwCallback = (DWORD)wplat.mainwindow;
    if (dwReturn = mciSendCommand(wDeviceID, MCI_PAUSE, 0, (DWORD)(LPVOID) &mciGenericParms))
		Con_DPrintf("MCI_PAUSE failed (%i)", dwReturn);

	wasPlaying = playing;
	playing = false;
}


void CDAudio_Resume(void)
{
	DWORD			dwReturn;
    MCI_PLAY_PARMS	mciPlayParms;

#ifdef SUPPORTS_MP3_MUSIC // Baker change
	if (using_directshow)
	{
		MediaPlayer_Resume ();
		return;
	}
#endif // Baker change +

	if (!enabled)
		return;

	if (!cdValid)
		return;

	if (!wasPlaying)
		return;

    mciPlayParms.dwFrom = MCI_MAKE_TMSF(playTrack, 0, 0, 0);
    mciPlayParms.dwTo = MCI_MAKE_TMSF(playTrack + 1, 0, 0, 0);
    mciPlayParms.dwCallback = (DWORD)wplat.mainwindow;
    dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_TO | MCI_NOTIFY, (DWORD)(LPVOID) &mciPlayParms);

	if (dwReturn)
	{
		Con_DPrintf("CDAudio: MCI_PLAY failed (%i)\n", dwReturn);
		return;
	}

	playing = true;
}


static void CD_f (void)
{
	const char	*command;
	int		n;

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("Usage: %s {play|stop|on|off|info|pause|resume} [filename]\n", Cmd_Argv (0));
		Con_Printf ("  Note: music files should be in gamedir\\music\n");
		Con_Printf ("Example: quake\\id1\\track04.mp3 \n");

		return;
	}

	command = Cmd_Argv (1);

	if (Q_strcasecmp(command, "on") == 0)
	{
#ifdef SUPPORTS_MP3_MUSIC // Baker change
		// Baker: Due to code changes, I can't see this code even being reachable now if command line turned it off.
		if (MediaPlayer_Force_On () )
			Con_Printf ("Music track play will start on map load\n");
		else
			Con_Printf ("Disabled due to command line param -nosound or -nocdaudio or dedicated server\n");
#endif // Baker change +

		return;
	}

	if (Q_strcasecmp(command, "off") == 0)
	{
#ifdef SUPPORTS_MP3_MUSIC // Baker change
		MediaPlayer_Shutdown ();
		MediaPlayer_Force_Off ();
		using_directshow = false;
#endif // Baker change +

		if (playing)
			CDAudio_Stop();

		enabled = false;
		return;
	}

	if (Q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;

		if (playing)
			CDAudio_Stop();

		for (n = 0; n < 100; n++)
			remap[n] = n;

		CDAudio_GetAudioDiskInfo();
		return;
	}

	if (Q_strcasecmp(command, "remap") == 0)
	{
		int ret = Cmd_Argc() - 2;

		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf("  %u -> %u\n", n, remap[n]);

			return;
		}

		for (n = 1; n <= ret; n++)
			remap[n] = Q_atoi(Cmd_Argv (n+1));

		return;
	}

	if (Q_strcasecmp(command, "close") == 0)
	{
		CDAudio_CloseDoor();
		return;
	}

#if !defined(SUPPORTS_MP3_MUSIC) // Baker change
	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Con_Printf("No CD in player.\n");
			return;
		}
	}
#endif // Baker change -

	if (Q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), false);
		return;
	}

	if (Q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), true);
		return;
	}

	if (Q_strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

#if !defined(SUPPORTS_MP3_MUSIC) // Baker change
	if (Q_strcasecmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}
#endif // Baker change -

	if (Q_strcasecmp(command, "info") == 0)
	{
		Con_Printf("%u tracks\n", maxTrack);

		if (playing)
			Con_Printf("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Con_Printf("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);

		Con_Printf("Volume is %f\n", cdvolume);
		return;
	}

#ifdef SUPPORTS_MP3_MUSIC // Baker change
	// the intention is that the "cd" command works with MP3 tracks too, so here we only check if there is a CD in the drive
	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();

		if (!cdValid)
		{
			Con_Printf ("No CD in player.\n");
			return;
		}
	}

	// Placement here prevents eject if no CD in drive
	if (Q_strcasecmp (command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();

		CDAudio_Eject();
		cdValid = false;
		return;
	}



}


LONG WIN_MediaPlayer_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// pass to direct show stuff
	MediaPlayer_Message (/*(int)Looping*/ 1 );

	return 0;
#endif // Baker change +
}


LONG WIN_CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (lParam != (int)wDeviceID)
		return 1;

#ifdef SUPPORTS_MP3_MUSIC // Baker change
	// don't handle CD messages if using directshow
	if (using_directshow) return 0;
#endif // Baker change +

	switch (wParam)
	{
		case MCI_NOTIFY_SUCCESSFUL:

			if (playing)
			{
				playing = false;

				if (playLooping)
					CDAudio_Play(playTrack, true);
			}

			break;

		case MCI_NOTIFY_ABORTED:
		case MCI_NOTIFY_SUPERSEDED:
			break;

		case MCI_NOTIFY_FAILURE:
			Con_DPrintf("MCI_NOTIFY_FAILURE\n");
			CDAudio_Stop ();
			cdValid = false;
			break;

		default:
			Con_DPrintf("Unexpected MM_MCINOTIFY type (%i)\n", wParam);
			return 1;
	}

	return 0;
}

#ifdef SUPPORTS_MP3_MUSIC // Baker change
void MediaPlayer_ChangeVolume (float newvolume);
#endif // Baker change +

void CDAudio_Update(void)
{
#ifdef SUPPORTS_MP3_MUSIC // Baker change
	static float old_effective_volume = -1;
	float effective_volume = 9999;
#endif // Baker change +

	if (!enabled)
		return;

#ifdef SUPPORTS_MP3_MUSIC // Baker change
	if (using_directshow)
	{
		extern qboolean muted;
#ifdef _DEBUG
		MediaPlayer_Update ();
#endif
		// Baker: Effective volume with mp3 music is relative to sound volume
		// Sets volume to 0 when not active app or minimized

		if (vid.ActiveApp == false || vid.Minimized == true || muted == true)
			effective_volume = 0;
		else
			effective_volume = bgmvolume.value * sfxvolume.value;
	} else effective_volume = bgmvolume.value;


	if (using_directshow && effective_volume == old_effective_volume)
		return;
	else if (!using_directshow && cdvolume == bgmvolume.value)
		return;

	if (using_directshow) // A volume changed
	{
		if (effective_volume == 0)
			effective_volume = effective_volume;
		MediaPlayer_ChangeVolume (effective_volume);
	}
	else if (bgmvolume.value)
		CDAudio_Resume ();
	else CDAudio_Pause ();		// A zero value will pause

	cdvolume = bgmvolume.value;
	old_effective_volume = effective_volume;
#else // Baker change +
	if (bgmvolume.value != cdvolume)
	{
		if (cdvolume)
		{
			Cvar_SetValue ("bgmvolume", 0.0);
			cdvolume = bgmvolume.value;
			CDAudio_Pause ();
		}
		else
		{
			Cvar_SetValue ("bgmvolume", 1.0);
			cdvolume = bgmvolume.value;
			CDAudio_Resume ();
		}
	}
#endif // Baker change -
}

int CDAudio_Init(void)
{
	DWORD	dwReturn;
	MCI_OPEN_PARMS	mciOpenParms;
    MCI_SET_PARMS	mciSetParms;
	int				n;

	qboolean disabled_musics = false;

	if (cls.state == ca_dedicated)
		disabled_musics = true;

	if (COM_CheckParm("-nosound"))
		disabled_musics = true; // No sound --> no track music either

	if (COM_CheckParm("-nocdaudio"))
		disabled_musics = true;

	if (disabled_musics)
	{
#ifdef SUPPORTS_MP3_MUSIC
		MediaPlayer_Command_Line_Disabled ();
#endif // SUPPORTS_MP3_MUSIC
		return -1;
	}

	mciOpenParms.lpstrDeviceType = "cdaudio";

	if (dwReturn = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_SHAREABLE, (DWORD) (LPVOID) &mciOpenParms))
	{
		Con_SafePrintf ("CDAudio_Init: MCI_OPEN failed (%i)\n", dwReturn);
		return -1;
	}

	wDeviceID = mciOpenParms.wDeviceID;

    // Set the time format to track/minute/second/frame (TMSF).
    mciSetParms.dwTimeFormat = MCI_FORMAT_TMSF;

    if (dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD)(LPVOID) &mciSetParms))
    {
		Con_Printf("MCI_SET_TIME_FORMAT failed (%i)\n", dwReturn);
        mciSendCommand(wDeviceID, MCI_CLOSE, 0, (DWORD)NULL);
		return -1;
    }

	for (n = 0; n < 100; n++)
		remap[n] = n;

	initialized = true;
	enabled = true;

	if (CDAudio_GetAudioDiskInfo())
	{
		Con_SafePrintf("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	Cmd_AddCommand ("cd", CD_f);

	Con_SafePrintf("CD Audio Initialized\n");

	return 0;
}


void CDAudio_Shutdown(void)
{
	if (!initialized)
		return;

#ifdef SUPPORTS_MP3_MUSIC // Baker change
	MediaPlayer_Shutdown ();
#endif // Baker change +

	CDAudio_Stop();

	if (mciSendCommand(wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD)NULL))
		Con_DPrintf("CDAudio_Shutdown: MCI_CLOSE failed\n");
}

