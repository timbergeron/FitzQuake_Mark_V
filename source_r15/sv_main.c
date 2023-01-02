/*  This file is NOT done.  Some 64-bit stuff will/could change it.

Versus Quakespasm:

1. sv_cheats support.  (Disable cheating in coop like god, noclip, etc.)
2. Protocol 668 (smooth rotation)
3. sv_cullentities
4. Smart max edicts

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
// sv_main.c -- server main program

#include "quakedef.h"

server_t		sv;
server_static_t	svs;

static char	localmodels[MAX_FITZQUAKE_MODELS][8];			// inline model names for precache

int sv_protocol = PROTOCOL_FITZQUAKE; //johnfitz

extern qboolean		pr_alpha_supported; //johnfitz

//============================================================================

/*
===============
SV_Protocol_f
===============
*/
void SV_Protocol_f (void)
{
	int i;

	switch (Cmd_Argc())
	{
	case 1:
		Con_Printf ("\"sv_protocol\" is \"%i\"\n", sv_protocol);
		break;
	case 2:
		i = atoi(Cmd_Argv(1));
		if (i != PROTOCOL_NETQUAKE && i != PROTOCOL_FITZQUAKE && i != PROTOCOL_FITZQUAKE_PLUS)
			Con_Printf ("sv_protocol must be %i, %i or %i\n", PROTOCOL_NETQUAKE, PROTOCOL_FITZQUAKE, PROTOCOL_FITZQUAKE_PLUS);
		else
		{
			sv_protocol = i;
			if (sv.active)
				Con_Printf ("changes will not take effect until the next level load.\n");
		}
		break;
	default:
		Con_SafePrintf ("usage: sv_protocol <protocol>\n");
		break;
	}
}

cvar_t sv_cheats = {"sv_cheats", "1", false, true};

// SV_Cheats didn't notify that a level change or map restart was needed
void SV_Cheats_f (cvar_t *var)
{
	if (sv.active)
		Con_Printf ("sv_cheats change will take effect on map restart/change.\n");

}

cvar_t sv_external_ents = {"external_ents", "1", CVAR_NONE};

/*
===============
SV_Init
===============
*/
void SV_Init (void)
{
	int		i;

	Cvar_RegisterVariable (&sv_maxvelocity);
	Cvar_RegisterVariable (&sv_gravity);
	Cvar_RegisterVariable (&sv_friction);
	Cvar_RegisterVariable (&sv_edgefriction);
	Cvar_RegisterVariable (&sv_stopspeed);
	Cvar_RegisterVariable (&sv_maxspeed);
	Cvar_RegisterVariable (&sv_accelerate);
	Cvar_RegisterVariable (&sv_idealpitchscale);
	Cvar_RegisterVariable (&sv_aim);
	Cvar_RegisterVariable (&sv_nostep);
	Cvar_RegisterVariable (&sv_novis);

	Cvar_RegisterVariable (&sv_bouncedownslopes);
	Cvar_RegisterVariable (&sv_cullentities);
	Cvar_RegisterVariable (&sv_imp12hack);
	Cvar_RegisterVariable (&sv_fishfix);

	Cvar_RegisterVariable (&sv_altnoclip); //johnfitz
	Cvar_RegisterVariable (&sv_external_ents);

	Cvar_RegisterVariableWithCallback (&sv_cheats, SV_Cheats_f); // Add notification
	Cmd_AddCommand ("freezeall", SV_Freezeall_f);


	Cmd_AddCommand ("sv_protocol", SV_Protocol_f); //johnfitz
	Cmd_AddCommand ("sv_hints", SV_Hints_List_f); //johnfitz

	for (i=0 ; i<MAX_FITZQUAKE_MODELS ; i++)
		sprintf (localmodels[i], "*%i", i);
}

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count)
{
	int		i, v;

	if (sv.datagram.cursize > sv.datagram.maxsize-16)
		return;
	MSG_WriteByte (&sv.datagram, svc_particle);
	MSG_WriteCoord (&sv.datagram, org[0]);
	MSG_WriteCoord (&sv.datagram, org[1]);
	MSG_WriteCoord (&sv.datagram, org[2]);
	for (i=0 ; i<3 ; i++)
	{
		v = dir[i]*16;
		if (v > 127)
			v = 127;
		else if (v < -128)
			v = -128;
		MSG_WriteChar (&sv.datagram, v);
	}
	MSG_WriteByte (&sv.datagram, count);
	MSG_WriteByte (&sv.datagram, color);
}

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

==================
*/
void SV_StartSound (edict_t *entity, int channel, const char *sample, int volume, float attenuation)
{
    int         sound_num, ent;
    int 		i, field_mask;

	if (volume < 0 || volume > 255)
		Host_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Host_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Host_Error ("SV_StartSound: channel = %i", channel);

	if (sv.datagram.cursize > sv.datagram.maxsize-16)
		return;

// find precache number for sound
    for (sound_num=1 ; sound_num<MAX_FITZQUAKE_SOUNDS && sv.sound_precache[sound_num] ; sound_num++)
	{
        if (!strcmp(sample, sv.sound_precache[sound_num]))
            break;
	}

    if ( sound_num == MAX_FITZQUAKE_SOUNDS || !sv.sound_precache[sound_num] )
    {
        Con_Printf ("SV_StartSound: %s not precacheed\n", sample);
        return;
    }

	ent = NUM_FOR_EDICT(entity);

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_ATTENUATION;

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (ent >= 8192)
	{
		if (sv.protocol == PROTOCOL_NETQUAKE)
			return; //don't send any info protocol can't support
		else
			field_mask |= SND_LARGEENTITY;
	}
	if (sound_num >= 256 || channel >= 8)
	{
		if (sv.protocol == PROTOCOL_NETQUAKE)
			return; //don't send any info protocol can't support
		else
			field_mask |= SND_LARGESOUND;
	}
	//johnfitz

// directed messages go only to the entity the are targeted on
	MSG_WriteByte (&sv.datagram, svc_sound);
	MSG_WriteByte (&sv.datagram, field_mask);
	if (field_mask & SND_VOLUME)
		MSG_WriteByte (&sv.datagram, volume);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteByte (&sv.datagram, attenuation*64);

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (field_mask & SND_LARGEENTITY)
	{
		MSG_WriteShort (&sv.datagram, ent);
		MSG_WriteByte (&sv.datagram, channel);
	}
	else
		MSG_WriteShort (&sv.datagram, (ent<<3) | channel);
	if (field_mask & SND_LARGESOUND)
		MSG_WriteShort (&sv.datagram, sound_num);
	else
		MSG_WriteByte (&sv.datagram, sound_num);
	//johnfitz

	for (i=0 ; i<3 ; i++)
		MSG_WriteCoord (&sv.datagram, entity->v.origin[i]+0.5*(entity->v.mins[i]+entity->v.maxs[i]));
}

