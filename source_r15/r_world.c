/* Versus Quakespasm:

1. Dynamic light speed up
2. Teleporter excluded from r_wateralpha hack.
3. Support for various texture types

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
// r_world.c: world model rendering

#include "quakedef.h"


//extern glpoly_t	*lightmap_polys[MAX_FITZQUAKE_LIGHTMAPS];

byte *SV_FatPVS (vec3_t org, qmodel_t *worldmodel);
extern byte mod_novis[MAX_MAP_LEAFS/8];
int vis_changed; //if true, force pvs to be refreshed

//==============================================================================
//
// SETUP CHAINS
//
//==============================================================================

/*
===============
R_MarkSurfaces -- johnfitz -- mark surfaces based on PVS and rebuild texture chains
===============
*/
void R_MarkSurfaces (void)
{
	byte		*vis;
	mleaf_t		*leaf;
	mnode_t		*node;
	msurface_t	*surf, **mark;
	int			i, j;
	qboolean	nearwaterportal;

	// clear lightmap chains
	for (i=0; i < MAX_FITZQUAKE_LIGHTMAPS; i++)
		lightmap[i].polys = NULL;

#ifdef SUPPORTS_MIRRORS // Baker change
// mirror:  5. (DONE) If there is a mirror.  We don't update the framecount or set the old leaf or recalculate vis or mark visible leafs
// This only happens AFTER we've been here once to notice (possibly below)
	if (mirror_in_scene)
//		return;
	goto skip_for_mirror;
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change

	// check this leaf for water portals
	// TODO: loop through all water surfs and use distance to leaf cullbox
	nearwaterportal = false;
	for (i=0, mark = r_viewleaf->firstmarksurface; i < r_viewleaf->nummarksurfaces; i++, mark++)
		if ((*mark)->flags & SURF_DRAWTURB)
			nearwaterportal = true;

	// choose vis data
	if (r_novis.value || r_viewleaf->contents == CONTENTS_SOLID || r_viewleaf->contents == CONTENTS_SKY)
		vis = &mod_novis[0];
	else if (nearwaterportal)
		vis = SV_FatPVS (r_origin, cl.worldmodel);
	else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	// if surface chains don't need regenerating, just add static entities and return
	if (r_oldviewleaf == r_viewleaf && !vis_changed && !nearwaterportal)
	{
		leaf = &cl.worldmodel->leafs[1];
		for (i=0 ; i<cl.worldmodel->numleafs ; i++, leaf++)
			if (vis[i>>3] & (1<<(i&7)))
				if (leaf->efrags)
					R_StoreEfrags (&leaf->efrags);
		return;
	}

	vis_changed = false;
	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	// iterate through leaves, marking surfaces
	leaf = &cl.worldmodel->leafs[1];
	for (i=0 ; i<cl.worldmodel->numleafs ; i++, leaf++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			if (r_oldskyleaf.value || leaf->contents != CONTENTS_SKY)
				for (j=0, mark = leaf->firstmarksurface; j<leaf->nummarksurfaces; j++, mark++)
					(*mark)->visframe = r_visframecount;

			// add static models
			if (leaf->efrags)
				R_StoreEfrags (&leaf->efrags);
		}
	}

#ifdef SUPPORTS_MIRRORS // Baker change
skip_for_mirror:
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change

	// set all chains to null
	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
		if (cl.worldmodel->textures[i])
 			cl.worldmodel->textures[i]->texturechain = NULL;

	// rebuild chains

#if 1
	//iterate through surfaces one node at a time to rebuild chains
	//need to do it this way if we want to work with tyrann's skip removal tool
	//becuase his tool doesn't actually remove the surfaces from the bsp surfaces lump
	//nor does it remove references to them in each leaf's marksurfaces list
	for (i=0, node = cl.worldmodel->nodes ; i<cl.worldmodel->numnodes ; i++, node++)
		for (j=0, surf=&cl.worldmodel->surfaces[node->firstsurface] ; j<node->numsurfaces ; j++, surf++)
			if (surf->visframe == r_visframecount)
			{
#ifdef SUPPORTS_MIRRORS // Baker change
// Mirror: 4 (DONE)
// If surface is a mirror and we are in mirror scene draw, do not add to the chain
				if (mirror_in_scene && surf->texinfo->texture != cl.worldmodel->textures[mirrortexturenum])
					continue;
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change
				surf->texturechain = surf->texinfo->texture->texturechain;
				surf->texinfo->texture->texturechain = surf;
			}
