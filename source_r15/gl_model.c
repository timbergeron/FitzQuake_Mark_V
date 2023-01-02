/* This file isn't done!

Versus Quakespasm:

1. Quakespasm registers gl_subdivide_size here.
2. Handles external ents here.
3. Mark V supports many kinds of texture effects (alpha masked, etc.) and
   Half-Life WAD3.
4. Mark V supports external replacement textures for models.
5. Mark V only loads external replacement textures following same rules that
   Quakespasm uses for .lit files.
6. Mark V bsp 30, antiwallhack, etc.  Extra models lists.

Note: I don't have Mark V do external ents here because I feel that external
 entities are server side only, therefore a server function.  Doing it
 any other way creates weird client/server oddnesses with "hacked" on features
  like fog and skyboxes that don't work via server svc messages.

*/

/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2011 O.Sezer

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
// models.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "quakedef.h"

qmodel_t	*loadmodel;
char	loadname[MAX_QPATH];	// for hunk tags

void Mod_LoadSpriteModel (qmodel_t *mod, void *buffer);
void Mod_LoadBrushModel (qmodel_t *mod, void *buffer);
void Mod_LoadAliasModel (qmodel_t *mod, void *buffer);
qmodel_t *Mod_LoadModel (qmodel_t *mod, qboolean crash);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	2048 /*johnfitz -- was 512 */
qmodel_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

texture_t	*r_notexture_mip; //johnfitz -- moved here from r_main.c
texture_t	*r_notexture_mip2; //johnfitz -- used for non-lightmapped surfs with a missing texture


/*
===============
Mod_Init
===============
*/
cvar_t external_lits = {"external_lits", "1", CVAR_NONE};
cvar_t external_textures = {"external_textures", "1", CVAR_NONE};

void External_Textures_Change_f (cvar_t* var);

void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));

	//johnfitz -- create notexture miptex
	r_notexture_mip = (texture_t *)Hunk_AllocName (sizeof(texture_t), "r_notexture_mip");
	strcpy (r_notexture_mip->name, "notexture");
	r_notexture_mip->height = r_notexture_mip->width = 32;

	r_notexture_mip2 = (texture_t *)Hunk_AllocName (sizeof(texture_t), "r_notexture_mip2");
	strcpy (r_notexture_mip2->name, "notexture2");
	r_notexture_mip2->height = r_notexture_mip2->width = 32;
	//johnfitz

	Cvar_RegisterVariableWithCallback (&external_lits, Host_Changelevel_Required_Msg);
	Cvar_RegisterVariableWithCallback (&external_textures, External_Textures_Change_f);
}

/*
===============
Mod_Extradata

Caches the data if needed
===============
*/
void *Mod_Extradata (qmodel_t *mod)
{
	void	*r;

	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);

	if (!mod->cache.data)
		Host_Error ("Mod_Extradata: caching failed");
	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, qmodel_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;

	if (!model || !model->nodes)
		Host_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, qmodel_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->numleafs+7)>>3;
	out = decompressed;

#if 0
	memcpy (out, in, row);
#else
	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
#endif

	return decompressed;
}

byte *Mod_LeafPVS (mleaf_t *leaf, qmodel_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	int		i;
	qmodel_t	*mod;

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (mod->type != mod_alias)
		{
			mod->needload = true;
			TexMgr_FreeTexturesForOwner (mod); //johnfitz
		}
}

/*
==================
Mod_FindName

==================
*/
qmodel_t *Mod_FindName (const char *name)
{
	int		i;
	qmodel_t	*mod;

	if (!name[0])
		Host_Error ("Mod_FindName: NULL name"); //johnfitz -- was "Mod_ForName"

//
// search the currently loaded models
//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!strcmp (mod->name, name) )
			break;

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Host_Error ("mod_numknown == MAX_MOD_KNOWN");
		q_strlcpy (mod->name, name, MAX_QPATH);
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void Mod_TouchModel (const char *name)
{
	qmodel_t	*mod;

	mod = Mod_FindName (name);

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
qmodel_t *Mod_LoadModel (qmodel_t *mod, qboolean crash)
{
	byte	*buf;
	byte	stackbuf[1024];		// avoid dirtying the cache heap
	int	mod_type;

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
		{
			if (Cache_Check (&mod->cache))
				return mod;
		}
		else
			return mod;		// not cached at all
	}

//
// because the world is so huge, load it one piece at a time
//
	if (!crash)
	{

	}

//
// load the file
//
	buf = (byte *)COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf));
	if (!buf)
	{
		if (crash)
			Host_Error ("Mod_LoadModel: %s not found", mod->name); //johnfitz -- was "Mod_NumForName"
		return NULL;
	}

//
// allocate a new model
//
	COM_FileBase (mod->name, loadname, sizeof(loadname));
//	Con_Printf ("Loadname is %s\n", loadname);
	loadmodel = mod;

	// Update the path
	q_strlcpy (mod->loadinfo.searchpath, com_filepath, sizeof(mod->loadinfo.searchpath));

//
// fill it in
//

