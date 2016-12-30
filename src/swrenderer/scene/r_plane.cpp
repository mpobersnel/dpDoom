// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Here is a core component: drawing the floors and ceilings,
//		 while maintaining a per column clipping list only.
//		Moreover, the sky areas have to be determined.
//
// MAXVISPLANES is no longer a limit on the number of visplanes,
// but a limit on the number of hash slots; larger numbers mean
// better performance usually but after a point they are wasted,
// and memory and time overheads creep in.
//
// Lee Killough
//
// [RH] Further modified to significantly increase accuracy and add slopes.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <float.h>

#include "templates.h"
#include "i_system.h"
#include "w_wad.h"

#include "doomdef.h"
#include "doomstat.h"

#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_things.h"
#include "r_sky.h"
#include "stats.h"

#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_3dfloors.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "gl/dynlights/gl_dynlight.h"
#include "r_walldraw.h"
#include "r_clip_segment.h"
#include "r_draw_segment.h"
#include "r_portal.h"
#include "swrenderer/r_memory.h"

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

CVAR(Bool, r_linearsky, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Bool, tilt, false, 0);

EXTERN_CVAR(Int, r_skymode)

namespace swrenderer
{
	using namespace drawerargs;

extern int wallshade;

extern subsector_t *InSubsector;

static void R_DrawSkyStriped (visplane_t *pl);

planefunction_t 		floorfunc;
planefunction_t 		ceilingfunc;

visplane_t 				*floorplane;
visplane_t 				*ceilingplane;

// These are copies of the main parameters used when drawing stacked sectors.
// When you change the main parameters, you should copy them here too *unless*
// you are changing them to draw a stacked sector. Otherwise, stacked sectors
// won't draw in skyboxes properly.
int stacked_extralight;
double stacked_visibility;
DVector3 stacked_viewpos;
DAngle stacked_angle;

//
// Clip values are the solid pixel bounding the range.
//	floorclip starts out SCREENHEIGHT and is just outside the range
//	ceilingclip starts out 0 and is just inside the range
//
short					floorclip[MAXWIDTH];
short					ceilingclip[MAXWIDTH];

//
// texture mapping
//

static double			planeheight;

extern "C" {
//
// spanend holds the end of a plane span in each screen row
//
short					spanend[MAXHEIGHT];

int						planeshade;
FVector3				plane_sz, plane_su, plane_sv;
float					planelightfloat;
bool					plane_shade;
fixed_t					pviewx, pviewy;
}

float 					yslope[MAXHEIGHT];
static fixed_t			xscale, yscale;
static double			xstepscale, ystepscale;
static double			basexfrac, baseyfrac;

void					R_DrawSinglePlane (visplane_t *, fixed_t alpha, bool additive, bool masked);

//==========================================================================
//
// R_MapPlane
//
// Globals used: planeheight, ds_source, basexscale, baseyscale,
// pviewx, pviewy, xoffs, yoffs, basecolormap, xscale, yscale.
//
//==========================================================================

void R_MapPlane (int y, int x1)
{
	int x2 = spanend[y];
	double distance;

#ifdef RANGECHECK
	if (x2 < x1 || x1<0 || x2>=viewwidth || (unsigned)y>=(unsigned)viewheight)
	{
		I_FatalError ("R_MapPlane: %i, %i at %i", x1, x2, y);
	}
#endif

	// [RH] Notice that I dumped the caching scheme used by Doom.
	// It did not offer any appreciable speedup.

	distance = planeheight * yslope[y];

	if (ds_xbits != 0)
	{
		ds_xstep = xs_ToFixed(32 - ds_xbits, distance * xstepscale);
		ds_xfrac = xs_ToFixed(32 - ds_xbits, distance * basexfrac) + pviewx;
	}
	else
	{
		ds_xstep = 0;
		ds_xfrac = 0;
	}
	if (ds_ybits != 0)
	{
		ds_ystep = xs_ToFixed(32 - ds_ybits, distance * ystepscale);
		ds_yfrac = xs_ToFixed(32 - ds_ybits, distance * baseyfrac) + pviewy;
	}
	else
	{
		ds_ystep = 0;
		ds_yfrac = 0;
	}

	if (r_swtruecolor)
	{
		double distance2 = planeheight * yslope[(y + 1 < viewheight) ? y + 1 : y - 1];
		double xmagnitude = fabs(ystepscale * (distance2 - distance) * FocalLengthX);
		double ymagnitude = fabs(xstepscale * (distance2 - distance) * FocalLengthX);
		double magnitude = MAX(ymagnitude, xmagnitude);
		double min_lod = -1000.0;
		ds_lod = MAX(log2(magnitude) + r_lod_bias, min_lod);
	}

	if (plane_shade)
	{
		// Determine lighting based on the span's distance from the viewer.
		R_SetDSColorMapLight(basecolormap, GlobVis * fabs(CenterY - y), planeshade);
	}

	if (r_dynlights)
	{
		// Find row position in view space
		float zspan = planeheight / (fabs(y + 0.5 - CenterY) / InvZtoScale);
		dc_viewpos.X = (float)((x1 + 0.5 - CenterX) / CenterX * zspan);
		dc_viewpos.Y = zspan;
		dc_viewpos.Z = (float)((CenterY - y - 0.5) / InvZtoScale * zspan);
		dc_viewpos_step.X = (float)(zspan / CenterX);

		static TriLight lightbuffer[64 * 1024];
		static int nextlightindex = 0;

		// Setup lights for column
		dc_num_lights = 0;
		dc_lights = lightbuffer + nextlightindex;
		visplane_light *cur_node = ds_light_list;
		while (cur_node && nextlightindex < 64 * 1024)
		{
			double lightX = cur_node->lightsource->X() - ViewPos.X;
			double lightY = cur_node->lightsource->Y() - ViewPos.Y;
			double lightZ = cur_node->lightsource->Z() - ViewPos.Z;

			float lx = (float)(lightX * ViewSin - lightY * ViewCos);
			float ly = (float)(lightX * ViewTanCos + lightY * ViewTanSin) - dc_viewpos.Y;
			float lz = (float)lightZ - dc_viewpos.Z;

			// Precalculate the constant part of the dot here so the drawer doesn't have to.
			float lconstant = ly * ly + lz * lz;

			// Include light only if it touches this row
			float radius = cur_node->lightsource->GetRadius();
			if (radius * radius >= lconstant)
			{
				uint32_t red = cur_node->lightsource->GetRed();
				uint32_t green = cur_node->lightsource->GetGreen();
				uint32_t blue = cur_node->lightsource->GetBlue();

				nextlightindex++;
				auto &light = dc_lights[dc_num_lights++];
				light.x = lx;
				light.y = lconstant;
				light.radius = 256.0f / radius;
				light.color = (red << 16) | (green << 8) | blue;
			}

			cur_node = cur_node->next;
		}

		if (nextlightindex == 64 * 1024)
			nextlightindex = 0;
	}
	else
	{
		dc_num_lights = 0;
	}

	ds_y = y;
	ds_x1 = x1;
	ds_x2 = x2;

	(R_Drawers()->*spanfunc)();
}

//==========================================================================
//
// R_MapTiltedPlane
//
//==========================================================================

void R_MapTiltedPlane (int y, int x1)
{
	R_Drawers()->DrawTiltedSpan(y, x1, spanend[y], plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy);
}

//==========================================================================
//
// R_MapColoredPlane
//
//==========================================================================

void R_MapColoredPlane(int y, int x1)
{
	R_Drawers()->DrawColoredSpan(y, x1, spanend[y]);
}

void R_DrawFogBoundarySection(int y, int y2, int x1)
{
	for (; y < y2; ++y)
	{
		R_Drawers()->DrawFogBoundaryLine(y, x1, spanend[y]);
	}
}

//==========================================================================

void R_DrawFogBoundary(int x1, int x2, short *uclip, short *dclip)
{
	// This is essentially the same as R_MapVisPlane but with an extra step
	// to create new horizontal spans whenever the light changes enough that
	// we need to use a new colormap.

	double lightstep = rw_lightstep;
	double light = rw_light + rw_lightstep*(x2 - x1 - 1);
	int x = x2 - 1;
	int t2 = uclip[x];
	int b2 = dclip[x];
	int rcolormap = GETPALOOKUP(light, wallshade);
	int lcolormap;
	uint8_t *basecolormapdata = basecolormap->Maps;

	if (b2 > t2)
	{
		fillshort(spanend + t2, b2 - t2, x);
	}

	R_SetColorMapLight(basecolormap, (float)light, wallshade);

	uint8_t *fake_dc_colormap = basecolormap->Maps + (GETPALOOKUP(light, wallshade) << COLORMAPSHIFT);

	for (--x; x >= x1; --x)
	{
		int t1 = uclip[x];
		int b1 = dclip[x];
		const int xr = x + 1;
		int stop;

		light -= rw_lightstep;
		lcolormap = GETPALOOKUP(light, wallshade);
		if (lcolormap != rcolormap)
		{
			if (t2 < b2 && rcolormap != 0)
			{ // Colormap 0 is always the identity map, so rendering it is
			  // just a waste of time.
				R_DrawFogBoundarySection(t2, b2, xr);
			}
			if (t1 < t2) t2 = t1;
			if (b1 > b2) b2 = b1;
			if (t2 < b2)
			{
				fillshort(spanend + t2, b2 - t2, x);
			}
			rcolormap = lcolormap;
			R_SetColorMapLight(basecolormap, (float)light, wallshade);
			fake_dc_colormap = basecolormap->Maps + (GETPALOOKUP(light, wallshade) << COLORMAPSHIFT);
		}
		else
		{
			if (fake_dc_colormap != basecolormapdata)
			{
				stop = MIN(t1, b2);
				while (t2 < stop)
				{
					int y = t2++;
					R_Drawers()->DrawFogBoundaryLine(y, xr, spanend[y]);
				}
				stop = MAX(b1, t2);
				while (b2 > stop)
				{
					int y = --b2;
					R_Drawers()->DrawFogBoundaryLine(y, xr, spanend[y]);
				}
			}
			else
			{
				t2 = MAX(t2, MIN(t1, b2));
				b2 = MIN(b2, MAX(b1, t2));
			}

			stop = MIN(t2, b1);
			while (t1 < stop)
			{
				spanend[t1++] = x;
			}
			stop = MAX(b2, t2);
			while (b1 > stop)
			{
				spanend[--b1] = x;
			}
		}

		t2 = uclip[x];
		b2 = dclip[x];
	}
	if (t2 < b2 && rcolormap != 0)
	{
		R_DrawFogBoundarySection(t2, b2, x1);
	}
}

//==========================================================================

namespace
{
	enum { max_plane_lights = 32 * 1024 };
	visplane_light plane_lights[max_plane_lights];
	int next_plane_light = 0;
}

void R_AddPlaneLights(visplane_t *plane, FLightNode *node)
{
	if (!r_dynlights)
		return;

	while (node)
	{
		if (!(node->lightsource->flags2&MF2_DORMANT))
		{
			bool found = false;
			visplane_light *light_node = plane->lights;
			while (light_node)
			{
				if (light_node->lightsource == node->lightsource)
				{
					found = true;
					break;
				}
				light_node = light_node->next;
			}
			if (!found)
			{
				if (next_plane_light == max_plane_lights)
					return;

				visplane_light *newlight = &plane_lights[next_plane_light++];
				newlight->next = plane->lights;
				newlight->lightsource = node->lightsource;
				plane->lights = newlight;
			}
		}
		node = node->nextLight;
	}
}

//==========================================================================
//
// R_ClearPlanes
//
// Called at the beginning of each frame.
//
//==========================================================================

void R_ClearPlanes (bool fullclear)
{
	int i;

	// Don't clear fake planes if not doing a full clear.
	if (!fullclear)
	{
		for (i = 0; i <= MAXVISPLANES-1; i++)	// new code -- killough
		{
			for (visplane_t **probe = &visplanes[i]; *probe != NULL; )
			{
				if ((*probe)->sky < 0)
				{ // fake: move past it
					probe = &(*probe)->next;
				}
				else
				{ // not fake: move to freelist
					visplane_t *vis = *probe;
					*freehead = vis;
					*probe = vis->next;
					vis->next = NULL;
					freehead = &vis->next;
				}
			}
		}
	}
	else
	{
		for (i = 0; i <= MAXVISPLANES; i++)	// new code -- killough
		{
			for (*freehead = visplanes[i], visplanes[i] = NULL; *freehead; )
			{
				freehead = &(*freehead)->next;
			}
		}

		// opening / clipping determination
		fillshort (floorclip, viewwidth, viewheight);
		// [RH] clip ceiling to console bottom
		fillshort (ceilingclip, viewwidth,
			!screen->Accel2D && ConBottom > viewwindowy && !bRenderingToCanvas
			? (ConBottom - viewwindowy) : 0);

		R_FreeOpenings();

		next_plane_light = 0;
	}
}

//==========================================================================
//
// R_FindPlane
//
// killough 2/28/98: Add offsets
//==========================================================================

visplane_t *R_FindPlane (const secplane_t &height, FTextureID picnum, int lightlevel, double Alpha, bool additive,
						const FTransform &xxform,
						 int sky, FSectorPortal *portal)
{
	secplane_t plane;
	visplane_t *check;
	unsigned hash;						// killough
	bool isskybox;
	const FTransform *xform = &xxform;
	fixed_t alpha = FLOAT2FIXED(Alpha);
	//angle_t angle = (xform.Angle + xform.baseAngle).BAMs();

	FTransform nulltransform;

	if (picnum == skyflatnum)	// killough 10/98
	{ // most skies map together
		lightlevel = 0;
		xform = &nulltransform;
		nulltransform.xOffs = nulltransform.yOffs = nulltransform.baseyOffs = 0;
		nulltransform.xScale = nulltransform.yScale = 1;
		nulltransform.Angle = nulltransform.baseAngle = 0.0;
		additive = false;
		// [RH] Map floor skies and ceiling skies to separate visplanes. This isn't
		// always necessary, but it is needed if a floor and ceiling sky are in the
		// same column but separated by a wall. If they both try to reside in the
		// same visplane, then only the floor sky will be drawn.
		plane.set(0., 0., height.fC(), 0.);
		isskybox = portal != NULL && !(portal->mFlags & PORTSF_INSKYBOX);
	}
	else if (portal != NULL && !(portal->mFlags & PORTSF_INSKYBOX))
	{
		plane = height;
		isskybox = true;
	}
	else
	{
		plane = height;
		isskybox = false;
		// kg3D - hack, store alpha in sky
		// i know there is ->alpha, but this also allows to identify fake plane
		// and ->alpha is for stacked sectors
		if (fake3D & (FAKE3D_FAKEFLOOR|FAKE3D_FAKECEILING)) sky = 0x80000000 | fakeAlpha;
		else sky = 0;	// not skyflatnum so it can't be a sky
		portal = NULL;
		alpha = OPAQUE;
	}

	// New visplane algorithm uses hash table -- killough
	hash = isskybox ? MAXVISPLANES : visplane_hash (picnum.GetIndex(), lightlevel, height);

	for (check = visplanes[hash]; check; check = check->next)	// killough
	{
		if (isskybox)
		{
			if (portal == check->portal && plane == check->height)
			{
				if (portal->mType != PORTS_SKYVIEWPOINT)
				{ // This skybox is really a stacked sector, so we need to
				  // check even more.
					if (check->extralight == stacked_extralight &&
						check->visibility == stacked_visibility &&
						check->viewpos == stacked_viewpos &&
						(
							// headache inducing logic... :(
							(portal->mType != PORTS_STACKEDSECTORTHING) ||
							(
								check->Alpha == alpha &&
								check->Additive == additive &&
								(alpha == 0 ||	// if alpha is > 0 everything needs to be checked
									(plane == check->height &&
									 picnum == check->picnum &&
									 lightlevel == check->lightlevel &&
									 basecolormap == check->colormap &&	// [RH] Add more checks
									 *xform == check->xform
									)
								) &&
								check->viewangle == stacked_angle
							)
						)
					   )
					{
						return check;
					}
				}
				else
				{
					return check;
				}
			}
		}
		else
		if (plane == check->height &&
			picnum == check->picnum &&
			lightlevel == check->lightlevel &&
			basecolormap == check->colormap &&	// [RH] Add more checks
			*xform == check->xform &&
			sky == check->sky &&
			CurrentPortalUniq == check->CurrentPortalUniq &&
			MirrorFlags == check->MirrorFlags &&
			CurrentSkybox == check->CurrentSkybox &&
			ViewPos == check->viewpos
			)
		{
		  return check;
		}
	}

	check = new_visplane (hash);		// killough

	check->height = plane;
	check->picnum = picnum;
	check->lightlevel = lightlevel;
	check->xform = *xform;
	check->colormap = basecolormap;		// [RH] Save colormap
	check->sky = sky;
	check->portal = portal;
	check->left = viewwidth;			// Was SCREENWIDTH -- killough 11/98
	check->right = 0;
	check->extralight = stacked_extralight;
	check->visibility = stacked_visibility;
	check->viewpos = stacked_viewpos;
	check->viewangle = stacked_angle;
	check->Alpha = alpha;
	check->Additive = additive;
	check->CurrentPortalUniq = CurrentPortalUniq;
	check->MirrorFlags = MirrorFlags;
	check->CurrentSkybox = CurrentSkybox;

	fillshort (check->top, viewwidth, 0x7fff);

	return check;
}

//==========================================================================
//
// R_CheckPlane
//
//==========================================================================

visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop)
{
	int intrl, intrh;
	int unionl, unionh;
	int x;

	assert (start >= 0 && start < viewwidth);
	assert (stop > start && stop <= viewwidth);

	if (start < pl->left)
	{
		intrl = pl->left;
		unionl = start;
	}
	else
	{
		unionl = pl->left;
		intrl = start;
	}
		
	if (stop > pl->right)
	{
		intrh = pl->right;
		unionh = stop;
	}
	else
	{
		unionh = pl->right;
		intrh = stop;
	}

	for (x = intrl; x < intrh && pl->top[x] == 0x7fff; x++)
		;

	if (x >= intrh)
	{
		// use the same visplane
		pl->left = unionl;
		pl->right = unionh;
	}
	else
	{
		// make a new visplane
		unsigned hash;

		if (pl->portal != NULL && !(pl->portal->mFlags & PORTSF_INSKYBOX) && viewactive)
		{
			hash = MAXVISPLANES;
		}
		else
		{
			hash = visplane_hash (pl->picnum.GetIndex(), pl->lightlevel, pl->height);
		}
		visplane_t *new_pl = new_visplane (hash);

		new_pl->height = pl->height;
		new_pl->picnum = pl->picnum;
		new_pl->lightlevel = pl->lightlevel;
		new_pl->xform = pl->xform;
		new_pl->colormap = pl->colormap;
		new_pl->portal = pl->portal;
		new_pl->extralight = pl->extralight;
		new_pl->visibility = pl->visibility;
		new_pl->viewpos = pl->viewpos;
		new_pl->viewangle = pl->viewangle;
		new_pl->sky = pl->sky;
		new_pl->Alpha = pl->Alpha;
		new_pl->Additive = pl->Additive;
		new_pl->CurrentPortalUniq = pl->CurrentPortalUniq;
		new_pl->MirrorFlags = pl->MirrorFlags;
		new_pl->CurrentSkybox = pl->CurrentSkybox;
		new_pl->lights = pl->lights;
		pl = new_pl;
		pl->left = start;
		pl->right = stop;
		fillshort (pl->top, viewwidth, 0x7fff);
	}
	return pl;
}


