/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2012 John Fitzgibbons and others

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
// sys_win.c -- Win32 system interface code

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"
#include "conproc.h"
#include <limits.h>
#include <errno.h>

#define MINIMUM_WIN_MEMORY		0x0880000
#define MAXIMUM_WIN_MEMORY		0x8000000 // R8 Increased to 128 MB just to be safe.  Necros, for example, goes crazy with files.
//#define MAXIMUM_WIN_MEMORY		0x4000000 //Baker: 64 MB.  Can it handle ARWOP?

#define CONSOLE_ERROR_TIMEOUT	60.0	// # of seconds to wait on Sys_Error running
										//  dedicated before exiting
#define PAUSE_SLEEP		50				// sleep time on pause or minimization
#define NOT_FOCUS_SLEEP	20				// sleep time when not focus

int			starttime;
qboolean	WinNT;

static double		pfreq;
static double		curtime = 0.0;
static double		lastcurtime = 0.0;
static int			lowshift;
qboolean			isDedicated;
static qboolean		sc_return_on_enter = false;
HANDLE				hinput, houtput;

static char			*tracking_tag = "Clams & Mooses";

static HANDLE	tevent;
static HANDLE	hFile;
static HANDLE	heventParent;
static HANDLE	heventChild;

void Sys_InitFloatTime (void);

volatile int					sys_checksum;


/*
================
Sys_PageIn
================
*/
void Sys_PageIn (void *ptr, int size)
{
	byte	*x;
	int		m, n;

// touch all the memory to make sure it's there. The 16-page skip is to
// keep Win 95 from thinking we're trying to page ourselves in (we are
// doing that, of course, but there's no reason we shouldn't)
	x = (byte *)ptr;

	for (n=0 ; n<4 ; n++)
	{
		for (m=0 ; m<(size - 16 * 0x1000) ; m += 4)
		{
			sys_checksum += *(int *)&x[m];
			sys_checksum += *(int *)&x[m + 16 * 0x1000];
		}
	}
}


/*
===============================================================================

FILE IO

===============================================================================
*/

#define	MAX_HANDLES		100 //johnfitz -- was 10
FILE	*sys_handles[MAX_HANDLES];

