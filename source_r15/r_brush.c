/* Versus Quakespasm:

1.  Quakespasm's zfix shows secret doors a little :(  The E1M1 Quad area is
    annoying with a zfix.  See if you can use a hack for E1M1 the way that
    the shotgun shell fix is done.
2.  Demo rewind
3.  Calc tex coords and different texture types
4.  Dynamic lighting correction for moved/rotated brush models.
5.  MH dynamic light speed up
6.  Keep teleporter from getting r_wateralpha hack.


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
// r_brush.c: brush model rendering. renamed from r_surf.c

#include "quakedef.h"

static void R_BuildLightMap (msurface_t *surf);


/*
===============
R_TextureAnimation -- johnfitz -- added "frame" param to eliminate use of "currententity" global

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base, int frame)
{
	int		relative;
	int		count;

	if (frame)
		if (base->alternate_anims)
			base = base->alternate_anims;

	if (!base->anim_total)
		return base;

	relative = (int)(cl.ctime*10) % base->anim_total;

	count = 0;
	while (base->anim_min > relative || base->anim_max <= relative)
	{
		base = base->anim_next;
		if (!base)
			Host_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Host_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}

void CalcTexCoords (float *verts, float *s, float *t, int renderfx)
{
	float scroll;
	if (renderfx & SURF_DRAWENVMAP)
	{
		vec3_t		dir;
		float		length;

		VectorSubtract (verts, r_origin, dir);
		dir[2] *= 3;	// flatten the sphere

		length = VectorLength (dir);
		length = 6*63/length;

		dir[0] *= length;
		dir[1] *= length;

		*s = (dir[0]) * (1.0/256);
		*t = (dir[1]) * (1.0/256);
		return;
	}

	// We want this to scroll 1 texture length every X seconds in increments of 256

	// Seconds range.  10 secconds.

#define SCROLL_CYCLE_SECONDS 5
	scroll = ((int)cl.ctime % SCROLL_CYCLE_SECONDS) * SCROLL_CYCLE_SECONDS;	// (whole number); // Calc floor.  Will be an integer
	scroll = cl.ctime - scroll;			// Scroll is a number between 0 and 10 (but not 10)
	scroll = (1.0f / SCROLL_CYCLE_SECONDS) * scroll;

	*s = verts[3];
	*t = verts[4];

	if (renderfx & SURF_SCROLLX)
		*s = *s + scroll;
	if (renderfx & SURF_SCROLLY)
		*t = *t + scroll;
}


/*
================
DrawGLPoly
================
*/
void DrawGLPoly (glpoly_t *p, int renderfx)
{
	float	*v;
	int		i;

	if (renderfx & (SURF_DRAWENVMAP | SURF_SCROLLX | SURF_SCROLLY))
	{
		glBegin (GL_POLYGON);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			float s0, t0;
			CalcTexCoords (v, &s0, &t0, renderfx);
			glTexCoord2f (s0, t0);
			glVertex3fv (v);
		}
		glEnd ();
		return;
	}

	if ((renderfx & SURF_DRAWTURB) && (renderfx & SURF_DRAWTELEPORTER)==0 && r_waterripple.value) // liquid, not a tele and with water ripple
	{
		glBegin (GL_POLYGON);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			vec3_t		newverts;
			newverts[0] = v[0];
			newverts[1] = v[1];
			newverts[2] = v[2] + r_waterripple.value * sin(v[0] * 0.05 + cl.ctime * 3) * sin(v[2] * 0.05 + cl.ctime * 3);

			glTexCoord2f (v[3], v[4]);
			glVertex3fv (newverts);
		}
		glEnd ();
		return;
	}

	glBegin (GL_POLYGON);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[3], v[4]);
		glVertex3fv (v);
	}
	glEnd ();
}

