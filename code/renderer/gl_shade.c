/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_shade.c

#include "tr_local.h"
#include "tr_state.h"
#include "gl_common.h"

static qboolean	setArraysOnce;

/*
================
R_ArrayElement

@pjb: Wrapper around qglArrayElement
================
*/
static void APIENTRY GLR_ArrayElement( const shaderCommands_t* input, int stage, GLint index ) {
    qglArrayElement( index );
    (void)( input );
    (void)( stage );
}

/*
================
R_ArrayElementDiscrete

@pjb: OpenGL internal

This is just for OpenGL conformance testing, it should never be the fastest
================
*/
static void APIENTRY GLR_ArrayElementDiscrete( const shaderCommands_t* input, int stage, GLint index ) {
	qglColor4ubv( input->svars[stage].colors[ index ] );
	if ( glState.currenttmu ) {
		qglMultiTexCoord2fARB( 0, input->svars[stage].texcoords[ 0 ][ index ][0], input->svars[stage].texcoords[ 0 ][ index ][1] );
		qglMultiTexCoord2fARB( 1, input->svars[stage].texcoords[ 1 ][ index ][0], input->svars[stage].texcoords[ 1 ][ index ][1] );
	} else {
		qglTexCoord2fv( input->svars[stage].texcoords[ 0 ][ index ] );
	}
	qglVertex3fv( input->xyz[ index ] );
}

/*
===================
R_DrawStripElements

@pjb: OpenGL internal
===================
*/
static int		c_vertexes;		// for seeing how long our average strips are
static int		c_begins;
static void GLR_DrawStripElements( const shaderCommands_t* input, int stage, int numIndexes, const glIndex_t *indexes, void ( APIENTRY *element )( const shaderCommands_t*, int, GLint) ) {
	int i;
	glIndex_t last[3] = { (glIndex_t) -1, (glIndex_t) -1, (glIndex_t) -1 };
	qboolean even;

	c_begins++;

	if ( numIndexes <= 0 ) {
		return;
	}

	qglBegin( GL_TRIANGLE_STRIP );

	// prime the strip
	element( input, stage, indexes[0] );
	element( input, stage, indexes[1] );
	element( input, stage, indexes[2] );
	c_vertexes += 3;

	last[0] = indexes[0];
	last[1] = indexes[1];
	last[2] = indexes[2];

	even = qfalse;

	for ( i = 3; i < numIndexes; i += 3 )
	{
		// odd numbered triangle in potential strip
		if ( !even )
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( indexes[i+0] == last[2] ) && ( indexes[i+1] == last[1] ) )
			{
				element( input, stage, indexes[i+2] );
				c_vertexes++;
				even = qtrue;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( input, stage, indexes[i+0] );
				element( input, stage, indexes[i+1] );
				element( input, stage, indexes[i+2] );

				c_vertexes += 3;

				even = qfalse;
			}
		}
		else
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( last[2] == indexes[i+1] ) && ( last[0] == indexes[i+0] ) )
			{
				element( input, stage, indexes[i+2] );
				c_vertexes++;

				even = qfalse;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( input, stage, indexes[i+0] );
				element( input, stage, indexes[i+1] );
				element( input, stage, indexes[i+2] );
				c_vertexes += 3;

				even = qfalse;
			}
		}

		// cache the last three vertices
		last[0] = indexes[i+0];
		last[1] = indexes[i+1];
		last[2] = indexes[i+2];
	}

	qglEnd();
}



/*
==================
R_DrawElements

@pjb: OpenGL internal

Optionally performs our own glDrawElements that looks for strip conditions
instead of using the single glDrawElements call that may be inefficient
without compiled vertex arrays.
==================
*/
static void GLR_DrawElements( const shaderCommands_t* input, int stage, int numIndexes, const glIndex_t *indexes ) {
	int		primitives;

	primitives = r_primitives->integer;

	// default is to use triangles if compiled vertex arrays are present
	if ( primitives == 0 ) {
		if ( qglLockArraysEXT ) {
			primitives = 2;
		} else {
			primitives = 1;
		}
	}


	if ( primitives == 2 ) {
		qglDrawElements( GL_TRIANGLES, 
						numIndexes,
						GL_INDEX_TYPE,
						indexes );
		return;
	}

	if ( primitives == 1 ) {
		GLR_DrawStripElements( input, stage, numIndexes, indexes, GLR_ArrayElement );
		return;
	}
	
	if ( primitives == 3 ) {
		GLR_DrawStripElements( input, stage, numIndexes, indexes, GLR_ArrayElementDiscrete );
		return;
	}

	// anything else will cause no drawing
}