int		findhandle (void)
{
	int		i;

	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

/*
================
filelength
================
*/
int Sys_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (const char *path, int *hndl)
{
	FILE	*f;
	int		i, retval;

	i = findhandle ();

	f = FS_fopen_read(path, "rb");

	if (!f)
	{
		*hndl = -1;
		retval = -1;
	}
	else
	{
		sys_handles[i] = f;
		*hndl = i;
		retval = Sys_filelength(f);
	}

	return retval;
}

int Sys_FileOpenWrite (const char *path)
{
	FILE	*f;
	int		i;
//	int		t;

	i = findhandle ();

	f = FS_fopen_write(path, "wb");
	if (!f)
		Sys_Error ("Error opening %s: %s", path,strerror(errno));
	sys_handles[i] = f;

	return i;
}

void Sys_FileClose (int handle)
{
	FS_fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	int	 x;

	x = fread (dest, 1, count, sys_handles[handle]);
	return x;
}

int Sys_FileWrite (int handle, const void *data, int count)
{
	int		x;

	x = fwrite (data, 1, count, sys_handles[handle]);
	return x;
}

int	Sys_FileTime (const char *path)
{
	FILE	*f;
	int		 retval;


	f = FS_fopen_read(path, "rb");

	if (f)
	{
		FS_fclose(f);
		retval = 1;
	}
	else
	{
		retval = -1;
	}

	return retval;
}

#include <direct.h> // Baker: Removes a warning
void Sys_mkdir (const char *path)
{
	_mkdir (path);
}

void Sys_chdir (const char *path)
{
	_chdir (path);
}


qboolean Sys_OpenFolder_HighlightFile (const char *absolutefilename)
{
	char folder_to_open[MAX_OSPATH];
	char file_highlight[MAX_OSPATH];
	char command_line  [1024];
	int i;

	if (Sys_FileTime(absolutefilename) == -1)
	{
		Con_DPrintf ("File \"%s\" does not exist to show\n", absolutefilename);
		Con_Printf ("File does not exist to show\n");
		return false;
	}

	// Copy it
	strcpy (file_highlight, absolutefilename);

	// Windows format the slashes
	for (i = 0; file_highlight[i]; i++)
		if (file_highlight[i] == '/')
			file_highlight[i] = '\\';

	// Get the path
	strcpy (folder_to_open, file_highlight);
	String_Edit_Reduce_To_Parent_Path (folder_to_open);

	sprintf (command_line, "/select,%s", file_highlight);

	// Zero is failure, non-zero is success
	Con_DPrintf ("Folder highlight: explorer.exe with \"%s\"\n", command_line);

	return (ShellExecute(0, "Open", "explorer.exe", command_line, NULL, SW_NORMAL) != 0);

}

void Sys_OpenFolder_Highlight_Binary (void)
{
	char executableAbsoluteName[1024];
	int i;

	i = GetModuleFileNameA (NULL, executableAbsoluteName, sizeof(executableAbsoluteName) - 1 );
	if (i == 0)
	{
		Sys_Error ("Couldn't determine executable directory");
	}
	executableAbsoluteName[i] = 0;  //MSDN: Windows XP:  The string is truncated to nSize characters and is not null-terminated.

	Sys_OpenFolder_HighlightFile (executableAbsoluteName);
}

void Sys_MessageBox (const char* title, const char* message)
{
	MessageBox (NULL, message, title ? title : "Alert" , MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
}

void Sys_Alert (const char* message)
{
	Sys_MessageBox (NULL, message);
}


qboolean Sys_OpenFolder (const char *fullpath)
{
	char folder_to_open[MAX_OSPATH];
	int i;

	// Copy it
	strcpy (folder_to_open, fullpath);

	// Windows format the slashes
	for (i = 0; folder_to_open[i]; i++)
		if (folder_to_open[i] == '/')
			folder_to_open[i] = '\\';

	return (ShellExecute(0, "Open", "explorer.exe", folder_to_open, NULL, SW_NORMAL) != 0);
}



/*
===============================================================================

SYSTEM IO

===============================================================================
*/

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
	DWORD  flOldProtect;

	if (!VirtualProtect((LPVOID)startaddr, length, PAGE_READWRITE, &flOldProtect))
   		Sys_Error("Protection change failed\n");
}

/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

	Sys_InitFloatTime ();

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if ((vinfo.dwMajorVersion < 4) ||
		(vinfo.dwPlatformId == VER_PLATFORM_WIN32s))
	{
		Sys_Error ("WinQuake requires at least Win95 or NT 4.0");
	}

	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		WinNT = true;
	else
		WinNT = false;
}


