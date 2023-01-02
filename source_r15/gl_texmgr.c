/* This file isn't done!  

Versus Quakespasm:

1. gl_texturemode is a cvar in Quakespasm.

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

//gl_texmgr.c -- fitzquake's texture manager. manages opengl texture images

#include "quakedef.h"

const int	gl_solid_format = 3;
const int	gl_alpha_format = 4;

static cvar_t		gl_texture_anisotropy = {"gl_texture_anisotropy", "1", CVAR_ARCHIVE};
static cvar_t		gl_max_size = {"gl_max_size", "0", CVAR_NONE};
static cvar_t		gl_picmip = {"gl_picmip", "0", CVAR_NONE};
static int			gl_hardware_maxsize;

#define	MAX_GLTEXTURES	2048
static int numgltextures;
static gltexture_t	*active_gltextures, *free_gltextures;
gltexture_t		*notexture, *nulltexture;

unsigned int d_8to24table[256];
unsigned int d_8to24table_fbright[256];
unsigned int d_8to24table_nobright[256];
unsigned int d_8to24table_conchars[256];
unsigned int d_8to24table_shirt[256];
unsigned int d_8to24table_pants[256];
unsigned int d_8to24table_fbright_alpha[256]; // fbright pal but 255 has no alpha

/*
================================================================================

	COMMANDS

================================================================================
*/

typedef struct
{
	int	magfilter;
	int minfilter;
	const char  *name;
} glmode_t;
static glmode_t glmodes[] = {
	{GL_NEAREST, GL_NEAREST,				"GL_NEAREST"},
	{GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST,	"GL_NEAREST_MIPMAP_NEAREST"},
	{GL_NEAREST, GL_NEAREST_MIPMAP_LINEAR,	"GL_NEAREST_MIPMAP_LINEAR"},
	{GL_LINEAR,  GL_LINEAR,					"GL_LINEAR"},
	{GL_LINEAR,  GL_LINEAR_MIPMAP_NEAREST,	"GL_LINEAR_MIPMAP_NEAREST"},
	{GL_LINEAR,  GL_LINEAR_MIPMAP_LINEAR,	"GL_LINEAR_MIPMAP_LINEAR"},
};
#define NUM_GLMODES (int)(sizeof(glmodes)/sizeof(glmodes[0]))
static int glmode_idx = NUM_GLMODES - 1; /* trilinear */

const char* TexMgr_TextureModes_ListExport (void)
{	
	static char returnbuf[32];
	
	static last = -1; // Top of list.
	// We want first entry >= this
	int		wanted = CLAMP(0, last + 1, NUM_GLMODES );  // Baker: bounds check
	
	int i;

	for (i = wanted; i < NUM_GLMODES ; i++)
	{
		
		if (i >= wanted) // Baker: i must be >=want due to way I setup this function
		{
			q_strlcpy (returnbuf, glmodes[i].name, sizeof(returnbuf) );
			String_Edit_To_Lower_Case (returnbuf); // Baker: avoid hassles with uppercase keynames

			last = i;
//			Con_Printf ("Added %s\n", returnbuf);
			return returnbuf;
		}
	}

	// Not found, reset to top of list and return NULL
	last = -1;
	return NULL;
}


/*
===============
TexMgr_DescribeTextureModes_f -- report available texturemodes
===============
*/
static void TexMgr_DescribeTextureModes_f (void)
{
	int i;

	for (i=0; i<NUM_GLMODES; i++)
		Con_SafePrintf ("   %2i: %s\n", i + 1, glmodes[i].name);

	Con_Printf ("%i modes\n", i);
}

/*
===============
TexMgr_SetFilterModes
===============
*/
static void TexMgr_SetFilterModes (gltexture_t *glt)
{
	GL_Bind (glt);

	if (glt->flags & TEXPREF_NEAREST)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else if (glt->flags & TEXPREF_LINEAR)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else if (glt->flags & TEXPREF_MIPMAP)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glmodes[glmode_idx].magfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glmodes[glmode_idx].minfilter);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glmodes[glmode_idx].magfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glmodes[glmode_idx].magfilter);
	}

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_anisotropy.value);
}

/*
===============
TexMgr_TextureMode_f -- called when gl_texturemode changes
===============
*/
static void TexMgr_TextureMode_f (void)
{
	gltexture_t	*glt;
	const char *arg;
	int i;

	switch (Cmd_Argc())
	{
	case 1:
		Con_Printf ("\"gl_texturemode\" is \"%s\"\n", glmodes[glmode_idx].name);
		break;
	case 2:
		arg = Cmd_Argv(1);
		if (arg[0] == 'G' || arg[0] == 'g')
		{
			for (i=0; i<NUM_GLMODES; i++)
				if (!Q_strcasecmp (glmodes[i].name, arg))
				{
					glmode_idx = i;
					goto stuff;
				}
			Con_Printf ("\"%s\" is not a valid texturemode\n", arg);
			return;
		}
		else if (arg[0] >= '0' && arg[0] <= '9')
		{
			i = atoi(arg);
			if (i > NUM_GLMODES || i < 1)
			{
				Con_Printf ("\"%s\" is not a valid texturemode\n", arg);
				return;
			}
			glmode_idx = i - 1;
		}
		else
			Con_Printf ("\"%s\" is not a valid texturemode\n", arg);

stuff:
		for (glt=active_gltextures; glt; glt=glt->next)
			TexMgr_SetFilterModes (glt);

		Sbar_Changed (); //sbar graphics need to be redrawn with new filter mode

		//FIXME: warpimages need to be redrawn, too.

		break;
	default:
		Con_SafePrintf ("usage: gl_texturemode <mode>\n");
		break;
	}
}

/*
===============
TexMgr_Anisotropy_f -- called when gl_texture_anisotropy changes

FIXME: this is getting called twice (becuase of the recursive Cvar_SetValue call)
===============
*/
static void TexMgr_Anisotropy_f (cvar_t *var)
{
	gltexture_t	*glt;

	Cvar_SetValue ("gl_texture_anisotropy", CLAMP (1.0, gl_texture_anisotropy.value, renderer.gl_max_anisotropy));

	for (glt=active_gltextures; glt; glt=glt->next)
		TexMgr_SetFilterModes (glt);
}

/*
===============
TexMgr_Imagelist_f -- report loaded textures
===============
*/
enum {SHOW_ALL, SHOW_UNREPLACED_BSP};
static void TexMgr_Imagelist_Run (int textures_to_show)
{
	float mb;
	float texels = 0;
	gltexture_t	*glt;

	for (glt=active_gltextures; glt; glt=glt->next)
	{
		qboolean isbsp = glt->owner && glt->owner->type == mod_brush;
		qboolean replaced = glt->owner && glt->source_format == SRC_RGBA;
#pragma message ("Message determine if replacement texture here?")
//		switch (textures_to_show)
		
		Con_SafePrintf ("   %4i x%4i %s%s\n", glt->width, glt->height, glt->name, replaced ? " (R)" : "" );
		if (glt->flags & TEXPREF_MIPMAP)
			texels += glt->width * glt->height * 4.0f / 3.0f;
		else
			texels += (glt->width * glt->height);
	}
	mb = texels * (vid.desktop.bpp / 8.0f) / 0x100000;
	Con_Printf ("%i textures %i pixels %1.1f megabytes\n", numgltextures, (int)texels, mb);
}


static void TexMgr_Imagelist_f (void)
{
	TexMgr_Imagelist_Run (SHOW_ALL);
}

/*
===============
TexMgr_Imagedump_f -- dump all current textures to TGA files
===============
*/
static void TexMgr_Imagedump_f (void)
{
	char tganame[MAX_OSPATH], tempname[MAX_OSPATH], dirname[MAX_OSPATH];
	gltexture_t	*glt;
	byte *buffer;
	char *c;

	//create directory
	q_snprintf(dirname, sizeof(dirname), "%s/imagedump", com_gamedir);
	Sys_mkdir (dirname);

	//loop through textures
	for (glt=active_gltextures; glt; glt=glt->next)
	{
		q_strlcpy (tempname, glt->name, sizeof(tempname));
		while ( (c = strchr(tempname, ':')) ) *c = '_';
		while ( (c = strchr(tempname, '/')) ) *c = '_';
		while ( (c = strchr(tempname, '*')) ) *c = '_';
		q_snprintf(tganame, sizeof(tganame), "imagedump/%s.tga", tempname);

		GL_Bind (glt);
		if (glt->flags & TEXPREF_ALPHA)
		{
			buffer = (byte *) malloc(glt->width*glt->height*4);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
			Image_WriteTGA (tganame, buffer, glt->width, glt->height, 32, true);
		}
		else
		{
			buffer = (byte *) malloc(glt->width*glt->height*3);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer);
			Image_WriteTGA (tganame, buffer, glt->width, glt->height, 24, true);
		}
		free (buffer);
	}

	Recent_File_Set_FullPath (dirname);

	Con_Printf ("dumped %i textures to %s\n", numgltextures, dirname);
}

