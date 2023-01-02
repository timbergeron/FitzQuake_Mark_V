/*
Copyright (C) 1996-2001 Id Software, Inc.
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

#ifndef __SYS_WIN_MENU_H
#define __SYS_WIN_MENU_H

#ifdef SUPPORTS_WINDOWS_MENUS // Baker change 

void Extra_Menus_Show (void);
void Extra_Menus_Hide (void);
void Extra_Menus_Toggle (void);

void Extra_Menus_Rebuild (void);
void Extra_Menus_Make_Command_List_Free (void);

void Extra_Menus_Execute_Command (unsigned int);


#endif // Baker change +




#endif	/* __SYS_WIN_MENU_H */