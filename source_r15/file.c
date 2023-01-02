/*
Copyright (C) 2013 Baker

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// file.c -- file functions

#include "quakedef.h"

qboolean File_Exists (const char *path)
{
	return Sys_FileExists (path);

}

qboolean File_Is_Folder (const char* path)
{
	return System_File_Is_Folder (path);
}

typedef char path1024[1024];

typedef struct
{
	qboolean	isopen;
	void*		addy;
	path1024	filename;
} fs_handles_t;
#define	MAX_HANDLES		100
typedef struct
{
	int				first_slot;
	int				num_open;
	fs_handles_t	files[MAX_HANDLES];
} fs_mgr_t;

fs_mgr_t file_mgr;

void FS_RegisterClose (FILE* f)
{
	int i;
	for (i = 0; i < MAX_HANDLES; i ++)
	{
		fs_handles_t* cur = &file_mgr.files[i];
		if (cur->addy == f)
		{
			file_mgr.num_open --;
			Con_DPrintf ("FILECLOSE: %s found and closed\n", cur->filename);
			//Con_Printf ("Files open is %d\n", file_mgr.num_open);
			cur->filename[0] = 0;
			cur->addy = NULL;
			return;
		} 
		
			
	}
	Con_Printf ("Couldn't reconcile closed file (%p)\n", f);
}


void FS_RegisterOpen (const char* filename, FILE* f)
{
	int i;
	for (i = 0; i < MAX_HANDLES; i ++)
	{
		fs_handles_t* cur = &file_mgr.files[i];
		if (cur->filename[0] == 0)
		{
			q_snprintf (cur->filename, sizeof(cur->filename), "%s", filename);
			cur->addy = f;
			file_mgr.num_open ++;

			Con_DPrintf ("FS_fopen: %p -> %s\n", cur->addy, cur->filename);
			
			if (file_mgr.num_open > 25)
			{
				Con_Warning ("Over 25 files open!\n");
				Files_List_f ();

			}
			
			return;
		}
	}
}

void Files_List_f (void)
{
	int i;
	Con_SafePrintf ("Open files:\n");
	for (i = 0; i < MAX_HANDLES; i ++)
	{
		fs_handles_t* cur = &file_mgr.files[i];
		if (cur->addy)
			Con_SafePrintf ("%i: %p %s\n", i, cur->addy, cur->filename);
	}
	Con_SafePrintf ("\n");
}





FILE *FS_fopen_write(const char *filename, const char *mode/*, qboolean bCreatePath*/)
{
	FILE *f;
#if 0
	// SECURITY: Compare all file writes to com_basedir
	if (strlen(filename) < strlen(com_basedir) || strncasecmp (filename, com_basedir, strlen(com_basedir))!=0 || strstr(filename, "..") )
		Sys_Error ("Security violation:  Attempted path is %s\n\nWrite access is limited to %s folder tree.", filename, com_basedir);
#endif
	f = fopen (filename, mode);		// fopen OK

#if 0
	if (!f && bCreatePath == FS_CREATE_PATH && strstr(mode,"w"))
	{
		// If specified, on failure to open file for write, create the path
		COM_CreatePath (filename);
		f = fopen (filename, mode); // fopen OK
	}
#endif
	if (f)
	{
//		Con_Printf ("FOPEN_WRITE: File open for write: '%s' (%s) %s\n", filename, mode, f ? "" : "(couldn't find file)");
		FS_RegisterOpen (filename, f);
	}
		

	return f;
}


FILE *FS_fopen_write_create_path (const char *filename, const char *mode)
{
	
#if 0
	// SECURITY: Compare all file writes to com_basedir
	if (strlen(filename) < strlen(com_basedir) || strncasecmp (filename, com_basedir, strlen(com_basedir))!=0 || strstr(filename, "..") )
		Sys_Error ("Security violation:  Attempted path is %s\n\nWrite access is limited to %s folder tree.", filename, com_basedir);
#endif
	FILE *f = fopen (filename, mode);		// fopen OK

#if 1
	if (!f)
	{
		// If specified, on failure to open file for write, create the path
		COM_CreatePath ((char*)filename);
		f = fopen (filename, mode); // fopen OK
	}
#endif
	if (f)
	{
//		Con_Printf ("FOPEN_WRITE: File open for write: '%s' (%s) %s\n", filename, mode, f ? "" : "(couldn't find file)");
		FS_RegisterOpen (filename, f);
	}
		

	return f;
}

FILE *FS_fopen_read(const char *filename, const char *mode)
{
	FILE *f = fopen (filename, mode); // fopen OK
	
	if (f)
	{
//		Con_Printf ("FOPEN_READ: File open for read: '%s' (%s) %s\n", filename, mode, f ? "" : "(couldn't find file)");
		FS_RegisterOpen (filename, f);
	}
		
	return f;
}


void FS_fclose (FILE* myfile)
{
	FS_RegisterClose (myfile);
	fclose (myfile);
}


qboolean File_Memory_To_File (const char* fileToWrite, byte* data, size_t numBytes)
{
	FILE*	fout	= fopen (fileToWrite, "wb");

	if (!fout)											return false;
	if ( fwrite (data, numBytes, 1, fout) != 1)			return false;

	fclose (fout);

	return true;
}