/*
===============
TexMgr_FrameUsage -- report texture memory usage for this frame
===============
*/
float TexMgr_FrameUsage (void)
{
	float mb;
	float texels = 0;
	gltexture_t	*glt;

	for (glt=active_gltextures; glt; glt=glt->next)
	{
		if (glt->visframe == r_framecount)
		{
			if (glt->flags & TEXPREF_MIPMAP)
				texels += glt->width * glt->height * 4.0f / 3.0f;
			else
				texels += (glt->width * glt->height);
		}
	}
	mb = texels * ( vid.desktop.bpp / 8.0f) / 0x100000;
	return mb;
}

/*
================================================================================

	TEXTURE MANAGER

================================================================================
*/

/*
================
TexMgr_FindTexture
================
*/
gltexture_t *TexMgr_FindTexture (qmodel_t *owner, const char *name)
{
	gltexture_t	*glt;

	if (name)
	{
		for (glt=active_gltextures; glt; glt=glt->next)
		{
			if (!strcmp (glt->name, name))
				if (glt->owner == owner)
				return glt;
		}
	}

	return NULL;
}

/*
================
TexMgr_NewTexture
================
*/
gltexture_t *TexMgr_NewTexture (void)
{
	gltexture_t *glt;

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error("numgltextures == MAX_GLTEXTURES\n");

	glt = free_gltextures;
	free_gltextures = glt->next;
	glt->next = active_gltextures;
	active_gltextures = glt;

	glGenTextures(1, &glt->texnum);
	numgltextures++;
	return glt;
}

/*
================
TexMgr_FreeTexture
================
*/
gltexture_t* TexMgr_FreeTexture (gltexture_t *kill)
{
	gltexture_t *glt;

	if (kill == NULL)
	{
		Con_Printf ("TexMgr_FreeTexture: NULL texture\n");
		return NULL;
	}

	if (active_gltextures == kill)
	{
		active_gltextures = kill->next;
		kill->next = free_gltextures;
		free_gltextures = kill;

		glDeleteTextures(1, &kill->texnum);
		numgltextures--;
		return NULL;
	}

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (glt->next == kill)
		{
			glt->next = kill->next;
			kill->next = free_gltextures;
			free_gltextures = kill;

			glDeleteTextures(1, &kill->texnum);
			numgltextures--;
			return NULL;
		}
	}

	Con_Printf ("TexMgr_FreeTexture: not found\n");
	return NULL;
}

/*
================
TexMgr_FreeTextures

compares each bit in "flags" to the one in glt->flags only if that bit is active in "mask"
================
*/
void TexMgr_FreeTextures (unsigned int flags, unsigned int mask)
{
	gltexture_t *glt, *next;

	for (glt = active_gltextures; glt; glt = next)
	{
		next = glt->next;
		if ((glt->flags & mask) == (flags & mask))
			TexMgr_FreeTexture (glt);
	}
}

/*
================
TexMgr_FreeTexturesForOwner
================
*/
void TexMgr_FreeTexturesForOwner (qmodel_t *owner)
{
	gltexture_t *glt, *next;

	for (glt = active_gltextures; glt; glt = next)
	{
		next = glt->next;
		if (glt && glt->owner == owner)
			TexMgr_FreeTexture (glt);
	}
}

/*
================================================================================

	INIT

================================================================================
*/

/*
=================
TexMgr_LoadPalette -- johnfitz -- was VID_SetPalette, moved here, renamed, rewritten
=================
*/
void TexMgr_LoadPalette (void)
{
	byte mask[] = {255,255,255,0};
	byte black[] = {0,0,0,255};

	byte *pal, *src, *dst;
	int i, mark;
	FILE *f;

	COM_FOpenFile ("gfx/palette.lmp", &f);
	if (!f)
		Sys_Error ("Couldn't load gfx/palette.lmp");

	mark = Hunk_LowMark ();
	pal = (byte *) Hunk_Alloc (PALETTE_SIZE_768);
	fread (pal, 1, PALETTE_SIZE_768, f);
	FS_fclose(f);

	//standard palette, 255 is transparent
	dst = (byte *)d_8to24table;
	src = pal;
	for (i=0; i<256; i++)
	{
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = 255;
	}
	d_8to24table[255] &= *(int *)mask;

	//fullbright palette, 0-223 are black (for additive blending)
	src = pal + 224*3;
	dst = (byte *)(d_8to24table_fbright) + 224*4;
	for (i=224; i<256; i++)
	{
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = 255;
	}
	for (i=0; i<224; i++)
		d_8to24table_fbright[i] = *(int *)black;

	//nobright palette, 224-255 are black (for additive blending)
	dst = (byte *)d_8to24table_nobright;
	src = pal;
	for (i=0; i<256; i++)
	{
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = 255;
	}
	for (i=224; i<256; i++)
		d_8to24table_nobright[i] = *(int *)black;

// Baker: Why do this?  Here is why ...
// 255 is mask color for alpha textures (prefixed "{")
// Now even though 255 pink is a silly color to use in a texture
// no one says you can't do it.  So we need to support that.

// Change 1.  We can't have nobright 255 have alpha.  This should be harmless
// since FitzQuake kills the alpha for color 255 in the regular palette
// d_8to24table.
	d_8to24table_nobright[255] = 0; // Alpha of zero.

// Change 2.  If fullbright pink legitimately is intentionally or
// unintentionally in a map texture, we still want to draw it correctly.
// Even though the usage is extremely uncommon (in old GLQuake days a texture
// might convert some pixels in TexMex to this color and mapper might not know.)
// Either way Quake let's this color be in map textures so we must create
// a separate color conversion for alpha intended textures with the single
// exception of making color 255 black with no alpha.

	memcpy (d_8to24table_fbright_alpha, d_8to24table_fbright, sizeof(d_8to24table_fbright_alpha));
	d_8to24table_fbright_alpha[255] = 0; // Black with no alpha.

	//conchars palette, 0 and 255 are transparent
	memcpy(d_8to24table_conchars, d_8to24table, 256*4);
	d_8to24table_conchars[0] &= *(int *)mask;

	Hunk_FreeToLowMark (mark);
}

/*
================
TexMgr_NewGame
================
*/
void TexMgr_NewGame (void)
{
	TexMgr_FreeTextures (0, TEXPREF_PERSIST); //deletes all textures where TEXPREF_PERSIST is unset
	TexMgr_LoadPalette ();
}

/*
=============
TexMgr_RecalcWarpImageSize -- called during init, and after a vid_restart

choose safe warpimage size and resize existing warpimage textures
=============
*/
void TexMgr_RecalcWarpImageSize (void)
{
	int	mark, oldsize;
	gltexture_t *glt;
	byte *dummy;

	//
	// find the new correct size
	//
	oldsize = gl_warpimagesize;

	gl_warpimagesize = TexMgr_SafeTextureSize (512);

	while (gl_warpimagesize > (int)vid.screen.width)
		gl_warpimagesize >>= 1;
	while (gl_warpimagesize > (int)vid.screen.height)
		gl_warpimagesize >>= 1;

	if (gl_warpimagesize == oldsize)
		return;

	//
	// resize the textures in opengl
	//
	mark = Hunk_LowMark();
	dummy = (byte *) Hunk_Alloc (gl_warpimagesize*gl_warpimagesize*4);

	for (glt=active_gltextures; glt; glt=glt->next)
	{
		if (glt->flags & TEXPREF_WARPIMAGE)
		{
			GL_Bind (glt);
			glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, gl_warpimagesize, gl_warpimagesize, 0, GL_RGBA, GL_UNSIGNED_BYTE, dummy);
			glt->width = glt->height = gl_warpimagesize;
		}
	}

	Hunk_FreeToLowMark (mark);
}


