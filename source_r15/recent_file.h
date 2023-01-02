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

#ifndef __RECENT_FILE_H
#define __RECENT_FILE_H


void Recent_File_Set_FullPath (const char* full_filename);

void Recent_File_Set_RelativePath (const char* relative_filename);
// Fills in the recent file with the gamedir path + filename.

const char* Recent_File_Get (void);

void Recent_File_NewGame (void); // Done on a gamedir change.

void Recent_File_Init (void);

#endif	/* __RECENT_FILE_H */