//==========================================================================
//
// R_MakeSpans
//
//
//==========================================================================

inline void R_MakeSpans (int x, int t1, int b1, int t2, int b2, void (*mapfunc)(int y, int x1))
{
}

//==========================================================================
//
// R_DrawSky
//
// Can handle overlapped skies. Note that the front sky is *not* masked in
// in the normal convention for patches, but uses color 0 as a transparent
// color instead.
//
// Note that since ZDoom now uses color 0 as transparent for other purposes,
// you can use normal texture transparency, so the distinction isn't so
// important anymore, but you should still be aware of it.
//
//==========================================================================

static FTexture *frontskytex, *backskytex;
static angle_t skyflip;
static int frontpos, backpos;
static double frontyScale;
static fixed_t frontcyl, backcyl;
static double skymid;
static angle_t skyangle;
static double frontiScale;

extern float swall[MAXWIDTH];
extern fixed_t lwall[MAXWIDTH];
extern fixed_t rw_offset;
extern FTexture *rw_pic;

// Allow for layer skies up to 512 pixels tall. This is overkill,
// since the most anyone can ever see of the sky is 500 pixels.
// We need 4 skybufs because R_DrawSkySegment can draw up to 4 columns at a time.
// Need two versions - one for true color and one for palette
#define MAXSKYBUF 3072
static BYTE skybuf[4][512];
static uint32_t skybuf_bgra[MAXSKYBUF][512];
static DWORD lastskycol[4];
static DWORD lastskycol_bgra[MAXSKYBUF];
static int skycolplace;
static int skycolplace_bgra;