cvar_t	gl_greyscale = {"gl_greyscale", "0"};

void TexMgr_GreyScale_f (cvar_t *var)
{
	static float lastgreyscale = 0.0; // Depends on a default of 0

	if (gl_greyscale.value == lastgreyscale)
		return;

	lastgreyscale = gl_greyscale.value;

	TexMgr_ReloadImages ();
	TexMgr_RecalcWarpImageSize ();
}

/*
================
TexMgr_Init

must be called before any texture loading
================
*/
void TexMgr_Init (void)
{
	int i;
	static byte notexture_data[16] = {159,91,83,255,0,0,0,255,0,0,0,255,159,91,83,255}; //black and pink checker
	static byte nulltexture_data[16] = {127,191,255,255,0,0,0,255,0,0,0,255,127,191,255,255}; //black and blue checker
	extern texture_t *r_notexture_mip, *r_notexture_mip2;

	// init texture list
	free_gltextures = (gltexture_t *) Hunk_AllocName (MAX_GLTEXTURES * sizeof(gltexture_t), "gltextures");
	active_gltextures = NULL;
	for (i = 0; i < MAX_GLTEXTURES-1; i ++)
		free_gltextures[i].next = &free_gltextures[i+1];
	free_gltextures[i].next = NULL;
	numgltextures = 0;

	// palette
	TexMgr_LoadPalette ();

	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);
	Cvar_RegisterVariableWithCallback (&gl_texture_anisotropy, TexMgr_Anisotropy_f);

	Cvar_RegisterVariableWithCallback (&gl_greyscale, TexMgr_GreyScale_f);

	Cmd_AddCommand ("gl_texturemode", TexMgr_TextureMode_f);
	Cmd_AddCommand ("gl_describetexturemodes", TexMgr_DescribeTextureModes_f);
	Cmd_AddCommand ("imagelist", TexMgr_Imagelist_f);
	Cmd_AddCommand ("texreload", TexMgr_ReloadImages);
	Cmd_AddCommand ("textures", TexMgr_Imagelist_f); // Baker another way to get to this
	Cmd_AddCommand ("imagedump", TexMgr_Imagedump_f);

	// poll max size from hardware
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &gl_hardware_maxsize);

	// load notexture images
	notexture = TexMgr_LoadImage (NULL, -1, "notexture", 2, 2, SRC_RGBA, notexture_data, "", (src_offset_t)notexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP);
	nulltexture = TexMgr_LoadImage (NULL, -1, "nulltexture", 2, 2, SRC_RGBA, nulltexture_data, "", (src_offset_t)nulltexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP);

	//have to assign these here becuase Mod_Init is called before TexMgr_Init
	r_notexture_mip->gltexture = r_notexture_mip2->gltexture = notexture;

	//set safe size for warpimages
	gl_warpimagesize = 0;
	TexMgr_RecalcWarpImageSize ();
}

/*
================================================================================

	IMAGE LOADING

================================================================================
*/

/*
================
TexMgr_Pad -- return smallest power of two greater than or equal to s
================
*/
int TexMgr_Pad (int s)
{
	int i;
	for (i = 1; i < s; i<<=1)
		;
	return i;
}

/*
===============
TexMgr_SafeTextureSize -- return a size with hardware and user prefs in mind
===============
*/
int TexMgr_SafeTextureSize (int s)
{
	s = TexMgr_Pad(s);
	if ((int)gl_max_size.value > 0)
		s = q_min(TexMgr_Pad((int)gl_max_size.value), s);
	s = q_min(gl_hardware_maxsize, s);
	return s;
}

/*
================
TexMgr_PadConditional -- only pad if a texture of that size would be padded. (used for tex coords)
================
*/
int TexMgr_PadConditional (int s)
{
	if (s < TexMgr_SafeTextureSize(s))
		return TexMgr_Pad(s);
	else
		return s;
}

/*
================
TexMgr_MipMapW
================
*/
static unsigned *TexMgr_MipMapW (unsigned *data, int width, int height)
{
	int		i, size;
	byte	*out, *in;

	out = in = (byte *)data;
	size = (width*height)>>1;

	for (i=0; i<size; i++, out+=4, in+=8)
	{
		out[0] = (in[0] + in[4])>>1;
		out[1] = (in[1] + in[5])>>1;
		out[2] = (in[2] + in[6])>>1;
		out[3] = (in[3] + in[7])>>1;
	}

	return data;
}

/*
================
TexMgr_MipMapH
================
*/
static unsigned *TexMgr_MipMapH (unsigned *data, int width, int height)
{
	int		i, j;
	byte	*out, *in;

	out = in = (byte *)data;
	height>>=1;
	width<<=2;

	for (i=0; i<height; i++, in+=width)
	{
		for (j=0; j<width; j+=4, out+=4, in+=4)
		{
			out[0] = (in[0] + in[width+0])>>1;
			out[1] = (in[1] + in[width+1])>>1;
			out[2] = (in[2] + in[width+2])>>1;
			out[3] = (in[3] + in[width+3])>>1;
		}
	}

	return data;
}


/*
================
TexMgr_ResampleTextureForce
================
*/
static unsigned *TexMgr_ResampleTextureForce (unsigned *in, int inwidth, int inheight, int outwidth, int outheight, qboolean alpha)
{
	byte *nwpx, *nepx, *swpx, *sepx, *dest;
	unsigned xfrac, yfrac, x, y, modx, mody, imodx, imody, injump, outjump;
	unsigned *out;
	int i, j;

	out = Hunk_Alloc(outwidth*outheight*4);

	xfrac = ((inwidth-1) << 16) / (outwidth-1);
	yfrac = ((inheight-1) << 16) / (outheight-1);
	y = outjump = 0;

	for (i=0; i<outheight; i++)
	{
		mody = (y>>8) & 0xFF;
		imody = 256 - mody;
		injump = (y>>16) * inwidth;
		x = 0;

		for (j=0; j<outwidth; j++)
		{
			modx = (x>>8) & 0xFF;
			imodx = 256 - modx;

			nwpx = (byte *)(in + (x>>16) + injump);
			nepx = nwpx + 4;
			swpx = nwpx + inwidth*4;
			sepx = swpx + 4;

			dest = (byte *)(out + outjump + j);

			dest[0] = (nwpx[0]*imodx*imody + nepx[0]*modx*imody + swpx[0]*imodx*mody + sepx[0]*modx*mody)>>16;
			dest[1] = (nwpx[1]*imodx*imody + nepx[1]*modx*imody + swpx[1]*imodx*mody + sepx[1]*modx*mody)>>16;
			dest[2] = (nwpx[2]*imodx*imody + nepx[2]*modx*imody + swpx[2]*imodx*mody + sepx[2]*modx*mody)>>16;
			if (alpha)
				dest[3] = (nwpx[3]*imodx*imody + nepx[3]*modx*imody + swpx[3]*imodx*mody + sepx[3]*modx*mody)>>16;
			else
				dest[3] = 255;

			x += xfrac;
		}
		outjump += outwidth;
		y += yfrac;
	}

	return out;
}


/*
================
TexMgr_ResampleTexture -- bilinear resample
================
*/
static unsigned *TexMgr_ResampleTexture (unsigned *in, int inwidth, int inheight, qboolean alpha)
{
	byte *nwpx, *nepx, *swpx, *sepx, *dest;
	unsigned xfrac, yfrac, x, y, modx, mody, imodx, imody, injump, outjump;
	unsigned *out;
	int i, j, outwidth, outheight;

	if (inwidth == TexMgr_Pad(inwidth) && inheight == TexMgr_Pad(inheight))
		return in;

	outwidth = TexMgr_Pad(inwidth);
	outheight = TexMgr_Pad(inheight);
	out = (unsigned *)Hunk_Alloc(outwidth*outheight*4);

	xfrac = ((inwidth-1) << 16) / (outwidth-1);
	yfrac = ((inheight-1) << 16) / (outheight-1);
	y = outjump = 0;

	for (i=0; i<outheight; i++)
	{
		mody = (y>>8) & 0xFF;
		imody = 256 - mody;
		injump = (y>>16) * inwidth;
		x = 0;

		for (j=0; j<outwidth; j++)
		{
			modx = (x>>8) & 0xFF;
			imodx = 256 - modx;

			nwpx = (byte *)(in + (x>>16) + injump);
			nepx = nwpx + 4;
			swpx = nwpx + inwidth*4;
			sepx = swpx + 4;

			dest = (byte *)(out + outjump + j);

			dest[0] = (nwpx[0]*imodx*imody + nepx[0]*modx*imody + swpx[0]*imodx*mody + sepx[0]*modx*mody)>>16;
			dest[1] = (nwpx[1]*imodx*imody + nepx[1]*modx*imody + swpx[1]*imodx*mody + sepx[1]*modx*mody)>>16;
			dest[2] = (nwpx[2]*imodx*imody + nepx[2]*modx*imody + swpx[2]*imodx*mody + sepx[2]*modx*mody)>>16;
			if (alpha)
				dest[3] = (nwpx[3]*imodx*imody + nepx[3]*modx*imody + swpx[3]*imodx*mody + sepx[3]*modx*mody)>>16;
			else
				dest[3] = 255;

			x += xfrac;
		}
		outjump += outwidth;
		y += yfrac;
	}

	return out;
}