/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/

void SV_Hints_List_f (void)
{
	hint_type_t hint_num;
	if (sv.active)
	{
		Con_Printf ("Server hints:\n");
		for (hint_num = 0; hint_num < MAX_NUM_HINTS; hint_num ++)
		{
			const char* hintname = hintnames[hint_num].keystring;
			const char* hintvalue = sv.hintstrings[hint_num];

			Con_Printf ("%-10s: %s\n", hintname, hintvalue);
		}
	} else Con_Printf ("no active server\n");
}

static void SV_HintStrings_NewMap (void)
{
	const char* gamedir_name = COM_SkipPath (com_gamedir);
	const char* hud_style_name = Gamedir_TypeName ();

	hint_type_t hint_num;

	for (hint_num = 0; hint_num < MAX_NUM_HINTS; hint_num ++)
	{
		char* sv_make_hint_string = sv.hintstrings[hint_num];
		switch (hint_num)
		{
		case hint_game:
			q_snprintf (sv_make_hint_string, MAX_HINT_BUF_64, "%s %s", gamedir_name, hud_style_name);
			break;
		case hint_skill:
//			if (!pr_global_struct->deathmatch) // Don't bother with a deathmatch skill hint
				q_snprintf (sv_make_hint_string, MAX_HINT_BUF_64, "%d", sv.current_skill );
			break;
		default:
			Host_Error ("SV_HintStrings: Unknown hint_num %d", hint_num);
		}
	}

}

// If early, send the game only.
// If late, send everything.  Even the game again.  Client clears that on new map.
// Which occurs real early.
static void SV_InsertHints (client_t *client, qboolean early)
{
	const char *sv_hint_string;
	int hstart = 0, hlast_plus_one = MAX_NUM_HINTS;

	hint_type_t hint_num;
	Con_DPrintf ("Send hints ... ones = %i\n", early);

	// Must send gamedir change very early.
	// Other hints must occur AFTER
	if (early)
		hstart = hint_game /* 0 */, hlast_plus_one = 1;
	else hstart = 0 /* 1 */, hlast_plus_one = MAX_NUM_HINTS;

	for (hint_num = hstart; hint_num < hlast_plus_one; hint_num ++)
	{
		if (sv.hintstrings[hint_num][0] == 0)
			continue; // omitted

		// Issue a commented out svc_stufftext like "//hint game warp -quoth"
		sv_hint_string = va("%s%s %s\n", HINT_MESSAGE_PREIX, hintnames[hint_num].keystring, &sv.hintstrings[hint_num][0]);
		Con_DPrintf ("Sending: %s", sv_hint_string); // No newline, hint_string already has one
		MSG_WriteByte (&client->message, svc_stufftext);
		MSG_WriteString (&client->message, sv_hint_string);
	}
}



void SV_SendServerinfo (client_t *client)
{
	const char		**s;
	char			message[2048];
	int				i; //johnfitz

	MSG_WriteByte (&client->message, svc_print);
	q_snprintf (message, sizeof(message), "%c\n%s %1.2f SERVER (%i CRC)\n", 2, ENGINE_NAME, FITZQUAKE_VERSION, pr_crc); //johnfitz -- include fitzquake version
	MSG_WriteString (&client->message, message);

	SV_InsertHints (client, true /* early hints */); // Baker -- throw some extra hints to client here.

	MSG_WriteByte (&client->message, svc_serverinfo);
	MSG_WriteLong (&client->message, sv.protocol); //johnfitz -- sv.protocol instead of PROTOCOL_VERSION
	MSG_WriteByte (&client->message, svs.maxclients);

	if (!coop.value && deathmatch.value)
		MSG_WriteByte (&client->message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&client->message, GAME_COOP);

	q_strlcpy (message, PR_GetString(sv.edicts->v.message), sizeof(message));

	MSG_WriteString (&client->message,message);

	//johnfitz -- only send the first 256 model and sound precaches if protocol is 15
	for (i=0,s = sv.model_precache+1 ; *s; s++,i++)
		if (sv.protocol != PROTOCOL_NETQUAKE || i < 256)
			MSG_WriteString (&client->message, *s);
	MSG_WriteByte (&client->message, 0);

	for (i=0,s = sv.sound_precache+1 ; *s ; s++,i++)
		if (sv.protocol != PROTOCOL_NETQUAKE || i < 256)
			MSG_WriteString (&client->message, *s);
	MSG_WriteByte (&client->message, 0);
	//johnfitz

// send music
	MSG_WriteByte (&client->message, svc_cdtrack);
	MSG_WriteByte (&client->message, sv.edicts->v.sounds);
	MSG_WriteByte (&client->message, sv.edicts->v.sounds);

// set view
	MSG_WriteByte (&client->message, svc_setview);
	MSG_WriteShort (&client->message, NUM_FOR_EDICT(client->edict));

// send late hints
	Con_DPrintf ("Ready to send late hints\n");
	SV_InsertHints (client, false /* late hints */); // Baker -- throw some extra hints to client here.


	MSG_WriteByte (&client->message, svc_signonnum);
	MSG_WriteByte (&client->message, 1);


	client->sendsignon = true;
	client->spawned = false;		// need prespawn, spawn, etc
}

/*
================
SV_ConnectClient

Initializes a client_t for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
void SV_ConnectClient (int clientnum)
{
	edict_t			*ent;
	client_t		*client;
	int				edictnum;
	struct qsocket_s *netconnection;
	int				i;
	float			spawn_parms[NUM_SPAWN_PARMS];

	client = svs.clients + clientnum;

	Con_DPrintf ("Client %s connected\n", NET_QSocketGetAddressString(client->netconnection));

	edictnum = clientnum+1;

	ent = EDICT_NUM(edictnum);

// set up the client_t
	netconnection = client->netconnection;

	if (sv.loadgame)
		memcpy (spawn_parms, client->spawn_parms, sizeof(spawn_parms));
	memset (client, 0, sizeof(*client));
	client->netconnection = netconnection;

	strcpy (client->name, "unconnected");
	client->active = true;
	client->spawned = false;
	client->edict = ent;
	client->message.data = client->msgbuf;
	client->message.maxsize = sizeof(client->msgbuf);
	client->message.allowoverflow = true;		// we can catch it
	client->privileged = false;

	if (sv.loadgame)
	{
		memcpy (client->spawn_parms, spawn_parms, sizeof(spawn_parms));
	}
	else
	{
	// call the progs to get default spawn parms for the new client
		PR_ExecuteProgram (pr_global_struct->SetNewParms);
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			client->spawn_parms[i] = (&pr_global_struct->parm1)[i];
	}

	SV_SendServerinfo (client);
}


/*
===================
SV_CheckForNewClients

===================
*/
void SV_CheckForNewClients (void)
{
	struct qsocket_s	*ret;
	int				i;

//
// check for new connections
//
	while (1)
	{
		ret = NET_CheckNewConnections ();
		if (!ret)
			break;

	//
	// init a new client structure
	//
		for (i=0 ; i<svs.maxclients ; i++)
			if (!svs.clients[i].active)
				break;
		if (i == svs.maxclients)
			Host_Error ("Host_CheckForNewClients: no free clients");

		svs.clients[i].netconnection = ret;
		SV_ConnectClient (i);

		net_activeconnections++;
	}
}



