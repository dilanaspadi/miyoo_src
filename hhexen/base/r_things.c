
//**************************************************************************
//**
//** r_things.c : Heretic 2 : Raven Software, Corp.
//**
//** $Revision: 575 $
//** $Date: 2011-04-13 22:06:28 +0300 (Wed, 13 Apr 2011) $
//**
//**************************************************************************

#include "h2stdinc.h"
#include <math.h>
#include "h2def.h"
#include "r_local.h"
#ifdef RENDER3D
#include "ogl_def.h"
#endif

void R_DrawColumn (void);
void R_DrawFuzzColumn (void);
void R_DrawAltFuzzColumn(void);
//void R_DrawTranslatedAltFuzzColumn(void);

typedef struct
{
	int		x1, x2;

	int		column;
	int		topclip;
	int		bottomclip;
} maskdraw_t;

/*

Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn CLOCKWISE around the axis.
This is not the same as the angle, which increases counter clockwise
(protractor).  There was a lot of stuff grabbed wrong, so I changed it...

*/

fixed_t			pspritescale, pspriteiscale;

// constant arrays used for psprite clipping and initializing clipping
#ifndef RENDER3D
short			negonearray[SCREENWIDTH];
short			screenheightarray[SCREENWIDTH];
#endif

boolean			LevelUseFullBright;

// variables used to look up and range check thing_t sprites patches
spritedef_t		*sprites;
int			numsprites;

#ifndef RENDER3D
static lighttable_t	**spritelights;
#endif

static spriteframe_t	sprtemp[30];
static int		maxframe;
static const char	*spritename;


/*
===============================================================================

						INITIALIZATION FUNCTIONS

===============================================================================
*/

/*
=================
=
= R_InstallSpriteLump
=
= Local function for R_InitSprites
=================
*/

void R_InstallSpriteLump (int lump, unsigned frame, unsigned rotation, boolean flipped)
{
	int		r;

	if (frame >= 30 || rotation > 8)
		I_Error ("R_InstallSpriteLump: Bad frame characters in lump %i", lump);

	if ((int)frame > maxframe)
		maxframe = frame;

	if (rotation == 0)
	{
	// the lump should be used for all rotations
		if (sprtemp[frame].rotate == false)
		{
			I_Error ("R_InitSprites: Sprite %s frame %c has multip rot=0 lump",
				 spritename, 'A'+frame);
		}
		if (sprtemp[frame].rotate == true)
		{
			I_Error ("R_InitSprites: Sprite %s frame %c has rotations and a rot=0 lump",
				 spritename, 'A'+frame);
		}

		sprtemp[frame].rotate = false;
		for (r = 0; r < 8; r++)
		{
			sprtemp[frame].lump[r] = lump - firstspritelump;
			sprtemp[frame].flip[r] = (byte)flipped;
		}
		return;
	}

	// the lump is only used for one rotation
	if (sprtemp[frame].rotate == false)
	{
		I_Error ("R_InitSprites: Sprite %s frame %c has rotations and a rot=0 lump",
			 spritename, 'A'+frame);
	}

	sprtemp[frame].rotate = true;

	rotation--;		// make 0 based
	if (sprtemp[frame].lump[rotation] != -1)
	{
		I_Error ("R_InitSprites: Sprite %s : %c : %c has two lumps mapped to it",
			 spritename, 'A'+frame, '1' + rotation);
	}

	sprtemp[frame].lump[rotation] = lump - firstspritelump;
	sprtemp[frame].flip[rotation] = (byte)flipped;
}

/*
=================
=
= R_InitSpriteDefs
=
= Pass a null terminated list of sprite names (4 chars exactly) to be used
= Builds the sprite rotation matrixes to account for horizontally flipped
= sprites.  Will report an error if the lumps are inconsistant
=
Only called at startup
=
= Sprite lump names are 4 characters for the actor, a letter for the frame,
= and a number for the rotation, A sprite that is flippable will have an
= additional letter/number appended.  The rotation character can be 0 to
= signify no rotations
=================
*/

