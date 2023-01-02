/* Versus Quakespasm:

1. envmap works
2. precalculated flash bubble
3. colormapped dead bodies and other entities

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
// r_misc.c

#include "quakedef.h"

/*
====================
GL_Overbright_f -- johnfitz
====================
*/
static void GL_Overbright_f (cvar_t *var)
{
	R_RebuildAllLightmaps ();
}

/*
====================
GL_Fullbrights_f -- johnfitz
====================
*/
static void GL_Fullbrights_f (cvar_t *var)
{
	TexMgr_ReloadNobrightImages ();
}

/*
====================
R_SetClearColor_f -- johnfitz
====================
*/
static void R_SetClearColor_f (cvar_t *var)
{
	byte	*rgb;
	int		s;

	s = (int)r_clearcolor.value & 0xFF;
	rgb = (byte*)(d_8to24table + s);
	glClearColor (rgb[0]/255.0,rgb[1]/255.0,rgb[2]/255.0,0);
}

/*
====================
R_Novis_f -- johnfitz
====================
*/
static void R_VisChanged (cvar_t *var)
{
	extern int vis_changed;
	vis_changed = 1;
}

/*
=======================
R_Modelflags_Refresh_f -- johnfitz -- called when r_nolerp_list or other list cvar changes
=======================
*/
static void R_Modelflags_Refresh_f (cvar_t *var)
{
	int i;
	for (i=0; i < MAX_FITZQUAKE_MODELS; i++)
		Mod_SetExtraFlags (cl.model_precache[i]);
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
static void R_Envmap_f (void)
{
	byte	buffer[256*256*4];
//	char	name[1024];

	envmap = true;

	glx = gly = r_refdef.vrect.x = r_refdef.vrect.y = 0;
	glwidth = glheight = r_refdef.vrect.width = r_refdef.vrect.height = 256;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;

	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	glFinish ();
	Image_WriteTGA ("env0.tga", buffer, 256, 256, 32, false);

	r_refdef.viewangles[1] = 90;
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	glFinish ();
	Image_WriteTGA ("env1.tga", buffer, 256, 256, 32, false);

	r_refdef.viewangles[1] = 180;
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	glFinish ();
	Image_WriteTGA ("env2.tga", buffer, 256, 256, 32, false);

	r_refdef.viewangles[1] = 270;
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	glFinish ();
	Image_WriteTGA ("env3.tga", buffer, 256, 256, 32, false);

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	glFinish ();
	Image_WriteTGA ("env4.tga", buffer, 256, 256, 32, false);

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	glFinish ();
	Image_WriteTGA ("env5.tga", buffer, 256, 256, 32, false);

	envmap = false;

	Recent_File_Set_RelativePath ( "env0.tga");
	Con_Printf ("Envmaps env.tga files created\n");
	vid.recalc_refdef = true; // Recalc the refdef next frame

}

/*
===============
R_Init
===============
*/
void R_Init (void)
{
	extern cvar_t gl_finish;

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand ("envmap", R_Envmap_f);
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);

	Cvar_RegisterVariable (&r_norefresh);
	Cvar_RegisterVariable (&r_lightmap);
	Cvar_RegisterVariable (&r_fullbright);
	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_shadows);
	Cvar_RegisterVariable (&r_wateralpha);