/*
===============================================================================

FRAME UPDATES

===============================================================================
*/

/*
==================
SV_ClearDatagram

==================
*/
void SV_ClearDatagram (void)
{
	SZ_Clear (&sv.datagram);
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

static int	fatbytes;
static byte	fatpvs[MAX_MAP_LEAFS/8];

void SV_AddToFatPVS (vec3_t org, mnode_t *node, qmodel_t *worldmodel) //johnfitz -- added worldmodel as a parameter
{
	int		i;
	byte	*pvs;
	mplane_t	*plane;
	float	d;

	while (1)
	{
	// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				pvs = Mod_LeafPVS ( (mleaf_t *)node, worldmodel); //johnfitz -- worldmodel as a parameter
				for (i=0 ; i<fatbytes ; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		d = DotProduct (org, plane->normal) - plane->dist;
		if (d > 8)
			node = node->children[0];
		else if (d < -8)
			node = node->children[1];
		else
		{	// go down both
			SV_AddToFatPVS (org, node->children[0], worldmodel); //johnfitz -- worldmodel as a parameter
			node = node->children[1];
		}
	}
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
byte *SV_FatPVS (vec3_t org, qmodel_t *worldmodel) //johnfitz -- added worldmodel as a parameter
{
	fatbytes = (worldmodel->numleafs+31)>>3;
	Q_memset (fatpvs, 0, fatbytes);
	SV_AddToFatPVS (org, worldmodel->nodes, worldmodel); //johnfitz -- worldmodel as a parameter
	return fatpvs;
}


qboolean Q1BSP_Trace(qmodel_t *model, int forcehullnum, int frame, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, trace_t *trace)
{
	hull_t *hull;
	vec3_t size;
	vec3_t start_l, end_l;
	vec3_t offset;

	memset (trace, 0, sizeof(trace_t));
	trace->fraction = 1;
	trace->allsolid = true;

	VectorSubtract (maxs, mins, size);
	if (forcehullnum >= 1 && forcehullnum <= MAX_MAP_HULLS && model->hulls[forcehullnum-1].available)
		hull = &model->hulls[forcehullnum-1];
	else
	{
		if (size[0] < 3 || !model->hulls[1].available)
			hull = &model->hulls[0];
		else if (size[0] <= 32)
		{
			if (size[2] < 54 && model->hulls[3].available)
				hull = &model->hulls[3]; // 32x32x36 (half-life's crouch)
			else
				hull = &model->hulls[1];
		}
		else
			hull = &model->hulls[2];
	}

// calculate an offset value to center the origin
	VectorSubtract (hull->clip_mins, mins, offset);
	VectorSubtract(start, offset, start_l);
	VectorSubtract(end, offset, end_l);
	SV_RecursiveHullCheck(hull, hull->firstclipnode, 0, 1, start_l, end_l, trace);
	if (trace->fraction == 1)
	{
		VectorCopy (end, trace->endpos);
	}
	else
	{
		VectorAdd (trace->endpos, offset, trace->endpos);
	}

	return trace->fraction != 1;
}


qboolean SV_InvisibleToClient(edict_t *viewer, edict_t *seen)
{
	int i;
	trace_t tr;
	vec3_t start;
	vec3_t end;

//	if (seen->v->solid == SOLID_BSP)
//		return false;	//bsp ents are never culled this way

	//stage 1: check against their origin
	VectorAdd(viewer->v.origin, viewer->v.view_ofs, start);
	tr.fraction = 1;

	if (!Q1BSP_Trace (sv.worldmodel, 1, 0, start, seen->v.origin, vec3_origin, vec3_origin, &tr))
		return false;	//wasn't blocked


	//stage 2: check against their bbox
	for (i = 0; i < 8; i++)
	{
		end[0] = seen->v.origin[0] + ((i&1)? seen->v.mins[0]: seen->v.maxs[0]);
		end[1] = seen->v.origin[1] + ((i&2)? seen->v.mins[1]: seen->v.maxs[1]);
		end[2] = seen->v.origin[2] + ((i&4)? seen->v.mins[2]+0.1: seen->v.maxs[2]);

		tr.fraction = 1;
		if (!Q1BSP_Trace (sv.worldmodel, 1, 0, start, end, vec3_origin, vec3_origin, &tr))
			return false;	//this trace went through, so don't cull
	}

	return true;
}

#if 0 // Baker: This doesn't actually work ...
/*
=============
SV_VisibleToClient -- johnfitz

PVS test encapsulated in a nice function
=============
*/
qboolean SV_VisibleToClient (edict_t *client, edict_t *test, qmodel_t *worldmodel)
{
	byte	*pvs;
	vec3_t	org;
	int		i;

	VectorAdd (client->v.origin, client->v.view_ofs, org);
	pvs = SV_FatPVS (org, worldmodel);

	for (i=0 ; i < test->num_leafs ; i++)
		if (pvs[test->leafnums[i] >> 3] & (1 << (test->leafnums[i]&7) ))
			return true;

	return false;
}
#endif // Baker: This isn't actually used in code + doesn't work

//=============================================================================

/*
=============
SV_WriteEntitiesToClient

=============
*/
void SV_WriteEntitiesToClient (edict_t	*clent, sizebuf_t *msg)
{
	int		e, i;
	int		bits;
	byte	*pvs;
	vec3_t	org;
	float	miss;
	edict_t	*ent;

// find the client's PVS
	VectorAdd (clent->v.origin, clent->v.view_ofs, org);
	pvs = SV_FatPVS (org, sv.worldmodel);

// send over all entities (excpet the client) that touch the pvs
	ent = NEXT_EDICT(sv.edicts);
	for (e=1 ; e<sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
	{

		if (ent != clent)	// clent is ALLWAYS sent
		{
			// ignore ents without visible models
			if (!ent->v.modelindex || !(const char*)(PR_GetString(ent->v.model))[0] ) // Baker: Future question, why did I const char* that
				continue;

			//johnfitz -- don't send model>255 entities if protocol is 15
			if (sv.protocol == PROTOCOL_NETQUAKE && (int)ent->v.modelindex & 0xFF00)
				continue;

			if (!sv_novis.value)
			{
				// ignore if not touching a PV leaf
				for (i=0 ; i < ent->num_leafs ; i++)
					if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i]&7) ))
						break;
				if (i == ent->num_leafs)
					continue;		// not visible
			}

			if (sv_cullentities.value) // 1 = players.  2 = everything.
				if (e <= svs.maxclients || sv_cullentities.value > 1 ) // A player or we are checking everything
					if (SV_InvisibleToClient(clent, ent))
						continue; // Entity cannot be seen, don't send it
		}

		//johnfitz -- max size for protocol 15 is 18 bytes, not 16 as originally
		//assumed here.  And, for protocol 85 the max size is actually 24 bytes.
		if (msg->cursize + 24 > msg->maxsize)
		{
			//johnfitz -- less spammy overflow message
			if (!dev_overflows.packetsize || dev_overflows.packetsize + CONSOLE_RESPAM_TIME < realtime )
			{
				Con_Printf ("Packet overflow!\n");
				dev_overflows.packetsize = realtime;
			}
			goto stats;
			//johnfitz
		}

// send an update
		bits = 0;

		for (i=0 ; i<3 ; i++)
		{
			miss = ent->v.origin[i] - ent->baseline.origin[i];
			if ( miss < -0.1 || miss > 0.1 )
				bits |= U_ORIGIN1<<i;
		}

		if ( ent->v.angles[0] != ent->baseline.angles[0] )
			bits |= U_ANGLE1;

		if ( ent->v.angles[1] != ent->baseline.angles[1] )
			bits |= U_ANGLE2;

		if ( ent->v.angles[2] != ent->baseline.angles[2] )
			bits |= U_ANGLE3;

		if (ent->v.movetype == MOVETYPE_STEP)
			bits |= U_STEP;	// don't mess up the step animation

		if (ent->baseline.colormap != ent->v.colormap)
			bits |= U_COLORMAP;

		if (ent->baseline.skin != ent->v.skin)
			bits |= U_SKIN;

		if (ent->baseline.frame != ent->v.frame)
			bits |= U_FRAME;

		if (ent->baseline.effects != ent->v.effects)
			bits |= U_EFFECTS;

		if (ent->baseline.modelindex != ent->v.modelindex)
			bits |= U_MODEL;

		//johnfitz -- alpha
		if (pr_alpha_supported)
		{
			// TODO: find a cleaner place to put this code
			eval_t	*val;
			val = GETEDICTFIELDVALUE(ent, eval_alpha);
			if (val)
				ent->alpha = ENTALPHA_ENCODE(val->_float);
		}

		//don't send invisible entities unless they have effects
		if (ent->alpha == ENTALPHA_ZERO && !ent->v.effects)
			continue;
		//johnfitz

		//johnfitz -- PROTOCOL_FITZQUAKE
		if (sv.protocol != PROTOCOL_NETQUAKE)
		{

			if (ent->baseline.alpha != ent->alpha) bits |= U_ALPHA;
			if (bits & U_FRAME && (int)ent->v.frame & 0xFF00) bits |= U_FRAME2;
			if (bits & U_MODEL && (int)ent->v.modelindex & 0xFF00) bits |= U_MODEL2;
			if (ent->sendinterval) bits |= U_LERPFINISH;
			if (bits >= 65536) bits |= U_EXTEND1;
			if (bits >= 16777216) bits |= U_EXTEND2;
		}
		//johnfitz

		if (e >= 256)
			bits |= U_LONGENTITY;

		if (bits >= 256)
			bits |= U_MOREBITS;

	//
	// write the message
	//
		MSG_WriteByte (msg, bits | U_SIGNAL);

		if (bits & U_MOREBITS)
			MSG_WriteByte (msg, bits>>8);

		//johnfitz -- PROTOCOL_FITZQUAKE
		if (bits & U_EXTEND1)
			MSG_WriteByte(msg, bits>>16);
		if (bits & U_EXTEND2)
			MSG_WriteByte(msg, bits>>24);
		//johnfitz

		if (bits & U_LONGENTITY)
			MSG_WriteShort (msg,e);
		else
			MSG_WriteByte (msg,e);

		if (bits & U_MODEL)
			MSG_WriteByte (msg,	ent->v.modelindex);
		if (bits & U_FRAME)
			MSG_WriteByte (msg, ent->v.frame);
		if (bits & U_COLORMAP)
			MSG_WriteByte (msg, ent->v.colormap);
		if (bits & U_SKIN)
			MSG_WriteByte (msg, ent->v.skin);
		if (bits & U_EFFECTS)
			MSG_WriteByte (msg, ent->v.effects);
		if (bits & U_ORIGIN1)
			MSG_WriteCoord (msg, ent->v.origin[0]);
		if (bits & U_ANGLE1)
			if (sv.protocol == PROTOCOL_FITZQUAKE_PLUS)
				MSG_WriteAngle16(msg, ent->v.angles[0]);
			else
				MSG_WriteAngle(msg, ent->v.angles[0]);
		if (bits & U_ORIGIN2)
			MSG_WriteCoord (msg, ent->v.origin[1]);
		if (bits & U_ANGLE2)
			if (sv.protocol == PROTOCOL_FITZQUAKE_PLUS)
				MSG_WriteAngle16(msg, ent->v.angles[1]);
			else
				MSG_WriteAngle(msg, ent->v.angles[1]);
		if (bits & U_ORIGIN3)
			MSG_WriteCoord (msg, ent->v.origin[2]);
		if (bits & U_ANGLE3)
			if (sv.protocol == PROTOCOL_FITZQUAKE_PLUS)
				MSG_WriteAngle16(msg, ent->v.angles[2]);
			else
				MSG_WriteAngle(msg, ent->v.angles[2]);

		//johnfitz -- PROTOCOL_FITZQUAKE
		if (bits & U_ALPHA)
			MSG_WriteByte(msg, ent->alpha);
		if (bits & U_FRAME2)
			MSG_WriteByte(msg, (int)ent->v.frame >> 8);
		if (bits & U_MODEL2)
			MSG_WriteByte(msg, (int)ent->v.modelindex >> 8);
		if (bits & U_LERPFINISH)
			MSG_WriteByte(msg, (byte)(Q_rint((ent->v.nextthink-sv.time)*255)));
		//johnfitz
	}

	//johnfitz -- devstats
stats:
	if (msg->cursize > MAX_WINQUAKE_DATAGRAM /*1024*/ && dev_peakstats.packetsize <= MAX_WINQUAKE_DATAGRAM /*1024*/)
		Con_Warning ("%i byte packet exceeds standard limit of %d.\n", msg->cursize, MAX_WINQUAKE_DATAGRAM /*1024*/);
	dev_stats.packetsize = msg->cursize;
	dev_peakstats.packetsize = q_max(msg->cursize, dev_peakstats.packetsize);
	//johnfitz
}

