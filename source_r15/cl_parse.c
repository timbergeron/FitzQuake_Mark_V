/* Versus Quakespasm:

1. Removed winquake relics
2. Smooth rotation protocol 668, demo rewind (out of intermission, finale, etc.)
3. ProQuake smooth view angles transitions
4. Ping scoreboard support
5. Colormapping all entities
6. .loc file support (part of talk macro expansion)

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
// cl_parse.c  -- parse a message received from the server

#include "quakedef.h"

const char *svc_strings[] =
{
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",		// [long] server version
	"svc_setview",		// [short] entity number
	"svc_sound",			// <see code>
	"svc_time",			// [float] server time
	"svc_print",			// [string] null terminated string
	"svc_stufftext",		// [string] stuffed into client's console buffer
						// the string should be \n terminated
	"svc_setangle",		// [vec3] set the view angle to this absolute value

	"svc_serverinfo",		// [long] version
						// [string] signon string
						// [string]..[0]model cache [string]...[0]sounds cache
						// [string]..[0]item cache
	"svc_lightstyle",		// [byte] [string]
	"svc_updatename",		// [byte] [string]
	"svc_updatefrags",	// [byte] [short]
	"svc_clientdata",		// <shortbits + data>
	"svc_stopsound",		// <see code>
	"svc_updatecolors",	// [byte] [byte]
	"svc_particle",		// [vec3] <variable>
	"svc_damage",			// [byte] impact [byte] blood [vec3] from

	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",

	"svc_temp_entity",		// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",			// [string] music [string] text
	"svc_cdtrack",			// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_cutscene",
//johnfitz -- new server messages
	"",	// 35
	"",	// 36
	"svc_skybox", // 37					// [string] skyname
	"", // 38
	"", // 39
	"svc_bf", // 40						// no data
	"svc_fog", // 41					// [byte] density [byte] red [byte] green [byte] blue [float] time
	"svc_spawnbaseline2", //42			// support for large modelindex, large framenum, alpha, using flags
	"svc_spawnstatic2", // 43			// support for large modelindex, large framenum, alpha, using flags
	"svc_spawnstaticsound2", //	44		// [coord3] [short] samp [byte] vol [byte] aten
	"", // 44
	"", // 45
	"", // 46
	"", // 47
	"", // 48
	"", // 49
//johnfitz
};

qboolean warn_about_nehahra_protocol; //johnfitz

extern vec3_t	v_punchangles[2]; //johnfitz

//=============================================================================

/*
===============
CL_EntityNum

This error checks and tracks the total number of entities
===============
*/
entity_t	*CL_EntityNum (int num)
{
	//johnfitz -- check minimum number too
	if (num < 0)
		Host_Error ("CL_EntityNum: %i is an invalid number",num);
	//john

	if (num >= cl.num_entities)
	{
		if (num >= cl_max_edicts) //johnfitz -- no more MAX_EDICTS
			Host_Error ("CL_EntityNum: %i is an invalid number",num);
		while (cl.num_entities<=num)
		{
			cl_entities[cl.num_entities].colormap = 0; // Baker: No color map
			cl_entities[cl.num_entities].lerpflags |= LERP_RESETMOVE|LERP_RESETANIM; //johnfitz
			cl.num_entities++;
		}
	}

	return &cl_entities[num];
}