// call the apropriate loader
	mod->needload = false;

	mod_type = (buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
	switch (mod_type)
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel (mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;

	default:
		Mod_LoadBrushModel (mod, buf);
		break;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
qmodel_t *Mod_ForName (const char *name, qboolean crash)
{
	qmodel_t	*mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

static byte	*mod_base;

/*
=================
Mod_CheckFullbrights -- johnfitz
=================
*/
qboolean Mod_CheckFullbrights (byte *pixels, int count, qboolean alphatex)
{
	int i;
	for (i = 0; i < count; i++)
	{
		if (pixels[i] > 223)
			if (alphatex && pixels[i] == 255)
				continue; // If alphatex 255 isn't fullbright so ignore mask pixel
			else return true;
	}
	return false;
}

/*
=================
Mod_LoadTextures
=================
*/
qboolean Is_Texture_Prefix (const char *texturename, const char *prefixstring)
{	
	if (prefixstring[0] == 0)
		return false; // 0 length string

	if (Q_strncasecmp(texturename, prefixstring, strlen(prefixstring)) == 0)
		return true;

	return false;
}



#include "mod_load_textures.h"

// Baker: The below is from FTE
byte lmgamma[256];
void BuildLightMapGammaTable (float g, float c)
{
	int i, inf;

//	g = bound (0.1, g, 3);
//	c = bound (1, c, 3);

	if (g == 1 && c == 1)
	{
		for (i = 0; i < 256; i++)
			lmgamma[i] = i;
		return;
	}

	for (i = 0; i < 256; i++)
	{
		inf = 255 * pow ((i + 0.5) / 255.5 * c, g) + 0.5;
		if (inf < 0)
			inf = 0;
		else if (inf > 255)
			inf = 255;		
		lmgamma[i] = inf;
	}
}

void LightNormalize (byte* litdata, const byte* normal, int bsp_lit_size)
{
	static qboolean built_table = false;
	float prop;
	int i;

	if (!built_table)
	{
		BuildLightMapGammaTable(1, 1);
		built_table = true;
	}
				
	//force it to the same intensity. (or less, depending on how you see it...)
	for (i = 0; i < bsp_lit_size; i++)	
	{
		#define m(a, b, c) (a>(b>c?b:c)?a:(b>c?b:c))
		prop = (float)m(litdata[0],  litdata[1], litdata[2]);

		if (!prop)
		{
			litdata[0] = lmgamma[*normal];
			litdata[1] = lmgamma[*normal];
			litdata[2] = lmgamma[*normal];
		}
		else
		{
			prop = lmgamma[*normal] / prop;
			litdata[0] *= prop;
			litdata[1] *= prop;
			litdata[2] *= prop;
		}

		normal++;
		litdata+=3;
	}
}


/*
=================
Mod_LoadLighting -- johnfitz -- replaced with lit support code via lordhavoc
=================
*/
static void Mod_LoadLighting (lump_t *l)
{
	int mark = Hunk_LowMark();

	loadmodel->lightdata = NULL;

	if (loadmodel->bspversion == BSPVERSION_HALFLIFE)
	{
		if (!l->filelen) 
			return; // Because we won't be checking for external lits with Half-Life, this is automatic fail

		loadmodel->lightdata = Hunk_AllocName(l->filelen, loadname);
		Con_Printf ("lighting data at %i with length\n", mod_base + l->fileofs, l->filelen);
		memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
		return;
	}

	
	// External lit phase
	do
	{
		char litfilename[MAX_OSPATH];
		int qlitversion;
		byte *data, *bsp_light_data;

		if (!external_lits.value)
			break;

		// LordHavoc: check for a .lit file
		q_strlcpy(litfilename, loadmodel->name, sizeof(litfilename));
		COM_StripExtension(litfilename, litfilename, sizeof(litfilename));
		q_strlcat(litfilename, ".lit", sizeof(litfilename));
		
		data = (byte*) COM_LoadHunkFile_Limited (litfilename, loadmodel->loadinfo.searchpath);
//		Con_Printf ("com_filesize is %i", com_filesize);

		if (!data)
			break;

		if (l->filelen && (com_filesize - 8) != l->filelen * 3)
		{
			Con_Printf("Corrupt .lit file (old version?), ignoring\n");
			break;
		}

		if (memcmp (data, "QLIT", 4) !=0 )
		{
			Con_Printf("Corrupt .lit file (old version?), ignoring\n");
			break;
		}


		qlitversion = LittleLong(((int *)data)[1]);
		if (qlitversion != 1)
		{
			Con_Printf("Unknown .lit file version (%d)\n", qlitversion);
			break;
		}

		// Commmitted
		loadmodel->lightdata = data + 8;

		// Final act
		bsp_light_data = mod_base + l->fileofs;
		LightNormalize (loadmodel->lightdata, bsp_light_data, l->filelen);

		// Success
		return;

	} while (0);	
	
	// No lit, regular data
	Hunk_FreeToLowMark (mark);

	if (l->filelen)
	{
		int i;
		byte *in, *out;
		byte d;

		// Expand the white lighting data to color ...
		loadmodel->lightdata = (byte *) Hunk_AllocName (l->filelen * 3, "qlightdata");

		// place the file at the end, so it will not be overwritten until the very last write
		in = loadmodel->lightdata + l->filelen * 2;
		memcpy (in, mod_base + l->fileofs, l->filelen);

		for (i = 0, out = loadmodel->lightdata; i < l->filelen; i ++, out +=3)
		{
			d = *in++;
			out[0] = out[1] = out[2] = d;
		}
	}
}


/*
=================
Mod_LoadVisibility
=================
*/
static void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		Con_DPrintf ("No vis for model\n");
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = (byte *)Hunk_AllocName ( l->filelen, loadname);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
static void Mod_LoadEntities (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}

	loadmodel->entities = (char *)Hunk_AllocName ( l->filelen, loadname);
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVertexes
=================
*/
static void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (dvertex_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadVertexes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mvertex_t *)Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
static void Mod_LoadEdges (lump_t *l, int bsp2)
{
	medge_t *out;
	int 	i, count;

	if (bsp2)
	{
		dledge_t *in = (dledge_t *)(mod_base + l->fileofs);

		if (l->filelen % sizeof(*in))
			Host_Error ("Mod_LoadEdges: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*in);
		out = (medge_t *)Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);

		loadmodel->edges = out;
		loadmodel->numedges = count;

		for ( i=0 ; i<count ; i++, in++, out++)
		{
			out->v[0] = LittleLong(in->v[0]);
			out->v[1] = LittleLong(in->v[1]);
		}
	}
	else
	{
		dsedge_t *in = (dsedge_t *)(mod_base + l->fileofs);

		if (l->filelen % sizeof(*in))
			Host_Error ("Mod_LoadEdges: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*in);
		out = (medge_t *)Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);

		loadmodel->edges = out;
		loadmodel->numedges = count;

		for ( i=0 ; i<count ; i++, in++, out++)
		{
			out->v[0] = (unsigned short)LittleShort(in->v[0]);
			out->v[1] = (unsigned short)LittleShort(in->v[1]);
		}
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
static void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count, miptex;
//	float	len1, len2;
	int missing = 0; //johnfitz

	in = (texinfo_t *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadTexinfo: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mtexinfo_t *)Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<4 ; j++)
		{
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat (in->vecs[1][j]);
		}
/* Baker: Removed, unused
		len1 = VectorLength (out->vecs[0]);
		len2 = VectorLength (out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;
#if 0
		if (len1 + len2 < 0.001)
			out->mipadjust = 1;		// don't crash
		else
			out->mipadjust = 1 / floor( (len1+len2)/2 + 0.1 );
#endif
*/

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		//johnfitz -- rewrote this section
		if (miptex >= loadmodel->numtextures-1 || !loadmodel->textures[miptex])
		{
			if (out->flags & TEX_SPECIAL)
				out->texture = loadmodel->textures[loadmodel->numtextures-1];
			else
				out->texture = loadmodel->textures[loadmodel->numtextures-2];

			out->flags |= TEX_MISSING;
			missing++;
		}
		else
		{
			out->texture = loadmodel->textures[miptex];
		}

		//johnfitz
	}

	//johnfitz: report missing textures
	if (missing && loadmodel->numtextures > 1)
		Con_Printf ("Mod_LoadTexinfo: %d texture(s) missing from BSP file\n", missing);

	//johnfitz
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
static void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];

		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];

			if (val < mins[j])
				mins[j] = val;

			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > MAX_FITZQUAKE_SURFACE_EXTENTS) //johnfitz -- was 512 in glquake, 256 in winquake
			Host_Error ("CalcSurfaceExtents: Bad surface extents %d (MAX: %d), texture is %s.", s->extents[i], MAX_FITZQUAKE_SURFACE_EXTENTS, tex->texture->name);

		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > MAX_WINQUAKE_SURFACE_EXTENTS /* 256*/ )
			Con_Warning ("%d surface extents exceed standard limit of %d.\n", s->extents[i], MAX_WINQUAKE_SURFACE_EXTENTS);

	}
}

/*
================
Mod_PolyForUnlitSurface -- johnfitz -- creates polys for unlightmapped surfaces (sky and water)

TODO: merge this into BuildSurfaceDisplayList?
================
*/
void Mod_PolyForUnlitSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts, i, lindex;
	float		*vec;
	glpoly_t	*poly;
	float		texscale;

	if (fa->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
		texscale = (1.0/128.0); //warp animation repeats every 128
	else
		texscale = (1.0/32.0); //to match r_notexture_mip

	// convert edges back to a normal polygon
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	//create the poly
	poly = (glpoly_t *)Hunk_Alloc (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = NULL;
	fa->polys = poly;
	poly->numverts = numverts;
	for (i=0, vec=(float *)verts; i<numverts; i++, vec+= 3)
	{
		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = DotProduct(vec, fa->texinfo->vecs[0]) * texscale;
		poly->verts[i][4] = DotProduct(vec, fa->texinfo->vecs[1]) * texscale;
	}
}

/*
=================
Mod_CalcSurfaceBounds -- johnfitz -- calculate bounding box for per-surface frustum culling
=================
*/
void Mod_CalcSurfaceBounds (msurface_t *s)
{
	int			i, e;
	mvertex_t	*v;

	s->mins[0] = s->mins[1] = s->mins[2] =  9999999;
	s->maxs[0] = s->maxs[1] = s->maxs[2] = -9999999;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		if (s->mins[0] > v->position[0])
			s->mins[0] = v->position[0];
		if (s->mins[1] > v->position[1])
			s->mins[1] = v->position[1];
		if (s->mins[2] > v->position[2])
			s->mins[2] = v->position[2];

		if (s->maxs[0] < v->position[0])
			s->maxs[0] = v->position[0];
		if (s->maxs[1] < v->position[1])
			s->maxs[1] = v->position[1];
		if (s->maxs[2] < v->position[2])
			s->maxs[2] = v->position[2];
	}
}


