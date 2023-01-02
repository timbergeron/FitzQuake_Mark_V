/* Versus Quakespasm: 

1. Support for extra chase cam modes (chasemode -- experimental)

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
// chase.c -- chase camera code

#include "quakedef.h"

cvar_t	chase_back = {"chase_back", "100",CVAR_NONE};
cvar_t	chase_up = {"chase_up", "16",CVAR_NONE};
cvar_t	chase_right = {"chase_right", "0",CVAR_NONE};
cvar_t	chase_active = {"chase_active", "0",CVAR_NONE};

typedef struct
{
	char		*description;
	float 		back;
	float 		up;
	float 		right;
	float 		pitch;
	float 		yaw;
	float 		roll;
	qboolean	step_to_position;
	qboolean	clip_against_objects;		// Camera doesn't clip against map and such
	qboolean	locate_target_lookat;	// Targets a lookat spot
	qboolean	accepts_pitch;	// Use provided pitch
	qboolean	accepts_yaw;	// Use provided yaw
	qboolean	accepts_roll;	// Use provided roll
} chase_cam_t;

chase_cam_t chase_classic_plus 	 = {"Classic",		  100.0,   16.0,    0.0,  0.0,  0.0, 0.0, 1, 1, 1, 1, 1, 1};
chase_cam_t chase_classic_noclip = {"Classic noclip", 100.0,   16.0,    0.0,  0.0,  0.0, 0.0, 1, 0, 1, 1, 1, 1};
chase_cam_t chase_overhead 		 = {"Overhead",		    0.0,  200.0,    0.0, 90.0,  0.0, 0.0, 0, 0, 0, 0, 0, 0};
chase_cam_t chase_overhead_yaw 	 = {"Overhead yaw",   125.0,  200.0,    0.0, 45.0,  0.0, 0.0, 1, 0, 0, 0, 1, 0};
chase_cam_t chase_overhead_yaw_c = {"Overhead yaw",   125.0,  200.0,    0.0, 45.0,  0.0, 0.0, 1, 1, 0, 0, 1, 0};
chase_cam_t chase_overhead_clip  = {"Overhead clip",    0.0,  200.0,    0.0, 90.0,  0.0, 0.0, 0, 1, 0, 0, 0, 0};
chase_cam_t chase_rpg 			 = {"RPG",		     -160.0,  200.0, -160.0, 45.0, 45.0, 0.0, 0, 0, 0, 0, 0, 0};
chase_cam_t chase_side 			 = {"Side",          -200.0,   16.0,    0.0,  0.0,  0.0, 0.0, 0, 0, 0, 0, 0, 0};

int chase_mode;
chase_cam_t *chase_type = NULL;

void Chase_Mode_f (void) // overhead, rpg, side, classic, classic
{
//	int 	n;

	if (Cmd_Argc() >= 2)
	{
		int newmode = Q_atoi(Cmd_Argv(1));
		chase_mode = newmode;
	}
	else
		chase_mode = chase_mode + 1;

	if (chase_mode > 8 || chase_mode < 0)
		chase_mode = 0;

	if (chase_mode == 0)
		chase_type = NULL;
	else if (chase_mode == 1)
		chase_type = &chase_classic_plus;
	else if (chase_mode == 2)
		chase_type = &chase_classic_noclip;
	else if (chase_mode == 3)
		chase_type = &chase_overhead;
	else if (chase_mode == 4)
		chase_type = &chase_overhead_yaw;
	else if (chase_mode == 5)
		chase_type = &chase_overhead_yaw_c;
	else if (chase_mode == 6)
		chase_type = &chase_overhead_clip;
	else if (chase_mode == 7)
		chase_type = &chase_rpg;
	else if (chase_mode == 8)
		chase_type = &chase_side;

	if (chase_type)
		Con_DPrintf ("chase_mode %s\n", chase_type->description);
	else
		Con_DPrintf ("chase_mode toggle\n");
}


/*
==============
Chase_Init
==============
*/
void Chase_Init (void)
{
	Cvar_RegisterVariable (&chase_back);
	Cvar_RegisterVariable (&chase_up);
	Cvar_RegisterVariable (&chase_right);
	Cvar_RegisterVariable (&chase_active);

	Cmd_AddCommand ("chase_mode", Chase_Mode_f);
}