// Get a column of sky when there is only one sky texture.
static const BYTE *R_GetOneSkyColumn (FTexture *fronttex, int x)
{
	int tx;
	if (r_linearsky)
	{
		angle_t xangle = (angle_t)((0.5 - x / (double)viewwidth) * FocalTangent * ANGLE_90);
		angle_t column = (skyangle + xangle) ^ skyflip;
		tx = (UMulScale16(column, frontcyl) + frontpos) >> FRACBITS;
	}
	else
	{
		angle_t column = (skyangle + xtoviewangle[x]) ^ skyflip;
		tx = (UMulScale16(column, frontcyl) + frontpos) >> FRACBITS;
	}

	if (!r_swtruecolor)
		return fronttex->GetColumn(tx, NULL);
	else
	{
		return (const BYTE *)fronttex->GetColumnBgra(tx, NULL);
	}
}

// Get a column of sky when there are two overlapping sky textures
static const BYTE *R_GetTwoSkyColumns (FTexture *fronttex, int x)
{
	DWORD ang, angle1, angle2;

	if (r_linearsky)
	{
		angle_t xangle = (angle_t)((0.5 - x / (double)viewwidth) * FocalTangent * ANGLE_90);
		ang = (skyangle + xangle) ^ skyflip;
	}
	else
	{
		ang = (skyangle + xtoviewangle[x]) ^ skyflip;
	}
	angle1 = (DWORD)((UMulScale16(ang, frontcyl) + frontpos) >> FRACBITS);
	angle2 = (DWORD)((UMulScale16(ang, backcyl) + backpos) >> FRACBITS);

	// Check if this column has already been built. If so, there's
	// no reason to waste time building it again.
	DWORD skycol = (angle1 << 16) | angle2;
	int i;

	if (!r_swtruecolor)
	{
		for (i = 0; i < 4; ++i)
		{
			if (lastskycol[i] == skycol)
			{
				return skybuf[i];
			}
		}

		lastskycol[skycolplace] = skycol;
		BYTE *composite = skybuf[skycolplace];
		skycolplace = (skycolplace + 1) & 3;

		// The ordering of the following code has been tuned to allow VC++ to optimize
		// it well. In particular, this arrangement lets it keep count in a register
		// instead of on the stack.
		const BYTE *front = fronttex->GetColumn(angle1, NULL);
		const BYTE *back = backskytex->GetColumn(angle2, NULL);

		int count = MIN<int>(512, MIN(backskytex->GetHeight(), fronttex->GetHeight()));
		i = 0;
		do
		{
			if (front[i])
			{
				composite[i] = front[i];
			}
			else
			{
				composite[i] = back[i];
			}
		} while (++i, --count);
		return composite;
	}
	else
	{
		//return R_GetOneSkyColumn(fronttex, x);
		for (i = skycolplace_bgra - 4; i < skycolplace_bgra; ++i)
		{
			int ic = (i % MAXSKYBUF); // i "checker" - can wrap around the ends of the array
			if (lastskycol_bgra[ic] == skycol)
			{
				return (BYTE*)(skybuf_bgra[ic]);
			}
		}

		lastskycol_bgra[skycolplace_bgra] = skycol;
		uint32_t *composite = skybuf_bgra[skycolplace_bgra];
		skycolplace_bgra = (skycolplace_bgra + 1) % MAXSKYBUF;

		// The ordering of the following code has been tuned to allow VC++ to optimize
		// it well. In particular, this arrangement lets it keep count in a register
		// instead of on the stack.
		const uint32_t *front = (const uint32_t *)fronttex->GetColumnBgra(angle1, NULL);
		const uint32_t *back = (const uint32_t *)backskytex->GetColumnBgra(angle2, NULL);

		//[SP] Paletted version is used for comparison only
		const BYTE *frontcompare = fronttex->GetColumn(angle1, NULL);

		int count = MIN<int>(512, MIN(backskytex->GetHeight(), fronttex->GetHeight()));
		i = 0;
		do
		{
			if (frontcompare[i])
			{
				composite[i] = front[i];
			}
			else
			{
				composite[i] = back[i];
			}
		} while (++i, --count);
		return (BYTE*)composite;
	}
}

