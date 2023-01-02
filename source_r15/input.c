/*
Copyright (C) 2013 Baker

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
// input.c -- input

#include "quakedef.h"


typedef enum
{
	input_none,
	input_have_keyboard,
	input_have_mouse_keyboard,
} input_state_t;

typedef struct
{
	input_state_t	current_state;
	qboolean		initialized, have_mouse, have_keyboard;

// Internals
	mrect_t			mouse_clip_screen_rect;
	int				mouse_accum_x, mouse_accum_y;
	int				mouse_old_button_state;
} inp_info_t;


#define NUM_MOUSE_BUTTONS 5
#define MRECT_PRINT(_x) _x.left, _x.top, _x.right, _x.bottom, _x.center_x, _x.center_y
enum { GET_IT = 1, LOSE_IT = 2 };

cvar_t in_freelook = {"in_freelook", "1", CVAR_NONE};
cvar_t in_nomouse = {"nomouse", "0", CVAR_NONE};

keyvalue_t input_state_text [] = 
{ 
	KEYVALUE (input_none), 
	KEYVALUE (input_have_keyboard), 
	KEYVALUE (input_have_mouse_keyboard), 
NULL, 0 };  // NULL termination

static inp_info_t inps;


static void Force_CenterView_f (void) { cl.viewangles[PITCH] = 0; }

static void IN_Info_f (void)
{
	Con_Printf ("IN Info ...\n");
	Con_Printf ("current_state: %s\n", KeyValue_GetKey (input_state_text, inps.current_state) );
	Con_Printf ("initialized: %i\n", inps.initialized);
	Con_Printf ("have_mouse: %i\n", inps.have_mouse);
	Con_Printf ("have_keyboard: %i\n", inps.have_keyboard);
	Con_Printf ("mouse_clip_screen_rect: (%i, %i)-(%i, %i) center: %i, %i\n", MRECT_PRINT(inps.mouse_clip_screen_rect) );
	Con_Printf ("mouse_accum_x: %i\n", inps.mouse_accum_x);
	Con_Printf ("mouse_accum_y: %i\n", inps.mouse_accum_y);
	Con_Printf ("mouse_old_button_state: %i\n", inps.mouse_old_button_state);
}

void Input_Think (void)
{
	input_state_t	newstate = (inps.initialized && vid.ActiveApp && !vid.Minimized) ? input_have_keyboard : input_none;
	qboolean		windowed_mouse_grab = !cl.paused && ( key_dest == key_game  || key_dest == key_message || (key_dest == key_menu && m_keys_bind_grab));
	qboolean		mouse_grab = (vid.screen.type == MODE_FULLSCREEN || windowed_mouse_grab);

	// newstate upgrades from should have "keyboard" to should have "mouse"
	// If the key_dest is game or we are binding keys in the menu
	if (newstate == input_have_keyboard && mouse_grab && in_nomouse.value == 0)
		newstate = input_have_mouse_keyboard;

//	Con_Printf ("current_state: %s (init %i active %i mini %i)\n", Keypair_String (input_state_text, inps.current_state), 
//		inps.initialized, vid.ActiveApp, vid.Minimized);

	if (newstate != inps.current_state)
	{ // New state.
		char	mouse_action	= ( newstate == input_have_mouse_keyboard && inps.have_mouse == false) ? GET_IT :  (( newstate != input_have_mouse_keyboard && inps.have_mouse == true) ? LOSE_IT : 0);
		char	keyboard_action = ( newstate != input_none && inps.have_keyboard == false) ? GET_IT :  (( newstate == input_none && inps.have_keyboard == true) ? LOSE_IT : 0);

//		Con_Printf ("State change\n");
		switch (keyboard_action)
		{
		case GET_IT:
			
			inps.have_keyboard = true;
			break;

		case LOSE_IT:
			// Key ups
			Key_Release_Keys ();

			inps.have_keyboard = false;
			break;
		}

		switch (mouse_action)
		{
		case GET_IT:
			// Sticky keys, Window key disabled
			IN_Local_Keyboard_Disable_Annoying_Keys (true);
			
			// Load window screen coords to mouse_clip_screen_rect
			// And clip the mouse cursor to that area
			IN_Local_Update_Mouse_Clip_Region_Think (&inps.mouse_clip_screen_rect);

			// Hide the mouse cursor and attach it
			IN_Local_Capture_Mouse (true);

			// Center the mouse on-screen
			IN_Local_Mouse_Cursor_SetPos (inps.mouse_clip_screen_rect.center_x, inps.mouse_clip_screen_rect.center_y);
			
			// Clear movement accumulation
			inps.mouse_accum_x = inps.mouse_accum_y = 0;

			inps.have_mouse = true;
			break;

		case LOSE_IT:
			// Release mouse buttons so "-alias" get triggered.  Even the keyboard keys.
			// As if we lose the mouse, usually this is from entering the console.
			// The exceptions to this are weird and uninteresting (like changing a mouse cvar via a bind)
			Key_Release_Keys ();

			// Note we still need our key ups when entering the console
			// Sticky keys, Window key reenabled

			IN_Local_Keyboard_Disable_Annoying_Keys (false); 

			// Release it somewhere out of the way
			IN_Local_Mouse_Cursor_SetPos (inps.mouse_clip_screen_rect.right - 80, inps.mouse_clip_screen_rect.top + 80);

			// Release the mouse and show the cursor.  Also unclips mouse.
			IN_Local_Capture_Mouse (false);

			// Clear movement accumulation and buttons
			inps.mouse_accum_x = inps.mouse_accum_y = inps.mouse_old_button_state = 0;

			inps.have_mouse = false;
			break;
		}
		inps.current_state = newstate;
	}

	if (inps.have_mouse && IN_Local_Update_Mouse_Clip_Region_Think (&inps.mouse_clip_screen_rect) == true)
	{
		// Re-center the mouse cursor and clear mouse accumulation
		IN_Local_Mouse_Cursor_SetPos (inps.mouse_clip_screen_rect.center_x, inps.mouse_clip_screen_rect.center_y);
		inps.mouse_accum_x = inps.mouse_accum_y = 0;
	}

	// End of function
}


void Input_Mouse_Button_Event (int mstate)
{
	if (inps.have_mouse /*|| (key_dest == key_menu && m_keys_bind_grab)*/ )
	{  // perform button actions
		int i;
		for (i = 0 ; i < NUM_MOUSE_BUTTONS ; i ++)
		{
			int button_bit = (1 << i);
			qboolean button_pressed  =  (mstate & button_bit) && !(inps.mouse_old_button_state & button_bit);
			qboolean button_released = !(mstate & button_bit) &&  (inps.mouse_old_button_state & button_bit);

			if (button_pressed || button_released)
				Key_Event (K_MOUSE1 + i, button_pressed ? true : false);
		}
		inps.mouse_old_button_state = mstate;
	}
}

