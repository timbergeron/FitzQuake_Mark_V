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
#include "winquake.h" // Yuck



#ifdef SUPPORTS_WINDOWS_MENUS // Baker change
HMENU mainmenu;


#define AppendMenu_SubMenu(_menu, _text, _sub) AppendMenu((_menu), MF_POPUP,  (UINT)( (_sub) = CreateMenu() ), _text );

HMENU menu_newgame, menu_keys, menu_cheats, menu_media;
// Makes the menu; doesn't show it.
HMENU Extra_Menus_Build_Top (void)
{
	HMENU new_menu = CreateMenu();
	AppendMenu_SubMenu (new_menu, "&New Game", menu_newgame);
	AppendMenu_SubMenu (new_menu, "&Keys", menu_keys);
	AppendMenu_SubMenu (new_menu, "&Cheats", menu_cheats);
	AppendMenu_SubMenu (new_menu, "&Media", menu_media);
	return new_menu;
}

void Extra_Menus_Changed (void)
{
	// Now what?


}


int menu_shown = false;
void Extra_Menus_Show (void)
{
	if (!mainwindow) // No window so get out
		return;

//	if (mainmenu)
//		return;  // We already have something in place.  Strange!

	// We need to construct a stub.
	if (!mainmenu)
	{
		mainmenu = Extra_Menus_Build_Top ();

	}
	// That should show it.
	SetMenu (mainwindow, mainmenu);

	menu_shown = true;
//	Con_Printf("Menu Show\n");
}

void Extra_Menus_Hide (void)
{
	if (!mainwindow) // No window so get out
		return;

	if (!mainmenu)
		return; // That would be strange.

	SetMenu (mainwindow, NULL);
//	DestroyMenu (mainmenu);
//	mainmenu = NULL;
	menu_shown = false;
//	Con_Printf("Menu Hide\n");
}

int tool_menu_availability;
void Tool_Menu_Enabled_f (void)
{
	//extern int menu_shown;
	if (tool_menu_availability)
	{
		Con_Printf ("Tool menu already available.  Press Windows logo key to toggle it.");
		return;
	}

	tool_menu_availability = 1;

	Con_Printf ("tool_menu activated.  Press the Windows logo key to toggle menu.\n");
	if (menu_shown == false) // It can't possibly be shown.  But let's be safe.
		Extra_Menus_Toggle ();
}


void Extra_Menus_Toggle (void)
{
	if (menu_shown)
	{
//		VID_UpdateWindowStatus ();
		Extra_Menus_Hide();
	}
	else
	{
//		VID_UpdateWindowStatus ();
		Extra_Menus_Show();
	}

}


#define MAX_WM_CMDS_16384 16384
char* cmd_stringa[MAX_WM_CMDS_16384];
unsigned int wm_cmd_current = 0;


void Extra_Menus_Make_Command_List_Free (void)
{
	ezString** cursor;

	// Free everything
	for (cursor = &cmd_stringa[0]; *cursor; cursor++, wm_cmd_current--)
		ezString_Destroy (cursor);

	if (wm_cmd_current != 0)
		Sys_Error ("Extra_Menus_Make_Command_List_Free: Command reset !=0, is %u", wm_cmd_current);
}

unsigned int Extra_Menus_Command_Register (const char* command_string)
{
	if (wm_cmd_current == MAX_WM_CMDS_16384)
		Sys_Error ("Extra_Menus_Command_Register: Commands exceeded max %u", MAX_WM_CMDS_16384);

	ezString_Printf (&cmd_stringa[wm_cmd_current], "%s\n", command_string);

//	Con_Printf("Added Command %u: %-20.20s\n", wm_cmd_current, cmd_stringa[wm_cmd_current]);

	return ++wm_cmd_current; // We add +1.  We will subtract 1 later.  This is to avoid issuing a zero.
}

void Extra_Menus_Execute (unsigned int wParam)
{
	if (wParam == 0)
		return; // This is for menu item with no function

	// We need to remove 1 (we added one on assignment)
	wParam --; // Because we do not start with 0.

//	Con_Printf ("Menu execute %u\n", wParam);

	// Sanity check; keep from poking around outside of memory bounds
	if (wParam >= wm_cmd_current)
		Con_Printf ("wParam %u >= wm_cmd_current %u", wParam, wm_cmd_current);

	// Run the command
	if (cmd_stringa[wParam])
		Cbuf_AddText (cmd_stringa[wParam]);
	else Con_Printf ("Tried to run invalid command???? %u", wParam);
}





