
qboolean Is_Game_Level (const char* stripped_name)
{	
	int i;
	if (external_textures.value > 1)
		return true;

	for (i = 0; i < num_quake_original_levels; i ++)
		if (strcmp (levels[i].name, stripped_name)==0)
			return true;

	return false;
}

enum {warp_texture, regular_texture};

static const char* Mod_LoadExternalTexture (qmodel_t* mod, const char* texture_name, byte** data, int* fwidth, int* fheight)
{	
	char plainname[MAX_QPATH];

	if (!external_textures.value)
		return NULL;

	if (mod->bspversion == BSPVERSION_HALFLIFE)
		return NULL;

	COM_StripExtension (COM_SkipPath(mod->name), plainname, sizeof(plainname) );

	{
#define NUMDATA_SOURCES 4
		static char filename_success[MAX_OSPATH];
		qboolean original_level = Is_Game_Level (plainname);
		char* fname0 = va ("texturepointer/%s", texture_name);
		char* fname1 = va ("textures/%s/%s", plainname, texture_name);
		char* fname2 = original_level ? va ("textures/exmy/%s", texture_name) : NULL;
		char* fname3 = va ("textures/%s", texture_name);
		char* filename[NUMDATA_SOURCES] = {fname0, fname1, fname2, fname3 };
		
		int i ;

		for (i = 0; i < NUMDATA_SOURCES; i ++)
		{
			char* current_filename = filename[i];
			char *ch;

			if (!current_filename)
				continue; // exmy might be nulled out if not an original level.

			// Remove asterisks
			for (ch = current_filename; *ch; ch++)
				if (*ch == '*')
					*ch = '#';

			// Try the texture
			*data = Image_LoadImage_Limited (current_filename, fwidth, fheight, mod->loadinfo.searchpath);
			if (*data)
			{		
				q_strlcpy (filename_success, current_filename, sizeof(filename_success));
				return filename_success;
			}
		}
	}

	return NULL;
}


