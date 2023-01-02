/*  Note that this file is likely not done yet

Versus Quakespasm:   Various.  Extensive ...

1. WinQuake relics removed.

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

#ifndef __GLQUAKE_H
#define __GLQUAKE_H

#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(__APPLE__) || defined(MACOSX)

#include 	<OpenGL/gl.h>
#include 	<OpenGL/glu.h>
#include	<OpenGL/glext.h>
#include	<math.h>

#else

#include <GL/gl.h>
#include <GL/glu.h>

#endif /* __APPLE__ || MACOSX */



void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);
void GL_Set2D (void);

extern	int glx, gly, glwidth, glheight;

// r_local.h -- private refresh defs

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		480



#define BACKFACE_EPSILON	0.01


void R_TimeRefresh_f (void);
void R_ReadPointFile_f (void);
texture_t *R_TextureAnimation (texture_t *base, int frame);



typedef enum {
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2
} ptype_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s
{
// driver-usable fields
	vec3_t		org;
	float		color;
// drivers never touch the following fields
	struct particle_s	*next;
	vec3_t		vel;
	float		ramp;
	float		die;
	ptype_t		type;
} particle_t;


//====================================================

extern	vec3_t		modelorg, r_entorigin;
extern	entity_t	*currententity;
extern	int			r_visframecount;	// ??? what difs?
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern  vec3_t 		nonchase_origin;
extern  vec3_t 		nonchase_angles;


//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	qboolean	envmap;

extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadows;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_viewmodelfov;

extern float r_fovx, r_fovy;

extern cvar_t r_nolerp_list;
extern cvar_t r_noshadow_list;
extern cvar_t r_fbrighthack_list;
extern cvar_t r_nocolormap_list;

extern	cvar_t	r_texprefix_fence;
extern	cvar_t	r_texprefix_envmap;
extern	cvar_t	r_texprefix_mirror;
extern	cvar_t	r_texprefix_tele;
extern	cvar_t	r_texprefix_scrollx;
extern	cvar_t	r_texprefix_scrolly;

extern	cvar_t	r_stains_fadeamount;
extern	cvar_t	r_stains_fadetime;
extern	cvar_t	r_stains;

extern	cvar_t	r_waterripple;
extern	cvar_t	r_watercshift;
extern	cvar_t	r_lavacshift;
extern	cvar_t	r_slimecshift;

//johnfitz -- new cvars
extern cvar_t r_stereo;
extern cvar_t r_stereodepth;
extern cvar_t r_clearcolor;
extern cvar_t r_drawflat;
extern cvar_t r_flatlightstyles;
extern cvar_t gl_fullbrights;
extern cvar_t gl_farclip;
extern cvar_t gl_overbright;
extern cvar_t gl_overbright_models;
extern cvar_t r_waterquality;
extern cvar_t r_oldwater;
extern cvar_t r_waterwarp;
extern cvar_t r_oldskyleaf;
extern cvar_t r_drawworld;
extern cvar_t r_showtris;
extern cvar_t r_showbboxes;
extern cvar_t r_lerpmodels;
extern cvar_t r_lerpmove;
extern cvar_t r_nolerp_list;
//johnfitz
extern cvar_t r_noshadow_list;
extern cvar_t r_fbrighthack_list;


extern cvar_t r_vfog; //johnfitz



extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_lockfrustum;
extern	cvar_t	r_lockpvs;

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern cvar_t gl_zfix; // QuakeSpasm z-fighting fix
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_nocolors;

extern	cvar_t	gl_playermip;

extern	cvar_t	gl_subdivide_size;
extern	float	load_subdivide_size; //johnfitz -- remember what subdivide_size value was when this map was loaded

extern	float	r_world_matrix[16];

// Multitexture
extern	qboolean	mtexenabled;

#define    TEXTURE0_SGIS				0x835E
#define    TEXTURE1_SGIS				0x835F
//johnfitz -- modified multitexture support

typedef void (APIENTRY *PFNGLMULTITEXCOORD2FARBPROC) (GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *PFNGLACTIVETEXTUREARBPROC) (GLenum);
//johnfitz

//johnfitz -- anisotropic filtering
#define	GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define	GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
//johnfitz

//johnfitz -- polygon offset
#define OFFSET_BMODEL 1
#define OFFSET_NONE 0
#define OFFSET_DECAL -1
#define OFFSET_FOG -2
#define OFFSET_SHOWTRIS -3
void GL_PolygonOffset (int);
//johnfitz