void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[1024], text2[1024];
	char		*text3 = "Press Enter to exit\n";
	char		*text4 = "***********************************\n";
	char		*text5 = "\n";
	DWORD		dummy;
	double		starttime;
	static int	in_sys_error0 = 0;
	static int	in_sys_error1 = 0;
	static int	in_sys_error2 = 0;
	static int	in_sys_error3 = 0;

	if (!in_sys_error3)
	{
		in_sys_error3 = 1;
	}

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	if (isDedicated)
	{
		va_start (argptr, error);
		vsprintf (text, error, argptr);
		va_end (argptr);

		sprintf (text2, "ERROR: %s\n", text);
		WriteFile (houtput, text5, strlen (text5), &dummy, NULL);
		WriteFile (houtput, text4, strlen (text4), &dummy, NULL);
		WriteFile (houtput, text2, strlen (text2), &dummy, NULL);
		WriteFile (houtput, text3, strlen (text3), &dummy, NULL);
		WriteFile (houtput, text4, strlen (text4), &dummy, NULL);


		starttime = Sys_DoubleTime ();
		sc_return_on_enter = true;	// so Enter will get us out of here

		while (!Sys_ConsoleInput () &&
				((Sys_DoubleTime () - starttime) < CONSOLE_ERROR_TIMEOUT))
		{
		}
	}
	else
	{
	// switch to windowed so the message box is visible, unless we already
	// tried that and failed
		if (!in_sys_error0)
		{
			in_sys_error0 = 1;
			Input_Shutdown ();
			MessageBox(NULL, text, "Quake Error",
					   MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}
		else
		{
			MessageBox(NULL, text, "Double Quake Error",
					   MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}
	}

	if (!in_sys_error1)
	{
		in_sys_error1 = 1;
		Host_Shutdown ();
	}

// shut down QHOST hooks if necessary
	if (!in_sys_error2)
	{
		in_sys_error2 = 1;
		DeinitConProc ();
	}

	exit (1);
}

void Sys_Printf (const char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	DWORD		dummy;

	if (isDedicated)
	{
		va_start (argptr,fmt);
		vsprintf (text, fmt, argptr);
		va_end (argptr);

		WriteFile(houtput, text, strlen (text), &dummy, NULL);
	}
}

void Sys_Quit (void)
{

	Host_Shutdown();

	if (tevent)
		CloseHandle (tevent);

	if (isDedicated)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (0);
}

static	double	pfreq;
static qboolean	hwtimer = false;

void Sys_InitFloatTime (void)
{
	__int64	freq;

	if (!COM_CheckParm("-nohwtimer") && QueryPerformanceFrequency ((LARGE_INTEGER *)&freq) && freq > 0)
	{
		// hardware timer available
		pfreq = (double)freq;
		hwtimer = true;
	}
	else
	{
		// make sure the timer is high precision, otherwise NT gets 18ms resolution
		timeBeginPeriod (1);
	}
}

double Sys_DoubleTime (void)
{
	__int64		pcount;
	static	__int64	startcount;
	static	DWORD	starttime;
	static qboolean	first = true;
	DWORD	now;

	if (hwtimer)
	{
		QueryPerformanceCounter ((LARGE_INTEGER *)&pcount);
		if (first)
		{
			first = false;
			startcount = pcount;
			return 0.0;
		}
		// TODO: check for wrapping
		return (pcount - startcount) / pfreq;
	}

	now = timeGetTime ();

	if (first)
	{
		first = false;
		starttime = now;
		return 0.0;
	}

	if (now < starttime) // wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;
}


// copies given text to clipboard
void Sys_CopyToClipboard(char *text)
{
	char *clipText;
	HGLOBAL hglbCopy;

	if (!OpenClipboard(NULL))
		return;

	if (!EmptyClipboard())
	{
		CloseClipboard();
		return;
	}

	if (!(hglbCopy = GlobalAlloc(GMEM_DDESHARE, strlen(text) + 1)))
	{
		CloseClipboard();
		return;
	}

	if (!(clipText = GlobalLock(hglbCopy)))
	{
		CloseClipboard();
		return;
	}

	strcpy((char *) clipText, text);
	GlobalUnlock(hglbCopy);
	SetClipboardData(CF_TEXT, hglbCopy);

	CloseClipboard();
}

void Sys_Image_BGRA_To_Clipboard (byte *bmbits, int width, int height, int size)
{

	HBITMAP hBitmap= CreateBitmap (width, height, 1, 32 /* bits per pixel is 32 */, bmbits);

	OpenClipboard (NULL);

	if (!EmptyClipboard())
	{
      CloseClipboard();
      return;
	}

	if ((SetClipboardData (CF_BITMAP, hBitmap)) == NULL)
	 Sys_Error ("SetClipboardData failed");

	CloseClipboard ();

}

#ifdef CLIPBOARD_IMAGE // Baker change
	static void sSystem_Clipboard_Set_Image_BGRA (byte* bgradata, int width, int height)
	{
		HBITMAP hBitmap= CreateBitmap (width, height, 1, 32 /* bits per pixel is 32 */, bgradata);

		OpenClipboard (NULL);

		if (!EmptyClipboard())
		{
		  CloseClipboard();
		  return;
		}

		if ((SetClipboardData (CF_BITMAP, hBitmap)) == NULL)
		 Sys_Error ("SetClipboardData failed");

		CloseClipboard ();

	}

	void Image_FlipBuffer (byte *buffer, int columns, int rows, int BytesPerPixel)
	{
		int		bufsize = columns * BytesPerPixel; // bufsize=widthBytes;
		byte*	tb1 = malloc (bufsize); // Flip buffer
		byte*	tb2 = malloc (bufsize); // Flip buffer2
		int		i, offset1, offset2;

		for (i = 0;i < (rows + 1) / 2; i ++)
		{
			offset1 = i * bufsize;
			offset2 = ((rows - 1) - i) * bufsize;

			memcpy(tb1,				buffer + offset1, bufsize);
			memcpy(tb2,				buffer + offset2, bufsize);
			memcpy(buffer+offset1,	tb2,			  bufsize);
			memcpy(buffer+offset2,	tb1,			  bufsize);
		}

		free (tb1);
		free (tb2);
		return;
	}

	static void sSystem_Clipboard_Set_Image_RGBA_Maybe_Flip (const byte* rgbadata, int width, int height, qboolean Flip)
	{
		int		pelscount = width * height;
		int		buffersize = pelscount * 4;
		byte*	bgradata = malloc (buffersize); // Clipboard From RGBA work
		int		i;
		byte	temp;

		memcpy (bgradata, rgbadata, buffersize);

		// If flip ....
		if (Flip)
			Image_FlipBuffer (bgradata, width, height, 4);

		// RGBA to BGRA so clipboard will take it
		for (i = 0 ; i < buffersize ; i += 4)
		{
			temp = bgradata[i];

			bgradata[i] = bgradata[i + 2];
			bgradata[i + 2] = temp;
		}

		sSystem_Clipboard_Set_Image_BGRA (bgradata, width, height);
		free (bgradata);
	}

	qboolean System_Clipboard_Set_Image_RGBA (const byte* rgbadata, int width, int height)
	{
		sSystem_Clipboard_Set_Image_RGBA_Maybe_Flip (rgbadata, width, height, false);
		return true;
	}

	// Was extremely helpful info ... https://sites.google.com/site/michaelleesimons/clipboard
	byte* System_Clipboard_Get_Image_RGBA_Alloc (int* outwidth, int* outheight)
	{
		byte* ptr = NULL;

		if (OpenClipboard(NULL))
		{
			HBITMAP hBitmap = GetClipboardData (CF_BITMAP);
			BITMAP csBitmap;
			if (hBitmap && GetObject(hBitmap, sizeof(csBitmap), &csBitmap) && csBitmap.bmBitsPixel == 32)
			{
				// allocate buffer
				int i, bufsize = csBitmap.bmWidth * csBitmap.bmHeight * (csBitmap.bmBitsPixel / 8);

				csBitmap.bmBits = ptr = malloc (bufsize); // "bmbits buffer"
				GetBitmapBits((HBITMAP)hBitmap, bufsize, csBitmap.bmBits );

				// Convert BGRA --> RGBA, set alpha full since clipboard loses it somehow
				for (i = 0; i < bufsize; i += 4)
				{
					byte temp = ptr[i + 0];
					ptr[i + 0] = ptr[i + 2];
					ptr[i + 2] = temp;
					ptr[i + 3] = 255; // Full alpha
				}
				*outwidth = csBitmap.bmWidth;
				*outheight = csBitmap.bmHeight;
			}
			CloseClipboard ();
		}
		return ptr;
	}


#endif // Baker change +

const char *Sys_ConsoleInput (void)
{
	static char	text[256];
	static int		len;
	INPUT_RECORD	recs[1024];
//	int		count;
	int		ch;
	DWORD	numread, numevents, dummy;

	if (!isDedicated)
		return NULL;


	for ( ;; )
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);

						if (len)
						{
							text[len] = 0;
							len = 0;
							return text;
						}
						else if (sc_return_on_enter)
						{
						// special case to allow exiting from the error handler on Enter
							text[0] = '\r';
							len = 0;
							return text;
						}

						break;

					case '\b':
						WriteFile(houtput, "\b \b", 3, &dummy, NULL);
						if (len)
						{
							len--;
						}
						break;

					default:
						if (ch >= ' ')
						{
							WriteFile(houtput, &ch, 1, &dummy, NULL);
							text[len] = ch;
							len = (len + 1) & 0xff;
						}

						break;

				}
			}
		}
	}

	return NULL;
}

