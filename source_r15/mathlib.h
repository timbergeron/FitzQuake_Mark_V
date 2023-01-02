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

#ifndef __MATHLIB_H
#define __MATHLIB_H

// mathlib.h

#include <math.h>

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#define M_PI_DIV_180 (M_PI / 180.0) //johnfitz

struct mplane_s;

extern vec3_t vec3_origin;

extern	int nanmask;
#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

#define Q_rint(x) ((x) > 0 ? (int)((x) + 0.5) : (int)((x) - 0.5)) //johnfitz -- from joequake

#define DotProduct(x,y) (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c) {c[0]=a[0]-b[0];c[1]=a[1]-b[1];c[2]=a[2]-b[2];}
#define VectorAdd(a,b,c) {c[0]=a[0]+b[0];c[1]=a[1]+b[1];c[2]=a[2]+b[2];}
#define VectorCopy(a,b) {b[0]=a[0];b[1]=a[1];b[2]=a[2];}

//johnfitz -- courtesy of lordhavoc
// QuakeSpasm: To avoid strict aliasing violations, use a float/int union instead of type punning.
#define VectorNormalizeFast(_v)\
{\
	union { float f; int i; } _y, _number;\
	_number.f = DotProduct(_v, _v);\
	if (_number.f != 0.0)\
	{\
		_y.i = 0x5f3759df - (_number.i >> 1);\
		_y.f = _y.f * (1.5f - (_number.f * 0.5f * _y.f * _y.f));\
		VectorScale(_v, _y.f, _v);\
	}\
}

void TurnVector (vec3_t out, const vec3_t forward, const vec3_t side, float angle); //johnfitz
void VectorAngles (const vec3_t forward, vec3_t angles); //johnfitz

void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);

vec_t _DotProduct (vec3_t v1, vec3_t v2);
void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorCopy (vec3_t in, vec3_t out);

int VectorCompare (vec3_t v1, vec3_t v2);
vec_t VectorLength (vec3_t v);
void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);
float VectorNormalize (vec3_t v);		// returns vector length
void VectorInverse (vec3_t v);
void VectorScale (vec3_t in, vec_t scale, vec3_t out);
int Q_log2(int val);

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

void FloorDivMod (double numer, double denom, int *quotient,
		int *rem);
fixed16_t Invert24To16(fixed16_t val);
int GreatestCommonDivisor (int i1, int i2);

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);
float	anglemod(float a);


#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))

#define VectorSetColor4f(v, r, g, b, a)	 ((v)[0] = (r), (v)[1] = (g), (v)[2] = (b), (v)[3] = (a))

#define VectorSet(v, x, y, z) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z))
vec_t DistanceBetween2Points (vec3_t v1, vec3_t v2);

void VectorAverage (vec3_t v1, vec3_t v2, vec3_t out);
void VectorExtendLimits (vec3_t newvalue, vec3_t minlimit, vec3_t maxlimit);

int ParseFloats(const char *s, float *f, int *f_size);
qboolean PointInCube(vec3_t point, vec3_t cube_mins, vec3_t cube_maxs);


typedef struct _glmatrix
{
    union
	{
		// put first because gcc barfs a little
		float m16[16];
        float m4x4[4][4];

        struct
		{
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
    };
} glmatrix;


struct _glmatrix *GL_MultiplyMatrix (struct _glmatrix *out, struct _glmatrix *m1, struct _glmatrix *m2);
struct _glmatrix *GL_TranslateMatrix (struct _glmatrix *m, float x, float y, float z);
struct _glmatrix *GL_ScaleMatrix (struct _glmatrix *m, float x, float y, float z);
struct _glmatrix *GL_RotateMatrix (struct _glmatrix *m, float a, float x, float y, float z);
struct _glmatrix *GL_IdentityMatrix (struct _glmatrix *m);
struct _glmatrix *GL_LoadMatrix (struct _glmatrix *dst, struct _glmatrix *src);

#endif	/* __MATHLIB_H */

