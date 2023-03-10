/* Versus Quakespasm: 

1. Ability to load images limiting success using Quakespasm wrong content
   protection.  Except Quakespasm doesn't do this for images as far as I
   know.  Prevents a gaudy replacement texture set from affecting a mod.

*/

/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others

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

#ifndef __GL_IMAGE_H
#define __GL_IMAGE_H

//image.h -- image reading / writing

//be sure to free the hunk after using these loading functions
byte *Image_LoadTGA (FILE *f, int *width, int *height);
byte *Image_LoadPCX (FILE *f, int *width, int *height);
byte *Image_LoadImage (const char *name, int *width, int *height);
byte *Image_LoadImage_Limited (const char *name, int *width, int *height, const char *media_owner_path);

qboolean Image_WriteTGA (const char *name, byte *data, int width, int height, int bpp, qboolean upsidedown);

#endif	/* __GL_IMAGE_H */

