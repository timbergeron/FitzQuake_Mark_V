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
// common_ez.c -- server code for moving users

#include "quakedef.h"





	#if 1 // Bandages.  Not gonna rewrite all my external code
		#define Sys_Error Sys_Error
		#define System_Clipboard_Set_Text Sys_CopyToClipboard
		#define NSLogc(x)
		#define Memory_calloc(_count, _num, _why) calloc(_count, _num)
		#define Memory_malloc(_count, _why) malloc(_count)
		#define Memory_strdup strdup
		#define Memory_realloc(_p, _size, _why) realloc (_p, _size)
//		#define fbool qboolean

		#ifdef _WIN32
			#define strcasecmp stricmp
			#define strncasecmp strnicmp
		#endif
	#endif




// strlcpy and strlcat from BSD.  And workaround for Windows vsnprintf.

void* Memory_free (void* x)
{
	free (x);
	return NULL;
}


#ifdef _WIN32
#define vsnprintf _VSNPrintf
#endif

size_t _VSNPrintf (char *buffer, const size_t count, const char *format, va_list args)
{
	int result;

	// For _vsnprintf, if the number of bytes to write exceeds buffer, then count bytes are written
	// and ñ1 is returned.

	result = _vsnprintf (buffer, count, format, args);

	// Conditionally null terminate the string
	if (result == -1 && buffer)
		buffer[count - 1] = '\0';

	return result;
}


	/*
	 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
	 *
	 * Permission to use, copy, modify, and distribute this software for any
	 * purpose with or without fee is hereby granted, provided that the above
	 * copyright notice and this permission notice appear in all copies.
	 *
	 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
	 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
	 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
	 */


	#ifndef HAVE_STRLCAT
	size_t strlcat(char *dst, const char *src, size_t siz)
	{
		register char *d = dst;
		register const char *s = src;
		register size_t n = siz;
		size_t dlen;

		/* Find the end of dst and adjust bytes left but don't go past end */
		while (n-- != 0 && *d != '\0')
			d++;
		dlen = d - dst;
		n = siz - dlen;

		if (n == 0)
			return(dlen + strlen(s));
		while (*s != '\0')
		{
			if (n != 1)
			{
				*d++ = *s;
				n--;
			}
			s++;
		}
		*d = '\0';

		return(dlen + (s - src));	/* count does not include NUL */
	}
	#endif  // #ifndef HAVE_STRLCAT


	#ifndef HAVE_STRLCPY

	size_t strlcpy(char *dst, const char *src, size_t siz)
	{
		char *d = dst;
		const char *s = src;
		size_t n = siz;

		/* Copy as many bytes as will fit */
		if (n != 0 && --n != 0)
		{
			do
			{
				if ((*d++ = *s++) == 0)
					break;
			} while (--n != 0);
		}

		/* Not enough room in dst, add NUL and traverse rest of src */
		if (n == 0)
		{
			if (siz != 0)
				*d = '\0';		/* NUL-terminate dst */
			while (*s++)
				;
		}

		return(s - src - 1);	/* count does not include NUL */
	}


	#endif  // #ifndef HAVE_STRLCPY

// Workaround Visual Studio 6 lack of a decent vsnprintf
size_t length_vsnprintf (/*char *buffer, const size_t count,*/ const char *format, va_list args)
{
	int bufsize = 16384;

	while (1)
	{
		char *buffer = Memory_calloc (1,bufsize,"length_printf");
		int count = bufsize;
		int result = 0;

		// For _vsnprintf, if the number of bytes to write exceeds buffer, then count bytes are written
		// and ñ1 is returned.

		result = _vsnprintf (buffer, count, format, args);

		buffer = Memory_free (buffer);
		if (result >= 0)
			return result;

		bufsize = bufsize * 2; // Double allocation
//		NSLogc ("Increased length_vsnprintf buffer to %i", bufsize);
	}

}



// More convenient dynamic strings
static const char* zeroLengthString = "";

// Allocates space for an easy string.  If wasn't allocated this space is nulled at start.
static void ezString_SetSize (ezString** stringEdit, size_t bufsize)
{
	if (bufsize == 0)
		Sys_Error("ezString_SetSize bufsize is 0");
	if (*stringEdit)
	{
		*stringEdit = Memory_realloc ( (void*)*stringEdit, bufsize, "ezstring realloc");
		// We have to assume it was null terminated, since we cannot know size?
		// Or do we strlen?
	}
	else
	{
		char* tbuf = Memory_malloc(bufsize, "ezstring alloc");
		tbuf[0] = 0; // Null terminate.  Why?  I'm not entirely sure myself since above realloc can't be assured of being safe.
		*stringEdit = tbuf;
		// If new size is less than old size.
		// Maybe so we can do blind operations on a previously unallocated stringEdit and it still works right. (i.e. it was null before)
	}
}

