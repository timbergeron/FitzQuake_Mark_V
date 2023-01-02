/* Versus Quakespasm: 

1. Side bobbing weapon support (cl_sidebobbing, etc.)
2. Support for weapon unaffected by FOV (r_viewmodelfov).
3. Demo rewind support (cl.ctime can go forward or reverse).
4. Content blend customization.
5. View angle smoothing (noticable when connected to a server chasecamming)
6. Changed default powerup shifts/content blends to be more reasonable
   and not so overpowering that you cannot see.

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
// view.c -- player eye positioning

#include "quakedef.h"

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

cvar_t	scr_ofsx = {"scr_ofsx","0", CVAR_NONE};
cvar_t	scr_ofsy = {"scr_ofsy","0", CVAR_NONE};
cvar_t	scr_ofsz = {"scr_ofsz","0", CVAR_NONE};

cvar_t	cl_rollspeed = {"cl_rollspeed", "200", CVAR_NONE};
cvar_t	cl_rollangle = {"cl_rollangle", "2.0", CVAR_NONE};

cvar_t	cl_bob = {"cl_bob","0.02", CVAR_NONE};
cvar_t	cl_bobcycle = {"cl_bobcycle","0.6", CVAR_NONE};
cvar_t	cl_bobup = {"cl_bobup","0.5", CVAR_NONE};

cvar_t	cl_sidebobbing = {"cl_sidebobbing","0", CVAR_NONE};
cvar_t	cl_bobside = {"cl_bobside","0.02", CVAR_NONE};
cvar_t	cl_bobsidecycle = {"cl_bobsidecycle","0.9", CVAR_NONE};
cvar_t	cl_bobsideup = {"cl_bobsideup","0.5", CVAR_NONE};

cvar_t	v_kicktime = {"v_kicktime", "0.5", CVAR_NONE};
cvar_t	v_kickroll = {"v_kickroll", "0.6", CVAR_NONE};
cvar_t	v_kickpitch = {"v_kickpitch", "0.6", CVAR_NONE};
cvar_t	v_gunkick = {"v_gunkick", "1", CVAR_NONE}; //johnfitz
cvar_t	r_viewmodelhackpos = {"r_viewmodelhackpos", "1", CVAR_NONE};
cvar_t	r_viewmodelfov = {"r_viewmodelfov", "90", CVAR_ARCHIVE};

cvar_t	v_iyaw_cycle = {"v_iyaw_cycle", "2", CVAR_NONE};
cvar_t	v_iroll_cycle = {"v_iroll_cycle", "0.5", CVAR_NONE};
cvar_t	v_ipitch_cycle = {"v_ipitch_cycle", "1", CVAR_NONE};
cvar_t	v_iyaw_level = {"v_iyaw_level", "0.3", CVAR_NONE};
cvar_t	v_iroll_level = {"v_iroll_level", "0.1", CVAR_NONE};
cvar_t	v_ipitch_level = {"v_ipitch_level", "0.3", CVAR_NONE};

cvar_t	v_idlescale = {"v_idlescale", "0", CVAR_NONE};

cvar_t	crosshair = {"crosshair", "0", CVAR_ARCHIVE};

cvar_t	gl_cshiftpercent = {"gl_cshiftpercent", "100", CVAR_NONE};

float	v_dmg_time, v_dmg_roll, v_dmg_pitch;

extern	int			in_forward, in_forward2, in_back;

vec3_t	v_punchangles[2]; //johnfitz -- copied from cl.punchangle.  0 is current, 1 is previous value. never the same unless map just loaded

/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	vec3_t	forward, right, up;
	float	sign;
	float	side;
	float	value;

	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);

	value = cl_rollangle.value;
//	if (cl.inwater)
//		value *= 6;

	if (side < cl_rollspeed.value)
		side = side * value / cl_rollspeed.value;
	else
		side = value;

	return side*sign;

}


/*
===============
V_CalcBob

===============
*/
float V_CalcBob (void)
{
	float	bob;
	float	cycle;

	cycle = cl.ctime - (int)(cl.ctime/cl_bobcycle.value)*cl_bobcycle.value;
	cycle /= cl_bobcycle.value;
	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI*(cycle-cl_bobup.value)/(1.0 - cl_bobup.value);

// bob is proportional to velocity in the xy plane
// (don't count Z, or jumping messes it up)

	bob = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]) * cl_bob.value;
