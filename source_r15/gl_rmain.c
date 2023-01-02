/*  Note that this file is likely not done yet

Versus Quakespasm:   Various.  Extensive ...

*/

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
// r_main.c

#include "quakedef.h"



vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

#ifdef SUPPORTS_MIRRORS // Baker change
int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror_in_scene;
mplane_t	*mirror_plane;
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change

//johnfitz -- rendering statistics
int rs_brushpolys, rs_aliaspolys, rs_skypolys, rs_particles, rs_fogpolys;
int rs_dynamiclightmaps, rs_brushpasses, rs_aliaspasses, rs_skypasses;
float rs_megatexels;

qboolean	envmap;				// true during envmap command capture

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

float r_fovx, r_fovy; //johnfitz -- rendering fov may be different becuase of r_waterwarp and r_stereo

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


cvar_t	r_norefresh = {"r_norefresh","0",CVAR_NONE};
cvar_t	r_drawentities = {"r_drawentities","1",CVAR_NONE};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1",CVAR_NONE};
cvar_t	r_speeds = {"r_speeds","0",CVAR_NONE};
cvar_t	r_fullbright = {"r_fullbright","0",CVAR_NONE};
cvar_t	r_lightmap = {"r_lightmap","0",CVAR_NONE};
cvar_t	r_shadows = {"r_shadows","0",CVAR_NONE};
cvar_t	r_wateralpha = {"r_wateralpha","1",CVAR_ARCHIVE};
#ifdef SUPPORTS_MIRRORS // Baker change
cvar_t	r_mirroralpha = {"r_mirroralpha","1"};
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change
cvar_t	r_dynamic = {"r_dynamic","1",CVAR_NONE};
cvar_t	r_novis = {"r_novis","0",CVAR_NONE};
cvar_t	r_lockfrustum =	{"r_lockfrustum","0",CVAR_NONE};
cvar_t	r_lockpvs =	{"r_lockpvs","0",CVAR_NONE};

cvar_t	r_waterripple = {"r_waterripple","0",CVAR_NONE};
cvar_t	r_watercshift = {"r_watercolor","0",CVAR_NONE};
cvar_t	r_lavacshift = {"r_lavacolor","0",CVAR_NONE};
cvar_t	r_slimecshift = {"r_slimecolor","0",CVAR_NONE};

cvar_t	gl_finish = {"gl_finish","0",CVAR_NONE};
cvar_t	gl_clear = {"gl_clear","0",CVAR_NONE};
cvar_t	gl_cull = {"gl_cull","1",CVAR_NONE};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1",CVAR_NONE};
cvar_t	gl_affinemodels = {"gl_affinemodels","0",CVAR_NONE};
cvar_t	gl_polyblend = {"gl_polyblend","1",CVAR_NONE};
cvar_t	gl_flashblend = {"gl_flashblend","1",CVAR_ARCHIVE};
cvar_t	gl_playermip = {"gl_playermip","0",CVAR_NONE};
cvar_t	gl_nocolors = {"gl_nocolors","0",CVAR_NONE};

//johnfitz -- new cvars
cvar_t	r_stereo = {"r_stereo","0",CVAR_NONE};
cvar_t	r_stereodepth = {"r_stereodepth","128",CVAR_NONE};
cvar_t	r_clearcolor = {"r_clearcolor","2", CVAR_ARCHIVE};
cvar_t	r_drawflat = {"r_drawflat","0",CVAR_NONE};
cvar_t	r_flatlightstyles = {"r_flatlightstyles", "0",CVAR_NONE};
cvar_t	gl_fullbrights = {"gl_fullbrights", "1", CVAR_NONE};
cvar_t	gl_farclip = {"gl_farclip", "16384", CVAR_NONE};
cvar_t	gl_overbright = {"gl_overbright", "1", CVAR_NONE};
cvar_t	gl_overbright_models = {"gl_overbright_models", "1", CVAR_NONE};
cvar_t	r_oldskyleaf = {"r_oldskyleaf", "0",CVAR_NONE};
cvar_t	r_drawworld = {"r_drawworld", "1",CVAR_NONE};
cvar_t	r_showtris = {"r_showtris", "0",CVAR_NONE};
cvar_t	r_showbboxes = {"r_showbboxes", "0",CVAR_NONE};
cvar_t	r_lerpmodels = {"r_lerpmodels", "1",CVAR_NONE};
cvar_t	r_lerpmove = {"r_lerpmove", "1",CVAR_NONE};
cvar_t	r_nolerp_list = {"r_nolerp_list", "progs/flame.mdl,progs/flame2.mdl,progs/braztall.mdl,progs/brazshrt.mdl,progs/longtrch.mdl,progs/flame_pyre.mdl,progs/v_saw.mdl,progs/v_xfist.mdl,progs/h2stuff/newfire.mdl",CVAR_NONE};
//johnfitz
cvar_t	r_noshadow_list = {"r_noshadow_list", "progs/flame.mdl,progs/flame2.mdl,progs/bolt.mdl,progs/bolt1.mdl,progs/bolt2.mdl,progs/bolt3.mdl,progs/laser.mdl",CVAR_NONE};
cvar_t	r_fbrighthack_list = {"r_fbrighthack_list", "progs/flame.mdl,progs/flame2.mdl,progs/boss.mdl",CVAR_NONE};
cvar_t r_texprefix_fence = {"r_texprefix_fence", "{",CVAR_NONE}; // Fence texture.  Alpha mask on color 255.
cvar_t r_texprefix_envmap = {"r_texprefix_envmap", "envmap_",CVAR_NONE}; // sphere mapped
cvar_t r_texprefix_mirror = {"r_texprefix_mirror", "mirror_",CVAR_NONE}; // mirror
cvar_t r_texprefix_tele = {"r_texprefix_tele", "*tele",CVAR_NONE}; // To prevent r_wateralpha etc from affecting teleporters (Lava, Slime are own contents type but a tele isn't}.
cvar_t r_texprefix_scrollx = {"r_texprefix_scrollx", "scrollx",CVAR_NONE}; // Scroll left -> right
cvar_t r_texprefix_scrolly = {"r_texprefix_scrolly", "scrolly",CVAR_NONE}; // Scroll up -> down
cvar_t	gl_zfix = {"gl_zfix", "0", CVAR_NONE}; // QuakeSpasm z-fighting fix

