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
// common.c -- misc functions used in client and server

#include "quakedef.h"
#include <errno.h>

#define NUM_SAFE_ARGVS  7

static char     *largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static char     argvdummy[] = " ";

int		safemode;
static char     *safeargvs[NUM_SAFE_ARGVS] =
	{"-stdvid", "-nolan", "-nosound", "-nocdaudio", "-nojoy", "-nomouse", "-dibonly"};

cvar_t  registered = {"registered","0", CVAR_NONE};
cvar_t  cmdline = {"cmdline","", CVAR_NONE | CVAR_SERVERINFO};

qboolean        com_modified;   // set true if using non-id files


struct searchpath_s	*com_base_searchpaths;	// without id1 and its packs

int             static_registered = 1;  // only for startup check, then set

static void COM_Path_f (void);

// if a packfile directory differs from this, it is assumed to be hacked
#define PAK0_COUNT		339	/* id1/pak0.pak - v1.0x */
#define PAK0_CRC_V100		13900	/* id1/pak0.pak - v1.00 */
#define PAK0_CRC_V101		62751	/* id1/pak0.pak - v1.01 */
#define PAK0_CRC_V106		32981	/* id1/pak0.pak - v1.06 */
#define PAK0_CRC	(PAK0_CRC_V106)
#define PAK0_COUNT_V091		308	/* id1/pak0.pak - v0.91/0.92, not supported */
#define PAK0_CRC_V091		28804	/* id1/pak0.pak - v0.91/0.92, not supported */

char	com_token[1024];
int		com_argc;
char	**com_argv;

#define CMDLINE_LENGTH	256 /* johnfitz -- mirrored in cmd.c */
char	com_cmdline[CMDLINE_LENGTH];

qboolean standard_quake = true, rogue, hipnotic, quoth;

// this graphic needs to be in the pak file to use registered features
static unsigned short pop[] =
{
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x6600,0x0000,0x0000,0x0000,0x6600,0x0000,
	0x0000,0x0066,0x0000,0x0000,0x0000,0x0000,0x0067,0x0000,
	0x0000,0x6665,0x0000,0x0000,0x0000,0x0000,0x0065,0x6600,
	0x0063,0x6561,0x0000,0x0000,0x0000,0x0000,0x0061,0x6563,
	0x0064,0x6561,0x0000,0x0000,0x0000,0x0000,0x0061,0x6564,
	0x0064,0x6564,0x0000,0x6469,0x6969,0x6400,0x0064,0x6564,
	0x0063,0x6568,0x6200,0x0064,0x6864,0x0000,0x6268,0x6563,
	0x0000,0x6567,0x6963,0x0064,0x6764,0x0063,0x6967,0x6500,
	0x0000,0x6266,0x6769,0x6a68,0x6768,0x6a69,0x6766,0x6200,
	0x0000,0x0062,0x6566,0x6666,0x6666,0x6666,0x6562,0x0000,
	0x0000,0x0000,0x0062,0x6364,0x6664,0x6362,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0062,0x6662,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0061,0x6661,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x6500,0x0000,0x0000,0x0000,
	0x0000,0x0000,0x0000,0x0000,0x6400,0x0000,0x0000,0x0000
};

/*

All of Quake's data access is through a hierchal file system, but the contents
of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all
game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.
This can be overridden with the "-basedir" command line parm to allow code
debugging in a different directory.  The base directory is only used during
filesystem initialization.

The "game directory" is the first tree on the search path and directory that all
generated files (savegames, screenshots, demos, config files) will be saved to.
This can be overridden with the "-game" command line parameter.  The game
directory can never be changed while quake is executing.  This is a precacution
against having a malicious server instruct clients to write files over areas they
shouldn't.

The "cache directory" is only used during development to save network bandwidth,
especially over ISDN / T1 lines.  If there is a cache directory specified, when
a file is found by the normal search path, it will be mirrored into the cache
directory, then opened there.

FIXME:
The file "parms.txt" will be read out of the game directory and appended to the
current command line arguments to allow different games to initialize startup
parms differently.  This could be used to add a "-sspeed 22050" for the high
quality sound edition.  Because they are added at the end, they will not
override an explicit setting on the original command line.

*/

//============================================================================


// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter (link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

int q_vsnprintf(char *str, size_t size, const char *format, va_list args)
{
	int		ret;

	ret = vsnprintf_func (str, size, format, args);

	if (ret < 0)
		ret = (int)size;
	if (size == 0)	/* no buffer */
		return ret;
	if ((size_t)ret >= size)
		str[size - 1] = '\0';

	return ret;
}

int q_snprintf (char *str, size_t size, const char *format, ...)
{
	int		ret;
	va_list		argptr;

	va_start (argptr, format);
	ret = q_vsnprintf (str, size, format, argptr);
	va_end (argptr);

	return ret;
}

void Q_memset (void *dest, int fill, size_t count)
{
	size_t             i;

	if ( (((size_t)dest | count) & 3) == 0)
	{
		count >>= 2;
		fill = fill | (fill<<8) | (fill<<16) | (fill<<24);
		for (i=0 ; i<count ; i++)
			((int *)dest)[i] = fill;
	}
	else
		for (i=0 ; i<count ; i++)
			((byte *)dest)[i] = fill;
}

void Q_memcpy (void *dest, const void *src, size_t count)
{
	size_t             i;

	if (( ( (size_t)dest | (size_t)src | count) & 3) == 0 )
	{
		count>>=2;
		for (i=0 ; i<count ; i++)
			((int *)dest)[i] = ((int *)src)[i];
	}
	else
		for (i=0 ; i<count ; i++)
			((byte *)dest)[i] = ((byte *)src)[i];
}

int Q_memcmp (const void *m1, const void *m2, size_t count)
{
	while(count)
	{
		count--;
		if (((byte *)m1)[count] != ((byte *)m2)[count])
			return -1;
	}
	return 0;
}

void Q_strcpy (char *dest, const char *src)
{
	while (*src)
	{
		*dest++ = *src++;
	}
	*dest++ = 0;
}

void Q_strncpy (char *dest, const char *src, int count)
{
	while (*src && count--)
	{
		*dest++ = *src++;
	}
	if (count)
		*dest++ = 0;
}

int Q_strlen (const char *str)
{
	int             count;

	count = 0;
	while (str[count])
		count++;

	return count;
}

char *Q_strrchr(const char *s, char c)
{
    int len = Q_strlen(s);
    s += len;
    while (len--)
	{
	if (*--s == c) 
		return (char *)s;
	}
    return NULL;
}

