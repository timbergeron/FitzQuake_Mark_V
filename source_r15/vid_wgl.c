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
// vid_modes.c -- server code for moving users

#include "quakedef.h"
#include "winquake.h"
#include "resource.h" // IDI_ICON2

wplat_t wplat;

//
// miscelleanous init
//

void VID_Local_Window_PreSetup (void)
{
	WNDCLASS		wc;
	wplat.hIcon = LoadIcon (wplat.hInstance, MAKEINTRESOURCE (IDI_ICON2));


	// Register the frame class
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)WIN_MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = wplat.hInstance;
    wc.hIcon         = wplat.hIcon;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = ENGINE_NAME;

    if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

#ifdef SUPPORTS_MULTISAMPLE // Baker change
	if (wplat.hwnd_dialog)
	{
		if (vid_multisample.value)
		{
			// Poke into it for the PFD
			HDC	hdc			= GetDC(wplat.hwnd_dialog);
			int unused		= WIN_SetupPixelFormat (hdc);
			HGLRC wglHRC	= wglCreateContext( hdc );
			HDC wglHDC 		= wglGetCurrentDC();
			int unused2		= wglMakeCurrent( hdc, wglHRC);
			int ask_samples	= (int)vid_multisample.value;

			if (ask_samples != 2 && ask_samples != 4 && ask_samples != 8)
			{
				Con_Warning ("Multisamples requested \"%i\" is invalid, trying 4\n", ask_samples);
				ask_samples = 4;
			}

			// Do it.  We already have desktop properties
			wplat.multisamples = WIN_InitMultisample (wplat.hInstance, wplat.hwnd_dialog, wplat.pfd, ask_samples, &wplat.forcePixelFormat);

			// Your mission is complete.  You may leave now ...		
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(wglHRC);
			ReleaseDC(wplat.hwnd_dialog, wglHDC);
			ReleaseDC(wplat.hwnd_dialog, hdc);

			if (wplat.multisamples)
				Con_Printf ("Multisample x %i Enabled (Requested %i, Received %i).\n", wplat.multisamples, ask_samples, wplat.multisamples);
			else Con_Warning ("Multisample: Requested but not available.\n");
		} else Con_Printf ("Note: Multisample not requested\n");

		// Post teardown
		DestroyWindow (wplat.hwnd_dialog);
		wplat.hwnd_dialog = NULL;
	}
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change	
}

vmode_t VID_Local_GetDesktopProperties (void)
{
	DEVMODE	devmode;
	vmode_t desktop = {0};

	if (!EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &devmode))
	{
		Sys_Error ("VID_UpdateDesktopProperties: EnumDisplaySettings failed\n");
		return desktop;
	}

	desktop.type		=	MODE_FULLSCREEN;
	desktop.width		=	devmode.dmPelsWidth;
	desktop.height		=	devmode.dmPelsHeight;
	desktop.bpp			=	devmode.dmBitsPerPel;
	
	return desktop;
}

//
// vsync
//


qboolean VID_Local_Vsync_Init (const char* gl_extensions_str)
{
	if (strstr(gl_extensions_str, "GL_EXT_swap_control") || strstr(gl_extensions_str, "GL_WIN_swap_hint"))

	{
		wplat.wglSwapIntervalEXT = (SETSWAPFUNC) wglGetProcAddress("wglSwapIntervalEXT");
		wplat.wglGetSwapIntervalEXT = (GETSWAPFUNC) wglGetProcAddress("wglGetSwapIntervalEXT");

		if (wplat.wglSwapIntervalEXT && wplat.wglGetSwapIntervalEXT && wplat.wglSwapIntervalEXT(0) && 
			wplat.wglGetSwapIntervalEXT() != -1)
				return true;
	}
	return false;
}

void VID_Local_Vsync_f (cvar_t *var)
{
	if (renderer.gl_swap_control)
	{
		if (vid_vsync.value)
		{
			if (!wplat.wglSwapIntervalEXT(1))
				Con_Printf ("VID_Vsync_f: failed on wglSwapIntervalEXT\n");
		}
		else
		{
			if (!wplat.wglSwapIntervalEXT(0))
				Con_Printf ("VID_Vsync_f: failed on wglSwapIntervalEXT\n");
		}
	}
}

