/* This file isn't done!  

Versus Quakespasm:

1.  Mark V can size the console with CTRL+up/down arrows
2. I don't believe in a different K_COMMAND key for OS X as the keyboard is
   the same as a Windows/Linux keyboard, but the key names are different.
   This makes sharing a config between Windows and OS X a pain.

*/

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

#include "quakedef.h"

/* key up events are sent even if in console mode */

#define		HISTORY_FILE_NAME	"history.txt"

char		key_lines[CMDLINES][MAXCMDLINE];
int		key_linepos;
int		shift_down=false;
int			key_lastpress;
int			key_insert = true;	//johnfitz -- insert key toggle (for editing)
double		key_blinktime; //johnfitz -- fudge cursor blinking to make it easier to spot in certain cases

int			edit_line=0;
int			history_line=0;

keydest_t	key_dest = key_game;

int			key_count;			// incremented every key event

char		*keybindings[256];
int			keyshift[256];		// key to map to if shift held down in console
int			key_repeats[256];	// if > 1, it is autorepeating
qboolean	consolekeys[256];	// if true, can't be rebound while in console
qboolean	menubound[256];	// if true, can't be rebound while in menu
qboolean	keydown[256];
qboolean	keygamedown[256];  // Baker: to prevent -aliases from triggering

qboolean	repeatkeys[256]; //johnfitz -- if true, autorepeat is enabled for this key

typedef struct
{
	const char	*name;
	int		keynum;
} keyname_t;

keyname_t keynames[] =
{
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},

//	{"KP_NUMLOCK",		K_KP_NUMLOCK},
	{"KP_SLASH",		K_KP_SLASH },
	{"KP_STAR",			K_KP_STAR },
	{"KP_MINUS",		K_KP_MINUS },
	{"KP_HOME",			K_KP_HOME },
	{"KP_UPARROW",		K_KP_UPARROW },
	{"KP_PGUP",			K_KP_PGUP },
	{"KP_PLUS",			K_KP_PLUS },
	{"KP_LEFTARROW",	K_KP_LEFTARROW },
	{"KP_5",			K_KP_5 },
	{"KP_RIGHTARROW",	K_KP_RIGHTARROW },
	{"KP_END",			K_KP_END },
	{"KP_DOWNARROW",	K_KP_DOWNARROW },
	{"KP_PGDN",			K_KP_PGDN },
	{"KP_ENTER",		K_KP_ENTER },
	{"KP_INS",			K_KP_INS },
	{"KP_DEL",			K_KP_DEL },

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},
	{"AUX17", K_AUX17},
	{"AUX18", K_AUX18},
	{"AUX19", K_AUX19},
	{"AUX20", K_AUX20},
	{"AUX21", K_AUX21},
	{"AUX22", K_AUX22},
	{"AUX23", K_AUX23},
	{"AUX24", K_AUX24},
	{"AUX25", K_AUX25},
	{"AUX26", K_AUX26},
	{"AUX27", K_AUX27},
	{"AUX28", K_AUX28},
	{"AUX29", K_AUX29},
	{"AUX30", K_AUX30},
	{"AUX31", K_AUX31},
	{"AUX32", K_AUX32},

	{"PAUSE", K_PAUSE},

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},

	{"SEMICOLON", ';'},	// because a raw semicolon seperates commands
	{"BACKQUOTE", '`'},	// allow binding of backquote/tilde to toggleconsole after unbind all
	{"TILDE", '`'},		// allow binding of backquote/tilde to toggleconsole after unbind all

	{NULL, 0 } // Baker: Note that this list has null termination 
};

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

float	scr_con_size = 0.5;
void AdjustConsoleHeight (const int delta)
{
	int		height;


	if (!cl.worldmodel || cls.signon != SIGNONS)
		return;
	height = (scr_con_size * glheight + delta + 5) / 10;
	height *= 10;
	if (delta < 0 && height < 30)
		height = 30;
	if (delta > 0 && height > (int)(vid.screen.height - 10))
		height = vid.screen.height - 10;
	scr_con_size = (float)height / glheight;
}

