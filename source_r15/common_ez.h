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

#ifndef __COMMON_EZ_H
#define __COMMON_EZ_H




// Easier dynamic strings.
typedef const char ezString;


// Easier dynamic lists
typedef struct ezSortList_s
{
	struct ezSortList_s	*next;
	struct ezSortList_s	*prev;

	ezString*			string;
	ezString*			itemdata;	// Default

	void				*data;
} ezSortList;

#define ForEachSortList(_j, _cursor, _list) for (_j = 0, _cursor = _list; _cursor; _j++, _cursor = _cursor->next)

#define SortListCursorForwardStopAtEnd(_j, _cursor, _start, _count) for ((_cursor) = (_start), (_j) = 0; (_j) < (int)(_count) && (_cursor)->next; (_j)++, (_cursor) = (_cursor)->next)
// This will move forward X items or stop prematurely at last non-null item



ezSortList* ezSortList_Add (ezSortList** alphaboss, const char* string);
ezSortList* ezSortList_Add_With_ItemData (ezSortList** alphaboss, const char* string, const char* itemdata);
ezSortList* ezLinkEnd_Add_With_ItemData (ezSortList** chaintop, ezSortList* tail, const char* string, const char* itemdata);
// This one we add "down"






#endif	/* __COMMON_EZ_H */