void Sys_Sleep (unsigned long milliseconds)
{
	Sleep (milliseconds);
}


void Sys_SendKeyEvents (void)
{
    MSG        msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
	// we always update if there are any event, even if we're paused
		scr_skipupdate = 0;

		if (!GetMessage (&msg, NULL, 0, 0))
			Sys_Quit ();

      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}
}

#include <sys/types.h>
#include <sys/stat.h>


// Requires at least Windows XP
qboolean System_File_Is_Folder (const char* path)
{
//	#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
	if (GetFileAttributes (path) & FILE_ATTRIBUTE_DIRECTORY)
		return true;
	if (INVALID_FILE_ATTRIBUTES)
		return false;

	return false;
}

qboolean Sys_FileExists (const char *path)
{
	struct _stat buf;
	int	result = _stat(path, &buf);

	if( result != 0 )
      return false;

	return true;
}

char *Sys_ExecutableDirectory (void)
{
	static char executableAbsoluteName[1024];
	int i;

	i = GetModuleFileNameA (NULL, executableAbsoluteName, sizeof(executableAbsoluteName) - 1 );
	if (i == 0)
	{
		Sys_Error ("Couldn't determine executable directory");
		return NULL;
	}
	executableAbsoluteName[i] = 0;  //MSDN: Windows XP:  The string is truncated to nSize characters and is not null-terminated.

	String_Edit_SlashesForward_Like_Unix	(executableAbsoluteName);
	String_Edit_Reduce_To_Parent_Path		(executableAbsoluteName);

	return (char *)executableAbsoluteName;
}