void ezString_Destroy (ezString** stringEdit)
{
	if (*stringEdit == NULL)
	{
//		Message_Warning("ezString_Destroy NULL string");  This is normal and valid so don't warn.  It's a convenience.
		return;
	}
	*stringEdit = Memory_free ( (void*)*stringEdit);
}



void ezString_Cat (ezString** stringEdit, const char* addText)
{
	if (addText == NULL)
		Sys_Error("ezString_Edit_Concatenate passed null string");
	else
	{
		// realloc: If the new size of the memory object would require movement of the object,
		// the space for the previous instantiation of the object is freed.

		size_t originallength = (*stringEdit == NULL) ? 0 : strlen(*stringEdit);
		size_t newbufsize = originallength + strlen(addText) + 1 /*null terminator*/;

		ezString_SetSize (stringEdit, newbufsize);

		strlcat ((char*)*stringEdit, addText, newbufsize);
	}
}

void ezString_PreCat (ezString** stringEdit, const char* addTextBefore)
{
	if (addTextBefore == NULL)
		Sys_Error("ezString_Edit_Concatenate passed null string");
	else
	{
		char* newBuf = NULL; // = ;
		size_t originallength = (*stringEdit == NULL) ? 0 : strlen(*stringEdit);
		size_t newbufsize = originallength + strlen(addTextBefore) + 1 /*null terminator*/;

		ezString_SetSize ((ezString**)&newBuf, newbufsize);
//		NSLogc("ezString_Edit_PreConcatenate %s and %s", *stringEdit ? *stringEdit : "NULL", addTextBefore);

		strlcpy (newBuf, addTextBefore, newbufsize);
		if (*stringEdit)
		{
			strlcat(newBuf, *stringEdit, newbufsize);
			ezString_Destroy(stringEdit);
		}
		*stringEdit = newBuf;  // Update!
	}
}


ezString* ezString_Set (ezString** stringEdit, const char* string)
{
	// If string is NULL deal with that situation, reassigning.
	const char* mystring = (string == NULL) ? zeroLengthString : string;

	// Get string length
	size_t bufsize =  strlen(mystring) + 1 /*null terminator*/;

	ezString_SetSize (stringEdit, bufsize);

	strlcpy((char*)*stringEdit, mystring, bufsize);
	return *stringEdit;
}


ezString* ezString_Set_N (ezString** stringEdit, const char* string, size_t length)
{
	size_t len_plus_term = length + 1;
	char* temp_string = Memory_calloc (1, len_plus_term, "temp set_n");
	strncpy (temp_string, string, length);

	ezString_Set (stringEdit, temp_string);
	Memory_free (temp_string);
	return *stringEdit;
}

// Removes the occurance of a single character from a string.
void ezString_Scrub (ezString** stringEdit, char killthis)
{
	ezString* cursor = *stringEdit;

	int count = 0;
	while (*cursor)
	{
		if (*cursor != killthis)
			count ++;

		cursor++;
	}


	{
		char* tempbuf = Memory_calloc (1, count + 1, "temp scrub");
		ezString* src = *stringEdit;
		char* dest= tempbuf;
		while (*src)
		{
			if (*src != killthis)
				*dest++ = *src;
			src++;
		}
		ezString_Set (stringEdit, tempbuf);
		Memory_free (tempbuf);
	}


}

ezString* Temp_Stringa (const char* string)
{
	// If string is NULL deal with that situation, reassigning.
	const char* mystring = (string == NULL) ? zeroLengthString : string;

	return Memory_strdup(mystring);
}

ezString* Temp_String_free (ezString* myezString)
{
	return Memory_free((void*)myezString);
}


// http://stackoverflow.com/questions/3919995/determining-sprintf-buffer-size-whats-the-standard
// For snprintf trick.  Would require strict compliance to ISO snprintf for our fake windows printf.
// char * a = malloc(snprintf(NULL, 0, "%d", 132) + 1);
// sprintf(a, "%d", 132);

ezString* ezString_Assume (ezString** stringEdit, const char* tempstringa)
{
	if (*stringEdit)
		ezString_Destroy (stringEdit);
	*stringEdit = tempstringa;
	return *stringEdit;
}