/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num;
    int 	volume;
    int 	field_mask;
    float 	attenuation;
 	int		i;

    field_mask = MSG_ReadByte();

    if (field_mask & SND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

    if (field_mask & SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (field_mask & SND_LARGEENTITY)
	{
		ent = (unsigned short) MSG_ReadShort ();
		channel = MSG_ReadByte ();
	}
	else
	{
		channel = (unsigned short) MSG_ReadShort ();
		ent = channel >> 3;
		channel &= 7;
	}

	if (field_mask & SND_LARGESOUND)
		sound_num = (unsigned short) MSG_ReadShort ();
	else
		sound_num = MSG_ReadByte ();
	//johnfitz

	//johnfitz -- check soundnum
	if (sound_num >= MAX_FITZQUAKE_SOUNDS)
		Host_Error ("CL_ParseStartSoundPacket: %i > MAX_SOUNDS", sound_num);
	//johnfitz

	if (ent > cl_max_edicts) //johnfitz -- no more MAX_EDICTS
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);

	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord ();

    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/
static byte	net_olddata[NET_FITZQUAKE_MAXMESSAGE];
void CL_KeepaliveMessage (void)
{
	float	time;
	static float lastmsg;
	int		ret;
	sizebuf_t	old;
	byte	*olddata;

	if (sv.active)
		return;		// no need if server is local
	if (cls.demoplayback)
		return;

// read messages from server, should just be nops
	olddata = net_olddata;
	old = net_message;
	memcpy (olddata, net_message.data, net_message.cursize);

	do
	{
		ret = CL_GetMessage ();
		switch (ret)
		{
		default:
			Host_Error ("CL_KeepaliveMessage: CL_GetMessage failed");
		case 0:
			break;	// nothing waiting
		case 1:
			Host_Error ("CL_KeepaliveMessage: received a message");
			break;
		case 2:
			if (MSG_ReadByte() != svc_nop)
				Host_Error ("CL_KeepaliveMessage: datagram wasn't a nop");
			break;
		}
	} while (ret);

	net_message = old;
	memcpy (net_message.data, olddata, net_message.cursize);

// check time
	time = Sys_DoubleTime ();
	if (time - lastmsg < 5)
		return;
	lastmsg = time;

// write out a nop
	Con_Printf ("--> client to server keepalive\n");

	MSG_WriteByte (&cls.message, clc_nop);
	NET_SendMessage (cls.netcon, &cls.message);
	SZ_Clear (&cls.message);
}

/*
==================
CL_ParseServerInfo
==================
*/
void CL_ParseServerInfo (void)
{
	const char	*str;
	int		i;
	int		nummodels, numsounds;
	char	model_precache[MAX_FITZQUAKE_MODELS][MAX_QPATH];
	char	sound_precache[MAX_FITZQUAKE_SOUNDS][MAX_QPATH];

	Con_DPrintf ("Serverinfo packet received.\n");

// parse protocol version number
	i = MSG_ReadLong ();
	//johnfitz -- support multiple protocols
	if (i != PROTOCOL_NETQUAKE && i != PROTOCOL_FITZQUAKE && i != PROTOCOL_FITZQUAKE_PLUS)
	{
		Con_Printf ("\n"); //becuase there's no newline after serverinfo print
		Host_Error ("Server returned version %i, not %i, %i or %i\n", i, PROTOCOL_NETQUAKE, PROTOCOL_FITZQUAKE, PROTOCOL_FITZQUAKE_PLUS);
	}

//
// wipe the client_state_t struct
//
	CL_ClearState (i); // Baker --- Sets the protocol here


// parse maxclients
	cl.maxclients = MSG_ReadByte ();
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
	{
		Con_Printf("Bad maxclients (%u) from server\n", cl.maxclients);
		return;
	}
	cl.scores = (scoreboard_t *)Hunk_AllocName (cl.maxclients*sizeof(*cl.scores), "scores");

// parse gametype
	cl.gametype = MSG_ReadByte ();

// parse signon message
	str = MSG_ReadString ();
	q_strlcpy (cl.levelname, str, sizeof(cl.levelname));

// seperate the printfs so the server message can have a color
	Con_Printf ("\n%s\n", Con_Quakebar(40)); //johnfitz
	Con_Printf ("%c%s\n", 2, str);

//johnfitz -- tell user which protocol this is
	Con_Printf ("Using protocol %i\n", i);

// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it

// precache models
	memset (cl.model_precache, 0, sizeof(cl.model_precache));
	for (nummodels=1 ; ; nummodels++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (nummodels==MAX_FITZQUAKE_MODELS)
		{
			Con_Printf ("Server sent too many model precaches\n");
			return;
		}
		q_strlcpy (model_precache[nummodels], str, MAX_QPATH);
		Mod_TouchModel (str);
	}

	//johnfitz -- check for excessive models
	if (nummodels >= MAX_WINQUAKE_MODELS)
		Con_Warning ("%i models exceeds standard limit of %d.\n", nummodels, MAX_WINQUAKE_MODELS); //256
	//johnfitz

// precache sounds
	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));
	for (numsounds=1 ; ; numsounds++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (numsounds==MAX_FITZQUAKE_SOUNDS)
		{
			Con_Printf ("Server sent too many sound precaches\n");
			return;
		}
		q_strlcpy (sound_precache[numsounds], str, MAX_QPATH);
		S_TouchSound (str);
	}

	//johnfitz -- check for excessive sounds
	if (numsounds >= MAX_WINQUAKE_SOUNDS)
		Con_Warning ("%i sounds exceeds standard limit of %d.\n", numsounds, MAX_WINQUAKE_SOUNDS); // 256
	//johnfitz

	// Baker: maps/e1m1.bsp ---> e1m1
	// We need this early for external vis to know if a model is worldmodel or not
	COM_StripExtension (COM_SkipPath(model_precache[1]), cl.worldname, sizeof (cl.worldname) );

//
// now we try to load everything else until a cache allocation fails
//

	for (i=1 ; i<nummodels ; i++)
	{
// Baker: Use this as a place to warn about dumbness
		COM_Uppercase_Check (model_precache[i]); // Baker: Use this as a place to warn about dumbness

		cl.model_precache[i] = Mod_ForName (model_precache[i], false);
		if (cl.model_precache[i] == NULL)
		{
			Con_Printf("Model %s not found\n", model_precache[i]);
			return;
		}
		CL_KeepaliveMessage ();
	}

	S_BeginPrecaching ();
	for (i=1 ; i<numsounds ; i++)
	{
		COM_Uppercase_Check (sound_precache[i]); // Baker: Use this as a place to warn about dumbness

		cl.sound_precache[i] = S_PrecacheSound (sound_precache[i]);
		CL_KeepaliveMessage ();
	}
	S_EndPrecaching ();

// local state
	cl_entities[0].model = cl.worldmodel = cl.model_precache[1];

	LOC_LoadLocations ();	// Read a location file

	R_NewMap ();
	Lists_Refresh_NewMap ();

	//johnfitz -- clear out string; we don't consider identical
	//messages to be duplicates if the map has changed in between
	con_lastcenterstring[0] = 0;
	//johnfitz

	Hunk_Check ();		// make sure nothing is hurt


	warn_about_nehahra_protocol = true; //johnfitz -- warn about nehahra protocol hack once per server connection

