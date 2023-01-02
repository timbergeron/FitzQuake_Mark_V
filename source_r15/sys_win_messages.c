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
// sys_win_menu.c -- server code for moving users

#include "quakedef.h"
#include "winquake.h"




LRESULT CALLBACK WIN_MainWndProc (HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	int fActive, fMinimized;

	// check for input messages
	if (WIN_IN_ReadInputMessages (hWnd, Msg, wParam, lParam)) return 0;

    switch (Msg)
    {
	// events we want to discard
	case WM_CREATE:		return 0;
	case WM_ERASEBKGND: return 1; // MH: treachery!!! see your MSDN!
	case WM_SYSCHAR:	return 0;

	case WM_KILLFOCUS:
		// Baker: Plus this makes it survive a Windows firewall warning "better"
		if (vid.screen.type == MODE_FULLSCREEN)
			ShowWindow(wplat.mainwindow, SW_SHOWMINNOACTIVE);
		break;

	case WM_SYSCOMMAND:
		switch (wParam & ~0x0F)
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
			// prevent from happening
			return 0;
		}
		break;

   	case WM_CLOSE:
		//if (MessageBox (wplat.mainwindow, "Are you sure you want to quit?", "Confirm Exit", MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
		Sys_Quit ();

	    return 0;

	case WM_ACTIVATE:
		fActive = LOWORD(wParam);
		fMinimized = (BOOL) HIWORD(wParam);
		VID_AppActivate(!(fActive == WA_INACTIVE), fMinimized);

		return 0;

   	case WM_DESTROY:
        PostQuitMessage (0);
        return 0;

#ifdef SUPPORTS_MP3_MUSIC // Baker change
	case WM_GRAPHNOTIFY:
		return WIN_MediaPlayer_MessageHandler (hWnd, Msg, wParam, lParam);
#endif // Baker change +

	case MM_MCINOTIFY:
        return WIN_CDAudio_MessageHandler (hWnd, Msg, wParam, lParam);

    default:
		break;
    }

	// pass all unhandled messages to DefWindowProc
	return DefWindowProc (hWnd, Msg, wParam, lParam);
}