/*
=============
SV_CleanupEnts

=============
*/
void SV_CleanupEnts (void)
{
	int		e;
	edict_t	*ent;

	ent = NEXT_EDICT(sv.edicts);
	for (e=1 ; e<sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
	{
		ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
	}

}

/*
==================
SV_WriteClientdataToMessage

==================
*/
void SV_WriteClientdataToMessage (edict_t *ent, sizebuf_t *msg)
{
	int		bits;
	int		i;
	edict_t	*other;
	int		items;
	eval_t	*val;

//
// send a damage message
//
	if (ent->v.dmg_take || ent->v.dmg_save)
	{
		other = PROG_TO_EDICT(ent->v.dmg_inflictor);
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, ent->v.dmg_save);
		MSG_WriteByte (msg, ent->v.dmg_take);
		for (i=0 ; i<3 ; i++)
			MSG_WriteCoord (msg, other->v.origin[i] + 0.5*(other->v.mins[i] + other->v.maxs[i]));

		ent->v.dmg_take = 0;
		ent->v.dmg_save = 0;
	}

//
// send the current viewpos offset from the view entity
//
	SV_SetIdealPitch ();		// how much to look up / down ideally

// a fixangle might get lost in a dropped packet.  Oh well.
	if ( ent->v.fixangle )
	{
		MSG_WriteByte (msg, svc_setangle);
		for (i=0 ; i < 3 ; i++)
			MSG_WriteAngle (msg, ent->v.angles[i] );
		ent->v.fixangle = 0;
	}

	bits = 0;

	if (ent->v.view_ofs[2] != DEFAULT_VIEWHEIGHT)
		bits |= SU_VIEWHEIGHT;

	if (ent->v.idealpitch)
		bits |= SU_IDEALPITCH;

// stuff the sigil bits into the high bits of items for sbar, or else
// mix in items2
	val = GETEDICTFIELDVALUE(ent, eval_items2);

	if (val)
		items = (int)ent->v.items | ((int)val->_float << 23);
	else
		items = (int)ent->v.items | ((int)pr_global_struct->serverflags << 28);

	bits |= SU_ITEMS;

	if ( (int)ent->v.flags & FL_ONGROUND)
		bits |= SU_ONGROUND;

	if ( ent->v.waterlevel >= 2)
		bits |= SU_INWATER;

	for (i=0 ; i<3 ; i++)
	{
		if (ent->v.punchangle[i])
			bits |= (SU_PUNCH1<<i);
		if (ent->v.velocity[i])
			bits |= (SU_VELOCITY1<<i);
	}

	if (ent->v.weaponframe)
		bits |= SU_WEAPONFRAME;

	if (ent->v.armorvalue)
		bits |= SU_ARMOR;

//	if (ent->v.weapon)
		bits |= SU_WEAPON;

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (sv.protocol != PROTOCOL_NETQUAKE)
	{
		if (bits & SU_WEAPON && SV_ModelIndex(PR_GetString(ent->v.weaponmodel)) & 0xFF00) bits |= SU_WEAPON2;
		if ((int)ent->v.armorvalue & 0xFF00) bits |= SU_ARMOR2;
		if ((int)ent->v.currentammo & 0xFF00) bits |= SU_AMMO2;
		if ((int)ent->v.ammo_shells & 0xFF00) bits |= SU_SHELLS2;
		if ((int)ent->v.ammo_nails & 0xFF00) bits |= SU_NAILS2;
		if ((int)ent->v.ammo_rockets & 0xFF00) bits |= SU_ROCKETS2;
		if ((int)ent->v.ammo_cells & 0xFF00) bits |= SU_CELLS2;
		if (bits & SU_WEAPONFRAME && (int)ent->v.weaponframe & 0xFF00) bits |= SU_WEAPONFRAME2;
		if (bits & SU_WEAPON && ent->alpha != ENTALPHA_DEFAULT) bits |= SU_WEAPONALPHA; //for now, weaponalpha = client entity alpha
		if (bits >= 65536) bits |= SU_EXTEND1;
		if (bits >= 16777216) bits |= SU_EXTEND2;
	}
	//johnfitz

// send the data

	MSG_WriteByte (msg, svc_clientdata);
	MSG_WriteShort (msg, bits);

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & SU_EXTEND1) MSG_WriteByte(msg, bits>>16);
	if (bits & SU_EXTEND2) MSG_WriteByte(msg, bits>>24);
	//johnfitz

	if (bits & SU_VIEWHEIGHT)
		MSG_WriteChar (msg, ent->v.view_ofs[2]);

	if (bits & SU_IDEALPITCH)
		MSG_WriteChar (msg, ent->v.idealpitch);

	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i))
			MSG_WriteChar (msg, ent->v.punchangle[i]);
		if (bits & (SU_VELOCITY1<<i))
			MSG_WriteChar (msg, ent->v.velocity[i]/16);
	}