/*
=============================================================

SURFACE SHADERS

=============================================================
*/

/*
=================
R_BindAnimatedImage

@pjb: OpenGL internal
=================
*/
static void GLR_BindAnimatedImage( textureBundle_t *bundle, float shaderTime ) {
	int		index;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		GL_Bind( bundle->image[0] );
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	index = myftol( shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
	index >>= FUNCTABLE_SIZE2;

	if ( index < 0 ) {
		index = 0;	// may happen with shader time offsets
	}
	index %= bundle->numImageAnimations;

	GL_Bind( bundle->image[ index ] );
}

/*
================
DrawTris

@pjb: OpenGL internal 

Draws triangle outlines for debugging
================
*/
void GLRB_DebugDrawTris( const shaderCommands_t *input ) {
	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	qglDepthRange( 0, 0 );

	qglDisableClientState (GL_COLOR_ARRAY);
	qglDisableClientState (GL_TEXTURE_COORD_ARRAY);

	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);	// padded for SIMD

	if (qglLockArraysEXT) {
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	GLR_DrawElements( input, 0, input->numIndexes, input->indexes );

	if (qglUnlockArraysEXT) {
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
	qglDepthRange( 0, 1 );
}


/*
================
DrawNormals

@pjb: OpenGL internal

Draws vertex normals for debugging
================
*/
void GLRB_DebugDrawNormals( const shaderCommands_t *input ) {
	int		i;
	vec3_t	temp;

	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);
	qglDepthRange( 0, 0 );	// never occluded
	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );

	qglBegin (GL_LINES);
	for (i = 0 ; i < input->numVertexes ; i++) {
		qglVertex3fv (input->xyz[i]);
		VectorMA (input->xyz[i], 2, input->normal[i], temp);
		qglVertex3fv (temp);
	}
	qglEnd ();

	qglDepthRange( 0, 1 );
}

/*
===================
DrawMultitextured

@pjb: OpenGL internal

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
static void GLR_DrawMultitextured( const shaderCommands_t *input, int stage ) {
	shaderStage_t	*pStage;

	pStage = input->xstages[stage];

	GL_State( pStage->stateBits );

	// this is an ugly hack to work around a GeForce driver
	// bug with multitexture and clip planes
	if ( backEnd.viewParms.isPortal ) {
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	//
	// base
	//
	GL_SelectTexture( 0 );
	qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars[stage].texcoords[0] );
	GLR_BindAnimatedImage( &pStage->bundle[0], input->shaderTime );

	//
	// lightmap/secondary pass
	//
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( input->shader->multitextureEnv );
	}

	qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars[stage].texcoords[1] );

	GLR_BindAnimatedImage( &pStage->bundle[1], input->shaderTime );

	GLR_DrawElements( input, stage, input->numIndexes, input->indexes );

	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
	//qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglDisable( GL_TEXTURE_2D );

	GL_SelectTexture( 0 );
}

/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass

@pjb: todo: split the bulk of this out into a helper function and then 
do the GL calls right at the end.
===================
*/
static void GLR_ProjectDlightTexture( const shaderCommands_t* input, int stage ) {
	int		i, l;
#if idppc_altivec
	vec_t	origin0, origin1, origin2;
	float   texCoords0, texCoords1;
	vector float floatColorVec0, floatColorVec1;
	vector float modulateVec, colorVec, zero;
	vector short colorShort;
	vector signed int colorInt;
	vector unsigned char floatColorVecPerm, modulatePerm, colorChar;
	vector unsigned char vSel = (vector unsigned char)(0x00, 0x00, 0x00, 0xff,
							   0x00, 0x00, 0x00, 0xff,
							   0x00, 0x00, 0x00, 0xff,
							   0x00, 0x00, 0x00, 0xff);
#else
	vec3_t	origin;
#endif
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
	MAC_STATIC float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	byte	colorArray[SHADER_MAX_VERTEXES][4];
	unsigned	hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	float	scale;
	float	radius;
	vec3_t	floatColor;
	float	modulate;

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}

