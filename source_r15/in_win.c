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
// in_win.c -- windows 95 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.


#include "quakedef.h"
#include "winquake.h"


void IN_Local_Capture_Mouse (qboolean bDoCapture)
{
	static qboolean captured = false;

	if (bDoCapture && !captured)
	{
		ShowCursor (FALSE); // Hides mouse cursor
		SetCapture (wplat.mainwindow);	// Captures mouse events
		Con_DPrintf ("Mouse Captured\n");
		captured = true;
	}
	
	if (!bDoCapture && captured)
	{
		ShowCursor (TRUE); // Hides mouse cursor
		ReleaseCapture ();
		ClipCursor (NULL); // Can't hurt
		Con_DPrintf ("Mouse Released\n");
		captured = false;
	}

}


qboolean IN_Local_Update_Mouse_Clip_Region_Think (mrect_t* mouseregion)
{
	mrect_t oldregion = *mouseregion;
	WINDOWINFO windowinfo;
	windowinfo.cbSize = sizeof (WINDOWINFO);
	GetWindowInfo (wplat.mainwindow, &windowinfo);	// client_area screen coordinates

	// Fill in top left, bottom, right, center
	mouseregion->left = windowinfo.rcClient.left;
	mouseregion->right = windowinfo.rcClient.right;
	mouseregion->bottom = windowinfo.rcClient.bottom;
	mouseregion->top = windowinfo.rcClient.top;

	if (memcmp (mouseregion, &oldregion, sizeof(mrect_t) ) != 0)
	{  // Changed!	
		mouseregion->width = mouseregion->right - mouseregion->left;
		mouseregion->height = mouseregion->bottom - mouseregion->top;
		mouseregion->center_x = (mouseregion->left + mouseregion->right) / 2;
		mouseregion->center_y = (mouseregion->top + mouseregion->bottom) / 2;
		ClipCursor (&windowinfo.rcClient);
		return true;
	}
	return false;
}

void IN_Local_Mouse_Cursor_SetPos (int x, int y)
{
	SetCursorPos (x, y);
}

void IN_Local_Mouse_Cursor_GetPos (int *x, int *y)
{
	POINT current_pos;
	GetCursorPos (&current_pos);

	*x = current_pos.x;
	*y = current_pos.y;
}


STICKYKEYS StartupStickyKeys = {sizeof (STICKYKEYS), 0};
TOGGLEKEYS StartupToggleKeys = {sizeof (TOGGLEKEYS), 0};
FILTERKEYS StartupFilterKeys = {sizeof (FILTERKEYS), 0};


void AllowAccessibilityShortcutKeys (qboolean bAllowKeys)
{
	static qboolean initialized = false;

	if (!initialized)
	{	// Save the current sticky/toggle/filter key settings so they can be restored them later
		SystemParametersInfo (SPI_GETSTICKYKEYS, sizeof (STICKYKEYS), &StartupStickyKeys, 0);
		SystemParametersInfo (SPI_GETTOGGLEKEYS, sizeof (TOGGLEKEYS), &StartupToggleKeys, 0);
		SystemParametersInfo (SPI_GETFILTERKEYS, sizeof (FILTERKEYS), &StartupFilterKeys, 0);
		Con_DPrintf ("Accessibility key startup settings saved\n");
		initialized = true;
	}

	if (bAllowKeys)
	{
		// Restore StickyKeys/etc to original state
		// (note that this function is called "allow", not "enable"; if they were previously
		// disabled it will put them back that way too, it doesn't force them to be enabled.)
		SystemParametersInfo (SPI_SETSTICKYKEYS, sizeof (STICKYKEYS), &StartupStickyKeys, 0);
		SystemParametersInfo (SPI_SETTOGGLEKEYS, sizeof (TOGGLEKEYS), &StartupToggleKeys, 0);
		SystemParametersInfo (SPI_SETFILTERKEYS, sizeof (FILTERKEYS), &StartupFilterKeys, 0);

		Con_DPrintf ("Accessibility keys enabled\n");
	}
	else
	{
		// Disable StickyKeys/etc shortcuts but if the accessibility feature is on,
		// then leave the settings alone as its probably being usefully used
		STICKYKEYS skOff = StartupStickyKeys;
		TOGGLEKEYS tkOff = StartupToggleKeys;
		FILTERKEYS fkOff = StartupFilterKeys;

		if ((skOff.dwFlags & SKF_STICKYKEYSON) == 0)
		{
			// Disable the hotkey and the confirmation
			skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
			skOff.dwFlags &= ~SKF_CONFIRMHOTKEY;

			SystemParametersInfo (SPI_SETSTICKYKEYS, sizeof (STICKYKEYS), &skOff, 0);
		}

		if ((tkOff.dwFlags & TKF_TOGGLEKEYSON) == 0)
		{
			// Disable the hotkey and the confirmation
			tkOff.dwFlags &= ~TKF_HOTKEYACTIVE;
			tkOff.dwFlags &= ~TKF_CONFIRMHOTKEY;

			SystemParametersInfo (SPI_SETTOGGLEKEYS, sizeof (TOGGLEKEYS), &tkOff, 0);
		}

		if ((fkOff.dwFlags & FKF_FILTERKEYSON) == 0)
		{
			// Disable the hotkey and the confirmation
			fkOff.dwFlags &= ~FKF_HOTKEYACTIVE;
			fkOff.dwFlags &= ~FKF_CONFIRMHOTKEY;

			SystemParametersInfo (SPI_SETFILTERKEYS, sizeof (FILTERKEYS), &fkOff, 0);
		}

		Con_DPrintf ("Accessibility keys disabled\n");
	}
}