/*
=================
Mod_LoadFaces
=================
*/
static void Mod_LoadFaces (lump_t *l, qboolean bsp2)
{
	dsface_t	*ins;
	dlface_t	*inl;
	msurface_t 	*out;
	int			i, count, surfnum, lofs;
	int			planenum, side, texinfon;

	if (bsp2)
	{
		ins = NULL;
		inl = (dlface_t *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*inl))
			Host_Error ("Mod_LoadFaces: funny lump size in %s",loadmodel->name);
		count = l->filelen / sizeof(*inl);
	}
	else
	{
		ins = (dsface_t *)(mod_base + l->fileofs);
		inl = NULL;
		if (l->filelen % sizeof(*ins))
			Host_Error ("Mod_LoadFaces: funny lump size in %s",loadmodel->name);
		count = l->filelen / sizeof(*ins);
	}

	out = (msurface_t *)Hunk_AllocName ( count*sizeof(*out), loadname);

	//johnfitz -- warn mappers about exceeding old limits
//	if (count > 32767)
//		Con_Warning ("%i faces exceeds standard limit of 32767.\n", count);

	if (count > MAX_WINQUAKE_MAP_FACES)
		Con_Warning ("%i faces exceeds standard limit of %d.\n", count, MAX_WINQUAKE_MAP_FACES);

	//johnfitz

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, out++)
	{
		if (bsp2)
		{
			out->firstedge = LittleLong(inl->firstedge);
			out->numedges = LittleLong(inl->numedges);
			planenum = LittleLong(inl->planenum);
			side = LittleLong(inl->side);
			texinfon = LittleLong (inl->texinfo);
			for (i=0 ; i<MAXLIGHTMAPS ; i++)
				out->styles[i] = inl->styles[i];
			lofs = LittleLong(inl->lightofs);
			inl++;
		}
		else
		{
			out->firstedge = LittleLong(ins->firstedge);
			out->numedges = LittleShort(ins->numedges);
			planenum = LittleShort(ins->planenum);
			side = LittleShort(ins->side);
			texinfon = LittleShort (ins->texinfo);
			for (i=0 ; i<MAXLIGHTMAPS ; i++)
				out->styles[i] = ins->styles[i];
			lofs = LittleLong(ins->lightofs);
			ins++;
		}

		out->flags = 0;

		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + texinfon;

		CalcSurfaceExtents (out);

		Mod_CalcSurfaceBounds (out); //johnfitz -- for per-surface frustum culling

	// lighting info
		if (lofs == -1)
			out->samples = NULL;
		else if (loadmodel->bspversion == BSPVERSION_HALFLIFE)
			out->samples = loadmodel->lightdata + lofs;
		else
			out->samples = loadmodel->lightdata + (lofs * 3); //johnfitz -- lit support via lordhavoc (was "+ i")

		//johnfitz -- this section rewritten
		if (!Q_strncasecmp(out->texinfo->texture->name,"sky",3)) // sky surface //also note -- was Q_strncmp, changed to match qbsp
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			Mod_PolyForUnlitSurface (out); //no more subdivision
			continue;
		}
		else if (out->texinfo->texture->name[0] == '*') // warp surface
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);

			if (Is_Texture_Prefix (out->texinfo->texture->name, r_texprefix_tele.string))
				out->flags |= SURF_DRAWTELEPORTER; // *tele ... so that r_wateralpha doesn't affect teleporters.
			Mod_PolyForUnlitSurface (out);
			GL_SubdivideSurface (out);
			continue;
		}
		else if (out->texinfo->flags & TEX_MISSING) // texture is missing from bsp
		{
			if (out->samples) //lightmapped
			{
				out->flags |= SURF_NOTEXTURE;
			}
			else // not lightmapped
			{
				out->flags |= (SURF_NOTEXTURE | SURF_DRAWTILED);
				Mod_PolyForUnlitSurface (out);
			}
			continue;
		}

		// Normal texture
		if (Is_Texture_Prefix (out->texinfo->texture->name, r_texprefix_fence.string))
			out->flags |= SURF_DRAWFENCE;
		if (Is_Texture_Prefix (out->texinfo->texture->name, r_texprefix_envmap.string))
			out->flags |= SURF_DRAWENVMAP; // env_  Shiny
		if (Is_Texture_Prefix (out->texinfo->texture->name, r_texprefix_mirror.string))
			out->flags |= SURF_DRAWMIRROR; // mirror_
		if (Is_Texture_Prefix (out->texinfo->texture->name, r_texprefix_scrollx.string))
			out->flags |= SURF_SCROLLX; // scrollx_
		if (Is_Texture_Prefix (out->texinfo->texture->name, r_texprefix_scrolly.string))
			out->flags |= SURF_SCROLLY; // scrolly_

		//johnfitz
	}
}


/*
=================
Mod_SetParent
=================
*/
static void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;

	if (node->contents < 0)
		return;

	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes_S (lump_t *l)
{
	int			i, j, count, p;
	dsnode_t	*in;
	mnode_t 	*out;

	in = (dsnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadNodes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mnode_t *)Hunk_AllocName ( count*sizeof(*out), loadname);

	//johnfitz -- warn mappers about exceeding old limits
//	if (count > 32767)
//		Con_Warning ("%i nodes exceeds standard limit of 32767.\n", count);

	if (count > MAX_WINQUAKE_MAP_NODES)
		Con_Warning ("%i nodes exceeds standard limit of %d.\n", count, MAX_WINQUAKE_MAP_NODES);

	//johnfitz

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = (unsigned short)LittleShort (in->firstface); //johnfitz -- explicit cast as unsigned short
		out->numsurfaces = (unsigned short)LittleShort (in->numfaces); //johnfitz -- explicit cast as unsigned short

		for (j=0 ; j<2 ; j++)
		{
			//johnfitz -- hack to handle nodes > 32k, adapted from darkplaces
			p = (unsigned short)LittleShort(in->children[j]);
			if (p < count)
				out->children[j] = loadmodel->nodes + p;
			else
			{
				p = 0xffff /*65535*/ - p; //note this uses 65535 intentionally, -1 is leaf 0
				if (p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else
				{
					Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
					out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
				}
			}
			//johnfitz
		}
	}
}

static void Mod_LoadNodes_L1 (lump_t *l)
{
	int			i, j, count, p;
	dl1node_t	*in;
	mnode_t 	*out;

	in = (dl1node_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadNodes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mnode_t *)Hunk_AllocName ( count*sizeof(*out), loadname);

	//johnfitz -- warn mappers about exceeding old limits
//	if (count > 32767)
//		Con_Warning ("%i nodes exceeds standard limit of 32767.\n", count);

	if (count > MAX_WINQUAKE_MAP_NODES)
		Con_Warning ("%i nodes exceeds standard limit of %d.\n", count, MAX_WINQUAKE_MAP_NODES);

	//johnfitz

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleLong (in->firstface); //johnfitz -- explicit cast as unsigned short
		out->numsurfaces = LittleLong (in->numfaces); //johnfitz -- explicit cast as unsigned short

		for (j=0 ; j<2 ; j++)
		{
			//johnfitz -- hack to handle nodes > 32k, adapted from darkplaces
			p = LittleLong(in->children[j]);
			if (p >= 0 && p < count)
				out->children[j] = loadmodel->nodes + p;
			else
			{
				p = 0xffffffff - p; //note this uses 65535 intentionally, -1 is leaf 0
				if (p >= 0 && p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else
				{
					Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
					out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
				}
			}
			//johnfitz
		}
	}
}