/*
===============
TexMgr_AlphaEdgeFix

eliminate pink edges on sprites, etc.
operates in place on 32bit data
===============
*/
static void TexMgr_AlphaEdgeFix (byte *data, int width, int height)
{
	int i, j, n = 0, b, c[3] = {0,0,0},
		lastrow, thisrow, nextrow,
		lastpix, thispix, nextpix;
	byte *dest = data;

	for (i=0; i<height; i++)
	{
		lastrow = width * 4 * ((i == 0) ? height-1 : i-1);
		thisrow = width * 4 * i;
		nextrow = width * 4 * ((i == height-1) ? 0 : i+1);

		for (j=0; j<width; j++, dest+=4)
		{
			if (dest[3]) //not transparent
				continue;

			lastpix = 4 * ((j == 0) ? width-1 : j-1);
			thispix = 4 * j;
			nextpix = 4 * ((j == width-1) ? 0 : j+1);

			b = lastrow + lastpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = thisrow + lastpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = nextrow + lastpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = lastrow + thispix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = nextrow + thispix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = lastrow + nextpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = thisrow + nextpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = nextrow + nextpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}

			//average all non-transparent neighbors
			if (n)
			{
				dest[0] = (byte)(c[0]/n);
				dest[1] = (byte)(c[1]/n);
				dest[2] = (byte)(c[2]/n);

				n = c[0] = c[1] = c[2] = 0;
			}
		}
	}
}

/*
===============
TexMgr_PadEdgeFixW -- special case of AlphaEdgeFix for textures that only need it because they were padded

operates in place on 32bit data, and expects unpadded height and width values
===============
*/
static void TexMgr_PadEdgeFixW (byte *data, int width, int height)
{
	byte *src, *dst;
	int i, padw, padh;

	padw = TexMgr_PadConditional(width);
	padh = TexMgr_PadConditional(height);

	//copy last full column to first empty column, leaving alpha byte at zero
	src = data + (width - 1) * 4;
	for (i=0; i<padh; i++)
	{
		src[4] = src[0];
		src[5] = src[1];
		src[6] = src[2];
		src += padw * 4;
	}

	//copy first full column to last empty column, leaving alpha byte at zero
	src = data;
	dst = data + (padw - 1) * 4;
	for (i=0; i<padh; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += padw * 4;
		dst += padw * 4;
	}
}

/*
===============
TexMgr_PadEdgeFixH -- special case of AlphaEdgeFix for textures that only need it because they were padded

operates in place on 32bit data, and expects unpadded height and width values
===============
*/
static void TexMgr_PadEdgeFixH (byte *data, int width, int height)
{
	byte *src, *dst;
	int i, padw, padh;

	padw = TexMgr_PadConditional(width);
	padh = TexMgr_PadConditional(height);

	//copy last full row to first empty row, leaving alpha byte at zero
	dst = data + height * padw * 4;
	src = dst - padw * 4;
	for (i=0; i<padw; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += 4;
		dst += 4;
	}

	//copy first full row to last empty row, leaving alpha byte at zero
	dst = data + (padh - 1) * padw * 4;
	src = data;
	for (i=0; i<padw; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += 4;
		dst += 4;
	}
}

/*
================
TexMgr_8to32
================
*/
unsigned *TexMgr_8to32 (byte *in, int pixels, unsigned int *usepal)
{
	int i;
	unsigned *out, *data;

	out = data = (unsigned *) Hunk_Alloc(pixels*4);

	for (i=0 ; i<pixels ; i++)
		*out++ = usepal[*in++];

	return data;
}

/*
================
TexMgr_PadImageW -- return image with width padded up to power-of-two dimentions
================
*/
static byte *TexMgr_PadImageW (byte *in, int width, int height, byte padbyte)
{
	int i, j, outwidth;
	byte *out, *data;

	if (width == TexMgr_Pad(width))
		return in;

	outwidth = TexMgr_Pad(width);

	out = data = (byte *) Hunk_Alloc(outwidth*height);

	for (i=0; i<height; i++)
	{
		for (j=0; j<width; j++)
			*out++ = *in++;
		for (   ; j<outwidth; j++)
			*out++ = padbyte;
	}

	return data;
}

/*
================
TexMgr_PadImageH -- return image with height padded up to power-of-two dimentions
================
*/
static byte *TexMgr_PadImageH (byte *in, int width, int height, byte padbyte)
{
	int i, srcpix, dstpix;
	byte *data, *out;

	if (height == TexMgr_Pad(height))
		return in;

	srcpix = width * height;
	dstpix = width * TexMgr_Pad(height);

	out = data = (byte *) Hunk_Alloc(dstpix);

	for (i=0; i<srcpix; i++)
		*out++ = *in++;
	for (   ; i<dstpix; i++)
		*out++ = padbyte;

	return data;
}