void Input_Mouse_Accumulate (void)
{
	static last_key_dest;
	

	int new_mouse_x, new_mouse_y;

	Input_Think ();

		
	if (inps.have_mouse)
	{
		qboolean nuke_mouse_accum = false;

		// Special cases: fullscreen doesn't release mouse so doesn't clear accum
		// when entering/exiting the console.  I consider those input artifacts.  Also
		// we simply don't want accum from fullscreen if not key_dest == key_game.
		if (vid.screen.type == MODE_FULLSCREEN)
		{
			if (cl.paused)
				nuke_mouse_accum = true;
			else
			{
				qboolean in_game_or_message = (key_dest == key_game || key_dest == key_message);
				qboolean was_in_game_or_message = (last_key_dest == key_game || last_key_dest == key_message);
				qboolean entered_game_or_message = in_game_or_message && !was_in_game_or_message;
				if (entered_game_or_message || !in_game_or_message)
					nuke_mouse_accum = true;
			}				
		}

		IN_Local_Mouse_Cursor_GetPos (&new_mouse_x, &new_mouse_y); // GetCursorPos (&current_pos);

		inps.mouse_accum_x += new_mouse_x - inps.mouse_clip_screen_rect.center_x;
		inps.mouse_accum_y += new_mouse_y - inps.mouse_clip_screen_rect.center_y;

		// Re-center the mouse cursor
		IN_Local_Mouse_Cursor_SetPos (inps.mouse_clip_screen_rect.center_x, inps.mouse_clip_screen_rect.center_y);

		if (nuke_mouse_accum)
			inps.mouse_accum_x = inps.mouse_accum_y = 0;
	}		
	last_key_dest = key_dest;
}

void Input_Mouse_Move (usercmd_t *cmd)
{
	Input_Mouse_Accumulate ();

	if (inps.mouse_accum_x || inps.mouse_accum_y)
	{
		int	mouse_x = inps.mouse_accum_x *= sensitivity.value;
		int mouse_y = inps.mouse_accum_y *= sensitivity.value;
	// add mouse X/Y movement to cmd
		if ( (in_strafe.state & 1) || (lookstrafe.value && MOUSELOOK_ACTIVE ))
			cmd->sidemove += m_side.value * mouse_x;
		else cl.viewangles[YAW] -= m_yaw.value * mouse_x;

		if (MOUSELOOK_ACTIVE)
			V_StopPitchDrift ();

		if ( MOUSELOOK_ACTIVE && !(in_strafe.state & 1))
		{
			cl.viewangles[PITCH] += m_pitch.value * mouse_y;
			cl.viewangles[PITCH] = CLAMP (cl_minpitch.value, cl.viewangles[PITCH], cl_maxpitch.value);
		}
		else
		{
			if ((in_strafe.state & 1) && cl.noclip_anglehack)
				cmd->upmove -= m_forward.value * mouse_y;
			else cmd->forwardmove -= m_forward.value * mouse_y;
		}
		inps.mouse_accum_x = inps.mouse_accum_y = 0;
	}
}

void Input_Init (void)
{
	Cmd_AddCommand ("in_info", IN_Info_f);
	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
	Cvar_RegisterVariable (&in_freelook);
	Cvar_RegisterVariable (&in_nomouse);
	if (COM_CheckParm ("-nomouse"))
		Cvar_SetValue (in_nomouse.name, 1);
	inps.initialized = true;
	Input_Think ();
	Con_Printf ("Input initialized\n");
}

void Input_Shutdown (void)
{
	inps.initialized = false;
	Input_Think (); // Will shut everything off
}