/*
================
DrawGLTriangleFan -- johnfitz -- like DrawGLPoly but for r_showtris
================
*/
void DrawGLTriangleFan (glpoly_t *p, int renderfx)
{
	float	*v;
	int		i;

	if ((renderfx & SURF_DRAWTURB) && (renderfx & SURF_DRAWTELEPORTER)==0 && r_waterripple.value)
	{
		glBegin (GL_TRIANGLE_FAN);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			vec3_t		newverts;
			newverts[0] = v[0];
			newverts[1] = v[1];
			newverts[2] = v[2] + r_waterripple.value * sin(v[0] * 0.05 + cl.ctime * 3) * sin(v[2] * 0.05 + cl.ctime * 3);

			glVertex3fv (newverts);

		}
		glEnd ();
		return;
	}

	glBegin (GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glVertex3fv (v);
	}
	glEnd ();
}

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
================
R_DrawSequentialPoly -- johnfitz -- rewritten
================
*/
void R_DrawSequentialPoly (msurface_t *s)
{
	glpoly_t	*p;
	texture_t	*t;
	float		*v;
	float		entalpha;
	int			i;

	t = R_TextureAnimation (s->texinfo->texture, currententity->frame);
	entalpha = ENTALPHA_DECODE(currententity->alpha);

// drawflat
	if (r_drawflat_cheatsafe)
	{
		if ((s->flags & SURF_DRAWTURB) && r_oldwater.value)
		{
			for (p = s->polys->next; p; p = p->next)
			{
				srand((unsigned int) (uintptr_t) p);
				glColor3f (rand()%256/255.0, rand()%256/255.0, rand()%256/255.0);
				DrawGLPoly (p, s->flags);
				rs_brushpasses++;
			}
			return;
		}

		srand((unsigned int) (uintptr_t) s->polys);
		glColor3f (rand()%256/255.0, rand()%256/255.0, rand()%256/255.0);
		DrawGLPoly (s->polys, 0); // Drawflat doesn't do texture
		rs_brushpasses++;
		return;
	}

// fullbright
	if ((r_fullbright_cheatsafe) && !(s->flags & SURF_DRAWTILED))
	{
		if (entalpha < 1)
		{
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f(1, 1, 1, entalpha);
		}
		if (s->flags & SURF_DRAWFENCE)
			glEnable (GL_ALPHA_TEST); // Flip on alpha test

		GL_Bind (t->gltexture);
		DrawGLPoly (s->polys, s->flags);
		rs_brushpasses++;
			if (s->flags & SURF_DRAWFENCE)
				glDisable (GL_ALPHA_TEST); // Flip alpha test back off

		if (entalpha < 1)
		{
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glColor3f(1, 1, 1);
		}
		goto fullbrights;
	}

// r_lightmap
	if (r_lightmap_cheatsafe)
	{
		if (s->flags & SURF_DRAWTILED)
		{
			glDisable (GL_TEXTURE_2D);
			DrawGLPoly (s->polys, 0); // Lightmap doesn't do texture
			glEnable (GL_TEXTURE_2D);
			rs_brushpasses++;
			return;
		}

		R_RenderDynamicLightmaps (s);
		GL_Bind (lightmap[s->lightmaptexturenum].texture);
		if (!gl_overbright.value)
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor3f(0.5, 0.5, 0.5);
		}
		glBegin (GL_POLYGON);
		v = s->polys->verts[0];
		for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
		{
			glTexCoord2f (v[5], v[6]);
			glVertex3fv (v);
		}
		glEnd ();
		if (!gl_overbright.value)
		{
			glColor3f(1,1,1);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		rs_brushpasses++;
		return;
	}

// sky poly -- skip it, already handled in gl_sky.c
	if (s->flags & SURF_DRAWSKY)
		return;

// water poly
	if (s->flags & SURF_DRAWTURB)
	{
		if (currententity->alpha == ENTALPHA_DEFAULT && (s->flags & SURF_DRAWTELEPORTER)== false)
			entalpha = CLAMP(0.0,r_wateralpha.value,1.0);

		if (entalpha < 1)
		{
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f(1, 1, 1, entalpha);
		}
		if (r_oldwater.value)
		{
			GL_Bind (s->texinfo->texture->gltexture);
			for (p = s->polys->next; p; p = p->next)
			{
				DrawWaterPoly (p, s->flags & SURF_DRAWTELEPORTER);
				rs_brushpasses++;
			}
			rs_brushpasses++;
		}
		else
		{
			GL_Bind (s->texinfo->texture->warpimage);
			s->texinfo->texture->update_warp = true; // FIXME: one frame too late!
			DrawGLPoly (s->polys, s->flags);
			rs_brushpasses++;
		}
		if (entalpha < 1)
		{
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glColor3f(1, 1, 1);
		}
		return;
	}

// missing texture
	if (s->flags & SURF_NOTEXTURE)
	{
		if (entalpha < 1)
		{
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f(1, 1, 1, entalpha);
		}
		GL_Bind (t->gltexture);
		DrawGLPoly (s->polys, 0); // I'm thinking texturefx on missing texture would be bad
		rs_brushpasses++;
		if (entalpha < 1)
		{
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glColor3f(1, 1, 1);
		}
		return;
	}

// lightmapped poly
	if (entalpha < 1)
	{
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f(1, 1, 1, entalpha);
	}
	else
		glColor3f(1, 1, 1);
	if (gl_overbright.value)
	{
		if (renderer.gl_texture_env_combine && renderer.gl_mtexable) //case 1: texture and lightmap in one pass, overbright using texture combiners
		{
			if (s->flags & SURF_DRAWFENCE)
				glEnable (GL_ALPHA_TEST); // Flip on alpha test

			GL_DisableMultitexture(); // selects TEXTURE0
			GL_Bind (t->gltexture);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_EnableMultitexture(); // selects TEXTURE1
			GL_Bind (lightmap[s->lightmaptexturenum].texture);
			R_RenderDynamicLightmaps (s);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
			glBegin(GL_POLYGON);
			v = s->polys->verts[0];
			for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
			{
				if (s->flags & (SURF_DRAWENVMAP | SURF_SCROLLX | SURF_SCROLLY))
				{
					float s0, t0;
					CalcTexCoords (v, &s0, &t0, s->flags);
					renderer.GL_MTexCoord2fFunc (renderer.TEXTURE0, s0, t0);
				}
				else renderer.GL_MTexCoord2fFunc (renderer.TEXTURE0, v[3], v[4]);

				renderer.GL_MTexCoord2fFunc (renderer.TEXTURE1, v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_DisableMultitexture ();
			if (s->flags & SURF_DRAWFENCE)
				glDisable (GL_ALPHA_TEST); // Flip alpha test back off
			rs_brushpasses++;
		}
		else if (entalpha < 1) //case 2: can't do multipass if entity has alpha, so just draw the texture
		{
			GL_Bind (t->gltexture);
			DrawGLPoly (s->polys, s->flags);
			rs_brushpasses++;
		}
		else //case 3: texture in one pass, lightmap in second pass using 2x modulation blend func, fog in third pass
		{
			//first pass -- texture with no fog
			Fog_DisableGFog ();
			GL_Bind (t->gltexture);
			DrawGLPoly (s->polys, s->flags);
			Fog_EnableGFog ();
			rs_brushpasses++;

			//second pass -- lightmap with black fog, modulate blended
			R_RenderDynamicLightmaps (s);
			GL_Bind (lightmap[s->lightmaptexturenum].texture);
			glDepthMask (GL_FALSE);
			glEnable (GL_BLEND);
			glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR); //2x modulate
			Fog_StartAdditive ();
			glBegin (GL_POLYGON);
			v = s->polys->verts[0];
			for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
			Fog_StopAdditive ();
			rs_brushpasses++;

			//third pass -- black geo with normal fog, additive blended
			if (Fog_GetDensity() > 0)
			{
				glBlendFunc(GL_ONE, GL_ONE); //add
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				glColor3f(0,0,0);
				DrawGLPoly (s->polys, s->flags);
				glColor3f(1,1,1);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				rs_brushpasses++;
			}

			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable (GL_BLEND);
			glDepthMask (GL_TRUE);
		}
	}
	else
	{
		if (renderer.gl_mtexable) //case 4: texture and lightmap in one pass, regular modulation
		{
		if (s->flags & SURF_DRAWFENCE)
			glEnable (GL_ALPHA_TEST); // Flip on alpha test
			GL_DisableMultitexture(); // selects TEXTURE0
			GL_Bind (t->gltexture);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_EnableMultitexture(); // selects TEXTURE1
			GL_Bind (lightmap[s->lightmaptexturenum].texture);
			R_RenderDynamicLightmaps (s);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glBegin(GL_POLYGON);
			v = s->polys->verts[0];
			for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
			{
				if (s->flags & (SURF_DRAWENVMAP | SURF_SCROLLX | SURF_SCROLLY))
				{
					float s0, t0;
					CalcTexCoords (v, &s0, &t0, s->flags);
					renderer.GL_MTexCoord2fFunc (renderer.TEXTURE0, s0, t0);
				}
				else renderer.GL_MTexCoord2fFunc (renderer.TEXTURE0, v[3], v[4]);

				renderer.GL_MTexCoord2fFunc (renderer.TEXTURE1, v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
			GL_DisableMultitexture ();
			if (s->flags & SURF_DRAWFENCE)
				glDisable (GL_ALPHA_TEST); // Flip alpha test back off
			rs_brushpasses++;
		}
		else if (entalpha < 1) //case 5: can't do multipass if entity has alpha, so just draw the texture
		{
			GL_Bind (t->gltexture);
			DrawGLPoly (s->polys, s->flags);
			rs_brushpasses++;
		}
		else //case 6: texture in one pass, lightmap in a second pass, fog in third pass
		{
			//first pass -- texture with no fog
			Fog_DisableGFog ();
			GL_Bind (t->gltexture);
			DrawGLPoly (s->polys, s->flags);
			Fog_EnableGFog ();
			rs_brushpasses++;

			//second pass -- lightmap with black fog, modulate blended
			R_RenderDynamicLightmaps (s);
			GL_Bind (lightmap[s->lightmaptexturenum].texture);
			glDepthMask (GL_FALSE);
			glEnable (GL_BLEND);
			glBlendFunc (GL_ZERO, GL_SRC_COLOR); //modulate
			Fog_StartAdditive ();
			glBegin (GL_POLYGON);
			v = s->polys->verts[0];
			for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
			Fog_StopAdditive ();
			rs_brushpasses++;

			//third pass -- black geo with normal fog, additive blended
			if (Fog_GetDensity() > 0)
			{
				glBlendFunc(GL_ONE, GL_ONE); //add
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				glColor3f(0,0,0);
				DrawGLPoly (s->polys, s->flags);
				glColor3f(1,1,1);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				rs_brushpasses++;
			}

			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable (GL_BLEND);
			glDepthMask (GL_TRUE);

		}
	}
	if (entalpha < 1)
	{
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor3f(1, 1, 1);
	}

fullbrights:
	if (gl_fullbrights.value && t->fullbright)
	{
		glDepthMask (GL_FALSE);
		glEnable (GL_BLEND);
		glBlendFunc (GL_ONE, GL_ONE);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor3f (entalpha, entalpha, entalpha);
		GL_Bind (t->fullbright);
		Fog_StartAdditive ();
		DrawGLPoly (s->polys, s->flags);
		Fog_StopAdditive ();
		glColor3f(1, 1, 1);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable (GL_BLEND);
		glDepthMask (GL_TRUE);
		rs_brushpasses++;
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
	int			i;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	qmodel_t		*clmodel;

	if (R_CullModelForEntity(e))
		return;

	currententity = e;
	clmodel = e->model;

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend.value)
	{
		// calculate entity local space for dlight transforms
		GL_IdentityMatrix (&e->gl_matrix);

		// don't need to negate angles[0] as it's not going through the extra negation in R_RotateForEntity
		if (e->angles[2]) GL_RotateMatrix (&e->gl_matrix, -e->angles[2], 1, 0, 0);
		if (e->angles[0]) GL_RotateMatrix (&e->gl_matrix, -e->angles[0], 0, 1, 0);
		if (e->angles[1]) GL_RotateMatrix (&e->gl_matrix, -e->angles[1], 0, 0, 1);

		GL_TranslateMatrix (&e->gl_matrix, -e->origin[0], -e->origin[1], -e->origin[2]);
		R_PushDlights (e);
	}

    glPushMatrix ();
	e->angles[0] = -e->angles[0];	// stupid quake bug
	if (gl_zfix.value)
	{
		e->origin[0] -= DIST_EPSILON;
		e->origin[1] -= DIST_EPSILON;
		e->origin[2] -= DIST_EPSILON;
	}
	R_RotateForEntity (e->origin, e->angles);
	if (gl_zfix.value)
	{
		e->origin[0] += DIST_EPSILON;
		e->origin[1] += DIST_EPSILON;
		e->origin[2] += DIST_EPSILON;
	}
	e->angles[0] = -e->angles[0];	// stupid quake bug

	//
	// draw it
	//
	if (r_drawflat_cheatsafe) //johnfitz
		glDisable(GL_TEXTURE_2D);

	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
	{
		pplane = psurf->plane;
		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_DrawSequentialPoly (psurf);
			rs_brushpolys++;
		}
	}

	if (r_drawflat_cheatsafe) //johnfitz
		glEnable(GL_TEXTURE_2D);

	GL_DisableMultitexture(); // selects TEXTURE0

	glPopMatrix ();
}

/*
=================
R_DrawBrushModel_ShowTris -- johnfitz
=================
*/
void R_DrawBrushModel_ShowTris (entity_t *e)
{
	int			i;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	qmodel_t		*clmodel;
	glpoly_t	*p;

	if (R_CullModelForEntity(e))
		return;

	currententity = e;
	clmodel = e->model;

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

    glPushMatrix ();
	e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e->origin, e->angles);
	e->angles[0] = -e->angles[0];	// stupid quake bug

	//
	// draw it
	//
	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
	{
		pplane = psurf->plane;
		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if ((psurf->flags & SURF_DRAWTURB) && r_oldwater.value)
				for (p = psurf->polys->next; p; p = p->next)
					DrawGLTriangleFan (p, psurf->flags);
			else DrawGLTriangleFan (psurf->polys, psurf->flags);
		}
	}

	glPopMatrix ();
}