void R_InitSpriteDefs (const char **namelist)
{
	const char	**check;
	int		i, l, frame, rotation;
	int		start, end;

// count the number of sprite names
	check = namelist;
	while (*check != NULL)
		check++;
	numsprites = check - namelist;

	if (!numsprites)
		return;

	sprites = (spritedef_t *) Z_Malloc(numsprites *sizeof(*sprites), PU_STATIC, NULL);

	start = firstspritelump - 1;
	end = lastspritelump + 1;

// scan all the lump names for each of the names, noting the highest
// frame letter
// Just compare 4 characters as ints
	for (i = 0; i < numsprites; i++)
	{
		spritename = namelist[i];
		memset (sprtemp, -1, sizeof(sprtemp));

		maxframe = -1;

		//
		// scan the lumps, filling in the frames for whatever is found
		//
		for (l = start + 1; l < end; l++)
		{
			if (memcmp(lumpinfo[l].name, namelist[i], 4) == 0)
			{
				frame = lumpinfo[l].name[4] - 'A';
				rotation = lumpinfo[l].name[5] - '0';
				R_InstallSpriteLump (l, frame, rotation, false);
				if (lumpinfo[l].name[6])
				{
					frame = lumpinfo[l].name[6] - 'A';
					rotation = lumpinfo[l].name[7] - '0';
					R_InstallSpriteLump (l, frame, rotation, true);
				}
			}
		}

		//
		// check the frames that were found for completeness
		//
		if (maxframe == -1)
		{
			//continue;
			sprites[i].numframes = 0;
			if (shareware)
				continue;
			I_Error ("R_InitSprites: No lumps found for sprite %s", namelist[i]);
		}

		maxframe++;
		for (frame = 0; frame < maxframe; frame++)
		{
			switch ((int)sprtemp[frame].rotate)
			{
			case -1:	// no rotations were found for that frame at all
				I_Error ("R_InitSprites: No patches found for %s frame %c",
					 namelist[i], frame+'A');
			case 0:	// only the first rotation is needed
				break;

			case 1:	// must have all 8 frames
				for (rotation = 0; rotation < 8; rotation++)
				{
					if (sprtemp[frame].lump[rotation] == -1)
					{
						I_Error ("R_InitSprites: Sprite %s frame %c is missing rotations",
							 namelist[i], frame+'A');
					}
				}
			}
		}

		//
		// allocate space for the frames present and copy sprtemp to it
		//
		sprites[i].numframes = maxframe;
		sprites[i].spriteframes =
			(spriteframe_t *) Z_Malloc (maxframe * sizeof(spriteframe_t), PU_STATIC, NULL);
		memcpy (sprites[i].spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));
	}
}


/*
===============================================================================

							GAME FUNCTIONS

===============================================================================
*/

vissprite_t	vissprites[MAXVISSPRITES], *vissprite_p;

/*
===================
=
= R_InitSprites
=
= Called at program start
===================
*/

void R_InitSprites (const char **namelist)
{
#ifndef RENDER3D
	int		i;

	for (i = 0; i < SCREENWIDTH; i++)
	{
		negonearray[i] = -1;
	}
#endif
	R_InitSpriteDefs (namelist);
}


/*
===================
=
= R_ClearSprites
=
= Called at frame start
===================
*/

void R_ClearSprites (void)
{
	vissprite_p = vissprites;
}


/*
===================
=
= R_NewVisSprite
=
===================
*/

static vissprite_t	overflowsprite;

static vissprite_t *R_NewVisSprite (void)
{
	if (vissprite_p == &vissprites[MAXVISSPRITES])
		return &overflowsprite;
	vissprite_p++;
	return vissprite_p - 1;
}


#ifndef RENDER3D
/*
================
=
= R_DrawMaskedColumn
=
= Used for sprites and masked mid textures
================
*/

short		*mfloorclip;
short		*mceilingclip;
fixed_t		spryscale;
fixed_t		sprtopscreen;
fixed_t		sprbotscreen;