void Sys_WindowCaptionSet (char *newcaption)
{
	if (!wplat.mainwindow)
		return;

	if (!newcaption)
		SetWindowText (wplat.mainwindow, ENGINE_NAME);
	else
		SetWindowText (wplat.mainwindow, newcaption);
}



/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/


/*
==================
WinMain
==================
*/
void SleepUntilInput (int time)
{

	MsgWaitForMultipleObjects(1, &tevent, FALSE, time, QS_ALLINPUT);
}


/*
==================
WinMain
==================
*/


char		*argv[MAX_NUM_ARGVS];
static char	*empty_string = "";


int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
//    MSG				msg;
	quakeparms_t	parms;
	double			time, oldtime, newtime;
	MEMORYSTATUS	lpBuffer;
	static	char	cwd[1024];
	int				t;

    /* previous instances do not exist in Win32 */
    if (hPrevInstance)
        return 0;

	host_parms = &parms;

	wplat.hInstance = hInstance;

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	if (!GetCurrentDirectory (sizeof(cwd), cwd))
		Sys_Error ("Couldn't determine current directory");

	if (cwd[Q_strlen(cwd)-1] == '/')
		cwd[Q_strlen(cwd)-1] = 0;

// Check for a pak0.pak in id1.  If not found, use the executable directory and check.
// If both aren't found, they will eventually hit the gfx.wad unless they are a bizarre modder not using pakfiles.
// Or if the -basedir parameter is being used.  Basedir overrides this stuff later in initialization.

