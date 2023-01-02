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
// recent_file.c -- Recent file.

#include "quakedef.h"



static char recent_file[MAX_OSPATH];


static void Recent_File_Update_Event (void)
{
	// Updated recent file
#ifdef _DEBUG
//	Con_Printf ("Recent file set: '%s'\n", recent_file);
#endif
}

void Recent_File_Set_FullPath (const char* full_filename)
{
	q_strlcpy (recent_file, full_filename, sizeof(recent_file) );
	Recent_File_Update_Event ();
}

void Recent_File_Set_RelativePath (const char* relative_filename)
{
	q_snprintf (recent_file, sizeof(recent_file), "%s/%s", com_gamedir, relative_filename);
	Recent_File_Update_Event ();
}



void Recent_File_NewGame (void)
{
	recent_file[0] = 0;
	Recent_File_Update_Event ();
}

const char* Recent_File_Get (void)
{
	return recent_file;
}


void Recent_File_Show_f (void)
{
	if (vid.screen.type != MODE_WINDOWED)
	{
		Con_Printf ("'showfile' command only works in windowed mode\n");
		Con_Printf ("alt-enter and try again?\n");
		return;
	}

	if (recent_file[0] && File_Exists(recent_file))
	{
		switch (File_Is_Folder(recent_file))
		{
		case true: // folder
			if (Sys_OpenFolder (recent_file))
				goto open_ok;
			break;

		case false: // file
			if (Sys_OpenFolder_HighlightFile (recent_file))
				goto open_ok;
			break;
		}
	}

	// Couldn't do the above, try exploring to the gamedir
	if (!Sys_OpenFolder (com_gamedir))
	{
		Con_Printf ("Opening folder failed\n");
		return;
	}

open_ok:
	Con_Printf ("Explorer opening folder ...\n");
	
}

void Recent_File_Init (void)
{
	Cmd_AddCommand ("folder", Recent_File_Show_f);
}