byte quakePalette [] =
{
      0,   0,   0,
     15,  15,  15,
     31,  31,  31,
     47,  47,  47,
     63,  63,  63,
     75,  75,  75,
     91,  91,  91,
    107, 107, 107,
    123, 123, 123,
    139, 139, 139,
    155, 155, 155,
    171, 171, 171,
    187, 187, 187,
    203, 203, 203,
    219, 219, 219,
    235, 235, 235,
     15,  11,   7,
     23,  15,  11,
     31,  23,  11,
     39,  27,  15,
     47,  35,  19,
     55,  43,  23,
     63,  47,  23,
     75,  55,  27,
     83,  59,  27,
     91,  67,  31,
     99,  75,  31,
    107,  83,  31,
    115,  87,  31,
    123,  95,  35,
    131, 103,  35,
    143, 111,  35,
     11,  11,  15,
     19,  19,  27,
     27,  27,  39,
     39,  39,  51,
     47,  47,  63,
     55,  55,  75,
     63,  63,  87,
     71,  71, 103,
     79,  79, 115,
     91,  91, 127,
     99,  99, 139,
	107, 107, 151,
    115, 115, 163,
    123, 123, 175,
    131, 131, 187,
    139, 139, 203,
      0,   0,   0,
      7,   7,   0,
     11,  11,   0,
     19,  19,   0,
     27,  27,   0,
     35,  35,   0,
     43,  43,   7,
     47,  47,   7,
     55,  55,   7,
     63,  63,   7,
     71,  71,   7,
     75,  75,  11,
     83,  83,  11,
     91,  91,  11,
     99,  99,  11,
    107, 107,  15,
      7,   0,   0,
     15,   0,   0,
     23,   0,   0,
     31,   0,   0,
     39,   0,   0,
     47,   0,   0,
     55,   0,   0,
     63,   0,   0,
     71,   0,   0,
     79,   0,   0,
     87,   0,   0,
     95,   0,   0,
    103,   0,   0,
    111,   0,   0,
    119,   0,   0,
    127,   0,   0,
     19,  19,   0,
     27,  27,   0,
     35,  35,   0,
     47,  43,   0,
     55,  47,   0,
     67,  55,   0,
     75,  59,   7,
     87,  67,   7,
     95,  71,   7,
    107,  75,  11,
    119,  83,  15,
    131,  87,  19,
    139,  91,  19,
    151,  95,  27,
    163,  99,  31,
    175, 103,  35,
     35,  19,   7,
     47,  23,  11,
     59,  31,  15,
     75,  35,  19,
     87,  43,  23,
     99,  47,  31,
    115,  55,  35,
    127,  59,  43,
    143,  67,  51,
    159,  79,  51,
    175,  99,  47,
    191, 119,  47,
    207, 143,  43,
    223, 171,  39,
    239, 203,  31,
    255, 243,  27,
     11,   7,   0,
     27,  19,   0,
     43,  35,  15,
     55,  43,  19,
     71,  51,  27,
     83,  55,  35,
     99,  63,  43,
    111,  71,  51,
    127,  83,  63,
    139,  95,  71,
    155, 107,  83,
    167, 123,  95,
    183, 135, 107,
    195, 147, 123,
    211, 163, 139,
    227, 179, 151,
    171, 139, 163,
    159, 127, 151,
    147, 115, 135,
    139, 103, 123,
    127,  91, 111,
    119,  83,  99,
    107,  75,  87,
     95,  63,  75,
     87,  55,  67,
     75,  47,  55,
     67,  39,  47,
     55,  31,  35,
     43,  23,  27,
     35,  19,  19,
     23,  11,  11,
     15,   7,   7,
    187, 115, 159,
    175, 107, 143,
    163,  95, 131,
    151,  87, 119,
    139,  79, 107,
    127,  75,  95,
    115,  67,  83,
    107,  59,  75,
     95,  51,  63,
     83,  43,  55,
     71,  35,  43,
     59,  31,  35,
     47,  23,  27,
     35,  19,  19,
     23,  11,  11,
     15,   7,   7,
    219, 195, 187,
    203, 179, 167,
    191, 163, 155,
    175, 151, 139,
    163, 135, 123,
    151, 123, 111,
    135, 111,  95,
    123,  99,  83,
    107,  87,  71,
     95,  75,  59,
     83,  63,  51,
     67,  51,  39,
     55,  43,  31,
     39,  31,  23,
     27,  19,  15,
     15,  11,   7,
    111, 131, 123,
    103, 123, 111,
     95, 115, 103,
     87, 107,  95,
     79,  99,  87,
     71,  91,  79,
     63,  83,  71,
     55,  75,  63,
     47,  67,  55,
     43,  59,  47,
     35,  51,  39,
     31,  43,  31,
     23,  35,  23,
     15,  27,  19,
     11,  19,  11,
      7,  11,   7,
    255, 243,  27,
    239, 223,  23,
    219, 203,  19,
    203, 183,  15,
    187, 167,  15,
    171, 151,  11,
    155, 131,   7,
    139, 115,   7,
    123,  99,   7,
    107,  83,   0,
     91,  71,   0,
     75,  55,   0,
     59,  43,   0,
     43,  31,   0,
     27,  15,   0,
     11,   7,   0,
      0,   0, 255,
     11,  11, 239,
     19,  19, 223,
     27,  27, 207,
     35,  35, 191,
     43,  43, 175,
     47,  47, 159,
     47,  47, 143,
     47,  47, 127,
     47,  47, 111,
     47,  47,  95,
     43,  43,  79,
     35,  35,  63,
     27,  27,  47,
     19,  19,  31,
     11,  11,  15,
     43,   0,   0,
     59,   0,   0,
     75,   7,   0,
     95,   7,   0,
    111,  15,   0,
    127,  23,   7,
    147,  31,   7,
    163,  39,  11,
    183,  51,  15,
    195,  75,  27,
    207,  99,  43,
    219, 127,  59,
    227, 151,  79,
    231, 171,  95,
    239, 191, 119,
    247, 211, 139,
    167, 123,  59,
    183, 155,  55,
    199, 195,  55,
    231, 227,  87,
    127, 191, 255,
    171, 231, 255,
    215, 255, 255,
    103,   0,   0,
    139,   0,   0,
    179,   0,   0,
    215,   0,   0,
    255,   0,   0,
    255, 243, 147,
    255, 247, 199,
    255, 255, 255,
    159,  91,  83,
};

#define SQUARED(x) ((x)*(x))
void Pixel_Apply_Best_Palette_Index (byte* red, byte* green, byte *blue, const byte* myPalette, const int* myPaletteNumColors)
{
	int				bestColorIndex		=  -1;
	int				bestColorDistance	=  -1;
	const byte*		currentPaletteIndex	= myPalette;
	int				i;

	for (i = 0; i < *myPaletteNumColors; i++, currentPaletteIndex += 3 )
	{
		const int		redDistance		= *red   - currentPaletteIndex[0];
		const int		greenDistance	= *green - currentPaletteIndex[1];
		const int		blueDistance	= *blue  - currentPaletteIndex[2];
		const int		ColorDistance	= SQUARED(redDistance) + SQUARED(greenDistance) + SQUARED(blueDistance); // Could sqrt this but no reason to do so

		if (bestColorDistance == -1 || ColorDistance < bestColorDistance)
		{
			bestColorDistance	= ColorDistance;
			bestColorIndex		= i;
		}
	}

	*red	= myPalette[bestColorIndex * 3 + 0];
	*green	= myPalette[bestColorIndex * 3 + 1];
	*blue	= myPalette[bestColorIndex * 3 + 2];

}



/*
================
TexMgr_LoadImage32 -- handles 32bit source data
================
*/
static void TexMgr_LoadImage32 (gltexture_t *glt, unsigned *data)
{
	int	internalformat,	miplevel, mipwidth, mipheight, picmip;

	// resample up
	if (!renderer.gl_texture_non_power_of_two)
	{
		data = TexMgr_ResampleTexture (data, glt->width, glt->height, glt->flags & TEXPREF_ALPHA);
		glt->width = TexMgr_Pad(glt->width);
		glt->height = TexMgr_Pad(glt->height);
	}

	// mipmap down
	picmip = (glt->flags & TEXPREF_NOPICMIP) ? 0 : q_max ((int)gl_picmip.value, 0);
	mipwidth = TexMgr_SafeTextureSize (glt->width >> picmip);
	mipheight = TexMgr_SafeTextureSize (glt->height >> picmip);
	while ((int) glt->width > mipwidth)
	{
		TexMgr_MipMapW (data, glt->width, glt->height);
		glt->width >>= 1;
		if (glt->flags & TEXPREF_ALPHA)
			TexMgr_AlphaEdgeFix ((byte *)data, glt->width, glt->height);
	}
	while ((int) glt->height > mipheight)
	{
		TexMgr_MipMapH (data, glt->width, glt->height);
		glt->height >>= 1;
		if (glt->flags & TEXPREF_ALPHA)
			TexMgr_AlphaEdgeFix ((byte *)data, glt->width, glt->height);
	}

	{ // BEGIN BLOCK
		unsigned *myData = data;
		int pixelcount = glt->width * glt->height;
		qboolean madecopy = false;

		if ( (gl_greyscale.value == 2 && glt->source_format == SRC_RGBA) || gl_greyscale.value == 1) 
		{
			int i;

			float favg;
			byte	average;
			myData = malloc (pixelcount * 4);
			memcpy (myData, data, pixelcount * 4);
			madecopy = true;
			//	{
			// !(glt->flags & TEXPREF_PERSIST)
			// Baker: TEXPREF_PERSIST cannot be overwritten, trying to attempt so would be an access violation.
			for (i = 0; i < pixelcount; i++)
			{
				byte *pixel = (byte*)&myData[i];
				if (gl_greyscale.value == 2)
				{
					static const int numNoFullBrightColors = 224;
					Pixel_Apply_Best_Palette_Index (&pixel[0], &pixel[1], &pixel[2], quakePalette, &numNoFullBrightColors);
				} 
				else
				{
					favg = 0.299f * pixel[0] /* red */ + 0.587f * pixel[1] /*green*/ + 0.114f * pixel[2] /*blue*/;
					favg = CLAMP(0, favg, 255);
					average = (byte)(favg + 0.5f);
					pixel[0] = pixel[1] = pixel[2] = average;
//					if (pixel[3] != 255 && pixel[3] !=0)
//						i=i;
				}
			}
		}

		// upload
		GL_Bind (glt);
		internalformat = (glt->flags & TEXPREF_ALPHA) ? gl_alpha_format : gl_solid_format;
		glTexImage2D (GL_TEXTURE_2D, 0, internalformat, glt->width, glt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, myData);

		// upload mipmaps
		if (glt->flags & TEXPREF_MIPMAP)
		{
			mipwidth = glt->width;
			mipheight = glt->height;

			for (miplevel=1; mipwidth > 1 || mipheight > 1; miplevel++)
			{
				if (mipwidth > 1)
				{
					TexMgr_MipMapW (myData, mipwidth, mipheight);
					mipwidth >>= 1;
				}
				if (mipheight > 1)
				{
					TexMgr_MipMapH (myData, mipwidth, mipheight);
					mipheight >>= 1;
				}
				glTexImage2D (GL_TEXTURE_2D, miplevel, internalformat, mipwidth, mipheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, myData);
			}
		}

		if (madecopy)
			free (myData);
	} // END BLOCK

	// set filter modes
	TexMgr_SetFilterModes (glt);
}