// What is this for?  If a user makes their own shortcut, they may not set the "Start in" folder that says
// what to use for the current working directory ... so it doesn't find Quake.  The user may not know why it
// isn't finding Quake.


	if (File_Exists (va ("%s/id1/pak0.pak", cwd) ) == false)
	{
		// Check executable dir.  Silently fix if executable directory has an id1 folder with pak0.pak
		char* exe_dir = Sys_ExecutableDirectory ();

//		Sys_MessageBox ("CWD", cwd);
		if (File_Exists (va ("%s/id1/pak0.pak", exe_dir) ) == true)
			strcpy (cwd, exe_dir); // Copy exe_dir to cwd
	}


	parms.basedir = cwd;

	parms.argc = 1;
	argv[0] = empty_string;

	while (*lpCmdLine && (parms.argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			if (*lpCmdLine == '\"')
			{
				lpCmdLine++;

				argv[parms.argc] = lpCmdLine;
				parms.argc++;

				while (*lpCmdLine && *lpCmdLine != '\"')
					lpCmdLine++;
			}
			else
			{
				argv[parms.argc] = lpCmdLine;
				parms.argc++;


				while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
					lpCmdLine++;
			}

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}

	parms.argv = argv;

	COM_InitArgv (parms.argc, parms.argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	isDedicated = (COM_CheckParm ("-dedicated") != 0);

#ifdef SUPPORTS_MULTISAMPLE // Baker change
#if 1 //johnfitz -- 0 to supress the 'starting quake' dialog
	if (!isDedicated)
	{
		wplat.hwnd_dialog = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, NULL);

		if (wplat.hwnd_dialog)
		{
			RECT rect;
			GetWindowRect (wplat.hwnd_dialog, &rect);
			WIN_AdjustRectToCenterScreen (&rect);
			SetWindowPos
			(
				wplat.hwnd_dialog,
				NULL,
				rect.left,
				rect.top,
				0,
				0,
				SWP_NOZORDER | SWP_NOSIZE
			);
			ShowWindow (wplat.hwnd_dialog, SW_SHOWDEFAULT);
			UpdateWindow (wplat.hwnd_dialog);
			SetForegroundWindow (wplat.hwnd_dialog);
		}
	}
#endif
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change

// take the greater of all the available memory or half the total memory,
// but at least 8 Mb and no more than 16 Mb, unless they explicitly
// request otherwise
	parms.memsize = lpBuffer.dwAvailPhys;

	if (parms.memsize < MINIMUM_WIN_MEMORY)
		parms.memsize = MINIMUM_WIN_MEMORY;

	if (parms.memsize < (int)(lpBuffer.dwTotalPhys >> 1))
		parms.memsize = lpBuffer.dwTotalPhys >> 1;

	if (parms.memsize > MAXIMUM_WIN_MEMORY)
		parms.memsize = MAXIMUM_WIN_MEMORY;

	if (COM_CheckParm ("-heapsize"))
	{
		t = COM_CheckParm("-heapsize") + 1;

		if (t < com_argc)
			parms.memsize = Q_atoi (com_argv[t]) * 1024;
	}

	parms.membase = malloc (parms.memsize);

	if (!parms.membase)
		Sys_Error ("Not enough memory free; check disk space\n");

#if 0 // Baker: July 8 2014 -- if I recall this function causes problems at times
	Sys_PageIn (parms.membase, parms.memsize);
#endif

	tevent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (!tevent)
		Sys_Error ("Couldn't create event");

	if (isDedicated)
	{
		if (!AllocConsole ())
		{
			Sys_Error ("Couldn't create dedicated server console");
		}

		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);

	// give QHOST a chance to hook into the console
		if ((t = COM_CheckParm ("-HFILE")) > 0)
		{
			if (t < com_argc)
				hFile = (HANDLE)Q_atoi (com_argv[t+1]);
		}

		if ((t = COM_CheckParm ("-HPARENT")) > 0)
		{
			if (t < com_argc)
				heventParent = (HANDLE)Q_atoi (com_argv[t+1]);
		}

		if ((t = COM_CheckParm ("-HCHILD")) > 0)
		{
			if (t < com_argc)
				heventChild = (HANDLE)Q_atoi (com_argv[t+1]);
		}

		InitConProc (hFile, heventParent, heventChild);
	}

	Sys_Init ();

// because sound is off until we become active
	S_BlockSound ();

	Sys_Printf ("Host_Init\n");
	Host_Init ();

	oldtime = Sys_DoubleTime ();

    /* main window message loop */
	while (1)
	{
		if (isDedicated)
		{
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;

			while (time < sys_ticrate.value )
			{
				Sys_Sleep(1);
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
			}
		}
		else
		{
		// yield the CPU for a little while when paused, minimized, or not the focus
			if ((cl.paused && (!vid.ActiveApp )) || vid.Minimized )
			{
				SleepUntilInput (PAUSE_SLEEP);
				scr_skipupdate = 1;		// no point in bothering to draw
			}
			else if (!vid.ActiveApp )
			{
				SleepUntilInput (NOT_FOCUS_SLEEP);
			}

			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
		}

		Host_Frame (time);
		oldtime = newtime;
	}

    /* return success of application */
    return TRUE;
}