// Inserts text in a buffer at offset (with buf maxsize of bufsize) and returns inserted character count
size_t String_Insert_String_Into_Buffer_At (const char* insert_text, char* buffer, size_t offset, size_t bufsize)
{
	// about our buffer ...
	size_t buffer_maxsize = bufsize - 1; // room for null
	size_t buffer_strlen = strlen(buffer);
	size_t buffer_remaining = buffer_maxsize - buffer_strlen;	// max insertable chars
	size_t chars_after_insert_point = buffer_strlen - offset;

	// now factor in insertion text
	size_t insert_text_len = strlen(insert_text);
	size_t copy_length = insert_text_len > buffer_remaining ? buffer_remaining : insert_text_len;

	if (copy_length)
	{
		memmove (buffer + offset + copy_length, buffer + offset, chars_after_insert_point);
		memcpy (buffer + offset, insert_text, copy_length);		
	}

	return copy_length;
}

/*
====================
Key_Console -- johnfitz -- heavy revision

Interactive line editing and console scrollback
====================
*/
extern char *con_text, 	key_tabpartial[MAXCMDLINE];
extern int con_current, con_linewidth, con_vislines;

void Key_Console (int key)
{
	switch (key)
	{
	case K_ENTER:
	case K_KP_ENTER:
		key_tabpartial[0] = 0;
		Cbuf_AddText (key_lines[edit_line]+1);	// skip the prompt
		Cbuf_AddText ("\n");
		Con_Printf ("%s\n",key_lines[edit_line]);
		edit_line = (edit_line + 1) & (CMDLINES-1);
		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_lines[edit_line][1] = 0; //johnfitz -- otherwise old history items show up in the new edit line
		key_linepos = 1;
		if (cls.state == ca_disconnected)
			SCR_UpdateScreen (); // force an update, because the command may take some time
		return;

	case K_TAB:
		Con_TabComplete ();
		return;

	case K_BACKSPACE:

		key_tabpartial[0] = 0;
		if (key_linepos > 1)
		{
			strcpy(key_lines[edit_line] + key_linepos - 1, key_lines[edit_line] + key_linepos);
			key_linepos--;
		}
		return;

	case K_DEL:
		key_tabpartial[0] = 0;
		if (key_linepos < (int)strlen(key_lines[edit_line]))
			strcpy(key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + 1);
		return;

	case K_HOME:
		if (keydown[K_CTRL])
		{
			//skip initial empty lines
			int		i, x;
			char	*line;

			for (i = con_current - con_totallines + 1 ; i <= con_current ; i++)
			{
				line = con_text + (i%con_totallines)*con_linewidth;
				for (x=0 ; x<con_linewidth ; x++)
				{
					if (line[x] != ' ')
						break;
				}
				if (x != con_linewidth)
					break;
			}
			con_backscroll = CLAMP(0, con_current-i%con_totallines-2, con_totallines-(glheight>>3)-1);
		}
		else	key_linepos = 1;
		return;

	case K_END:
		if (keydown[K_CTRL])
			con_backscroll = 0;
		else key_linepos = strlen(key_lines[edit_line]);
		return;

	case K_PGUP:
	case K_MWHEELUP:
		con_backscroll += keydown[K_CTRL] ? ((con_vislines>>3) - 4) : 2;
		if (con_backscroll > con_totallines - (int)(vid.screen.height>>3) - 1)
			con_backscroll = con_totallines - (int)(vid.screen.height>>3) - 1;
		return;

	case K_PGDN:
	case K_MWHEELDOWN:
		con_backscroll -= keydown[K_CTRL] ? ((con_vislines>>3) - 4) : 2;
		if (con_backscroll < 0)
			con_backscroll = 0;
		return;

	case K_LEFTARROW:
		if (key_linepos > 1)
		{
			key_linepos--;
			key_blinktime = realtime;
		}
		return;

	case K_RIGHTARROW:
		if ((int)strlen(key_lines[edit_line]) == key_linepos)
		{
			if ((int)strlen(key_lines[(edit_line + (CMDLINES-1)) & (CMDLINES-1)]) <= key_linepos)
				return; // no character to get

			key_lines[edit_line][key_linepos] = key_lines[(edit_line + (CMDLINES-1)) & (CMDLINES-1)][key_linepos];
			key_linepos++;
			key_lines[edit_line][key_linepos] = 0;
		}
		else
		{
			key_linepos++;
			key_blinktime = realtime;
		}
		return;

	case K_UPARROW:
		if (keydown[K_CTRL])
		{
			AdjustConsoleHeight (-10);
			return;
		}

		key_tabpartial[0] = 0;
		do
		{
			history_line = (history_line - 1) & (CMDLINES-1);
		} while (history_line != edit_line
				&& !key_lines[history_line][1]);
		if (history_line == edit_line)
			history_line = (edit_line+1)&(CMDLINES-1);
		Q_strcpy(key_lines[edit_line], key_lines[history_line]);
		key_linepos = Q_strlen(key_lines[edit_line]);
		return;

	case K_DOWNARROW:
		if (keydown[K_CTRL])
		{
			AdjustConsoleHeight (10);
			return;
		}

		key_tabpartial[0] = 0;

		if (history_line == edit_line)
		{
			//clear editline
			key_lines[edit_line][1] = 0;
			key_linepos = 1;
			return;
		}

		do 
		{
			history_line = (history_line + 1) & (CMDLINES-1);
		} while (history_line != edit_line && !key_lines[history_line][1]);

		if (history_line == edit_line)
		{
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
		}
		else
		{
			Q_strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = Q_strlen(key_lines[edit_line]);
		}
		return;

	case K_INS:
		key_insert ^= 1;
		return;

	case 'v':
	case 'V':
		if (keydown[K_CTRL])
		{
			const char	*clip_text = Sys_GetClipboardTextLine (); // chars < ' ' removed
			key_linepos += String_Insert_String_Into_Buffer_At (clip_text, key_lines[edit_line], key_linepos, MAXCMDLINE);
			return;
		}
		break;	
	}

	if (key < 32 || key > 127)
		return;	// non printable

	if (key_linepos < MAXCMDLINE - 1 )
	{
		int i;

		key_tabpartial[0] = 0; //johnfitz

		if (key_insert)	// check insert mode
		{
			// can't do strcpy to move string to right
			i = strlen(key_lines[edit_line]) - 1;

			if (i == (MAXCMDLINE - 2) )
				i--;

			for (; i >= key_linepos; i--)
				key_lines[edit_line][i + 1] = key_lines[edit_line][i];
		}

		// only null terminate if at the end
		i = key_lines[edit_line][key_linepos];
		key_lines[edit_line][key_linepos] = key;
		key_linepos++;

		if (!i)
			key_lines[edit_line][key_linepos] = 0;
	}
	//johnfitz
}

