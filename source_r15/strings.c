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
// common_string.c -- extended string functionality

#include "quakedef.h"

// Copy an allocated string to a static temp buffer, return temp buffer.
// Meanwhile ... deallocate the allocated string.
char *veg(const char* x)
{
	static char	string[1024];

	q_strlcpy (string, x, sizeof(string) );

	free ( (void*) x);
	return string;
}


char *COM_NiceFloatString (float floatvalue)
{
	static char buildstring[32];
	int			i;

	q_snprintf (buildstring, sizeof(buildstring), "%f", floatvalue);

	// Strip off ending zeros
	for (i = strlen(buildstring) - 1 ; i > 0 && buildstring[i] == '0' ; i--)
		buildstring[i] = 0;

	// Strip off ending period
	if (buildstring[i] == '.')
		buildstring[i] = 0;

	return buildstring;
}

int COM_Minutes (int seconds)
{
	return seconds / 60;
}

int COM_Seconds (int seconds)
{
	return seconds %60;
}


// Finds match to whole word within a string (comma and null count as delimiters)
qboolean COM_ListMatch (const char *liststr, const char *itemstr)
{
	const char	*start, *matchspot, *nextspot;
	int		itemlen;

	// Check for disqualifying situations ...
	if (liststr == NULL || itemstr == NULL || *itemstr == 0 || strchr (itemstr, ','))
		return false;

	itemlen = strlen (itemstr);

	for (start = liststr ; matchspot = strstr (start, itemstr) ; start = nextspot)
	{
		nextspot = matchspot + itemlen;

		// Validate that either the match begins the string or the previous character is a comma
		// And that the terminator is either null or a comma.  This protects against partial
		// matches

		if ((matchspot == start || *(matchspot - 1) == ',') && (*nextspot == 0 || *nextspot == ','))
			return true; // matchspot;
	}

	return false;
}

int	COM_StringCount (const char *bigstring, const char *findwhat)
{
	char	*cursor = strstr (bigstring, findwhat);
	int		strlen_findwhat = strlen (findwhat);
	int		count = 0;

	for (; cursor; count++, cursor = strstr (cursor, findwhat))
		cursor += strlen_findwhat; // Advance position beyond current match

	return count;
}

char* String_Edit_SlashesForward_Like_Unix (char *WindowsStylePath)
{
	int	i;
	// Translate "\" to "/"
	for (i = 0 ; i < Q_strlen(WindowsStylePath) ; i ++)
		if (WindowsStylePath[i] == '\\')
			WindowsStylePath[i] = '/';

	return WindowsStylePath;
}


// Turns c:/quake/id1 into c:\quake\id1
char* String_Edit_SlashesBack_Like_Windows (char *UnixStylePath)
{
	int i;

	// Translate "/" to "\"
	for (i = 0 ; i < Q_strlen(UnixStylePath) ; i ++)
		if (UnixStylePath[i] == '/')
			UnixStylePath[i] = '\\';

	return UnixStylePath;
}


void String_Edit_Reduce_To_Parent_Path (char* myPath)
{
	char* terminatePoint = strrchr (myPath, '/');

	if (terminatePoint)
		*terminatePoint = '\0';

}

// Better way to check if a string ends with something.
qboolean String_Does_End_With (const char *bigstring, const char *littlestring)
{
	// Check to ensure strlen is equal or exceeds strlen extension
	if (Q_strlen(bigstring) < Q_strlen(littlestring))	// This allows for stupid "situations like filename is '.dem'
		return false;

	// Checked the math many times.  It is correct.
	if (Q_strcmp (bigstring + strlen(bigstring) - strlen(littlestring), littlestring) !=0 )
		return false;

	// Matches
	return true;
}


// Better way to check if a string ends with something.
qboolean String_Does_End_With_Caseless (const char *bigstring, const char *littlestring)
{
	// Check to ensure strlen is equal or exceeds strlen extension
	if (Q_strlen(bigstring) < Q_strlen(littlestring))	// This allows for stupid "situations like filename is '.dem'
		return false;

	// Checked the math many times.  It is correct.
	if (Q_strcasecmp (bigstring + strlen(bigstring) - strlen(littlestring), littlestring) !=0 )
		return false;

	// Matches
	return true;
}

const char* KeyValue_GetKey (keyvalue_t table[], int val)
{
	keyvalue_t* cur;

	for (cur = &table[0]; cur->keystring != NULL; cur++)
	{
		if (cur->value == val)
			return cur->keystring;
	}

	return NULL;
}