#if idppc_altivec
	// There has to be a better way to do this so that floatColor 
	// and/or modulate are already 16-byte aligned.
	floatColorVecPerm = vec_lvsl(0,(float *)floatColor);
	modulatePerm = vec_lvsl(0,(float *)&modulate);
	modulatePerm = (vector unsigned char)vec_splat((vector unsigned int)modulatePerm,0);
	zero = (vector float)vec_splat_s8(0);
#endif

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( input->dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray[0];
		colors = colorArray[0];

		dl = &backEnd.refdef.dlights[l];
#if idppc_altivec
		origin0 = dl->transformed[0];
		origin1 = dl->transformed[1];
		origin2 = dl->transformed[2];
#else
		VectorCopy( dl->transformed, origin );
#endif
		radius = dl->radius;
		scale = 1.0f / radius;

		floatColor[0] = dl->color[0] * 255.0f;
		floatColor[1] = dl->color[1] * 255.0f;
		floatColor[2] = dl->color[2] * 255.0f;
#if idppc_altivec
		floatColorVec0 = vec_ld(0, floatColor);
		floatColorVec1 = vec_ld(11, floatColor);
		floatColorVec0 = vec_perm(floatColorVec0,floatColorVec0,floatColorVecPerm);
#endif
		for ( i = 0 ; i < input->numVertexes ; i++, texCoords += 2, colors += 4 ) {
#if idppc_altivec
			vec_t dist0, dist1, dist2;
#else
			vec3_t	dist;
#endif
			int		clip;

			backEnd.pc.c_dlightVertexes++;

#if idppc_altivec
			//VectorSubtract( origin, input->xyz[i], dist );
			dist0 = origin0 - input->xyz[i][0];
			dist1 = origin1 - input->xyz[i][1];
			dist2 = origin2 - input->xyz[i][2];
			texCoords0 = 0.5f + dist0 * scale;
			texCoords1 = 0.5f + dist1 * scale;

			clip = 0;
			if ( texCoords0 < 0.0f ) {
				clip |= 1;
			} else if ( texCoords0 > 1.0f ) {
				clip |= 2;
			}
			if ( texCoords1 < 0.0f ) {
				clip |= 4;
			} else if ( texCoords1 > 1.0f ) {
				clip |= 8;
			}
			texCoords[0] = texCoords0;
			texCoords[1] = texCoords1;
			
			// modulate the strength based on the height and color
			if ( dist2 > radius ) {
				clip |= 16;
				modulate = 0.0f;
			} else if ( dist2 < -radius ) {
				clip |= 32;
				modulate = 0.0f;
			} else {
				dist2 = Q_fabs(dist2);
				if ( dist2 < radius * 0.5f ) {
					modulate = 1.0f;
				} else {
					modulate = 2.0f * (radius - dist2) * scale;
				}
			}
			clipBits[i] = clip;

			modulateVec = vec_ld(0,(float *)&modulate);
			modulateVec = vec_perm(modulateVec,modulateVec,modulatePerm);
			colorVec = vec_madd(floatColorVec0,modulateVec,zero);
			colorInt = vec_cts(colorVec,0);	// RGBx
			colorShort = vec_pack(colorInt,colorInt);		// RGBxRGBx
			colorChar = vec_packsu(colorShort,colorShort);	// RGBxRGBxRGBxRGBx
			colorChar = vec_sel(colorChar,vSel,vSel);		// RGBARGBARGBARGBA replace alpha with 255
			vec_ste((vector unsigned int)colorChar,0,(unsigned int *)colors);	// store color
#else
			VectorSubtract( origin, input->xyz[i], dist );
			texCoords[0] = 0.5f + dist[0] * scale;
			texCoords[1] = 0.5f + dist[1] * scale;

			clip = 0;
			if ( texCoords[0] < 0.0f ) {
				clip |= 1;
			} else if ( texCoords[0] > 1.0f ) {
				clip |= 2;
			}
			if ( texCoords[1] < 0.0f ) {
				clip |= 4;
			} else if ( texCoords[1] > 1.0f ) {
				clip |= 8;
			}
			// modulate the strength based on the height and color
			if ( dist[2] > radius ) {
				clip |= 16;
				modulate = 0.0f;
			} else if ( dist[2] < -radius ) {
				clip |= 32;
				modulate = 0.0f;
			} else {
				dist[2] = Q_fabs(dist[2]);
				if ( dist[2] < radius * 0.5f ) {
					modulate = 1.0f;
				} else {
					modulate = 2.0f * (radius - dist[2]) * scale;
				}
			}
			clipBits[i] = clip;

			colors[0] = myftol(floatColor[0] * modulate);
			colors[1] = myftol(floatColor[1] * modulate);
			colors[2] = myftol(floatColor[2] * modulate);
			colors[3] = 255;
#endif
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < input->numIndexes ; i += 3 ) {
			int		a, b, c;

			a = input->indexes[i];
			b = input->indexes[i+1];
			c = input->indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );

		qglEnableClientState( GL_COLOR_ARRAY );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

		GL_Bind( tr.dlightImage );
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		if ( dl->additive ) {
			GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		else {
			GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		GLR_DrawElements( input, stage, numIndexes, hitIndexes );
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
}

/*
===================
RB_FogPass

@pjb: OpenGL internal

Blends a fog texture on top of everything else
===================
*/
static void GLRB_FogPass( const shaderCommands_t* input, int stage ) {
	fog_t		*fog;
	int			i;

	qglEnableClientState( GL_COLOR_ARRAY );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars[stage].colors );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars[stage].texcoords[0] );

	fog = tr.world->fogs + input->fogNum;

	for ( i = 0; i < input->numVertexes; i++ ) {
		* ( int * )&input->svars[stage].colors[i] = fog->colorInt;
	}

	RB_CalcFogTexCoords( ( float * ) input->svars[stage].texcoords[0] );

	GL_Bind( tr.fogImage );

	if ( input->shader->fogPass == FP_EQUAL ) {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

	GLR_DrawElements( input, stage, input->numIndexes, input->indexes );
}

/*
** RB_IterateStagesGeneric


@pjb: OpenGL internal
*/
static void GLRB_IterateStagesGeneric( const shaderCommands_t *input )
{
	int stage;

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = input->xstages[stage];

		if ( !pStage )
		{
			break;
		}

		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars[stage].colors );

		//
		// do multitexture
		//
		if ( pStage->bundle[1].image[0] != 0 )
		{
			GLR_DrawMultitextured( input, stage );
		}
		else
		{
			qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars[stage].texcoords[0] );

			//
			// set state
			//
			if ( pStage->bundle[0].vertexLightmap && ( (r_vertexLight->integer && !r_uiFullScreen->integer) || vdConfig.hardwareType == GLHW_PERMEDIA2 ) && r_lightmap->integer )
			{
				GL_Bind( tr.whiteImage );
			}
			else 
				GLR_BindAnimatedImage( &pStage->bundle[0], input->shaderTime );

			GL_State( pStage->stateBits );

			//
			// draw
			//
			GLR_DrawElements( input, stage, input->numIndexes, input->indexes );
		}
		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
		{
			break;
		}
	}
}

