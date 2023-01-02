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
// lists.c -- Lists (maps, demos, games, keys, saves)

#include "quakedef.h"
#include "q_dirent.h"

	
//	int	unused;	const char	*description, *extension, *associated_commands;
list_info_t list_info[] =
{
	{ list_type_none,		"none",		NULL,		NULL,					},
	{ list_type_config,		"config",	".cfg",		"exec"					},
	{ list_type_demo,		"demo",		".dem",		"playdemo,capturedemo"	},
	{ list_type_game,		"game",		NULL,		"game",					},
	{ list_type_key,		"key",		NULL,		"bind,unbind"			},
	{ list_type_map,		"map",		".bsp",		"map,changelevel"		},
	{ list_type_savegame,	"save game",".sav",		"save,load"				},
	{ list_type_sky,		"sky",		"rt.tga",	"sky"		},	
	{ list_type_sound,		"sound",	".wav",		"play,playvol"			},
	{ list_type_texmode,	"texmode",	NULL,		"gl_texturemode"		},
};


//
//Baker: List construction, a rework of the johnfitz code
//

void List_Add (listmedia_t** headnode, const char *name)
{
	listmedia_t	*listent,*cursor,*prev;

	// ignore duplicate
	for (listent = *headnode; listent; listent = listent->next)
	{
		if (!Q_strcmp (name, listent->name))
			return;
	}

	listent = (listmedia_t *)Z_Malloc(sizeof(listmedia_t));
	q_strlcpy (listent->name, name, sizeof(listent->name));
	String_Edit_To_Lower_Case (listent->name);

	//insert each entry in alphabetical order
    if (*headnode == NULL || 
	    Q_strcasecmp(listent->name, (*headnode)->name) < 0) //insert at front
	{
        listent->next = *headnode;
        *headnode = listent;
    }
    else //insert later
	{
        prev = *headnode;
        cursor = (*headnode)->next;
		while (cursor && (Q_strcasecmp(listent->name, cursor->name) > 0))
		{
            prev = cursor;
            cursor = cursor->next;
        }
        listent->next = prev->next;
        prev->next = listent;
    }
}


void List_Free (listmedia_t** headnode)
{
	listmedia_t *blah;

	while (*headnode)
	{
		blah = (*headnode)->next;
		Z_Free (*headnode);
		*headnode = blah;
	}
	*headnode = NULL;
}


static void List_Print (list_info_t* list_info_item)
{
	int count;
	listmedia_t	*cursor;

	for (cursor = list_info_item->plist, count = 0; cursor; cursor = cursor->next, count ++)
		Con_SafePrintf ("   %s\n", cursor->name);

	if (count)
		Con_SafePrintf ("%i %s(s)\n", count, list_info_item->description);
	else Con_SafePrintf ("no %ss found\n", list_info_item->description);
}



//==============================================================================
//johnfitz -- modlist management
//==============================================================================

void Modlist_Init (void)
{
	DIR		*dir_p, *mod_dir_p;
	struct dirent	*dir_t, *mod_dir_t;
	qboolean	progs_found, pak_found;
	char			dir_string[MAX_OSPATH], mod_dir_string[MAX_OSPATH];

	q_snprintf (dir_string, sizeof(dir_string), "%s/", com_basedir); // Fill in the com_basedir into dir_string

	dir_p = opendir(dir_string);
	if (dir_p == NULL)
		return;

	while ((dir_t = readdir(dir_p)) != NULL)
	{
		if ((strcmp(dir_t->d_name, ".") == 0) || (strcmp(dir_t->d_name, "..") == 0))
			continue;
		q_snprintf(mod_dir_string, sizeof(mod_dir_string), "%s%s/", dir_string, dir_t->d_name);
		mod_dir_p = opendir(mod_dir_string);
		if (mod_dir_p == NULL)
			continue;
		progs_found = false;
		pak_found = false;
		// find progs.dat and pak file(s)
		while ((mod_dir_t = readdir(mod_dir_p)) != NULL)
		{
			if ((strcmp(mod_dir_t->d_name, ".") == 0) || (strcmp(mod_dir_t->d_name, "..") == 0))
				continue;
			if (Q_strcasecmp(mod_dir_t->d_name, "progs.dat") == 0)
				progs_found = true;
			if (strstr(mod_dir_t->d_name, ".pak") || strstr(mod_dir_t->d_name, ".PAK"))
				pak_found = true;
			if (progs_found || pak_found)
				break;
		}
		closedir(mod_dir_p);
		if (!progs_found && !pak_found)
			continue;
		List_Add(&list_info[list_type_game].plist, dir_t->d_name);
	}

	closedir(dir_p);
}




