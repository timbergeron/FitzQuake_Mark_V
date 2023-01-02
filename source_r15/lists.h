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
// lists.h -- Lists (maps, demos, games, keys, saves)

#ifndef __LISTS_H
#define __LISTS_H


typedef struct listmedia_s
{
	char			name[MAX_QPATH];
	struct listmedia_s	*next;
} listmedia_t;

typedef enum
{
	list_type_none= 0,
	list_type_config, 
	list_type_demo,
	list_type_game, 
	list_type_key,
	list_type_map,
	list_type_savegame,
	list_type_sky,
	list_type_sound,
	list_type_texmode,
	
	MAX_LIST_TYPES,
} list_type_t;

typedef struct
{
	int				unused;	
	const char		*description, *extension, *associated_commands;
	listmedia_t*	plist;
} list_info_t;

extern list_info_t list_info[];

void Lists_NewGame (void);
void Lists_Refresh_NewMap (void);
void Lists_Update_Savelist (void);
void Lists_Update_Demolist (void);
void Lists_Init (void);


void List_Free (listmedia_t** headnode); // Have to make it available externally :(
void List_Add (listmedia_t** headnode, const char *name); // ditto

void List_Filelist_Rebuild (listmedia_t** list, const char* slash_subfolder, const char* dot_extension, int pakminfilesize, int directives);
// Due to data types this is in common.c right now :(  I could redesign but not time well spent.



#endif	/* __LISTS_H */