static void Mod_LoadNodes_L2 (lump_t *l)
{
	int			i, j, count, p;
	dl2node_t	*in;
	mnode_t 	*out;

	in = (dl2node_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadNodes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mnode_t *)Hunk_AllocName ( count*sizeof(*out), loadname);

	//johnfitz -- warn mappers about exceeding old limits
//	if (count > 32767)
//		Con_Warning ("%i nodes exceeds standard limit of 32767.\n", count);

	if (count > MAX_WINQUAKE_MAP_NODES)
		Con_Warning ("%i nodes exceeds standard limit of %d.\n", count, MAX_WINQUAKE_MAP_NODES);

	//johnfitz

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleFloat (in->mins[j]);
			out->minmaxs[3+j] = LittleFloat (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleLong (in->firstface); //johnfitz -- explicit cast as unsigned short
		out->numsurfaces = LittleLong (in->numfaces); //johnfitz -- explicit cast as unsigned short

		for (j=0 ; j<2 ; j++)
		{
			//johnfitz -- hack to handle nodes > 32k, adapted from darkplaces
			p = LittleLong(in->children[j]);
			if (p > 0 && p < count)
				out->children[j] = loadmodel->nodes + p;
			else
			{
				p = 0xffffffff - p; //note this uses 65535 intentionally, -1 is leaf 0
				if (p > 0 && p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else
				{
					Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
					out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
				}
			}
			//johnfitz
		}
	}
}

static void Mod_LoadNodes (lump_t *l, int bsp2)
{
	if (bsp2 == 2)
		Mod_LoadNodes_L2(l);
	else if (bsp2)
		Mod_LoadNodes_L1(l);
	else
		Mod_LoadNodes_S(l);

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

static void Mod_ProcessLeafs_S (dsleaf_t *in, int filelen)
{
	mleaf_t 	*out;
	int			i, j, count, p;

	if (filelen % sizeof(*in))
		Host_Error ("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);

	count = filelen / sizeof(*in);

	out = (mleaf_t *) Hunk_AllocName (count * sizeof(*out), loadname);


	if (count > MAX_MAP_LEAFS)
		Host_Error ("Mod_LoadLeafs: %i leafs exceeds limit of %i.\n", count, MAX_MAP_LEAFS);
	if (count > MAX_WINQUAKE_MAP_LEAFS)
		Con_Warning ("Mod_LoadLeafs: %i leafs exceeds standard limit of %i.\n", count, MAX_WINQUAKE_MAP_LEAFS);

	loadmodel->leafs		= out;
	loadmodel->numleafs		= count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p					= LittleLong(in->contents);
		out->contents		= p;

		out->firstmarksurface = loadmodel->marksurfaces + (unsigned short)LittleShort(in->firstmarksurface); //johnfitz -- unsigned short
		out->nummarksurfaces = (unsigned short)LittleShort(in->nummarksurfaces); //johnfitz -- unsigned short

		p					= LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags			= NULL;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp. Baker: This marks the surface as underwater
		if (out->contents != CONTENTS_EMPTY)
		{
			for (j=0 ; j < out->nummarksurfaces ; j++)
			{
				msurface_t** surf = &out->firstmarksurface[j];
				if (!surf || !(*surf) || !(*surf)->texinfo || !(*surf)->texinfo->texture)
				{
					continue;
				}
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER; // This screws up?
			}
		}
	}

}


static void Mod_ProcessLeafs_L1 (dl1leaf_t *in, int filelen)
{
	mleaf_t 	*out;
	int			i, j, count, p;

	if (filelen % sizeof(*in))
		Host_Error ("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);

	count = filelen / sizeof(*in);

	out = (mleaf_t *) Hunk_AllocName (count * sizeof(*out), loadname);


	if (count > MAX_MAP_LEAFS)
		Host_Error ("Mod_LoadLeafs: %i leafs exceeds limit of %i.\n", count, MAX_MAP_LEAFS);
	if (count > MAX_WINQUAKE_MAP_LEAFS)
		Con_Warning ("Mod_LoadLeafs: %i leafs exceeds standard limit of %i.\n", count, MAX_WINQUAKE_MAP_LEAFS);

	loadmodel->leafs		= out;
	loadmodel->numleafs		= count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p					= LittleLong(in->contents);
		out->contents		= p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleLong(in->firstmarksurface); //johnfitz -- unsigned short
		out->nummarksurfaces = LittleLong(in->nummarksurfaces); //johnfitz -- unsigned short

		p					= LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags			= NULL;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp. Baker: This marks the surface as underwater
		if (out->contents != CONTENTS_EMPTY)
		{
			for (j=0 ; j < out->nummarksurfaces ; j++)
			{
				msurface_t** surf = &out->firstmarksurface[j];
				if (!surf || !(*surf) || !(*surf)->texinfo || !(*surf)->texinfo->texture)
				{
					continue;
				}
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER; // This screws up?
			}
		}
	}

}

static void Mod_ProcessLeafs_L2 (dl2leaf_t *in, int filelen)
{
	mleaf_t 	*out;
	int			i, j, count, p;

	if (filelen % sizeof(*in))
		Host_Error ("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);

	count = filelen / sizeof(*in);

	out = (mleaf_t *) Hunk_AllocName (count * sizeof(*out), loadname);


	if (count > MAX_MAP_LEAFS)
		Host_Error ("Mod_LoadLeafs: %i leafs exceeds limit of %i.\n", count, MAX_MAP_LEAFS);
	if (count > MAX_WINQUAKE_MAP_LEAFS)
		Con_Warning ("Mod_LoadLeafs: %i leafs exceeds standard limit of %i.\n", count, MAX_WINQUAKE_MAP_LEAFS);

	loadmodel->leafs		= out;
	loadmodel->numleafs		= count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleFloat (in->mins[j]);
			out->minmaxs[3+j] = LittleFloat (in->maxs[j]);
		}

		p					= LittleLong(in->contents);
		out->contents		= p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleLong(in->firstmarksurface); //johnfitz -- unsigned short
		out->nummarksurfaces = LittleLong(in->nummarksurfaces); //johnfitz -- unsigned short

		p					= LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags			= NULL;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp. Baker: This marks the surface as underwater
		if (out->contents != CONTENTS_EMPTY)
		{
			for (j=0 ; j < out->nummarksurfaces ; j++)
			{
				msurface_t** surf = &out->firstmarksurface[j];
				if (!surf || !(*surf) || !(*surf)->texinfo || !(*surf)->texinfo->texture)
				{
					continue;
				}
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER; // This screws up?
			}
		}
	}

}


/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs (lump_t *l, int bsp2)
{
	void 	*in = (void *)(mod_base + l->fileofs);

	if (bsp2 == 2)
		Mod_ProcessLeafs_L2 (in, l->filelen);
	else if (bsp2)
		Mod_ProcessLeafs_L1 (in, l->filelen);
	else
		Mod_ProcessLeafs_S (in, l->filelen);
}


/*
=================
Mod_LoadClipnodes
=================
*/
static void Mod_LoadClipnodes (lump_t *l, qboolean bsp2)
{
	dsclipnode_t *ins;
	dlclipnode_t *inl;

	mclipnode_t *out; //johnfitz -- was dclipnode_t
	int			i, count;
	hull_t		*hull;

	if (bsp2)
	{
		ins = NULL;
		inl = (dlclipnode_t *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*inl))
			Host_Error ("Mod_LoadClipnodes: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*inl);
	}
	else
	{
		ins = (dsclipnode_t *)(mod_base + l->fileofs);
		inl = NULL;
		if (l->filelen % sizeof(*ins))
			Host_Error ("Mod_LoadClipnodes: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*ins);
	}
	out = (mclipnode_t *)Hunk_AllocName ( count*sizeof(*out), loadname);

	//johnfitz -- warn about exceeding old limits
//	if (count > 32767)
//		Con_Warning ("%i clipnodes exceeds standard limit of 32767.\n", count);
	if (count > MAX_WINQUAKE_MAP_CLIPNODES)
		Con_Warning ("%i clipnodes exceeds standard limit of %d.\n", count, MAX_WINQUAKE_MAP_CLIPNODES);
	//johnfitz

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	// Player Hull
	hull = &loadmodel->hulls[1];
	{
		hull->available			= true;
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;

		VectorSet (hull->clip_mins, -16, -16, -24);
		VectorSet (hull->clip_maxs,  16,  16,  32);

		if (loadmodel->bspversion == BSPVERSION_HALFLIFE)
		{	// Player hull is taller
			hull->clip_mins[2]	= -36;
			hull->clip_maxs[2]	= 36;
		}
	}

	// Monster hull
	hull = &loadmodel->hulls[2];
	{
		hull->available			= true;
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;

		VectorSet (hull->clip_mins, -32, -32, -24);
		VectorSet (hull->clip_maxs,  32,  32,  64);

		if (loadmodel->bspversion == BSPVERSION_HALFLIFE)
		{ // Monster hull is shorter
			hull->clip_mins[2]	= -32;
			hull->clip_maxs[2]	= 32;
		}
	}

	// Half-Life crouch hull
	hull					= &loadmodel->hulls[3];
	{
		hull->clipnodes			= out;
		hull->firstclipnode		= 0;
		hull->lastclipnode		= count-1;
		hull->planes			= loadmodel->planes;

		VectorSet (hull->clip_mins, -16, -16,  -6);
		VectorSet (hull->clip_maxs,  16,  16,  30);

		hull->available			= false;

		if (loadmodel->bspversion == BSPVERSION_HALFLIFE)
		{
			hull->clip_mins[2]	= -18;
			hull->clip_maxs[2]	= 18;
			hull->available		= true;
		}
	}

	if (bsp2)
	{
		for (i=0 ; i<count ; i++, out++, inl++)
		{
			out->planenum = LittleLong(inl->planenum);

			//johnfitz -- bounds check
			if (out->planenum < 0 || out->planenum >= loadmodel->numplanes)
				Host_Error ("Mod_LoadClipnodes: planenum out of bounds");
			//johnfitz

			out->children[0] = LittleLong(inl->children[0]);
			out->children[1] = LittleLong(inl->children[1]);
			//Spike: FIXME: bounds check
		}
	}
	else
	{
		for (i=0 ; i<count ; i++, out++, ins++)
		{
			out->planenum = LittleLong(ins->planenum);

			//johnfitz -- bounds check
			if (out->planenum < 0 || out->planenum >= loadmodel->numplanes)
				Host_Error ("Mod_LoadClipnodes: planenum out of bounds");
			//johnfitz

			//johnfitz -- support clipnodes > 32k
			out->children[0] = (unsigned short)LittleShort(ins->children[0]);
			out->children[1] = (unsigned short)LittleShort(ins->children[1]);

			if (out->children[0] >= count)
				out->children[0] -= 65536;
			if (out->children[1] >= count)
				out->children[1] -= 65536;
			//johnfitz
		}
	}
}

/*
=================
Mod_MakeHull0

Duplicate the drawing hull structure as a clipping hull
=================
*/
static void Mod_MakeHull0 (void)
{
	hull_t		*hull = &loadmodel->hulls[0];
	mnode_t		*in = loadmodel->nodes;
	int			count = loadmodel->numnodes;
	mclipnode_t *out = (mclipnode_t *)Hunk_AllocName ( count*sizeof(*out), loadname);

	mnode_t		*child;
	int			i, j;

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j=0 ; j<2 ; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
static void Mod_LoadMarksurfaces (lump_t *l, int bsp2)
{
	int		i, j, count;
	msurface_t **out;
	if (bsp2)
	{
		unsigned int		*in = (unsigned int *)(mod_base + l->fileofs);

		if (l->filelen % sizeof(*in))
			Host_Error ("Mod_LoadMarksurfaces: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*in);
		out = (msurface_t **)Hunk_AllocName ( count*sizeof(*out), loadname);

		loadmodel->marksurfaces = out;
		loadmodel->nummarksurfaces = count;

		//johnfitz -- warn mappers about exceeding old limits
	//	if (count > 32767)
	//		Con_Warning ("%i marksurfaces exceeds standard limit of 32767.\n", count);

		if (count > MAX_WINQUAKE_MAP_MARKSURFACES)
			Con_Warning ("%i marksurfaces exceeds standard limit of %d.\n", count, MAX_WINQUAKE_MAP_MARKSURFACES);

		//johnfitz

		for ( i=0 ; i<count ; i++)
		{
			j = LittleLong(in[i]); //johnfitz -- explicit cast as unsigned short
			if (j >= loadmodel->numsurfaces)
				Host_Error ("Mod_LoadMarksurfaces: bad surface number");
			out[i] = loadmodel->surfaces + j;
		}
	}
	else
	{
		short		*in = (short *)(mod_base + l->fileofs);

		if (l->filelen % sizeof(*in))
			Host_Error ("Mod_LoadMarksurfaces: funny lump size in %s",loadmodel->name);
	
		count = l->filelen / sizeof(*in);
		out = (msurface_t **)Hunk_AllocName ( count*sizeof(*out), loadname);
	
		loadmodel->marksurfaces = out;
		loadmodel->nummarksurfaces = count;
	
		//johnfitz -- warn mappers about exceeding old limits
	//	if (count > 32767)
	//		Con_Warning ("%i marksurfaces exceeds standard limit of 32767.\n", count);
	
		if (count > MAX_WINQUAKE_MAP_MARKSURFACES)
			Con_Warning ("%i marksurfaces exceeds standard limit of %d.\n", count, MAX_WINQUAKE_MAP_MARKSURFACES);
	
		//johnfitz
	
		for ( i=0 ; i<count ; i++)
		{
			j = (unsigned short)LittleShort(in[i]); //johnfitz -- explicit cast as unsigned short
			if (j >= loadmodel->numsurfaces)
				Host_Error ("Mod_LoadMarksurfaces: bad surface number");
			out[i] = loadmodel->surfaces + j;
		}
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
static void Mod_LoadSurfedges (lump_t *l)
{
	int		*in = (int *)(mod_base + l->fileofs);

	int		i, count, *out;

	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadSurfedges: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (int *)Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
static void Mod_LoadPlanes (lump_t *l)
{
	int			i, j, count, bits;
	mplane_t	*out;
	dplane_t 	*in = (dplane_t *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadPlanes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mplane_t *)Hunk_AllocName ( count*2*sizeof(*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

// 2001-12-28 .VIS support by Maddes  start
/*
=================
Mod_LoadExternalVisibility
=================
*/
static void Mod_LoadVisibilityExternal (FILE **fhandle)
{
	long	filelen=0;;

	// get visibility data length
	filelen = 0;
	fread (&filelen, 1, 4, *fhandle);
	filelen = LittleLong(filelen);

	Con_DPrintf ("...%i bytes visibility data\n", filelen);

	// load visibility data
	if (!filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Hunk_AllocName (filelen, "EXT_VIS");
	fread (loadmodel->visdata, 1, filelen, *fhandle);
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength (corner);
}

/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in, *out;
	int			i, j, count;

	in = (dmodel_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadSubmodels: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	if (count > MAX_FITZQUAKE_MODELS)
		Host_Error ("Mod_LoadSubmodels: count > MAX_MODELS");

	out = (dmodel_t *)Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}

	// johnfitz -- check world visleafs -- adapted from bjp
	out = loadmodel->submodels;

	if (out->visleafs > MAX_MAP_LEAFS)
		Host_Error ("Mod_LoadSubmodels: too many visleafs (%d, max = %d) in %s", out->visleafs, MAX_MAP_LEAFS, loadmodel->name);

//	if (out->visleafs > 8192)
//		Con_Warning ("%i visleafs exceeds standard limit of 8192.\n", out->visleafs);

	if (out->visleafs > MAX_WINQUAKE_MAP_LEAFS)
		Con_Warning ("%i visleafs exceeds standard limit of %i.\n", out->visleafs, MAX_WINQUAKE_MAP_LEAFS);

	//johnfitz
}


/*
=================
Mod_LoadExternalLeafs
=================
*/
static void Mod_LoadLeafsExternal (FILE **fhandle)
{
	dsleaf_t 	*in;
	long	filelen;

	// get leaf data length
	filelen = 0;
	fread (&filelen, 1, 4, *fhandle);
	filelen = LittleLong(filelen);

	Con_DPrintf("...%i bytes leaf data\n", filelen);

	// load leaf data
	if (!filelen)
	{
		loadmodel->leafs = NULL;
		loadmodel->numleafs = 0;
		return;
	}
	in = Hunk_AllocName (filelen, "EXT_LEAF");
	fread  (in, 1, filelen, *fhandle);

	Mod_ProcessLeafs_S (in, filelen);
}

// 2001-12-28 .VIS support by Maddes  start
#define VISPATCH_MAPNAME_LENGTH	32

typedef struct vispatch_s
{
	char	mapname[VISPATCH_MAPNAME_LENGTH];	// Baker: DO NOT CHANGE THIS to MAX_QPATH, must be 32
	int		filelen;		// length of data after VisPatch header (VIS+Leafs)
} vispatch_t;
// 2001-12-28 .VIS support by Maddes  end
#define VISPATCH_HEADER_LEN_36 36

static qboolean Mod_FindVisibilityExternal (FILE **filehandle)
{
	char		visfilename[MAX_QPATH];
	q_snprintf (visfilename, sizeof(visfilename), "maps/%s.vis", loadname);
	COM_FOpenFile_Limited (visfilename, filehandle, loadmodel->loadinfo.searchpath); // COM_FOpenFile (name, &cls.demofile);

	if (!*filehandle)
	{
		Con_DPrintf ("Standard vis, %s not found\n", visfilename);
		return false; // None
	}

	Con_Printf ("External .vis found: %s\n", visfilename);
	{
		int			i, pos;
		vispatch_t	header;

		pos = 0;

		while ((i = fread (&header, 1, VISPATCH_HEADER_LEN_36, *filehandle))) // i will be length of read, continue while a read
		{
			header.filelen = LittleLong(header.filelen);	// Endian correct header.filelen
			pos += i;										// Advance the length of the break

			if (!Q_strcasecmp (header.mapname, loadmodel->name));	// found, get out
			{
				break;
			}

			pos += header.filelen;							// Advance the length of the filelength
			fseek (*filehandle, pos, SEEK_SET);
		}

		if (i != VISPATCH_HEADER_LEN_36)
		{
			FS_fclose (*filehandle);
			return false;
		}
	}

	return true;

}
// 2001-12-28 .VIS support by Maddes  end


/*
=================
Mod_BoundsFromClipNode -- johnfitz

update the model's clipmins and clipmaxs based on each node's plane.

This works because of the way brushes are expanded in hull generation.
Each brush will include all six axial planes, which bound that brush.
Therefore, the bounding box of the hull can be constructed entirely
from axial planes found in the clipnodes for that hull.
=================
*/
void Mod_BoundsFromClipNode (qmodel_t *mod, int hull, int nodenum)
{
	mplane_t	*plane;
	mclipnode_t	*node;

	if (nodenum < 0)
		return; //hit a leafnode

	node = &mod->clipnodes[nodenum];
	plane = mod->hulls[hull].planes + node->planenum;
	switch (plane->type)
	{

	case PLANE_X:
		if (plane->signbits == 1)
			mod->clipmins[0] = q_min (mod->clipmins[0], -plane->dist - mod->hulls[hull].clip_mins[0]);
		else
			mod->clipmaxs[0] = q_max (mod->clipmaxs[0], plane->dist - mod->hulls[hull].clip_maxs[0]);
		break;
	case PLANE_Y:
		if (plane->signbits == 2)
			mod->clipmins[1] = q_min (mod->clipmins[1], -plane->dist - mod->hulls[hull].clip_mins[1]);
		else
			mod->clipmaxs[1] = q_max (mod->clipmaxs[1], plane->dist - mod->hulls[hull].clip_maxs[1]);
		break;
	case PLANE_Z:
		if (plane->signbits == 4)
			mod->clipmins[2] = q_min (mod->clipmins[2], -plane->dist - mod->hulls[hull].clip_mins[2]);
		else
			mod->clipmaxs[2] = q_max (mod->clipmaxs[2], plane->dist - mod->hulls[hull].clip_maxs[2]);
		break;
	default:
		//skip nonaxial planes; don't need them
		break;
	}

	Mod_BoundsFromClipNode (mod, hull, node->children[0]);
	Mod_BoundsFromClipNode (mod, hull, node->children[1]);
}

/*
=================
Mod_LoadBrushModel
=================
*/

void Mod_LoadBrushModel (qmodel_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	dmodel_t 	*bm;
	float		radius; //johnfitz
	int			bsp2;

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;

	mod->bspversion = LittleLong (header->version);

	switch(mod->bspversion)
	{
	case BSPVERSION:
	case BSPVERSION_HALFLIFE:
		bsp2 = false;
		break;
	case BSP2VERSION_2PSB:
		bsp2 = 1;	//first iteration
		break;
	case BSP2VERSION_BSP2:
		bsp2 = 2;	//sanitised revision
		break;
	default:
		Host_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i (Quake) or %i (HalfLife))", mod->name, mod->bspversion, BSPVERSION, BSPVERSION_HALFLIFE);
		break;
	}

	{ // cl.worldmodel
		qboolean servermatch = sv.modelname[0] && !Q_strcasecmp (loadname, sv.name);
		qboolean clientmatch = cl.worldname[0] && !Q_strcasecmp (loadname, cl.worldname);
		Con_DPrintf ("%s loadname\n", loadname);
		Con_DPrintf ("%s sv.modelname\n", sv.modelname);
		Con_DPrintf ("%s cl.modelname\n", cl.worldname);
		loadmodel->isworldmodel = servermatch || clientmatch;
	}

// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i< (int)sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES], bsp2);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES], bsp2);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES], bsp2);

	do
	{
		// 2001-12-28 .VIS support by Maddes  start
		FILE		*visfhandle;	// Baker: try to localize this var

		loadmodel->visdata = NULL;
		loadmodel->leafs = NULL;
		loadmodel->numleafs = 0;

		if (loadmodel->isworldmodel && model_external_vis.value)
		{
			Con_DPrintf ("trying to open external vis file\n");

			if (Mod_FindVisibilityExternal (&visfhandle /* We should be passing infos here &visfilehandle*/) )
			{
				// File exists, valid and open
				Con_DPrintf ("found valid external .vis file for map\n");
				Mod_LoadVisibilityExternal (&visfhandle);
				Mod_LoadLeafsExternal (&visfhandle);

				FS_fclose  (visfhandle);

				if (loadmodel->visdata && loadmodel->leafs && loadmodel->numleafs)
				{
					break; // skip standard vis
				}
				Con_Printf("External VIS data are invalid! Doing standard vis.\n");
			}
		}
		// Extern vis didn't exist or was invalid ..

		//
		// standard vis
		//
		Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
		Mod_LoadLeafs (&header->lumps[LUMP_LEAFS], bsp2);
	} while (0);

	Mod_LoadNodes (&header->lumps[LUMP_NODES], bsp2);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES], bsp2);
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();

	mod->numframes = 2;		// regular and alternate animation

//
// set up the submodels (FIXME: this is confusing)
//

	// johnfitz -- okay, so that i stop getting confused every time i look at this loop, here's how it works:
	// we're looping through the submodels starting at 0.  Submodel 0 is the main model, so we don't have to
	// worry about clobbering data the first time through, since it's the same data.  At the end of the loop,
	// we create a new copy of the data to use the next time through.
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		//johnfitz -- calculate rotate bounds and yaw bounds
		radius = RadiusFromBounds (mod->mins, mod->maxs);
		mod->rmaxs[0] = mod->rmaxs[1] = mod->rmaxs[2] = mod->ymaxs[0] = mod->ymaxs[1] = mod->ymaxs[2] = radius;
		mod->rmins[0] = mod->rmins[1] = mod->rmins[2] = mod->ymins[0] = mod->ymins[1] = mod->ymins[2] = -radius;
		//johnfitz

		//johnfitz -- correct physics cullboxes so that outlying clip brushes on doors and stuff are handled right
		if (i > 0 || strcmp(mod->name, sv.modelname) != 0) //skip submodel 0 of sv.worldmodel, which is the actual world
		{
			// start with the hull0 bounds
			VectorCopy (mod->maxs, mod->clipmaxs);
			VectorCopy (mod->mins, mod->clipmins);

			// process hull1 (we don't need to process hull2 becuase there's
			// no such thing as a brush that appears in hull2 but not hull1)
			//Mod_BoundsFromClipNode (mod, 1, mod->hulls[1].firstclipnode); // (disabled for now becuase it fucks up on rotating models)
		}
		//johnfitz

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[10];

			q_snprintf (name, sizeof(name), "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

byte		**player_8bit_texels_tbl;
byte		*player_8bit_texels;

/*
=================
Mod_LoadAliasFrame
=================
*/
void * Mod_LoadAliasFrame (void * pin, maliasframedesc_t *frame)
{
	trivertx_t		*pinframe;
	int				i;
	daliasframe_t	*pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	strcpy (frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i=0 ; i<3 ; i++)
	{
		// these are byte values, so we don't have to worry about
		// endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];
	}


	pinframe = (trivertx_t *)(pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void *Mod_LoadAliasGroup (void * pin,  maliasframedesc_t *frame)
{
	daliasgroup_t		*pingroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	void				*ptemp;

	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i=0 ; i<3 ; i++)
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];
	}


	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		poseverts[posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		posenum++;

		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

//=========================================================


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
do {								\
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
} while (0)

void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}


char *texpaths[] = {"progs/", NULL};

void Mod_LoadSkinTexture (gltexture_t **tex, gltexture_t **fb, char *name, char *fbr_mask_name, char *extname, byte *data, int width, int height, unsigned int offset)
{
	int i;
	char extload[MAX_OSPATH] = {0};
	int extwidth = 0;
	int extheight = 0;
	byte *extdata;
	int hunkmark = Hunk_LowMark ();

	if (external_textures.value)
	{
		for (i = 0; ; i++)
	
		{
	
			// no more paths
			if (!texpaths[i]) break;
	
			q_snprintf (extload, sizeof(extload), "%s%s", texpaths[i], extname);
	
	
	
	
	
			
	
	
	
	
			extdata = Image_LoadImage_Limited (extload, &extwidth, &extheight, loadmodel->loadinfo.searchpath);
	
	
			if (extdata)
			{
				tex[0] = TexMgr_LoadImage (loadmodel, -1 /*not bsp texture*/, extload, extwidth, extheight, SRC_RGBA, extdata, extload, 0,  /*TEXPREF_PAD |*/ TEXPREF_NOBRIGHT | TEXPREF_FLOODFILL);
	
				fb[0] = NULL;
	
				// now try to load glow/luma image from the same place
				Hunk_FreeToLowMark (hunkmark);
	
				q_snprintf (extload, sizeof(extload), "%s%s_glow", texpaths[i], extname);
				extdata = Image_LoadImage_Limited (extload, &extwidth, &extheight, loadmodel->loadinfo.searchpath);
	
				if (!extdata)
				{
					q_snprintf (extload, sizeof(extload), "%s%s_luma", texpaths[i], extname);
					extdata = Image_LoadImage_Limited (extload, &extwidth, &extheight, loadmodel->loadinfo.searchpath);
				}
	
				if (extdata)
				{
	#ifdef FIX_LUMA_TEXTURE	 // Baker change
						fb[0] = TexMgr_LoadImage (loadmodel, -1 /*not bsp texture*/, extload, extwidth, extheight, SRC_RGBA, extdata, extload, 0,  /*TEXPREF_PAD |*/ TEXPREF_NOBRIGHT /*  | TEXPREF_FLOODFILL*/);
	#else // Baker change +
					fb[0] = TexMgr_LoadImage (loadmodel, -1 /*not bsp texture*/, extload, width, height, SRC_RGBA, extdata, extload, 0,  TEXPREF_PAD | TEXPREF_NOBRIGHT  | TEXPREF_FLOODFILL);
	#endif // Baker change -
	
	
					Hunk_FreeToLowMark (hunkmark);
	
	
				}
	
	
				return;
			}
		}
	}
	// hurrah! for readable code
	if (Mod_CheckFullbrights (data, width * height, false))
	{
		tex[0] = TexMgr_LoadImage (loadmodel, -1 /*not bsp texture*/, name, width, height, SRC_INDEXED, data, loadmodel->name, offset, TEXPREF_PAD | TEXPREF_NOBRIGHT);
		fb[0] = TexMgr_LoadImage (loadmodel, -1 /*not bsp texture*/, fbr_mask_name, width, height, SRC_INDEXED, data, loadmodel->name, offset, TEXPREF_PAD | TEXPREF_FULLBRIGHT);
	}
	else
	{
		tex[0] = TexMgr_LoadImage (loadmodel, -1 /*not bsp texture*/, name, width, height, SRC_INDEXED, data, loadmodel->name, offset, TEXPREF_PAD);
		fb[0] = NULL;
	}

	// just making sure...
	Hunk_FreeToLowMark (hunkmark);
}

/*
===============
Mod_LoadAllSkins
===============
*/


void *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype)
{
	int						i, j, k, size, groupskins;
	char					name[MAX_OSPATH];	// 32 this isn;t long enough for some mods
	byte					*skin, *texels;
	daliasskingroup_t		*pinskingroup;
	daliasskininterval_t	*pinskinintervals;
	char					fbr_mask_name[MAX_OSPATH]; // johnfitz -- added for fullbright support
	char					extname[MAX_OSPATH];
	src_offset_t			offset; //johnfitz

	skin = (byte *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Host_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	size = pheader->skinwidth * pheader->skinheight;

	for (i=0 ; i<numskins ; i++)
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE)
		{
//			Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );

			// save 8 bit texels for the player model to remap
			texels = (byte *) Hunk_AllocName(size, loadname);
			pheader->texels[i] = texels - (byte *)pheader;
			memcpy (texels, (byte *)(pskintype + 1), size);

			//johnfitz -- rewritten
			q_snprintf (name, sizeof(name), "%s:frame%i", loadmodel->name, i);
			offset = (src_offset_t) (pskintype + 1) - (src_offset_t) mod_base;
			q_snprintf (extname, sizeof(extname), "%s_%i", loadmodel->name, i);
			q_snprintf (fbr_mask_name, sizeof(fbr_mask_name), "%s:frame%i_glow", loadmodel->name, i);


			Mod_LoadSkinTexture
			(
				&pheader->gltextures[i][0],
				&pheader->fbtextures[i][0],
				name,
				fbr_mask_name,
				&extname[6],
				(byte *) (pskintype + 1),
				pheader->skinwidth,
				pheader->skinheight,
				offset
			);

			pheader->gltextures[i][3] = pheader->gltextures[i][2] = pheader->gltextures[i][1] = pheader->gltextures[i][0];
			pheader->fbtextures[i][3] = pheader->fbtextures[i][2] = pheader->fbtextures[i][1] = pheader->fbtextures[i][0];
			//johnfitz

			pskintype = (daliasskintype_t *)((byte *)(pskintype+1) + size);
		}
		else
		{
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (daliasskintype_t *)(pinskinintervals + groupskins);

			for (j=0 ; j<groupskins ; j++)
			{
//				Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );
				if (j == 0) 
				{
					texels = (byte *)Hunk_AllocName(size, loadname);
					pheader->texels[i] = texels - (byte *)pheader;
					memcpy (texels, (byte *)(pskintype), size);
				}

				//johnfitz -- rewritten
				q_snprintf (name, sizeof(name), "%s:frame%i_%i", loadmodel->name, i,j);
				offset = (src_offset_t)(pskintype) - (src_offset_t)mod_base; //johnfitz
				q_snprintf (extname, sizeof(extname), "%s_%i_%i", loadmodel->name, i, j);
				q_snprintf (fbr_mask_name, sizeof(fbr_mask_name), "%s:frame%i_%i_glow", loadmodel->name, i, j);

				Mod_LoadSkinTexture
				(
					&pheader->gltextures[i][j & 3],
					&pheader->fbtextures[i][j & 3],
					name,
					fbr_mask_name,
					extname,
					(byte *) (pskintype),
					pheader->skinwidth,
					pheader->skinheight,
					offset
				);
				//johnfitz

				pskintype = (daliasskintype_t *)((byte *)(pskintype) + size);
			}
			k = j;
			for (/* */; j < 4; j++)
				pheader->gltextures[i][j&3] =
				pheader->gltextures[i][j - k];
		}
	}

	return (void *)pskintype;
}

//=========================================================================

/*
=================
Mod_CalcAliasBounds -- johnfitz -- calculate bounds of alias model for nonrotated, yawrotated, and fullrotated cases
=================
*/
void Mod_CalcAliasBounds (aliashdr_t *a)
{
	int			i,j,k;
	float		dist, yawradius, radius;
	vec3_t		v;

	//clear out all data
	for (i=0; i<3;i++)
	{
		loadmodel->mins[i] = loadmodel->ymins[i] = loadmodel->rmins[i] = 999999;
		loadmodel->maxs[i] = loadmodel->ymaxs[i] = loadmodel->rmaxs[i] = -999999;
		radius = yawradius = 0;
	}

	//process verts
	for (i=0 ; i<a->numposes; i++)
		for (j=0; j<a->numverts; j++)
		{
			for (k=0; k<3;k++)
				v[k] = poseverts[i][j].v[k] * pheader->scale[k] + pheader->scale_origin[k];

			for (k=0; k<3;k++)
			{
				loadmodel->mins[k] = q_min (loadmodel->mins[k], v[k]);
				loadmodel->maxs[k] = q_max (loadmodel->maxs[k], v[k]);
			}
			dist = v[0] * v[0] + v[1] * v[1];
			if (yawradius < dist)
				yawradius = dist;
			dist += v[2] * v[2];
			if (radius < dist)
				radius = dist;
		}

	//rbounds will be used when entity has nonzero pitch or roll
	radius = sqrt(radius);
	loadmodel->rmins[0] = loadmodel->rmins[1] = loadmodel->rmins[2] = -radius;
	loadmodel->rmaxs[0] = loadmodel->rmaxs[1] = loadmodel->rmaxs[2] = radius;

	//ybounds will be used when entity has nonzero yaw
	yawradius = sqrt(yawradius);
	loadmodel->ymins[0] = loadmodel->ymins[1] = -yawradius;
	loadmodel->ymaxs[0] = loadmodel->ymaxs[1] = yawradius;
	loadmodel->ymins[2] = loadmodel->mins[2];
	loadmodel->ymaxs[2] = loadmodel->maxs[2];
}

/*
=================
Mod_SetExtraFlags -- johnfitz -- set up extra flags that aren't in the mdl
=================
*/
void Mod_SetExtraFlags (qmodel_t *mod)
{
	if (!mod || !mod->name || mod->type != mod_alias)
		return;

	mod->flags &= 0xFF; //only preserve first byte

	// nolerp flag
	if (COM_ListMatch (r_nolerp_list.string, mod->name) == true)
		mod->flags |= MOD_NOLERP;


	if (COM_ListMatch (r_noshadow_list.string, mod->name) == true)
		mod->flags |= MOD_NOSHADOW;

	if (COM_ListMatch (r_fbrighthack_list.string, mod->name) == true)
		mod->flags |= MOD_FBRIGHTHACK;


	if (COM_ListMatch (r_nocolormap_list.string, mod->name) == true)
		mod->flags |= MOD_NOCOLORMAP;

}

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (qmodel_t *mod, void *buffer)
{
	int					i, j;
	mdl_t				*pinmodel;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	int					version, numframes;
	int					size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	int					start, end, total;

	start = Hunk_LowMark ();

	pinmodel = (mdl_t *)buffer;
	mod_base = (byte *)buffer; //johnfitz

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = 	sizeof (aliashdr_t) +
			(LittleLong (pinmodel->numframes) - 1) * sizeof (pheader->frames[0]);
	pheader = (aliashdr_t *)Hunk_AllocName (size, loadname);

	mod->flags = LittleLong (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	pheader->numskins = LittleLong (pinmodel->numskins);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Host_Error ("model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	pheader->numverts = LittleLong (pinmodel->numverts);

	if (pheader->numverts <= 0)
		Host_Error ("model %s has no vertices", mod->name);

	if (pheader->numverts > MAXALIASVERTS)
		Host_Error ("model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong (pinmodel->numtris);

	if (pheader->numtris <= 0)
		Host_Error ("model %s has no triangles", mod->name);

	pheader->numframes = LittleLong (pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Host_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = (synctype_t) LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}


//
// load the skins
//
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = (daliasskintype_t *) Mod_LoadAllSkins (pheader->numskins, pskintype);

//
// load base s and t vertices
//
	pinstverts = (stvert_t *)pskintype;

	for (i=0 ; i<pheader->numverts ; i++)
	{
		stverts[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts[i].s = LittleLong (pinstverts[i].s);
		stverts[i].t = LittleLong (pinstverts[i].t);
	}

//
// load triangle lists
//
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];

	for (i=0 ; i<pheader->numtris ; i++)
	{
		triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j=0 ; j<3 ; j++)
		{
			triangles[i].vertindex[j] =
					LittleLong (pintriangles[i].vertindex[j]);
		}
	}

//
// load the frames
//
	posenum = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	for (i=0 ; i<numframes ; i++)
	{
		aliasframetype_t	frametype;
		frametype = (aliasframetype_t) LittleLong (pframetype->type);
		if (frametype == ALIAS_SINGLE)
			pframetype = (daliasframetype_t *) Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
		else
			pframetype = (daliasframetype_t *) Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);
	}

	pheader->numposes = posenum;

	mod->type = mod_alias;

	Mod_SetExtraFlags (mod); //johnfitz

	Mod_CalcAliasBounds (pheader); //johnfitz

	//
	// build the draw lists
	//
	GL_MakeAliasModelDisplayLists (mod, pheader);

//
// move the complete, relocatable alias model to the cache
//
	end = Hunk_LowMark ();
	total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}

//=============================================================================

/*
=================
Mod_LoadSpriteFrame
=================
*/
void * Mod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	int					width, height, size, origin[2];
	char				name[64];
	src_offset_t			offset; //johnfitz

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = (mspriteframe_t *)Hunk_AllocName (sizeof (mspriteframe_t),loadname);
	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	//johnfitz -- image might be padded
	pspriteframe->smax = (float)width/(float)TexMgr_PadConditional(width);
	pspriteframe->tmax = (float)height/(float)TexMgr_PadConditional(height);
	//johnfitz

	q_snprintf (name, sizeof(name), "%s:frame%i", loadmodel->name, framenum);
	offset = (src_offset_t)(pinframe+1) - (src_offset_t)mod_base; //johnfitz
	pspriteframe->gltexture =
		TexMgr_LoadImage (loadmodel, -1 /*not bsp texture*/, name, width, height, SRC_INDEXED,
			(byte *)(pinframe + 1), loadmodel->name, offset,
			TEXPREF_PAD | TEXPREF_ALPHA | TEXPREF_NOPICMIP); //johnfitz -- TexMgr

	return (void *)((byte *)pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void * Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = (mspritegroup_t *) Hunk_AllocName (sizeof (mspritegroup_t) +
				(numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = (float *) Hunk_AllocName (numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_Error ("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i);
	}

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (qmodel_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;

	pin = (dsprite_t *)buffer;
	mod_base = (byte *)buffer; //johnfitz

	version = LittleLong (pin->version);
	if (version != SPRITE_VERSION)
		Host_Error ("%s has wrong version number "
				 "(%i should be %i)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = (msprite_t *) Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = (synctype_t) LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

//
// load the frames
//
	if (numframes < 1)
		Host_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = (spriteframetype_t) LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteFrame (pframetype + 1, &psprite->frames[i].frameptr, i);
		}
		else
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteGroup (pframetype + 1, &psprite->frames[i].frameptr, i);
		}
	}

	mod->type = mod_sprite;
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print (void)
{
	int		i;
	qmodel_t	*mod;

	Con_SafePrintf ("Cached models:\n"); //johnfitz -- safeprint instead of print
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		Con_SafePrintf ("%8p : %s\n", mod->cache.data, mod->name); //johnfitz -- safeprint instead of print
	}
	Con_Printf ("%i models\n",mod_numknown); //johnfitz -- print the total too
}


/*
================
Mod_Print
================
*/
void Mod_PrintEx (void)
{
	int		i, count;
	qmodel_t	*mod;

	Con_SafePrintf ("Cached models:\n"); //johnfitz -- safeprint instead of print
	for (i = 0, mod = mod_known, count = 0 ; i < mod_numknown ; i++, mod++)
	{
		if (mod->cache.data == NULL)
			continue; // It isn't loaded
		
		if (mod->name[0] == '*')
			continue; // Don't list map submodels

		count++;
		Con_SafePrintf ("%-3i : %s\n", count, mod->name); //johnfitz -- safeprint instead of print
	}
}



void External_Textures_Change_f  (cvar_t* var)
{
	int i;
	
	if (!cl.worldmodel)
		return;

	Mod_Brush_ReloadTextures (cl.worldmodel);

	for (i = 1; i < MAX_FITZQUAKE_MODELS && cl.model_precache[i] ; i++)
	{	
		qmodel_t* mod = cl.model_precache[i];
		
		switch (mod->type)
		{
		case mod_brush:
			// If world submodel, it shares textures with world --- no need so skip
			if (mod->name[0] != '*')
				Mod_Brush_ReloadTextures (mod);
			break;
		default:
			// Not supported at this time (alias, sprites)
			break;
		}
	}

	// Handles the alias models
	Cache_Flush (); 
#pragma message ("Do we say anything about gamedir change required")
}