/*
=============================================================

	LIGHTMAPS

=============================================================
*/



lightmapinfo_t lightmap[MAX_FITZQUAKE_LIGHTMAPS];

static unsigned	blocklights[BLOCKLITE_BLOCK_SIZE]; //johnfitz -- was 18*18, added lit support (*3) and loosened surface extents maximum (LIGHTMAPS_BLOCK_WIDTH*LIGHTMAPS_BLOCK_HEIGHT)

/*
================
R_RenderDynamicLightmaps
called during rendering
================
*/
void R_RenderDynamicLightmaps (msurface_t *fa)
{
	int			maps;

	if (fa->flags & SURF_DRAWTILED) //johnfitz -- not a lightmapped surface
		return;
#if 1 // This is only for non-multitexture dynamic lights
	// add to lightmap chain
	fa->polys->chain = lightmap[fa->lightmaptexturenum].polys;
	lightmap[fa->lightmaptexturenum].polys = fa->polys;
#endif

	// check for lightmap modification
	for (maps=0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
	{
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
		{
			goto dynamic;
		}
	}

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
dynamic:
		if (r_dynamic.value)
		{
			lightmap[fa->lightmaptexturenum].modified = true;
			if (fa->light_t < lightmap[fa->lightmaptexturenum].rectchange.t)
			{
				if (lightmap[fa->lightmaptexturenum].rectchange.h)
					lightmap[fa->lightmaptexturenum].rectchange.h += lightmap[fa->lightmaptexturenum].rectchange.t - fa->light_t;
				lightmap[fa->lightmaptexturenum].rectchange.t = fa->light_t;
			}
			if (fa->light_s < lightmap[fa->lightmaptexturenum].rectchange.l)
			{
				if (lightmap[fa->lightmaptexturenum].rectchange.w)
					lightmap[fa->lightmaptexturenum].rectchange.w += lightmap[fa->lightmaptexturenum].rectchange.l - fa->light_s;
				lightmap[fa->lightmaptexturenum].rectchange.l = fa->light_s;
			}

			if ((lightmap[fa->lightmaptexturenum].rectchange.w + lightmap[fa->lightmaptexturenum].rectchange.l) < (fa->light_s + fa->smax))
				lightmap[fa->lightmaptexturenum].rectchange.w = (fa->light_s-lightmap[fa->lightmaptexturenum].rectchange.l) + fa->smax;

			if ((lightmap[fa->lightmaptexturenum].rectchange.h + lightmap[fa->lightmaptexturenum].rectchange.t) < (fa->light_t + fa->tmax))
				lightmap[fa->lightmaptexturenum].rectchange.h = (fa->light_t-lightmap[fa->lightmaptexturenum].rectchange.t) + fa->tmax;

			R_BuildLightMap (fa);
		}
	}
}