ezString* Game_Mode_StringA (void)
{
	ezString* describe = NULL;
	if (coop.value == 0 && deathmatch.value == 0 && teamplay.value == 0 && svs.maxclients == 1)
		ezString_Set (&describe, "Single Player");
	else if (coop.value && deathmatch.value == 0 && teamplay.value == 0 && svs.maxclients == 4)
		ezString_Set (&describe, "Cooperative (up to 4 players)");
	else if (coop.value == 0 && deathmatch.value && teamplay.value == 0 && svs.maxclients == 4)
		ezString_Set (&describe, "Deathmatch (up to 4 players)");
	else if (coop.value)
		ezString_Printf (&describe, "Cooperative (up to %u player)", svs.maxclients);
	else if (deathmatch.value)
		ezString_Printf (&describe, "Deathmatch (up to %u player)", svs.maxclients);
	else if (svs.maxclients == 1)
		ezString_Set (&describe, "Single Player (?)");
	else ezString_Set (&describe, "Mixed");

	return describe;
}

/*
ezSortList* ezSortList_Jump_X(ezSortList* start, unsigned int jumpcount)
{
	ezSortList* out = NULL;
	return out;
}
*/

#define AppendMenu_Separator(_menu) AppendMenu((_menu), MF_SEPARATOR | MF_GRAYED, 0, "-")
#define AppendMenu_Inert(_menu, _text) AppendMenu((_menu), MF_DISABLED, 0, (_text) )
#define AppendMenu_Grayed(_menu, _text) AppendMenu((_menu), MF_GRAYED, 0, (_text) )
#define AppendMenu_LiveCmd(_menu, _text, _cmd) AppendMenu((_menu), 0, Extra_Menus_Command_Register(_cmd), (_text) )
#define AppendMenu_SubMenu(_menu, _text, _sub) AppendMenu((_menu), MF_POPUP,  (UINT)( (_sub) = CreateMenu() ), _text );




#define PREPARE_LIST(_head, _func, _i, _list, _count, _item) ezSortList* ## _func ## (unsigned int*out_count); HMENU daddy = (_head); \
	unsigned int _i, _count; \
	ezSortList* _list = ## _func ## (&_count), * ## _item;

#define CLEANUP_LIST(_list) ezSortList_Destroy(&_list);

//#define IF_NEW_PAGE_MAKE_CAPTION(PAGE_SIZE_25, caption) if(1)
#define IF_NEW_PAGE_MAKE_CAPTION(_i, _pagesize, _item, _caption) const char* _caption; ezSortList* plus24; \
			if ((_i % _pagesize) ==0 && \
			    ((int)(plus24 = ezSortList_Jump_X(_item, _pagesize - 1 )) || 1)  && \
				((int)(_caption = ((plus24 == _item) ? _item->string : va("%s\t%s", _item->string, plus24->string))) || 1) )


#define IF_MORE_MAKE_HIT(_i, _limit, _count, _caption) const char* _caption; if(_i == _limit && \
				( (int)( _caption = va("... %u more",  (_count) - (_limit) ) ) || 1) )

#define PAGE_SIZE_25 25