void R_DrawMaskedColumn (column_t *column, signed int baseclip)
{
	int		topscreen, bottomscreen;
	fixed_t	basetexturemid;

	basetexturemid = dc_texturemid;

	for ( ; column->topdelta != 0xff ; )
	{
	// calculate unclipped screen coordinates for post
		topscreen = sprtopscreen + spryscale*column->topdelta;
		bottomscreen = topscreen + spryscale*column->length;
		dc_yl = (topscreen + FRACUNIT - 1)>>FRACBITS;
		dc_yh = (bottomscreen - 1)>>FRACBITS;

		if (dc_yh >= mfloorclip[dc_x])
			dc_yh = mfloorclip[dc_x] - 1;
		if (dc_yl <= mceilingclip[dc_x])
			dc_yl = mceilingclip[dc_x] + 1;

		if (dc_yh >= baseclip && baseclip != -1)
			dc_yh = baseclip;

		if (dc_yl <= dc_yh)
		{
			dc_source = (byte *)column + 3;
			dc_texturemid = basetexturemid - (column->topdelta<<FRACBITS);
		//	dc_source = (byte *)column + 3 - column->topdelta;
			colfunc ();		// either R_DrawColumn or R_DrawFuzzColumn
		}
		column = (column_t *)(  (byte *)column + column->length + 4);
	}

	dc_texturemid = basetexturemid;
}


/*
================
=
= R_DrawVisSprite
=
= mfloorclip and mceilingclip should also be set
================
*/

void R_DrawVisSprite (vissprite_t *vis, int x1, int x2)
{
	column_t	*column;
	int		texturecolumn;
	fixed_t		frac;
	patch_t		*patch;
	fixed_t		baseclip;

	patch = (patch_t *) W_CacheLumpNum(vis->patch + firstspritelump, PU_CACHE);

	dc_colormap = vis->colormap;

//	if (!dc_colormap)
//		colfunc = fuzzcolfunc;	// NULL colormap = shadow draw

	if (vis->mobjflags & (MF_SHADOW|MF_ALTSHADOW))
	{
		if (vis->mobjflags & MF_TRANSLATION)
		{
			colfunc = R_DrawTranslatedFuzzColumn;
			dc_translation = translationtables - 256 + vis->playerclass * ((MAXPLAYERS - 1) * 256) +
					 ((vis->mobjflags & MF_TRANSLATION)>>(MF_TRANSSHIFT - 8));
		}
		else if (vis->mobjflags & MF_SHADOW)
		{ // Draw using shadow column function
			colfunc = fuzzcolfunc;
		}
		else
		{
			colfunc = R_DrawAltFuzzColumn;
		}
	}
	else if (vis->mobjflags & MF_TRANSLATION)
	{
		// Draw using translated column function
		colfunc = R_DrawTranslatedColumn;
		dc_translation = translationtables - 256 + vis->playerclass * ((MAXPLAYERS - 1) * 256) +
				 ((vis->mobjflags & MF_TRANSLATION)>>(MF_TRANSSHIFT - 8));
	}

	dc_iscale = abs(vis->xiscale) >> detailshift;
	dc_texturemid = vis->texturemid;
	frac = vis->startfrac;
	spryscale = vis->scale;

	sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);

	// check to see if vissprite is a weapon
	if (vis->psprite)
	{
		dc_texturemid += FixedMul(((centery-viewheight/2)<<FRACBITS), vis->xiscale);
		sprtopscreen += (viewheight/2 - centery)<<FRACBITS;
	}

	if (vis->floorclip && !vis->psprite)
	{
		sprbotscreen = sprtopscreen + FixedMul(SHORT(patch->height)<<FRACBITS, spryscale);
		baseclip = (sprbotscreen - FixedMul(vis->floorclip, spryscale))>>FRACBITS;
	}
	else
	{
		baseclip = -1;
	}

	for (dc_x = vis->x1; dc_x <= vis->x2; dc_x++, frac += vis->xiscale)
	{
		texturecolumn = frac>>FRACBITS;
#ifdef RANGECHECK
		if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
			I_Error ("R_DrawSpriteRange: bad texturecolumn");
#endif
		column = (column_t *) ((byte *)patch + LONG(patch->columnofs[texturecolumn]));
		R_DrawMaskedColumn (column, baseclip);
	}

	colfunc = basecolfunc;
}
#endif	/* RENDER3D */