/*
========================
AllocBlock -- returns a texture number and the position inside it
========================
*/

static int AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_FITZQUAKE_LIGHTMAPS ; texnum++)
	{
		best = LIGHTMAPS_BLOCK_HEIGHT;

		for (i=0 ; i<LIGHTMAPS_BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (lightmap[texnum].allocated[i+j] >= best)
					break;
				if (lightmap[texnum].allocated[i+j] > best2)
					best2 = lightmap[texnum].allocated[i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > LIGHTMAPS_BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			lightmap[texnum].allocated[*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}



/*
========================
GL_CreateSurfaceLightmap
========================
*/
static void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	// store these out so that we don't have to recalculate them every time
	surf->smax = (surf->extents[0] >> 4) + 1;
	surf->tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum = AllocBlock (surf->smax, surf->tmax, &surf->light_s, &surf->light_t);

	R_BuildLightMap (surf);
}

/*
================
BuildSurfaceDisplayList -- called at level load time
================
*/
static void BuildSurfaceDisplayList (qmodel_t *curmodel, msurface_t *fa)
{
	medge_t		*pedges = curmodel->edges;

// reconstruct the polygon
	int			lnumverts = fa->numedges;
	glpoly_t	*poly = (glpoly_t *) Hunk_AllocName (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float), "lm_polys");

	medge_t 	*r_pedge;

	int			i, lindex;
	float		*vec, s, t;
	//
	// draw texture
	//
	poly->next = fa->polys;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = curmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = curmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = curmodel->vertexes[r_pedge->v[1]].position;
		}

		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s*16;
		s += 8;
		s /= LIGHTMAPS_BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t*16;
		t += 8;
		t /= LIGHTMAPS_BLOCK_HEIGHT*16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	poly->numverts = lnumverts;
}