qboolean r_drawflat_cheatsafe, r_fullbright_cheatsafe, r_lightmap_cheatsafe, r_drawworld_cheatsafe; //johnfitz

/*
=================
R_CullBox -- johnfitz -- replaced with new function from lordhavoc

Returns true if the box is completely outside the frustum
=================
*/
qboolean R_CullBox (vec3_t emins, vec3_t emaxs)
{
	int i;
	mplane_t *p;
	for (i = 0;i < 4;i++)
	{
		p = frustum + i;
		switch(p->signbits)
		{
		default:
		case 0:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		}
	}
	return false;
}
/*
===============
R_CullModelForEntity -- johnfitz -- uses correct bounds based on rotation
===============
*/
qboolean R_CullModelForEntity (entity_t *e)
{
	vec3_t mins, maxs;

	if (e->angles[0] || e->angles[2]) //pitch or roll
	{
		VectorAdd (e->origin, e->model->rmins, mins);
		VectorAdd (e->origin, e->model->rmaxs, maxs);
	}
	else if (e->angles[1]) //yaw
	{
		VectorAdd (e->origin, e->model->ymins, mins);
		VectorAdd (e->origin, e->model->ymaxs, maxs);
	}
	else //no rotation
	{
		VectorAdd (e->origin, e->model->mins, mins);
		VectorAdd (e->origin, e->model->maxs, maxs);
	}

	return R_CullBox (mins, maxs);
}

/*
===============
R_RotateForEntity -- johnfitz -- modified to take origin and angles instead of pointer to entity
===============
*/
void R_RotateForEntity (vec3_t origin, vec3_t angles)
{
	glTranslatef (origin[0],  origin[1],  origin[2]);
	glRotatef (angles[1],  0, 0, 1);
	glRotatef (-angles[0],  0, 1, 0);
	glRotatef (angles[2],  1, 0, 0);
}