static void R_DrawSkyColumnStripe(int start_x, int y1, int y2, int columns, double scale, double texturemid, double yrepeat)
{
	uint32_t height = frontskytex->GetHeight();

	for (int i = 0; i < columns; i++)
	{
		double uv_stepd = skyiscale * yrepeat;
		double v = (texturemid + uv_stepd * (y1 - CenterY + 0.5)) / height;
		double v_step = uv_stepd / height;

		uint32_t uv_pos = (uint32_t)(v * 0x01000000);
		uint32_t uv_step = (uint32_t)(v_step * 0x01000000);

		int x = start_x + i;
		if (MirrorFlags & RF_XFLIP)
			x = (viewwidth - x);

		DWORD ang, angle1, angle2;

		if (r_linearsky)
		{
			angle_t xangle = (angle_t)((0.5 - x / (double)viewwidth) * FocalTangent * ANGLE_90);
			ang = (skyangle + xangle) ^ skyflip;
		}
		else
		{
			ang = (skyangle + xtoviewangle[x]) ^ skyflip;
		}
		angle1 = (DWORD)((UMulScale16(ang, frontcyl) + frontpos) >> FRACBITS);
		angle2 = (DWORD)((UMulScale16(ang, backcyl) + backpos) >> FRACBITS);

		if (r_swtruecolor)
		{
			dc_wall_source[i] = (const BYTE *)frontskytex->GetColumnBgra(angle1, nullptr);
			dc_wall_source2[i] = backskytex ? (const BYTE *)backskytex->GetColumnBgra(angle2, nullptr) : nullptr;
		}
		else
		{
			dc_wall_source[i] = (const BYTE *)frontskytex->GetColumn(angle1, nullptr);
			dc_wall_source2[i] = backskytex ? (const BYTE *)backskytex->GetColumn(angle2, nullptr) : nullptr;
		}

		dc_wall_iscale[i] = uv_step;
		dc_wall_texturefrac[i] = uv_pos;
	}

	dc_wall_sourceheight[0] = height;
	dc_wall_sourceheight[1] = backskytex ? backskytex->GetHeight() : height;
	int pixelsize = r_swtruecolor ? 4 : 1;
	dc_dest = (ylookup[y1] + start_x) * pixelsize + dc_destorg;
	dc_count = y2 - y1;

	uint32_t solid_top = frontskytex->GetSkyCapColor(false);
	uint32_t solid_bottom = frontskytex->GetSkyCapColor(true);

	if (!backskytex)
		R_Drawers()->DrawSingleSkyColumn(solid_top, solid_bottom);
	else
		R_Drawers()->DrawDoubleSkyColumn(solid_top, solid_bottom);
}