#else
	//the old way
	surf = &cl.worldmodel->surfaces[cl.worldmodel->firstmodelsurface];
	for (i=0 ; i<cl.worldmodel->nummodelsurfaces ; i++, surf++)
	{
		if (surf->visframe == r_visframecount)
		{
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
		}
	}
#endif
}

/*
================
R_BackFaceCull -- johnfitz -- returns true if the surface is facing away from vieworg
================
*/
qboolean R_BackFaceCull (msurface_t *surf)
{
	double dot;

	switch (surf->plane->type)
	{
	case PLANE_X:
		dot = r_refdef.vieworg[0] - surf->plane->dist;
		break;
	case PLANE_Y:
		dot = r_refdef.vieworg[1] - surf->plane->dist;
		break;
	case PLANE_Z:
		dot = r_refdef.vieworg[2] - surf->plane->dist;
		break;
	default:
		dot = DotProduct (r_refdef.vieworg, surf->plane->normal) - surf->plane->dist;
		break;
	}

	if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
		return true;

	return false;
}

/*
================
R_CullSurfaces -- johnfitz
================
*/
void R_CullSurfaces (void)
{
	msurface_t *s;
	int i;

	if (!r_drawworld_cheatsafe)
		return;

	s = &cl.worldmodel->surfaces[cl.worldmodel->firstmodelsurface];
	for (i=0 ; i<cl.worldmodel->nummodelsurfaces ; i++, s++)
	{
		if (s->visframe == r_visframecount)
		{
			if (R_CullBox(s->mins, s->maxs) || R_BackFaceCull (s))
				s->culled = true;
			else
			{
				s->culled = false;
				rs_brushpolys++; //count wpolys here
				if (s->texinfo->texture->warpimage)
					s->texinfo->texture->update_warp = true;
			}
		}
	}
}

/*
================
R_BuildLightmapChains -- johnfitz -- used for r_lightmap 1
================
*/
void R_BuildLightmapChains (void)
{
	msurface_t *s;
	int i;

	// clear lightmap chains (already done in r_marksurfaces, but clearing them here to be safe becuase of r_stereo)
	for (i=0; i < MAX_FITZQUAKE_LIGHTMAPS; i++)
		lightmap[i].polys = NULL;

	// now rebuild them
	s = &cl.worldmodel->surfaces[cl.worldmodel->firstmodelsurface];
	for (i=0 ; i<cl.worldmodel->nummodelsurfaces ; i++, s++)
		if (s->visframe == r_visframecount && !R_CullBox(s->mins, s->maxs) && !R_BackFaceCull (s))
			R_RenderDynamicLightmaps (s);
}

//==============================================================================
//
// DRAW CHAINS
//
//==============================================================================

/*
================
R_DrawTextureChains_ShowTris -- johnfitz
================
*/
void R_DrawTextureChains_ShowTris (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	glpoly_t	*p;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;

		if (r_oldwater.value && t->texturechain && (t->texturechain->flags & SURF_DRAWTURB))
		{
			for (s = t->texturechain; s; s = s->texturechain)
				if (!s->culled)
					for (p = s->polys->next; p; p = p->next)
					{
						DrawGLTriangleFan (p, s->flags);
					}
		}
		else
		{
			for (s = t->texturechain; s; s = s->texturechain)
				if (!s->culled)
				{
					DrawGLTriangleFan (s->polys, s->flags);
				}
		}
	}
}