/*
===================
=
= R_ProjectSprite
=
= Generates a vissprite for a thing if it might be visible
=
===================
*/

void R_ProjectSprite (mobj_t *thing)
{
	fixed_t		xtr, ytr;
	fixed_t		gxt, gyt;
	fixed_t		tz;
	fixed_t		xscale;
	int		x1 = 0, x2 = 0;
	spritedef_t	*sprdef;
	spriteframe_t	*sprframe;
	int		lump;
	unsigned int	rot;
	boolean		flip;
#ifndef RENDER3D
	fixed_t		tx;
	int		idx;
#endif
	vissprite_t	*vis;
	angle_t		ang;
	fixed_t		iscale;
#ifdef RENDER3D
	float		v1[2], v2[2];
	float		sinrv, cosrv, thangle;	// rv = real value
#endif

	if (thing->flags2 & MF2_DONTDRAW)
	{ // Never make a vissprite when MF2_DONTDRAW is flagged.
		return;
	}

//
// transform the origin point
//
	xtr = thing->x - viewx;
	ytr = thing->y - viewy;

	gxt = FixedMul(xtr,viewcos);
	gyt = -FixedMul(ytr,viewsin);
	tz = gxt - gyt;

#ifdef RENDER3D
	if (tz < 0)
		tz = -tz;	// Make it positive. The clipper will handle backside.
	if (tz < FRACUNIT)
		tz = FRACUNIT;
#else
	if (tz < MINZ)
		return;		// thing is behind view plane
#endif
	xscale = FixedDiv(projection, tz);

#ifndef RENDER3D
	gxt = -FixedMul(xtr,viewsin);
	gyt = FixedMul(ytr,viewcos);
	tx = -(gyt + gxt);

	if (abs(tx) > (tz<<2))
		return;		// too far off the side
#endif

//
// decide which patch to use for sprite reletive to player
//
#ifdef RANGECHECK
	if ((unsigned int)thing->sprite >= numsprites)
		I_Error ("R_ProjectSprite: invalid sprite number %i ", thing->sprite);
#endif
	sprdef = &sprites[thing->sprite];
#ifdef RANGECHECK
	if ((thing->frame&FF_FRAMEMASK) >= sprdef->numframes)
		I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ", thing->sprite, thing->frame);
#endif
	sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];

	if (sprframe->rotate)
	{	// choose a different rotation based on player view
		ang = R_PointToAngle (thing->x, thing->y);
		rot = (ang - thing->angle + (unsigned int)(ANG45/2)*9) >> 29;
		lump = sprframe->lump[rot];
		flip = !!(sprframe->flip[rot]);
	}
	else
	{	// use single rotation for all views
		lump = sprframe->lump[0];
		flip = !!(sprframe->flip[0]);
	}

//
// calculate edges of the shape
//
#ifdef RENDER3D
	v1[VX] = FIX2FLT(thing->x);
	v1[VY] = FIX2FLT(thing->y);
//	thangle = BANG2RAD(bamsAtan2((v1[VY]-FIX2FLT(viewy))*10, (v1[VX]-FIX2FLT(viewx))*10)) - PI/2;
	thangle = BANG2RAD(bamsAtan2(FIX2FLT(ytr)*10, FIX2FLT(xtr)*10)) - PI/2;
	sinrv = sin(thangle);
	cosrv = cos(thangle);
	v1[VX] -= cosrv*(spriteoffset[lump]>>FRACBITS);
	v1[VY] -= sinrv*(spriteoffset[lump]>>FRACBITS);
	v2[VX] = v1[VX] + cosrv*(spritewidth[lump]>>FRACBITS);
	v2[VY] = v1[VY] + sinrv*(spritewidth[lump]>>FRACBITS);
 	// Check for visibility.
	if (!C_CheckViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]))
	//if (!C_IsAngleVisible(RAD2BANG(thangle+PI/2)))
		return;		// Isn't visible.