static void R_DrawSkyColumn(int start_x, int y1, int y2, int columns)
{
	if (1 << frontskytex->HeightBits == frontskytex->GetHeight())
	{
		double texturemid = skymid * frontskytex->Scale.Y + frontskytex->GetHeight();
		R_DrawSkyColumnStripe(start_x, y1, y2, columns, frontskytex->Scale.Y, texturemid, frontskytex->Scale.Y);
	}
	else
	{
		double yrepeat = frontskytex->Scale.Y;
		double scale = frontskytex->Scale.Y * skyscale;
		double iscale = 1 / scale;
		short drawheight = short(frontskytex->GetHeight() * scale);
		double topfrac = fmod(skymid + iscale * (1 - CenterY), frontskytex->GetHeight());
		if (topfrac < 0) topfrac += frontskytex->GetHeight();
		double texturemid = topfrac - iscale * (1 - CenterY);
		R_DrawSkyColumnStripe(start_x, y1, y2, columns, scale, texturemid, yrepeat);
	}
}

static void R_DrawCapSky(visplane_t *pl)
{
	int x1 = pl->left;
	int x2 = pl->right;
	short *uwal = (short *)pl->top;
	short *dwal = (short *)pl->bottom;

	for (int x = x1; x < x2; x++)
	{
		int y1 = uwal[x];
		int y2 = dwal[x];
		if (y2 <= y1)
			continue;

		R_DrawSkyColumn(x, y1, y2, 1);
	}
}

static void R_DrawSky (visplane_t *pl)
{
	if (r_skymode == 2)
	{
		R_DrawCapSky(pl);
		return;
	}

	int x;
	float swal;

 	if (pl->left >= pl->right)
		return;

	swal = skyiscale;
	for (x = pl->left; x < pl->right; ++x)
	{
		swall[x] = swal;
	}

	if (MirrorFlags & RF_XFLIP)
	{
		for (x = pl->left; x < pl->right; ++x)
		{
			lwall[x] = (viewwidth - x) << FRACBITS;
		}
	}
	else
	{
		for (x = pl->left; x < pl->right; ++x)
		{
			lwall[x] = x << FRACBITS;
		}
	}

	for (x = 0; x < 4; ++x)
	{
		lastskycol[x] = 0xffffffff;
		lastskycol_bgra[x] = 0xffffffff;
	}

	rw_pic = frontskytex;
	rw_offset = 0;

	frontyScale = rw_pic->Scale.Y;
	dc_texturemid = skymid * frontyScale;

	if (1 << frontskytex->HeightBits == frontskytex->GetHeight())
	{ // The texture tiles nicely
		for (x = 0; x < 4; ++x)
		{
			lastskycol[x] = 0xffffffff;
			lastskycol_bgra[x] = 0xffffffff;
		}
		R_DrawSkySegment (pl->left, pl->right, (short *)pl->top, (short *)pl->bottom, swall, lwall,
			frontyScale, backskytex == NULL ? R_GetOneSkyColumn : R_GetTwoSkyColumns);
	}
	else
	{ // The texture does not tile nicely
		frontyScale *= skyscale;
		frontiScale = 1 / frontyScale;
		R_DrawSkyStriped (pl);
	}
}