void Q_strcat (char *dest, const char *src)
{
	dest += Q_strlen(dest);
	Q_strcpy (dest, src);
}

int Q_strcmp (const char *s1, const char *s2)
{
	while (1)
	{
		if (*s1 != *s2)
			return -1;              // strings not equal
		if (!*s1)
			return 0;               // strings are equal
		s1++;
		s2++;
	}

	return -1;
}

int Q_strncmp (const char *s1, const char *s2, int count)
{
	while (1)
	{
		if (!count--)
			return 0;
		if (*s1 != *s2)
			return -1;              // strings not equal
		if (!*s1)
			return 0;               // strings are equal
		s1++;
		s2++;
	}

	return -1;
}

int Q_strncasecmp (const char *s1, const char *s2, int n)
{
	int             c1, c2;

	if (s1 == s2 || n <= 0)
		return 0;

	while (1)
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;               // strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
		}
		if (!c1)
			return (c2 == 0) ? 0 : -1;
		if (c1 < c2)
			return -1;
		if (c1 > c2)
			return 1;
	}
}

int Q_strcasecmp (const char *s1, const char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}

int Q_atoi (const char *str)
{
	int             val;
	int             sign;
	int             c;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;

	val = 0;

//
// check for hex
//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val<<4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val<<4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val<<4) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

//
// check for character
//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}

//
// assume decimal
//
	while (1)
	{
		c = *str++;
		if (c <'0' || c > '9')
			return val*sign;
		val = val*10 + c - '0';
	}

	return 0;
}


float Q_atof (const char *str)
{
	double			val;
	int             sign;
	int             c;
	int             decimal, total;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;

	val = 0;

//
// check for hex
//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val*16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val*16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val*16) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

//
// check for character
//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}

//
// assume decimal
//
	decimal = -1;
	total = 0;
	while (1)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}
		if (c <'0' || c > '9')
			break;
		val = val*10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val*sign;
	while (total > decimal)
	{
		val /= 10;
		total--;
	}

	return val*sign;
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

qboolean        host_bigendian;

short   (*BigShort) (short l);
short   (*LittleShort) (short l);
int     (*BigLong) (int l);
int     (*LittleLong) (int l);
float   (*BigFloat) (float l);
float   (*LittleFloat) (float l);