/*
================
R_DrawTextureChains_Drawflat -- johnfitz
================
*/
void R_DrawTextureChains_Drawflat (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	glpoly_t	*p;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;

		if (r_oldwater.value && t->texturechain && (t->texturechain->flags & SURF_DRAWTURB))
		{
			for (s = t->texturechain; s; s = s->texturechain)
				if (!s->culled)
					for (p = s->polys->next; p; p = p->next)
					{
						srand((unsigned int) (uintptr_t) p);
						glColor3f (rand()%256/255.0, rand()%256/255.0, rand()%256/255.0);
						DrawGLPoly (p, 0);
						rs_brushpasses++;
					}
		}
		else
		{
			for (s = t->texturechain; s; s = s->texturechain)
				if (!s->culled)
				{
					srand((unsigned int) (uintptr_t) s->polys);
					glColor3f (rand()%256/255.0, rand()%256/255.0, rand()%256/255.0);
					DrawGLPoly (s->polys, 0);
					rs_brushpasses++;
				}
		}
	}
	glColor3f (1,1,1);
	srand ((int) (cl.ctime * 1000));
}

/*
================
R_DrawTextureChains_Glow -- johnfitz
================
*/
void R_DrawTextureChains_Glow (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	gltexture_t	*glt;
	qboolean	bound;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || !(glt = R_TextureAnimation(t,0)->fullbright))
			continue;

		bound = false;

		for (s = t->texturechain; s; s = s->texturechain)
			if (!s->culled)
			{
				if (!bound) //only bind once we are sure we need this texture
				{
					GL_Bind (glt);
					bound = true;
				}
				DrawGLPoly (s->polys, s->flags);
				rs_brushpasses++;
			}
	}
}

/*
================
R_DrawTextureChains_Multitexture -- johnfitz
================
*/
void R_DrawTextureChains_Multitexture (void)
{
	int			i, j;
	msurface_t	*s;
	texture_t	*t;
	float		*v;
	qboolean	bound;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || t->texturechain->flags & (SURF_DRAWTILED | SURF_NOTEXTURE))
			continue;

		bound = false;
		for (s = t->texturechain; s; s = s->texturechain)
#ifdef SUPPORTS_MIRRORS // Baker change
			if (!s->culled || mirror_in_scene)
			{
// mirror:  2. When drawing the texture chains, set mirror to true.
// mirror: This will set mirror to true (we have mirror in frame)
// mirror: And this will set the mirror plane
// When drawing the texture chains, if we encounter a mirror
// texture that is not culled.  Don't draw it
				if (i == mirrortexturenum && r_mirroralpha.value < 1.0) // Baker 3.99: changed, max value is 1
				{
//					R_MirrorChain (s);
					if (!mirror_in_scene) // If first first, set mirror to true and set plane
					{
						mirror_in_scene = true;
						mirror_plane = s->plane;
					}
					continue; // Don't draw
				}
				if (!bound) //only bind once we are sure we need this texture
				{
					GL_Bind ((R_TextureAnimation(t,0))->gltexture);
					GL_EnableMultitexture(); // selects TEXTURE1
					bound = true;
				}
				R_RenderDynamicLightmaps (s);
				GL_Bind (lightmap[s->lightmaptexturenum].texture);

				glBegin(GL_POLYGON);
				v = s->polys->verts[0];
				for (j=0 ; j<s->polys->numverts ; j++, v+= VERTEXSIZE)
				{
					renderer.GL_MTexCoord2fFunc (renderer.TEXTURE0, v[3], v[4]);
					renderer.GL_MTexCoord2fFunc (renderer.TEXTURE1, v[5], v[6]);
					glVertex3fv (v);
				}
				glEnd ();
				rs_brushpasses++;
			}
		GL_DisableMultitexture(); // selects TEXTURE0
	}
}