static void R_DrawSkyStriped (visplane_t *pl)
{
	short drawheight = short(frontskytex->GetHeight() * frontyScale);
	double topfrac;
	double iscale = frontiScale;
	short top[MAXWIDTH], bot[MAXWIDTH];
	short yl, yh;
	int x;

	topfrac = fmod(skymid + iscale * (1 - CenterY), frontskytex->GetHeight());
	if (topfrac < 0) topfrac += frontskytex->GetHeight();
	yl = 0;
	yh = short((frontskytex->GetHeight() - topfrac) * frontyScale);
	dc_texturemid = topfrac - iscale * (1 - CenterY);

	while (yl < viewheight)
	{
		for (x = pl->left; x < pl->right; ++x)
		{
			top[x] = MAX (yl, (short)pl->top[x]);
			bot[x] = MIN (yh, (short)pl->bottom[x]);
		}
		for (x = 0; x < 4; ++x)
		{
			lastskycol[x] = 0xffffffff;
			lastskycol_bgra[x] = 0xffffffff;
		}
		R_DrawSkySegment (pl->left, pl->right, top, bot, swall, lwall, rw_pic->Scale.Y,
			backskytex == NULL ? R_GetOneSkyColumn : R_GetTwoSkyColumns);
		yl = yh;
		yh += drawheight;
		dc_texturemid = iscale * (centery-yl-1);
	}
}

//==========================================================================
//
// R_DrawPlanes
//
// At the end of each frame.
//
//==========================================================================


int R_DrawPlanes ()
{
	visplane_t *pl;
	int i;
	int vpcount = 0;

	ds_color = 3;

	for (i = 0; i < MAXVISPLANES; i++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			// kg3D - draw only correct planes
			if(pl->CurrentPortalUniq != CurrentPortalUniq || pl->CurrentSkybox != CurrentSkybox)
				continue;
			// kg3D - draw only real planes now
			if(pl->sky >= 0) {
				vpcount++;
				R_DrawSinglePlane (pl, OPAQUE, false, false);
			}
		}
	}
	return vpcount;
}

// kg3D - draw all visplanes with "height"
void R_DrawHeightPlanes(double height)
{
	visplane_t *pl;
	int i;

	ds_color = 3;

	DVector3 oViewPos = ViewPos;
	DAngle oViewAngle = ViewAngle;

	for (i = 0; i < MAXVISPLANES; i++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			// kg3D - draw only correct planes
			if(pl->CurrentSkybox != CurrentSkybox || pl->CurrentPortalUniq != CurrentPortalUniq)
				continue;
			if(pl->sky < 0 && pl->height.Zat0() == height) {
				ViewPos = pl->viewpos;
				ViewAngle = pl->viewangle;
				MirrorFlags = pl->MirrorFlags;
				R_DrawSinglePlane (pl, pl->sky & 0x7FFFFFFF, pl->Additive, true);
			}
		}
	}
	ViewPos = oViewPos;
	ViewAngle = oViewAngle;
}


//==========================================================================
//
// R_DrawSinglePlane
//
// Draws a single visplane.
//
//==========================================================================

void R_DrawSinglePlane (visplane_t *pl, fixed_t alpha, bool additive, bool masked)
{
	if (pl->left >= pl->right)
		return;

	if (r_drawflat)
	{ // [RH] no texture mapping
		ds_color += 4;
		R_MapVisPlane (pl, R_MapColoredPlane);
	}
	else if (pl->picnum == skyflatnum)
	{ // sky flat
		R_DrawSkyPlane (pl);
	}
	else
	{ // regular flat
		FTexture *tex = TexMan(pl->picnum, true);

		if (tex->UseType == FTexture::TEX_Null)
		{
			return;
		}

		if (!masked && !additive)
		{ // If we're not supposed to see through this plane, draw it opaque.
			alpha = OPAQUE;
		}
		else if (!tex->bMasked)
		{ // Don't waste time on a masked texture if it isn't really masked.
			masked = false;
		}
		R_SetSpanTexture(tex);
		double xscale = pl->xform.xScale * tex->Scale.X;
		double yscale = pl->xform.yScale * tex->Scale.Y;

		basecolormap = pl->colormap;
		planeshade = LIGHT2SHADE(pl->lightlevel);

		if (r_drawflat || (!pl->height.isSlope() && !tilt))
		{
			R_DrawNormalPlane(pl, xscale, yscale, alpha, additive, masked);
		}
		else
		{
			R_DrawTiltedPlane(pl, xscale, yscale, alpha, additive, masked);
		}
	}
	NetUpdate ();
}

//==========================================================================
//
// R_DrawSkyPlane
//
//==========================================================================

void R_DrawSkyPlane (visplane_t *pl)
{
	FTextureID sky1tex, sky2tex;
	double frontdpos = 0, backdpos = 0;

	if ((level.flags & LEVEL_SWAPSKIES) && !(level.flags & LEVEL_DOUBLESKY))
	{
		sky1tex = sky2texture;
	}
	else
	{
		sky1tex = sky1texture;
	}
	sky2tex = sky2texture;
	skymid = skytexturemid;
	skyangle = ViewAngle.BAMs();

	if (pl->picnum == skyflatnum)
	{
		if (!(pl->sky & PL_SKYFLAT))
		{	// use sky1
		sky1:
			frontskytex = TexMan(sky1tex, true);
			if (level.flags & LEVEL_DOUBLESKY)
				backskytex = TexMan(sky2tex, true);
			else
				backskytex = NULL;
			skyflip = 0;
			frontdpos = sky1pos;
			backdpos = sky2pos;
			frontcyl = sky1cyl;
			backcyl = sky2cyl;
		}
		else if (pl->sky == PL_SKYFLAT)
		{	// use sky2
			frontskytex = TexMan(sky2tex, true);
			backskytex = NULL;
			frontcyl = sky2cyl;
			skyflip = 0;
			frontdpos = sky2pos;
		}
		else
		{	// MBF's linedef-controlled skies
			// Sky Linedef
			const line_t *l = &lines[(pl->sky & ~PL_SKYFLAT)-1];

			// Sky transferred from first sidedef
			const side_t *s = l->sidedef[0];
			int pos;

			// Texture comes from upper texture of reference sidedef
			// [RH] If swapping skies, then use the lower sidedef
			if (level.flags & LEVEL_SWAPSKIES && s->GetTexture(side_t::bottom).isValid())
			{
				pos = side_t::bottom;
			}
			else
			{
				pos = side_t::top;
			}

			frontskytex = TexMan(s->GetTexture(pos), true);
			if (frontskytex == NULL || frontskytex->UseType == FTexture::TEX_Null)
			{ // [RH] The blank texture: Use normal sky instead.
				goto sky1;
			}
			backskytex = NULL;

			// Horizontal offset is turned into an angle offset,
			// to allow sky rotation as well as careful positioning.
			// However, the offset is scaled very small, so that it
			// allows a long-period of sky rotation.
			skyangle += FLOAT2FIXED(s->GetTextureXOffset(pos));

			// Vertical offset allows careful sky positioning.
			skymid = s->GetTextureYOffset(pos) - 28;

			// We sometimes flip the picture horizontally.
			//
			// Doom always flipped the picture, so we make it optional,
			// to make it easier to use the new feature, while to still
			// allow old sky textures to be used.
			skyflip = l->args[2] ? 0u : ~0u;

			int frontxscale = int(frontskytex->Scale.X * 1024);
			frontcyl = MAX(frontskytex->GetWidth(), frontxscale);
			if (skystretch)
			{
				skymid = skymid * frontskytex->GetScaledHeightDouble() / SKYSTRETCH_HEIGHT;
			}
		}
	}
	frontpos = int(fmod(frontdpos, sky1cyl * 65536.0));
	if (backskytex != NULL)
	{
		backpos = int(fmod(backdpos, sky2cyl * 65536.0));
	}

	bool fakefixed = false;
	if (fixedcolormap)
	{
		R_SetColorMapLight(fixedcolormap, 0, 0);
	}
	else
	{
		fakefixed = true;
		fixedcolormap = &NormalLight;
		R_SetColorMapLight(fixedcolormap, 0, 0);
	}

	R_DrawSky (pl);

	if (fakefixed)
		fixedcolormap = NULL;
}