//johnfitz -- reset developer stats
	memset(&dev_stats, 0, sizeof(dev_stats));
	memset(&dev_peakstats, 0, sizeof(dev_peakstats));
	memset(&dev_overflows, 0, sizeof(dev_overflows));
}

/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked.  Other attributes can change without relinking.
==================
*/
void CL_ParseUpdate (int bits)
{
	int			i;
	qmodel_t		*model;
	int			modnum;
	qboolean	forcelink;
	entity_t	*ent;
	int			num;
	int			skin;

	if (cls.signon == SIGNONS - 1)
	{	// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS)
	{
		i = MSG_ReadByte ();
		bits |= (i<<8);
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol >= PROTOCOL_FITZQUAKE)
	{
		if (bits & U_EXTEND1)
			bits |= MSG_ReadByte() << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte() << 24;
	}
	//johnfitz

	if (bits & U_LONGENTITY)
		num = MSG_ReadShort ();
	else
		num = MSG_ReadByte ();

	ent = CL_EntityNum (num);

	if (ent->msgtime != cl.mtime[1])
		forcelink = true;	// no previous frame to lerp from
	else
		forcelink = false;

	//johnfitz -- lerping
	if (ent->msgtime + 0.2 < cl.mtime[0]) //more than 0.2 seconds since the last message (most entities think every 0.1 sec)
		ent->lerpflags |= LERP_RESETANIM; //if we missed a think, we'd be lerping from the wrong frame
	//johnfitz

	ent->msgtime = cl.mtime[0];

	if (bits & U_MODEL)
	{
		modnum = MSG_ReadByte ();
		if (modnum >= MAX_FITZQUAKE_MODELS)
			Host_Error ("CL_ParseModel: bad modnum");
	}
	else
		modnum = ent->baseline.modelindex;

	if (bits & U_FRAME)
		ent->frame = MSG_ReadByte ();
	else
		ent->frame = ent->baseline.frame;

	if (bits & U_COLORMAP)
		i = MSG_ReadByte();
	else
		i = ent->baseline.colormap;
	if (!i)
		ent->colormap = 0;
	else
	{
		if (i > cl.maxclients)
			Host_Error ("i >= cl.maxclients");
		ent->colormap = i;
	}
	if (bits & U_SKIN)
		skin = MSG_ReadByte();
	else
		skin = ent->baseline.skin;
	if (skin != ent->skinnum)
	{
		ent->skinnum = skin;
	}
	if (bits & U_EFFECTS)
		ent->effects = MSG_ReadByte();
	else
		ent->effects = ent->baseline.effects;

// shift the known values for interpolation
	VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
	VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);

	if (bits & U_ORIGIN1)
		ent->msg_origins[0][0] = MSG_ReadCoord ();
	else
		ent->msg_origins[0][0] = ent->baseline.origin[0];
	if (bits & U_ANGLE1)
		if (cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
			ent->msg_angles[0][0] = MSG_ReadAngle16();
		else
			ent->msg_angles[0][0] = MSG_ReadAngle();
	else
		ent->msg_angles[0][0] = ent->baseline.angles[0];

	if (bits & U_ORIGIN2)
		ent->msg_origins[0][1] = MSG_ReadCoord ();
	else
		ent->msg_origins[0][1] = ent->baseline.origin[1];
	if (bits & U_ANGLE2)
		if (cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
			ent->msg_angles[0][1] = MSG_ReadAngle16();
		else
			ent->msg_angles[0][1] = MSG_ReadAngle();
	else
		ent->msg_angles[0][1] = ent->baseline.angles[1];

	if (bits & U_ORIGIN3)
		ent->msg_origins[0][2] = MSG_ReadCoord ();
	else
		ent->msg_origins[0][2] = ent->baseline.origin[2];
	if (bits & U_ANGLE3)
		if (cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
			ent->msg_angles[0][2] = MSG_ReadAngle16();
		else
			ent->msg_angles[0][2] = MSG_ReadAngle();
	else
		ent->msg_angles[0][2] = ent->baseline.angles[2];

	//johnfitz -- lerping for movetype_step entities
	if ( bits & U_STEP )
	{
		ent->lerpflags |= LERP_MOVESTEP;
		ent->forcelink = true;
	}
	else
		ent->lerpflags &= ~LERP_MOVESTEP;
	//johnfitz

	//johnfitz -- PROTOCOL_FITZQUAKE and PROTOCOL_NEHAHRA
	if (cl.protocol >= PROTOCOL_FITZQUAKE)
	{
		if (bits & U_ALPHA)
			ent->alpha = MSG_ReadByte();
		else
			ent->alpha = ent->baseline.alpha;
		if (bits & U_FRAME2)
			ent->frame = (ent->frame & 0x00FF) | (MSG_ReadByte() << 8);
		if (bits & U_MODEL2)
			modnum = (modnum & 0x00FF) | (MSG_ReadByte() << 8);
		if (bits & U_LERPFINISH)
		{
			ent->lerpfinish = ent->msgtime + ((float)(MSG_ReadByte()) / 255);
			ent->lerpflags |= LERP_FINISH;
		}
		else
			ent->lerpflags &= ~LERP_FINISH;
	}
	else if (cl.protocol == PROTOCOL_NETQUAKE)
	{
		//HACK: if this bit is set, assume this is PROTOCOL_NEHAHRA
		if (bits & U_TRANS)
		{
			float a,b;

			if (warn_about_nehahra_protocol)
			{
				Con_Warning ("nonstandard update bit, assuming Nehahra protocol\n");
				warn_about_nehahra_protocol = false;
			}

			a = MSG_ReadFloat();
			b = MSG_ReadFloat(); //alpha
			if (a == 2)
				MSG_ReadFloat(); //fullbright (not using this yet)
			ent->alpha = ENTALPHA_ENCODE(b);
		}
		else
			ent->alpha = ent->baseline.alpha;
	}
	//johnfitz

	//johnfitz -- moved here from above
	model = cl.model_precache[modnum];
	if (model != ent->model)
	{
		ent->model = model;
	// automatic animation (torches, etc) can be either all together
	// or randomized
		if (model)
		{
			if (model->synctype == ST_RAND)
				ent->syncbase = (float)(rand()&0x7fff) / 0x7fff;
			else
				ent->syncbase = 0.0;
		}
		else
			forcelink = true;	// hack to make null model players work

		ent->lerpflags |= LERP_RESETANIM; //johnfitz -- don't lerp animation across model changes
	}
	//johnfitz

	if ( forcelink )
	{	// didn't have an update last message
		VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
		VectorCopy (ent->msg_origins[0], ent->origin);
		VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
		VectorCopy (ent->msg_angles[0], ent->angles);
		ent->forcelink = true;
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (entity_t *ent, int version) //johnfitz -- added argument
{
	int	i;
	int bits; //johnfitz

	//johnfitz -- PROTOCOL_FITZQUAKE
	bits = (version == 2) ? MSG_ReadByte() : 0;
	ent->baseline.modelindex = (bits & B_LARGEMODEL) ? MSG_ReadShort() : MSG_ReadByte();
	ent->baseline.frame = (bits & B_LARGEFRAME) ? MSG_ReadShort() : MSG_ReadByte();
	//johnfitz

	ent->baseline.colormap = MSG_ReadByte();
	ent->baseline.skin = MSG_ReadByte();
	for (i=0 ; i<3 ; i++)
	{
		ent->baseline.origin[i] = MSG_ReadCoord ();
		ent->baseline.angles[i] = MSG_ReadAngle ();
	}

	ent->baseline.alpha = (bits & B_ALPHA) ? MSG_ReadByte() : ENTALPHA_DEFAULT; //johnfitz -- PROTOCOL_FITZQUAKE
}


/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata (void)
{
	int		i, j;
	int		bits; //johnfitz

	bits = (unsigned short)MSG_ReadShort (); //johnfitz -- read bits here isntead of in CL_ParseServerMessage()

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & SU_EXTEND1)
		bits |= (MSG_ReadByte() << 16);
	if (bits & SU_EXTEND2)
		bits |= (MSG_ReadByte() << 24);
	//johnfitz

	if (bits & SU_VIEWHEIGHT)
		cl.viewheight = MSG_ReadChar ();
	else
		cl.viewheight = DEFAULT_VIEWHEIGHT;

	if (bits & SU_IDEALPITCH)
		cl.idealpitch = MSG_ReadChar ();
	else
		cl.idealpitch = 0;

	VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);
	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i) )
			cl.punchangle[i] = MSG_ReadChar();
		else
			cl.punchangle[i] = 0;

		if (bits & (SU_VELOCITY1<<i) )
			cl.mvelocity[0][i] = MSG_ReadChar()*16;
		else
			cl.mvelocity[0][i] = 0;
	}

	//johnfitz -- update v_punchangles
	if (v_punchangles[0][0] != cl.punchangle[0] || v_punchangles[0][1] != cl.punchangle[1] || v_punchangles[0][2] != cl.punchangle[2])
	{
		VectorCopy (v_punchangles[0], v_punchangles[1]);
		VectorCopy (cl.punchangle, v_punchangles[0]);
	}
	//johnfitz

// [always sent]	if (bits & SU_ITEMS)
		i = MSG_ReadLong ();

	if (cl.items != i)
	{	// set flash times
		Sbar_Changed ();
		for (j=0 ; j<32 ; j++)
			if ( (i & (1<<j)) && !(cl.items & (1<<j)))
				cl.item_gettime[j] = cl.time;
		cl.items = i;
	}

	cl.onground = (bits & SU_ONGROUND) != 0;
	cl.inwater = (bits & SU_INWATER) != 0;

	if (bits & SU_WEAPONFRAME)
		cl.stats[STAT_WEAPONFRAME] = MSG_ReadByte ();
	else
		cl.stats[STAT_WEAPONFRAME] = 0;

	if (bits & SU_ARMOR)
		i = MSG_ReadByte ();
	else
		i = 0;
	if (cl.stats[STAT_ARMOR] != i)
	{
		cl.stats[STAT_ARMOR] = i;
		Sbar_Changed ();
	}

	if (bits & SU_WEAPON)
		i = MSG_ReadByte ();
	else
		i = 0;
	if (cl.stats[STAT_WEAPON] != i)
	{
		cl.stats[STAT_WEAPON] = i;
		Sbar_Changed ();

		//johnfitz -- lerping
		if (cl.viewent.model != cl.model_precache[cl.stats[STAT_WEAPON]])
			cl.viewent.lerpflags |= LERP_RESETANIM; //don't lerp animation across model changes
		//johnfitz
	}

	i = MSG_ReadShort ();
	if (cl.stats[STAT_HEALTH] != i)
	{
		if (i <= 0)
			memcpy(cl.death_location, cl_entities[cl.viewentity].origin, sizeof(vec3_t));
		cl.stats[STAT_HEALTH] = i;
		Sbar_Changed ();
	}

	i = MSG_ReadByte ();
	if (cl.stats[STAT_AMMO] != i)
	{
		cl.stats[STAT_AMMO] = i;
		Sbar_Changed ();
	}

	for (i=0 ; i<4 ; i++)
	{
		j = MSG_ReadByte ();
		if (cl.stats[STAT_SHELLS+i] != j)
		{
			cl.stats[STAT_SHELLS+i] = j;
			Sbar_Changed ();
		}
	}

	i = MSG_ReadByte ();

	if (standard_quake)
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != i)
		{
			cl.stats[STAT_ACTIVEWEAPON] = i;
			Sbar_Changed ();
		}
	}
	else
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != (1<<i))
		{
			cl.stats[STAT_ACTIVEWEAPON] = (1<<i);
			Sbar_Changed ();
		}
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & SU_WEAPON2)
		cl.stats[STAT_WEAPON] |= (MSG_ReadByte() << 8);
	if (bits & SU_ARMOR2)
		cl.stats[STAT_ARMOR] |= (MSG_ReadByte() << 8);
	if (bits & SU_AMMO2)
		cl.stats[STAT_AMMO] |= (MSG_ReadByte() << 8);
	if (bits & SU_SHELLS2)
		cl.stats[STAT_SHELLS] |= (MSG_ReadByte() << 8);
	if (bits & SU_NAILS2)
		cl.stats[STAT_NAILS] |= (MSG_ReadByte() << 8);
	if (bits & SU_ROCKETS2)
		cl.stats[STAT_ROCKETS] |= (MSG_ReadByte() << 8);
	if (bits & SU_CELLS2)
		cl.stats[STAT_CELLS] |= (MSG_ReadByte() << 8);
	if (bits & SU_WEAPONFRAME2)
		cl.stats[STAT_WEAPONFRAME] |= (MSG_ReadByte() << 8);
	if (bits & SU_WEAPONALPHA)
		cl.viewent.alpha = MSG_ReadByte();
	else
		cl.viewent.alpha = ENTALPHA_DEFAULT;
	//johnfitz
}