// Baker -- I could calc pixels data (tx+1) and wad3paldata (tx+1)+numpixels but this = better.
void Mod_Load_Brush_Model_Texture (qmodel_t* ownermod, int bsp_texnum, texture_t* tx, byte* tx_qpixels, byte* tx_wad3_palette)
{
	int			pixelcount = tx->width * tx->height;

	int			mark = Hunk_LowMark(); // Both paths do this
	int			fwidth, fheight;
	byte		*data;
	const char* external_filename =  Mod_LoadExternalTexture (ownermod, tx->name,&data, &fwidth, &fheight);

	char		warp_prefix = ownermod->bspversion == BSPVERSION ? '*' : '!';			
	int			texture_type = (tx->name[0] == warp_prefix) ? warp_texture : regular_texture;
	int			extraflags = 0;
	const char *warp_texturename; 


#pragma message ("Baker: If this isn't right, we will find out")
	if (tx->gltexture) 	 tx->gltexture = TexMgr_FreeTexture (tx->gltexture);  
	if (tx->fullbright)  tx->fullbright= TexMgr_FreeTexture (tx->fullbright);  
	if (tx->warpimage)   tx->warpimage = TexMgr_FreeTexture (tx->warpimage);  

	tx->update_warp = false;

	switch (texture_type)
	{
	case warp_texture:
		//if external texture found then
		//now load whatever we found
		if (external_filename) //load external image
		{
			tx->gltexture = TexMgr_LoadImage (
				ownermod, bsp_texnum,	// owner, bsp texture num
				external_filename, 		// description of source
				fwidth, fheight,		// width, height
				SRC_RGBA, 				// type of bytes
				data, 					// the bytes
				external_filename,		// data source file
				0, 						// offset into file
				TEXPREF_NONE			// processing flags
			);
			warp_texturename = va ("%s_warp", external_filename);
		}
		else if (ownermod->bspversion == BSPVERSION_HALFLIFE) 
		{
			//use the texture from the bsp file
			const char *in_model_texturename = va ("%s:%s", ownermod->name, tx->name);
			tx->gltexture = TexMgr_LoadImage_SetPal (
				ownermod, bsp_texnum, in_model_texturename, 
				tx->width, tx->height, 
				SRC_INDEXED_WITH_PALETTE, tx_qpixels, tx_wad3_palette, 
				"", (src_offset_t)tx_qpixels, (src_offset_t)tx_wad3_palette, 
				TEXPREF_NONE
			);					
			warp_texturename = va ("%s_warp", in_model_texturename);
		}
		else
		{
			//use the texture from the bsp file
			const char *in_model_texturename = va ("%s:%s", ownermod->name, tx->name);

			tx->gltexture = TexMgr_LoadImage (
				ownermod, bsp_texnum, 	// owner, bsp texnum
				in_model_texturename, 	// description of source
				tx->width, tx->height,	// width, height
				SRC_INDEXED, 			// type of bytes
				tx_qpixels, 			// the bytes
				"", 					// data source file
				(src_offset_t)tx_qpixels,// offset into file
				TEXPREF_NONE			// processing flags
			);
			warp_texturename = va ("%s_warp", in_model_texturename);
		}

		{
			extern byte *hunk_base;

			//now create the warpimage, using dummy data from the hunk to create the initial image
			Hunk_Alloc (gl_warpimagesize*gl_warpimagesize*4); //make sure hunk is big enough so we don't reach an illegal address
			Hunk_FreeToLowMark (mark);
			tx->warpimage = TexMgr_LoadImage (
					ownermod, -1 /* texturepointer won't use this*/, warp_texturename, 
					gl_warpimagesize, gl_warpimagesize, 
					SRC_RGBA, hunk_base, "", 
					(src_offset_t)hunk_base, 
					TEXPREF_NOPICMIP | TEXPREF_WARPIMAGE
			);
			tx->update_warp = true;
		}
		// Warp texture we already cleared the hunk
		break;

	case regular_texture:

		// Checking for fence texture
		switch (ownermod->bspversion)
		{
		case BSPVERSION_HALFLIFE:
			if (tx->name[0] == '{')
				extraflags |=  TEXPREF_ALPHA;
			break;
		default:
			if (Is_Texture_Prefix (tx->name, r_texprefix_fence.string))
				extraflags |=  TEXPREF_ALPHA;
			break;
		}	
		
		
		//now load whatever we found
		if (external_filename) //load external image
		{
			const char* glow_name = va ("%s_glow");
			const char* luma_name = va ("%s_luma");				
			
			tx->gltexture = TexMgr_LoadImage (
				ownermod, bsp_texnum,
				external_filename, fwidth, fheight,
				SRC_RGBA, 
				data, 
				external_filename, 
				0, 
				TEXPREF_MIPMAP | extraflags
			);

			//now try to load glow/luma image from the same place
			Hunk_FreeToLowMark (mark);
			external_filename = Mod_LoadExternalTexture (ownermod, glow_name,&data, &fwidth, &fheight);
			if (external_filename == NULL)
				external_filename = Mod_LoadExternalTexture (ownermod, luma_name, &data, &fwidth, &fheight);

			if (external_filename == NULL)
				break; // Bust out of our switch statement

			// Load the glow or luma we found
			tx->fullbright = TexMgr_LoadImage (
				ownermod, bsp_texnum, 
				external_filename, 
				fwidth, fheight,
				SRC_RGBA, 
				data, 
				external_filename, 
				0, 
				TEXPREF_MIPMAP | extraflags
			);
		}
		else
		{
			// No external texture so
			// use the texture from the bsp file
			const char *in_model_texturename = va ("%s:%s", ownermod->name, tx->name);
			if (ownermod->bspversion == BSPVERSION_HALFLIFE)
			{
				tx->gltexture = TexMgr_LoadImage_SetPal (
					ownermod, bsp_texnum, in_model_texturename, 
					tx->width, tx->height, 
					SRC_INDEXED_WITH_PALETTE, tx_qpixels, tx_wad3_palette, 
					"", (src_offset_t)tx_qpixels, (src_offset_t)tx_wad3_palette, 
					TEXPREF_MIPMAP | extraflags
				);
			} 
			else
			// Quake texture no external, check for fullbrights
			if (Mod_CheckFullbrights (tx_qpixels, pixelcount, extraflags))
			{
				// Quake fullbright texture
				const char* in_model_glow_texturename = va ("%s:%s_glow", ownermod->name, tx->name);
				tx->gltexture = TexMgr_LoadImage (
					ownermod, bsp_texnum, in_model_texturename, 
					tx->width, tx->height, 
					SRC_INDEXED, tx_qpixels, 
					"", (src_offset_t)tx_qpixels, 
					TEXPREF_MIPMAP | TEXPREF_NOBRIGHT | extraflags
				);
				tx->fullbright = TexMgr_LoadImage (
					ownermod, bsp_texnum, in_model_glow_texturename, 
					tx->width, tx->height, 
					SRC_INDEXED, tx_qpixels, 
					"", (src_offset_t)tx_qpixels, 
					TEXPREF_MIPMAP | TEXPREF_FULLBRIGHT | extraflags
				);
			}
			else
			{
				// Quake non-fullbright texture
				tx->gltexture = TexMgr_LoadImage (
					ownermod, bsp_texnum, in_model_texturename, 
					tx->width, tx->height, 
					SRC_INDEXED, tx_qpixels, 
					"", (src_offset_t)tx_qpixels, 
					TEXPREF_MIPMAP | extraflags
				);
			}	
		}
		// Regular texture clears to low mark
		Hunk_FreeToLowMark (mark);
		break;
	} // End of switch statement

}