/*
==================
GL_BuildLightmaps -- called at level load time

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/

void GL_BuildLightmaps_Upload_All_NewMap (void)
{
	char	name[16];
	int		i, j;

	for (i = 0; i < MAX_FITZQUAKE_LIGHTMAPS; i ++)
		memset (lightmap[i].allocated, 0, sizeof(lightmap[i].allocated));

	r_framecount = 1; // no dlightcache

	//johnfitz -- null out array (the gltexture objects themselves were already freed by Mod_ClearAll)
	for (i=0; i < MAX_FITZQUAKE_LIGHTMAPS; i++)
		lightmap[i].texture = NULL;
	//johnfitz

	for (j=1 ; j<MAX_FITZQUAKE_MODELS ; j++)
	{
		if (!cl.model_precache[j])
			break;

		if (cl.model_precache[j]->name[0] == '*')
			continue;

		for (i=0 ; i<cl.model_precache[j]->numsurfaces ; i++)
		{
			//johnfitz -- rewritten to use SURF_DRAWTILED instead of the sky/water flags
			if (cl.model_precache[j]->surfaces[i].flags & SURF_DRAWTILED)
				continue;
			GL_CreateSurfaceLightmap (cl.model_precache[j]->surfaces + i);
			BuildSurfaceDisplayList (cl.model_precache[j], cl.model_precache[j]->surfaces + i);
			//johnfitz
		}
	}

	//
	// upload all lightmaps that were filled
	//
	for (i=0; i<MAX_FITZQUAKE_LIGHTMAPS; i++)
	{
		if (!lightmap[i].allocated[0])
			break;		// no more used

		lightmap[i].modified = false;
		lightmap[i].rectchange.l = LIGHTMAPS_BLOCK_WIDTH;
		lightmap[i].rectchange.t = LIGHTMAPS_BLOCK_HEIGHT;
		lightmap[i].rectchange.w = 0;
		lightmap[i].rectchange.h = 0;

		//johnfitz -- use texture manager
		q_snprintf(name, sizeof(name), "lightmap%03i",i);
		lightmap[i].texture = TexMgr_LoadImage (cl.worldmodel, -1, name, LIGHTMAPS_BLOCK_WIDTH, LIGHTMAPS_BLOCK_HEIGHT,
			 SRC_LIGHTMAP, lightmap[i].lightmaps, "", (src_offset_t)lightmap[i].lightmaps, TEXPREF_LINEAR | TEXPREF_NOPICMIP);
		//johnfitz
	}

	//johnfitz -- warn about exceeding old limits
	if (i >= MAX_WINQUAKE_LIGHTMAPS)
		Con_Warning ("%i lightmaps exceeds standard limit of %d.\n", i, MAX_WINQUAKE_LIGHTMAPS); // 64

	//johnfitz
}

/*
===============
R_AddDynamicLights
===============
*/
static void R_AddDynamicLights (msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	mtexinfo_t	*tex;
	//johnfitz -- lit support via lordhavoc
	float		cred, cgreen, cblue, brightness;
	unsigned	*bl;
	//johnfitz

	tex = surf->texinfo;

	for (lnum=0 ; lnum<MAX_FITZQUAKE_DLIGHTS ; lnum++)
	{
		if ( !(surf->dlightbits[lnum >> 5] & (1U << (lnum & 31))))
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct (cl_dlights[lnum].transformed, surf->plane->normal) -
				surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = cl_dlights[lnum].transformed[i] -
					surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		//johnfitz -- lit support via lordhavoc
		bl = blocklights;
		cred = cl_dlights[lnum].color[0] * 256.0f;
		cgreen = cl_dlights[lnum].color[1] * 256.0f;
		cblue = cl_dlights[lnum].color[2] * 256.0f;
		//johnfitz
		for (t = 0 ; t < surf->tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;

			for (s=0 ; s < surf->smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
				//johnfitz -- lit support via lordhavoc
				{
					brightness = rad - dist;
					bl[0] += (int) (brightness * cred);
					bl[1] += (int) (brightness * cgreen);
					bl[2] += (int) (brightness * cblue);
				}
				bl += 3;
				//johnfitz
			}
		}
	}
}