//johnfitz -- GL_EXT_texture_env_combine
//the values for GL_ARB_ are identical
#define GL_COMBINE_EXT			0x8570
#define GL_COMBINE_RGB_EXT		0x8571
#define GL_COMBINE_ALPHA_EXT	0x8572
#define GL_RGB_SCALE_EXT		0x8573
#define GL_CONSTANT_EXT			0x8576
#define GL_PRIMARY_COLOR_EXT	0x8577
#define GL_PREVIOUS_EXT			0x8578
#define GL_SOURCE0_RGB_EXT		0x8580
#define GL_SOURCE1_RGB_EXT		0x8581
#define GL_SOURCE0_ALPHA_EXT	0x8588
#define GL_SOURCE1_ALPHA_EXT	0x8589
//johnfitz

// Baker:
#define GL_OPERAND0_RGB_EXT		0x8590
#define GL_OPERAND1_RGB_EXT		0x8591
#define GL_OPERAND2_RGB_EXT		0x8592


#define	GL_TEXTURE0_ARB	0x84C0
#define	GL_TEXTURE1_ARB	0x84C1
#define GL_BGRA_EXT 0x80E1
// mh - new defines for lightmapping
#define GL_BGRA 0x80E1
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367



typedef struct
{
	const char	*gl_vendor;
	const char	*gl_renderer;
	const char	*gl_version;
	const char	*gl_extensions;
	char		*gl_extensions_nice;

	qboolean	isIntelVideo;
	qboolean	gl_mtexable;
	qboolean	gl_texture_env_combine;
	qboolean	gl_texture_env_add;
	qboolean	gl_texture_non_power_of_two;
	qboolean	gl_swap_control;
	qboolean	gl_anisotropy_able;
	float		gl_max_anisotropy; //johnfitz
	int			gl_stencilbits; //johnfitz

	GLenum		TEXTURE0, TEXTURE1; //johnfitz

	PFNGLMULTITEXCOORD2FARBPROC GL_MTexCoord2fFunc;
	PFNGLACTIVETEXTUREARBPROC GL_SelectTextureFunc;

} renderer_t;

extern renderer_t renderer;



//johnfitz -- rendering statistics
extern int rs_brushpolys, rs_aliaspolys, rs_skypolys, rs_particles, rs_fogpolys;
extern int rs_dynamiclightmaps, rs_brushpasses, rs_aliaspasses, rs_skypasses;
extern float rs_megatexels;
//johnfitz

//johnfitz -- track developer statistics that vary every frame
extern cvar_t devstats;
typedef struct {
	int		packetsize;
	int		edicts;
	int		visedicts;
	int		efrags;
	int		tempents;
	int		beams;
	int		dlights;
} devstats_t;
extern devstats_t dev_stats, dev_peakstats;
//johnfitz

//ohnfitz -- reduce overflow warning spam
typedef struct {
	double	packetsize;
	double	efrags;
	double	beams;
} overflowtimes_t;
extern overflowtimes_t dev_overflows; //this stores the last time overflow messages were displayed, not the last time overflows occured
#define CONSOLE_RESPAM_TIME 3 // seconds between repeated warning messages
//johnfitz

//johnfitz -- moved here from r_brush.c
//#define MAX_LIGHTMAPS 256 //johnfitz -- was 64
//extern gltexture_t *lightmap_textures[MAX_FITZQUAKE_LIGHTMAPS]; //johnfitz -- changed to an array
//johnfitz
#define	LIGHTMAPS_BLOCK_WIDTH	128
#define	LIGHTMAPS_BLOCK_HEIGHT	128
#define BLOCKLITE_BYTES_3		3
#define LIGHTMAPS_BYTES_4 		4

#define LIGHTMAPS_BLOCK_SIZE	(LIGHTMAPS_BYTES_4 * LIGHTMAPS_BLOCK_WIDTH * LIGHTMAPS_BLOCK_HEIGHT)
#define BLOCKLITE_BLOCK_SIZE	(BLOCKLITE_BYTES_3 * LIGHTMAPS_BLOCK_WIDTH * LIGHTMAPS_BLOCK_HEIGHT)

typedef struct glRect_s {
	unsigned char l,t,w,h;
} glRect_t;

typedef struct
{
	qboolean	modified;
	glRect_t	rectchange;
	int			allocated[LIGHTMAPS_BLOCK_WIDTH]; // int			allocated[MAX_FITZQUAKE_LIGHTMAPS][LIGHTMAP_BLOCK_WIDTH];
	byte		lightmaps[LIGHTMAPS_BLOCK_SIZE];
	byte		stainmaps[BLOCKLITE_BLOCK_SIZE];
	glpoly_t	*polys;
	gltexture_t	*texture;
} lightmapinfo_t;


extern lightmapinfo_t lightmap[MAX_FITZQUAKE_LIGHTMAPS];


extern int gl_warpimagesize; //johnfitz -- for water warp

extern qboolean r_drawflat_cheatsafe, r_fullbright_cheatsafe, r_lightmap_cheatsafe, r_drawworld_cheatsafe; //johnfitz