// [always sent]	if (bits & SU_ITEMS)
	MSG_WriteLong (msg, items);

	if (bits & SU_WEAPONFRAME)
		MSG_WriteByte (msg, ent->v.weaponframe);
	if (bits & SU_ARMOR)
		MSG_WriteByte (msg, ent->v.armorvalue);
	if (bits & SU_WEAPON)
		MSG_WriteByte (msg, SV_ModelIndex(PR_GetString(ent->v.weaponmodel)));

	MSG_WriteShort (msg, ent->v.health);
	MSG_WriteByte (msg, ent->v.currentammo);
	MSG_WriteByte (msg, ent->v.ammo_shells);
	MSG_WriteByte (msg, ent->v.ammo_nails);
	MSG_WriteByte (msg, ent->v.ammo_rockets);
	MSG_WriteByte (msg, ent->v.ammo_cells);

	if (standard_quake)
	{
		MSG_WriteByte (msg, ent->v.weapon);
	}
	else
	{
		for(i=0;i<32;i++)
		{
			if ( ((int)ent->v.weapon) & (1<<i) )
			{
				MSG_WriteByte (msg, i);
				break;
			}
		}
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & SU_WEAPON2)
		MSG_WriteByte (msg, SV_ModelIndex(PR_GetString(ent->v.weaponmodel)) >> 8);
	if (bits & SU_ARMOR2)
		MSG_WriteByte (msg, (int)ent->v.armorvalue >> 8);
	if (bits & SU_AMMO2)
		MSG_WriteByte (msg, (int)ent->v.currentammo >> 8);
	if (bits & SU_SHELLS2)
		MSG_WriteByte (msg, (int)ent->v.ammo_shells >> 8);
	if (bits & SU_NAILS2)
		MSG_WriteByte (msg, (int)ent->v.ammo_nails >> 8);
	if (bits & SU_ROCKETS2)
		MSG_WriteByte (msg, (int)ent->v.ammo_rockets >> 8);
	if (bits & SU_CELLS2)
		MSG_WriteByte (msg, (int)ent->v.ammo_cells >> 8);
	if (bits & SU_WEAPONFRAME2)
		MSG_WriteByte (msg, (int)ent->v.weaponframe >> 8);
	if (bits & SU_WEAPONALPHA)
		MSG_WriteByte (msg, ent->alpha); //for now, weaponalpha = client entity alpha
	//johnfitz
}