//Con_Printf ("speed: %5.1f\n", VectorLength(cl.velocity));
	bob = bob*0.3 + bob*0.7*sin(cycle);
	if (bob > 4)
		bob = 4;
	else if (bob < -7)
		bob = -7;
	return bob;

}

/*
===============
V_CalcBobSide

===============
*/
float V_CalcBobSide (void)
{
	float	bobside;
	float	cycle;

	cycle = cl.ctime - (int)(cl.ctime/cl_bobsidecycle.value)*cl_bobsidecycle.value;
	cycle /= cl_bobsidecycle.value;
	if (cycle < cl_bobsideup.value)
		cycle = M_PI * cycle / cl_bobsideup.value;
	else
		cycle = M_PI + M_PI*(cycle-cl_bobsideup.value)/(1.0 - cl_bobsideup.value);

// bob is proportional to velocity in the xy plane
// (don't count Z, or jumping messes it up)

	bobside = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]) * cl_bobside.value;
//Con_Printf ("speed: %5.1f\n", VectorLength(cl.velocity));
	bobside = bobside*0.3 + bobside*0.7*sin(cycle);
	if (bobside > 4)
		bobside = 4;
	else if (bobside < -7)
		bobside = -7;

	return bobside;
}

//=============================================================================


cvar_t	v_centermove = {"v_centermove", "0.15", CVAR_NONE};
cvar_t	v_centerspeed = {"v_centerspeed","500", CVAR_NONE};


void V_StartPitchDrift (void)
{
#if 1
	if (cl.laststop == cl.time)
	{
		return;		// something else is keeping it from drifting
	}
#endif
	if (cl.nodrift || !cl.pitchvel)
	{
		cl.pitchvel = v_centerspeed.value;
		cl.nodrift = false;
		cl.driftmove = 0;
	}
}

void V_StopPitchDrift (void)
{
	cl.laststop = cl.time;
	cl.nodrift = true;
	cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when
===============
*/
void V_DriftPitch (void)
{
	float		delta, move;

	if (cl.noclip_anglehack || !cl.onground || cls.demoplayback )
	//FIXME: noclip_anglehack is set on the server, so in a nonlocal game this won't work.
	{
		cl.driftmove = 0;
		cl.pitchvel = 0;
		return;
	}

// don't count small mouse motion
	if (cl.nodrift)
	{
		if ( fabs(cl.cmd.forwardmove) < cl_forwardspeed.value)
			cl.driftmove = 0;
		else
			cl.driftmove += host_frametime;

		if ( cl.driftmove > v_centermove.value)
		{
			if (lookspring.value)
			V_StartPitchDrift ();
		}
		return;
	}

	delta = cl.idealpitch - cl.viewangles[PITCH];

	if (!delta)
	{
		cl.pitchvel = 0;
		return;
	}

	move = host_frametime * cl.pitchvel;
	cl.pitchvel += host_frametime * v_centerspeed.value;

//Con_Printf ("move: %f (%f)\n", move, host_frametime);

	if (delta > 0)
	{
		if (move > delta)
		{
			cl.pitchvel = 0;
			move = delta;
		}
		cl.viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			cl.pitchvel = 0;
			move = -delta;
		}
		cl.viewangles[PITCH] -= move;
	}
}

/*
==============================================================================

	VIEW BLENDING

==============================================================================
*/

cshift_t	cshift_empty = { {130,80,50}, 0 };

#if 0
cshift_t	cshift_water = { {130,80,50}, 128 };
#else
cshift_t	cshift_water = { {130,80,50}, 50 };
#endif

cshift_t	cshift_slime = { {0,25,5}, 150 };
cshift_t	cshift_lava = { {255,80,0}, 150 };

float		v_blend[4];		// rgba 0.0 - 1.0

//johnfitz -- deleted BuildGammaTable(), V_CheckGamma(), gammatable[], and ramps[][]



void V_Custom_CShift (cshift_t *cshift, cshift_t *default_cshift, const char *string)
{
	if (string[0] && string[1])
	{
		float values[4];
		int size = sizeof(values)/sizeof(values[0]);

		ParseFloats(string, values, &size);

		cshift->destcolor[0] = values[0];
		cshift->destcolor[1] = values[1];
		cshift->destcolor[2] = values[2];
		cshift->percent = values[3];
		return;
	}

	memcpy (cshift, default_cshift, sizeof(cshift));  // Empty string.  Default it
}