#define	SYS_CLIPBOARD_SIZE		256

const char *Sys_GetClipboardTextLine (void)
{
	HANDLE		th;
	const char* src;
	const char* clipText;
	char* dst;
	static	char	clipboard[SYS_CLIPBOARD_SIZE];

	if (!OpenClipboard(NULL))
		return NULL;

	if (!(th = GetClipboardData(CF_TEXT)))
	{
		CloseClipboard ();
		return NULL;
	}

	if (!(clipText = GlobalLock(th)))
	{
		CloseClipboard ();
		return NULL;
	}

	src = clipText;
	dst = clipboard;

	/*
	\e	Write an <escape> character.
	\a	Write a <bell> character.
	\b	Write a <backspace> character.
	\f	Write a <form-feed> character.
	\n	Write a <new-line> character.
	\r	Write a <carriage return> character.
	\t	Write a <tab> character.
	\v	Write a <vertical tab> character.
	\'	Write a <single quote> character.
	\\	Write a backslash character.
	*/

	// Filter out newlines, carriage return and backspace characters
//	while (*s && t - clipboard < SYS_CLIPBOARD_SIZE - 1 && *s != '\n' && *s != '\r' && *s != '\b')

	// Truncate at any new line or carriage return or backspace character
	// BUT convert any whitespace characters that are not actual spaces into spaces.
	while (*src && dst - clipboard < SYS_CLIPBOARD_SIZE - 1 && *src != '\n' && *src != '\r' && *src != '\b')
	{
		if (*src < ' ')
			*dst++ = ' ';
		else *dst++ = *src;
		src++;
	}
	*dst = 0;

	GlobalUnlock (th);
	CloseClipboard ();

	return clipboard;
}

#include "unzip.h"

// This function operates under idea that we chdir'd there.
void Sys_UnZip_File (const char*zipfile)
{

	HZIP hz = OpenZipFile (zipfile,0 );
	ZIPENTRY ze; 
	GetZipItem(hz,-1,&ze); 

	Con_Printf ("Zip list:\n");
	{
		int numitems=ze.index;
		int zi;
		// -1 gives overall information about the zipfile
		for (zi=0; zi<numitems; zi++)
		{ 
			ZIPENTRY ze; GetZipItem(hz,zi,&ze); // fetch individual details
			// The following unzips to the cwd.  
			UnzipItemFile(hz, zi, ze.name);         // e.g. the item's name.
			Con_Printf ("%i:  %s\n", zi, ze.name);
		}
		CloseZip(hz);
	}
}



void Sys_ZipList_f (void)
{
	char buffer[MAX_QPATH];
	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: %s <zipfile> : displays contents of zipfile\n", Cmd_Argv(0));
		return;
	}

	q_snprintf (buffer, sizeof(buffer), "%s/%s", com_gamedir, Cmd_Argv(1));
	{

		//HZIP hz = OpenZipFile ("c:\\quake\\id1\\txqbspbjp.zip",0);
		HZIP hz = OpenZipFile ("c:\\quake\\id1\\dkt1000.zip",0);
		ZIPENTRY ze; 
		GetZipItem(hz,-1,&ze); 

		Con_Printf ("Zip list:\n");
		{
			int numitems=ze.index;
			int zi;
			// -1 gives overall information about the zipfile
			for (zi=0; zi<numitems; zi++)
			{ 
				ZIPENTRY ze; GetZipItem(hz,zi,&ze); // fetch individual details
				// The following unzips to the cwd.  
				UnzipItemFile(hz, zi, ze.name);         // e.g. the item's name.
				Con_Printf ("%i:  %s\n", zi, ze.name);
			}
			CloseZip(hz);
		}
	}
}

void Zip_Init (void)
{
//	Cmd_AddCommand ("showfile", Recent_File_Show_f);
	Cmd_AddCommand ("ziplist", Sys_ZipList_f);
}