/*
================
R_DrawTextureChains_Multitexture -- johnfitz
================
*/
void R_DrawTextureChains_Multitexture_SingleTexture (int texturenum)
{
	int			i = texturenum;
	int			j;
	msurface_t	*s;
	texture_t	*t;
	float		*v;
	qboolean	bound;

	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || t->texturechain->flags & (SURF_DRAWTILED | SURF_NOTEXTURE))
			return;

		bound = false;
		for (s = t->texturechain; s; s = s->texturechain)
			if (!s->culled || mirror_in_scene /* if mirror in scene, culling is wrong for pov */)
			{
// mirror:  2. When drawing the texture chains, set mirror to true.
// mirror: This will set mirror to true (we have mirror in frame)
// mirror: And this will set the mirror plane
// When drawing the texture chains, if we encounter a mirror
// texture that is not culled.  Don't draw it
				if (i == mirrortexturenum && r_mirroralpha.value < 1.0) // Baker 3.99: changed, max value is 1
				{
//					R_MirrorChain (s);
					if (!mirror_in_scene) // If first first, set mirror to true and set plane
					{
						mirror_in_scene = true;
						mirror_plane = s->plane;
					}
					continue; // Don't draw
				}
#else // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change
			if (!s->culled)
			{
#endif // Baker change -
				if (!bound) //only bind once we are sure we need this texture
				{
					GL_Bind ((R_TextureAnimation(t,0))->gltexture);
					GL_EnableMultitexture(); // selects TEXTURE1
					bound = true;
				}
				if (s->flags & SURF_DRAWFENCE)
				{
					GL_DisableMultitexture(); // selects TEXTURE0
					glEnable (GL_ALPHA_TEST); // Flip alpha test back on
					GL_EnableMultitexture(); // selects TEXTURE1
				}
				R_RenderDynamicLightmaps (s);
				GL_Bind (lightmap[s->lightmaptexturenum].texture);
				glBegin(GL_POLYGON);
				v = s->polys->verts[0];
				for (j=0 ; j<s->polys->numverts ; j++, v+= VERTEXSIZE)
				{
					if (s->flags & (SURF_DRAWENVMAP | SURF_SCROLLX | SURF_SCROLLY))
					{
						float s0, t0;
						void CalcTexCoords (float *verts, float *s, float *t, int renderfx);
						CalcTexCoords (v, &s0, &t0, s->flags);
						renderer.GL_MTexCoord2fFunc (renderer.TEXTURE0, s0, t0);
					}
					else
						renderer.GL_MTexCoord2fFunc (renderer.TEXTURE0, v[3], v[4]);

					renderer.GL_MTexCoord2fFunc (renderer.TEXTURE1, v[5], v[6]);
					glVertex3fv (v);
				}
				glEnd ();
				if (s->flags & SURF_DRAWFENCE)
				{
					GL_DisableMultitexture(); // selects TEXTURE0
					glDisable (GL_ALPHA_TEST); // Flip alpha test back off
					GL_EnableMultitexture(); // selects TEXTURE1
				}

				rs_brushpasses++;
			}
		GL_DisableMultitexture(); // selects TEXTURE0
	}
}

/*
================
R_DrawTextureChains_NoTexture -- johnfitz

draws surfs whose textures were missing from the BSP
================
*/
void R_DrawTextureChains_NoTexture (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qboolean	bound;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || !(t->texturechain->flags & SURF_NOTEXTURE))
			continue;

		bound = false;

		for (s = t->texturechain; s; s = s->texturechain)
			if (!s->culled)
			{
				if (!bound) //only bind once we are sure we need this texture
				{
					GL_Bind (t->gltexture);
					bound = true;
				}
				DrawGLPoly (s->polys, 0);
				rs_brushpasses++;
			}
	}
}

/*
================
R_DrawTextureChains_TextureOnly -- johnfitz
================
*/
void R_DrawTextureChains_TextureOnly (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qboolean	bound;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || t->texturechain->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
			continue;

		bound = false;

		for (s = t->texturechain; s; s = s->texturechain)
			if (!s->culled)
			{
				if (!bound) //only bind once we are sure we need this texture
				{
					GL_Bind ((R_TextureAnimation(t,0))->gltexture);
					bound = true;
				}
				if (s->flags & SURF_DRAWFENCE)
					glEnable (GL_ALPHA_TEST); // Flip alpha test back on
				R_RenderDynamicLightmaps (s); //adds to lightmap chain
				DrawGLPoly (s->polys, s->flags);

				if (s->flags & SURF_DRAWFENCE)
					glDisable (GL_ALPHA_TEST); // Flip alpha test back off
				rs_brushpasses++;
			}
	}
}