void V_WaterCshift_f (cvar_t *var)
{
	cshift_t cshift_water_default = { {130,80,50}, 50 };
	V_Custom_CShift (&cshift_water, &cshift_water_default, r_watercshift.string);
}

void V_LavaCshift_f (cvar_t *var)
{
	cshift_t	cshift_lava_default = { {255,80,0}, 150 };
	V_Custom_CShift (&cshift_lava, &cshift_lava_default, r_lavacshift.string);
}

void V_SlimeCshift_f (cvar_t *var)
{
	cshift_t	cshift_slime_default = { {0,25,5}, 150 };
	V_Custom_CShift (&cshift_slime, &cshift_slime_default, r_slimecshift.string);
}

/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (void)
{
	int		armor, blood;
	vec3_t	from;
	int		i;
	vec3_t	forward, right, up;
	entity_t	*ent;
	float	side;
	float	count;

	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	for (i=0 ; i<3 ; i++)
		from[i] = MSG_ReadCoord ();

	count = blood*0.5 + armor*0.5;
	if (count < 10)
		count = 10;

	cl.faceanimtime = cl.time + 0.2;		// but sbar face into pain frame

	cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
		cl.cshifts[CSHIFT_DAMAGE].percent = 150;

	if (armor > blood)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}
	else if (armor)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	}
	else
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

//
// calculate view angle kicks
//
	ent = &cl_entities[cl.viewentity];

	VectorSubtract (from, ent->origin, from);
	VectorNormalize (from);

	AngleVectors (ent->angles, forward, right, up);

	side = DotProduct (from, right);
	v_dmg_roll = count*side*v_kickroll.value;

	side = DotProduct (from, forward);
	v_dmg_pitch = count*side*v_kickpitch.value;

	v_dmg_time = v_kicktime.value;
}


/*
==================
V_cshift_f
==================
*/
void V_cshift_f (void)
{
	cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
	cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
	cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
	cshift_empty.percent = atoi(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f (void)
{
	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor (int contents)
{
	switch (contents)
	{
	case CONTENTS_EMPTY:
	case CONTENTS_SOLID:
	case CONTENTS_SKY: //johnfitz -- no blend in sky
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		break;
	case CONTENTS_LAVA:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
		break;
	case CONTENTS_SLIME:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
		break;
	default:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	}
}

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift (void)
{
	if (cl.items & IT_QUAD)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 20; // Baker: was 30
	}
	else if (cl.items & IT_SUIT)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20;
	}
	else if (cl.items & IT_INVISIBILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cl.cshifts[CSHIFT_POWERUP].percent = 20; // Baker: Was 100
	}
	else if (cl.items & IT_INVULNERABILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20; // Baker: was 30
	}
	else
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
}

/*
=============
V_CalcBlend
=============
*/
void V_CalcBlend (void)
{
	float	r, g, b, a, a2;
	int		j;

	r = 0;
	g = 0;
	b = 0;
	a = 0;

	for (j=0 ; j<NUM_CSHIFTS ; j++)
	{
		if (!gl_cshiftpercent.value)
			continue;

		//johnfitz -- only apply leaf contents color shifts during intermission
		if (cl.intermission && j != CSHIFT_CONTENTS)
			continue;
		//johnfitz

		a2 = ((cl.cshifts[j].percent * gl_cshiftpercent.value) / 100.0) / 255.0;
		if (!a2)
			continue;
		a = a + a2*(1-a);
		a2 = a2/a;
		r = r*(1-a2) + cl.cshifts[j].destcolor[0]*a2;
		g = g*(1-a2) + cl.cshifts[j].destcolor[1]*a2;
		b = b*(1-a2) + cl.cshifts[j].destcolor[2]*a2;
	}

	v_blend[0] = r/255.0;
	v_blend[1] = g/255.0;
	v_blend[2] = b/255.0;
	v_blend[3] = a;
	if (v_blend[3] > 1)
		v_blend[3] = 1;
	if (v_blend[3] < 0)
		v_blend[3] = 0;
}

/*
=============
V_UpdateBlend -- johnfitz -- V_UpdatePalette cleaned up and renamed
=============
*/
void V_UpdateBlend (void)
{
	int		i, j;
	qboolean	blend_changed;


	V_CalcPowerupCshift ();

	blend_changed = false;

	for (i=0 ; i<NUM_CSHIFTS ; i++)
	{
		if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent)
		{
			blend_changed = true;
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j=0 ; j<3 ; j++)
			if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j])
			{
				blend_changed = true;
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
	}

// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime*100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	if (blend_changed)
		V_CalcBlend ();
}

/*
============
V_PolyBlend -- johnfitz -- moved here from gl_rmain.c, and rewritten to use glOrtho
============
*/
void V_PolyBlend (void)
{
	if (!gl_polyblend.value || !v_blend[3])
		return;

	GL_DisableMultitexture();

	glDisable (GL_ALPHA_TEST);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_DEPTH_TEST);
	glEnable (GL_BLEND);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho (0, 1, 1, 0, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glColor4fv (v_blend);

	glBegin (GL_QUADS);
	glVertex2f (0,0);
	glVertex2f (1, 0);
	glVertex2f (1, 1);
	glVertex2f (0, 1);
	glEnd ();

	glDisable (GL_BLEND);
	glEnable (GL_DEPTH_TEST);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);
}

/*
==============================================================================

	VIEW RENDERING

==============================================================================
*/

float angledelta (float a)
{
	a = anglemod(a);
	if (a > 180)
		a -= 360;
	return a;
}

/*
==================
CalcGunAngle
==================
*/
void CalcGunAngle (void)
{
	float	yaw, pitch, move;
	static float oldyaw = 0;
	static float oldpitch = 0;

	yaw = r_refdef.viewangles[YAW];
	pitch = -r_refdef.viewangles[PITCH];

	yaw = angledelta(yaw - r_refdef.viewangles[YAW]) * 0.4;
	if (yaw > 10)
		yaw = 10;
	if (yaw < -10)
		yaw = -10;
	pitch = angledelta(-pitch - r_refdef.viewangles[PITCH]) * 0.4;
	if (pitch > 10)
		pitch = 10;
	if (pitch < -10)
		pitch = -10;
	move = host_frametime*20;
	if (yaw > oldyaw)
	{
		if (oldyaw + move < yaw)
			yaw = oldyaw + move;
	}
	else
	{
		if (oldyaw - move > yaw)
			yaw = oldyaw - move;
	}

	if (pitch > oldpitch)
	{
		if (oldpitch + move < pitch)
			pitch = oldpitch + move;
	}
	else
	{
		if (oldpitch - move > pitch)
			pitch = oldpitch - move;
	}

	oldyaw = yaw;
	oldpitch = pitch;

	cl.viewent.angles[YAW] = r_refdef.viewangles[YAW] + yaw;
	cl.viewent.angles[PITCH] = - (r_refdef.viewangles[PITCH] + pitch);

	cl.viewent.angles[ROLL] -= v_idlescale.value * sin(cl.ctime*v_iroll_cycle.value) * v_iroll_level.value;
	cl.viewent.angles[PITCH] -= v_idlescale.value * sin(cl.ctime*v_ipitch_cycle.value) * v_ipitch_level.value;
	cl.viewent.angles[YAW] -= v_idlescale.value * sin(cl.ctime*v_iyaw_cycle.value) * v_iyaw_level.value;
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets (void)
{
	entity_t	*ent;

	ent = &cl_entities[cl.viewentity];

// absolutely bound refresh reletive to entity clipping hull
// so the view can never be inside a solid wall

	r_refdef.vieworg[0] = q_max(r_refdef.vieworg[0], ent->origin[0] - 14);
	r_refdef.vieworg[0] = q_min(r_refdef.vieworg[0], ent->origin[0] + 14);
	r_refdef.vieworg[1] = q_max(r_refdef.vieworg[1], ent->origin[1] - 14);
	r_refdef.vieworg[1] = q_min(r_refdef.vieworg[1], ent->origin[1] + 14);
	r_refdef.vieworg[2] = q_max(r_refdef.vieworg[2], ent->origin[2] - 22);
	r_refdef.vieworg[2] = q_min(r_refdef.vieworg[2], ent->origin[2] + 30);
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle (void)
{
	r_refdef.viewangles[ROLL] += v_idlescale.value * sin(cl.ctime*v_iroll_cycle.value) * v_iroll_level.value;
	r_refdef.viewangles[PITCH] += v_idlescale.value * sin(cl.ctime*v_ipitch_cycle.value) * v_ipitch_level.value;
	r_refdef.viewangles[YAW] += v_idlescale.value * sin(cl.ctime*v_iyaw_cycle.value) * v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll (void)
{
	float		side;

	side = V_CalcRoll (cl_entities[cl.viewentity].angles, cl.velocity);
	r_refdef.viewangles[ROLL] += side;

	if (v_dmg_time > 0)
	{
		r_refdef.viewangles[ROLL] += v_dmg_time/v_kicktime.value*v_dmg_roll;
		r_refdef.viewangles[PITCH] += v_dmg_time/v_kicktime.value*v_dmg_pitch;
		v_dmg_time -= host_frametime;
	}

	if (cl.stats[STAT_HEALTH] <= 0)
	{
		r_refdef.viewangles[ROLL] = 80;	// dead view angle
		return;
	}

}

/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef (void)
{
	entity_t	*ent, *view;
	float		old;

// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
	view = &cl.viewent;

	VectorCopy (ent->origin, r_refdef.vieworg);
	VectorCopy (ent->angles, r_refdef.viewangles);
	view->model = NULL;

// allways idle in intermission
	old = v_idlescale.value;
	v_idlescale.value = 1;
	V_AddIdle ();
	v_idlescale.value = old;
}

/*
==================
V_CalcRefdef
==================
*/
void V_CalcRefdef (void)
{
	entity_t	*ent, *view;
	int			i;
	vec3_t		forward, right, up;
	vec3_t		angles;
	float		bob, bobside;
	static float oldz = 0;
	static vec3_t punch = {0,0,0}; //johnfitz -- v_gunkick
	float delta; //johnfitz -- v_gunkick

	V_DriftPitch ();

// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
	view = &cl.viewent;


// transform the view offset by the model's matrix to get the offset from
// model origin for the view
	ent->angles[YAW] = cl.lerpangles[YAW];	// the model should face the view dir
	ent->angles[PITCH] = -cl.lerpangles[PITCH];	// the model should face

	if (cl_sidebobbing.value)
		bobside = V_CalcBobSide ();
	bob = V_CalcBob ();

// refresh position
	VectorCopy (ent->origin, r_refdef.vieworg);
	r_refdef.vieworg[2] += cl.viewheight + bob;

// never let it sit exactly on a node line, because a water plane can
// dissapear when viewed with the eye exactly on it.
// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	r_refdef.vieworg[0] += 1.0/32;
	r_refdef.vieworg[1] += 1.0/32;
	r_refdef.vieworg[2] += 1.0/32;

	VectorCopy (cl.lerpangles, r_refdef.viewangles);

	V_CalcViewRoll ();
	V_AddIdle ();

// offsets
	angles[PITCH] = -ent->angles[PITCH]; // because entity pitches are actually backward
	angles[YAW] = ent->angles[YAW];
	angles[ROLL] = ent->angles[ROLL];

	AngleVectors (angles, forward, right, up);

	if (cl.maxclients <= 1) //johnfitz -- moved cheat-protection here from V_RenderView
		for (i=0 ; i<3 ; i++)
			r_refdef.vieworg[i] += scr_ofsx.value*forward[i] + scr_ofsy.value*right[i] + scr_ofsz.value*up[i];

	V_BoundOffsets ();

// set up gun position
	VectorCopy (cl.viewangles, view->angles);

	CalcGunAngle ();

	VectorCopy (ent->origin, view->origin);
	view->origin[2] += cl.viewheight;

	for (i=0 ; i<3 ; i++)
		if (cl_sidebobbing.value)
		{
			view->origin[i] += right[i]*bobside*0.2;
			view->origin[i] += up[i]*bob*0.2;
		}
		else
			view->origin[i] += forward[i]*bob*0.4;
	view->origin[2] += bob;

	//johnfitz -- removed all gun position fudging code (was used to keep gun from getting covered by sbar)
	// fudge position around to keep amount of weapon visible roughly equal with different viewsize
	if (r_viewmodelhackpos.value)
	{
		if (scr_viewsize.value == 110)
			view->origin[2] += 1;
		else if (scr_viewsize.value == 100)
			view->origin[2] += 2;
		else if (scr_viewsize.value == 90)
			view->origin[2] += 1;
		else if (scr_viewsize.value == 80)
			view->origin[2] += 0.5;
	}

	view->model = cl.model_precache[cl.stats[STAT_WEAPON]];
	view->frame = cl.stats[STAT_WEAPONFRAME];
	view->colormap = 0;


//johnfitz -- v_gunkick
	if (v_gunkick.value == 1) //original quake kick
		VectorAdd (r_refdef.viewangles, cl.punchangle, r_refdef.viewangles);
	if (v_gunkick.value == 2) //lerped kick
	{
		for (i=0; i<3; i++)
			if (punch[i] != v_punchangles[0][i])
			{
				//speed determined by how far we need to lerp in 1/10th of a second
				delta = (v_punchangles[0][i]-v_punchangles[1][i]) * host_frametime * 10;

				if (delta > 0)
					punch[i] = q_min(punch[i]+delta, v_punchangles[0][i]);
				else if (delta < 0)
					punch[i] = q_max(punch[i]+delta, v_punchangles[0][i]);
			}

		VectorAdd (r_refdef.viewangles, punch, r_refdef.viewangles);
	}
//johnfitz

// smooth out stair step ups
	if (!cl.noclip_anglehack && cl.onground && ent->origin[2] - oldz > 0) //johnfitz -- added exception for noclip
	//FIXME: noclip_anglehack is set on the server, so in a nonlocal game this won't work.
	{
		float steptime;

		steptime = cl.ctime - cl.oldtime;

		if (steptime < 0)
			//FIXME	I_Error ("steptime < 0");
			steptime = 0;

		oldz += steptime * 80;
		if (oldz > ent->origin[2])
			oldz = ent->origin[2];
		if (ent->origin[2] - oldz > 12)
			oldz = ent->origin[2] - 12;
		r_refdef.vieworg[2] += oldz - ent->origin[2];
		view->origin[2] += oldz - ent->origin[2];
	}
	else
		oldz = ent->origin[2];

	if (chase_active.value)
	{
		VectorCopy (r_refdef.vieworg,  nonchase_origin);
		VectorCopy (r_refdef.viewangles,  nonchase_angles);
		Chase_UpdateForDrawing (); //johnfitz
	}
}

// Attempt to get chase camera to use visibility for
// its actual position, not the player's position.
// This fails for a number of different reasons.
// The chase camera might be in a different visibility leaf than the player
// and the server only sends entities based on actual player position
vec3_t nonchase_origin;
vec3_t nonchase_angles;

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
extern vrect_t	scr_vrect;

void V_RenderView (void)
{
	if (con_forcedup)
		return;

	if (cl.intermission)
		V_CalcIntermissionRefdef ();
	else // Baker: commented out conditions
		V_CalcRefdef ();

	//johnfitz -- removed lcd code

	R_RenderView ();

	Entity_Inspector_Think ();

	V_PolyBlend (); //johnfitz -- moved here from R_Renderview ();
}

/*
==============================================================================

	INIT

==============================================================================
*/

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("v_cshift", V_cshift_f);
	Cmd_AddCommand ("bf", V_BonusFlash_f);
	Cmd_AddCommand ("centerview", V_StartPitchDrift);

	Cvar_RegisterVariable (&v_centermove);
	Cvar_RegisterVariable (&v_centerspeed);

	Cvar_RegisterVariable (&v_iyaw_cycle);
	Cvar_RegisterVariable (&v_iroll_cycle);
	Cvar_RegisterVariable (&v_ipitch_cycle);
	Cvar_RegisterVariable (&v_iyaw_level);
	Cvar_RegisterVariable (&v_iroll_level);
	Cvar_RegisterVariable (&v_ipitch_level);

	Cvar_RegisterVariable (&v_idlescale);
	Cvar_RegisterVariable (&crosshair);
	Cvar_RegisterVariable (&gl_cshiftpercent);

	Cvar_RegisterVariable (&scr_ofsx);
	Cvar_RegisterVariable (&scr_ofsy);
	Cvar_RegisterVariable (&scr_ofsz);
	Cvar_RegisterVariable (&cl_rollspeed);
	Cvar_RegisterVariable (&cl_rollangle);
	Cvar_RegisterVariable (&cl_bob);
	Cvar_RegisterVariable (&cl_bobcycle);
	Cvar_RegisterVariable (&cl_bobup);

	Cvar_RegisterVariable (&cl_sidebobbing);
	Cvar_RegisterVariable (&cl_bobside);
	Cvar_RegisterVariable (&cl_bobsidecycle);
	Cvar_RegisterVariable (&cl_bobsideup);

	Cvar_RegisterVariable (&v_kicktime);
	Cvar_RegisterVariable (&v_kickroll);
	Cvar_RegisterVariable (&v_kickpitch);
	Cvar_RegisterVariable (&v_gunkick); //johnfitz
	Cvar_RegisterVariable (&r_viewmodelhackpos); //johnfitz
	Cvar_RegisterVariable (&r_viewmodelfov); //johnfitz

}