//==========================================================================
//
// R_DrawNormalPlane
//
//==========================================================================

void R_DrawNormalPlane (visplane_t *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked)
{
	if (alpha <= 0)
	{
		return;
	}

	double planeang = (pl->xform.Angle + pl->xform.baseAngle).Radians();
	double xstep, ystep, leftxfrac, leftyfrac, rightxfrac, rightyfrac;
	double x;

	xscale = xs_ToFixed(32 - ds_xbits, _xscale);
	yscale = xs_ToFixed(32 - ds_ybits, _yscale);
	if (planeang != 0)
	{
		double cosine = cos(planeang), sine = sin(planeang);
		pviewx = FLOAT2FIXED(pl->xform.xOffs + ViewPos.X * cosine - ViewPos.Y * sine);
		pviewy = FLOAT2FIXED(pl->xform.yOffs - ViewPos.X * sine - ViewPos.Y * cosine);
	}
	else
	{
		pviewx = FLOAT2FIXED(pl->xform.xOffs + ViewPos.X);
		pviewy = FLOAT2FIXED(pl->xform.yOffs - ViewPos.Y);
	}

	pviewx = FixedMul (xscale, pviewx);
	pviewy = FixedMul (yscale, pviewy);
	
	// left to right mapping
	planeang += (ViewAngle - 90).Radians();

	// Scale will be unit scale at FocalLengthX (normally SCREENWIDTH/2) distance
	xstep = cos(planeang) / FocalLengthX;
	ystep = -sin(planeang) / FocalLengthX;

	// [RH] flip for mirrors
	if (MirrorFlags & RF_XFLIP)
	{
		xstep = -xstep;
		ystep = -ystep;
	}

	planeang += M_PI/2;
	double cosine = cos(planeang), sine = -sin(planeang);
	x = pl->right - centerx - 0.5;
	rightxfrac = _xscale * (cosine + x * xstep);
	rightyfrac = _yscale * (sine + x * ystep);
	x = pl->left - centerx - 0.5;
	leftxfrac = _xscale * (cosine + x * xstep);
	leftyfrac = _yscale * (sine + x * ystep);

	basexfrac = rightxfrac;
	baseyfrac = rightyfrac;
	xstepscale = (rightxfrac - leftxfrac) / (pl->right - pl->left);
	ystepscale = (rightyfrac - leftyfrac) / (pl->right - pl->left);

	planeheight = fabs(pl->height.Zat0() - ViewPos.Z);

	GlobVis = r_FloorVisibility / planeheight;
	ds_light = 0;
	if (fixedlightlev >= 0)
	{
		R_SetDSColorMapLight(basecolormap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
		plane_shade = false;
	}
	else if (fixedcolormap)
	{
		R_SetDSColorMapLight(fixedcolormap, 0, 0);
		plane_shade = false;
	}
	else
	{
		plane_shade = true;
	}

	if (spanfunc != &SWPixelFormatDrawers::FillSpan)
	{
		if (masked)
		{
			if (alpha < OPAQUE || additive)
			{
				if (!additive)
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanMaskedTranslucent;
					dc_srcblend = Col2RGB8[alpha>>10];
					dc_destblend = Col2RGB8[(OPAQUE-alpha)>>10];
					dc_srcalpha = alpha;
					dc_destalpha = OPAQUE - alpha;
				}
				else
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanMaskedAddClamp;
					dc_srcblend = Col2RGB8_LessPrecision[alpha>>10];
					dc_destblend = Col2RGB8_LessPrecision[FRACUNIT>>10];
					dc_srcalpha = alpha;
					dc_destalpha = FRACUNIT;
				}
			}
			else
			{
				spanfunc = &SWPixelFormatDrawers::DrawSpanMasked;
			}
		}
		else
		{
			if (alpha < OPAQUE || additive)
			{
				if (!additive)
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanTranslucent;
					dc_srcblend = Col2RGB8[alpha>>10];
					dc_destblend = Col2RGB8[(OPAQUE-alpha)>>10];
					dc_srcalpha = alpha;
					dc_destalpha = OPAQUE - alpha;
				}
				else
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanAddClamp;
					dc_srcblend = Col2RGB8_LessPrecision[alpha>>10];
					dc_destblend = Col2RGB8_LessPrecision[FRACUNIT>>10];
					dc_srcalpha = alpha;
					dc_destalpha = FRACUNIT;
				}
			}
			else
			{
				spanfunc = &SWPixelFormatDrawers::DrawSpan;
			}
		}
	}
	R_MapVisPlane (pl, R_MapPlane);
}

//==========================================================================
//
// R_DrawTiltedPlane
//
//==========================================================================