/*
================
R_DrawTextureChains_Water -- johnfitz
================
*/
void R_DrawTextureChains_Water (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	glpoly_t	*p;
	qboolean	bound;
	qboolean	isteleporter;

	if (r_drawflat_cheatsafe || r_lightmap_cheatsafe || !r_drawworld_cheatsafe)
		return;

	if (r_wateralpha.value < 1.0)
	{
		// Baker: Yikes ... we will have to turn this off and on as we draw teleporters.  Or create a new chain (NO).
		glDepthMask(GL_FALSE);
		glEnable (GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f (1,1,1,r_wateralpha.value);
	}

	if (r_oldwater.value)
	{
		for (i=0 ; i<cl.worldmodel->numtextures ; i++)
		{
			t = cl.worldmodel->textures[i];
			if (!t || !t->texturechain || !(t->texturechain->flags & SURF_DRAWTURB))
				continue;
			bound = false;
			for (s = t->texturechain; s; s = s->texturechain)
				if (!s->culled)
				{
					if (!bound) //only bind once we are sure we need this texture
					{
						GL_Bind (t->gltexture);
						bound = true;
					}
					isteleporter = s->flags & SURF_DRAWTELEPORTER;
					if (isteleporter && r_wateralpha.value < 1.0)
					{
						glDepthMask(GL_TRUE); // Flip water alpha off for teleporter
						glDisable (GL_BLEND);
						glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
						glColor3f (1,1,1);
					}
					for (p = s->polys->next; p; p = p->next)
					{
						DrawWaterPoly (p, isteleporter);
						rs_brushpasses++;
					}
					if (isteleporter && r_wateralpha.value < 1.0)
					{
						glDepthMask(GL_FALSE);  // Water alpha back on
						glEnable (GL_BLEND);
						glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
						glColor4f (1,1,1,r_wateralpha.value);

					}
				}
		}
	}
	else
	{
		for (i=0 ; i<cl.worldmodel->numtextures ; i++)
		{
			t = cl.worldmodel->textures[i];
			if (!t || !t->texturechain || !(t->texturechain->flags & SURF_DRAWTURB))
				continue;
			bound = false;
			for (s = t->texturechain; s; s = s->texturechain)
				if (!s->culled)
				{
					if (!bound) //only bind once we are sure we need this texture
					{
						GL_Bind (t->warpimage);
						bound = true;
					}
					isteleporter = s->flags & SURF_DRAWTELEPORTER;
					if (isteleporter && r_wateralpha.value < 1.0)
					{
						glDepthMask(GL_TRUE);  // Flip water alpha off for teleporter
						glDisable (GL_BLEND);
						glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
						glColor3f (1,1,1);
					}
					DrawGLPoly (s->polys, s->flags);
					rs_brushpasses++;
					if (isteleporter && r_wateralpha.value < 1.0)
					{
						glDepthMask(GL_FALSE);  // Flip water alpha back off
						glEnable (GL_BLEND);
						glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
						glColor4f (1,1,1,r_wateralpha.value);
					}
				}
		}
	}

	if (r_wateralpha.value < 1.0)
	{
		glDepthMask(GL_TRUE);
		glDisable (GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor3f (1,1,1);
	}
}

/*
================
R_DrawTextureChains_White -- johnfitz -- draw sky and water as white polys when r_lightmap is 1
================
*/
void R_DrawTextureChains_White (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	glDisable (GL_TEXTURE_2D);
	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || !(t->texturechain->flags & SURF_DRAWTILED))
			continue;

		for (s = t->texturechain; s; s = s->texturechain)
			if (!s->culled)
			{
				DrawGLPoly (s->polys, 0); // Not here.
				rs_brushpasses++;
			}
	}
	glEnable (GL_TEXTURE_2D);
}

