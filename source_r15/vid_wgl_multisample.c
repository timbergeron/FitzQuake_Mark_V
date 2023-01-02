

/*========================================================================================

  Name:   ARB_multisample.cpp
	Author: Colt "MainRoach" McAnlis
	Date:   4/29/04
	Desc:   This file contains the context to load a WGL extension from a string
		    As well as collect the sample format available based upon the graphics card.

========================================================================================*/

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>

//#include "arb_multisample.h"
/*====================================
	Name: ARB_multisample.h
	Author: Colt "MainRoach" McAnlis
	Date: 4/29/04
	Desc:
		This file contains our external items

====================================*/

#ifndef __ARB_MULTISAMPLE_H__
#define __ARB_MULTISAMPLE_H__


//#include <gl/glext.h>		//GL extensions

//Globals
extern int	arbMultisampleSupported;
extern int arbMultisampleFormat;

//If you don't want multisampling, set this to 0
#define CHECK_FOR_MULTISAMPLE 1

//to check for our sampling
int WIN_InitMultisample (HINSTANCE hInstance,HWND hWnd,PIXELFORMATDESCRIPTOR pfd, int ask_samples, int* pixelForceFormat);

#endif

// Declairations We'll Use
#define WGL_SAMPLE_BUFFERS_ARB		 0x2041
#define WGL_SAMPLES_ARB			     0x2042

#define false 0
#define true 1