/*
** RB_StageIteratorGeneric
*/
void GLRB_StageIteratorGeneric( const shaderCommands_t *input )
{
	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", input->shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	// set polygon offset if necessary
	if ( input->shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	//
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	if ( input->numPasses > 1 || input->shader->multitextureEnv )
	{
		setArraysOnce = qfalse;
		qglDisableClientState (GL_COLOR_ARRAY);
		qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
	}
	else
	{
		setArraysOnce = qtrue;

		qglEnableClientState( GL_COLOR_ARRAY);
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
	}

	//
	// lock XYZ
	//
	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);	// padded for SIMD
	if (qglLockArraysEXT)
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	//
	// enable color and texcoord arrays after the lock if necessary
	//
	if ( !setArraysOnce )
	{
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglEnableClientState( GL_COLOR_ARRAY );
	}

	//
	// call shader function
	//
	GLRB_IterateStagesGeneric( input );

	// 
	// now do any dynamic lighting needed
	//
	if ( input->dlightBits && input->shader->sort <= SS_OPAQUE
		&& !(input->shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
		GLR_ProjectDlightTexture( input, 0 ); // @pjb: todo: check this stage index
	}

	//
	// now do fog
	//
	if ( input->fogNum && input->shader->fogPass ) {
		GLRB_FogPass( input, 0 ); // @pjb: todo: check this stage index
	}

	// 
	// unlock arrays
	//
	if (qglUnlockArraysEXT) 
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}

	//
	// reset polygon offset
	//
	if ( input->shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
}

/*
** RB_StageIteratorVertexLitTexture
*/
void GLRB_StageIteratorVertexLitTexture( const shaderCommands_t *input )
{
	shader_t		*shader;
	shader = input->shader;

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorVertexLitTexturedUnfogged( %s ) ---\n", input->shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	//
	// set arrays and lock
	//
	qglEnableClientState( GL_COLOR_ARRAY);
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY);

	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars[0].colors );
	qglTexCoordPointer( 2, GL_FLOAT, 16, input->texCoords[0][0] );
	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);

	if ( qglLockArraysEXT )
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	//
	// call special shade routine
	//
	GLR_BindAnimatedImage( &input->xstages[0]->bundle[0], input->shaderTime );
	GL_State( input->xstages[0]->stateBits );
	GLR_DrawElements( input, 0, input->numIndexes, input->indexes );

	// 
	// now do any dynamic lighting needed
	//
	if ( input->dlightBits && input->shader->sort <= SS_OPAQUE ) {
		GLR_ProjectDlightTexture( input, 0 );
	}

	//
	// now do fog
	//
	if ( input->fogNum && input->shader->fogPass ) {
		GLRB_FogPass( input, 0 );
	}

	// 
	// unlock arrays
	//
	if (qglUnlockArraysEXT) 
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
}