/*
================
R_DrawLightmapChains -- johnfitz -- R_BlendLightmaps stripped down to almost nothing
================
*/
void R_DrawLightmapChains (void)
{
	int			i, j;
	glpoly_t	*p;
	float		*v;

	for (i=0 ; i<MAX_FITZQUAKE_LIGHTMAPS ; i++)
	{
		if (!lightmap[i].polys)
			continue;

		GL_Bind (lightmap[i].texture);
		for (p = lightmap[i].polys; p; p=p->chain)
		{
			glBegin (GL_POLYGON);
			v = p->verts[0];
			for (j=0 ; j<p->numverts ; j++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
			rs_brushpasses++;
		}
	}
}

/*
=============
R_DrawWorld -- johnfitz -- rewritten
=============
*/
void R_DrawWorld (void)
{
	R_UploadLightmaps_Modified ();
	if (!r_drawworld_cheatsafe)
		return;

	if (r_drawflat_cheatsafe)
	{
		glDisable (GL_TEXTURE_2D);
		R_DrawTextureChains_Drawflat ();
		glEnable (GL_TEXTURE_2D);
		return;
	}

	if (r_fullbright_cheatsafe)
	{
		R_DrawTextureChains_TextureOnly ();
		goto fullbrights;
	}

	if (r_lightmap_cheatsafe)
	{
		R_BuildLightmapChains ();
		if (!gl_overbright.value)
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor3f(0.5, 0.5, 0.5);
		}
		R_DrawLightmapChains ();
		if (!gl_overbright.value)
		{
			glColor3f(1,1,1);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		R_DrawTextureChains_White ();
		return;
	}

	R_DrawTextureChains_NoTexture ();

	if (gl_overbright.value)
	{
		if (renderer.gl_texture_env_combine && renderer.gl_mtexable)
		{
			GL_EnableMultitexture ();
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
			GL_DisableMultitexture ();
			R_DrawTextureChains_Multitexture ();
			GL_EnableMultitexture ();
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_DisableMultitexture ();
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		else
		{
			//to make fog work with multipass lightmapping, need to do one pass
			//with no fog, one modulate pass with black fog, and one additive
			//pass with black geometry and normal fog
			Fog_DisableGFog ();
			R_DrawTextureChains_TextureOnly ();
			Fog_EnableGFog ();
			glDepthMask (GL_FALSE);
			glEnable (GL_BLEND);
			glBlendFunc (GL_DST_COLOR, GL_SRC_COLOR); //2x modulate
			Fog_StartAdditive ();
			R_DrawLightmapChains ();
			Fog_StopAdditive ();
			if (Fog_GetDensity() > 0)
			{
				glBlendFunc(GL_ONE, GL_ONE); //add
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				glColor3f(0,0,0);
				R_DrawTextureChains_TextureOnly ();
				glColor3f(1,1,1);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}
			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable (GL_BLEND);
			glDepthMask (GL_TRUE);
		}
	}
	else
	{
		if (renderer.gl_mtexable)
		{
			GL_EnableMultitexture ();
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_DisableMultitexture ();
			R_DrawTextureChains_Multitexture ();
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		else
		{
			//to make fog work with multipass lightmapping, need to do one pass
			//with no fog, one modulate pass with black fog, and one additive
			//pass with black geometry and normal fog
			Fog_DisableGFog ();
			R_DrawTextureChains_TextureOnly ();
			Fog_EnableGFog ();
			glDepthMask (GL_FALSE);
			glEnable (GL_BLEND);
			glBlendFunc(GL_ZERO, GL_SRC_COLOR); //modulate
			Fog_StartAdditive ();
			R_DrawLightmapChains ();
			Fog_StopAdditive ();
			if (Fog_GetDensity() > 0)
			{
				glBlendFunc(GL_ONE, GL_ONE); //add
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				glColor3f(0,0,0);
				R_DrawTextureChains_TextureOnly ();
				glColor3f(1,1,1);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable (GL_BLEND);
			glDepthMask (GL_TRUE);
		}
	}

fullbrights:
	if (gl_fullbrights.value)
	{
		glDepthMask (GL_FALSE);
		glEnable (GL_BLEND);
		glBlendFunc (GL_ONE, GL_ONE);
		Fog_StartAdditive ();
		R_DrawTextureChains_Glow ();
		Fog_StopAdditive ();
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable (GL_BLEND);
		glDepthMask (GL_TRUE);
	}
}