/*
===============
R_BuildLightMap -- johnfitz -- revised for lit support via lordhavoc

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/

static void R_BuildLightMap (msurface_t *surf)
{
	int			size = surf->smax * surf->tmax;
	int			block_size = size * BLOCKLITE_BYTES_3;
	surf->cached_dlight = (surf->dlightframe == r_framecount);

	if (cl.worldmodel->lightdata)
	{
		byte		*mylightmap = surf->samples;
	// clear to no light
		memset (&blocklights[0], 0, block_size * sizeof (unsigned int)); //johnfitz -- lit support via lordhavoc
	// add all the lightmaps for the surface
		if (mylightmap)
		{
			int			maps, i;
			unsigned	scale;

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
			{
				unsigned	*blocklight_fill = blocklights;

				surf->cached_light[maps] = scale =  d_lightstylevalue[surf->styles[maps]];	// 8.8 fraction

				for ( i = 0 ; i < block_size; i++)
					*blocklight_fill++ += *mylightmap++ * scale;
			}
		} // End lightmap
	// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			R_AddDynamicLights (surf);
	}
	else
	{
	// set to full bright if no light data
		memset (&blocklights[0], 255, block_size * sizeof (unsigned int)); //johnfitz -- lit support via lordhavoc
	}

// bound, invert, and shift
//store:
	{
		int i, j, k, h, t;
//		int r,g,b;
		unsigned	*block_lights_paint=blocklights;
		int			overbright = (gl_overbright.value) ? 1 : 0;
#if 1
		qboolean	isstained		=   r_stains.value ? surf->stained : false;
		byte		*stain			=	lightmap[surf->lightmaptexturenum].stainmaps +
										   (surf->light_t * LIGHTMAPS_BLOCK_WIDTH + surf->light_s) * BLOCKLITE_BYTES_3;

#endif

		int stride = (LIGHTMAPS_BLOCK_WIDTH - surf->smax) * LIGHTMAPS_BYTES_4;
		int stain_stride = (LIGHTMAPS_BLOCK_WIDTH - surf->smax) * BLOCKLITE_BYTES_3;
		byte		*base = lightmap[surf->lightmaptexturenum].lightmaps;
		byte		*dest =  base + (surf->light_t * LIGHTMAPS_BLOCK_WIDTH + surf->light_s) * LIGHTMAPS_BYTES_4;

		for (i = 0 ; i < surf->tmax ; i++, dest += stride, stain +=stain_stride)
		{
			for (j=0 ; j< surf->smax ; j++, block_lights_paint +=BLOCKLITE_BYTES_3, dest += LIGHTMAPS_BYTES_4, stain += BLOCKLITE_BYTES_3)
			{
				for (k = 0, h = 2; k < 3; k++, h--) // b then g then r
				{
					t = block_lights_paint[2-k];

					switch (overbright)
					{
						case 1:		t = (t >> 8);	break;
						default:	t = (t >> 7);	break;
					}

					// Baker: Fake quick integer division.
					if (isstained && stain[k])
						t = (t * (256 - stain[k])) >> 8;

					dest[k] = CLAMP (0, t, 255);

				} // End k
				dest[3] = 255;
			}  // End j

		} // End i

	} // End store
}

/*
===============
R_UploadLightmap -- johnfitz -- uploads the modified lightmap to opengl if necessary

assumes lightmap texture is already bound
===============
*/
static void R_UploadLightmap_Changed_Region (const msurface_t *surf, int in_lightmapnum)
{
	int lightmapnum = !surf ? in_lightmapnum : surf->lightmaptexturenum;

	if (!lightmap[lightmapnum].modified)
		return;

	lightmap[lightmapnum].modified = false;

	glTexSubImage2D(
		GL_TEXTURE_2D, 0, 0,
		lightmap[lightmapnum].rectchange.t,
		LIGHTMAPS_BLOCK_WIDTH,
		lightmap[lightmapnum].rectchange.h,
		GL_BGRA,
		GL_UNSIGNED_INT_8_8_8_8_REV,
		lightmap[lightmapnum].lightmaps + (lightmap[lightmapnum].rectchange.t * LIGHTMAPS_BLOCK_WIDTH * LIGHTMAPS_BYTES_4)
	);

	// Restore update region to the whole
	lightmap[lightmapnum].rectchange.l = LIGHTMAPS_BLOCK_WIDTH;
	lightmap[lightmapnum].rectchange.t = LIGHTMAPS_BLOCK_HEIGHT;
	lightmap[lightmapnum].rectchange.h = 0;
	lightmap[lightmapnum].rectchange.w = 0;
	rs_dynamiclightmaps++;
}