short   ShortSwap (short l)
{
	byte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

short   ShortNoSwap (short l)
{
	return l;
}

int    LongSwap (int l)
{
	byte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

int     LongNoSwap (int l)
{
	return l;
}

float FloatSwap (float f)
{
	union
	{
		float   f;
		byte    b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap (float f)
{
	return f;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

//
// writing functions
//

void MSG_WriteChar (sizebuf_t *sb, int c)
{
	byte    *buf;

#ifdef PARANOID
	if (c < -128 || c > 127)
		Sys_Error ("MSG_WriteChar: range error");
#endif

	buf = (byte *) SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c)
{
	byte    *buf;

#ifdef PARANOID
	if (c < 0 || c > 255)
		Sys_Error ("MSG_WriteByte: range error");
#endif

	buf = (byte *) SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c)
{
	byte    *buf;

#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Sys_Error ("MSG_WriteShort: range error");
#endif

	buf = (byte *) SZ_GetSpace (sb, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void MSG_WriteLong (sizebuf_t *sb, int c)
{
	byte    *buf;

	buf = (byte *) SZ_GetSpace (sb, 4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

void MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union
	{
		float   f;
		int     l;
	} dat;

	dat.f = f;
	dat.l = LittleLong (dat.l);

	SZ_Write (sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, const char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, Q_strlen(s)+1);
}

//johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
void MSG_WriteCoord16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, Q_rint(f*8));
}

//johnfitz -- 16.8 fixed point coords, max range +-32768
void MSG_WriteCoord24 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, f);
	MSG_WriteByte (sb, (int)(f*255)%255);
}

//johnfitz -- 32-bit float coords
void MSG_WriteCoord32f (sizebuf_t *sb, float f)
{
	MSG_WriteFloat (sb, f);
}

void MSG_WriteCoord (sizebuf_t *sb, float f)
{
	MSG_WriteCoord16 (sb, f);
}

void MSG_WriteAngle (sizebuf_t *sb, float f)
{
	MSG_WriteByte (sb, Q_rint(f * 256.0 / 360.0) & 255); //johnfitz -- use Q_rint instead of (int)
}

//johnfitz -- for PROTOCOL_FITZQUAKE
void MSG_WriteAngle16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, Q_rint(f * 65536.0 / 360.0) & 65535);
}
//johnfitz

//
// reading functions
//
int                     msg_readcount;
qboolean        msg_badread;

void MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar (void)
{
	int     c;

	if (msg_readcount+1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (signed char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadByte (void)
{
	int     c;

	if (msg_readcount+1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (unsigned char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadShort (void)
{
	int     c;

	if (msg_readcount+2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (short)(net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1]<<8));

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong (void)
{
	int     c;

	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1]<<8)
	+ (net_message.data[msg_readcount+2]<<16)
	+ (net_message.data[msg_readcount+3]<<24);

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat (void)
{
	union
	{
		byte    b[4];
		float   f;
		int     l;
	} dat;

	dat.b[0] =      net_message.data[msg_readcount];
	dat.b[1] =      net_message.data[msg_readcount+1];
	dat.b[2] =      net_message.data[msg_readcount+2];
	dat.b[3] =      net_message.data[msg_readcount+3];
	msg_readcount += 4;

	dat.l = LittleLong (dat.l);

	return dat.f;
}

char *MSG_ReadString (void)
{
	static char     string[2048];
	int		c;
	size_t		l;

	l = 0;
	do
	{
		c = MSG_ReadChar ();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

//johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
float MSG_ReadCoord16 (void)
{
	return MSG_ReadShort() * (1.0/8);
}

//johnfitz -- 16.8 fixed point coords, max range +-32768
float MSG_ReadCoord24 (void)
{
	return MSG_ReadShort() + MSG_ReadByte() * (1.0/255);
}

//johnfitz -- 32-bit float coords
float MSG_ReadCoord32f (void)
{
	return MSG_ReadFloat();
}

float MSG_ReadCoord (void)
{
	return MSG_ReadCoord16();
}

float MSG_ReadAngle (void)
{
	return MSG_ReadChar() * (360.0/256);
}

//johnfitz -- for PROTOCOL_FITZQUAKE
float MSG_ReadAngle16 (void)
{
	return MSG_ReadShort() * (360.0 / 65536);
}
//johnfitz

//===========================================================================

void SZ_Alloc (sizebuf_t *buf, int startsize)
{
	if (startsize < 256)
		startsize = 256;
	buf->data = (byte *) Hunk_AllocName (startsize, "sizebuf");
	buf->maxsize = startsize;
	buf->cursize = 0;
}


void SZ_Free (sizebuf_t *buf)
{
//      Z_Free (buf->data);
//      buf->data = NULL;
//      buf->maxsize = 0;
	buf->cursize = 0;
}

void SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
}

void *SZ_GetSpace (sizebuf_t *buf, int length)
{
	void    *data;

	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Sys_Error ("SZ_GetSpace: overflow without allowoverflow set");

		if (length > buf->maxsize)
			Sys_Error ("SZ_GetSpace: %i is > full buffer size", length);

		buf->overflowed = true;
		Con_Printf ("SZ_GetSpace: overflow");
		SZ_Clear (buf);
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write (sizebuf_t *buf, const void *data, int length)
{
	Q_memcpy (SZ_GetSpace(buf,length),data,length);
}

void SZ_Print (sizebuf_t *buf, const char *data)
{
	int             len = Q_strlen(data)+1;

	if (buf->data[buf->cursize-1])
	{	/* no trailing 0 */
		Q_memcpy ((byte *)SZ_GetSpace(buf, len),data,len);
	}
	else
	{	/* write over trailing 0 */
		Q_memcpy ((byte *)SZ_GetSpace(buf, len-1)-1,data,len);
	}
}


//============================================================================

/*
============
COM_SkipPath
============
*/
const char *COM_SkipPath (const char *pathname)
{
	const char    *last;

	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}

const char *COM_SkipColon (const char *str)
{
	const char    *last;

	last = str;
	while (*str)
	{
		if (*str==':')
			last = str+1;
		str++;
	}
	return last;
}


/*
============
COM_StripExtension
============
*/
void COM_StripExtension (const char *in, char *out, size_t outsize)
{
	int	length;

	if (!*in)
	{
		*out = '\0';
		return;
	}
	if (in != out)	/* copy when not in-place editing */
		q_strlcpy (out, in, outsize);
	length = (int)strlen(out) - 1;
	while (length > 0 && out[length] != '.')
	{
		--length;
		if (out[length] == '/' || out[length] == '\\')
			return;	/* no extension */
	}
	if (length > 0)
		out[length] = '\0';
}

/*
============
COM_FileGetExtension - doesn't return NULL
============
*/
const char *COM_FileGetExtension (const char *in)
{
	const char	*src;
	size_t		len;

	len = strlen(in);
	if (len < 2)	/* nothing meaningful */
		return "";

	src = in + len - 1;
	while (src != in && src[-1] != '.')
		src--;
	if (src == in || strchr(src, '/') != NULL || strchr(src, '\\') != NULL)
		return "";	/* no extension, or parent directory has a dot */

	return src;
}

/*
============
COM_ExtractExtension
============
*/
void COM_ExtractExtension (const char *in, char *out, size_t outsize)
{
	const char *ext = COM_FileGetExtension (in);
	if (! *ext)
		*out = '\0';
	else
		q_strlcpy (out, ext, outsize);
}

/*
============
COM_FileBase
take 'somedir/otherdir/filename.ext',
write only 'filename' to the output
============
*/
void COM_FileBase (const char *in, char *out, size_t outsize)
{
	const char	*dot, *slash, *s;

	s = in;
	slash = in;
	dot = NULL;
	while (*s)
	{
		if (*s == '/')
			slash = s + 1;
		if (*s == '.')
			dot = s;
		s++;
	}
	if (dot == NULL)
		dot = s;

	if (dot - slash < 2)
		q_strlcpy (out, "?model?", outsize);
	else
	{
		size_t	len = dot - slash;
		if (len >= outsize)
			len = outsize - 1;
		memcpy (out, slash, len);
		out[len] = '\0';
	}
}

/*
==================
COM_ForceExtension

If path doesn't have an extension or has a different extension, append(!) specified extension
Extension should include the .
==================
*/
void COM_ForceExtension (char *path, const char *extension, size_t len)
{
	const char *curext = COM_FileGetExtension (path);

	if (curext[0] == 0)
	{
		// NO EXTENSION
		COM_DefaultExtension (path,extension,len);
		return;
	}

	// We have an extension, is it right?
	// curext points to it.

	if (strcmp(extension+1, curext)==0)
		return; // Has right extension, so done.

	q_strlcat(path, extension, len);
}

/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, const char *extension, size_t len)
{
	char	*src;
	size_t	l;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	if (!*path)
		return;
	l = strlen(path);
	src = path + l - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;		// it has an extension
		src--;
	}

	q_strlcat(path, extension, len);
}


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
const char *COM_Parse (const char *data)
{
	int             c;
	int             len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;	// end of file
		data++;
	}

// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}


// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c == '{' || c == '}'|| c == '(' || c == ')' ||  c == '\'' || c==':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		/* commented out the check for ':' so that ip:port works */
		if (c=='{' || c=='}'|| c=='(' || c==')' || c=='\'' /* || c == ':' */)
			break;
	} while (c>32);

	com_token[len] = 0;
	return data;
}


/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm (const char *parm)
{
	int             i;

	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;               // NEXTSTEP sometimes clears appkit vars.
		if (!Q_strcmp (parm,com_argv[i]))
			return i;
	}

	return 0;
}

// this graphic needs to be in the pak file to use registered features
unsigned short community[] =
{
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
,0x0000,0x0000,0x3600,0x0000,0x0000,0x0000,0x3600,0x0000
,0x0000,0x0036,0x0000,0x0000,0x0000,0x0000,0x0036,0x0000
,0x0000,0x3635,0x0000,0x0000,0x0000,0x0000,0x0035,0x3600
,0x0033,0x3531,0x0000,0x0000,0x0000,0x0000,0x0031,0x3533
,0x0034,0x3531,0x0000,0x0000,0x0000,0x0000,0x0031,0x3534
,0x0034,0x3534,0x0000,0x3438,0x3838,0x3400,0x0034,0x3534
,0x0033,0x3538,0x3200,0x0034,0x3834,0x0000,0x3238,0x3533
,0x0000,0x3537,0x3833,0x0034,0x3734,0x0033,0x3837,0x3500
,0x0000,0x3236,0x3738,0x3938,0x3738,0x3938,0x3736,0x3200
,0x0000,0x0032,0x3536,0x3636,0x3636,0x3636,0x3532,0x0000
,0x0000,0x0000,0x0032,0x3334,0x3534,0x3332,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0032,0x3632,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0031,0x3631,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0000,0x3500,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0000,0x3400,0x0000,0x0000,0x0000
};

/*
================
COM_CheckRegistered

Looks for the pop.txt file and verifies it.
Sets the "registered" cvar.
Immediately exits out if an alternate game was attempted to be started without
being registered.
================
*/
int community_check;

static void COM_CheckRegistered (void)
{
	int             h;
	unsigned short  check[128];
	int                     i;

	
	// New community file check
	COM_OpenFile("gfx/community.lmp", &h);
	static_registered = 0;
	community_check = 0;
	
	if (h != -1)
	{
		Sys_FileRead (h, check, sizeof(check));
		COM_CloseFile (h);
		community_check = 1;
		
		for (i=0 ; i<128 ; i++)
		{
			if (community[i] != (unsigned short)BigShort (check[i]))
				community_check = 0;
		}
	}
	
	if (community_check == 1)
	{
		Cvar_Set ("cmdline", com_cmdline+1); //johnfitz -- eliminate leading space
		Cvar_Set ("registered", "1");
		static_registered = 1;
		Con_Printf ("Playing community version.\n");
		return;
	}

	COM_OpenFile("gfx/pop.lmp", &h);
	static_registered = 0;

	if (h == -1)
	{		
		Con_Printf ("Playing shareware version.\n");
		if (com_modified)
			Sys_Error ("You must have the registered version to use modified games");
		return;
	}
	
	Sys_FileRead (h, check, sizeof(check));
	COM_CloseFile (h);
	
	for (i=0 ; i<128 ; i++)
	{
	     if (pop[i] != (unsigned short)BigShort (check[i]))
	        Sys_Error ("Corrupted data file.");
	}

	for (i = 0; com_cmdline[i]; i++)
	{
		if (com_cmdline[i]!= ' ')
			break;
	}

	Cvar_Set ("cmdline", &com_cmdline[i]);
	Cvar_Set ("registered", "1");
	static_registered = 1;
	Con_Printf ("Playing registered version.\n");
}


/*
================
COM_InitArgv
================
*/
void COM_InitArgv (int argc, char **argv)
{
	int             i, j, n;

// reconstitute the command line for the cmdline externally visible cvar
	n = 0;

	for (j=0 ; (j<MAX_NUM_ARGVS) && (j< argc) ; j++)
	{
		i = 0;

		while ((n < (CMDLINE_LENGTH - 1)) && argv[j][i])
		{
			com_cmdline[n++] = argv[j][i++];
		}

		if (n < (CMDLINE_LENGTH - 1))
			com_cmdline[n++] = ' ';
		else
			break;
	}

	if (n > 0 && com_cmdline[n-1] == ' ')
		com_cmdline[n-1] = 0; //johnfitz -- kill the trailing space

	Con_Printf("Command line: %s\n", com_cmdline);

	for (com_argc=0 ; (com_argc<MAX_NUM_ARGVS) && (com_argc < argc) ; com_argc++)
	{
		largv[com_argc] = argv[com_argc];
		if (!Q_strcmp ("-safe", argv[com_argc]))
			safemode = 1;
	}

	if (safemode)
	{
	// force all the safe-mode switches. Note that we reserved extra space in
	// case we need to add these, so we don't need an overflow check
		for (i=0 ; i<NUM_SAFE_ARGVS ; i++)
		{
			largv[com_argc] = safeargvs[i];
			com_argc++;
		}
	}

	largv[com_argc] = argvdummy;
	com_argv = largv;

	if (COM_CheckParm ("-rogue"))
	{
		rogue = true;
		standard_quake = false;
	}

	if (COM_CheckParm ("-hipnotic") || COM_CheckParm ("-quoth")) //johnfitz -- "-quoth" support
	{
		hipnotic = true;
		standard_quake = false;
		if (COM_CheckParm ("-quoth")) //johnfitz -- "-quoth" support
			quoth = true;

	}
}

/*
================
Test_f -- johnfitz
================
*/
#ifdef _DEBUG
static void FitzTest_f (void)
{
}

// Baker: "crash" command to forcibly crash a debug build
void Crash_f (void)
{
	void (*Crash_Me_Now) (void);

	Crash_Me_Now = NULL;
	Crash_Me_Now ();	// CRASH!!  To test crash protection.
}
#endif

/*
================
COM_Init
================
*/
void COM_Init (void)
{
	int	i = 0x12345678;
		/*    U N I X */

	/*
	BE_ORDER:  12 34 56 78
		   U  N  I  X

	LE_ORDER:  78 56 34 12
		   X  I  N  U

	PDP_ORDER: 34 12 78 56
		   N  U  X  I
	*/
	if ( *(char *)&i == 0x12 )
		host_bigendian = true;
	else if ( *(char *)&i == 0x78 )
		host_bigendian = false;
	else /* if ( *(char *)&i == 0x34 ) */
		Sys_Error ("Unsupported endianism.");

	if (host_bigendian)
	{
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}
	else /* assumed LITTLE_ENDIAN. */
	{
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}

#ifdef _DEBUG
	Cmd_AddCommand ("fitztest", FitzTest_f); //johnfitz
	Cmd_AddCommand ("crash", Crash_f); //Baker
#endif
}


/*
============
va

does a varargs printf into a temp buffer. cycles between
4 different static buffers. the number of buffers cycled
is defined in VA_NUM_BUFFS.
FIXME: make this buffer size safe someday
============
*/
#define	VA_NUM_BUFFS	4
#define	VA_BUFFERLEN	1024

static char *get_va_buffer(void)
{
	static char va_buffers[VA_NUM_BUFFS][VA_BUFFERLEN];
	static int buffer_idx = 0;
	buffer_idx = (buffer_idx + 1) & (VA_NUM_BUFFS - 1);
	return va_buffers[buffer_idx];
}

char *va (const char *format, ...)
{
	va_list		argptr;
	char		*va_buf;

	va_buf = get_va_buffer ();
	va_start (argptr, format);
	q_vsnprintf (va_buf, VA_BUFFERLEN, format, argptr);
	va_end (argptr);

	return va_buf;
}

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

char	com_filepath[MAX_OSPATH];
int     com_filesize;

//
// on disk
//
typedef struct
{
	char    name[56];
	int             filepos, filelen;
} dpackfile_t;

typedef struct
{
	char    id[4];
	int             dirofs;
	int             dirlen;
} dpackheader_t;

#define MAX_FILES_IN_PACK       2048

char    com_gamedir[MAX_OSPATH];
char 	com_basedir[MAX_OSPATH];

//============================================================================
typedef struct
{
	char    name[MAX_QPATH];
	int             filepos, filelen;
} packfile_t;


#define NUM_QFS_FILE_EXT 24
#define NUM_QFS_FOLDER_PREFIX 12
#define NUM_QFS_TYPES (NUM_QFS_FILE_EXT * NUM_QFS_FOLDER_PREFIX)

const char* qfs_file_extensions[NUM_QFS_FILE_EXT] =
{
// Texture lookups are slow and numerous, maps may have hundreds of textures
	"_glow.pcx", 
	"_glow.tga",
	"_luma.pcx", 
	"_luma.tga",
	".pcx",
	".tga",
// game assets
	".lmp",
	".mdl",
	".wav",
	".spr",
	".bsp",
	".wad",
	".dat",
// potential game assets
	".way",		// Frikbot waypoint
	".mp3",		// mp3 music
	".ogg",		// ogg music
// map related
	".lit",
	".ent",
	".vis",
	".loc",
// other media
	".dem",
	".cfg",
	".rc",
	NULL		// Catch all
};

const char* qfs_folder_prefixes[NUM_QFS_FOLDER_PREFIX] =
{
	"textures/exmy/",
	"textures/*/",		// Special: TEXTURES_WILDCARD_SUBPATH (for textures/<mapname>/
	"textures/",
	"maps/",
	"progs/",
	"gfx/env/",			// Skyboxes live here
	"gfx/particles/",	// Not that we support this
	"gfx/crosshairs/",	// Not that we support this
	"gfx/",
	"sound/",
	"music/",			// We don't support playing tracks out of pak files :(  I could, but ...
	NULL,
};

typedef struct
{
	qboolean		contains;
	int				first_index;
	int				last_index;
} pakcontent_t;

typedef struct pack_s
{
	char    filename[MAX_OSPATH];
	int             handle;
	int             numfiles;
	packfile_t      *files;

	pakcontent_t	contents[NUM_QFS_TYPES];
} pack_t;

typedef struct searchpath_s
{
	char    filename[MAX_OSPATH];
	pack_t  *pack;          // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

extern searchpath_t *com_searchpaths;




#define TEXTURES_WILDCARD_SUBPATH_1 1 


searchpath_t    *com_searchpaths;

/*
============
COM_Path_f
============
*/
static void COM_Path_f (void)
{
	searchpath_t    *s;

	Con_Printf ("Current search path:\n");
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s->pack)
		{
			Con_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		}
		else
			Con_Printf ("%s\n", s->filename);
	}
}

static void COM_Exists_f (void)
{
	if (Cmd_Argc() == 2)
	{
		FILE	*f;
		const char *check_filename = Cmd_Argv (1);
		COM_FOpenFile (check_filename, &f);

		if (f) 
		{
			FS_fclose (f);
			Con_Printf ("File %s exists with size of %i\n", Cmd_Argv (1), com_filesize);
		} else Con_Printf ("%s does not exist\n", Cmd_Argv (1), com_filesize);
	} else Con_Printf ("Exists: Indicates if a file exists in the path\n");
}


/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile (const char *filename, const void *data, int len)
{
	int             handle;
	char    name[MAX_OSPATH];

	Sys_mkdir (com_gamedir); //johnfitz -- if we've switched to a nonexistant gamedir, create it now so we don't crash

	q_snprintf (name, sizeof(name), "%s/%s", com_gamedir, filename);

	handle = Sys_FileOpenWrite (name);
	if (handle == -1)
	{
		Sys_Printf ("COM_WriteFile: failed on %s\n", name);
		return;
	}

	Sys_Printf ("COM_WriteFile: %s\n", name);
	Sys_FileWrite (handle, data, len);
	Sys_FileClose (handle);
}

// Return filename fullpath from successful open.  For mp3 music currently.
char *COM_FindFile_NoPak (const char *file_to_find)
{
	static char namebuffer [MAX_OSPATH];
	searchpath_t	*search;
	FILE *f;

	// Look through each search path
	for (search = com_searchpaths ; search ; search = search->next)
	{
		// Ignore pack files, we could one up this by not ignoring pak files and copying bytes out to temp dir file
		if (!search->pack)
		{
			q_snprintf (namebuffer, sizeof(namebuffer), "%s/%s", search->filename, file_to_find);
			f = FS_fopen_read (namebuffer, "rb");

			if (f)
			{
				FS_fclose (f);
				Con_DPrintf ("Located file '%s' in '%s'\n", file_to_find, search->filename);
				return namebuffer;
			}
			else Con_DPrintf ("Failed to locate file '%s' in '%s'\n", file_to_find, search->filename);
		}
	}

	return NULL;	// Failure
}

/*
============
COM_CreatePath
============
*/
void COM_CreatePath (char *path)
{
	char    *ofs;

	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{       // create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
}

int QFS_File_Category(const char *qpath_filename)
{
	const char* file_ext;
	int			file_ext_class;
	const char* file_folder_prefix;
	int			file_folder_prefix_class;
	int			classification = 0;
	int			i;

	for (i = 0; i < NUM_QFS_FILE_EXT && qfs_file_extensions[i] != NULL; i ++)
	{
		const char* this_ext = qfs_file_extensions[i];
		
		if ( String_Does_End_With (qpath_filename, this_ext) )
			break;
	}
	file_ext_class = i;
	file_ext = qfs_file_extensions[i];

	for (i = 0; i < NUM_QFS_FOLDER_PREFIX && qfs_folder_prefixes[i] != NULL; i ++)
	{
		const char* this_prefix = qfs_folder_prefixes[i];

		// Baker: If I could eliminate this I would ...
		if (i == TEXTURES_WILDCARD_SUBPATH_1) // Special exception
		{
			// Is there a "/" past "textures/"
			if (String_Does_Start_With (qpath_filename, "textures/") && strstr (qpath_filename + 9, "/") )
				break;
			else continue; // Don't bother with the rest
		}

		if ( String_Does_Start_With (qpath_filename, this_prefix) )
			break;
	}
	file_folder_prefix_class = i;
	file_folder_prefix = qfs_folder_prefixes[i];

	classification = file_folder_prefix_class * NUM_QFS_FILE_EXT + file_ext_class;
	return classification;
}



static int COM_FindFile (const char *filename, int *handle, FILE **file, 
							const char *media_owner_path)
{
	searchpath_t    *search;
	qboolean 		pak_limiter_blocking = false;
	int				file_category = QFS_File_Category(filename);
			
	char            netpath[MAX_OSPATH];	
	int             findtime;
	int             i;

// Invalidate existing data
	com_filesize		= -1;	// File length
	com_filepath[0]		= 0;

	if (file && handle)
		Sys_Error ("COM_FindFile: both handle and file set");
	if (!file && !handle)
		Sys_Error ("COM_FindFile: neither handle or file set");

//
// search through the path, one element at a time
//
	for (search = com_searchpaths; search ; search = search->next)
	{
		// is the element a pak file?
		if (search->pack && pak_limiter_blocking)	// We hit earliest accepted pak file.
			continue;
		else if (search->pack)
		{
			pack_t          *pak = search->pack;

			int		starting_index  = 0;
			int		ending_index    = pak->numfiles;	// Baker: Yeah, this isn't the "ending_index" unless we add +1

			do
			{
				if (!pak->contents[file_category].contains)
				{
//					static int prevent = 0;

//					prevent++;
//					Con_SafePrintf ("Prevented %i on %s\n", prevent++, filename);
					break;	// There are no files with that extension class in the pak
				}

				// Ok this file type exists in pak
				starting_index = pak->contents[file_category].first_index;
				ending_index   = pak->contents[file_category].last_index + 1;	// Baker: must +1, you know ... its a "for loop" don't wanna skip last file

				// look through all the pak file elements
				// Formerly from i = 0 to i < pak->numfiles
				for (i = starting_index ; i < ending_index  ; i++)
				{
					if (strcmp (pak->files[i].name, filename) != 0)
						continue; // No match
	
					// found it!
//					Sys_Printf ("PackFile: %s : %s\n",pak->filename, filename);  // Baker: This spams!
					if (handle)
					{
						*handle = pak->handle;
						Sys_FileSeek (pak->handle, pak->files[i].filepos);
					}
					else
					{   
						// open a new file on the pakfile
						*file = FS_fopen_read (pak->filename, "rb");
						// advance to correct seek position
						if (*file)
						
							fseek (*file, pak->files[i].filepos, SEEK_SET);


					}
					com_filesize = pak->files[i].filelen;
					q_strlcpy (com_filepath, pak->filename, sizeof(com_filepath));
					return com_filesize;
	
	
	
				}
			} while (0);
			// File is not in this pack
			if (media_owner_path && Q_strcasecmp (media_owner_path, pak->filename) == 0)
				pak_limiter_blocking = true; // Found pak of media owner.  No more pak checking.

			continue;
		}
		else /* check a file in the directory tree */
		{
			if (!static_registered)
			{ /* if not a registered version, don't ever go beyond base */
				if ( strchr (filename, '/') || strchr (filename,'\\'))
					continue;
			}

			q_snprintf (netpath, sizeof(netpath), "%s/%s",search->filename, filename);
			findtime = Sys_FileTime (netpath);
			if (findtime == -1)
			{
				if (media_owner_path && (Q_strcasecmp (media_owner_path, search->filename) == 0 || (pak_limiter_blocking && Q_strncasecmp(media_owner_path, search->filename, strlen(search->filename))==0 )))
					break; // If this search path is the media owner path or the full search path is in the media owner path, stop.  Baker: the 2nd case is for media owner's in a subfolder.  Like search path is quake/id and media owner is quake/id1/progs
				else
					continue;
			}

//			Sys_Printf ("FindFile: %s\n",netpath);  Baker: This spams
			com_filesize = Sys_FileOpenRead (netpath, &i);
			if (handle)
				*handle = i;
			else
			{
				Sys_FileClose (i);
				*file = FS_fopen_read (netpath, "rb");
			}
			q_strlcpy (com_filepath, search->filename, sizeof(com_filepath) );
			return com_filesize;
		}
	}

	if (developer.value > 1) // Baker: This spams too much
	Con_DPrintf ("FindFile: can't find %s\n", filename);

	if (handle)
		*handle = -1;
	else
		*file = NULL;
	com_filesize = -1;
	return -1;
}


/*
===========
COM_OpenFile

filename never has a leading slash, but may contain directory walks
returns a handle and a length
it may actually be inside a pak file
===========
*/

int COM_OpenFile_Limited (const char *filename, int *handle, const char *media_owner_path)
{
	return COM_FindFile (filename, handle, NULL, media_owner_path);
}

int COM_OpenFile (const char *filename, int *handle)
{
	return COM_FindFile (filename, handle, NULL, NULL /* no media owner path */);
}

/*
===========
COM_FOpenFile

If the requested file is inside a packfile, a new FILE * will be opened
into the file.
===========
*/
int COM_FOpenFile_Limited (const char *filename, FILE **file, const char *media_owner_path)
{
	return COM_FindFile (filename, NULL, file, media_owner_path);
}

int COM_FOpenFile (const char *filename, FILE **file)
{
	return COM_FOpenFile_Limited (filename, file, NULL /* no media owner path */);
}

/*
============
COM_CloseFile

If it is a pak file handle, don't really close it
============
*/
void COM_CloseFile (int h)
{
	searchpath_t    *s;

	for (s = com_searchpaths ; s ; s=s->next)
		if (s->pack && s->pack->handle == h)
			return;

	Sys_FileClose (h);
}


/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Allways appends a 0 byte.
============
*/
#define	LOADFILE_ZONE		0
#define	LOADFILE_HUNK		1
#define	LOADFILE_TEMPHUNK	2
#define	LOADFILE_CACHE		3
#define	LOADFILE_STACK		4
#define LOADFILE_MALLOC		5

static byte    *loadbuf;
static cache_user_t *loadcache;
static int             loadsize;

static byte *COM_LoadFile_Limited (const char *path, int usehunk, const char *media_owner_path)
{
	int             h;
	byte    *buf;
	char    base[32];
	int             len;

	buf = NULL;     // quiet compiler warning

// look for it in the filesystem or pack files
	len = COM_OpenFile_Limited (path, &h, media_owner_path);
	if (h == -1)
		return NULL;

// extract the filename base name for hunk tag
	COM_FileBase (path, base, sizeof(base));

	switch (usehunk)
	{
	case LOADFILE_HUNK:
		buf = (byte*) Hunk_AllocName (len+1, base);
		break;
	case LOADFILE_TEMPHUNK:
		buf = (byte*) Hunk_TempAlloc (len+1);
		break;
	case LOADFILE_ZONE:
		buf = (byte*) Z_Malloc (len+1);
		break;
	case LOADFILE_CACHE:
		buf = (byte*)  Cache_Alloc (loadcache, len+1, base);
		break;
	case LOADFILE_STACK:
		if (len < loadsize)
			buf = loadbuf;
		else
			buf = (byte *) Hunk_TempAlloc (len+1);
		break;
	case LOADFILE_MALLOC:
		buf = (byte *) malloc (len+1);
		break;
	default:
		Sys_Error ("COM_LoadFile: bad usehunk");
	}

	if (!buf)
		Sys_Error ("COM_LoadFile: not enough space for %s", path);

	((byte *)buf)[len] = 0;

	Sys_FileRead (h, buf, len);
	COM_CloseFile (h);

	return buf;
}

byte *COM_LoadFile (const char *path, int usehunk)
{
	return COM_LoadFile_Limited (path, usehunk, NULL /* no media owner path */);
}

byte *COM_LoadHunkFile_Limited (const char *path, const char *media_owner_path)
{
	return COM_LoadFile_Limited (path, LOADFILE_HUNK, media_owner_path);
}

byte *COM_LoadHunkFile (const char *path)
{
	return COM_LoadFile (path, LOADFILE_HUNK);
}

byte *COM_LoadTempFile (const char *path)
{
	return COM_LoadFile (path, LOADFILE_TEMPHUNK);
}


// uses temp hunk if larger than bufsize
byte *COM_LoadStackFile (const char *path, void *buffer, int bufsize)
{
	byte    *buf;

	loadbuf = (byte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, LOADFILE_STACK);

	return buf;
}

// returns malloc'd memory
byte *COM_LoadMallocFile (const char *path)
{
	return COM_LoadFile (path, LOADFILE_MALLOC);
}


/*
=================
COM_LoadPackFile -- johnfitz -- modified based on topaz's tutorial

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *COM_LoadPackFile (const char *packfile)
{
	dpackheader_t   header;
	int                             i;
	packfile_t              *newfiles;
	int                             numpackfiles;
	pack_t                  *pack;
	int                             packhandle;
	dpackfile_t             info[MAX_FILES_IN_PACK];
	unsigned short          crc;

	if (Sys_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;
	Sys_FileRead (packhandle, (void *)&header, sizeof(header));
	if (header.id[0] != 'P' || header.id[1] != 'A' || header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error ("%s is not a packfile", packfile);
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error ("%s has %i files", packfile, numpackfiles);

	if (numpackfiles != PAK0_COUNT)
		com_modified = true;    // not the original file

	//johnfitz -- dynamic gamedir loading
	//Hunk_AllocName (numpackfiles * sizeof(packfile_t), "packfile");
	newfiles = (packfile_t *) Z_Malloc(numpackfiles * sizeof(packfile_t));
	//johnfitz

	Sys_FileSeek (packhandle, header.dirofs);
	Sys_FileRead (packhandle, (void *)info, header.dirlen);

	//johnfitz -- dynamic gamedir loading
	//pack = Hunk_Alloc (sizeof (pack_t));
	pack = (pack_t *) Z_Malloc (sizeof (pack_t));
	//johnfitz

	q_strlcpy (pack->filename, packfile, sizeof(pack->filename));
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;


	// crc the directory to check for modifications
	CRC_Init (&crc);
	for (i = 0; i < header.dirlen ; i++)
		CRC_ProcessByte (&crc, ((byte *)info)[i]);
	if (crc != PAK0_CRC_V106 && crc != PAK0_CRC_V101 && crc != PAK0_CRC_V100)
		com_modified = true;

	{	// Speed up begin
		int k;
		for (k = 0; k < (NUM_QFS_FILE_EXT * NUM_QFS_FOLDER_PREFIX) ; k++)
		{
			pack->contents[k].contains	= false;
			pack->contents[k].first_index = -1;
			pack->contents[k].last_index = 0;
		}
	}

	// parse the directory
	for (i = 0; i < pack->numfiles ; i++)
	{
		int file_category = QFS_File_Category(info[i].name);
		
		q_strlcpy (newfiles[i].name, info[i].name, sizeof(newfiles[i].name));
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);

		// Speed up begin
		
		pack->contents[file_category].contains = true;
		if (pack->contents[file_category].first_index < 0)
			pack->contents[file_category].first_index = i;
		pack->contents[file_category].last_index = i; // Because i always increases, no need to see if is greater than last one
		// Speed up end
	}


	//Con_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}

/*
=================
COM_AddGameDirectory -- johnfitz -- modified based on topaz's tutorial
=================
*/
void COM_AddGameDirectory (const char *dir)
{
	int i;
	searchpath_t *search;
	pack_t *pak;
	char pakfile[MAX_OSPATH];

	q_strlcpy (com_gamedir, dir, sizeof(com_gamedir));

	for (search = com_searchpaths; search; search = search->next)
		if (Q_strcasecmp (search->filename, dir) == 0)
			return; // Already added this dir.

	// add the directory to the search path
	search = (searchpath_t *) Z_Malloc(sizeof(searchpath_t));
	q_strlcpy (search->filename, dir, sizeof(search->filename));
	search->next = com_searchpaths;
	com_searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	for (i = 0; ; i++)
	{
		q_snprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		pak = COM_LoadPackFile (pakfile);
		if (!pak)
			break;
		search = (searchpath_t *) Z_Malloc(sizeof(searchpath_t));
		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
}



/*
=================
COM_InitFilesystem
=================
*/
void COM_InitFilesystem (void) //johnfitz -- modified based on topaz's tutorial
{
	int i, j;

	Cvar_RegisterVariable (&registered);
	Cvar_RegisterVariable (&cmdline);
	Cmd_AddCommand ("path", COM_Path_f);
	Cmd_AddCommand ("exists", COM_Exists_f);

	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		q_strlcpy (com_basedir, com_argv[i + 1], sizeof(com_basedir));
	else
		q_strlcpy (com_basedir, host_parms->basedir, sizeof(com_basedir));

	j = strlen (com_basedir);
	if (j > 0)
	{
		if ((com_basedir[j-1] == '\\') || (com_basedir[j-1] == '/'))
		com_basedir[j-1] = 0;
	}
	
	// start up with GAMENAME by default (id1)
	COM_AddGameDirectory (va("%s/"GAMENAME, com_basedir) );
	q_snprintf (com_gamedir, sizeof(com_gamedir), "%s/"GAMENAME, com_basedir);

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	if (COM_CheckParm ("-rogue")) 
		COM_AddGameDirectory (va("%s/rogue", com_basedir) );
	if (COM_CheckParm ("-hipnotic")) 
		COM_AddGameDirectory (va("%s/hipnotic", com_basedir) );
	if (COM_CheckParm ("-quoth")) 
		COM_AddGameDirectory (va("%s/quoth", com_basedir) );

	i = COM_CheckParm ("-game");
	if (i && i < com_argc-1)
	{
		com_modified = true;
		COM_AddGameDirectory (va("%s/%s", com_basedir, com_argv[i+1]));
	}

	COM_CheckRegistered ();
}

// Baker: Make users aware of bad practices like mixed-case or uppercase names
// No this isn't perfect, but maybe will get ball rolling so those that do that
// are more aware without someone having to tell them.
void COM_Uppercase_Check (const char* in_name)
{
	const char* name = COM_SkipPath (in_name);
	if (String_Contains_Uppercase (name))
		Con_Warning ("Compatibility: filename \"%s\" contains uppercase\n", name);
}




//==============================================================================
//johnfitz -- dynamic gamedir stuff
//==============================================================================

void COM_RemoveAllPaths (void)
{
	searchpath_t *next;
	while (com_searchpaths != com_base_searchpaths)
	{
		next = com_searchpaths->next;
		if (com_searchpaths->pack)
		{
			Con_DPrintf("Releasing %s ...\n", com_searchpaths->pack->filename);
			Sys_FileClose (com_searchpaths->pack->handle);

			Z_Free (com_searchpaths->pack->files);
			Z_Free (com_searchpaths->pack);
		}

		Z_Free (com_searchpaths);

		com_searchpaths = next;
	}
	q_snprintf (com_gamedir, sizeof(com_gamedir), "%s/%s", com_basedir, GAMENAME );
	Con_Printf("Release complete.\n\n");
}

// Return the number of games in memory
int NumGames(searchpath_t *search)
{
	int found = 0;
	while (search)
	{
		if (*search->filename)
			found++;
		search = search->next;
	}
	return found;
}

#include "q_dirent.h"

enum {SPECIAL_NONE = 0, SPECIAL_NO_ID1_PAKFILE = 1, SPECIAL_DONOT_STRIP_EXTENSION = 2, SPECIAL_STRIP_6_SKYBOX = 4};
#define STRIP_LEN_6 6 // "rt.tga"
void List_Filelist_Rebuild (listmedia_t** list, const char* slash_subfolder, const char* dot_extension, int pakminfilesize, int directives)
{
	char			filenamebuf[MAX_QPATH];
	searchpath_t    *search;
	// If list exists, erase it first.
	if (*list) 
		List_Free (list);

	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (*search->filename) //directory
		{
			DIR		*dir_p = opendir(va ("%s%s", search->filename, slash_subfolder) );
			struct dirent	*dir_t;
			
			if (dir_p == NULL)
				continue;
			while ((dir_t = readdir(dir_p)) != NULL)
			{
				if (!String_Does_End_With_Caseless(dir_t->d_name, dot_extension))
					continue;

				q_strlcpy (filenamebuf, dir_t->d_name, sizeof(filenamebuf) );
				
				if ( (directives == SPECIAL_STRIP_6_SKYBOX) )
				{
					int len = strlen(filenamebuf);
					// length 7 - strip 6 = null @ 1 = strlen - 6 = 0
					if (len > STRIP_LEN_6)
						filenamebuf[len-STRIP_LEN_6] = 0;
					else Con_Printf ("Couldn't strip filename\n");
				}
				else	
				if ( (directives & SPECIAL_DONOT_STRIP_EXTENSION) == 0)
					String_Edit_RemoveExtension (filenamebuf);
				List_Add (list, filenamebuf);
			}
			closedir(dir_p);
			// End of directory
		}
		else //pakfile
		{
			pack_t          *pak;
			int             i;

			// This is used to exclude id1 maps from results (e1m1 etc.)
			if ( (directives & SPECIAL_NO_ID1_PAKFILE) && strstr(search->pack->filename, va("/%s/", GAMENAME)) )
				continue;

			for (i = 0, pak = search->pack; i < pak->numfiles ; i++)
			{
				// Baker --- if we are specifying a subfolder, for a pak
				// This must prefix the file AND then we must skip it.
				const char* current_filename = pak->files[i].name;

				if (!String_Does_End_With_Caseless(current_filename, dot_extension))
					continue;
				
				// we are passed something like "/maps" but for pak system we
				// need to skip the leading "/" &so slash_subfolder[1]
				if (slash_subfolder[0])
				{
					if (String_Does_Start_With_Caseless(current_filename, va("%s/", &slash_subfolder[1]) )  )
					{
						current_filename = COM_SkipPath (current_filename);
					} else continue; // Doesn't start with subfolder
				}

				// Baker: johnfitz uses this to keep ammo boxes, etc. out of list
				if (pak->files[i].filelen < pakminfilesize)
					continue;

				q_strlcpy (filenamebuf, current_filename, sizeof(filenamebuf) );
				if ( (directives & SPECIAL_STRIP_6_SKYBOX) )
				{
					int len = strlen(filenamebuf);
					// length 7 - strip 6 = null @ 1 = strlen - 6 = 0
					if (len > STRIP_LEN_6)
						filenamebuf[len-STRIP_LEN_6] = 0;
					else Con_Printf ("Couldn't strip filename\n");
				}
				else	
				if ( (directives & SPECIAL_DONOT_STRIP_EXTENSION) == 0)
					String_Edit_RemoveExtension (filenamebuf);
				List_Add (list, filenamebuf);
			}
			// End of pak
		}
		// End of main for loop
	}
	// End of function
}


char *COM_CL_Worldspawn_Value_For_Key (const char *find_keyname)
{
	static char		valuestring[4096];
	char			current_key[128];
	const char		*data;
	const char		*copy_start;

	// Read some data ...
	if (!(data = COM_Parse(data = cl.worldmodel->entities)) || com_token[0] != '{')	// Opening brace is start of worldspawn
		return NULL; // error

	while (1)
	{
		// Read some data ...
		if (!(data = COM_Parse(data)))
			return NULL; // End of data
			
		if (com_token[0] == '}')	// Closing brace is end of worldspawn
			return NULL; // End of worldspawn

		// Copy data over, skipping a prefix of '_' in a keyname
		copy_start = &com_token[0]; 
		
		if (*copy_start == '_')
			copy_start++;
		
		q_strlcpy (current_key, copy_start, sizeof(current_key) );

		String_Edit_RemoveTrailingSpaces (current_key);

		if (!(data = COM_Parse(data)))
			return NULL; // error

		if (Q_strcasecmp (find_keyname, current_key) == 0)
		{
			q_strlcpy (valuestring, com_token, sizeof(valuestring));
			return valuestring;
		}

	}

	return NULL;
}

void swapf (float* a, float* b)
{
	float c = *a;
	*a=*b;
	*b=c;
}