LRESULT CALLBACK LLWinKeyHook(int Code, WPARAM wParam, LPARAM lParam)
{
	PKBDLLHOOKSTRUCT p;
	p = (PKBDLLHOOKSTRUCT) lParam;

	if (vid.ActiveApp)
	{
		switch(p->vkCode)
		{
		case VK_LWIN:	// Left Windows Key
		case VK_RWIN:	// Right Windows key
		case VK_APPS: 	// Context Menu key

			return 1; // Ignore these keys
		}
	}

	return CallNextHookEx(NULL, Code, wParam, lParam);
}



void AllowWindowsShortcutKeys (qboolean bAllowKeys)
{
	static qboolean WinKeyHook_isActive = false;
	static HHOOK WinKeyHook;

	if (!bAllowKeys)
	{
		// Disable if not already disabled
		if (!WinKeyHook_isActive)
		{
			if (!(WinKeyHook = SetWindowsHookEx(13, LLWinKeyHook, wplat.hInstance, 0)))
			{
				Con_Printf("Failed to install winkey hook.\n");
				Con_Printf("Microsoft Windows NT 4.0, 2000 or XP is required.\n");
				return;
			}

			WinKeyHook_isActive = true;
			Con_DPrintf ("Windows and context menu key disabled\n");
		}
	}

	if (bAllowKeys)
	{	// Keys allowed .. stop hook
		if (WinKeyHook_isActive)
		{
			UnhookWindowsHookEx(WinKeyHook);
			WinKeyHook_isActive = false;
			Con_DPrintf ("Windows and context menu key enabled\n");
		}
	}
}

void IN_Local_Keyboard_Disable_Annoying_Keys (qboolean bDoDisable)
{
	if (bDoDisable)
	{
		AllowAccessibilityShortcutKeys (false);
		AllowWindowsShortcutKeys (false);
	}
	else
	{
		AllowAccessibilityShortcutKeys (true);
		AllowWindowsShortcutKeys (true);
	}
}

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int IN_Local_MapKey (int windowskey)
{
	static byte scantokey[128] =
	{
		0 ,	27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', K_BACKSPACE, 
		9, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 13 , 
		K_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 
		K_SHIFT,'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', K_SHIFT,'*',K_ALT,' ', 0 , 
		K_F1, K_F2, K_F3, K_F4, K_F5,K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, 0, 
		K_HOME, K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',K_END,
		K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0, 0, K_F11, K_F12,0 , 0 , 0 , 0 , 0 , 0 , 0,
	};
	int key = (windowskey >> 16) & 255;

	if (key > 127)
		return 0;

	key = scantokey[key];
/*
	switch (key) 
	{
		case K_KP_STAR:		return '*';
		case K_KP_MINUS:	return '-';
		case K_KP_5:		return '5';
		case K_KP_PLUS:		return '+';
	}
*/
	return key;
}

qboolean WIN_IN_ReadInputMessages (HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	int button_bits = 0;

	switch (Msg)
	{
	case WM_MOUSEWHEEL:
		if ((short) HIWORD(wParam) > 0) 
		{
			Key_Event(K_MWHEELUP, true);
			Key_Event(K_MWHEELUP, false);
		}
		else 
		{
			Key_Event(K_MWHEELDOWN, true);
			Key_Event(K_MWHEELDOWN, false);
		}
		return true;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
		if (wParam & MK_LBUTTON)	button_bits |= 1;
		if (wParam & MK_RBUTTON)	button_bits |= 2;
		if (wParam & MK_MBUTTON)	button_bits |= 4;
		if (wParam & MK_XBUTTON1)	button_bits |= 8;
		if (wParam & MK_XBUTTON2)	button_bits |= 16;
		Input_Mouse_Button_Event (button_bits);

		return true;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		Key_Event (IN_Local_MapKey (lParam), true);
		return true;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		Key_Event (IN_Local_MapKey(lParam), false);
		return true;

	default:
		return false;
	}
}