//============================================================================

#define MAX_CHAT_SIZE 45
qboolean chat_team = false;
char chat_buffer[MAX_CHAT_SIZE];
static int chat_bufferlen = 0;
void Key_EndChat (void)
{
	key_dest = key_game;
	chat_bufferlen = 0;
	chat_buffer[0] = 0;
}

void Key_Message (int key)
{
	if (key == K_ENTER)
	{
		if (chat_team)
			Cbuf_AddText ("say_team \"");
		else
			Cbuf_AddText ("say \"");
		Cbuf_AddText(chat_buffer);
		Cbuf_AddText("\"\n");

		Key_EndChat ();
		return;
	}

	if (key == K_ESCAPE)
	{
		Key_EndChat ();
		return;
	}

	if (key < 32 || key > 127)
		return;	// non printable

// ^^ UH this makes the key backspace thing unreachable!
	if (key == K_BACKSPACE)
	{
		if (chat_bufferlen)
			chat_buffer[--chat_bufferlen] = 0;
		return;
	}

	if (chat_bufferlen == MAX_CHAT_SIZE - (chat_team ? 6 : 1)) // 6 vs 1 = so same amount of text onscreen in "say" versus "say_team"
		return; // all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================


/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int Key_StringToKeynum (const char *str)
{
	keyname_t	*kn;

	if (!str || !str[0])
		return -1;

	if (!str[1])
		return str[0];

	for (kn=keynames ; kn->name ; kn++)
	{
		if (!Q_strcasecmp(str,kn->name))
			return kn->keynum;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
const char *Key_KeynumToString (int keynum)
{
	static	char	tinystr[2];
	keyname_t	*kn;
	
	if (keynum == -1)
		return "<KEY NOT FOUND>";
	if (keynum > 32 && keynum < 127)
	{	// printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for (kn=keynames ; kn->name ; kn++)
	{
		if (keynum == kn->keynum)
			return kn->name;
	}

	return "<UNKNOWN KEYNUM>";
}

const char* Key_ListExport (void)
{
	static char returnbuf[32];
	
	static last = -1; // Top of list.
	// We want first entry >= this
	int		wanted = CLAMP(0, last + 1, sizeof(keynames)/sizeof(keynames[0]) );  // Baker: bounds check
	keyname_t	*kn;
	int i;

	for (i = wanted, kn = &keynames[i]; kn->name ; kn ++, i++)
	{
		// Baker ignore single character keynames.
		if (memcmp(kn->name, "JOY", 3)==0)
			continue; // Baker --- Do not want
		if (memcmp(kn->name, "AUX", 3)==0)
			continue; // Baker --- Do not want

		if (kn->name[0] && kn->name[1] && i >= wanted) // Baker: i must be >=want due to way I setup this function
		{
			q_strlcpy (returnbuf, kn->name, sizeof(returnbuf) );

			last = i;
//			Con_Printf ("Added %s\n", kn->name);
			return returnbuf;
		}
	}

	// Not found, reset to top of list and return NULL
	last = -1;
	return NULL;
}




/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding (int keynum, const char *binding)
{
	if (keynum == -1)
		return;

// free old bindings
	if (keybindings[keynum])
	{
		Z_Free (keybindings[keynum]);
		keybindings[keynum] = NULL;
	}

// allocate memory for new binding
	keybindings[keynum] = Z_Strdup(binding);
}

/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f (void)
{
	int		b;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("unbind <key> : remove commands from a key\n");
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b==-1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_SetBinding (b, "");
}

void Key_Unbindall_f (void)
{
	int		i;

	for (i=0 ; i<256 ; i++)
	{
		if (keybindings[i])
			Key_SetBinding (i, "");
	}
}

/*
============
Key_Bindlist_f -- johnfitz
============
*/
void Key_Bindlist_f (void)
{
	int		i, count;

	count = 0;
	for (i=0 ; i<256 ; i++)
	{
		if (keybindings[i] && *keybindings[i])
		{
			Con_SafePrintf ("   %-12s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
			count++;
		}
	}
	Con_SafePrintf ("%i bindings\n", count);
}

/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f (void)
{
	int			i, c, b;
	char		cmd[1024];

	c = Cmd_Argc();

	if (c != 2 && c != 3)
	{
		Con_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b==-1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2)
	{
		if (keybindings[b])
			Con_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b] );
		else
			Con_Printf ("\"%s\" is not bound\n", Cmd_Argv(1) );
		return;
	}

// copy the rest of the command line
	cmd[0] = 0;
	for (i=2 ; i< c ; i++)
	{
		q_strlcat (cmd, Cmd_Argv(i), sizeof(cmd));
		if (i != (c-1))
			q_strlcat (cmd, " ", sizeof(cmd));
	}

	Key_SetBinding (b, cmd);
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings (FILE *f)
{
	int		i;

	// unbindall before loading stored bindings:
	if (cfg_unbindall.value)
		fprintf (f, "unbindall\n");
	for (i=0 ; i<256 ; i++)
	{
		if (keybindings[i] && *keybindings[i])
				fprintf (f, "bind \"%s\" \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
	}
}


void History_Init (void)
{
	int i, c;
	FILE *hf;

	for (i = 0; i < CMDLINES; i++) 
	{
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	hf = FS_fopen_read(va("%s/%s", com_basedir, HISTORY_FILE_NAME), "rt");
	if (hf != NULL)
	{
		do
		{
			i = 1;
			do
			{
				c = fgetc(hf);
				key_lines[edit_line][i++] = c;
			} while (c != '\r' && c != '\n' && c != EOF && i < MAXCMDLINE);
			key_lines[edit_line][i - 1] = 0;
			edit_line = (edit_line + 1) & (CMDLINES - 1);
			/* for people using a windows-generated history file on unix: */
			if (c == '\r' || c == '\n')
			{
				do
					c = fgetc(hf);
				while (c == '\r' || c == '\n');
				if (c != EOF)
					ungetc(c, hf);
				else	c = 0; /* loop once more, otherwise last line is lost */
			}
		} while (c != EOF && edit_line < CMDLINES);
		FS_fclose(hf);

		history_line = edit_line = (edit_line - 1) & (CMDLINES - 1);
		key_lines[edit_line][0] = ']';
		key_lines[edit_line][1] = 0;
	}
}

void History_Shutdown (void)
{
	int i;
	FILE *hf;
	char lastentry[1024] = {0};

	hf = FS_fopen_write (va("%s/%s", com_basedir, HISTORY_FILE_NAME), "wt");
	if (hf != NULL)
	{
		i = edit_line;
		do
		{
			i = (i + 1) & (CMDLINES - 1);
		} while (i != edit_line && !key_lines[i][1]);

		while (i != edit_line && key_lines[i][1])
		{
			if (lastentry[0]==0 || Q_strcasecmp (lastentry, key_lines[i] + 1) != 0) // Baker: prevent saving the same line multiple times in a row
				if (Q_strncasecmp(key_lines[i]+1, "quit", 4) != 0) // Baker: why save quit to the history file
					fprintf(hf, "%s\n", key_lines[i] + 1);

			q_strlcpy (lastentry, key_lines[i] + 1, sizeof(lastentry));
			i = (i + 1) & (CMDLINES - 1);
		}
		FS_fclose(hf);
	}
}

/*
===================
Key_Init
===================
*/
void Key_Init (void)
{
	int		i;

	History_Init ();

	key_blinktime = realtime; //johnfitz

//
// init ascii characters in console mode
//
	for (i=32 ; i<128 ; i++)
		consolekeys[i] = true;
	consolekeys[K_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_INS] = true;
	consolekeys[K_DEL] = true;
	consolekeys[K_HOME] = true;
	consolekeys[K_END] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_SHIFT] = true;
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;
	consolekeys['`'] = false;
	consolekeys['~'] = false;

	//johnfitz -- repeating keys
	for (i=0 ; i<256 ; i++)
		repeatkeys[i] = false;
	repeatkeys[K_BACKSPACE] = true;
	repeatkeys[K_DEL] = true;
	repeatkeys[K_PAUSE] = true;
	repeatkeys[K_PGUP] = true;
	repeatkeys[K_PGDN] = true;
	repeatkeys[K_LEFTARROW] = true;
	repeatkeys[K_RIGHTARROW] = true;
	//johnfitz

	for (i=0 ; i<256 ; i++)
		keyshift[i] = i;
	for (i='a' ; i<='z' ; i++)
		keyshift[i] = i - 'a' + 'A';
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = true;
	for (i=0 ; i<12 ; i++)
		menubound[K_F1+i] = true;

//
// register our functions
//
	Cmd_AddCommand ("bindlist",Key_Bindlist_f); //johnfitz
	Cmd_AddCommand ("binds",Key_Bindlist_f); // Baker --- alternate way to get here
	Cmd_AddCommand ("bind",Key_Bind_f);
	Cmd_AddCommand ("unbind",Key_Unbind_f);
	Cmd_AddCommand ("unbindall",Key_Unbindall_f);
}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
qboolean Keys_All_Up_But_Me (int key)
{
	qboolean anydown = false;
	int i;

	for (i = 0; i < 256; i ++)
	{
		if (key == i) // Don't check this key of course, it will be up soon.  But we can't do it here
			continue;
		else if (keydown[i])
			return false;
	}

	// Nothing down except the key being released
	return true;
}

enum
{
	COMBO_NONE = 0,
	COMBO_ALT_ENTER,
	COMBO_CTRL_M,
	COMBO_CTRL_C,
	COMBO_CTRL_V,
};

void Key_Event (int key, qboolean down)
{
	
	char		*kb;
	char		cmd[1024];
	qboolean wasgamekey = false;

	static int	in_key_combo = COMBO_NONE; // Combo hit.  Wait for all keys up.
	static int  enact_combo  = COMBO_NONE; // Activate combo
	
	if (in_key_combo && !down && enact_combo == 0)
		if (Keys_All_Up_But_Me (key))
		{
			// Combo was in effect and all keys are now released
			// Except current key which was just released.
			// Enact this combo.
			enact_combo = in_key_combo;

			Key_Event (key, down); // Process like normal ...

			// Now perform the combo action ...
			if (enact_combo == COMBO_ALT_ENTER) 
				VID_Alt_Enter_f ();
			if (enact_combo == COMBO_CTRL_M)		
				Sound_Toggle_Mute_f ();
			if (texturepointer_on && enact_combo == COMBO_CTRL_C)		
				TexturePointer_ClipboardCopy ();
			if (texturepointer_on && enact_combo == COMBO_CTRL_V)		
				TexturePointer_ClipboardPaste ();


			enact_combo = in_key_combo = 0;  // Reset
			return; // Get out.  We are done.  
		}

	keydown[key] = down;

	// Enter key released while 
	if (key == K_ENTER  && down && keydown[K_ALT] && in_key_combo == COMBO_NONE /* && vid_altenter_toggle.integer*/)
		in_key_combo = COMBO_ALT_ENTER; // Once all keys and buttons are released, vid change occurs.
	
	if (key == 'm' /* M */ && down && keydown[K_CTRL] && in_key_combo == COMBO_NONE)
		in_key_combo = COMBO_CTRL_M;
	
	if (key == 'c' /* C */ && down && keydown[K_CTRL] && in_key_combo == COMBO_NONE)
		in_key_combo = COMBO_CTRL_C;

	if (key == 'v' /* V */ && down && keydown[K_CTRL] && in_key_combo == COMBO_NONE)
		in_key_combo = COMBO_CTRL_V;


	wasgamekey = keygamedown[key]; // Baker: to prevent -aliases being triggered in-console needlessly
	if (!down) 
	{
		keygamedown[key] = false; // We can always set keygamedown to false if key is released
	}

	if (!down)
		key_repeats[key] = 0;

	key_lastpress = key;
	key_count++;
	if (key_count <= 0)
	{
		return;		// just catching keys for Con_NotifyBox
	}

// update auto-repeat status
	if (down)
	{
		key_repeats[key]++;
		if (key_repeats[key] > 1)
		{
			if (key_dest == key_game && !con_forcedup)
				return;	// ignore autorepeats in game mode
		}
	}

	if (key == K_SHIFT)
		shift_down = down;

//
// handle escape specialy, so the user can never unbind it
//
	if (key == K_ESCAPE)
	{
		if (!down)
			return;
		switch (key_dest)
		{
		case key_message:
			Key_Message (key);
			break;
		case key_menu:
			M_Keydown (key);
			break;
		case key_game:
		case key_console:
			M_ToggleMenu_f ();
			break;
		default:
			Sys_Error ("Bad key_dest");
		}
		return;
	}

#define DEMO_FAST_FORWARD_REVERSE_SPEED_5 5
// PGUP and PGDN rewind and fast-forward demos
	if (cls.demoplayback && cls.demonum == -1 && !cls.timedemo && !cls.capturedemo)
	{
		if (key == K_PGUP || key == K_PGDN)
		{
			if (key_dest == key_game && down /* && cls.demospeed == 0 && cls.demorewind == false*/)
			{
				// During normal demoplayback, PGUP/PGDN will rewind and fast forward (if key_dest is game)
				if (key == K_PGUP)
				{
					cls.demospeed = DEMO_FAST_FORWARD_REVERSE_SPEED_5;
					cls.demorewind =  false;
				}
				else if (key == K_PGDN)
				{
					cls.demospeed = DEMO_FAST_FORWARD_REVERSE_SPEED_5;
					cls.demorewind = true;
				}
				return; // If something is bound to it, do not process it.
			}
			else //if (!down && (cls.demospeed != 0 || cls.demorewind != 0))
			{
				// During normal demoplayback, releasing PGUP/PGDN resets the speed
				// We need to check even if not key_game in case something silly happened (to be safe)
				cls.demospeed = 0;
				cls.demorewind = false;

				if (key_dest == key_game)
					return; // Otherwise carry on ...
			}
		}
	}

//
// key up events only generate commands if the game key binding is
// a button command (leading + sign).  These will occur even in console mode,
// to keep the character from continuing an action started before a console
// switch.  Button commands include the kenum as a parameter, so multiple
// downs can be matched with ups
//
	if (!down)
	{
		// Baker: we only want to trigger -alias if appropriate
		//        but we ALWAYS want to exit if the key is up
		if (wasgamekey)
		{
			kb = keybindings[key];
			if (kb && kb[0] == '+')
			{
				q_snprintf (cmd, sizeof(cmd), "-%s %i\n", kb+1, key);
				Cbuf_AddText (cmd);
			}
			if (keyshift[key] != key)
			{
				kb = keybindings[keyshift[key]];
				if (kb && kb[0] == '+')
				{
					q_snprintf (cmd, sizeof(cmd), "-%s %i\n", kb+1, key);
					Cbuf_AddText (cmd);
				}
			}
		}	
		return;
	}

//
// during demo playback, most keys bring up the main menu
//
// Baker:  Quake was intended to bring up the menu with keys during the intro.
// so the user knew what to do.  But if someone does "playdemo" that isn't the intro.
// So we want this behavior ONLY when startdemos are in action.  If startdemos are
// not in action, cls.demonum == -1

	if (cls.demonum >= 0) // We are in startdemos intro.  Bring up menu for keys.
	{
		if (cls.demoplayback && down && consolekeys[key] && key_dest == key_game)
		{
			M_ToggleMenu_f ();
			return;
		}
	}

//
// if not a consolekey, send to the interpreter no matter what mode is
//
	if ( (key_dest == key_menu && menubound[key]) || 
		(key_dest == key_console && !consolekeys[key]) || 
			(key_dest == key_game && ( !con_forcedup || !consolekeys[key] ) ) )
	{
		kb = keybindings[key];
		if (kb)
		{
			// Baker: if we are here, the key is down
			//        and if it is retrigger a bind
			//        it must be allowed to trigger the -bind
			//
			keygamedown[key]=true; // Let it be untriggered anytime

			if (kb[0] == '+')
			{	// button commands add keynum as a parm
				q_snprintf (cmd, sizeof(cmd), "%s %i\n", kb, key);
				Cbuf_AddText (cmd);
			}
			else
			{
				Cbuf_AddText (kb);
				Cbuf_AddText ("\n");
			}
		}
		return;
	}

	// Baker: I think this next line is unreachable!
	if (!down)
		return;		// other systems only care about key down events

	if (shift_down)
	{
		key = keyshift[key];
	}

	switch (key_dest)
	{
	case key_message:
		Key_Message (key);
		break;

	case key_menu:
		M_Keydown (key);
		break;

	case key_game:
	case key_console:
		Key_Console (key);
		break;

	default:
		Sys_Error ("Bad key_dest");
	}
}

/*
===================
Key_ClearStates
===================
*/
void Key_Release_Keys (void)
{
   int      i;

   for (i=0 ; i<256 ; i++)
   {
      if (keydown[i])
         Key_Event (i, false);
   }
	// Baker 3.87: Clear the shift as well.
	shift_down = false;
}

/*
// If we release the mouse, we don't any buttons down to be stuck
void Key_Release_Mouse_Buttons (void)
{
	if (keydown[K_MOUSE1])  Key_Event (K_MOUSE1, false);
	if (keydown[K_MOUSE2])  Key_Event (K_MOUSE2, false);
	if (keydown[K_MOUSE3])  Key_Event (K_MOUSE3, false);
	if (keydown[K_MOUSE4])  Key_Event (K_MOUSE4, false);
	if (keydown[K_MOUSE5])  Key_Event (K_MOUSE5, false);
}
*/