// Rebuilds the menus
// There should be a mainmenu which is a stub or full skeleton.
void Extra_Menus_Rebuild (void)
{
	HMENU oldmenu = mainmenu;

	if (!mainwindow || !mainmenu) // Cannot rebuild if no window or the menu doesn't exist.
		return;

//	Con_Printf ("Menu Rebuild");

	// We should already have a stub.  But we are always destroying as to not risk things.
	if (wm_cmd_current > 0)
	{
		Extra_Menus_Make_Command_List_Free ();
	}

	mainmenu = Extra_Menus_Build_Top();

	//****************************
	// New Game Menu
	//****************************
	{
		HMENU head = menu_newgame, menu_gamemode; // The new game menu

		AppendMenu_SubMenu		(head, veg(Game_Mode_StringA() ), menu_gamemode );
		{
			HMENU daddy = menu_gamemode;
			AppendMenu_LiveCmd (daddy, "Single Player", 				"disconnect; wait; maxplayers 1; wait; listen 0; wait; coop 0; deathmatch 0; teamplay 0");
			AppendMenu_LiveCmd (daddy, "Cooperative\t(4 player slots)", "disconnect; wait; maxplayers 4; wait; listen 1; wait; coop 1; deathmatch 0; teamplay 0");
			AppendMenu_LiveCmd (daddy, "Deathmatch\t(4 player slots)", 	"disconnect; wait; maxplayers 4; wait; listen 1; wait; deathmatch 1; teamplay 0");
		}

		AppendMenu_Separator	(head);
		AppendMenu_LiveCmd		(head, "Standard Quake", "_showloading; skill 1; game id1; map start");
		AppendMenu_Separator	(head);

		//****************************
		// Mods - Grayed if unavailable

		{
			PREPARE_LIST(head, Mods_List_Alloc, i, list, count, item);

			ForEachSortList (i, item, list)
				if (item->itemdata) // String is empty if spacer
					AppendMenu_LiveCmd		(head, item->string, item->itemdata);
				else AppendMenu_Grayed		(head, item->string);

			CLEANUP_LIST(list);
		}

		AppendMenu_Separator	(head);
		AppendMenu_Inert		(head, "Load custom map:" );

		//****************************
		// Maps

		{
			PREPARE_LIST(head, Maps_List_Alloc, i, list, count, item)
			HMENU menu_for_children = daddy;

			ForEachSortList (i, item, list)
			{
				IF_NEW_PAGE_MAKE_CAPTION(i, PAGE_SIZE_25, item, caption)
					AppendMenu_SubMenu(daddy, caption, menu_for_children);
				AppendMenu_LiveCmd(menu_for_children, item->string, va("_showloading; map %s", item->string) );
			}
			CLEANUP_LIST(list)
		}

	}
	// End New Game Menu
	//****************************

	//****************************
	// Keys Menu
	//****************************

	{
		HMENU head = menu_keys, menu_set_mouse, menu_morekeys;

		AppendMenu_LiveCmd	(head, "Goto Options Menu", "menu_options");
		AppendMenu_Separator(head);
		AppendMenu			(head, in_mlook.state & 1 ? MF_CHECKED : MF_UNCHECKED, in_mlook.state & 1 ? Extra_Menus_Command_Register("-mlook") : Extra_Menus_Command_Register("+mlook"), va("Mouse look\t%s", (in_mlook.state & 1) ? "ON" : "OFF") );
		AppendMenu_SubMenu	(head, va("Mouse speed\t%1.3g", sensitivity.value), menu_set_mouse );
		{
			AppendMenu_LiveCmd	(menu_set_mouse, "Set to 3",  "sensitivity 3");
			AppendMenu_LiveCmd	(menu_set_mouse, "Set to 6",  "sensitivity 6");
			AppendMenu_LiveCmd	(menu_set_mouse, "Set to 9",  "sensitivity 9");
			AppendMenu_LiveCmd	(menu_set_mouse, "Set to 12", "sensitivity 12");
			AppendMenu_LiveCmd	(menu_set_mouse, "Set to 15", "sensitivity 15");
			AppendMenu_LiveCmd	(menu_set_mouse, "Set to 20", "sensitivity 20");
			AppendMenu_Separator(menu_set_mouse);
			AppendMenu 			(menu_set_mouse, m_pitch.value < 0 ? MF_CHECKED : MF_UNCHECKED,	 Extra_Menus_Command_Register(va("m_pitch %1.3g", -m_pitch.value) ), m_pitch.value < 0 ? "Inverted Mouse ON" : "Inverted Mouse OFF" );
		}
		AppendMenu(head, cl_forwardspeed.value > 200 ? MF_CHECKED : MF_UNCHECKED, cl_forwardspeed.value > 200 ? Extra_Menus_Command_Register ("cl_forwardspeed 200; cl_backspeed 200") : Extra_Menus_Command_Register ("cl_forwardspeed 400; cl_backspeed 400"), va("Always run\t%s", cl_forwardspeed.value > 200 ? "ON" : "OFF") );

		AppendMenu_Separator(head);

		// Customized Controls Keys (The binds in the menu)
		{
			PREPARE_LIST(head, Keybind_Menu_List_Alloc, i, list, count, item)
			unsigned int j = Extra_Menus_Command_Register("menu_keys");
			ForEachSortList (i, item, list)
				if (item->string[0])
					AppendMenu (daddy, 0, j,	va("%s\t%s", item->string, item->itemdata)); // EXCEPTION TO RULE
				else AppendMenu_Inert (daddy, item->string);

			CLEANUP_LIST(list);
		}
		AppendMenu_Separator(head);
		AppendMenu_SubMenu(head, "More keys ...", menu_morekeys);

		// Function Keys
		{
			HMENU menu_func_keys;
			AppendMenu_SubMenu (menu_morekeys, "Functions", menu_func_keys );
			{
				PREPARE_LIST(menu_func_keys, Keybind_Function_List_Alloc, i, list, count, item)
				HMENU menu_for_children = daddy;

				ForEachSortList (i, item, list)
				{
					IF_MORE_MAKE_HIT(i, PAGE_SIZE_25, count, caption)
						AppendMenu_SubMenu(daddy, caption, menu_for_children);
					AppendMenu_Inert(menu_for_children, va("%s\t%s", item->string, item->itemdata) );
				}
				CLEANUP_LIST(list);
			}
		}

		// Normal Keys
		{
			HMENU menu_normal_keys;
			AppendMenu_SubMenu (menu_morekeys, "Menu keys", menu_normal_keys );
			{
				PREPARE_LIST(menu_normal_keys, Keybind_MenuAll_List_Alloc, i, list, count, item)
				HMENU menu_for_children = daddy;

				ForEachSortList (i, item, list)
				{
					IF_MORE_MAKE_HIT(i, PAGE_SIZE_25, count, caption)
						AppendMenu_SubMenu(daddy, caption, menu_for_children);
					AppendMenu_Inert(menu_for_children, va("%s\t%s", item->string, item->itemdata) );
				}
				CLEANUP_LIST(list);
			}
		}

		// Interesting keys: Off head
		{
			PREPARE_LIST(menu_morekeys, Keybind_Other_List_Alloc, i, list, count, item)
			HMENU menu_for_children = daddy;

			ForEachSortList (i, item, list)
			{
				IF_MORE_MAKE_HIT(i, PAGE_SIZE_25, count, caption)
					AppendMenu_SubMenu(daddy, caption, menu_for_children);
				AppendMenu_Inert(menu_for_children, va("%s\t%s", item->string, item->itemdata) );
			}
			CLEANUP_LIST(list);
		}

	}
	// End Keys Menu
	//****************************


	//****************************
	// Cheats Menu
	//****************************

	{
		HMENU head = menu_cheats;
		{ // Cheats
			typedef struct { const char *string, *cmd; int checked, grayed; } menu_feed_t;
			HMENU daddy = head;
			menu_feed_t* cur; //int i;

			int notcl = (cls.signon != SIGNONS || cls.demofile !=0);
			int svok = sv.active !=0;
			int check_sv_cheats = !!(svok && sv.disallow_cheats);
			int check_god = !!(svok && ( ((int)svs.clients->edict->v.flags & FL_GODMODE)));
			int check_noclip = !!(svok && svs.clients->edict->v.movetype == MOVETYPE_NOCLIP);
			int check_notarget = !!(svok && ( ((int)svs.clients->edict->v.flags & FL_NOTARGET)));
			int check_freezeall = !!(svok && sv.frozen);

			menu_feed_t cheats[] =
			{ // Name		Command		Checked IF		Grayed When
				{ "Disable ability to cheat\tsv_cheats 0",	"toggle sv_cheats" , check_sv_cheats, svok == 0 },
				{ "Never get hurt\tgod",					"god",			check_god,			notcl },
				{ "Walk thru walls and fly\tnoclip",		"noclip",		check_noclip,		notcl },
				{ "Monsters ignore you\tnotarget",			"notarget",		check_notarget,		notcl },
				{ "Only you can move\tfreezeall",			"freezeall",	check_freezeall,	notcl },
				{ "" },
				{ "Guns and keys\timpulse 9",		   		"impulse 9",	0, 					notcl },
				{ "Quad damage\timpulse 255",			   	"impulse 255",	0, 					notcl },
				{ "High Health\tgive h 999",		   		"give h 999",	0, 					notcl },
				{ "Red Armor\tgive a 200" ,			   		"give a 200",	0, 					notcl },
				{ "Shotgun shells\tgive s 100" ,		   	"give s 100",	0, 					notcl },
				{ "Nails\tgive n 100" ,				   		"give n 100",	0, 					notcl },
				{ "Rockets\tgive r 100" ,	   				"give r 100",	0, 					notcl },
				{ "Cells\tgive c 100" ,			  			"give c 100",	0, 					notcl },
				{ "Rocket Launcher\tgive 7",			   	"give 7",		0, 					notcl },
				{ "Lightning Gun\tgive 8" ,				   	"give 8",		0, 					notcl },
			{ NULL} };

			if (notcl)
			{
				AppendMenu_Inert		(daddy, "Can't cheat - No map running" );
				AppendMenu_Separator	(daddy);
			}

			for (cur = &cheats[0]; cur->string; cur++)
				AppendMenu (daddy, cur->checked * MF_CHECKED + cur->grayed * MF_GRAYED, Extra_Menus_Command_Register(cur->cmd), cur->string);

		}


	}
	// End Cheats Menu
	//****************************

	//****************************
	// Media Menu
	//****************************

	{
		HMENU head = menu_media, menu_screenshot, menu_playdemo, menu_ghostdemo, menu_avidemo;

		AppendMenu_LiveCmd		(head, "Toggle Fullscreen/Windowed", "_vid_toggle" );
		AppendMenu_Separator	(head);
		AppendMenu_SubMenu 		(head, "Save screenshot", menu_screenshot);
		{
			// Postpone 2 frames because seems like OpenGL gets interrupted by the menu
			HMENU daddy = menu_screenshot;
			AppendMenu_LiveCmd(daddy, "To file", "wait; wait; screenshot");
			AppendMenu_LiveCmd(daddy, "To clipboard", "wait; wait; screenshot copy");
		}

		AppendMenu_LiveCmd(head, "Copy console to clipboard", "copy");
		AppendMenu_Separator(head);
		AppendMenu_SubMenu(head, "Play demo", menu_playdemo);
		{
			PREPARE_LIST(menu_playdemo, Demo_List_Alloc, i, list, count, item)
			HMENU menu_for_children = daddy;

			ForEachSortList (i, item, list)
			{
				IF_MORE_MAKE_HIT(i, PAGE_SIZE_25, count, caption)
					AppendMenu_SubMenu(daddy, caption, menu_for_children);
				AppendMenu_LiveCmd(menu_for_children, item->string, va("_showloading; playdemo \"%s\" ", item->string) );
			}

			CLEANUP_LIST(list);
		}

		AppendMenu_SubMenu(head, "Race against ghost", menu_ghostdemo);
		{
			PREPARE_LIST(menu_ghostdemo, Demo_List_Alloc, i, list, count, item)
			HMENU menu_for_children = daddy;

			ForEachSortList (i, item, list)
			{
				IF_MORE_MAKE_HIT(i, PAGE_SIZE_25, count, caption)
					AppendMenu_SubMenu(daddy, caption, menu_for_children);
				AppendMenu_LiveCmd(menu_for_children, item->string, va("ghostdemo \"%s\" ", item->string) );
			}
			CLEANUP_LIST(list);
		}

		AppendMenu_SubMenu(head, "Make AVI movie from demo", menu_avidemo);
		{
			PREPARE_LIST(menu_avidemo, Demo_List_Alloc, i, list, count, item)
			HMENU menu_for_children = daddy;

			AppendMenu_Inert	(daddy, "Note: Please have codec installed ..." );
			AppendMenu_Inert	(daddy, "divx, xvid or webm (vp80)" );
			AppendMenu_Inert	(daddy, "");
			AppendMenu_Inert	(daddy, "Otherwise this is unbearably slow");
			AppendMenu_Inert	(daddy, "and creates gigantic files (GBs per second).");
			AppendMenu_Separator(daddy);
			AppendMenu_LiveCmd	(daddy, "Copy Google VP80 download URL to clipboard", "copy vp80" );
			AppendMenu_Separator(daddy);

			ForEachSortList (i, item, list)
			{
				IF_MORE_MAKE_HIT(i, PAGE_SIZE_25, count, caption)
					AppendMenu_SubMenu(daddy, caption, menu_for_children);
				AppendMenu_LiveCmd(menu_for_children, item->string, va("capturedemo \"%s\" ", item->string) );
			}
			CLEANUP_LIST(list);
		}

		AppendMenu_Separator(head);
		if (modestate == MS_WINDOWED)
		{
			AppendMenu_LiveCmd	(head, String_Edit_SlashesBack_Like_Windows(va("Quake Folder: %s", host_parms->basedir)), "folder");
			AppendMenu_LiveCmd (head, "Open Folder In Explorer", "folder");
			if (recent_file[0])
				AppendMenu_LiveCmd (head, /*COM_SkipPath(*/ recent_file, "folder");
			else AppendMenu_Grayed (head, "Last Created File: None");
		}
		else
		{
			AppendMenu_Inert (head, "Available in windowed mode only:");
			AppendMenu_Grayed	(head, String_Edit_SlashesBack_Like_Windows(va("Quake Folder: %s", host_parms->basedir)));
			AppendMenu_Grayed (head, "Open Folder In Explorer");
			if (recent_file[0])
				AppendMenu_Grayed (head, COM_SkipPath(recent_file));
			else AppendMenu_Grayed (head, "Last Created File: None");
		}

	}

	// REPLACE the existing menu by assigning the new one and destroying the old one.
	SetMenu(mainwindow, mainmenu);
	if (oldmenu)
		DestroyMenu (oldmenu);

}

#endif // Baker change +

