/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2012 John Fitzgibbons and others

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
// winquake.h: Win32-specific Quake header file

//#pragma warning( disable : 4229 )  // mgraph gets this

#ifndef __WINQUAKE_H_
#define __WINQUAKE_H_

#include <windows.h>

#if !defined(__WINDOWS_VC6_BANDAGES__) && defined(_MSC_VER) && _MSC_VER < 1400
    #define __WINDOWS_VC6_BANDAGES__
    // Bandages to cover things that must require a service pack for Visual Studio 6 ..
    // Except Microsoft doesn't make the service packs available any more so we work around

    // GetFileAttributes ...
    #define INVALID_FILE_ATTRIBUTES ((DWORD)-1)

    // These are in <winuser.h> except I can't get them to work in MSVC6
    typedef struct tagWINDOWINFO
    {
      DWORD cbSize;
      RECT  rcWindow;
      RECT  rcClient;
      DWORD dwStyle;
      DWORD dwExStyle;
      DWORD dwWindowStatus;
      UINT  cxWindowBorders;
      UINT  cyWindowBorders;
      ATOM  atomWindowType;
      WORD  wCreatorVersion;
    } WINDOWINFO, *PWINDOWINFO, *LPWINDOWINFO;

    BOOL WINAPI GetWindowInfo(HWND hwnd, PWINDOWINFO pwi);

    //
    // Input ...
    //

    typedef ULONG ULONG_PTR;

    // MSVC6 ONLY -- Do not do for CodeBlocks/MinGW/GCC
    typedef struct
    {
        DWORD		vkCode;
        DWORD		scanCode;
        DWORD		flags;
        DWORD		time;
        ULONG_PTR dwExtraInfo;
    } *PKBDLLHOOKSTRUCT;

    #define WH_KEYBOARD_LL 13
    #define LLKHF_UP			(KF_UP >> 8)

    #define MAPVK_VK_TO_VSC 0
    #define MAPVK_VSC_TO_VK 1
    #define MAPVK_VK_TO_CHAR 2
    #define MAPVK_VSC_TO_VK_EX 3

    #define WM_INITMENUPOPUP                0x0117

    #define WM_MOUSEWHEEL                   0x020A
    #define MK_XBUTTON1 					0x0020
    #define MK_XBUTTON2 					0x0040
    #define WM_GRAPHNOTIFY  				WM_USER + 13

    // Vsync
    typedef BOOL (APIENTRY * SETSWAPFUNC) (int);
    typedef int (APIENTRY * GETSWAPFUNC) (void);

#endif // ! __WINDOWS_VC6_BANDAGES__

#if !defined(__GCC_VC6_BANDAGES__) && defined(__GNUC__)
    #define __GCC_BANDAGES__
    #define MK_XBUTTON1 					0x0020
    #define MK_XBUTTON2 					0x0040

       // Vsync
    typedef BOOL (APIENTRY * SETSWAPFUNC) (int);
    typedef int (APIENTRY * GETSWAPFUNC) (void);
#endif // ! __GCC_VC6_BANDAGES__


#if !defined(__WINDOWS_VC2008_BANDAGES__) && _MSC_VER && _MSC_VER == 1500 // Visual C++ 2008
	#define __WINDOWS_VC2008_BANDAGES__
       // Vsync
    typedef BOOL (APIENTRY * SETSWAPFUNC) (int);
    typedef int (APIENTRY * GETSWAPFUNC) (void);
#endif // ! __WINDOWS_VC2008_BANDAGES__

////////////////////////////////////////////////////////////////////
// Actual shared
////////////////////////////////////////////////////////////////////

// General ...
typedef struct
{
	HINSTANCE	hInstance;
	HICON		hIcon;
	HWND		mainwindow;
	HDC			draw_context;

	DEVMODE		gdevmode;

	SETSWAPFUNC wglSwapIntervalEXT;
	GETSWAPFUNC wglGetSwapIntervalEXT;

#ifdef SUPPORTS_MULTISAMPLE // Baker change
	HWND		hwnd_dialog;

	int			multisamples;
	int			forcePixelFormat;

	PIXELFORMATDESCRIPTOR pfd;
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change
} wplat_t;

extern wplat_t wplat;

// Sound ...

#include <dsound.h>
extern LPDIRECTSOUND pDS;
extern LPDIRECTSOUNDBUFFER pDSBuf;
extern DWORD gSndBufSize;

// Window setup/video

#define DW_BORDERLESS	(WS_POPUP) // Window covers entire screen; no caption, borders etc
#define DW_BORDERED		(WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)

#define RECT_WIDTH(_rect)	(_rect.right - _rect.left)
#define RECT_HEIGHT(_rect)  (_rect.bottom - _rect.top)

// Window / vid

void WIN_AdjustRectToCenterScreen (RECT *in_windowrect);
void WIN_AdjustRectToCenterUsable (RECT *in_windowrect);
void WIN_Change_DisplaySettings (int modenum);
void WIN_Construct_Or_Resize_Window (DWORD style, DWORD exstyle, RECT window_rect);
// Various functions passed around.

LRESULT CALLBACK WIN_MainWndProc (HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

BOOL WIN_SetupPixelFormat(HDC hDC);
LONG WIN_CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam); // WinQuake
LONG WIN_MediaPlayer_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
qboolean WIN_IN_ReadInputMessages (HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

#ifdef SUPPORTS_MULTISAMPLE // Baker change
int WIN_InitMultisample (HINSTANCE hInstance,HWND hWnd,PIXELFORMATDESCRIPTOR pfd, int ask_samples, int* pixelForceFormat);
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change

#endif // __WINQUAKE_H_
