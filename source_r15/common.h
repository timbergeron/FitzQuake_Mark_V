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

#ifndef _Q_COMMON_H
#define _Q_COMMON_H

// comndef.h  -- general definitions

#if defined(_WIN32)
#ifdef _MSC_VER
#  pragma warning(disable:4244)
	/* 'argument'	: conversion from 'type1' to 'type2',
			  possible loss of data */
#  pragma warning(disable:4305)
	/* 'identifier'	: truncation from 'type1' to 'type2' */
	/*  in our case, truncation from 'double' to 'float' */
#  pragma warning(disable:4267)
	/* 'var'	: conversion from 'size_t' to 'type',
			  possible loss of data (/Wp64 warning) */
/* MSC doesn't have fmin() / fmax(), use the min/max macros: */
#define fmax q_max
#define fmin q_min
#endif	/* _MSC_VER */
#endif	/* _WIN32 */

#undef min
#undef max
#define	q_min(a, b)	(((a) < (b)) ? (a) : (b))
#define	q_max(a, b)	(((a) > (b)) ? (a) : (b))
#define	CLAMP(_minval, x, _maxval)		\
	((x) < (_minval) ? (_minval) :		\
	 (x) > (_maxval) ? (_maxval) : (x))

#define INBOUNDS(_low, _test, _high) (_low <= _test && _test <= _high )

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a Sys_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	byte	*data;
	int		maxsize;
	int		cursize;
} sizebuf_t;

void SZ_Alloc (sizebuf_t *buf, int startsize);
void SZ_Free (sizebuf_t *buf);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, const void *data, int length);
void SZ_Print (sizebuf_t *buf, const char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;


void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (int)&(((t *)0)->m)))

//============================================================================

extern	qboolean		host_bigendian;

extern	short	(*BigShort) (short l);
extern	short	(*LittleShort) (short l);
extern	int	(*BigLong) (int l);
extern	int	(*LittleLong) (int l);
extern	float	(*BigFloat) (float l);
extern	float	(*LittleFloat) (float l);

//============================================================================

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, const char *s);
void MSG_WriteCoord (sizebuf_t *sb, float f);
void MSG_WriteAngle (sizebuf_t *sb, float f);
void MSG_WriteAngle16 (sizebuf_t *sb, float f); //johnfitz

extern	int			msg_readcount;
extern	qboolean	msg_badread;		// set if a read goes beyond end of message

void MSG_BeginReading (void);
int MSG_ReadChar (void);
int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
float MSG_ReadFloat (void);
char *MSG_ReadString (void);

float MSG_ReadCoord (void);
float MSG_ReadAngle (void);
float MSG_ReadAngle16 (void); //johnfitz

//============================================================================

void Q_memset (void *dest, int fill, size_t count);
void Q_memcpy (void *dest, const void *src, size_t count);
int Q_memcmp (const void *m1, const void *m2, size_t count);
void Q_strcpy (char *dest, const char *src);
void Q_strncpy (char *dest, const char *src, int count);
int Q_strlen (const char *str);
char *Q_strrchr (const char *s, char c);
void Q_strcat (char *dest, const char *src);
int Q_strcmp (const char *s1, const char *s2);
int Q_strncmp (const char *s1, const char *s2, int count);
int Q_strcasecmp (const char *s1, const char *s2);
int Q_strncasecmp (const char *s1, const char *s2, int n);
int	Q_atoi (const char *str);
float Q_atof (const char *str);


#include "strl_fn.h"

/* snprintf, vsnprintf : always use our versions. */
/* platform dependant (v)snprintf function names: */
#if defined(_WIN32)
#define	snprintf_func		_snprintf
#define	vsnprintf_func		_vsnprintf
#else
#define	snprintf_func		snprintf
#define	vsnprintf_func		vsnprintf
#endif

extern int q_snprintf (char *str, size_t size, const char *format, ...) __attribute__((__format__(__printf__,3,4)));
extern int q_vsnprintf(char *str, size_t size, const char *format, va_list args)
									__attribute__((__format__(__printf__,3,0)));

//============================================================================

extern	char		com_token[1024];
extern	qboolean	com_eof;

const char *COM_Parse (const char *data);


extern	int		com_argc;
extern	char	**com_argv;

extern	int		safemode;
/* safe mode: in true, the engine will behave as if one
   of these arguments were actually on the command line:
   -nosound, -nocdaudio, -nomidi, -stdvid, -dibonly,
   -nomouse, -nojoy, -nolan
 */

int COM_CheckParm (const char *parm);

void COM_Init (void);
void COM_InitArgv (int argc, char **argv);
void COM_InitFilesystem (void);

const char *COM_SkipPath (const char *pathname);
const char *COM_SkipColon (const char *str);
void COM_StripExtension (const char *in, char *out, size_t outsize);
void COM_FileBase (const char *in, char *out, size_t outsize);
void COM_DefaultExtension (char *path, const char *extension, size_t len);
void COM_ForceExtension (char *path, const char *extension, size_t len);
const char *COM_FileGetExtension (const char *in); /* doesn't return NULL */
void COM_ExtractExtension (const char *in, char *out, size_t outsize);
void COM_CreatePath (char *path);

char	*va(const char *format, ...) __attribute__((__format__(__printf__,1,2)));
// does a varargs printf into a temp buffer



//============================================================================

extern int com_filesize;
extern char	com_filepath[MAX_OSPATH];


extern char	com_basedir[MAX_OSPATH];
//extern char	com_modszippath[MAX_OSPATH];
extern	char	com_gamedir[MAX_OSPATH];

void COM_WriteFile (const char *filename, const void *data, int len);
int COM_OpenFile (const char *filename, int *hndl);
int COM_FOpenFile (const char *filename, FILE **file);
void COM_CloseFile (int h);

// these procedures open a file using COM_FindFile and loads it into a proper
// buffer. the buffer is allocated with a total size of com_filesize + 1. the
// procedures differ by their buffer allocation method.

int COM_FOpenFile_Limited (const char *filename, FILE **file, const char *media_owner_path);
byte *COM_LoadHunkFile_Limited (const char *path, const char *media_owner_path);


byte *COM_LoadStackFile (const char *path, void *buffer, int bufsize);
byte *COM_LoadTempFile (const char *path);
byte *COM_LoadHunkFile (const char *path);
byte *COM_LoadMallocFile (const char *path);

extern int static_registered;
extern	struct cvar_s	registered;

extern qboolean		standard_quake, rogue, hipnotic, quoth;

#ifdef SUPPORTS_MP3_MUSIC // Baker change
char *COM_FindFile_NoPak (const char *file_to_find);
#endif // Baker change +

void COM_AddGameDirectory (const char *dir);


void COM_Uppercase_Check (const char* name);

void COM_RemoveAllPaths (void);
qboolean Is_Game_Level (const char* stripped_name);

extern qboolean com_modified;

char *COM_CL_Worldspawn_Value_For_Key (const char *find_keyname);

void swapf (float* a, float* b);

#endif	/* _Q_COMMON_H */