#else
	tx -= spriteoffset[lump];
	x1 = (centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS;
	if (x1 > viewwidth)
		return;		// off the right side
	tx +=  spritewidth[lump];
	x2 = ((centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS) - 1;
	if (x2 < 0)
		return;		// off the left side
#endif

//
// store information in a vissprite
//
	vis = R_NewVisSprite ();
	vis->mobjflags = thing->flags;
	vis->psprite = false;
	vis->scale = xscale<<detailshift;
	vis->gx = thing->x;
	vis->gy = thing->y;
	vis->gz = thing->z;
	vis->gzt = thing->z + spritetopoffset[lump];

#ifdef RENDER3D
	vis->secfloor = FIX2FLT(thing->subsector->sector->floorheight);
	vis->secceil  = FIX2FLT(thing->subsector->sector->ceilingheight);
#endif

	if (thing->flags & MF_TRANSLATION)
	{
		if (thing->player)
		{
			vis->playerclass = thing->player->playerclass;
		}
		else
		{
			vis->playerclass = thing->special1;
		}
		if (vis->playerclass > 2)
		{
		// O.S. --  FIXME: HARDCODED NUMBER: 2 == PCLASS_MAGE
			vis->playerclass = 0;
		}
	}
	// foot clipping
	vis->floorclip = thing->floorclip;
	vis->texturemid = vis->gzt - viewz - vis->floorclip;

#ifdef RENDER3D
	// The start and end vertices.
	vis->v1[VX] = v1[VX];
	vis->v1[VY] = v1[VY];
	vis->v2[VX] = v2[VX];
	vis->v2[VY] = v2[VY];
#endif

	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
	iscale = FixedDiv (FRACUNIT, xscale);
	if (flip)
	{
		vis->startfrac = spritewidth[lump]-1;
		vis->xiscale = -iscale;
	}
	else
	{
		vis->startfrac = 0;
		vis->xiscale = iscale;
	}
	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1 - x1);
	vis->patch = lump;
//
// get light level
//

//	if (thing->flags & MF_SHADOW)
//		vis->colormap = NULL;			// shadow draw
//	else ...

#ifdef RENDER3D
	if (thing->frame & FF_FULLBRIGHT)
	{	// full bright
		vis->lightlevel = -1;
	}
	else
	{	// diminished light
		vis->lightlevel = thing->subsector->sector->lightlevel;
	}
#else
	if (fixedcolormap)
		vis->colormap = fixedcolormap;	// fixed map
	else if (LevelUseFullBright && thing->frame & FF_FULLBRIGHT)
		vis->colormap = colormaps;	// full bright
	else
	{	// diminished light
		idx = xscale>>(LIGHTSCALESHIFT - detailshift);
		if (idx >= MAXLIGHTSCALE)
			idx = MAXLIGHTSCALE-1;
		vis->colormap = spritelights[idx];
	}
#endif
}


/*
========================
=
= R_AddSprites
=
========================
*/

void R_AddSprites (sector_t *sec)
{
	if (sec->validcount == validcount)
	{
		return;		// already added
	}
	else
	{
		mobj_t		*thing;
#ifndef RENDER3D
		int		lightnum;

		lightnum = (sec->lightlevel >> LIGHTSEGSHIFT) + extralight;
		if (lightnum < 0)
			spritelights = scalelight[0];
		else if (lightnum >= LIGHTLEVELS)
			spritelights = scalelight[LIGHTLEVELS-1];
		else
			spritelights = scalelight[lightnum];
#endif

		sec->validcount = validcount;

		for (thing = sec->thinglist ; thing ; thing = thing->snext)
			R_ProjectSprite (thing);
	}
}


/*
========================
=
= R_DrawPSprite
=
========================
*/

/* Y-adjustment values for full screen (4 weapons) */
static int PSpriteSY[NUMCLASSES][NUMWEAPONS] =
{
	{		0, -12 * FRACUNIT, -10 * FRACUNIT, 10 * FRACUNIT },	/* Fighter	*/
	{   -8 * FRACUNIT,  10 * FRACUNIT,  10 * FRACUNIT,  0		 },	/* Cleric	*/
	{    9 * FRACUNIT,  20 * FRACUNIT,  20 * FRACUNIT, 20 * FRACUNIT },	/* Mage		*/
	{   10 * FRACUNIT,  10 * FRACUNIT,  10 * FRACUNIT, 10 * FRACUNIT }	/* Pig		*/
};