void R_DrawTiltedPlane (visplane_t *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked)
{
	static const float ifloatpow2[16] =
	{
		// ifloatpow2[i] = 1 / (1 << i)
		64.f, 32.f, 16.f, 8.f, 4.f, 2.f, 1.f, 0.5f,
		0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f,
		0.00390625f, 0.001953125f
		/*, 0.0009765625f, 0.00048828125f, 0.000244140625f,
		1.220703125e-4f, 6.103515625e-5, 3.0517578125e-5*/
	};
	double lxscale, lyscale;
	double xscale, yscale;
	FVector3 p, m, n;
	double ang, planeang, cosine, sine;
	double zeroheight;

	if (alpha <= 0)
	{
		return;
	}

	lxscale = _xscale * ifloatpow2[ds_xbits];
	lyscale = _yscale * ifloatpow2[ds_ybits];
	xscale = 64.f / lxscale;
	yscale = 64.f / lyscale;
	zeroheight = pl->height.ZatPoint(ViewPos);

	pviewx = xs_ToFixed(32 - ds_xbits, pl->xform.xOffs * pl->xform.xScale);
	pviewy = xs_ToFixed(32 - ds_ybits, pl->xform.yOffs * pl->xform.yScale);
	planeang = (pl->xform.Angle + pl->xform.baseAngle).Radians();

	// p is the texture origin in view space
	// Don't add in the offsets at this stage, because doing so can result in
	// errors if the flat is rotated.
	ang = M_PI*3/2 - ViewAngle.Radians();
	cosine = cos(ang), sine = sin(ang);
	p[0] = ViewPos.X * cosine - ViewPos.Y * sine;
	p[2] = ViewPos.X * sine + ViewPos.Y * cosine;
	p[1] = pl->height.ZatPoint(0.0, 0.0) - ViewPos.Z;

	// m is the v direction vector in view space
	ang = ang - M_PI / 2 - planeang;
	cosine = cos(ang), sine = sin(ang);
	m[0] = yscale * cosine;
	m[2] = yscale * sine;
//	m[1] = pl->height.ZatPointF (0, iyscale) - pl->height.ZatPointF (0,0));
//	VectorScale2 (m, 64.f/VectorLength(m));

	// n is the u direction vector in view space
#if 0
	//let's use the sin/cosine we already know instead of computing new ones
	ang += M_PI/2
	n[0] = -xscale * cos(ang);
	n[2] = -xscale * sin(ang);
#else
	n[0] = xscale * sine;
	n[2] = -xscale * cosine;
#endif
//	n[1] = pl->height.ZatPointF (ixscale, 0) - pl->height.ZatPointF (0,0));
//	VectorScale2 (n, 64.f/VectorLength(n));

	// This code keeps the texture coordinates constant across the x,y plane no matter
	// how much you slope the surface. Use the commented-out code above instead to keep
	// the textures a constant size across the surface's plane instead.
	cosine = cos(planeang), sine = sin(planeang);
	m[1] = pl->height.ZatPoint(ViewPos.X + yscale * sine, ViewPos.Y + yscale * cosine) - zeroheight;
	n[1] = -(pl->height.ZatPoint(ViewPos.X - xscale * cosine, ViewPos.Y + xscale * sine) - zeroheight);

	plane_su = p ^ m;
	plane_sv = p ^ n;
	plane_sz = m ^ n;

	plane_su.Z *= FocalLengthX;
	plane_sv.Z *= FocalLengthX;
	plane_sz.Z *= FocalLengthX;

	plane_su.Y *= IYaspectMul;
	plane_sv.Y *= IYaspectMul;
	plane_sz.Y *= IYaspectMul;

	// Premultiply the texture vectors with the scale factors
	plane_su *= 4294967296.f;
	plane_sv *= 4294967296.f;

	if (MirrorFlags & RF_XFLIP)
	{
		plane_su[0] = -plane_su[0];
		plane_sv[0] = -plane_sv[0];
		plane_sz[0] = -plane_sz[0];
	}

	planelightfloat = (r_TiltVisibility * lxscale * lyscale) / (fabs(pl->height.ZatPoint(ViewPos) - ViewPos.Z)) / 65536.f;

	if (pl->height.fC() > 0)
		planelightfloat = -planelightfloat;

	if (fixedlightlev >= 0)
	{
		R_SetDSColorMapLight(basecolormap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
		plane_shade = false;
	}
	else if (fixedcolormap)
	{
		R_SetDSColorMapLight(fixedcolormap, 0, 0);
		plane_shade = false;
	}
	else
	{
		R_SetDSColorMapLight(basecolormap, 0, 0);
		plane_shade = true;
	}

	// Hack in support for 1 x Z and Z x 1 texture sizes
	if (ds_ybits == 0)
	{
		plane_sv[2] = plane_sv[1] = plane_sv[0] = 0;
	}
	if (ds_xbits == 0)
	{
		plane_su[2] = plane_su[1] = plane_su[0] = 0;
	}

	R_MapVisPlane (pl, R_MapTiltedPlane);
}

//==========================================================================
//
// R_MapVisPlane
//
// t1/b1 are at x
// t2/b2 are at x+1
// spanend[y] is at the right edge
//
//==========================================================================

void R_MapVisPlane (visplane_t *pl, void (*mapfunc)(int y, int x1))
{
	int x = pl->right - 1;
	int t2 = pl->top[x];
	int b2 = pl->bottom[x];

	ds_light_list = pl->lights;

	if (b2 > t2)
	{
		fillshort (spanend+t2, b2-t2, x);
	}

	for (--x; x >= pl->left; --x)
	{
		int t1 = pl->top[x];
		int b1 = pl->bottom[x];
		const int xr = x+1;
		int stop;

		// Draw any spans that have just closed
		stop = MIN (t1, b2);
		while (t2 < stop)
		{
			mapfunc (t2++, xr);
		}
		stop = MAX (b1, t2);
		while (b2 > stop)
		{
			mapfunc (--b2, xr);
		}

		// Mark any spans that have just opened
		stop = MIN (t2, b1);
		while (t1 < stop)
		{
			spanend[t1++] = x;
		}
		stop = MAX (b2, t2);
		while (b1 > stop)
		{
			spanend[--b1] = x;
		}

		t2 = pl->top[x];
		b2 = pl->bottom[x];
		basexfrac -= xstepscale;
		baseyfrac -= ystepscale;
	}
	// Draw any spans that are still open
	while (t2 < b2)
	{
		mapfunc (--b2, pl->left);
	}

	ds_light_list = nullptr;
}

}