void R_UploadLightmaps_Modified (void)
{
	int i;

	for (i = 0; i < MAX_FITZQUAKE_LIGHTMAPS; i++)
	{
		if (!lightmap[i].modified)
			continue;

		GL_Bind (lightmap[i].texture); // Texture must be bound
		R_UploadLightmap_Changed_Region (NULL, i);
	}
}

/*
================
R_RebuildAllLightmaps -- johnfitz -- called when gl_overbright gets toggled
================
*/
void R_RebuildAllLightmaps (void)
{
	int			i, j;
	qmodel_t		*mod;
	msurface_t	*fa;

	if (!cl.worldmodel) // is this the correct test?
		return;

	//for each surface in each model, rebuild lightmap with new scale
	for (i=1; i<MAX_FITZQUAKE_MODELS; i++)
	{
		if (!(mod = cl.model_precache[i]))
			continue;
		fa = &mod->surfaces[mod->firstmodelsurface];
		for (j=0; j<mod->nummodelsurfaces; j++, fa++)
		{
			if (fa->flags & SURF_DRAWTILED)
				continue;
			R_BuildLightMap (fa);
		}
	}

	//for each lightmap, upload it
	for (i=0; i<MAX_FITZQUAKE_LIGHTMAPS; i++)
	{
		if (!lightmap[i].allocated[0])
			break;
		GL_Bind (lightmap[i].texture);
		glTexSubImage2D (
			GL_TEXTURE_2D, 0, 0,
			0,
			LIGHTMAPS_BLOCK_WIDTH,
			LIGHTMAPS_BLOCK_HEIGHT,
			GL_BGRA,
			GL_UNSIGNED_INT_8_8_8_8_REV,
			lightmap[i].lightmaps
		);
	}
}



/*
=============================================================

	STAINMAPS - ADAPTED FROM FTEQW

=============================================================
*/

cvar_t r_stains_fadeamount = {"r_stains_fadeamount", "1", CVAR_NONE};
cvar_t r_stains_fadetime = {"r_stains_fadetime", "1", CVAR_NONE};
cvar_t r_stains = {"r_stains", "1", CVAR_ARCHIVE};


void Stains_WipeStains_NewMap (void)
{
	int i;
	for (i = 0; i < MAX_FITZQUAKE_LIGHTMAPS; i++)
	{
		memset(lightmap[i].stainmaps, 0, sizeof(lightmap[i].stainmaps));
	}
}


// Fade out the stain map data lightmap[x].stainmap until decays to 255 (no stain)
// Needs to occur before
void Stain_FrameSetup_LessenStains(void)
{
	static		float time;

	if (!r_stains.value)
		return;

	time += host_frametime;

	if (time < r_stains_fadetime.value)
		return;

	// Time for stain fading.  Doesn't occur every frame
	time-=r_stains_fadetime.value;

	{
		int			decay_factor	= r_stains_fadeamount.value;
		msurface_t	*surf = cl.worldmodel->surfaces;
		int			i;

		for (i=0 ; i<cl.worldmodel->numsurfaces ; i++, surf++)
		{
			if (surf->stained)
			{
				int			stride			=	(LIGHTMAPS_BLOCK_WIDTH-surf->smax)*BLOCKLITE_BYTES_3;
				byte		*stain			=	lightmap[surf->lightmaptexturenum].stainmaps +
											     (surf->light_t * LIGHTMAPS_BLOCK_WIDTH + surf->light_s) * BLOCKLITE_BYTES_3;
				int			s, t;

				surf->cached_dlight=-1;		// nice hack here...
				surf->stained = false;		// Assume not stained until we know otherwise

				for (t = 0 ; t<surf->tmax ; t++, stain+=stride)
				{
					int smax_times_3 = surf->smax * BLOCKLITE_BYTES_3;
					for (s=0 ; s<smax_times_3 ; s++, stain++)
					{
						if (*stain < decay_factor)
						{
							*stain = 0;		//reset to 0
							continue;
						}

						//eventually decay to 0
						*stain -= decay_factor;
						surf->stained=true;
					}
				}
				// End of surface stained check
			}

			// Next surface

		}
	}
}