void R_DrawPSprite (pspdef_t *psp)
{
	fixed_t		tx;
	int		x1;
#ifndef RENDER3D
	int		x2;
#endif
	spritedef_t	*sprdef;
	spriteframe_t	*sprframe;
	int		lump;
	boolean		flip;
#ifdef RENDER3D
	float		light, alpha;
	int		y;
#else
	vissprite_t	*vis, avis;
#endif
	int		tempangle;

//
// decide which patch to use
//
#ifdef RANGECHECK
	if ( (unsigned int)psp->state->sprite >= numsprites)
		I_Error ("R_ProjectSprite: invalid sprite number %i ", psp->state->sprite);
#endif
	sprdef = &sprites[psp->state->sprite];
#ifdef RANGECHECK
	if ( (psp->state->frame & FF_FRAMEMASK)  >= sprdef->numframes)
		I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ", psp->state->sprite, psp->state->frame);
#endif
	sprframe = &sprdef->spriteframes[psp->state->frame & FF_FRAMEMASK];

	lump = sprframe->lump[0];
	flip = !!(sprframe->flip[0]);

//
// calculate edges of the shape
//
	tx = psp->sx - 160*FRACUNIT;

	tx -= spriteoffset[lump];
	if (viewangleoffset)
	{
		tempangle = ((centerxfrac/1024)*(viewangleoffset>>ANGLETOFINESHIFT));
	}
	else
	{
		tempangle = 0;
	}
	x1 = (centerxfrac + FixedMul (tx,pspritescale)+tempangle ) >>FRACBITS;

#ifdef RENDER3D
	// Set the OpenGL color & alpha.
	light = 1;
	alpha = 1;
	if (viewplayer->powers[pw_invulnerability] &&
		viewplayer->playerclass == PCLASS_CLERIC)
	{
		if (viewplayer->powers[pw_invulnerability] > 4*32)
		{
			if (viewplayer->mo->flags2 & MF2_DONTDRAW)
			{ // don't draw the psprite
				alpha = .333f;
			}
			else if (viewplayer->mo->flags & MF_SHADOW)
			{
				alpha = .666f;
			}
		}
		else if (viewplayer->powers[pw_invulnerability] & 8)
		{
			alpha = .333f;
		}
	}
	else if (fixedcolormap)
	{
		// Fixed color
		light = 1;
	}
	else if (psp->state->frame & FF_FULLBRIGHT)
	{
		// Full bright
		light = 1;
	}
	else
	{
		// local light
		light = viewplayer->mo->subsector->sector->lightlevel / 255.0;
	}

//
// do some OpenGL rendering, oh yeah
//
	y = -(spritetopoffset[lump]>>FRACBITS) + (psp->sy>>FRACBITS);
	if (viewheight == SCREENHEIGHT)
	{
		y += PSpriteSY[viewplayer->playerclass][players[consoleplayer].readyweapon] >> FRACBITS;
	}
	else
		y -= 39/2;
 
	light += .1f;	// Add some extra light.
	OGL_SetColorAndAlpha(light, light, light, alpha);
	OGL_DrawPSprite(x1, y, 1, flip, lump);

#else

	if (x1 > viewwidth)
		return;		// off the right side
	tx +=  spritewidth[lump];
	x2 = ((centerxfrac + FixedMul(tx, pspritescale) + tempangle) >>FRACBITS) - 1;
	if (x2 < 0)
		return;		// off the left side

//
// store information in a vissprite
//
	vis = &avis;
	vis->mobjflags = 0;
	vis->playerclass = 0;
	vis->psprite = true;
	vis->texturemid = (BASEYCENTER<<FRACBITS)+FRACUNIT/2 - (psp->sy-spritetopoffset[lump]);
	if (viewheight == SCREENHEIGHT)
	{
		vis->texturemid -= PSpriteSY[viewplayer->playerclass][players[consoleplayer].readyweapon];
	}
	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
	vis->scale = pspritescale<<detailshift;
	if (flip)
	{
		vis->xiscale = -pspriteiscale;
		vis->startfrac = spritewidth[lump]-1;
	}
	else
	{
		vis->xiscale = pspriteiscale;
		vis->startfrac = 0;
	}
	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1 - x1);
	vis->patch = lump;

	if (viewplayer->powers[pw_invulnerability] &&
		viewplayer->playerclass == PCLASS_CLERIC)
	{
		vis->colormap = spritelights[MAXLIGHTSCALE-1];
		if (viewplayer->powers[pw_invulnerability] > 4*32)
		{
			if (viewplayer->mo->flags2 & MF2_DONTDRAW)
			{ // don't draw the psprite
				vis->mobjflags |= MF_SHADOW;
			}
			else if (viewplayer->mo->flags & MF_SHADOW)
			{
				vis->mobjflags |= MF_ALTSHADOW;
			}
		}
		else if (viewplayer->powers[pw_invulnerability] & 8)
		{
			vis->mobjflags |= MF_SHADOW;
		}
	}
	else if (fixedcolormap)
	{
		// Fixed color
		vis->colormap = fixedcolormap;
	}
	else if (psp->state->frame & FF_FULLBRIGHT)
	{
		// Full bright
		vis->colormap = colormaps;
	}
	else
	{
		// local light
		vis->colormap = spritelights[MAXLIGHTSCALE-1];
	}
	R_DrawVisSprite(vis, vis->x1, vis->x2);