/*
=======================
SV_SendClientDatagram
=======================
*/
qboolean SV_SendClientDatagram (client_t *client)
{
		byte		buf[MAX_FITZQUAKE_DATAGRAM];
		sizebuf_t	msg;

		msg.data = buf;
		msg.maxsize = sv.datagram.maxsize; //sizeof(buf);
		msg.cursize = 0;

		//johnfitz -- if client is nonlocal, use smaller max size so packets aren't fragmented
		if (Q_strcmp (NET_QSocketGetAddressString(client->netconnection), "LOCAL") != 0)
			msg.maxsize = 1024; //MAX_FITZQUAKE_DATAGRAM_MTU;
		//johnfitz

		MSG_WriteByte (&msg, svc_time);
		MSG_WriteFloat (&msg, sv.time);

	// add the client specific data to the datagram
		SV_WriteClientdataToMessage (client->edict, &msg);

		SV_WriteEntitiesToClient (client->edict, &msg);

	// copy the server datagram if there is space
		if (msg.cursize + sv.datagram.cursize < msg.maxsize)
			SZ_Write (&msg, sv.datagram.data, sv.datagram.cursize);

	// send the datagram
		if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
		{
			SV_DropClient (true);// if the message couldn't send, kick off
			return false;
		}

		return true;
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
void SV_UpdateToReliableMessages (void)
{
	int			i, j;
	client_t *client;

// check for changes to be sent over the reliable streams
	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (host_client->old_frags != host_client->edict->v.frags)
		{
			for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
			{
				if (!client->active)
					continue;
				MSG_WriteByte (&client->message, svc_updatefrags);
				MSG_WriteByte (&client->message, i);
				MSG_WriteShort (&client->message, host_client->edict->v.frags);
			}

			host_client->old_frags = host_client->edict->v.frags;
		}
	}

	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
	{
		if (!client->active)
			continue;
		SZ_Write (&client->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);
	}

	SZ_Clear (&sv.reliable_datagram);
}


/*
=======================
SV_SendNop

Send a nop message without trashing or sending the accumulated client
message buffer
=======================
*/
void SV_SendNop (client_t *client)
{
	sizebuf_t	msg;
	byte		buf[4];

	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;

	MSG_WriteChar (&msg, svc_nop);

	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
		SV_DropClient (true);	// if the message couldn't send, kick off
	client->last_message = realtime;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int			i;

// update frags, names, etc
	SV_UpdateToReliableMessages ();

// build individual updates
	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		if (host_client->spawned)
		{
			if (!SV_SendClientDatagram (host_client))
				continue;
		}
		else
		{
		// the player isn't totally in the game yet
		// send small keepalive messages if too much time has passed
		// send a full message when the next signon stage has been requested
		// some other message data (name changes, etc) may accumulate
		// between signon stages
			if (!host_client->sendsignon)
			{
				if (realtime - host_client->last_message > 5)
					SV_SendNop (host_client);
				continue;	// don't send out non-signon messages
			}
		}

		// check for an overflowed message.  Should only happen
		// on a very fucked up connection that backs up a lot, then
		// changes level
		if (host_client->message.overflowed)
		{
			SV_DropClient (true);
			host_client->message.overflowed = false;
			continue;
		}

		if (host_client->message.cursize || host_client->dropasap)
		{
			if (!NET_CanSendMessage (host_client->netconnection))
			{
//				I_Printf ("can't write\n");
				continue;
			}

			if (host_client->dropasap)
			{
				SV_DropClient (false);	// went to another level
			}
			else
			{
				if (NET_SendMessage (host_client->netconnection
				, &host_client->message) == -1)
					SV_DropClient (true);	// if the message couldn't send, kick off
				SZ_Clear (&host_client->message);
				host_client->last_message = realtime;
				host_client->sendsignon = false;
			}
		}
	}


// clear muzzle flashes
	SV_CleanupEnts ();
}


/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex (const char *name)
{
	int		i;

	if (!name || !name[0])
		return 0;

	for (i=0 ; i<MAX_FITZQUAKE_MODELS && sv.model_precache[i] ; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;
	if (i==MAX_FITZQUAKE_MODELS || !sv.model_precache[i])
		Host_Error ("SV_ModelIndex: model %s not precached", name);
	return i;
}

/*
================
SV_CreateBaseline
================
*/
void SV_CreateBaseline (void)
{
	int			i;
	edict_t		*svent;
	int			entnum;
	int			bits; //johnfitz -- PROTOCOL_FITZQUAKE

	for (entnum = 0; entnum < sv.num_edicts ; entnum++)
	{
	// get the current server version
		svent = EDICT_NUM(entnum);
		if (svent->free)
			continue;
		if (entnum > svs.maxclients && !svent->v.modelindex)
			continue;

	//
	// create entity baseline
	//
		VectorCopy (svent->v.origin, svent->baseline.origin);
		VectorCopy (svent->v.angles, svent->baseline.angles);
		svent->baseline.frame = svent->v.frame;
		svent->baseline.skin = svent->v.skin;
		if (entnum > 0 && entnum <= svs.maxclients)
		{
			svent->baseline.colormap = entnum;
			svent->baseline.modelindex = SV_ModelIndex("progs/player.mdl");
			svent->baseline.alpha = ENTALPHA_DEFAULT; //johnfitz -- alpha support
		}
		else
		{
			svent->baseline.colormap = 0;
			svent->baseline.modelindex = SV_ModelIndex(PR_GetString(svent->v.model));
			svent->baseline.alpha = svent->alpha; //johnfitz -- alpha support
		}

		//johnfitz -- PROTOCOL_FITZQUAKE
		bits = 0;
		if (sv.protocol == PROTOCOL_NETQUAKE) //still want to send baseline in PROTOCOL_NETQUAKE, so reset these values
		{
			if (svent->baseline.modelindex & 0xFF00)
				svent->baseline.modelindex = 0;
			if (svent->baseline.frame & 0xFF00)
				svent->baseline.frame = 0;
			svent->baseline.alpha = ENTALPHA_DEFAULT;
		}
		else //decide which extra data needs to be sent
		{
			if (svent->baseline.modelindex & 0xFF00)
				bits |= B_LARGEMODEL;
			if (svent->baseline.frame & 0xFF00)
				bits |= B_LARGEFRAME;
			if (svent->baseline.alpha != ENTALPHA_DEFAULT)
				bits |= B_ALPHA;
		}
		//johnfitz

	//
	// add to the message
	//
		//johnfitz -- PROTOCOL_FITZQUAKE
		if (bits)
			MSG_WriteByte (&sv.signon, svc_spawnbaseline2);
		else
			MSG_WriteByte (&sv.signon, svc_spawnbaseline);
		//johnfitz

		MSG_WriteShort (&sv.signon,entnum);

		//johnfitz -- PROTOCOL_FITZQUAKE
		if (bits)
			MSG_WriteByte (&sv.signon, bits);

		if (bits & B_LARGEMODEL)
			MSG_WriteShort (&sv.signon, svent->baseline.modelindex);
		else
			MSG_WriteByte (&sv.signon, svent->baseline.modelindex);

		if (bits & B_LARGEFRAME)
			MSG_WriteShort (&sv.signon, svent->baseline.frame);
		else
			MSG_WriteByte (&sv.signon, svent->baseline.frame);
		//johnfitz

		MSG_WriteByte (&sv.signon, svent->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->baseline.skin);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteCoord(&sv.signon, svent->baseline.origin[i]);
			MSG_WriteAngle(&sv.signon, svent->baseline.angles[i]);
		}

		//johnfitz -- PROTOCOL_FITZQUAKE
		if (bits & B_ALPHA)
			MSG_WriteByte (&sv.signon, svent->baseline.alpha);
		//johnfitz
	}
}