#ifdef SUPPORTS_MIRRORS // Baker change
	Cvar_RegisterVariable (&r_mirroralpha);
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change
	Cvar_RegisterVariable (&r_waterripple);

	Cvar_RegisterVariable (&r_stains); // "Stain surfaces."
	Cvar_RegisterVariable (&r_stains_fadetime); // "Stain fade control in seconds between fade calculations."
	Cvar_RegisterVariable (&r_stains_fadeamount); // "Amount of alpha to reduce stains per fade calculation."

	Cvar_RegisterVariableWithCallback (&r_watercshift, V_WaterCshift_f);
	Cvar_RegisterVariableWithCallback (&r_lavacshift, V_LavaCshift_f);
	Cvar_RegisterVariableWithCallback (&r_slimecshift, V_SlimeCshift_f);

	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariableWithCallback (&r_novis, R_VisChanged);
	Cvar_RegisterVariable (&r_lockfrustum);
	Cvar_RegisterVariable (&r_lockpvs);
	
	Cvar_RegisterVariable (&r_speeds);

	Cvar_RegisterVariable (&gl_finish);
	Cvar_RegisterVariable (&gl_clear);
	Cvar_RegisterVariable (&gl_cull);
	Cvar_RegisterVariable (&gl_smoothmodels);
	Cvar_RegisterVariable (&gl_affinemodels);
	Cvar_RegisterVariable (&gl_polyblend);
	Cvar_RegisterVariable (&gl_flashblend);
	Cvar_RegisterVariable (&gl_playermip);
	Cvar_RegisterVariable (&gl_nocolors);

	//johnfitz -- new cvars
	Cvar_RegisterVariable (&r_stereo);
	Cvar_RegisterVariable (&r_stereodepth);
	Cvar_RegisterVariableWithCallback (&r_clearcolor, R_SetClearColor_f);
	Cvar_RegisterVariable (&r_waterquality);
	Cvar_RegisterVariable (&r_oldwater);
	Cvar_RegisterVariable (&r_waterwarp);
	Cvar_RegisterVariable (&r_drawflat);
	Cvar_RegisterVariable (&r_flatlightstyles);
	Cvar_RegisterVariableWithCallback (&r_oldskyleaf, R_VisChanged);
	Cvar_RegisterVariable (&r_drawworld);
	Cvar_RegisterVariable (&r_showtris);
	Cvar_RegisterVariable (&r_showbboxes);
	Cvar_RegisterVariable (&gl_farclip);
	Cvar_RegisterVariableWithCallback (&gl_fullbrights, GL_Fullbrights_f);
	Cvar_RegisterVariableWithCallback (&gl_overbright, GL_Overbright_f);
	Cvar_RegisterVariable (&gl_overbright_models);
	Cvar_RegisterVariable (&r_lerpmodels);
	Cvar_RegisterVariable (&r_lerpmove);
	
	Cvar_RegisterVariableWithCallback (&r_nolerp_list, R_Modelflags_Refresh_f);
	
	//johnfitz
	Cvar_RegisterVariableWithCallback (&r_noshadow_list, R_Modelflags_Refresh_f);	
	Cvar_RegisterVariableWithCallback (&r_fbrighthack_list, R_Modelflags_Refresh_f);	

	
	Cvar_RegisterVariable (&gl_zfix); // QuakeSpasm z-fighting fix

	Cvar_RegisterVariableWithCallback (&r_texprefix_fence, Host_Changelevel_Required_Msg);
	Cvar_RegisterVariableWithCallback (&r_texprefix_envmap, Host_Changelevel_Required_Msg);
	Cvar_RegisterVariableWithCallback (&r_texprefix_mirror, Host_Changelevel_Required_Msg);
	Cvar_RegisterVariableWithCallback (&r_texprefix_tele, Host_Changelevel_Required_Msg);
	Cvar_RegisterVariableWithCallback (&r_texprefix_scrollx, Host_Changelevel_Required_Msg);
	Cvar_RegisterVariableWithCallback (&r_texprefix_scrolly, Host_Changelevel_Required_Msg);

	Cvar_RegisterVariable (&gl_subdivide_size); //johnfitz -- moved here from gl_model.c

	R_InitParticles ();
	R_SetClearColor_f (&r_clearcolor); //johnfitz

	R_Init_FlashBlend_Bubble ();

	Entity_Inspector_Init ();
	TexturePointer_Init ();

	Sky_Init (); //johnfitz
	Fog_Init (); //johnfitz
}

/*
===============
R_NewGame -- johnfitz -- handle a game switch
===============
*/
void R_NewGame (void)
{
	int i;

	//clear playertexture pointers (the textures themselves were freed by texmgr_newgame)
	for (i = 0; i < MAX_COLORMAP_SKINS; i ++)
		playertextures[i] = NULL;
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles ();

	Stains_WipeStains_NewMap ();
//	TexturePointer_ClearCaches_NewMap ();
	GL_BuildLightmaps_Upload_All_NewMap ();

#ifdef SUPPORTS_MIRRORS // Baker change
// mirror:  1. Identify the mirror textures in the map (DONE)
	mirrortexturenum = -1;
	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
//		if (cl.worldmodel->textures[i]->name && !strncmp(cl.worldmodel->textures[i]->name,"window02_1",10) )
		if (cl.worldmodel->textures[i]->name)
		{
			qboolean Is_Texture_Prefix (const char *texturename, const char *prefixstring);
			if (Is_Texture_Prefix (cl.worldmodel->textures[i]->name, r_texprefix_mirror.string) )
			{
				mirrortexturenum = i;
				Con_Printf ("Located a mirror texture at %i\n", i);
			}
		}
	}
// mirror end 1
#endif // Baker change + #ifdef SUPPORTS_MIRRORS // Baker change

	r_framecount = 0; //johnfitz -- paranoid?
	r_visframecount = 0; //johnfitz -- paranoid?

	Sky_NewMap (); //johnfitz -- skybox in worldspawn
	Fog_NewMap (); //johnfitz -- global fog in worldspawn

	TexturePointer_Reset ();

	load_subdivide_size = gl_subdivide_size.value; //johnfitz -- is this the right place to set this?
}

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;

	if (cls.state != ca_connected)
	{
		Con_Printf("Not connected to a server\n");
		return;
	}

	start = Sys_DoubleTime ();
	for (i=0 ; i<128 ; i++)
	{
		GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
		r_refdef.viewangles[1] = i/128.0*360.0;
		R_RenderView ();
		GL_EndRendering ();
	}

	glFinish ();
	stop = Sys_DoubleTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);
}

#if 0
void D_FlushCaches (void)
{
}

#endif

/*
===============
R_TranslateNewModelSkinColormap

Looks through our table and returns an existing model/skin/pants/shirt combination already uploaded.
If does not exist.  Uploads it.

Since this re-uses colormapped skins (2 red players = same texture) this will upload fewer skins
than the GLQuake way, but will colormap everything.

r_nocolormap_list provides a means to exclude colormapping trivial things like gibs.  Although it
supports 1024 combinations, it will rarely use more than 10 to 30 in practice.  All skins are
deleted on a new map.
===============
*/