//johnfitz -- fog functions called from outside gl_fog.c
void Fog_ParseServerMessage (void);
float *Fog_GetColor (float *startdist, float *enddist);
float Fog_GetDensity (void);
void Fog_EnableGFog (void);
void Fog_DisableGFog (void);
void Fog_StartAdditive (void);
void Fog_StopAdditive (void);
void Fog_SetupFrame (void);
void Fog_NewMap (void);
void Fog_Init (void);
//johnfitz

void R_NewGame (void);

void R_AnimateLight (void);
void R_MarkSurfaces (void);
void R_CullSurfaces (void);
qboolean R_CullBox (vec3_t emins, vec3_t emaxs);
void R_StoreEfrags (efrag_t **ppefrag);
qboolean R_CullModelForEntity (entity_t *e);
void R_RotateForEntity (vec3_t origin, vec3_t angles);
void R_MarkLights (dlight_t *light, int num, mnode_t *node);
void R_PushDlights_World (void);

void R_InitParticles (void);
void R_DrawParticles (void);
void CL_RunParticles (void);
void R_ClearParticles (void);

//void R_TranslatePlayerSkin (int playernum);
//void R_TranslateNewPlayerSkin (int playernum); //johnfitz -- this handles cases when the actual texture changes
qboolean R_SkinTextureChanged (entity_t *cur_ent);
gltexture_t *R_TranslateNewModelSkinColormap (entity_t *cur_ent);

void R_UpdateWarpTextures (void);

void R_DrawWorld (void);
void R_DrawAliasModel (entity_t *e);
void R_DrawBrushModel (entity_t *e);
void R_DrawSpriteModel (entity_t *e);

void R_DrawTextureChains_Water (void);

void R_RenderDlights (void);
void GL_BuildLightmaps_Upload_All_NewMap (void);
void R_RebuildAllLightmaps (void);

int R_LightPoint (vec3_t p);

void GL_SubdivideSurface (msurface_t *fa);
void R_RenderDynamicLightmaps (msurface_t *fa);
//void R_UploadLightmap (int lmap);  // Baker: No more

void R_DrawTextureChains_ShowTris (void);
void R_DrawBrushModel_ShowTris (entity_t *e);
void R_DrawAliasModel_ShowTris (entity_t *e);
void R_DrawParticles_ShowTris (void);

void GL_DrawAliasShadow (entity_t *e);
//void DrawGLTriangleFan (glpoly_t *p);
//void DrawGLPoly (glpoly_t *p);
void DrawGLPoly (glpoly_t *p, int renderfx);
void DrawGLTriangleFan (glpoly_t *p, int renderfx);
void DrawWaterPoly (glpoly_t *p, qboolean isteleporter);
void R_UploadLightmaps_Modified (void);

//void DrawWaterPoly (glpoly_t *p);
void GL_MakeAliasModelDisplayLists (qmodel_t *m, aliashdr_t *hdr);

void Sky_Init (void);
void Sky_DrawSky (void);
void Sky_NewMap (void);
void Sky_LoadTexture (texture_t *mt);
void Sky_LoadSkyBox (const char *name);

void TexMgr_RecalcWarpImageSize (void);

void GL_Evaluate_Renderer (void);
void GL_SetupState (void);
void GL_Info_f (void);


void Stains_WipeStains_NewMap (void);
void Stain_FrameSetup_LessenStains(void);
void Stain_AddStain(const vec3_t origin, const float tint, /*float red, float green, float blue,*/ const float in_radius);


Point3D R_EmitSurfaceHighlight (entity_t* enty, msurface_t* surf, rgba4_t color, int style);
enum {FILLED_POLYGON, OUTLINED_POLYGON};

#ifdef SUPPORTS_MIRRORS // Baker change
extern	int			mirrortexturenum;	// quake texturenum, not gltexturenum
extern	qboolean	mirror_in_scene;
extern	mplane_t	*mirror_plane;

extern	cvar_t	r_mirroralpha;

void R_Mirror (void);
#define Q_GLDEPTHRANGE_MIN 0
#define Q_GLDEPTHRANGE_MAX 0.5
#define Q_GLDEPTHRANGE_MIN_MIRROR 0.5
#define Q_GLDEPTHRANGE_MAX_MIRROR 1

#define Q_GLDEPTHRANGE_GUN 0.125 // See if "ok", made this binary friendly. (0.125 = 1 / 8)
#else // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change

#define Q_GLDEPTHRANGE_MIN 0
#define Q_GLDEPTHRANGE_MAX 1
#define Q_GLDEPTHRANGE_GUN 0.3

#endif // Baker change - #ifdef SUPPORTS_MIRRORS // Baker change

#endif	/* __GLQUAKE_H */