// Somewhat suboptimal (does vsnprintf twice), yet if vsnprintf/snprintf are available, handles all buffer sizes fine.
ezString* ezString_Printf (ezString** stringEdit, const char *format, ...)
{
	size_t bufsize;
	char* newBuf = NULL;
	va_list args;			// pointer to the list of arguments

	va_start (args, format);

	bufsize = length_vsnprintf(/*NULL, 0,*/ format, args) + 1;
	va_end (args);

	ezString_SetSize((ezString**)&newBuf, bufsize);

	va_start (args, format);
	vsnprintf (newBuf, bufsize, format, args);
	va_end (args);

	if (*stringEdit)
		ezString_Destroy(stringEdit);
	*stringEdit = newBuf;
	return *stringEdit;
}

// Somewhat suboptimal (double vsnprintf)
ezString* String_Printf_Alloc (const char *format, ...)
{
	size_t bufsize;
	char* newBuf = NULL;
	va_list args;			// pointer to the list of arguments

	va_start (args, format);

	bufsize = length_vsnprintf(/*NULL, 0,*/ format, args) + 1;
	va_end (args);

	ezString_SetSize((ezString**)&newBuf, bufsize);

	va_start (args, format);
	vsnprintf (newBuf, bufsize, format, args);
	va_end (args);

	return newBuf;
}

// Somewhat suboptimal (does vsnprintf twice), yet if vsnprintf/snprintf are available, handles all buffer sizes fine.
ezString* ezString_Cat_Printf (ezString** stringEdit, const char *format, ...)
{
	size_t bufsize;
	char* newBuf = NULL;
	va_list args;			// pointer to the list of arguments

	va_start (args, format);

	bufsize = length_vsnprintf(/*NULL, 0,*/ format, args) + 1;
	va_end (args);

	ezString_SetSize((ezString**)&newBuf, bufsize);

	va_start (args, format);
	vsnprintf (newBuf, bufsize, format, args);
	va_end (args);

	ezString_Cat (stringEdit, newBuf);
	ezString_Destroy (&newBuf);

	return *stringEdit;
}




// Various linked list functions.

ezSortList* ezChain_Add_With_ItemData (ezSortList** chaintop, const char* string, const char* itemdata)
{
	// Alloc new member.

	ezSortList* newitem = Memory_calloc (1, sizeof(ezSortList), "ezChain Add");
	if (string == NULL)
		Sys_Error ("Don't provide a NULL for a list");

	newitem->string = Memory_strdup (string);
	if (itemdata)
		newitem->itemdata = itemdata; // Assume ownership


	newitem->next = *chaintop; // Assume top of chain, first our "next" is old top
	*chaintop = newitem; // Now top is new entry.
	return newitem;
}

ezSortList* ezChain_Add (ezSortList** chaintop, const char* string)
{
	return ezChain_Add_With_ItemData (chaintop, string, NULL);
}


ezSortList* ezLinkEnd_Add_With_ItemData (ezSortList** chaintop, ezSortList* tail, const char* string, const char* itemdata)
{
	// Alloc new member.

	ezSortList* newitem = Memory_calloc (1, sizeof(ezSortList), "ezLinkEnd Add");
	if (string == NULL)
		Sys_Error ("Don't provide a NULL for a list");

	newitem->string = Memory_strdup (string);
	if (itemdata)
		newitem->itemdata = Memory_strdup(itemdata); // Assume ownership

	if (tail)
	{
		tail->next = newitem;
		newitem->prev = tail;
	} else *chaintop = newitem; // Assume the top (no tail means no head either)

	// New entry becomes tail; return new tail
	return newitem;
}



int ezSortList_Contains (ezSortList* list, const char* string)
{
	for ( ; list; list = list->next)
		if (strcmp(list->string, string) == 0)
			return 1;

	return 0;
}

ezSortList* ezChain_Add_Distinct (ezSortList** chaintop, const char* string)
{
	// Alloc new member.
	if (ezSortList_Contains(*chaintop, string))
		return NULL; // DID NOT ADD
	else
	{
		ezSortList* newb = Memory_calloc (1, sizeof(ezSortList), "ezChain Add");
		if (string == NULL)
			Sys_Error ("Don't provide a NULL for a list");

		newb->string = Memory_strdup (string);

		newb->next = *chaintop; // Assume top of chain, first our "next" is old top
		*chaintop = newb; // Now top is new entry.
		return newb;
	}
}


ezSortList* ezSortList_Add (ezSortList** alphaboss, const char* string)
{
	return ezSortList_Add_With_ItemData (alphaboss, string, NULL);
}