/*
================
TexMgr_LoadImage8 -- handles 8bit source data, then passes it to LoadImage32
================
*/
static void TexMgr_LoadImage8 (gltexture_t *glt, byte *data, byte *palette_data)
{
	extern cvar_t gl_fullbrights;
	qboolean padw = false, padh = false;
	byte padbyte;
	unsigned int *usepal;
	int i;
	unsigned int custom_palette [257]; // +1 for black index of 256

#pragma message ("Baker: This must be here for texture reload")
	// HACK HACK HACK -- taken from tomazquake
	if (strstr(glt->name, "shot1sid") && 
		glt->width==32 && glt->height==32 && 
		CRC_Block(data, 1024) == 65393)
	{
		// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
		// They are invisible in software, but look really ugly in GL. So we just copy
		// 32 pixels from the bottom to make it look nice.
		memcpy (data, data + 32*31, 32);
	}

	// detect false alpha cases
	if ((glt->flags  & TEXPREF_ALPHA) && !(glt->flags & TEXPREF_CONCHARS))
	{
		for (i = 0; i < (int) (glt->width * glt->height); i++)
			if (data[i] == 255) //transparent index
				break;
		if (i == (int) (glt->width * glt->height))
			glt->flags -= TEXPREF_ALPHA;
	}

	// choose palette and padbyte
	if (glt->source_format == SRC_INDEXED_WITH_PALETTE)
	{
		int k;
		byte *palette = palette_data;

		for (k = 0; k < 256; k ++, palette += 3 /* RGB is +=3 */)
		{
			byte	red = palette[0];
			byte	green = palette[1];
			byte	blue = palette[2];
			byte	alpha = 255;

			custom_palette[k] = ((unsigned)red + ((unsigned)green << 8) + ((unsigned)blue << 16) + ((unsigned)alpha << 24));
		}

		custom_palette[256] = 0 + ((unsigned)255 << 24); // Solid black with full alpha

		if (glt->flags & TEXPREF_ALPHA)
			custom_palette[255] = 0;

		usepal = custom_palette;

		// Baker: This looks wrong but we are using the last index
		// of "custom_palette" that has 257 entries.  It's ok.
		// This is for a special case of alpha textures  where
		// fullbright pink is acceptable -- which is color 255.
		// so I had to make an entry beyond the normal range.
		// To allow fullbright pink to NOT be masked.  I wish I could
		// remember the specific example where I spotted this either
		// with a map or a monster or a weapon. Baker -1 for bad notes.
		// And even though I vaguely recall, I know this is right
		// because I remember discovering the issue and being very
		// disappointed before I fixed it.
		padbyte = 256;
	}
	else
	if (glt->flags & TEXPREF_FULLBRIGHT)
	{
// If an alpha intended image, use color conversion where 255 has no alpha
		if (glt->flags & TEXPREF_ALPHA)
			usepal = d_8to24table_fbright_alpha;
		else
			usepal = d_8to24table_fbright;
		padbyte = 0;
	}
	else if (glt->flags & TEXPREF_NOBRIGHT && gl_fullbrights.value)
	{
		usepal = d_8to24table_nobright;
		padbyte = 0;
	}
	else if (glt->flags & TEXPREF_CONCHARS)
	{
		usepal = d_8to24table_conchars;
		padbyte = 0;
	}
	else
	{
		usepal = d_8to24table;
		padbyte = 255;
	}

	// pad each dimention, but only if it's not going to be downsampled later
	if (glt->flags & TEXPREF_PAD)
	{
		if ((int) glt->width < TexMgr_SafeTextureSize(glt->width))
		{
			data = TexMgr_PadImageW (data, glt->width, glt->height, padbyte);
			glt->width = TexMgr_Pad(glt->width);
			padw = true;
		}
		if ((int) glt->height < TexMgr_SafeTextureSize(glt->height))
		{
			data = TexMgr_PadImageH (data, glt->width, glt->height, padbyte);
			glt->height = TexMgr_Pad(glt->height);
			padh = true;
		}
	}

	// convert to 32bit
	data = (byte *)TexMgr_8to32(data, glt->width * glt->height, usepal);

	// fix edges
	if (glt->flags & TEXPREF_ALPHA)
		TexMgr_AlphaEdgeFix (data, glt->width, glt->height);
	else
	{
		if (padw)
			TexMgr_PadEdgeFixW (data, glt->source_width, glt->source_height);
		if (padh)
			TexMgr_PadEdgeFixH (data, glt->source_width, glt->source_height);
	}

	// upload it
	TexMgr_LoadImage32 (glt, (unsigned *)data);
}

/*
================
TexMgr_LoadLightmap -- handles lightmap data
================
*/
static void TexMgr_LoadLightmap (gltexture_t *glt, byte *data)
{
	// upload it
	GL_Bind (glt);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, glt->width, glt->height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);

	// set filter modes
	TexMgr_SetFilterModes (glt);
}

/*
================
TexMgr_LoadImage -- the one entry point for loading all textures
================
*/
gltexture_t *TexMgr_LoadImage (qmodel_t *owner, int bsp_texnum, const char *name, int width, int height, enum srcformat format,
							   byte *data, const char *source_file, src_offset_t source_offset, unsigned flags)
{
	return TexMgr_LoadImage_SetPal (owner, bsp_texnum, name, width, height, format, data, NULL, source_file, source_offset, 0, flags);

}


gltexture_t *TexMgr_LoadImage_SetPal (qmodel_t *owner, int bsp_texnum, const char *name, int width, int height, enum srcformat format,
							   byte *data, byte *palette_data, const char *source_file, unsigned source_offset, unsigned source_palette_offset, unsigned flags)
{

	unsigned short crc;
	gltexture_t *glt;
	int mark;

	if (isDedicated)
		return NULL; // Baker: Can this really happen or is this just safety?

	// cache check
	switch (format)
	{
		case SRC_INDEXED_WITH_PALETTE:  // To do: Baker ... CRC the 32 bit data instead ...
			crc = CRC_Block(data, width * height);
			break;
		case SRC_INDEXED:
			crc = CRC_Block(data, width * height);
			break;
		case SRC_LIGHTMAP:
		case SRC_RGBA:
			crc = CRC_Block(data, width * height * 4);
			break;
		default: /* not reachable but avoids compiler warnings */
			crc = 0;
	}
	if ((flags & TEXPREF_OVERWRITE) && (glt = TexMgr_FindTexture (owner, name)))
	{
		if (glt->source_crc == crc)
			return glt;
	}
	else
		glt = TexMgr_NewTexture ();

	// copy data
	glt->owner = owner;
	q_strlcpy (glt->name, name, sizeof(glt->name));
	glt->width = width;
	glt->height = height;
	glt->flags = flags;
	glt->skinnum = 0;
	glt->bsp_texnum = bsp_texnum;
	glt->shirt = -1;
	glt->pants = -1;
	q_strlcpy (glt->source_file, source_file, sizeof(glt->source_file));
	glt->source_offset = source_offset;
	glt->source_palette_offset = source_palette_offset;
//	Con_Printf ("%s source offset %i pal offset %i and delta %i\n", glt->name, source_offset, source_palette_offset, source_palette_offset - source_offset);
	glt->source_format = format;
	glt->source_width = width;
	glt->source_height = height;
	glt->source_crc = crc;

	//upload it
	mark = Hunk_LowMark();

	switch (glt->source_format)
	{
		case SRC_INDEXED:
		case SRC_INDEXED_WITH_PALETTE:
			TexMgr_LoadImage8 (glt, data, palette_data);
			break;
		case SRC_LIGHTMAP:
			TexMgr_LoadLightmap (glt, data);
			break;
		case SRC_RGBA:
			TexMgr_LoadImage32 (glt, (unsigned *)data);
			break;
	}

	Hunk_FreeToLowMark(mark);

	return glt;
}