/*
** RB_StageIteratorLightmappedMultitexture
*/
void GLRB_StageIteratorLightmappedMultitexture( const shaderCommands_t *input )
{
	//
	// log this call
	//
	if ( r_logFile->integer ) {
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorLightmappedMultitexture( %s ) ---\n", input->shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	//
	// set color, pointers, and lock
	//
	GL_State( GLS_DEFAULT );
	qglVertexPointer( 3, GL_FLOAT, 16, input->xyz );

#ifdef REPLACE_MODE
	qglDisableClientState( GL_COLOR_ARRAY );
	qglColor3f( 1, 1, 1 );
	qglShadeModel( GL_FLAT );
#else
	qglEnableClientState( GL_COLOR_ARRAY );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->constantColor255 );
#endif

	//
	// select base stage
	//
	GL_SelectTexture( 0 );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	GLR_BindAnimatedImage( &input->xstages[0]->bundle[0], input->shaderTime );
	qglTexCoordPointer( 2, GL_FLOAT, 16, input->texCoords[0][0] );

	//
	// configure second stage
	//
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( GL_MODULATE );
	}
	GLR_BindAnimatedImage( &input->xstages[0]->bundle[1], input->shaderTime );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglTexCoordPointer( 2, GL_FLOAT, 16, input->texCoords[0][1] );

	//
	// lock arrays
	//
	if ( qglLockArraysEXT ) {
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	GLR_DrawElements( input, 0, input->numIndexes, input->indexes );

	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
	qglDisable( GL_TEXTURE_2D );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	GL_SelectTexture( 0 );
#ifdef REPLACE_MODE
	GL_TexEnv( GL_MODULATE );
	qglShadeModel( GL_SMOOTH );
#endif

	// 
	// now do any dynamic lighting needed
	//
	if ( input->dlightBits && input->shader->sort <= SS_OPAQUE ) {
		GLR_ProjectDlightTexture( input, 0 );
	}

	//
	// now do fog
	//
	if ( input->fogNum && input->shader->fogPass ) {
		GLRB_FogPass( input, 0 );
	}

	//
	// unlock arrays
	//
	if ( qglUnlockArraysEXT ) {
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
}