#endif
}

/*
========================
=
= R_DrawPlayerSprites
=
========================
*/

void R_DrawPlayerSprites (void)
{
	int		i;
	pspdef_t	*psp;
#ifndef RENDER3D
	int		lightnum;

//
// get light level
//
	lightnum = (viewplayer->mo->subsector->sector->lightlevel >> LIGHTSEGSHIFT) + extralight;
	if (lightnum < 0)
		spritelights = scalelight[0];
	else if (lightnum >= LIGHTLEVELS)
		spritelights = scalelight[LIGHTLEVELS-1];
	else
		spritelights = scalelight[lightnum];
//
// clip to screen bounds
//
	mfloorclip = screenheightarray;
	mceilingclip = negonearray;
#endif

//
// add all active psprites
//
	for (i = 0, psp = viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
	{
		if (psp->state)
			R_DrawPSprite (psp);
	}
}


/*
========================
=
= R_SortVisSprites
=
========================
*/

vissprite_t	vsprsortedhead;

void R_SortVisSprites (void)
{
	int		i, count;
	vissprite_t	*ds, *best;
	vissprite_t	unsorted;
	fixed_t		bestscale;

	count = vissprite_p - vissprites;

	unsorted.next = unsorted.prev = &unsorted;
	if (!count)
		return;

	for (ds = vissprites; ds < vissprite_p; ds++)
	{
		ds->next = ds + 1;
		ds->prev = ds - 1;
	}
	vissprites[0].prev = &unsorted;
	unsorted.next = &vissprites[0];
	(vissprite_p - 1)->next = &unsorted;
	unsorted.prev = vissprite_p - 1;

//
// pull the vissprites out by scale
//
	best = 0;		// shut up the compiler warning
	vsprsortedhead.next = vsprsortedhead.prev = &vsprsortedhead;
	for (i = 0; i < count; i++)
	{
		bestscale = H2MAXINT;
		for (ds = unsorted.next; ds != &unsorted; ds = ds->next)
		{
			if (ds->scale < bestscale)
			{
				bestscale = ds->scale;
				best = ds;
			}
		}
		best->next->prev = best->prev;
		best->prev->next = best->next;
		best->next = &vsprsortedhead;
		best->prev = vsprsortedhead.prev;
		vsprsortedhead.prev->next = best;
		vsprsortedhead.prev = best;
	}
}


#ifndef RENDER3D
/*
========================
=
= R_DrawSprite
=
========================
*/

void R_DrawSprite (vissprite_t *spr)
{
	drawseg_t	*ds;
	short		clipbot[SCREENWIDTH], cliptop[SCREENWIDTH];
	int		x, r1, r2;
	fixed_t		scale, lowscale;
	int		silhouette;

	for (x = spr->x1; x <= spr->x2; x++)
		clipbot[x] = cliptop[x] = -2;

//
// scan drawsegs from end to start for obscuring segs
// the first drawseg that has a greater scale is the clip seg
//
	for (ds = ds_p - 1; ds >= drawsegs; ds--)
	{
		//
		// determine if the drawseg obscures the sprite
		//
		if (ds->x1 > spr->x2 || ds->x2 < spr->x1 ||
				(!ds->silhouette && !ds->maskedtexturecol))
			continue;			// doesn't cover sprite

		r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
		r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;
		if (ds->scale1 > ds->scale2)
		{
			lowscale = ds->scale2;
			scale = ds->scale1;
		}
		else
		{
			lowscale = ds->scale1;
			scale = ds->scale2;
		}

		if (scale < spr->scale || ( lowscale < spr->scale
				&& !R_PointOnSegSide(spr->gx, spr->gy, ds->curline) ) )
		{
			if (ds->maskedtexturecol)	// masked mid texture
				R_RenderMaskedSegRange (ds, r1, r2);
			continue;			// seg is behind sprite
		}

//
// clip this piece of the sprite
//
		silhouette = ds->silhouette;
		if (spr->gz >= ds->bsilheight)
			silhouette &= ~SIL_BOTTOM;
		if (spr->gzt <= ds->tsilheight)
			silhouette &= ~SIL_TOP;

		if (silhouette == 1)
		{	// bottom sil
			for (x = r1; x <= r2; x++)
			{
				if (clipbot[x] == -2)
					clipbot[x] = ds->sprbottomclip[x];
			}
		}
		else if (silhouette == 2)
		{	// top sil
			for (x = r1; x <= r2; x++)
			{
				if (cliptop[x] == -2)
					cliptop[x] = ds->sprtopclip[x];
			}
		}
		else if (silhouette == 3)
		{	// both
			for (x = r1; x <= r2; x++)
			{
				if (clipbot[x] == -2)
					clipbot[x] = ds->sprbottomclip[x];
				if (cliptop[x] == -2)
					cliptop[x] = ds->sprtopclip[x];
			}
		}
	}

//
// all clipping has been performed, so draw the sprite
//

// check for unclipped columns
	for (x = spr->x1; x <= spr->x2; x++)
	{
		if (clipbot[x] == -2)
			clipbot[x] = viewheight;
		if (cliptop[x] == -2)
			cliptop[x] = -1;
	}

	mfloorclip = clipbot;
	mceilingclip = cliptop;
	R_DrawVisSprite (spr, spr->x1, spr->x2);
}
#endif	/* !RENDER3D */


/*
========================
=
= R_DrawMasked
=
========================
*/

void R_DrawMasked (void)
{
	vissprite_t		*spr;

#ifdef RENDER3D
	extern boolean willRenderSprites;

	if (!willRenderSprites)
		return;

	R_SortVisSprites();

	if (vissprite_p > vissprites)
	{
	// draw all vissprites back to front
		glDepthMask(GL_FALSE);

		for (spr = vsprsortedhead.next; spr != &vsprsortedhead;
							spr = spr->next)
		{
			R_RenderSprite(spr);
		}

		glDepthMask(GL_TRUE);
	}
#else
	drawseg_t		*ds;

	R_SortVisSprites ();

	if (vissprite_p > vissprites)
	{
	// draw all vissprites back to front
		for (spr = vsprsortedhead.next; spr != &vsprsortedhead;
							spr = spr->next)
		{
			R_DrawSprite (spr);
		}
	}

//
// render any remaining masked mid textures
//
	for (ds = ds_p - 1; ds >= drawsegs; ds--)
	{
		if (ds->maskedtexturecol)
			R_RenderMaskedSegRange (ds, ds->x1, ds->x2);
	}

//
// draw the psprites on top of everything
//
// Added for the sideviewing with an external device
	if (viewangleoffset <= 1024<<ANGLETOFINESHIFT ||
		viewangleoffset >= -1024<<ANGLETOFINESHIFT)
	{
	// don't draw on side views
		R_DrawPlayerSprites ();
	}

//	if (!viewangleoffset)		// don't draw on side views
//		R_DrawPlayerSprites ();
#endif
}