#ifdef SUPPORTS_MULTISAMPLE // Baker change
void VID_Local_Multisample_f (cvar_t *var)
{
	if (host_initialized)
		Con_Printf ("%s set to \"%s\".  requires engine restart.\n"
				    "Note settings are: 2, 4, 8 and 0\n", var->name, var->string);
}
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change

//
// vid modes
//


void VID_Local_AddFullscreenModes (void)
{

	BOOL		stat;						// Used to test mode validity
	DEVMODE		devmode = {0};
	int			hmodenum = 0;				// Hardware modes start at 0

	// Baker: Run through every display mode and get information
	
	while ( (stat = EnumDisplaySettings (NULL, hmodenum++, &devmode)) && vid.nummodes < MAX_MODE_LIST )
	{
		vmode_t test		= { MODE_FULLSCREEN, devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel };
/* Baker: No more!
		qboolean bpp_ok		= (devmode.dmBitsPerPel >= 16);
*/
		qboolean bpp_ok		= (int)devmode.dmBitsPerPel == vid.desktop.bpp;
		qboolean width_ok	= INBOUNDS (MIN_MODE_WIDTH, devmode.dmPelsWidth, MAX_MODE_WIDTH);
		qboolean height_ok	= INBOUNDS (MIN_MODE_HEIGHT, devmode.dmPelsHeight, MAX_MODE_HEIGHT);
		qboolean qualified	= (bpp_ok && width_ok && height_ok);

		devmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;

		if (qualified && !VID_Mode_Exists(&test, NULL) && ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
		{
			// Not a dup and test = ok ---> add it
			memcpy (&vid.modelist[vid.nummodes++], &test, sizeof(vmode_t) );
//			Con_SafePrintf ("Added %i x %i %i\n", vid.modelist[vid.nummodes-1].width, vid.modelist[vid.nummodes-1].height, vid.modelist[vid.nummodes-1].bpp);
		}
	}
}


void WIN_Construct_Or_Resize_Window (DWORD style, DWORD exstyle, RECT window_rect)
{
	const char* nm = ENGINE_NAME;
	
	int x = window_rect.left, y = window_rect.top;
	int w = RECT_WIDTH(window_rect), h = RECT_HEIGHT(window_rect);

	if (wplat.mainwindow)
	{
		SetWindowLong (wplat.mainwindow, GWL_EXSTYLE, exstyle);
		SetWindowLong (wplat.mainwindow, GWL_STYLE, style);
		SetWindowPos  (wplat.mainwindow, NULL, x, y, w, h, SWP_DRAWFRAME);
		return;
	}
	
	wplat.mainwindow = CreateWindowEx (exstyle, nm, nm, style, x, y, w, h, NULL, NULL, wplat.hInstance, NULL);

	if (!wplat.mainwindow) Sys_Error ("Couldn't create DIB window");
}

void WIN_Change_DisplaySettings (int modenum)
{
	// Change display settings
	wplat.gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	wplat.gdevmode.dmPelsWidth = vid.modelist[modenum].width;
	wplat.gdevmode.dmPelsHeight = vid.modelist[modenum].height;
	wplat.gdevmode.dmBitsPerPel = vid.modelist[modenum].bpp;
	wplat.gdevmode.dmSize = sizeof (DEVMODE);

	if (ChangeDisplaySettings (&wplat.gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		Sys_Error ("Couldn't set fullscreen mode %i x %i @ %i bpp", vid.modelist[modenum].width, vid.modelist[modenum].height, vid.modelist[modenum].bpp);
}

// Returns false if need to do GL setup again.
qboolean VID_Local_SetMode (int modenum)
{
	qboolean reuseok = false;
	RECT client_rect	= {0,0,vid.modelist[modenum].width, vid.modelist[modenum].height};
	RECT window_rect	= client_rect;
	qboolean bordered	= vid.modelist[modenum].type   == MODE_WINDOWED &&
						  (vid.modelist[modenum].width  != vid.desktop.width || 
						  vid.modelist[modenum].height != vid.desktop.height);

	DWORD ExWindowStyle = 0;
	DWORD WindowStyle	= bordered ? DW_BORDERED : DW_BORDERLESS;
	qboolean restart = (wplat.mainwindow != NULL);
	
	// Preserve these for hopeful reuse.
	HDC wglHDC 		= restart ? wglGetCurrentDC() : 0;
	HGLRC wglHRC 	= restart ? wglGetCurrentContext() : 0;

	if (restart)
		VID_Local_Window_Renderer_Teardown (TEARDOWN_NO_DELETE_GL_CONTEXT);
	
	if (vid.modelist[modenum].type == MODE_FULLSCREEN)
		WIN_Change_DisplaySettings (modenum);

	AdjustWindowRectEx (&window_rect, WindowStyle, FALSE, ExWindowStyle);
	WIN_AdjustRectToCenterScreen(&window_rect);


	WIN_Construct_Or_Resize_Window (WindowStyle, ExWindowStyle, window_rect);

	if (vid.modelist[modenum].type == MODE_WINDOWED)
		ChangeDisplaySettings (NULL, 0);

	// clear to black so it isn't empty
	#pragma message ("Baker: Oddly this does not seem to be doing anything now that I have multisample")
	wplat.draw_context = GetDC(wplat.mainwindow);
	PatBlt (wplat.draw_context, 0, 0, vid.modelist[modenum].width,vid.modelist[modenum].height, BLACKNESS);

// Get focus if we can, get foreground, finish setup, pump messages.
// then sleep a little.

	ShowWindow (wplat.mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (wplat.mainwindow);
	SetWindowPos (wplat.mainwindow, HWND_TOP, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOCOPYBITS);
	SetForegroundWindow (wplat.mainwindow);
	{
	    MSG				msg;
		while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
		{
	      	TranslateMessage (&msg);
	      	DispatchMessage (&msg);
		}
	
		Sleep (100);
	}

	WIN_SetupPixelFormat (wplat.draw_context);
	if (wglHRC && (reuseok = wglMakeCurrent (wplat.draw_context, wglHRC)) == 0)
	{
		// Tried to reuse context and it failed
		wglDeleteContext (wglHRC);
		wglHRC = NULL;
		Con_Printf ("Context reuse failed.  Must reload textures.\n");
	}

	if (!wglHRC)
	{
		// Must create a context.
		wglHRC = wglCreateContext( wplat.draw_context );
		if (!wglHRC)
			Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");
		if (!wglMakeCurrent( wplat.draw_context, wglHRC ))
			Sys_Error ("VID_Init: wglMakeCurrent failed");
	}

	return reuseok;
}

//
// in game
//

void VID_Local_SwapBuffers (void)
{
	if (SwapBuffers (wplat.draw_context) == 0)
		MessageBox (NULL, "Swapbuffers failed", "", MB_OK);
}


void VID_Local_Suspend (qboolean bSuspend)
{
	if (bSuspend == false)
	{
		ChangeDisplaySettings (&wplat.gdevmode, CDS_FULLSCREEN);
		ShowWindow(wplat.mainwindow, SW_SHOWNORMAL);
		MoveWindow(wplat.mainwindow, 0, 0, wplat.gdevmode.dmPelsWidth, wplat.gdevmode.dmPelsHeight, false); //johnfitz -- alt-tab fix via Baker
	} else  ChangeDisplaySettings (NULL, 0);
}

//
// window setup
//


BOOL WIN_SetupPixelFormat (HDC hDC)
{
    static PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
	1,						// version number
	PFD_DRAW_TO_WINDOW |	// support window
	PFD_SUPPORT_OPENGL |	// support OpenGL
	PFD_DOUBLEBUFFER,		// double buffered
	PFD_TYPE_RGBA,			// RGBA type
	24,						// 24-bit color depth
	0, 0, 0, 0, 0, 0,		// color bits ignored
	0,						// no alpha buffer
	0,						// shift bit ignored
	0,						// no accumulation buffer
	0, 0, 0, 0, 			// accum bits ignored
	32,						// 32-bit z-buffer
	8,						// 8-bit stencil buffer
	0,						// no auxiliary buffer
	PFD_MAIN_PLANE,			// main layer
	0,						// reserved
	0, 0, 0					// layer masks ignored
    };
    int pixelformat;
	PIXELFORMATDESCRIPTOR  test; //johnfitz

#ifdef SUPPORTS_MULTISAMPLE // Baker change
	if (!wplat.multisamples)
	{
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change
		if ( (pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0 )
		{
			Sys_Error ("Video: ChoosePixelFormat failed");
			return FALSE;
		}
#ifdef SUPPORTS_MULTISAMPLE // Baker change
	} else pixelformat = wplat.forcePixelFormat; // Multisample overrride
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change

	DescribePixelFormat(hDC, pixelformat, sizeof(PIXELFORMATDESCRIPTOR), &test);

    if (SetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
    {
        Sys_Error ("SetPixelFormat failed");
        return FALSE;
    }

#ifdef SUPPORTS_MULTISAMPLE // Baker change
	memcpy (&wplat.pfd, &pfd, sizeof(pfd) );
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change
    return TRUE;
}

void WIN_AdjustRectToCenterScreen (RECT *in_windowrect)
{
	vmode_t desktop = VID_Local_GetDesktopProperties ();
	int nwidth  = in_windowrect->right - in_windowrect->left;
	int nheight = in_windowrect->bottom - in_windowrect->top;

	in_windowrect->left = 0 + (desktop.width - nwidth) / 2;
	in_windowrect->top =  0 + (desktop.height - nheight) / 2;
	in_windowrect->right = in_windowrect->left + nwidth;
	in_windowrect->bottom = in_windowrect->top + nheight;
}

// 
// window teardown
//

void VID_Local_Window_Renderer_Teardown (int destroy)
{
	// destroy = 1 = TEARDOWN_FULL else TEARDOWN_NO_DELETE_GL_CONTEXT (don't destroy the context or destroy window)
	HGLRC hRC = wglGetCurrentContext();
    HDC	  hDC = wglGetCurrentDC();

    wglMakeCurrent(NULL, NULL);

    if (hRC && destroy)		wglDeleteContext(hRC);
	if (hDC)				ReleaseDC(wplat.mainwindow, hDC);

	if (wplat.draw_context)	
	{
		ReleaseDC (wplat.mainwindow, wplat.draw_context);
		wplat.draw_context = NULL;
	}

	if (destroy)
	{
		DestroyWindow (wplat.mainwindow);
		wplat.mainwindow = NULL;
	}

	ChangeDisplaySettings (NULL, 0);	
}

#ifdef SUPPORTS_MULTISAMPLE // Baker change
#include "vid_wglext.h"		//WGL extensions

int	arbMultisampleSupported	= false;
int		arbMultisampleFormat	= 0;

// WGLisExtensionSupported: This Is A Form Of The Extension For WGL
int WGLisExtensionSupported(const char *extension)
{
	const size_t extlen = strlen(extension);
	const char *supported = NULL;
	const char* p;

	// Try To Use wglGetExtensionStringARB On Current DC, If Possible
	PROC wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");

	if (wglGetExtString)
		supported = ((char*(__stdcall*)(HDC))wglGetExtString)(wglGetCurrentDC());

	// If That Failed, Try Standard Opengl Extensions String
	if (supported == NULL)
		supported = (char*)glGetString(GL_EXTENSIONS);

	// If That Failed Too, Must Be No Extensions Supported
	if (supported == NULL)
		return false;

	// Begin Examination At Start Of String, Increment By 1 On False Match
	for (p = supported; ; p++)
	{
		// Advance p Up To The Next Possible Match
		p = strstr(p, extension);

		if (p == NULL)
			return false;															// No Match

		// Make Sure That Match Is At The Start Of The String Or That
		// The Previous Char Is A Space, Or Else We Could Accidentally
		// Match "wglFunkywglExtension" With "wglExtension"

		// Also, Make Sure That The Following Character Is Space Or NULL
		// Or Else "wglExtensionTwo" Might Match "wglExtension"
		if ((p==supported || p[-1]==' ') && (p[extlen]=='\0' || p[extlen]==' '))
			return true;															// Match
	}
}

// InitMultisample: Used To Query The Multisample Frequencies
int WIN_InitMultisample (HINSTANCE hInstance,HWND hWnd,PIXELFORMATDESCRIPTOR pfd, int ask_samples, int* pixelForceFormat)
{  
	 // See If The String Exists In WGL!
	if (!WGLisExtensionSupported("WGL_ARB_multisample"))
	{
		return (arbMultisampleSupported = 0);
	}

	{		
		// Get Our Pixel Format
		PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");	
		if (!wglChoosePixelFormatARB) 
		{
			arbMultisampleSupported=false;
			return false;
		}


		{
			// Get Our Current Device Context
			HDC hDC = GetDC(hWnd);

			int		pixelFormat;
			int		valid;
			UINT	numFormats;
			float	fAttributes[] = {0,0};

			// These Attributes Are The Bits We Want To Test For In Our Sample
			// Everything Is Pretty Standard, The Only One We Want To 
			// Really Focus On Is The SAMPLE BUFFERS ARB And WGL SAMPLES
			// These Two Are Going To Do The Main Testing For Whether Or Not
			// We Support Multisampling On This Hardware.
			int iAttributes[] =
			{
				WGL_DRAW_TO_WINDOW_ARB,GL_TRUE,
				WGL_SUPPORT_OPENGL_ARB,GL_TRUE,
				WGL_ACCELERATION_ARB,WGL_FULL_ACCELERATION_ARB,
				WGL_COLOR_BITS_ARB, 24 /*currentbpp? Nah */, // Baker: Mirror current bpp color depth?
				WGL_ALPHA_BITS_ARB,8,
				WGL_DEPTH_BITS_ARB,16,
				WGL_STENCIL_BITS_ARB,8, // Baker: Stencil bits
				WGL_DOUBLE_BUFFER_ARB,GL_TRUE,
				WGL_SAMPLE_BUFFERS_ARB,GL_TRUE,
				WGL_SAMPLES_ARB, ask_samples /*multisample bits*/,
				0,0
			};

			
			while (ask_samples == 8 || ask_samples == 4 || ask_samples == 2)
			{
				iAttributes[19] = ask_samples;

				// First We Check To See If We Can Get A Pixel Format For 4 Samples
				valid = wglChoosePixelFormatARB(hDC,iAttributes,fAttributes,1,&pixelFormat,&numFormats);

				// If We Returned True, And Our Format Count Is Greater Than 1
				if (valid && numFormats >= 1)
				{				
					*pixelForceFormat = arbMultisampleFormat = pixelFormat;
					return (arbMultisampleSupported = ask_samples);
				}

				ask_samples >>= 1; // Divide by 2
			}
			  
			// Return Fail
			return  (arbMultisampleSupported = 0); 
		}
	}
}
#endif // Baker change + #ifdef SUPPORTS_MULTISAMPLE // Baker change


qboolean VID_Local_IsGammaAvailable (unsigned short* ramps)
{
	if (!GetDeviceGammaRamp (wplat.draw_context, ramps))
		return false;
	
	return true;
}


void VID_Local_Gamma_Set (unsigned short* ramps)
{
	SetDeviceGammaRamp (wplat.draw_context, ramps);
}

int VID_Local_Gamma_Reset (void)
{
	int i;
	HDC hdc = GetDC (NULL);
	WORD gammaramps[3][256];

	for (i = 0;i < 256; i++)
		gammaramps[0][i] = gammaramps[1][i] = gammaramps[2][i] = (i * 65535) / 255;

	i = SetDeviceGammaRamp(hdc, &gammaramps[0][0]);
	ReleaseDC (NULL, hdc);
	
	return !!i;
}

//#endif // SUPPORTS_HARDWARE_GAMMA