ezSortList* ezSortList_Add_With_ItemData_Assume_EZ (ezSortList** alphaboss, ezString* string, ezString* itemdata)
{
	ezSortList* cursor   = *alphaboss;
	ezSortList* newitem = Memory_calloc (1, sizeof(ezSortList), "ezSortList Add");
	newitem->string = string; // Assume ownership
	if (itemdata)
		newitem->itemdata = itemdata; // Assume ownership

	// Now link it in ...

	// Update alphabetical link list.  First check to see if we are top item
	if (*alphaboss == NULL || strcasecmp (newitem->string, (*alphaboss)->string) < 0 )
	{
		newitem->next = *alphaboss;
		if (newitem->next)					// If I have a next
			newitem->next->prev = newitem;	// It's next is now me

		*alphaboss = newitem;
	}
	else
	{
		// Not top ... run through list and stop at end or when we alphabetically beat an item
		while (cursor->next && strcasecmp(newitem->string, cursor->next->string) > 0)
			cursor = cursor->next;

		// cursor is prior item, cursor->next is item ahead
		// apples [frogs] monkeys
		newitem->next		= cursor->next; // Set my next to monkeys
		newitem->prev		= cursor;		// Set my previous to apples (I have to have a previous since I'm not the head)
		cursor->next		= newitem;		// Set one behind me (apples) to have a next of me (frogs)

		if (newitem->next)					// If I have a next ...
			newitem->next->prev	= newitem;	// Set my next's (monkeys) previous to me (frogs)
	}
	return newitem;
}


ezSortList* ezSortList_Add_With_ItemData (ezSortList** alphaboss, const char* string, const char* itemdata)
{
	ezSortList* cursor   = *alphaboss;
	ezSortList* newitem = Memory_calloc (1, sizeof(ezSortList), "ezSortList Add");
	newitem->string = Memory_strdup (string);
	if (itemdata)
		newitem->itemdata = Memory_strdup (itemdata);

	// Now link it in ...

	// Update alphabetical link list.  First check to see if we are top item
	if (*alphaboss == NULL || strcasecmp (newitem->string, (*alphaboss)->string) < 0 )
	{
		newitem->next = *alphaboss;
		if (newitem->next)					// If I have a next
			newitem->next->prev = newitem;	// It's next is now me

		*alphaboss = newitem;
	}
	else
	{
		// Not top ... run through list and stop at end or when we alphabetically beat an item
		while (cursor->next && strcasecmp(newitem->string, cursor->next->string) > 0)
			cursor = cursor->next;

		// cursor is prior item, cursor->next is item ahead
		// apples [frogs] monkeys
		newitem->next		= cursor->next; // Set my next to monkeys
		newitem->prev		= cursor;		// Set my previous to apples (I have to have a previous since I'm not the head)
		cursor->next		= newitem;		// Set one behind me (apples) to have a next of me (frogs)

		if (newitem->next)					// If I have a next ...
			newitem->next->prev	= newitem;	// Set my next's (monkeys) previous to me (frogs)
	}
	return newitem;
}





// Delete the first item
void ezSortList_Remove_First (ezSortList** alphaboss)
{
	if (*alphaboss == NULL)
	{
		Sys_Error ("Can't remove from a NULL list");
		// So nothing to delete
	}
	else
	{
		ezSortList * newhead = (*alphaboss)->next;
//		NSLogc ("Removing %s", (*alphaboss)->string);
		if ((*alphaboss)->itemdata)
			Memory_free ((void*)(*alphaboss)->itemdata);

		Memory_free ((void*)(*alphaboss)->string);
		Memory_free ((void*)*alphaboss);
		*alphaboss = newhead;
	}
}

void ezSortList_Destroy (ezSortList** alphaboss)
{
	while (*alphaboss)
		ezSortList_Remove_First (alphaboss);
}


// Scan forward X items.  Stops at end if we hit that.
ezSortList* ezSortList_Jump_X (ezSortList* startposition, unsigned int num_ahead)
{
	ezSortList*cursor;
	int j;

	SortListCursorForwardStopAtEnd(j, cursor, startposition, num_ahead);
	return cursor;
}

/*
const char* ezSortList_String_For_X_Ahead (ezSortList* startposition, unsigned int num_ahead)
{
	ezSortList*cursor = ezSortList_Item_X_Ahead(startposition, num_ahead) ;
	int j;
	// Scan forward to get last name.  This won't return NULL
	SortListCursorForwardStopAtEnd(j, cursor, item, PAGE_SIZE_25 - 1);
}
*/


