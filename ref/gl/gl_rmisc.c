/*
gl_rmisc.c - renderer misceallaneous
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "gl_local.h"
#include "shake.h"
#include "screenfade.h"
#include "cdll_int.h"

static void R_ParseDetailTextures( const char *filename )
{
	byte *afile;
	char *pfile;
	string	token, texname;
	string	detail_texname;
	string	detail_path;
	float	xScale, yScale;
	texture_t	*tex;
	int	i;

	afile = gEngfuncs.fsapi->LoadFile( filename, NULL, false );
	if( !afile ) return;

	pfile = (char *)afile;

	// format: 'texturename' 'detailtexture' 'xScale' 'yScale'
	while(( pfile = COM_ParseFile( pfile, token, sizeof( token ))) != NULL )
	{
		texname[0] = '\0';
		detail_texname[0] = '\0';

		// read texname
		if( token[0] == '{' )
		{
			// NOTE: COM_ParseFile handled some symbols seperately
			// this code will be fix it
			pfile = COM_ParseFile( pfile, token, sizeof( token ));
			Q_snprintf( texname, sizeof( texname ), "{%s", token );
		}
		else Q_strncpy( texname, token, sizeof( texname ));

		// read detailtexture name
		pfile = COM_ParseFile( pfile, token, sizeof( token ));
		Q_strncpy( detail_texname, token, sizeof( detail_texname ));

		// trying the scales or '{'
		pfile = COM_ParseFile( pfile, token, sizeof( token ));

		// read second part of detailtexture name
		if( token[0] == '{' )
		{
			Q_strncat( detail_texname, token, sizeof( detail_texname ));
			pfile = COM_ParseFile( pfile, token, sizeof( token )); // read scales
			Q_strncat( detail_texname, token, sizeof( detail_texname ));
			pfile = COM_ParseFile( pfile, token, sizeof( token )); // parse scales
		}

		Q_snprintf( detail_path, sizeof( detail_path ), "gfx/%s", detail_texname );

		// read scales
		xScale = Q_atof( token );

		pfile = COM_ParseFile( pfile, token, sizeof( token ));
		yScale = Q_atof( token );

		if( xScale <= 0.0f || yScale <= 0.0f )
			continue;

		// search for existing texture and uploading detail texture
		for( i = 0; i < WORLDMODEL->numtextures; i++ )
		{
			tex = WORLDMODEL->textures[i];

			if( Q_stricmp( tex->name, texname ))
				continue;

			tex->dt_texturenum = GL_LoadTexture( detail_path, NULL, 0, TF_FORCE_COLOR );

			// texture is loaded
			if( tex->dt_texturenum )
			{
				gl_texture_t	*glt;

				glt = R_GetTexture( tex->gl_texturenum );
				glt->xscale = xScale;
				glt->yscale = yScale;
			}
			break;
		}
	}

	Mem_Free( afile );
}

#if XASH_RAYTRACING
const float* rt_portal_posteffect_position = NULL;

static const char* rt_trament_modelname = NULL;
cl_entity_t*       rt_trament           = NULL;

static const char* rt_rocketblastdoor_modelname = NULL;
cl_entity_t*       rt_rocketblastdoor           = NULL;

static void RT_ResetTramLights()
{
    typedef struct edef_t
    {
        const char* mapname;
        const char* modelname;
    } edef_t;

    {
        rt_trament_modelname = NULL;
        rt_trament           = NULL;

        edef_t traments[] = {
            { "maps/c0a0.bsp", "*12" },  { "maps/c0a0a.bsp", "*24" }, { "maps/c0a0b.bsp", "*15" },
            { "maps/c0a0c.bsp", "*74" }, { "maps/c0a0d.bsp", "*10" }, { "maps/c0a0e.bsp", "*1" },
        };

        for( int m = 0; m < ( int )RT_ARRAYSIZE( traments ); m++ )
        {
            if( Q_strcmp( WORLDMODEL->name, traments[ m ].mapname ) == 0 )
            {
                rt_trament_modelname = traments[ m ].modelname;
                break;
            }
        }
    }
    {
        rt_rocketblastdoor_modelname = NULL;
        rt_rocketblastdoor           = NULL;

        edef_t blastdoors[] = {
            { "maps/c2a2h.bsp", "*2" },
        };

        for( int m = 0; m < ( int )RT_ARRAYSIZE( blastdoors ); m++ )
        {
            if( Q_strcmp( WORLDMODEL->name, blastdoors[ m ].mapname ) == 0 )
            {
                rt_rocketblastdoor_modelname = blastdoors[ m ].modelname;
                break;
            }
        }
    }
}

void RT_TryFindTramLights()
{
    // if not found yet
    if( rt_trament == NULL )
    {
        // if tram may exist
        if( rt_trament_modelname )
        {
            if( RI.currententity && RI.currentmodel )
            {
                if( Q_strcmp( RI.currentmodel->name, rt_trament_modelname ) == 0 )
                {
                    rt_trament = RI.currententity;
                }
            }
        }
    }
}

qboolean RT_IsBrushIgnored()
{
    if( rt_rocketblastdoor == NULL )
    {
        if( rt_rocketblastdoor_modelname )
        {
            if( RI.currententity && RI.currentmodel )
            {
                if( Q_strcmp( RI.currentmodel->name, rt_rocketblastdoor_modelname ) == 0 )
                {
                    rt_rocketblastdoor = RI.currententity;
                }
            }
        }
    }

    return RI.currententity == rt_rocketblastdoor;
}
#endif

void R_NewMap( void )
{
	texture_t	*tx;
	int	i;

	R_ClearDecals(); // clear all level decals

	R_StudioResetPlayerModels();

	// upload detailtextures
	if( CVAR_TO_BOOL( r_detailtextures ))
	{
		string	mapname, filepath;

		Q_strncpy( mapname, WORLDMODEL->name, sizeof( mapname ));
		COM_StripExtension( mapname );
		Q_sprintf( filepath, "%s_detail.txt", mapname );

		R_ParseDetailTextures( filepath );
	}

	if( gEngfuncs.pfnGetCvarFloat( "v_dark" ))
	{
		screenfade_t		*sf = gEngfuncs.GetScreenFade();
		float			fadetime = 5.0f;
		client_textmessage_t	*title;

		title = gEngfuncs.pfnTextMessageGet( "GAMETITLE" );
		if( ENGINE_GET_PARM( PARM_QUAKE_COMPATIBLE ))
			fadetime = 1.0f;

		if( title )
		{
			// get settings from titles.txt
			sf->fadeEnd = title->holdtime + title->fadeout;
			sf->fadeReset = title->fadeout;
		}
		else sf->fadeEnd = sf->fadeReset = fadetime;

		sf->fadeFlags = FFADE_IN;
		sf->fader = sf->fadeg = sf->fadeb = 0;
		sf->fadealpha = 255;
		sf->fadeSpeed = (float)sf->fadealpha / sf->fadeReset;
		sf->fadeReset += gpGlobals->time;
		sf->fadeEnd += sf->fadeReset;

		gEngfuncs.Cvar_SetValue( "v_dark", 0.0f );
	}

	// clear out efrags in case the level hasn't been reloaded
	for( i = 0; i < WORLDMODEL->numleafs; i++ )
		WORLDMODEL->leafs[i+1].efrags = NULL;

	glState.isFogEnabled = false;
	tr.skytexturenum = -1;
	pglDisable( GL_FOG );

	// clearing texture chains
	for( i = 0; i < WORLDMODEL->numtextures; i++ )
	{
		if( !WORLDMODEL->textures[i] )
			continue;

		tx = WORLDMODEL->textures[i];

		if( !Q_strncmp( tx->name, "sky", 3 ) && tx->width == ( tx->height * 2 ))
			tr.skytexturenum = i;

 		tx->texturechain = NULL;
	}

	R_SetupSky( MOVEVARS->skyName );

	GL_BuildLightmaps ();
	R_GenerateVBO();

	if( gEngfuncs.drawFuncs->R_NewMap != NULL )
		gEngfuncs.drawFuncs->R_NewMap();

#if XASH_RAYTRACING
    RT_ParseStaticLightEntities();
    RT_ResetChapterLogo();
    RT_ResetTramLights();

	// HACKHACK
    if( Q_strcmp( WORLDMODEL->name, "maps/c2a5.bsp" ) == 0 )
    {
        gEngfuncs.Cvar_Set( rt_cvars.rt_me_water_r->name, "150" );
        gEngfuncs.Cvar_Set( rt_cvars.rt_me_water_g->name, "150" );
        gEngfuncs.Cvar_Set( rt_cvars.rt_me_water_b->name, "155" );
    }
    else if( Q_strcmp( WORLDMODEL->name, "maps/c4a1.bsp" ) == 0 )
    {
        gEngfuncs.Cvar_Set( rt_cvars.rt_me_water_r->name, "2" );
        gEngfuncs.Cvar_Set( rt_cvars.rt_me_water_g->name, "1" );
        gEngfuncs.Cvar_Set( rt_cvars.rt_me_water_b->name, "2" );
    }
    else
    {
		// to default
        gEngfuncs.Cvar_Set( rt_cvars.rt_me_water_r->name, NULL );
        gEngfuncs.Cvar_Set( rt_cvars.rt_me_water_g->name, NULL );
        gEngfuncs.Cvar_Set( rt_cvars.rt_me_water_b->name, NULL );
    }

    // HACKHACK
    if( Q_strcmp( WORLDMODEL->name, "maps/c1a4k.bsp" ) == 0 )
    {
        gEngfuncs.Cvar_Set( rt_cvars.rt_normalmap_stren_water->name, "20" );
    }
    else
    {
        // to default
        gEngfuncs.Cvar_Set( rt_cvars.rt_normalmap_stren_water->name, NULL );
    }

    // HACKHACK
    if( Q_strcmp( WORLDMODEL->name, "maps/c3a2d.bsp" ) == 0 )
    {
        static const vec3_t campos    = { 1160.00000f, 268.921875f, -191.937500f };
        rt_portal_posteffect_position = campos;
    }
    else if( Q_strcmp( WORLDMODEL->name, "maps/c5a1.bsp" ) == 0 )
    {
        static const vec3_t campos    = { -1265.96875f, 427.031250f, -2674.29688f };
        rt_portal_posteffect_position = campos;
    }
    else
    {
        rt_portal_posteffect_position = NULL;
    }
#endif
}