/*
==============
TraceLine

TODO: impact on bmodels, monsters
==============
*/
qboolean TraceLine (vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t	trace;

	memset (&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);

	VectorCopy (trace.endpos, impact);

	if (impact[0] || impact[1] || impact[2])
		return false; // Traceline hit something

	return true; // Traceline hit unimpaired
}

/*
==============
Chase_UpdateForClient -- johnfitz -- orient client based on camera. called after input
==============
*/
void Chase_UpdateForClient (void)
{
	//place camera

	//assign client angles to camera

	//see where camera points

	//adjust client angles to point at the same place
}

/*
==============
Chase_UpdateForDrawing -- johnfitz -- orient camera based on client. called before drawing

TODO: stay at least 8 units away from all walls in this leaf
==============
*/
void Chase_UpdateForDrawing (void)
{
	int		i;
	vec3_t	forward, up, right;
	vec3_t	ideal, crosshair, temp;

	AngleVectors (cl.viewangles, forward, right, up);

	if (chase_mode && !chase_type->step_to_position)
	{
		ideal[0] = cl.viewent.origin[0] + chase_type->back;
		ideal[1] = cl.viewent.origin[1] + chase_type->right;
		ideal[2] = cl.viewent.origin[2] + chase_type->up;
	}
	else if (chase_mode && chase_type->step_to_position)
	{
		for (i=0 ; i<3 ; i++)
			ideal[i] = cl.viewent.origin[i] - forward[i]*chase_type->back + chase_type->right;  //+ up[i]*chase_up.value;
		ideal[2] = cl.viewent.origin[2] + chase_type->up;
	}
	else
	{
		// calc ideal camera location before checking for walls
		for (i=0 ; i<3 ; i++)
		ideal[i] = cl.viewent.origin[i]
		- forward[i]*chase_back.value
		+ right[i]*chase_right.value;
		//+ up[i]*chase_up.value;
		ideal[2] = cl.viewent.origin[2] + chase_up.value;
	}

	if (chase_mode && !chase_type->clip_against_objects)
	{
		// camera doesn't clip.  It is where it is.
	}
	else
	{
		// make sure camera is not in or behind a wall
		TraceLine(r_refdef.vieworg, ideal, temp);
		if (VectorLength(temp) != 0)
			VectorCopy(temp, ideal);
	}
	// place camera
	VectorCopy (ideal, r_refdef.vieworg);

	if (chase_mode && !chase_type->locate_target_lookat)
	{
		// Don't need to find where player is looking
	}
	else
	{
		// find the spot the player is looking at
		VectorMA (cl.viewent.origin, 4096, forward, temp);
		TraceLine (cl.viewent.origin, temp, crosshair);
	}


	if (chase_mode && !chase_type->locate_target_lookat)
	{
		// Don't need to calculate the angles
		r_refdef.viewangles[PITCH] = chase_type->pitch;
		if (chase_type->accepts_yaw)
			r_refdef.viewangles[YAW] = cl.viewangles[YAW];
		else
		{
			r_refdef.viewangles[YAW] = chase_type->yaw;
		}
		r_refdef.viewangles[ROLL] = chase_type->roll;
	}
	else
	{
		// calculate camera angles to look at the same spot
		VectorSubtract (crosshair, r_refdef.vieworg, temp);
		VectorAngles (temp, r_refdef.viewangles);
		if (r_refdef.viewangles[PITCH] == 90 || r_refdef.viewangles[PITCH] == -90)
			r_refdef.viewangles[YAW] = cl.viewangles[YAW];
	}
}
