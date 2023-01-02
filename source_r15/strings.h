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

#ifndef __COMMON_STRING_H
#define __COMMON_STRING_H


char *veg(const char* x); 
// Copies a string into a static buffer and then frees the string


qboolean COM_ListMatch (const char *liststr, const char *itemstr);
// Finds item in list.  If found, returns true.


int	COM_StringCount (const char *bigstring, const char *findwhat);
// How many of X are in the bigstring.

char *COM_NiceFloatString (float floatvalue);
// Strips trailing zeros if necessary.  Cannot recall if this has any advantage over "%g".  Not broke, don't fix?

int COM_Minutes (int seconds);
// Returns number of minutes for X seconds

int COM_Seconds (int seconds);
// Returns numbers of seconds excluding minutes for X seconds



void String_Edit_To_Lower_Case (char* string);
// Obvious

char* String_Edit_SlashesForward_Like_Unix (char *WindowsStylePath);
// Converts all the Windows slashes "\" into Unix slashes "/"
// Returns pointer to provided modified string for convenience.

char* String_Edit_SlashesBack_Like_Windows (char *UnixStylePath);
// Converts all the Unix slashes "/" into Windows slashes "\"
// Returns pointer to provided modified string for convenience.

void String_Edit_Reduce_To_Parent_Path (char* myPath);
// Removes the last path component reducing to the parent's path.

qboolean String_Does_End_With (const char *bigstring, const char *littlestring);
// Does bigstring end with little string?  true or false.  case sensitive check

qboolean String_Does_End_With_Caseless (const char *bigstring, const char *littlestring);
// Does bigstring end with little string?  true or false.  caseless check

#define String_Does_Start_With(bigstring, littlestring) !strncmp(bigstring, littlestring, strlen(littlestring))
#define String_Does_Start_With_Caseless(bigstring, littlestring) !Q_strncasecmp(bigstring, littlestring, strlen(littlestring))
// Does bigstring begin with little string.  This does care about case.

void String_Edit_RemoveExtension (char *filename_to_edit);


// This makes it somewhat easy to make a constants/enumeration table
typedef struct
{
	const char* keystring;
	int value;
} keyvalue_t;

#define KEYVALUE(x) { #x , x }

const char* KeyValue_GetKey (keyvalue_t table[], int val);
keyvalue_t* KeyValue_GetEntry (keyvalue_t table[], const char* keystring);

qboolean Parse_Float_From_String (float *retval, const char *string_to_search, const char *string_to_find);
// Searches through a text buffer (string_to_search) for a given string (string_to_find) and returns
// the int retval for the value of a number following it on that line.  Like search "config.cfg" buffer" for "vid_width".

qboolean String_Contains_Uppercase (const char* string);

void String_Edit_RemoveTrailingSpaces (char *string_with_spaces);

#endif	/* __COMMON_STRING_H */