/*
================
SV_SendReconnect

Tell all the clients that the server is changing levels
================
*/
void SV_SendReconnect (void)
{
	byte	data[128];
	sizebuf_t	msg;

	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof(data);

	MSG_WriteChar (&msg, svc_stufftext);
	MSG_WriteString (&msg, "reconnect\n");
	NET_SendToAll (&msg, 5.0);

	if (cls.state != ca_dedicated)
		Cmd_ExecuteString ("reconnect\n", src_command);
}


/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (void)
{
	int		i, j;

	svs.serverflags = pr_global_struct->serverflags;

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

	// call the progs to get default spawn parms for the new client
		pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
		PR_ExecuteProgram (pr_global_struct->SetChangeParms);
		for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
			host_client->spawn_parms[j] = (&pr_global_struct->parm1)[j];
	}
}


/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
typedef struct
{
	const char *modelname;			// "maps/e1m1.bsp"
	const char *checkstring;		// This string must be found in the entities (i.e. the problem to address)
	const char *patch_before_this;	// Insert before this text
	const char *patch;				// The "FIX"
} map_patches_t;

map_patches_t map_patches[] =
{
	{	"maps/e1m1.bsp",
		"{\n\"classname\" \"func_door\"\n\"targetname\" \"t4\"\n\"angle\" \"-2\"\n\"spawnflags\" \"1\"\n\"sounds\" \"2\"\n\"model\" \"*15\"\n}",
		"\"model\" \"*15\"",
		"\"lip\" \"-4\"\n\"origin\" \"0 0 -2\"\n"
	},
	{
		"maps/e1m2.bsp",
		"{\n\"sounds\" \"3\"\n\"classname\" \"func_door\"\n\"angle\" \"270\"\n\"wait\" \"-1\"\n\"model\" \"*34\"\n}\n",
		"\"model\" \"*34\"",
		"\"origin\" \"-3 0 0\"\n"
	},

	{
		"maps/e1m2.bsp",
		"{\n\"classname\" \"func_door\"\n\"angle\" \"90\"\n\"targetname\" \"t110\"\n\"wait\" \"-1\"\n\"model\" \"*33\"\n}\n",
		"\"model\" \"*33\"",
		"\"origin\" \"-3 0 0\"\n"
	},
	{ NULL }, // Terminator
};

static char* MapPatch (const char* modelname, char* entstring)
{
	char* patched_entstring = NULL;
	char *insertion_point;

	map_patches_t* map_patch;

	Con_DPrintf ("Map patch check\n");
	for (map_patch = &map_patches[0]; map_patch->modelname; map_patch++)
	{
		if (Q_strcasecmp (map_patch->modelname, modelname) != 0)
			continue; // Model name doesn't match

		if (strstr (entstring, map_patch->checkstring) == NULL)
			continue;

		insertion_point = strstr (entstring, map_patch->patch_before_this);

		if (insertion_point)
		{
			// Perform the operation
			int entstrlen = strlen (entstring);
			int patchstrlen = strlen (map_patch->patch);
			int newbufsize = entstrlen + patchstrlen + 1; // +1 for null term

			char* newbuf = malloc ( newbufsize);
			char temp = insertion_point[0];

			insertion_point[0] = 0;

		q_strlcpy (newbuf, entstring, newbufsize);
		q_strlcat (newbuf, map_patch->patch, newbufsize);
		insertion_point[0] = temp;
		q_strlcat (newbuf, insertion_point, newbufsize);

			// Already did one patch so free before replacing
			if (patched_entstring)
				free (patched_entstring);

			entstring = patched_entstring = newbuf;
//			Con_Printf ("Patch performed\n");
		}
	}

	return patched_entstring;
}

char *entity_string = NULL;