// Fill in the stain map data; lightmap[x].stainmap
static void sStain_AddStain_StainNodeRecursive_StainSurf (msurface_t *surf, const vec3_t origin, float tint, float radius /* float *parms*/)
{
	if (surf->lightmaptexturenum < 0)		// Baker: Does this happen?
		return;


	{
		float		dist;
		float		rad = radius;
		float		minlight=0;

		const int			lim = 240;
		mtexinfo_t	*tex = surf->texinfo;
		byte		*stainbase;

		int			s,t, i, sd, td;
		float		change, amm;
		vec3_t		impact, local;

		stainbase = lightmap[surf->lightmaptexturenum].stainmaps;	// Each lightmap has own special slot with a stainmap ... pointer?
		stainbase += (surf->light_t * LIGHTMAPS_BLOCK_WIDTH + surf->light_s) * BLOCKLITE_BYTES_3;

		// Calculate impact
		dist = DotProduct (origin, surf->plane->normal) - surf->plane->dist;
		rad -= fabs(dist);
		minlight = 0;

		if (rad < minlight)	//not hit
			return;

		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = origin[i] - surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];


		for (t = 0 ; t<surf->tmax ; t++, stainbase += BLOCKLITE_BYTES_3 * LIGHTMAPS_BLOCK_WIDTH /*stride*/)
		{
			td = local[1] - t*16;  if (td < 0) td = -td;

			for (s=0 ; s<surf->smax ; s++)
			{
				sd = local[0] - s*16;  if (sd < 0) sd = -sd;

				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
				{
					//amm = stainbase[s*3+0]*(rad - dist)*tint;
					amm = (rad - dist);
					change = stainbase[s*3+0] + amm * tint  /* parms[4+x] */;
					stainbase[s*3+0] = CLAMP (0, change, lim);
					stainbase[s*3+1] = CLAMP (0, change, lim);
					stainbase[s*3+2] = CLAMP (0, change, lim);

					surf->stained = true;
				}
			}

		}
	}
	// Mark it as a dynamic light so the lightmap gets updated

	if (surf->stained)
	{
		static int mystainnum;
		surf->cached_dlight=-1;
		if (!surf->stainnum)
			surf->stainnum = mystainnum++;
	}
}


//combination of R_AddDynamicLights and R_MarkLights
static void sStain_AddStain_StainNodeRecursive (mnode_t *node, const vec3_t origin, float tint, float radius)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;

	if (node->contents < 0)
		return;

	splitplane = node->plane;
	dist = DotProduct (origin, splitplane->normal) - splitplane->dist;

	if (dist > radius)
	{
		sStain_AddStain_StainNodeRecursive (node->children[0], origin, tint, radius);
		return;
	}

	if (dist < -radius)
	{
		sStain_AddStain_StainNodeRecursive (node->children[1], origin, tint, radius);
		return;
	}

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags &~ (SURF_DRAWTILED|SURF_PLANEBACK))
			continue;

		sStain_AddStain_StainNodeRecursive_StainSurf(surf, origin, tint, radius);
	}


	sStain_AddStain_StainNodeRecursive (node->children[0], origin, tint, radius);
	sStain_AddStain_StainNodeRecursive (node->children[1], origin, tint, radius);

}




// This builds the stain map
void Stain_AddStain(const vec3_t origin, const float tint, /*float red, float green, float blue,*/ const float in_radius)
{
	entity_t	*pe;
	int i;
	vec3_t		adjorigin;

	// Slight randomization.  Use 80 of the radius plus or minus 20%
	float		radius = in_radius * .90 + (sin(cl.ctime*3)*.20);

	if (!cl.worldmodel || !r_stains.value)
		return;

	// Stain the world surfaces
	sStain_AddStain_StainNodeRecursive(cl.worldmodel->nodes+cl.worldmodel->hulls[0].firstclipnode, origin, tint, radius);

	//now stain bsp models other than world.

	for (i=1 ; i< cl.num_entities ; i++)	//0 is world...
	{
		pe = &cl_entities[i];
		if (pe->model && pe->model->surfaces == cl.worldmodel->surfaces)
		{
			VectorSubtract (origin, pe->origin, adjorigin);
			if (pe->angles[0] || pe->angles[1] || pe->angles[2])
			{
				vec3_t f, r, u, temp;
				AngleVectors(pe->angles, f, r, u);
				VectorCopy(adjorigin, temp);
				adjorigin[0] = DotProduct(temp, f);
				adjorigin[1] = -DotProduct(temp, r);
				adjorigin[2] = DotProduct(temp, u);
			}

			sStain_AddStain_StainNodeRecursive (pe->model->nodes+pe->model->hulls[0].firstclipnode, adjorigin, tint, radius);
		}
	}
}