/*
================================================================================

	COLORMAPPING AND TEXTURE RELOADING

================================================================================
*/

// , const char *cache_filename
void TexMgr_ReplaceImage_RGBA (gltexture_t* texture, byte* data, int width, int height)
{
#if 0
	q_strlcpy (texture->paste_texture_name, cache_filename, sizeof(paste_texture_name) );
	texture->paste_texture_width		= width;
	texture->	paste_texture_height	= height;
	texture->crc = CRC_Block(data, width * height * 4);
#else
	texture->source_format	= SRC_RGBA;
	texture->source_width	= texture->width  = width;
	texture->source_height	= texture->height = height;
	
	texture->source_crc		= CRC_Block(data, width * height * 4);
	
	if (texture->bsp_texnum != -1)
	{
		texture_t* maptexture = texture->owner->textures[texture->bsp_texnum];
#pragma message ("This isn't a fix, we must properly delete it?")
		maptexture->fullbright = NULL; // Nuke fullbright texture
	}
	
#endif

//
// upload it
//
	switch (texture->source_format)
	{
		case SRC_RGBA:
			TexMgr_LoadImage32 (texture, (unsigned *)data);
			break;
		default:
			Con_Printf ("Error\n");
			return;
	}

#pragma message ("Need to turn fullbright to none?")

// Warp texture?
	if (texture->flags & TEXPREF_WARPIMAGE)
		TexMgr_RecalcWarpImageSize ();
}

/*
================
TexMgr_ReloadImage -- reloads a texture, and colormaps it if needed
================
*/
void TexMgr_ReloadImage (gltexture_t *glt, int shirt, int pants)
{
	byte	translation[256];
	byte	*src, *dst, *data = NULL, *translated;
	byte	*palette_data = NULL;
	int		mark, size, i;
	FILE	*f = NULL;

//
// get source data
//
	mark = Hunk_LowMark ();

	if (glt->source_file[0] && glt->source_offset)
	{
		//lump inside file
		FILE	*f = NULL;
		int		datasize;
		COM_FOpenFile (glt->source_file, &f);

		if (!f) 
			goto invalid;

		datasize = glt->source_width * glt->source_height;
		if (glt->source_format == SRC_RGBA)
			datasize *= 4;

		data = Hunk_Alloc (datasize);

		// need SEEK_CUR for PAKs and WADs
		fseek (f, glt->source_offset, SEEK_CUR);
		fread (data, datasize, 1, f);

		if (glt->source_format == SRC_INDEXED_WITH_PALETTE)
		{
			size_t net_offset = glt->source_palette_offset - glt->source_offset - datasize;
			datasize = PALETTE_SIZE_768;
			palette_data = Hunk_Alloc (datasize);
			fseek (f, net_offset, SEEK_CUR);
			fread (palette_data, datasize, 1, f);
		}

		// we must null this so that it doesn't try to fclose twice
		FS_fclose (f);
		f = NULL;
	}
	else if (glt->source_file[0] && !glt->source_offset)
		data = Image_LoadImage (glt->source_file, &glt->source_width, &glt->source_height); //simple file
	else if (!glt->source_file[0] && glt->source_offset)
	{

		data = (byte *) glt->source_offset; //image in memory
		if (glt->source_format == SRC_INDEXED_WITH_PALETTE)
			palette_data = (byte *) glt->source_palette_offset;
	}

	if (!data)
	{
invalid:
		Con_Printf ("TexMgr_ReloadImage: invalid source for %s\n", glt->name);
		Hunk_FreeToLowMark(mark);
		if (f) FS_fclose (f);
		return;
	}

	glt->width = glt->source_width;
	glt->height = glt->source_height;
//
// apply shirt and pants colors
//
// if shirt and pants are -1,-1, use existing shirt and pants colors
// if existing shirt and pants colors are -1,-1, don't bother colormapping
	if (shirt > -1 && pants > -1)
	{
		if (glt->source_format == SRC_INDEXED)
		{
			glt->shirt = shirt;
			glt->pants = pants;
		}
		else Con_Printf ("TexMgr_ReloadImage: can't colormap a non SRC_INDEXED texture: %s\n", glt->name);
	}
	if (glt->shirt > -1 && glt->pants > -1)
	{
		//create new translation table
		for (i=0 ; i<256 ; i++)
			translation[i] = i;

		shirt = glt->shirt * 16;
		if (shirt < 128)
		{
			for (i=0 ; i<16 ; i++)
				translation[TOP_RANGE+i] = shirt+i;
		}
		else
		{
			for (i=0 ; i<16 ; i++)
				translation[TOP_RANGE+i] = shirt+15-i;
		}

		pants = glt->pants * 16;
		if (pants < 128)
		{
			for (i=0 ; i<16 ; i++)
				translation[BOTTOM_RANGE+i] = pants+i;
		}
		else
		{
			for (i=0 ; i<16 ; i++)
				translation[BOTTOM_RANGE+i] = pants+15-i;
		}

		//translate texture
		size = glt->width * glt->height;
		dst = translated = (byte *) Hunk_Alloc (size);
		src = data;

		for (i=0; i<size; i++)
			*dst++ = translation[*src++];

		data = translated;
	}
	// fill the source (8-bit only)
	if ((glt->flags & TEXPREF_FLOODFILL) && glt->source_format == SRC_INDEXED)
	{
		Mod_FloodFillSkin (data, glt->source_width, glt->source_height);
	}
//
// upload it
//
	switch (glt->source_format)
	{
		case SRC_INDEXED:
		case SRC_INDEXED_WITH_PALETTE:
			TexMgr_LoadImage8 (glt, data, palette_data);
			break;
		case SRC_LIGHTMAP:
			TexMgr_LoadLightmap (glt, data);
			break;
		case SRC_RGBA:
			TexMgr_LoadImage32 (glt, (unsigned *)data);
			break;
	}

	Hunk_FreeToLowMark(mark);
	if (f) FS_fclose (f);
}

/*
================
TexMgr_ReloadImages -- reloads all texture images. called only by vid_restart
================
*/
void TexMgr_ReloadImages (void)
{
	gltexture_t *glt;
#pragma message ("Baker remove the conprint of make dprint")
	
	int count;
//	Con_Printf ("TexMgr_ReloadImages:\n");
	for (glt=active_gltextures, count = 0; glt; glt=glt->next, count ++)
	{
		if (count % 1 == 0)
//			Con_Printf ("%i .. ", count);
			Sys_WindowCaptionSet (va("Reload %i: %s ", count, glt->name));

		glGenTextures(1, &glt->texnum);
		TexMgr_ReloadImage (glt, -1, -1);
	}
	Con_Printf ("%s", "\n");
}

/*
================
TexMgr_ReloadNobrightImages -- reloads all texture that were loaded with the nobright palette.  called when gl_fullbrights changes
================
*/
void TexMgr_ReloadNobrightImages (void)
{
	gltexture_t *glt;

	for (glt=active_gltextures; glt; glt=glt->next)
		if (glt->flags & TEXPREF_NOBRIGHT)
			TexMgr_ReloadImage(glt, -1, -1);
}