void SV_SpawnServer (const char *server)
{
	edict_t		*ent;
	int			i;

	// let's not have any servers with no name
	if (hostname.string[0] == 0)
		Cvar_Set ("hostname", "UNNAMED");
	scr_centertime_off = 0;

	Con_DPrintf ("SpawnServer: %s\n",server);
	svs.changelevel_issued = false;		// now safe to issue another

//
// tell all connected clients that we are going to a new level
//
	if (sv.active)
	{
		SV_SendReconnect ();
	}

//
// make cvars consistant
//
	if (coop.value)
		Cvar_Set ("deathmatch", "0");

//  How did this get here?????  It shouldn't have been here.
//	Cvar_SetValue ("skill", (float)sv.current_skill);

	sv.disallow_cheats = !sv_cheats.value; // Set cvar state
//
// set up the new server
//
	//memset (&sv, 0, sizeof(sv));
	Host_ClearMemory ();

	Con_DPrintf ("Host clear memory\n");

	q_strlcpy (sv.name, server, sizeof(sv.name) );

	sv.current_skill = (int)(skill.value + 0.5);
	sv.current_skill = CLAMP (0, sv.current_skill, 3);
	sv.protocol = sv_protocol; // johnfitz

// Construct the hint strings now that we know the skill level
	SV_HintStrings_NewMap ();

// load progs to get entity field count
	PR_LoadProgs ();

// allocate server memory

	switch (sv_protocol)
	{
	case PROTOCOL_NETQUAKE:
		//sv.datagram.maxsize = sv_protocol == sizeof(sv.datagram_buf); // Baker: Was unused
		sv.datagram.maxsize		= MAX_WINQUAKE_DATAGRAM /* 1024 */ ;
		break;

	case PROTOCOL_FITZQUAKE:
	case PROTOCOL_FITZQUAKE_PLUS:
		sv.datagram.maxsize		= MAX_FITZQUAKE_DATAGRAM /* 32000 */ ;
		break;
	}
	sv.datagram.cursize = 0;
	sv.datagram.data = sv.datagram_buf;

	sv.reliable_datagram.maxsize = sv.datagram.maxsize;
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	switch (sv_protocol)
	{
	case PROTOCOL_NETQUAKE:
		sv.signon.maxsize = MAX_WINQUAKE_MSGLEN - 2; //MAX_WINQUAKE_MAXMESSAGE /* 8192 */ ;
		break;

	case PROTOCOL_FITZQUAKE:
	case PROTOCOL_FITZQUAKE_PLUS:
		sv.signon.maxsize = sizeof(sv.signon_buf); // MAX_FITZQUAKE_MSGLEN-2
		break;
	}

//	Con_Printf ("sv.datagram.maxsize is %d\n", sv.datagram.maxsize);
//	Con_Printf ("sv.signon.maxsize is %d\n", sv.signon.maxsize);
//	Con_Printf ("sv_protocol is %d\n", sv_protocol);
	sv.signon.cursize = 0;
	sv.signon.data = sv.signon_buf;

	sv.state = ss_loading;
	sv.paused = false;

	sv.time = 1.0;

	q_strlcpy (sv.name, server, sizeof(sv.name));
	q_snprintf (sv.modelname, sizeof(sv.modelname), "maps/%s.bsp", server);
	sv.worldmodel = Mod_ForName (sv.modelname, false);
	if (!sv.worldmodel)
	{
		// Baker: This is a rather irregular way for the server to be shutdown.
		// It is sort of a "CL_Disconnect lite".  Doesn't stop demo recording, although
		// I'm not sure it should.  My guess is yes.
		// In single player, this isn't setting ca_disconnected or signons to 0.
		// Isn't setting key_dest to console.
#pragma message ("This sv map not found couldn't spawn server needs love")
		Con_Printf ("Couldn't spawn server %s\n", sv.modelname);
		SCR_EndLoadingPlaque (); // Baker: any disconnect state should end the loading plague, right?
		sv.active = false;
		return;
	}



// Earliest point we can read the entity string, since we need the model loaded.
// Read entity_string from memory
	{
		int mark = Hunk_LowMark ();
		char* patched;

	if (sv_external_ents.value &&
		(entity_string = (char *)COM_LoadHunkFile_Limited (va ("maps/%s.ent", sv.name), sv.worldmodel->loadinfo.searchpath ) )      )
		Con_Printf ("External .ent file: Using entfile maps/%s.ent\n", sv.name);
	else entity_string = sv.worldmodel->entities; // Point it to the standard entities string

		patched = MapPatch(sv.modelname, entity_string);
		if (patched)
		{
			Hunk_FreeToLowMark (mark);
			entity_string = Hunk_Strdup (patched, "ent patch string");

			free (patched);
			Con_Printf ("Map entity string patched.\n");
		}
	}


// determine max edicts to use
	switch (sv_protocol)
	{
	case PROTOCOL_NETQUAKE:
		sv.max_edicts = MAX_WINQUAKE_EDICTS;
		break;

	case PROTOCOL_FITZQUAKE:
	case PROTOCOL_FITZQUAKE_PLUS:
		if (max_edicts.value == 0) // Automatic guess counting "classname"
		{
			int classname_count = COM_StringCount (entity_string, "classname");
			sv.max_edicts = classname_count * 1.5;
			Con_DPrintf ("Setting server edicts maximum to %i edicts on classname count of %i\n", sv.max_edicts, classname_count);
		} else sv.max_edicts = max_edicts.value;
		sv.max_edicts = CLAMP(MIN_SANE_EDICTS_512, sv.max_edicts, MAX_SANE_EDICTS_8192);

		break;
	}

// allocate the memory
	sv.edicts = Hunk_AllocName (sv.max_edicts * pr_edict_size, "edicts");

// leave slots at start for clients only
	sv.num_edicts = svs.maxclients + 1;
	for (i=0 ; i<svs.maxclients ; i++)
	{
		ent = EDICT_NUM(i+1);
		svs.clients[i].edict = ent;
	}

	sv.models[1] = sv.worldmodel;

//
// clear world interaction links
//
	SV_ClearWorld ();

	sv.sound_precache[0] = pr_strings;

	sv.model_precache[0] = pr_strings;
	sv.model_precache[1] = sv.modelname;

	for (i=1 ; i<sv.worldmodel->numsubmodels ; i++)
	{
		sv.model_precache[i + 1] = localmodels[i];
		sv.models[i+1] = Mod_ForName (localmodels[i], false);
	}

//
// load the rest of the entities
//
	ent = EDICT_NUM(0);
	memset (&ent->v, 0, progs->entityfields * 4);
	ent->free = false;
	ent->v.model = PR_SetEngineString(sv.worldmodel->name);
	ent->v.modelindex = 1;		// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	if (coop.value)
		pr_global_struct->coop = coop.value;
	else
		pr_global_struct->deathmatch = deathmatch.value;

	pr_global_struct->mapname = PR_SetEngineString(sv.name);

// serverflags are for cross level information (sigils)
	pr_global_struct->serverflags = svs.serverflags;

	ED_LoadFromFile (entity_string); // Baker loads from memory, not file.

	sv.active = true;

// all setup is completed, any further precache statements are errors
	sv.state = ss_active;

// run two frames to allow everything to settle
	host_frametime = 0.1;
	SV_Physics (); // frame 1
	SV_Physics (); // frame 2

// create a baseline for more efficient communications
	SV_CreateBaseline ();

	//johnfitz -- warn if signon buffer larger than standard server can handle
//	Con_Printf ("Signon size is %i\n", sv.signon.cursize);
	if (sv.signon.cursize > (MAX_WINQUAKE_MSGLEN /*8000*/ - 2) ) //max size that will fit into 8000-sized client->message buffer with 2 extra bytes on the end
//		Con_Warning ("%i byte signon buffer exceeds standard limit of 7998.\n", sv.signon.cursize);
		Con_Warning ("%i byte signon buffer exceeds standard limit of %d.\n", sv.signon.cursize, (MAX_WINQUAKE_MSGLEN /*8000*/ -2 ));
	//johnfitz

// send serverinfo to all connected clients
	for (i=0,host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_SendServerinfo (host_client);

	//CL_ClearTEnts ();
	Con_DPrintf ("Server spawned.\n");
}