/*
=====================
CL_NewTranslation
=====================
*/
void CL_NewTranslation (int slot)
{
	// Baker: Do something here to mark dead skins in future

}

/*
=====================
CL_ParseStatic
=====================
*/
void CL_ParseStatic (int version) //johnfitz -- added a parameter
{
	entity_t *ent;
	int		i;

	i = cl.num_statics;
	if (i >= MAX_FITZQUAKE_STATIC_ENTITIES)
		Host_Error ("Too many static entities");

	ent = &cl_static_entities[i];
	cl.num_statics++;
	CL_ParseBaseline (ent, version); //johnfitz -- added second parameter

// copy it to the current state

	ent->model = cl.model_precache[ent->baseline.modelindex];
	ent->lerpflags |= LERP_RESETANIM | LERP_RESETMOVE; //johnfitz -- lerping  Baker: Added LERP_RESETMOVE to list
	ent->frame = ent->baseline.frame;

	ent->colormap = 0; // Baker: no colormap
	ent->skinnum = ent->baseline.skin;
	ent->effects = ent->baseline.effects;
	ent->alpha = ent->baseline.alpha; //johnfitz -- alpha

	VectorCopy (ent->baseline.origin, ent->origin);
	VectorCopy (ent->baseline.angles, ent->angles);
	R_AddEfrags (ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound (int version) //johnfitz -- added argument
{
	vec3_t		org;
	int			sound_num, vol, atten;
	int			i;

	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (version == 2)
		sound_num = MSG_ReadShort ();
	else
		sound_num = MSG_ReadByte ();
	//johnfitz

	vol = MSG_ReadByte ();
	atten = MSG_ReadByte ();

	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}


extern cvar_t scoreboard_pings;

// Check text for useful information
void CL_ExamineParseText (char *string)
{
	int ping;//, i;
	char *s, *s2, *s3;

	s = string;
	if (!strcmp(string, "Client ping times:\n") && scoreboard_pings.value)
	{
		// Receiving a ping status.  Begin a new parse.
		cl.last_ping_time = cl.time;

		cl.in_ping_parse = true;
		cl.in_ping_parse_slot = 0;

		// If if we requested this ping from the server via the scoreboard
		// Let's null out the string so it doesn't print to console
		if (cl.expecting_ping)
			*string = 0;
	}
	else if (cl.in_ping_parse == true) // In a parse
	{
	// Calculate the ping
		while (*s == ' ')
			s++;
		ping = 0;
		if (*s >= '0' && *s <= '9') // Process the numbers
		{
			while (*s >= '0' && *s <= '9')
				ping = 10 * ping + *s++ - '0';
			if ((*s++ == ' ') && *s && (s2 = strchr(s, '\n')))
			{
				s3 = cl.scores[cl.in_ping_parse_slot].name;
				while ((s3 = strchr(s3, '\n')) && s2)
				{
					s3++;
					s2 = strchr(s2+1, '\n');
				}
				if (s2)
				{
					*s2 = 0;
					if (!strncmp(cl.scores[cl.in_ping_parse_slot].name, s, 15))
					{
						cl.scores[cl.in_ping_parse_slot].ping = ping > 9999 ? 9999 : ping;
						for (cl.in_ping_parse_slot++ ; !*cl.scores[cl.in_ping_parse_slot].name && cl.in_ping_parse_slot < cl.maxclients ; cl.in_ping_parse_slot++);
					}
					*s2 = '\n';
				}
			// If expecting a scoreboard ping (requested from scoreboard)
			// Then null out the response so it doesn't print to console
				if (cl.expecting_ping)
					*string = 0;
			// Ok ... received too many pings for the player slots so
			// Assume this is invalid and reset
				if (cl.in_ping_parse_slot == cl.maxclients)
					cl.in_ping_parse = false;
			}
			else
				cl.in_ping_parse = false;
		}
		else
			cl.in_ping_parse = false;

		// If anything odd happened with the parse, reset.
		if (cl.in_ping_parse == false)
			cl.expecting_ping = false;
	}

}

void CL_Hints_List_f (void)
{
	hint_type_t hint_num;
	Con_Printf ("Client hints:\n");
	for (hint_num = 0; hint_num < MAX_NUM_HINTS; hint_num ++)
	{
		const char* hintname = hintnames[hint_num].keystring;
		const char* hintvalue = cl.hintstrings[hint_num];

		Con_Printf ("%-10s: %s\n", hintname, hintvalue);
	}

//	Con_Printf ("Note that cl.skillhint is %i\n", cl.skillhint);
}

static void CL_Hint_Set (int cl_new_hintnum, const char* cl_set_hintstring)
{
	char * str, *arg1, *arg2;
	int result;
	char stringbufx[MAX_HINT_BUF_64] = {0};
	// Recognized strings on cmd_argv 0 look up and do a switch
	Con_DPrintf ("%s\n", cl_set_hintstring);
	switch ( cl_new_hintnum )
	{
	case hint_game:
		// Host_Game_f
		Con_DPrintf ("Game hint %s to %s\n", hintnames[cl_new_hintnum].keystring, cl_set_hintstring);
		q_strlcpy (stringbufx, cl_set_hintstring, sizeof (stringbufx));

		for (arg1 = stringbufx, arg2 = NULL; *arg1; arg1++)
			if (*arg1 == ' ')
			{
				*arg1 = 0; // NULL it
				arg2 = arg1 + 1;
				break; // Done
			}

		result = Host_Gamedir_Change (stringbufx, arg2, true, &str);
		if (result == -1 /*fail*/)
		{
			Host_Error ("game change to \"%s%s\" failed.\nReason: %s", stringbufx, arg2 ? va(" %s", arg2) : "", str);
		}
		break;

	case hint_skill:
		cl.skillhint = CLAMP(0, Q_atoi(cl_set_hintstring), 3);
		cl.skillhint ++; // So that 0 means unknown.
		Con_DPrintf ("Set skill hint to %i\n", cl.skillhint);
		break;
	default:
		// A client shouldn't error on an unknown hint
		Con_DPrintf ("Unknown hintnum %d from %s", cl_new_hintnum, hintnames[cl_new_hintnum] );
		return;
	}

	// Copy it off so we can display it with cl_hints.
	q_strlcpy (cl.hintstrings[cl_new_hintnum], cl_set_hintstring, MAX_HINT_BUF_64);
}

void CL_Hint_f (const char* in_text)
{
	char textbuf[MAX_HINT_BUF_64];
	char *src = textbuf;
	q_strlcpy (textbuf, in_text, sizeof(textbuf) );


	// find arg2
	while (*src > ' ')
		src++;


	if (src[0] == ' ' && src[1] > ' ' && (src[0] = 0) == 0 /*evile*/)
	{
		const char* harg1 = textbuf;
		const char* harg2 = &src[1];
		keyvalue_t* hint_entry = KeyValue_GetEntry (hintnames, harg1);

		// Kill newlines ...
		src = &src[1];
		while (*src)
		{
			if (*src == '\n')
				*src = 0;
			src ++;
		}

		// If not recognized, ignore it
		if (!hint_entry)
			return;

		Con_DPrintf ("Server hint: %s %s\n", harg1, harg2);
		Con_DPrintf ("Setting hint %i = %s\n", hint_entry->value, harg2);
		CL_Hint_Set (hint_entry->value, harg2);
	}
}

#define SHOWNET(x) if(cl_shownet.value==2)Con_Printf ("%3i:%s\n", msg_readcount-1, x);

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int			cmd;
	int			i;
	char		*str; //johnfitz
	int			total, j, lastcmd; //johnfitz
	qboolean	strip_pqc;

//
// if recording demos, copy the message out
//
	if (cl_shownet.value == 1)
		Con_Printf ("%i ",net_message.cursize);
	else if (cl_shownet.value == 2)
		Con_Printf ("------------------\n");

	cl.onground = false;	// unless the server says otherwise
//
// parse the message
//
	MSG_BeginReading ();

	while (1)
	{
		if (msg_badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte ();

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			return;		// end of message
		}

	// if the high bit of the command byte is set, it is a fast update
		if (cmd & U_SIGNAL) //johnfitz -- was 128, changed for clarity
		{
			SHOWNET("fast update");
			CL_ParseUpdate (cmd&127);
			continue;
		}

		SHOWNET(svc_strings[cmd]);

	// other commands
		switch (cmd)
		{
		default:
			Host_Error ("Illegible server message, previous was %s\n", svc_strings[lastcmd]); //johnfitz -- added svc_strings[lastcmd]
			break;

		case svc_nop:
//			Con_Printf ("svc_nop\n");
			break;

		case svc_time:
			cl.mtime[1] = cl.mtime[0];
			cl.mtime[0] = MSG_ReadFloat ();
			break;

		case svc_clientdata:
			CL_ParseClientdata (); //johnfitz -- removed bits parameter, we will read this inside CL_ParseClientdata()
			break;

		case svc_version:
			i = MSG_ReadLong ();
			//johnfitz -- support multiple protocols
			if (i != PROTOCOL_NETQUAKE && i != PROTOCOL_FITZQUAKE && i != PROTOCOL_FITZQUAKE_PLUS)
				Host_Error ("Server returned version %i, not %i, %i or %i\n", i, PROTOCOL_NETQUAKE, PROTOCOL_FITZQUAKE, PROTOCOL_FITZQUAKE_PLUS);
			cl.protocol = i;
			//johnfitz
			break;

		case svc_disconnect:
			Host_EndGame ("Server disconnected\n");

		case svc_print:
			str = MSG_ReadString ();

			CL_ExamineParseText(str);

			Con_Printf ("%s", str);

			break;

		case svc_centerprint:
			//johnfitz -- log centerprints to console
			str = MSG_ReadString ();
			SCR_CenterPrint (str);
			Con_LogCenterPrint (str);
			//johnfitz
			break;

		case svc_stufftext:
			str = MSG_ReadString();
			switch(cl.protocol)
			{
				case PROTOCOL_NETQUAKE:
					strip_pqc = true;
					break;
				default:
					// ProQuake does not support
					// these protocols
					strip_pqc = false;
					break;
			}
			if(strip_pqc)
			{
				// skip over ProQuake messages,
				// TODO actually interpret them
				// (they are sbar team score
				// updates), see proquake cl_parse.c
				if(*str == 0x01)
				{
					++str;
					while(*str >= 0x01 && *str <= 0x1F)
						++str;
				}
			}

			// Look for a server hint string.
			// Server hint string begins: "//hint "
			if (String_Does_Start_With (str, HINT_MESSAGE_PREIX))
			{
#if 0
				str += 2;
				Con_Printf ("Received server hint: %s\n", str);
#else
				str += strlen (HINT_MESSAGE_PREIX);
				CL_Hint_f (str); // We need this to happen NOW
#endif
				break; // Do not continue.
			}
			Cbuf_AddText (str);

			break;

		case svc_damage:
			V_ParseDamage ();
			break;

		case svc_serverinfo:
			CL_ParseServerInfo ();
			vid.recalc_refdef = true;	// leave intermission full screen
			break;

		case svc_setangle:
			for (i=0 ; i<3 ; i++)
				cl.viewangles[i] = MSG_ReadAngle ();

			if (cls.demoplayback)
			{
				VectorCopy (cl.viewangles, cl.mviewangles[0]);
				VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
				VectorCopy (cl.mviewangles[0], cl.lerpangles);
				cl.mviewangles[0][0] = cl.mviewangles[1][0] = cl.lerpangles[0] = -cl.lerpangles[0];
			}
			if (!cls.demoplayback)
			{
				VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);

				// From ProQuake - hack with cl.last_angle_time to autodetect continuous svc_setangles
				if (cl.last_angle_time > host_time - 0.3)
					cl.last_angle_time = host_time + 0.3;
				else if (cl.last_angle_time > host_time - 0.6)
					cl.last_angle_time = host_time;
				else
					cl.last_angle_time = host_time - 0.3;

				for (i=0 ; i<3 ; i++)
					cl.mviewangles[0][i] = cl.viewangles[i];
			}

			break;

		case svc_setview: // This doesn't happen at intermission cam
			cl.viewentity = MSG_ReadShort ();
			break;

		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
				Host_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
			q_strlcpy (cl_lightstyle[i].map, MSG_ReadString(), MAX_STYLESTRING);
			cl_lightstyle[i].length = Q_strlen(cl_lightstyle[i].map);
			//johnfitz -- save extra info
			if (cl_lightstyle[i].length)
			{
				total = 0;
				cl_lightstyle[i].peak = 'a';
				for (j=0; j<cl_lightstyle[i].length; j++)
				{
					total += cl_lightstyle[i].map[j] - 'a';
					cl_lightstyle[i].peak = q_max(cl_lightstyle[i].peak, cl_lightstyle[i].map[j]);
				}
				cl_lightstyle[i].average = total / cl_lightstyle[i].length + 'a';
			}
			else
				cl_lightstyle[i].average = cl_lightstyle[i].peak = 'm';
			//johnfitz
			break;

		case svc_sound:
			CL_ParseStartSoundPacket();
			break;

		case svc_stopsound:
			i = MSG_ReadShort();
			S_StopSound(i>>3, i&7);
			break;

		case svc_updatename:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatename > MAX_SCOREBOARD");
			q_strlcpy (cl.scores[i].name, MSG_ReadString(), MAX_SCOREBOARDNAME);
			break;

		case svc_updatefrags:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD");
			cl.scores[i].frags = MSG_ReadShort ();
			break;

		case svc_updatecolors:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatecolors > MAX_SCOREBOARD");
			cl.scores[i].colors = MSG_ReadByte ();
			CL_NewTranslation (i);
			break;

		case svc_particle:
			R_ParseParticleEffect ();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i), 1); // johnfitz -- added second parameter
			break;

		case svc_spawnstatic:
			CL_ParseStatic (1); //johnfitz -- added parameter
			break;

		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_setpause:
			cl.paused = MSG_ReadByte ();
			if (cl.paused)
			{
				CDAudio_Pause ();
			}
			else
			{
				CDAudio_Resume ();
			}
			break;

		case svc_signonnum:
			i = MSG_ReadByte ();
			if (i <= cls.signon)
				Host_Error ("Received signon %i when at %i", i, cls.signon);
			cls.signon = i;
			//johnfitz -- if signonnum==2, signon packet has been fully parsed, so check for excessive static ents and efrags
			if (i == 2)
			{
				if (cl.num_statics > 128)
					Con_Warning ("%i static entities exceeds standard limit of 128.\n", cl.num_statics);
				R_CheckEfrags ();
			}
			//johnfitz
			CL_SignonReply ();
			break;

		case svc_killedmonster:
			if (cls.demoplayback && cls.demorewind)
				cl.stats[STAT_MONSTERS]--;
			else
				cl.stats[STAT_MONSTERS]++;
			break;

		case svc_foundsecret:
			if (cls.demoplayback && cls.demorewind)
				cl.stats[STAT_SECRETS]--;
			else
				cl.stats[STAT_SECRETS]++;
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();
			if (i < 0 || i >= MAX_CL_STATS)
				Host_Error ("svc_updatestat: %i is invalid", i);
			cl.stats[i] = MSG_ReadLong ();;
			break;

		case svc_spawnstaticsound:
			CL_ParseStaticSound (1); //johnfitz -- added parameter
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			cl.looptrack = MSG_ReadByte ();

			// Baker: svc_cdtrack changed, we need to stop the old one
			// from playing.  MP3 is particularly affected by this here.
			// Otherwise will play 2 soundtracks at same time!
			CDAudio_Stop();
			if ( (cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1) )
				CDAudio_Play ((byte)cls.forcetrack, true);
			else
				CDAudio_Play ((byte)cl.cdtrack, true);
			break;

		case svc_intermission:
			if (cls.demoplayback && cls.demorewind)
				cl.intermission = 0; // Baker: Demo rewind out of intermission
			else
				cl.intermission = 1;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			break;

		case svc_finale:
			if (cls.demoplayback && cls.demorewind)
				cl.intermission = 0; // Baker: Demo rewind out of intermission
			else
				cl.intermission = 2;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			//johnfitz -- log centerprints to console
			str = MSG_ReadString ();
			SCR_CenterPrint (str);
			Con_LogCenterPrint (str);
			//johnfitz
			break;

		case svc_cutscene:
			if (cls.demoplayback && cls.demorewind)
				cl.intermission = 0; // Baker: Demo rewind out of intermission
			else
				cl.intermission = 3;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			//johnfitz -- log centerprints to console
			str = MSG_ReadString ();
			SCR_CenterPrint (str);
			Con_LogCenterPrint (str);
			//johnfitz
			break;

		case svc_sellscreen:
			Cmd_ExecuteString ("help", src_command);
			break;

		//johnfitz -- new svc types
		case svc_skybox:
			Sky_LoadSkyBox (MSG_ReadString());
			break;

		case svc_bf:
			Cmd_ExecuteString ("bf", src_command);
			break;

		case svc_fog:
			Fog_ParseServerMessage ();
			break;

		case svc_spawnbaseline2: //PROTOCOL_FITZQUAKE
			i = MSG_ReadShort ();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i), 2);
			break;

		case svc_spawnstatic2: //PROTOCOL_FITZQUAKE
			CL_ParseStatic (2);
			break;

		case svc_spawnstaticsound2: //PROTOCOL_FITZQUAKE
			CL_ParseStaticSound (2);
			break;
		//johnfitz
		}

		lastcmd = cmd; //johnfitz
	}
}