static byte* TexMgr_Texture_To_RGBA_From_Source_Main (gltexture_t *glt, int *hunkclearmark, int *numbytes)
{
	int				mark = *hunkclearmark = Hunk_LowMark ();

	int				pixelbytes = glt->source_format == SRC_RGBA ? 4 : 1;
	int				datasize = glt->source_width * glt->source_height* pixelbytes;
	
	unsigned int	custom_palette[256];
	unsigned int 	*palette_to_use= d_8to24table; // Default

	byte			*rawdata = NULL;
	FILE			*f = NULL;
	byte			*data = NULL;

//
// get source data
//
#pragma message ("Right now we can't copy a replaced texture correctly because we aren't writing a cache file")

	if (!glt->source_file[0] && glt->source_offset)
	{

		rawdata = (byte *) glt->source_offset; //image in memory
		if (glt->source_format == SRC_INDEXED_WITH_PALETTE)
		{
			int	palette_datasize = PALETTE_SIZE_768;
			int k;
			byte *palette_data = Hunk_Alloc (palette_datasize), *palette;
			palette_data = (byte *) glt->source_palette_offset;

			// And turn it into data
			for (palette = palette_data, k = 0; k < 256; k ++, palette += 3 /* RGB is 3*/)
			{
				byte r = palette[0], g = palette[1], b = palette[2], a = 255;
				custom_palette[k] = ((unsigned)r + ((unsigned)g << 8) + ((unsigned)b << 16) + ((unsigned)a << 24));
			}

			palette_to_use = custom_palette;
		}
	}
	else
	{

		switch (glt->source_offset)
		{
		case 0:
			// simple file.  Which will not have a Quake/Half-Life palette nor be indexed.
			rawdata = Image_LoadImage (glt->source_file, &glt->source_width, &glt->source_height);
			break;
	
		default:
			// Could be anything because it is in a pak or a map or who knows.
			COM_FOpenFile (glt->source_file, &f);
			if (!f) 
				break;
	
			do
			{
				int k;
	
				rawdata = Hunk_Alloc (datasize);
	
				// need SEEK_CUR for PAKs and WADs
				fseek (f, glt->source_offset, SEEK_CUR);
				fread (rawdata, datasize, 1, f);
	
				if (glt->source_format == SRC_INDEXED_WITH_PALETTE)
				{
					// Read palette too
					int net_offset = glt->source_palette_offset - glt->source_offset - datasize;
					int	palette_datasize = PALETTE_SIZE_768;
					byte *palette_data = Hunk_Alloc (palette_datasize), *palette;
	
					fseek (f, net_offset, SEEK_CUR);
					fread (palette_data, palette_datasize, 1, f);
					
					// And turn it into data
					for (palette = palette_data, k = 0; k < 256; k ++, palette += 3 /* RGB is 3*/)
					{
						byte r = palette[0], g = palette[1], b = palette[2], a = 255;
						custom_palette[k] = ((unsigned)r + ((unsigned)g << 8) + ((unsigned)b << 16) + ((unsigned)a << 24));
					}
	
					palette_to_use = custom_palette;
				} 
				// done
			} while (0);

		}
	}

	if (f) FS_fclose (f);

	if (!rawdata)
		return NULL;

	switch (glt->source_format)
	{
		case SRC_INDEXED:
		case SRC_INDEXED_WITH_PALETTE:
			data = (byte*)TexMgr_8to32 (rawdata, glt->source_width * glt->source_height, palette_to_use);	
			break;			

		case SRC_RGBA:
			// Nothing to do, should work as is
			data = rawdata;
			break;
	}

	// We always produce RGBA output which is 4 bytes per pixel
	*numbytes = glt->source_width * glt->source_height * 4;
	return data;
}


byte* TexMgr_Texture_To_RGBA_From_Source (gltexture_t *glt, int *hunkclearmark, int *bytes)
{
	FILE	*f = NULL;
	
	return TexMgr_Texture_To_RGBA_From_Source_Main (glt, hunkclearmark, bytes);
}

qboolean TexMgr_Clipboard_Set (gltexture_t *glt)
{
	qboolean ok = false;
	int mark, bytes;
	byte* data_rgba = TexMgr_Texture_To_RGBA_From_Source (glt, &mark, &bytes);

	if (!data_rgba)
	{
		Con_Printf  ("Couldn't get texture %s source data\n", glt->name);
		return false;
	}

	ok = System_Clipboard_Set_Image_RGBA (data_rgba, glt->source_width, glt->source_height);
	
	Hunk_FreeToLowMark (mark);

	if (ok)
		Con_Printf ("Copied %s to clipboard\n", glt->name);
	else Con_Printf ("Clipboard copy failed\n", glt->name);

	return ok;
}

#if 0

/*
===============
TexMgr_Imagedump_f -- dump all current textures to TGA files
===============
*/
static void TexMgr_Replacement_Save_f (void)
{
	char tganame[MAX_OSPATH], tempname[MAX_OSPATH], dirname[MAX_OSPATH];
	gltexture_t	*glt;
	byte *buffer;
	char *c;

	//create directory
	q_snprintf(dirname, sizeof(dirname), "%s/imagedump", com_gamedir);
	Sys_mkdir (dirname);

	//loop through textures
	for (glt=active_gltextures; glt; glt=glt->next)
	{
		q_strlcpy (tempname, glt->name, sizeof(tempname));
		while ( (c = strchr(tempname, ':')) ) *c = '_';
		while ( (c = strchr(tempname, '/')) ) *c = '_';
		while ( (c = strchr(tempname, '*')) ) *c = '_';
		q_snprintf(tganame, sizeof(tganame), "imagedump/%s.tga", tempname);

		GL_Bind (glt);
		if (glt->flags & TEXPREF_ALPHA)
		{
			buffer = (byte *) malloc(glt->width*glt->height*4);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
			Image_WriteTGA (tganame, buffer, glt->width, glt->height, 32, true);
		}
		else
		{
			buffer = (byte *) malloc(glt->width*glt->height*3);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer);
			Image_WriteTGA (tganame, buffer, glt->width, glt->height, 24, true);
		}
		free (buffer);
	}

	Recent_File_Set_FullPath (va("%s/", dirname)`);

	Con_Printf ("dumped %i textures to %s\n", numgltextures, dirname);
}

static void TexMgr_Replacement_Load_f (void)
{
	char tganame[MAX_OSPATH], tempname[MAX_OSPATH], dirname[MAX_OSPATH];
	gltexture_t	*glt;
	byte *buffer;
	char *c;

	//create directory
	q_snprintf(dirname, sizeof(dirname), "%s/imagedump", com_gamedir);
	Sys_mkdir (dirname);

	//loop through textures
	for (glt=active_gltextures; glt; glt=glt->next)
	{
		q_strlcpy (tempname, glt->name, sizeof(tempname));
		while ( (c = strchr(tempname, ':')) ) *c = '_';
		while ( (c = strchr(tempname, '/')) ) *c = '_';
		while ( (c = strchr(tempname, '*')) ) *c = '_';
		q_snprintf(tganame, sizeof(tganame), "imagedump/%s.tga", tempname);

		GL_Bind (glt);
		if (glt->flags & TEXPREF_ALPHA)
		{
			buffer = (byte *) malloc(glt->width*glt->height*4);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
			Image_WriteTGA (tganame, buffer, glt->width, glt->height, 32, true);
		}
		else
		{
			buffer = (byte *) malloc(glt->width*glt->height*3);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer);
			Image_WriteTGA (tganame, buffer, glt->width, glt->height, 24, true);
		}
		free (buffer);
	}

	Recent_File_Set_FullPath (dirname);

	Con_Printf ("dumped %i textures to %s\n", numgltextures, dirname);
}

#endif



/*
================================================================================

	TEXTURE BINDING / TEXTURE UNIT SWITCHING

================================================================================
*/

int	currenttexture = -1; // to avoid unnecessary texture sets

qboolean mtexenabled = false;

/*
================
GL_SelectTexture -- johnfitz -- rewritten
================
*/
static void GL_SelectTexture (GLenum target)
{
	static GLenum currenttarget;
	static int ct0, ct1;

	if (target == currenttarget)
		return;

	renderer.GL_SelectTextureFunc(target);

	if (target == renderer.TEXTURE0)
	{
		ct1 = currenttexture;
		currenttexture = ct0;
	}
	else //target == TEXTURE1
	{
		ct0 = currenttexture;
		currenttexture = ct1;
	}

	currenttarget = target;
}

/*
================
GL_DisableMultitexture -- selects texture unit 0
================
*/
void GL_DisableMultitexture(void)
{
	if (mtexenabled)
	{
		glDisable(GL_TEXTURE_2D);
		GL_SelectTexture(renderer.TEXTURE0); //johnfitz -- no longer SGIS specific
		mtexenabled = false;
	}
}

/*
================
GL_EnableMultitexture -- selects texture unit 1
================
*/
void GL_EnableMultitexture(void)
{
	if (renderer.gl_mtexable)
	{
		GL_SelectTexture(renderer.TEXTURE1); //johnfitz -- no longer SGIS specific
		glEnable(GL_TEXTURE_2D);
		mtexenabled = true;
	}
}

/*
================
GL_Bind -- johnfitz -- heavy revision
================
*/
void GL_Bind (gltexture_t *texture)
{
	if (!texture)
		texture = nulltexture;

	if ((int)texture->texnum != currenttexture)
	{
		currenttexture = texture->texnum;
		glBindTexture (GL_TEXTURE_2D, currenttexture);
		texture->visframe = r_framecount;
	}
}