void Mod_Brush_ReloadTextures (qmodel_t* mod)
{
	int i;
	
	// Baker --- (-2) last 2 textures are missing textures
	for (i = 0 ; i < mod->numtextures - 2 ; i++)
	{
		qboolean	is_wad3 = (mod->bspversion == BSPVERSION_HALFLIFE);
		texture_t	*tx = mod->textures[i];
		int			pixelcount = tx->width * tx->height;
		byte		*tx_qpixels = (byte*)(tx + 1);
		byte		*tx_wad3_palette = is_wad3 ? (byte*)tx_qpixels + pixelcount : NULL;			

		if (mod->type == mod_alias)

		// Except don't do sky
		if (mod->bspversion == BSPVERSION)

			if (!Q_strncasecmp(tx->name,"sky",3))
				continue;

		// Now what?
		Mod_Load_Brush_Model_Texture (mod, i, tx, tx_qpixels, tx_wad3_palette);
	}
}


static void Mod_LoadTextures (lump_t *l)
{
	dmiptexlump_t	*m = NULL;
	int				nummiptex=  0;
	int				i;

	//johnfitz -- need 2 dummy texture chains for missing textures
	loadmodel->numtextures = 2;

	//johnfitz -- don't return early if no textures; still need to create dummy texture
	if (l->filelen)
	{
		m = (dmiptexlump_t *)(mod_base + l->fileofs);
		nummiptex = m->nummiptex = LittleLong (m->nummiptex);
		loadmodel->numtextures += nummiptex;
	} else Con_Printf ("Mod_LoadTextures: no textures in bsp file\n");

	loadmodel->textures = (texture_t **)Hunk_AllocName (loadmodel->numtextures * sizeof(*loadmodel->textures) , loadname);

	for (i = 0 ; i < nummiptex ; i++)
	{
		miptex_t	*mt;
		texture_t	*tx;
		byte		*tx_qpixels;
		int			pixelcount;
		src_offset_t into_file_offset;
		byte		*tx_wad3_palette;

		if ( (m->dataofs[i] = LittleLong(m->dataofs[i]) ) == -1)
			continue;
		do
		{
			mt = (miptex_t *)((byte *)m + m->dataofs[i]);
	
			mt->width = LittleLong (mt->width);
			mt->height = LittleLong (mt->height);
			// Baker --- offsets 1,2,3 = on disk mipmaps = aren't used in GLQuake
			mt->offsets[0] = LittleLong (mt->offsets[0]);
	
			if ( (mt->width & 15) || (mt->height & 15) )
				Host_Error ("Mod_LoadTextures: Texture %s is not 16 aligned", mt->name);
	
			into_file_offset = (src_offset_t)(mt + 1) - (src_offset_t)mod_base;
			pixelcount = mt->width * mt->height; ///64*85;

			if (loadmodel->bspversion != BSPVERSION_HALFLIFE)
				break; // Get out of this segment
						
		} while (0);

		// Populate the texture_t
		do
		{
			// Baker: if Half-Life texture store the palette too, so tx is full of all data we need 
			// And so we do not need mt anymore.
			qboolean do_palette_too = (loadmodel->bspversion == BSPVERSION_HALFLIFE);

			int pixelcount_plus = pixelcount + (do_palette_too ? PALETTE_SIZE_768 : 0);
			tx = loadmodel->textures[i] = (texture_t *) Hunk_AllocName (sizeof(texture_t) + pixelcount_plus, loadname );
			memcpy (tx->name, mt->name, sizeof(tx->name));

			// Baker: so unnamed ones can have external textures
			if (!tx->name[0])
			{
				q_snprintf (tx->name, sizeof(tx->name), "unnamed%d", i);
				Con_Warning ("unnamed texture in %s, renaming to %s\n", loadmodel->name, tx->name);
			}
			
			// Fill in the data
			tx->width = mt->width;
			tx->height = mt->height;
			tx->offset0 = mt->offsets[0] + sizeof(texture_t) - sizeof(miptex_t);
			tx_qpixels = (byte*)(tx + 1);

			// copy the data over,  the pixels immediately follow the structures
			memcpy (tx_qpixels, mt + 1, pixelcount);

			if (do_palette_too)
			{
				// Determine WAD3 palette location then copy over
				byte* mt_wad3_palette = ((byte *) ((byte *) mt + mt->offsets[0])) + ((pixelcount * 85) >> 6) + 2;				
				tx_wad3_palette = (byte*)tx_qpixels + pixelcount;
				memcpy (tx_wad3_palette, mt_wad3_palette, PALETTE_SIZE_768);
				
				//	wad3_palette_into_file_offset = (src_offset_t)wad3_palette - (src_offset_t)mod_base;
			}

			// Baker: If we are going to store the original pixels off, let's do this here ... instead of gl_texmgr
			// Why? If you are going to store the original pixels off, do it right and ...
			// I might get bored make a live toggle between replacement and original texture someday 
			if (!strcmp(tx->name, "shot1sid") && tx->width == 32 && tx->height == 32 && CRC_Block(tx_qpixels, pixelcount) == 65393)
			{   // This texture in b_shell1.bsp has some of the first 32 pixels painted white.
				// They are invisible in software, but look really ugly in GL. So we just copy
				// 32 pixels from the bottom to make it look nice.
				memcpy (tx_qpixels, tx_qpixels + 32 * 31, 32);
			}

		} while (0);

		if (isDedicated) 
			continue;  // Baker: dedicated doesn't upload textures

		// Upload textures phase

		if (loadmodel->bspversion == BSPVERSION && !Q_strncasecmp(tx->name,"sky",3)) //sky texture //also note -- was Q_strncmp, changed to match qbsp
		{
			Sky_LoadTexture (tx);
			continue; // We're done for this texture
		}

		Mod_Load_Brush_Model_Texture (loadmodel, i, tx, tx_qpixels, tx_wad3_palette);
	}

//
// johnfitz -- last 2 slots in array should be filled with dummy textures
//
	loadmodel->textures[loadmodel->numtextures-2] = r_notexture_mip; //for lightmapped surfs
	loadmodel->textures[loadmodel->numtextures-1] = r_notexture_mip2; //for SURF_DRAWTILED surfs
//
// sequence the animations
//
	for (i=0 ; i<nummiptex ; i++)
	{
		int j, num, maxanim, altmax;
		texture_t	*ptx, *ptx2;
		texture_t	*anims[10];
		texture_t	*altanims[10];

		ptx = loadmodel->textures[i];
		if (!ptx || ptx->name[0] != '+')
			continue;

		if (ptx->anim_next)
			continue;	// already sequenced

	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		maxanim = ptx->name[1];
		altmax = 0;
		if (maxanim >= 'a' && maxanim <= 'z')
			maxanim -= 'a' - 'A';

		if (maxanim >= '0' && maxanim <= '9')
		{
			maxanim -= '0';
			altmax = 0;
			anims[maxanim] = ptx;
			maxanim++;
		}
		else if (maxanim >= 'A' && maxanim <= 'J')
		{
			altmax = maxanim - 'A';
			maxanim = 0;
			altanims[altmax] = ptx;
			altmax++;
		}
		else
		{
			Host_Error ("Mod_LoadTextures: Bad animating texture %s", ptx->name);
		}

		for (j=i+1 ; j<nummiptex ; j++)
		{
			ptx2 = loadmodel->textures[j];
			if (!ptx2 || ptx2->name[0] != '+')
				continue;
			if (strcmp (ptx2->name+2, ptx->name+2)) // Baker: +1 to skip prefix, but why +2
				continue;

			num = ptx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = ptx2;
				if (num+1 > maxanim)
					maxanim = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = ptx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
			{
				Host_Error ("Mod_LoadTextures: Bad animating texture %s", ptx->name);
			}
		}

#define	ANIM_CYCLE	2
	// link them all together
		for (j=0 ; j<maxanim ; j++)
		{
			ptx2 = anims[j];
			if (!ptx2)
				Host_Error ("Mod_LoadTextures: Missing frame %i of %s",j, ptx->name);
			ptx2->anim_total = maxanim * ANIM_CYCLE;
			ptx2->anim_min = j * ANIM_CYCLE;
			ptx2->anim_max = (j+1) * ANIM_CYCLE;
			ptx2->anim_next = anims[ (j+1)%maxanim ];
			if (altmax)
				ptx2->alternate_anims = altanims[0];
		}

		for (j=0 ; j<altmax ; j++)
		{
			ptx2 = altanims[j];
			if (!ptx2)
				Host_Error ("Mod_LoadTextures: Missing frame %i of %s",j, ptx->name);
			ptx2->anim_total = altmax * ANIM_CYCLE;
			ptx2->anim_min = j * ANIM_CYCLE;
			ptx2->anim_max = (j+1) * ANIM_CYCLE;
			ptx2->anim_next = altanims[ (j+1)%altmax ];
			if (maxanim)
				ptx2->alternate_anims = anims[0];
		}
	}
}