cvar_t	r_nocolormap_list = {"r_nocolormap_list", ""}; // Under consideration: progs/eyes.mdl,progs/h_player.mdl,progs/gib1.mdl,progs/gib2.mdl,progs/gib3.mdl

// This is for an entity WITH an existing skin texture
// So we know it is an alias model (or at least was until now!)
qboolean R_SkinTextureChanged (entity_t *cur_ent)
{
	gltexture_t *skintexture	= cur_ent->coloredskin;
	int entnum = cur_ent - cl_entities;

	if (skintexture->owner != cur_ent->model)
	{
//		Con_Printf ("ent %i Model changed\n", entnum);
		return true;	// Model changed
	}

	do
	{
		int playerslot				= cur_ent->colormap - 1;
		int shirt_color				= (cl.scores[playerslot].colors & 0xf0) >> 4;
		int pants_color				= cl.scores[playerslot].colors & 15;

		if (skintexture->pants != pants_color)
		{
//			Con_Printf ("ent %i: Pants changed\n", entnum);		// Pants changed
			return true;
		}

		if (skintexture->shirt != shirt_color)
		{
//			Con_Printf ("ent %i: Shirt changed\n", entnum);		// Shirt changed
			return true;
		}

		if (skintexture->skinnum != cur_ent->skinnum)
		{
//			Con_Printf ("ent %i: Player skin changed\n", entnum);		// Skin changed
			return true; // Skin changed
		}

		// NOTE: Baker --> invalid skin situation can persistently trigger "skin changed"
		return false;

	} while (0);

}


gltexture_t *R_TranslateNewModelSkinColormap (entity_t *cur_ent)
{
	int entity_number = cur_ent - cl_entities; // If player, this will be 1-16
	int shirt_color, pants_color, skinnum, matchingslot;
	aliashdr_t	*paliashdr;

	do	// REJECTIONS PHASE
	{
		// No model or it isn't an alias model
		if (!cur_ent->model || cur_ent->model->type != mod_alias)
			return NULL;

		// No color map or it is invalid
		if (cur_ent->colormap <= 0 || cur_ent->colormap > cl.maxclients)
			return NULL;

		// Certain models just aren't worth trying to colormap
		if (cur_ent->model->flags & MOD_NOCOLORMAP)
			return NULL;

		//TODO: move these tests to the place where skinnum gets received from the server
		paliashdr = (aliashdr_t *)Mod_Extradata (cur_ent->model);
		skinnum = cur_ent->skinnum;

		if (skinnum < 0 || skinnum >= paliashdr->numskins)
		{
			Con_DPrintf("(%d): Invalid player skin #%d\n", entity_number, skinnum);
			skinnum = 0;
		}

	} while (0);


	do // SEE IF WE HAVE SKIN + MODEL + COLOR ALREADY PHASE
	{
		int playerslot = cur_ent->colormap - 1;

		shirt_color = (cl.scores[playerslot].colors & 0xf0) >> 4;
		pants_color = cl.scores[playerslot].colors & 15;

		for (matchingslot = 0; matchingslot < MAX_COLORMAP_SKINS; matchingslot ++)
		{
			gltexture_t *curtex = playertextures[matchingslot];

			if (playertextures[matchingslot] == NULL)
				break; // Not found, but use this slot

			if (curtex->shirt != shirt_color) continue;
			if (curtex->pants != pants_color) continue;
			if (curtex->skinnum != skinnum) continue;
			if (curtex->owner != cur_ent->model) continue;

			// Found an existing translation for this
			return curtex;

		}

		if (matchingslot == MAX_COLORMAP_SKINS)
		{
			Host_Error ("Color Slots Full");
			return NULL;
		}

		// If we are here matchingslot is our new texture slot

	} while (0);

	do // UPLOAD THE NEW SKIN + MODEL PHASE (MAYBE COLOR)
	{
		aliashdr_t	*paliashdr = (aliashdr_t *)Mod_Extradata (cur_ent->model);
		byte		*pixels = (byte *)paliashdr + paliashdr->texels[skinnum]; // This is not a persistent place!
		char		name[64];

		sprintf(name, "player_%i", entity_number);

//		Con_Printf ("New upload\n");

		//upload new image
		playertextures[matchingslot] = TexMgr_LoadImage (cur_ent->model, -1 /*not bsp texture*/, name, paliashdr->skinwidth, paliashdr->skinheight,
		SRC_INDEXED, pixels, paliashdr->gltextures[skinnum][0]->source_file, paliashdr->gltextures[skinnum][0]->source_offset, TEXPREF_PAD | TEXPREF_OVERWRITE);

		if (playertextures[matchingslot])
		{
			playertextures[matchingslot]->skinnum = skinnum;
			TexMgr_ReloadImage (playertextures[matchingslot], shirt_color, pants_color);
		}

	} while (0);

	return playertextures[matchingslot];
}