keyvalue_t* KeyValue_GetEntry (keyvalue_t table[], const char* keystring)
{
	int i;
	
	for (i = 0; table[i].keystring; i++)
	{
		keyvalue_t* entry = &table[i];
		if (Q_strcasecmp(entry->keystring, keystring) == 0 )
			return entry;
	}
	return NULL; // Not found
}


qboolean Parse_Float_From_String (float *retval, const char *string_to_search, const char *string_to_find)
{
	int beginning_of_line, value_spot, value_spot_end;
	int spot, end_of_line, i;
	char *cursor;
//	qboolean found;

	cursor = strstr(string_to_search, string_to_find);

	while (1)
	{
		if (cursor == NULL)
			return false; // Didn't find it.

		spot = cursor - string_to_search; // Offset into string.

		// Find beginning of line.  Go from location backwards until find newline or hit beginning of buffer

		for (i = spot - 1, beginning_of_line = -1 ; i >= 0 && beginning_of_line == -1; i-- )
			if (string_to_search[i] == '\n')
				beginning_of_line = i + 1;
			else if (i == 0)
				beginning_of_line = 0;

		if (beginning_of_line == -1)
			return false; // Didn't find beginning of line?  Errr.  This shouldn't happen

		if (beginning_of_line != spot)
		{
			// Be skeptical of matches that are not beginning of the line
			// These might be aliases or something and the engine doesn't write the config
			// in this manner.  So advance the cursor past the search result and look again.
			cursor = strstr(cursor + strlen(string_to_find), string_to_find);
			continue; // Try again
		}

		break;
	}

	// Find end of line. Go from location ahead of string and stop at newline or it automatically stops at EOF

	for (i = spot + strlen(string_to_find), end_of_line = -1; string_to_find[i] && end_of_line == -1; i++ )
		if (string_to_search[i] == '\r' || string_to_search[i] == '\n')
			end_of_line = i - 1;

	if (end_of_line == -1) // Hit null terminator
		end_of_line = strlen(string_to_search) - 1;

	// We have beginning of line and end of line.  Go from spot + strlen forward and skip spaces and quotes.
	for (i = spot + strlen(string_to_find), value_spot = -1, value_spot_end = -1; i <= end_of_line && (value_spot == -1 || value_spot_end == -1); i++)
		if (string_to_search[i] == ' ' || string_to_search[i] == '\"')
		{
			// If we already found the start, we are looking for the end and just found it
			// Otherwise we are just skipping these characters because we ignore them
			if (value_spot != -1)
				value_spot_end = i - 1;

		}
		else if (value_spot == -1)
			value_spot = i; // We didn't have the start but now we found it

	// Ok check what we found

	if (value_spot == -1)
		return false; // No value

	if (value_spot_end == -1)
		value_spot_end = end_of_line;

	do
	{
		// Parse it and set return value
		char temp = string_to_search[value_spot_end + 1];
		char* temptermspot = (char*)&string_to_search[value_spot_end + 1]; // slightly evil

		//string_to_search[value_spot_end + 1] = 0;
		*temptermspot = 0;
		*retval = Q_atof (&string_to_search[value_spot]);
		*temptermspot = temp;

	} while (0);
	return true;

}

void String_Edit_RemoveExtension (char *filename_to_edit)
{
	char	*period_found = strrchr(filename_to_edit, '.');

	if (period_found )
	{
		// Special circumstance to avoid stripping "directory/mine.something/filename" incorrectly
		// In otherwords, if a file with no extension has a parent directory with an extension.
		char	*slash_found = strrchr(filename_to_edit, '/');

		if (slash_found && period_found < slash_found)
			return;

		while (*filename_to_edit && filename_to_edit != period_found)
			filename_to_edit++;
		
		// Null terminate at dot
		*filename_to_edit = 0;
	}
	
}



void String_Edit_RemoveTrailingSpaces (char *string_with_spaces)
{
	while (string_with_spaces[strlen(string_with_spaces)-1] == ' ') // remove trailing spaces
		string_with_spaces[strlen(string_with_spaces)-1] = 0;
}



#include <ctype.h> // isdigit, tolower, toupper, isupper
void String_Edit_To_Lower_Case (char* string)
{
	char* cursor = string;
	while (*cursor)
	{
		if ( isupper(*cursor) )
			*cursor = tolower(*cursor);
		
		cursor++;
	}
}

qboolean String_Contains_Uppercase (const char* string)
{
	while (*string)
	{
		if ( isupper(*string) )
			return true;
		string++;
	}
	return false;
}