/*
=============
GL_PolygonOffset -- johnfitz

negative offset moves polygon closer to camera
=============
*/
void GL_PolygonOffset (int offset)
{
	if (offset > 0)
	{
		glEnable (GL_POLYGON_OFFSET_FILL);
		glEnable (GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(1, offset);
	}
	else if (offset < 0)
	{
		glEnable (GL_POLYGON_OFFSET_FILL);
		glEnable (GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(-1, offset);
	}
	else
	{
		glDisable (GL_POLYGON_OFFSET_FILL);
		glDisable (GL_POLYGON_OFFSET_LINE);
	}
}

//==============================================================================
//
// SETUP FRAME
//
//==============================================================================

int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

/*
===============
TurnVector -- johnfitz

turn forward towards side on the plane defined by forward and side
if angle = 90, the result will be equal to side
assumes side and forward are perpendicular, and normalized
to turn away from side, use a negative angle
===============
*/
#define DEG2RAD( a ) ( (a) * M_PI_DIV_180 )
void TurnVector (vec3_t out, const vec3_t forward, const vec3_t side, float angle)
{
	float scale_forward, scale_side;

	scale_forward = cos( DEG2RAD( angle ) );
	scale_side = sin( DEG2RAD( angle ) );

	out[0] = scale_forward*forward[0] + scale_side*side[0];
	out[1] = scale_forward*forward[1] + scale_side*side[1];
	out[2] = scale_forward*forward[2] + scale_side*side[2];
}

/*
===============
R_SetFrustum -- johnfitz -- rewritten
===============
*/
void R_SetFrustum (float fovx, float fovy)
{
	int		i;

	if (r_lockfrustum.value)
		return;		// Do not update!

	if (r_stereo.value)
		fovx += 10; //silly hack so that polygons don't drop out becuase of stereo skew

	TurnVector(frustum[0].normal, vpn, vright, fovx/2 - 90); //left plane
	TurnVector(frustum[1].normal, vpn, vright, 90 - fovx/2); //right plane
	TurnVector(frustum[2].normal, vpn, vup, 90 - fovy/2); //bottom plane
	TurnVector(frustum[3].normal, vpn, vup, fovy/2 - 90); //top plane

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal); //FIXME: shouldn't this always be zero?
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

/*
=============
GL_SetFrustum -- johnfitz -- written to replace MYgluPerspective
=============
*/

#define NEARCLIP 1
float frustum_skew = 0.0; //used by r_stereo
void GL_SetFrustum(float fovx, float fovy)
{
	float xmax, ymax;
	xmax = NEARCLIP * tan( fovx * M_PI / 360.0 );
	ymax = NEARCLIP * tan( fovy * M_PI / 360.0 );
	glFrustum(-xmax + frustum_skew, xmax + frustum_skew, -ymax, ymax, NEARCLIP, gl_farclip.value);
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
#if 0 // Baker:  For testing
	vrect_t area =  {glx + r_refdef.vrect.x, gly + glheight - r_refdef.vrect.y - r_refdef.vrect.height, r_refdef.vrect.width, r_refdef.vrect.height};
#endif

	//johnfitz -- rewrote this section
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glViewport (glx + r_refdef.vrect.x,
				gly + glheight - r_refdef.vrect.y - r_refdef.vrect.height,
				r_refdef.vrect.width,
				r_refdef.vrect.height);
	//johnfitz

    GL_SetFrustum (r_fovx, r_fovy); //johnfitz -- use r_fov* vars

#ifdef SUPPORTS_MIRRORS // Baker change
//	glCullFace(GL_BACK); //johnfitz -- glquake used CCW with backwards culling -- let's do it right
// mirror: 6.  If there is a mirror, reverse culling (DONE)
//	if (mirror_in_scene)
//	{
// Baker: Since we aren't actually calling R_SetupGL in R_Mirror This won't happen
//		if (mirror_plane->normal[2])
//			glScalef (1, -1, 1);
//		else
//			glScalef (-1, 1, 1);
//		glCullFace(GL_FRONT);
//	}
//	else
//		glCullFace(GL_BACK);
#pragma message ("Baker: Mirror in R_SetupGL ?")
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up
	glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

/*
=============
R_Clear -- johnfitz -- rewritten and gutted
=============
*/
void R_Clear (void)
{
	unsigned int clearbits;

	clearbits = GL_DEPTH_BUFFER_BIT;
	if (gl_clear.value || renderer.isIntelVideo) 
		clearbits |= GL_COLOR_BUFFER_BIT; //intel video workarounds from Baker
	if (renderer.gl_stencilbits) 
		clearbits |= GL_STENCIL_BUFFER_BIT;
	glClear (clearbits);

#ifdef SUPPORTS_MIRRORS // Baker change
// Mirror: 7
//	if (r_mirroralpha.value < 1.0f)
//		glDepthRange (0, 0.5); // Might need to play with viewmodel 0, 0.3 thing
//	else
	
	glDepthRange (Q_GLDEPTHRANGE_MIN, Q_GLDEPTHRANGE_MAX); 
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change

}

/*
===============
R_SetupScene -- johnfitz -- this is the stuff that needs to be done once per eye in stereo mode
===============
*/
void R_SetupScene (void)
{
	Stain_FrameSetup_LessenStains ();
	R_PushDlights_World (); // temp
	R_AnimateLight ();
	r_framecount++;
	R_SetupGL ();
}

/*
===============
R_SetupView -- johnfitz -- this is the stuff that needs to be done once per frame, even in stereo mode
===============
*/
#ifdef SUPPORTS_MIRRORS // Baker change
qboolean in_mirror_draw = false;
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change
void R_SetupView (void)
{
	Fog_SetupFrame (); //johnfitz

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;

	if (r_lockpvs.value == 0)  // Don't update if PVS is locked
		if (chase_active.value && chase_mode) // Camera position isn't visibility leaf!
			r_viewleaf = Mod_PointInLeaf (nonchase_origin, cl.worldmodel);
		else
			r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

#ifdef SUPPORTS_MIRRORS // Baker change
	if (in_mirror_draw == false)
	{
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change
		// Mirror shouldn't do this

		V_SetContentsColor (r_viewleaf->contents);
		V_CalcBlend ();
#ifdef SUPPORTS_MIRRORS // Baker change
	}
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change


	//johnfitz -- calculate r_fovx and r_fovy here
	r_fovx = r_refdef.fov_x;
	r_fovy = r_refdef.fov_y;
	if (r_waterwarp.value)
	{
		int contents = r_viewleaf->contents; // Baker was: Mod_PointInLeaf (r_origin, cl.worldmodel)->contents;
		if (contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA)
		{
			//variance is a percentage of width, where width = 2 * tan(fov / 2) otherwise the effect is too dramatic at high FOV and too subtle at low FOV.  what a mess!
			r_fovx = atan(tan(DEG2RAD(r_refdef.fov_x) / 2) * (0.97 + sin(cl.ctime * 1.5) * 0.03)) * 2 / M_PI_DIV_180;
			r_fovy = atan(tan(DEG2RAD(r_refdef.fov_y) / 2) * (1.03 - sin(cl.ctime * 1.5) * 0.03)) * 2 / M_PI_DIV_180;
		}
	}
	//johnfitz

#ifdef SUPPORTS_MIRRORS // Baker change
	if (in_mirror_draw == false)
	{
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change
		R_SetFrustum (r_fovx, r_fovy); //johnfitz -- use r_fov* vars

		R_MarkSurfaces (); //johnfitz -- create texture chains from PVS

		R_CullSurfaces (); //johnfitz -- do after R_SetFrustum and R_MarkSurfaces

		R_UpdateWarpTextures (); //johnfitz -- do this before R_Clear

		R_Clear ();

		//johnfitz -- cheat-protect some draw modes
		r_drawflat_cheatsafe = r_fullbright_cheatsafe = r_lightmap_cheatsafe = false;

		// Baker: Strangely, FitzQuake 0.85 doesn't need to do this here to make sure that
		// a fullbright map in multiplayer is fullbrighted like it should be.
		// But for reasons beyond explanation, Mark V needs this.  The only factor is
		// cl.maxlients.

		if (!cl.worldmodel->lightdata)
			r_fullbright_cheatsafe = true;

		r_drawworld_cheatsafe = true;
		if (cl.maxclients == 1)
		{
			if (!r_drawworld.value) r_drawworld_cheatsafe = false;

			if (r_drawflat.value) r_drawflat_cheatsafe = true;
			else if (r_fullbright.value || !cl.worldmodel->lightdata) r_fullbright_cheatsafe = true;
			else if (r_lightmap.value) r_lightmap_cheatsafe = true;
		}
		//johnfitz
#ifdef SUPPORTS_MIRRORS // Baker change
	}
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change
}

//==============================================================================
//
// RENDER VIEW
//
//==============================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (qboolean alphapass) //johnfitz -- added parameter
{
	int		i;

	if (!r_drawentities.value)
		return;

	//johnfitz -- sprites are not a special case
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		//johnfitz -- if alphapass is true, draw only alpha entites this time
		//if alphapass is false, draw only nonalpha entities this time
		if ((ENTALPHA_DECODE(currententity->alpha) < 1 && !alphapass) ||
			(ENTALPHA_DECODE(currententity->alpha) == 1 && alphapass))
			continue;

		//johnfitz -- chasecam
		if (currententity == &cl_entities[cl.viewentity] && chase_active.value)
			currententity->angles[0] *= 0.3;
		//johnfitz

		switch (currententity->model->type)
		{
			case mod_alias:
					R_DrawAliasModel (currententity);
				break;
			case mod_brush:
					R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
					R_DrawSpriteModel (currententity);
				break;
		}
	}
}

#ifdef SUPPORTS_MIRRORS // Baker change

void R_DrawPlayerModel (entity_t *ent) //Baker --- for mirrors
{
	if (!r_drawentities.value)
		return;

	currententity = ent;
#pragma message ("Alpha?")
	switch (currententity->model->type)
	{
		case mod_alias:
				R_DrawAliasModel (currententity);
			break;
		case mod_brush:
				R_DrawBrushModel (currententity);
			break;
		case mod_sprite:
				R_DrawSpriteModel (currententity);
			break;
	}
}
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change


/*
=============
R_DrawViewModel -- johnfitz -- gutted
=============
*/
void R_DrawViewModel (void)
{
	if (!r_drawviewmodel.value || !r_drawentities.value || chase_active.value || envmap)
		return;

	if (cl.items & IT_INVISIBILITY || cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	//johnfitz -- this fixes a crash
	if (currententity->model->type != mod_alias)
		return;
	//johnfitz

	// hack the depth range to prevent view model from poking into walls
	glDepthRange (Q_GLDEPTHRANGE_MIN, Q_GLDEPTHRANGE_GUN); 
	R_DrawAliasModel (currententity);
	glDepthRange (Q_GLDEPTHRANGE_MIN, Q_GLDEPTHRANGE_MAX); 
}

/*
================
R_EmitWirePoint -- johnfitz -- draws a wireframe cross shape for point entities
================
*/
void R_EmitWirePoint (vec3_t origin)
{
	int size=8;

	glBegin (GL_LINES);
	glVertex3f (origin[0]-size, origin[1], origin[2]);
	glVertex3f (origin[0]+size, origin[1], origin[2]);
	glVertex3f (origin[0], origin[1]-size, origin[2]);
	glVertex3f (origin[0], origin[1]+size, origin[2]);
	glVertex3f (origin[0], origin[1], origin[2]-size);
	glVertex3f (origin[0], origin[1], origin[2]+size);
	glEnd ();
}

/*
================
R_EmitWireBox -- johnfitz -- draws one axis aligned bounding box
================
*/
void R_EmitWireBox (vec3_t mins, vec3_t maxs)
{
	glBegin (GL_QUAD_STRIP);
	glVertex3f (mins[0], mins[1], mins[2]);
	glVertex3f (mins[0], mins[1], maxs[2]);
	glVertex3f (maxs[0], mins[1], mins[2]);
	glVertex3f (maxs[0], mins[1], maxs[2]);
	glVertex3f (maxs[0], maxs[1], mins[2]);
	glVertex3f (maxs[0], maxs[1], maxs[2]);
	glVertex3f (mins[0], maxs[1], mins[2]);
	glVertex3f (mins[0], maxs[1], maxs[2]);
	glVertex3f (mins[0], mins[1], mins[2]);
	glVertex3f (mins[0], mins[1], maxs[2]);
	glEnd ();
}



static void R_EmitBox_Faces (const vec3_t mins, const vec3_t maxs)
{
	glBegin (GL_QUADS);

    // Front Face
    glVertex3f (mins[0], mins[1], maxs[2]);
    glVertex3f (maxs[0], mins[1], maxs[2]);
    glVertex3f (maxs[0], maxs[1], maxs[2]);
    glVertex3f (mins[0], maxs[1], maxs[2]);
    // Back Face
    glVertex3f (mins[0], mins[1], mins[2]);
    glVertex3f (mins[0], maxs[1], mins[2]);
    glVertex3f (maxs[0], maxs[1], mins[2]);
    glVertex3f (maxs[0], mins[1], mins[2]);
    // Top Face
    glVertex3f (mins[0], maxs[1], mins[2]);
    glVertex3f (mins[0], maxs[1], maxs[2]);
    glVertex3f (maxs[0], maxs[1], maxs[2]);
    glVertex3f (maxs[0], maxs[1], mins[2]);
    // Bottom Face
    glVertex3f (mins[0], mins[1], mins[2]);
    glVertex3f (maxs[0], mins[1], mins[2]);
    glVertex3f (maxs[0], mins[1], maxs[2]);
    glVertex3f (mins[0], mins[1], maxs[2]);
    // Right face
    glVertex3f (maxs[0], mins[1], mins[2]);
    glVertex3f (maxs[0], maxs[1], mins[2]);
    glVertex3f (maxs[0], maxs[1], maxs[2]);
    glVertex3f (maxs[0], mins[1], maxs[2]);
    // Left Face
    glVertex3f (mins[0], mins[1], mins[2]);
    glVertex3f (mins[0], mins[1], maxs[2]);
    glVertex3f (mins[0], maxs[1], maxs[2]);
    glVertex3f (mins[0], maxs[1], mins[2]);

	glEnd ();
}

Point3D R_EmitSurfaceHighlight (entity_t* enty, msurface_t* surf, rgba4_t color, int style)
{
	Point3D center;
	float *verts = surf->polys->verts[0];

	vec3_t mins = { 99999,  99999,  99999};
	vec3_t maxs = {-99999, -99999, -99999};
	int i;

	if (enty)
	{
		glPushMatrix ();
		R_RotateForEntity (enty->origin, enty->angles);
	}

	if (style == OUTLINED_POLYGON)	// Set to lines
		glPolygonMode	(GL_FRONT_AND_BACK, GL_LINE);

	glDisable (GL_TEXTURE_2D);
	glEnable (GL_POLYGON_OFFSET_FILL);
	glDisable (GL_CULL_FACE);
	glColor4f (color[0], color[1], color[2], color[3]);
	glDisable (GL_DEPTH_TEST);
	glEnable (GL_BLEND);

	glBegin (GL_POLYGON);
	
	// Draw polygon while collecting information for the center.
	for (i = 0 ; i < surf->polys->numverts ; i++, verts+= VERTEXSIZE)
	{
		VectorExtendLimits (verts, mins, maxs);
		glVertex3fv (verts);
	}
	glEnd ();

	glEnable (GL_TEXTURE_2D);
	glDisable (GL_POLYGON_OFFSET_FILL);
	glEnable (GL_CULL_FACE);
	glColor4f (1, 1, 1, 1);
	glEnable (GL_DEPTH_TEST);
	glDisable (GL_BLEND);

	if (style == OUTLINED_POLYGON)	// Set to lines
		glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	if (enty)
		glPopMatrix ();

	// Calculate the center
	VectorAverage (mins, maxs, center.vec3);

	return center;
		
}

static void R_EmitBox_CalcCoordsMinsMaxs (const vec3_t centerpoint, const vec3_t xyz_mins, const vec3_t xyz_maxs, const float ExpandSize, vec3_t mins, vec3_t maxs)
{	// Calculate mins and maxs
	int		calculation_type = !centerpoint ? 0 : (!xyz_mins ? 2 : 1);

	switch (calculation_type)
	{
	case 0:		// No centerpoint so these represent absolute coordinates
				VectorCopy (xyz_mins, mins);
				VectorCopy (xyz_maxs, maxs);
				break;

				// We have a centerpoint so the other 2 coordinates are relative to the center
	case 1:		VectorAdd (centerpoint, xyz_mins, mins);
				VectorAdd (centerpoint, xyz_maxs, maxs);
				break;

	case 2:		// No mins or maxs (point type entity)
				VectorCopy (centerpoint, mins);
				VectorCopy (centerpoint, maxs);
				break;
	}

	// Optionally, adjust the size a bit like pull the sides back just a little or expand it
#define VectorAddFloat(a, b, c)				((c)[0] = (a)[0] + (b), (c)[1] = (a)[1] + (b), (c)[2] = (a)[2] + (b))
	if (ExpandSize)		{ VectorAddFloat (mins,  -ExpandSize, mins);	VectorAddFloat (maxs, ExpandSize, maxs); }

}

vec3_t currentbox_mins, currentbox_maxs;
void R_EmitBox (const vec3_t myColor, const vec3_t centerpoint, const vec3_t xyz_mins, const vec3_t xyz_maxs, const int bTopMost, const int bLines, const float Expandsize)
{
	float	pointentity_size = 8.0f;
	float	smallster = 0.10f;
	vec3_t	mins, maxs;

	// Calculate box size based on what we received (i.e., if we have a centerpoint use it otherwise it is absolute coords)
	R_EmitBox_CalcCoordsMinsMaxs (centerpoint, xyz_mins, xyz_maxs, Expandsize, /* outputs -> */ mins, maxs);

	// Setup to draw

	glDisable			(GL_TEXTURE_2D);
	glEnable			(GL_POLYGON_OFFSET_FILL);
	glDisable			(GL_CULL_FACE);
	glColor4fv		(myColor);

	if (myColor[3])	// If no alpha then it is solid
	{
		glEnable		(GL_BLEND);
		glDepthMask	(GL_FALSE);
	}

	if (bTopMost)	// Draw topmost if specified
		glDisable		(GL_DEPTH_TEST);

	if (bLines)		// Set to lines
		glPolygonMode	(GL_FRONT_AND_BACK, GL_LINE);

	if (bLines)
		R_EmitWireBox (mins, maxs);
	else
		R_EmitBox_Faces (mins, maxs);

	//	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);		// This is a GL_SetupState default
	glEnable			(GL_TEXTURE_2D);
	glDisable			(GL_POLYGON_OFFSET_FILL);
	glEnable			(GL_CULL_FACE);
	glColor4f			(1, 1, 1, 1);

	if (myColor[3])	// If no alpha then it is solid
	{
		glDisable		(GL_BLEND);
		glDepthMask	(GL_TRUE);
	}

	if (bTopMost)	// Draw topmost if specified
		glEnable		(GL_DEPTH_TEST);

	if (bLines)		// Set to lines
		glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	VectorCopy (mins, currentbox_mins);
	VectorCopy (maxs, currentbox_maxs);
}




void R_EmitCaption_Text_Get_Columns_And_Rows (const char *myText, int *return_columns, int *return_rows)
{
	int i, maxcolumn = 0, numrows = 1, curcolumn = 0;

	for (i=0 ; i < (int)strlen(myText) ; i++)
	{
		if (myText[i] == '\b')
			continue;	// Bronzing toggle ... we ignore this

		if (myText[i] == '\n')
		{
			numrows ++;
			curcolumn = 0;
			continue;
		}

		curcolumn ++;
		if (curcolumn > maxcolumn)
			(maxcolumn = curcolumn);
	}

	*return_columns = maxcolumn;
	*return_rows = numrows;

}

typedef struct fRect_s {
	float	left, top, right, bottom;
} fRect_t;


void R_EmitCaption (vec3_t location, const char *caption, qboolean backgrounded)
{
	extern gltexture_t *char_texture;

	int			string_length = strlen(caption);
	float		charwidth	= 8;
	float		charheight  = 8;

	vec3_t		point;
	vec3_t		right, up;

	int			string_columns, string_rows;
	float		string_width, string_height; // pixels
	fRect_t		captionbox;
	int			i, x, y;
	qboolean	bronze = false;

	// Step 1: Detemine Rows and columns
	R_EmitCaption_Text_Get_Columns_And_Rows (caption, &string_columns, &string_rows);

	string_width = (float)string_columns * charwidth;
	string_height = (float)string_rows * charheight;

	// Step 2: Calculate bounds of rectangle

	captionbox.top	  = -(float)string_height/2;
	captionbox.bottom =  (float)string_height/2;
	captionbox.left   = -(float)string_width /2;
	captionbox.right  =  (float)string_width /2;

	// Copy the view origin up and right angles
	VectorCopy (vup, up);
	VectorCopy (vright, right);

	if (backgrounded)
	{

		// Draw box
		glDisable (GL_TEXTURE_2D);

		glEnable (GL_POLYGON_OFFSET_FILL);
		glDisable (GL_CULL_FACE);

		glDepthRange (Q_GLDEPTHRANGE_GUN, Q_GLDEPTHRANGE_MAX - 0.0125);  // 0.3 to 0.5 ish
		glColor4f (0,0,0,1);
		glBegin (GL_QUADS);

		// top left
		VectorMA (location, captionbox.top-2, up, point);
		VectorMA (point, captionbox.left+4, right, point);
		glVertex3fv (point);

		// top, right
		VectorMA (location, captionbox.top-2, up, point);
		VectorMA (point, captionbox.right+12, right, point);
		glVertex3fv (point);

		// bottom right

		VectorMA (location, captionbox.bottom+1, up, point);
		VectorMA (point, captionbox.right+12, right, point);
		glVertex3fv (point);

		// bottom, left
		VectorMA (location, captionbox.bottom+1, up, point);
		VectorMA (point, captionbox.left+4, right, point);
		glVertex3fv (point);

		glEnd ();
		glEnable (GL_TEXTURE_2D);
		glColor4f (1,1,1,1);

		glDisable (GL_POLYGON_OFFSET_FILL);
		glEnable (GL_CULL_FACE);
#pragma message ("Baker: Depth range here ...")
		glDepthRange (Q_GLDEPTHRANGE_MIN, Q_GLDEPTHRANGE_MAX); 
	}

	GL_Bind		(char_texture);
	glDisable	(GL_CULL_FACE);
	glEnable	(GL_ALPHA_TEST);

	glDepthRange (Q_GLDEPTHRANGE_GUN, Q_GLDEPTHRANGE_MAX - 0.25); 

	for (i=0, x = 0, y =0; i<string_length; i++)
	{
		// Deal with the string

		if (caption[i] == '\n')
		{
			// Reset the row, ignore and carry on
			x = 0;
			y ++;
			continue;
		}

		if (caption[i] == '\b')
		{
			// Toggle the bronzing and carry on
			bronze = !bronze;
			continue;
		}

		// New character to print
		x++;
		{
			fRect_t		charcoords;
			float		s, t;
			int			charcode  = caption[i] | (bronze * 128);	// "ascii" ish character code, but Quake character code
			int			charrow   = charcode >> 4; // Fancy way of dividing by 16
			int			charcol   = charcode & 15; // column = mod (16-1)

			// Calculate s, t texture coords from character set image

			s = charcol * 0.0625f; // 0.0625 = 1/16th
			t = charrow * 0.0625f;

			// Calculate coords

			// ....................................... Baker: Yeah these calc look a bit "clustered" :(
			charcoords.top	  =	 captionbox.top + ((string_rows - 1 - y)/(float)string_rows)*(captionbox.bottom - captionbox.top);
			charcoords.bottom =  captionbox.top + (((string_rows - 1 - y)+1)/(float)string_rows)*(captionbox.bottom - captionbox.top);
			charcoords.left   =  captionbox.left + (x/(float)string_columns)*(captionbox.right - captionbox.left);
			charcoords.right  =  captionbox.left + ((x+1)/(float)string_columns)*(captionbox.right - captionbox.left);


			// Draw box

			glBegin (GL_QUADS);

			// top left
			glTexCoord2f (s, t + 0.0625);
			VectorMA (location, charcoords.top, up, point);
			VectorMA (point, charcoords.left, right, point);
			glVertex3fv (point);

			// top, right
			glTexCoord2f (s + 0.0625, t + 0.0625);
			VectorMA (location, charcoords.top, up, point);
			VectorMA (point, charcoords.right, right, point);
			glVertex3fv (point);

			// bottom right

			glTexCoord2f (s + 0.0625, t);
			VectorMA (location, charcoords.bottom, up, point);
			VectorMA (point, charcoords.right, right, point);
			glVertex3fv (point);

			// bottom, left
			glTexCoord2f (s, t);
			VectorMA (location, charcoords.bottom, up, point);
			VectorMA (point, charcoords.left, right, point);
			glVertex3fv (point);

			glEnd ();
		}
	}

	glEnable (GL_CULL_FACE);
	glDisable (GL_ALPHA_TEST);
	glDepthRange (Q_GLDEPTHRANGE_MIN, Q_GLDEPTHRANGE_MAX); 
}

/*
================
R_ShowBoundingBoxes -- johnfitz

draw bounding boxes -- the server-side boxes, not the renderer cullboxes
================
*/
void R_ShowBoundingBoxes (void)
{
	extern		edict_t *sv_player;
	vec3_t		mins,maxs;
	edict_t		*ed;
	int			i;

	if (!r_showbboxes.value || cl.maxclients > 1 || !r_drawentities.value || !sv.active)
		return;

	glDisable (GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	GL_PolygonOffset (OFFSET_SHOWTRIS);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_CULL_FACE);
	glColor3f (1,1,1);

	for (i=0, ed=NEXT_EDICT(sv.edicts) ; i<sv.num_edicts ; i++, ed=NEXT_EDICT(ed))
	{
		if (ed == sv_player)
			continue; //don't draw player's own bbox

//		if (r_showbboxes.value != 2)
//			if (!SV_VisibleToClient (sv_player, ed, sv.worldmodel))
//				continue; //don't draw if not in pvs

		if (ed->v.mins[0] == ed->v.maxs[0] && ed->v.mins[1] == ed->v.maxs[1] && ed->v.mins[2] == ed->v.maxs[2])
		{
			//point entity
			R_EmitWirePoint (ed->v.origin);
		}
		else
		{
			//box entity
			VectorAdd (ed->v.mins, ed->v.origin, mins);
			VectorAdd (ed->v.maxs, ed->v.origin, maxs);
			R_EmitWireBox (mins, maxs);
		}
	}

	glColor3f (1,1,1);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_CULL_FACE);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	GL_PolygonOffset (OFFSET_NONE);
	glEnable (GL_DEPTH_TEST);

	Sbar_Changed (); //so we don't get dots collecting on the statusbar
}

/*
================
R_ShowTris -- johnfitz
================
*/
void R_ShowTris (void)
{
	extern cvar_t r_particles;
	int i;

	if (r_showtris.value < 1 || r_showtris.value > 2 || cl.maxclients > 1)
		return;

	if (r_showtris.value == 1)
		glDisable (GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	GL_PolygonOffset (OFFSET_SHOWTRIS);
	glDisable (GL_TEXTURE_2D);
	glColor3f (1,1,1);
//	glEnable (GL_BLEND);
//	glBlendFunc (GL_ONE, GL_ONE);

	if (r_drawworld.value)
	{
		R_DrawTextureChains_ShowTris ();
	}

	if (r_drawentities.value)
	{
		for (i=0 ; i<cl_numvisedicts ; i++)
		{
			currententity = cl_visedicts[i];

			if (currententity == &cl_entities[cl.viewentity]) // chasecam
				currententity->angles[0] *= 0.3;

			switch (currententity->model->type)
			{
			case mod_brush:
				R_DrawBrushModel_ShowTris (currententity);
				break;
			case mod_alias:
				R_DrawAliasModel_ShowTris (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
			default:
				break;
			}
		}

		// viewmodel
		currententity = &cl.viewent;
		if (r_drawviewmodel.value
			&& !chase_active.value
			&& !envmap
			&& cl.stats[STAT_HEALTH] > 0
			&& !(cl.items & IT_INVISIBILITY)
			&& currententity->model
			&& currententity->model->type == mod_alias)
		{
			glDepthRange (Q_GLDEPTHRANGE_MIN, Q_GLDEPTHRANGE_GUN); 
			R_DrawAliasModel_ShowTris (currententity);
			glDepthRange (Q_GLDEPTHRANGE_MIN, Q_GLDEPTHRANGE_MAX); 
		}
	}

	if (r_particles.value)
	{
		R_DrawParticles_ShowTris ();
	}

//	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	glDisable (GL_BLEND);
	glColor3f (1,1,1);
	glEnable (GL_TEXTURE_2D);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	GL_PolygonOffset (OFFSET_NONE);
	if (r_showtris.value == 1)
		glEnable (GL_DEPTH_TEST);

	Sbar_Changed (); //so we don't get dots collecting on the statusbar
}

/*
================
R_DrawShadows
================
*/
void R_DrawShadows (void)
{
	int i;

	if (!r_shadows.value || !r_drawentities.value || r_drawflat_cheatsafe || r_lightmap_cheatsafe)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		if (currententity->model->type != mod_alias)
			continue;

		if (currententity == &cl.viewent)
			return;

		GL_DrawAliasShadow (currententity);
	}
}

/*
================
R_RenderScene
================
*/
void R_RenderScene (void)
{
	R_SetupScene (); //johnfitz -- this does everything that should be done once per call to RenderScene

	Fog_EnableGFog (); //johnfitz

	Sky_DrawSky (); //johnfitz

	R_DrawWorld ();

	S_ExtraUpdate (); // don't let sound get messed up if going slow

	R_DrawShadows (); //johnfitz -- render entity shadows

	R_DrawEntitiesOnList (false); //johnfitz -- false means this is the pass for nonalpha entities

	R_DrawTextureChains_Water (); //johnfitz -- drawn here since they might have transparency

	R_DrawEntitiesOnList (true); //johnfitz -- true means this is the pass for alpha entities

	R_RenderDlights (); //triangle fan dlights -- johnfitz -- moved after water

	R_DrawParticles ();

	Fog_DisableGFog (); //johnfitz

	// Baker: TexturePointer ignores depth testing so needs to be here
	// before drawing view model but after the world (otherwise the
	// world draws over it.

	TexturePointer_Think (); 

	R_DrawViewModel (); //johnfitz -- moved here from R_RenderView

	R_ShowTris (); //johnfitz

	R_ShowBoundingBoxes (); //johnfitz

#ifdef SUPPORTS_MIRRORS // Baker change
	R_Mirror (); // In PQ it after everything except polyblen
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change

}

#ifdef SUPPORTS_MIRRORS // Baker change
void R_RenderScene_Partial (void)
{
//	R_SetupScene (); //johnfitz -- this does everything that should be done once per call to RenderScene

//	Fog_EnableGFog (); //johnfitz

	Sky_DrawSky (); //johnfitz

	R_DrawWorld ();

//	S_ExtraUpdate (); // don't let sound get messed up if going slow

//	R_Clear	();

	R_DrawEntitiesOnList (false); //johnfitz -- false means this is the pass for nonalpha entities

	R_DrawShadows (); //johnfitz -- render entity shadows

	R_DrawEntitiesOnList (false); //johnfitz -- false means this is the pass for nonalpha entities


	R_DrawPlayerModel (&cl_entities[cl.viewentity]); //johnfitz -- moved here from R_RenderView

	R_DrawTextureChains_Water (); //johnfitz -- drawn here since they might have transparency

	R_DrawEntitiesOnList (true); //johnfitz -- true means this is the pass for alpha entities

	R_RenderDlights (); //triangle fan dlights -- johnfitz -- moved after water

	R_DrawParticles ();

//	Fog_DisableGFog (); //johnfitz

//	{
//		void R_Mirror (void);
//		R_Mirror (); // In PQ it after everything except polyblen
//	}
	
	

//	R_ShowTris (); //johnfitz

//	R_ShowBoundingBoxes (); //johnfitz

//#ifdef GLTEST
//	Test_Draw ();
//#endif
}


/*
=============
R_Mirror
=============
*/
void R_Mirror (void)
{
	float		d;
//	msurface_t	*s;
//	entity_t	*ent;
	qboolean	showm = false;
	vec3_t		mirrorscalef;
	vec3_t		original_vieworg;
	vec3_t		original_viewangles;

	if (!mirror_in_scene)
		return;

	VectorCopy (r_refdef.vieworg, original_vieworg);
	VectorCopy (r_refdef.viewangles, original_viewangles);

//	Con_Printf ("Rendering mirror\n");
if (showm) 	MessageBox(NULL, "Rendering mirro 1", NULL, MB_OK);
	memcpy (r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin (vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

// PROPER BEHAVIOR:  Activate the stencil buffer
//					 Draw the mirror texture with the stencil operation that says solid where we are
//					 Render the scene
//					Turn the stencil buffer off

//		glCullFace(GL_FRONT);
#if 1

// PROJECTION MATRIX
	glMatrixMode(GL_PROJECTION);
//    glLoadIdentity ();

	// Baker: 1. Clear projection matrix, set frustum, mirror mod.  Modelview
//	R_SetFrustum (r_fovx, r_fovy); //johnfitz -- use r_fov* vars
//	if (mirror_in_scene)
//	{
	
		if (mirror_plane->normal[2])
		{
			mirrorscalef[0] = 1; mirrorscalef[1] = -1; mirrorscalef[2] = 1;
			//glScalef (1, -1, 1);
		}
		else
		{
			mirrorscalef[0] = -1; mirrorscalef[1] = 1; mirrorscalef[2] = 1;
			//glScalef (-1, 1, 1);
		}
//		glCullFace(GL_BACK);
//	}
//	else
	glScalef (mirrorscalef[0], mirrorscalef[1], mirrorscalef[2]);
		glCullFace(GL_FRONT);

	// Restore modelview and projection matrix?
// PROJECTION MATRIX

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();


    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up


//	glRotatef (180,  0, 1, 0);	    // put Z going up
	glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

//	R_Clear happens after the frustum
//	R_Clear ();
//			if (mirror_plane->normal[2])
//			glScalef (1, -1, 1);
//		else
//			glScalef (-1, 1, 1);

#endif

if (showm) 	MessageBox(NULL, "refdef set", NULL, MB_OK);

#if 0
	// Forces the player to be added to the draw list.
	ent = &cl_entities[cl.viewentity];
	if (cl_visedicts[cl_numvisedicts] != ent && cl_numvisedicts < MAX_FITZQUAKE_VISEDICTS)
	{
		Con_Printf ("frame %i adding player to draw list ... ", r_framecount);
		cl_visedicts[cl_numvisedicts++] = ent;
	}
#endif

//	R_SetupFrame (); // We surely don't want this.  Animate lights and stuff.  Once per frame stuff.

//	R_SetFrustum (); // Need to make sure mirror pass does not use frustum culling

//	R_SetupGL ();
//     Matrix crap ALL OF IT.  Has mirror logic.



if (showm) 	MessageBox(NULL, "after add player to view", NULL, MB_OK);
	{
//		float gldepthmin = 0.5;
//		float gldepthmax = 1;
		glDepthRange (Q_GLDEPTHRANGE_MIN_MIRROR, Q_GLDEPTHRANGE_MAX_MIRROR); 

		glDepthFunc (GL_LEQUAL);
if (showm) MessageBox(NULL, "Ready to RenderScene", NULL, MB_OK);
		R_RenderScene_Partial ();

	// Forces the player to be added to the draw list.
#if 0
	if (cl_visedicts[cl_numvisedicts-1] == ent)
	{
		Con_Printf ("%i framecount removed player\n", r_framecount);
		cl_visedicts[--cl_numvisedicts] = NULL; // If we have added the player, remove now.
	}
#endif
//F		R_DrawWaterSurfaces ();
if (showm) MessageBox(NULL, "After Renderscene", NULL, MB_OK);
		glDepthRange (Q_GLDEPTHRANGE_MIN, Q_GLDEPTHRANGE_MAX); 
//		gldepthmin = 0;
//		gldepthmax = 0.5;
//		glDepthRange (gldepthmin, gldepthmax);
	}
	glDepthFunc (GL_LEQUAL);
#if 1
	// blend on top
	glEnable (GL_BLEND);
	//Baker 3.60 - Mirror alpha fix - from QER

	if (r_mirroralpha.value <1) // Baker 3.61 - Only run mirror alpha fix if it is being used; hopefully this may fix a possible crash issue on some video cards
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	//mirror fix
// We've got to 
	glMatrixMode(GL_PROJECTION); // Set to projection matrix
//		if (mirror_plane->normal[2])
//			glScalef (1, -1, 1);
//		else
//			glScalef (-1, 1, 1);

	glScalef (-mirrorscalef[0], -mirrorscalef[1], -mirrorscalef[2]);


	VectorCopy (original_vieworg, r_refdef.vieworg);
	VectorCopy (original_viewangles, r_refdef.viewangles);



//	glCullFace(GL_FRONT);  // Set to forward culling??  Errr.
	glCullFace(GL_BACK); // Baker because Fitz reversed this right?
	glMatrixMode(GL_MODELVIEW); // Go to model view

	glLoadMatrixf (r_base_world_matrix); // Restore the matrix

	glColor4f (1,1,1,r_mirroralpha.value);


	//	s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
//	for ( ; s ; s=s->texturechain)
//		R_RenderBrushPoly (s);
if (showm) 	MessageBox(NULL, "Before blend mirror back in", NULL, MB_OK);
	{
		void R_DrawTextureChains_Multitexture_SingleTexture (int texturenum);
		R_DrawTextureChains_Multitexture_SingleTexture (mirrortexturenum);
	}
if (showm) 	MessageBox(NULL, "After mirror chain", NULL, MB_OK);
//	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
	glDisable (GL_BLEND);
	//Baker 3.60 - Mirror alpha fix - from QER
	if (r_mirroralpha.value <1) // Baker 3.61 - Only run mirror alpha fix if it is being used; hopefully this may fix a possible crash issue on some video cards
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	//mirror fix
	glColor4f (1,1,1,1);
#endif	
	
	if (showm) 	MessageBox(NULL, "Mirror End", NULL, MB_OK);
}
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change

/*
================
R_RenderView
================
*/
void R_RenderView (void)
{
	double	time1, time2;

	if (r_norefresh.value)
		return;

	if (!cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	time1 = 0; /* avoid compiler warning */
	if (r_speeds.value)
	{
		glFinish ();
		time1 = Sys_DoubleTime ();

		//johnfitz -- rendering statistics
		rs_brushpolys = rs_aliaspolys = rs_skypolys = rs_particles = rs_fogpolys = rs_megatexels =
		rs_dynamiclightmaps = rs_aliaspasses = rs_skypasses = rs_brushpasses = 0;
	}
	else if (gl_finish.value)
		glFinish ();

#ifdef SUPPORTS_MIRRORS // Baker change
// Mirror 8: Reset the mirror for the scene
	mirror_in_scene = false;
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change

	R_SetupView (); //johnfitz -- this does everything that should be done once per frame

	//johnfitz -- stereo rendering -- full of hacky goodness
	if (r_stereo.value)
	{
		float eyesep = CLAMP(-8.0f, r_stereo.value, 8.0f);
		float fdepth = CLAMP(32.0f, r_stereodepth.value, 1024.0f);

		AngleVectors (r_refdef.viewangles, vpn, vright, vup);

		//render left eye (red)
		glColorMask(1, 0, 0, 1);
		VectorMA (r_refdef.vieworg, -0.5f * eyesep, vright, r_refdef.vieworg);
		frustum_skew = 0.5 * eyesep * NEARCLIP / fdepth;
		srand((int) (cl.ctime * 1000)); //sync random stuff between eyes

		R_RenderScene ();

		//render right eye (cyan)
		glClear (GL_DEPTH_BUFFER_BIT);
		glColorMask(0, 1, 1, 1);
		VectorMA (r_refdef.vieworg, 1.0f * eyesep, vright, r_refdef.vieworg);
		frustum_skew = -frustum_skew;
		srand((int) (cl.ctime * 1000)); //sync random stuff between eyes

		R_RenderScene ();

		//restore
		glColorMask(1, 1, 1, 1);
		VectorMA (r_refdef.vieworg, -0.5f * eyesep, vright, r_refdef.vieworg);
		frustum_skew = 0.0f;
	}
	else
	{
		R_RenderScene ();
	}
	//johnfitz

	//johnfitz -- modified r_speeds output
	time2 = Sys_DoubleTime ();
	if (r_speeds.value == 2)
		Con_Printf ("%3i ms  %4i/%4i wpoly %4i/%4i epoly %3i lmap %4i/%4i sky %1.1f mtex\n",
					(int)((time2-time1)*1000),
					rs_brushpolys,
					rs_brushpasses,
					rs_aliaspolys,
					rs_aliaspasses,
					rs_dynamiclightmaps,
					rs_skypolys,
					rs_skypasses,
					TexMgr_FrameUsage ());
	else if (r_speeds.value)
		Con_Printf ("%3i ms  %4i wpoly %4i epoly %3i lmap\n",
					(int)((time2-time1)*1000),
					rs_brushpolys,
					rs_aliaspolys,
					rs_dynamiclightmaps);
	//johnfitz
}