typedef const char* (*constcharfunc_t) (void);
static void List_Func_Rebuild (listmedia_t** list, constcharfunc_t runFunction)
{	
	const char* namestring;
	if (*list) 
		List_Free (list);

	while ( (namestring = runFunction()    ) )
	{
		List_Add (list, namestring);
//		Con_Printf ("Adding %s\n", namestring);
	}

}

static void List_Configs_f (void)	{ List_Print (&list_info[list_type_config]);  }
static void List_Demos_f (void)		{ List_Print (&list_info[list_type_demo]); }
static void List_Games_f (void) 	{ List_Print (&list_info[list_type_game]); }
static void List_Keys_f (void)		{ List_Print (&list_info[list_type_key]); }
static void List_Maps_f (void)	 	{ List_Print (&list_info[list_type_map]); }
static void List_Savegames_f (void)	{ List_Print (&list_info[list_type_savegame]); }
static void List_Skys_f (void)		{ List_Print (&list_info[list_type_sky]); Con_Printf ("Located in gfx/env folder\n"); }
static void List_Sounds_f (void)	{ List_Print (&list_info[list_type_sound]); Con_Printf ("These are cached sounds only -- loaded in memory\n"); }




// These rely on the cache
void Lists_Refresh_NewMap (void)
{
	List_Func_Rebuild (&list_info[list_type_sound].plist, S_Sound_ListExport);
}

// For when demo record is finished
void Lists_Update_Demolist (void)
{
	List_Filelist_Rebuild (&list_info[list_type_demo].plist, "", list_info[list_type_demo].extension, 0, 0);
}

void Lists_Update_Savelist (void)
{
	List_Filelist_Rebuild (&list_info[list_type_savegame].plist, "", list_info[list_type_savegame].extension, 0, 0);
}


void Lists_NewGame (void)
{
	Lists_Refresh_NewMap ();
	Lists_Update_Demolist();
	Lists_Update_Savelist ();

	List_Filelist_Rebuild (&list_info[list_type_map].plist, "/maps", list_info[list_type_map].extension, 32*1024, 1);
	List_Filelist_Rebuild (&list_info[list_type_sky].plist, "/gfx/env", list_info[list_type_sky].extension, 0, 4);
	List_Filelist_Rebuild (&list_info[list_type_config].plist, "", list_info[list_type_config].extension, 0, 2);
}


void Lists_Init (void)
{
	Cmd_AddCommand ("mods", List_Games_f); //johnfitz

	Cmd_AddCommand ("configs", List_Configs_f); // Baker --- another way to get to this
	Cmd_AddCommand ("demos", List_Demos_f); 
	Cmd_AddCommand ("games", List_Games_f); // as an alias to "mods" -- S.A. / QuakeSpasm
	Cmd_AddCommand ("keys", List_Keys_f); 
	Cmd_AddCommand ("maps", List_Maps_f); //johnfitz
	Cmd_AddCommand ("saves", List_Savegames_f); 
	Cmd_AddCommand ("skys", List_Skys_f); //johnfitz
	Cmd_AddCommand ("sounds", List_Sounds_f); // Baker --- another way to get to this

	Lists_NewGame ();
	
	Modlist_Init ();
	List_Func_Rebuild (&list_info[list_type_key].plist, Key_ListExport);
	List_Func_Rebuild (&list_info[list_type_texmode].plist, TexMgr_TextureModes_ListExport);
	
}

