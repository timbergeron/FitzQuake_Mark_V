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

#ifndef _QUAKE_INPUT_H
#define _QUAKE_INPUT_H

// input.h -- external (non-keyboard) input devices

typedef struct mrect_s
{
	int				left, right, bottom, top;
	int				center_x, center_y;
	int				width, height;
} mrect_t;

void Input_Init (void);
void Input_Shutdown (void);
void Input_Think (void);
void Input_Mouse_Move (usercmd_t *cmd);
void Input_Mouse_Accumulate (void);
void Input_Mouse_Button_Event (int mstate);

// Platform
void IN_Local_Capture_Mouse (qboolean bDoCapture);
void IN_Local_Keyboard_Disable_Annoying_Keys (qboolean bDoDisable);
void IN_Local_Mouse_Cursor_SetPos (int x, int y);
void IN_Local_Mouse_Cursor_GetPos (int *x, int *y);
qboolean IN_Local_Update_Mouse_Clip_Region_Think (mrect_t* mouseregion);


extern cvar_t in_freelook;
#define MOUSELOOK_ACTIVE (in_freelook.value || (in_mlook.state & 1))

#endif	/* _QUAKE_INPUT_H */

