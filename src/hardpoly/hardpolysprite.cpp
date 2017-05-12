/*
**  Handling drawing a sprite
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "hardpolysprite.h"
#include "r_utility.h"
#include "actor.h"

EXTERN_CVAR(Float, transsouls)
EXTERN_CVAR(Int, r_drawfuzz)

HardpolyRenderSprite::HardpolyRenderSprite(AActor *thing, subsector_t *sub, uint32_t initSubsectorDepth, double dist, float t1, float t2)
	: thing(thing), sub(sub), SpriteLeft(t1), SpriteRight(t2)
{
	subsectorDepth = initSubsectorDepth;
	DistanceSquared = dist;
}

void HardpolyRenderSprite::Setup(HardpolyRenderer *renderer)
{
	DVector2 line[2];
	if (!GetLine(thing, line[0], line[1]))
		return;
	
	const auto &viewpoint = r_viewpoint;
	DVector3 pos = thing->InterpolatedPosition(viewpoint.TicFrac);
	pos.Z += thing->GetBobOffset(viewpoint.TicFrac);

	bool flipTextureX = false;
	tex = GetSpriteTexture(thing, flipTextureX);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
		return;

	DVector2 spriteScale = thing->Scale;
	double thingxscalemul = spriteScale.X / tex->Scale.X;
	double thingyscalemul = spriteScale.Y / tex->Scale.Y;

	if (flipTextureX)
		pos.X -= (tex->GetWidth() - tex->LeftOffset) * thingxscalemul;
	else
		pos.X -= tex->LeftOffset * thingxscalemul;

	pos.Z -= (tex->GetHeight() - tex->TopOffset) * thingyscalemul + thing->Floorclip;

	double spriteHalfWidth = thingxscalemul * tex->GetWidth() * 0.5;
	double spriteHeight = thingyscalemul * tex->GetHeight();

	pos.X += spriteHalfWidth;

	//double depth = 1.0;
	//visstyle_t visstyle = GetSpriteVisStyle(thing, depth);
	// Rumor has it that AlterWeaponSprite needs to be called with visstyle passed in somewhere around here..
	//R_SetColorMapLight(visstyle.BaseColormap, 0, visstyle.ColormapNum << FRACBITS);

	bool foggy = false;
	int actualextralight = foggy ? 0 : viewpoint.extralight << 4;

	std::pair<float, float> offsets[4] =
	{
		{ SpriteLeft,  1.0f },
		{ SpriteRight,  1.0f },
		{ SpriteRight,  0.0f },
		{ SpriteLeft,  0.0f },
	};

	DVector2 points[2] =
	{
		line[0] * (1.0 - SpriteLeft) + line[1] * SpriteLeft,
		line[0] * (1.0 - SpriteRight) + line[1] * SpriteRight
	};

	bool fullbrightSprite = ((thing->renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT));
	float lightlevel = (float)(fullbrightSprite ? 255 : thing->Sector->lightlevel + actualextralight);

	startVertex = (int)renderer->TranslucentVertices.size();

	for (int i = 0; i < 4; i++)
	{
		auto &p = (i == 0 || i == 3) ? points[0] : points[1];

		TranslucentVertex vertex;
		vertex.Position.X = (float)p.X;
		vertex.Position.Y = (float)p.Y;
		vertex.Position.Z = (float)(pos.Z + spriteHeight * offsets[i].second);
		vertex.Position.W = 1.0f;
		vertex.UV.X = (float)(offsets[i].first * tex->Scale.X);
		vertex.UV.Y = (float)((1.0f - offsets[i].second) * tex->Scale.Y);
		if (flipTextureX)
			vertex.UV.X = 1.0f - vertex.UV.X;
		vertex.LightLevel = lightlevel;

		renderer->TranslucentVertices.push_back(vertex);
	}
}

void HardpolyRenderSprite::Render(HardpolyRenderer *renderer)
{
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
		return;

	renderer->mContext->SetTexture(0, renderer->GetTexture(tex));
	renderer->mContext->Draw(GPUDrawMode::TriangleFan, startVertex, 4);

	/*
	PolyDrawArgs args;
	args.SetLight(GetColorTable(sub->sector->Colormap, sub->sector->SpecialColors[sector_t::sprites], true), lightlevel, PolyRenderer::Instance()->Light.SpriteGlobVis(foggy), fullbrightSprite);
	args.SetSubsectorDepth(subsectorDepth);
	args.SetTransform(&worldToClip);
	args.SetFaceCullCCW(true);
	args.SetStencilTestValue(stencilValue);
	args.SetWriteStencil(true, stencilValue);
	args.SetClipPlane(clipPlane);
	args.SetStyle(thing->RenderStyle, thing->Alpha, thing->fillcolor, thing->Translation, tex, fullbrightSprite);
	args.SetSubsectorDepthTest(true);
	args.SetWriteSubsectorDepth(false);
	args.SetWriteStencil(false);
	args.DrawArray(vertices, 4, PolyDrawMode::TriangleFan);
	*/
}

bool HardpolyRenderSprite::IsThingCulled(AActor *thing)
{
	FIntCVar *cvar = thing->GetInfo()->distancecheck;
	if (cvar != nullptr && *cvar >= 0)
	{
		double dist = (thing->Pos() - r_viewpoint.Pos).LengthSquared();
		double check = (double)**cvar;
		if (dist >= check * check)
			return true;
	}

	// Don't waste time projecting sprites that are definitely not visible.
	if (thing == nullptr ||
		(thing->renderflags & RF_INVISIBLE) ||
		!thing->RenderStyle.IsVisible(thing->Alpha) ||
		!thing->IsVisibleToPlayer())
	{
		return true;
	}

	return false;
}

FTexture *HardpolyRenderSprite::GetSpriteTexture(AActor *thing, /*out*/ bool &flipX)
{
	const auto &viewpoint = r_viewpoint;
	flipX = false;

	if (thing->renderflags & RF_FLATSPRITE)
		return nullptr;	// do not draw flat sprites.

	if (thing->picnum.isValid())
	{
		FTexture *tex = TexMan(thing->picnum);
		if (tex->UseType == FTexture::TEX_Null)
		{
			return nullptr;
		}

		if (tex->Rotations != 0xFFFF)
		{
			// choose a different rotation based on player view
			spriteframe_t *sprframe = &SpriteFrames[tex->Rotations];
			DVector3 pos = thing->InterpolatedPosition(viewpoint.TicFrac);
			pos.Z += thing->GetBobOffset(viewpoint.TicFrac);
			DAngle ang = (pos - viewpoint.Pos).Angle();
			angle_t rot;
			if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				rot = (ang - thing->Angles.Yaw + 45.0 / 2 * 9).BAMs() >> 28;
			}
			else
			{
				rot = (ang - thing->Angles.Yaw + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
			}
			flipX = (sprframe->Flip & (1 << rot)) != 0;
			tex = TexMan[sprframe->Texture[rot]];	// Do not animate the rotation
		}
		return tex;
	}
	else
	{
		// decide which texture to use for the sprite
		int spritenum = thing->sprite;
		if (spritenum >= (signed)sprites.Size() || spritenum < 0)
			return nullptr;

		spritedef_t *sprdef = &sprites[spritenum];
		if (thing->frame >= sprdef->numframes)
		{
			// If there are no frames at all for this sprite, don't draw it.
			return nullptr;
		}
		else
		{
			//picnum = SpriteFrames[sprdef->spriteframes + thing->frame].Texture[0];
			// choose a different rotation based on player view

			DVector3 pos = thing->InterpolatedPosition(viewpoint.TicFrac);
			pos.Z += thing->GetBobOffset(viewpoint.TicFrac);
			DAngle ang = (pos - viewpoint.Pos).Angle();

			DAngle sprangle = thing->GetSpriteAngle((pos - viewpoint.Pos).Angle(), viewpoint.TicFrac);
			FTextureID tex = sprdef->GetSpriteFrame(thing->frame, -1, sprangle, &flipX);
			if (!tex.isValid()) return nullptr;
			return TexMan[tex];
		}
	}
}

bool HardpolyRenderSprite::GetLine(AActor *thing, DVector2 &left, DVector2 &right)
{
	if (IsThingCulled(thing))
		return false;

	const auto &viewpoint = r_viewpoint;
	DVector3 pos = thing->InterpolatedPosition(viewpoint.TicFrac);

	bool flipTextureX = false;
	FTexture *tex = GetSpriteTexture(thing, flipTextureX);
	if (tex == nullptr)
		return false;

	DVector2 spriteScale = thing->Scale;
	double thingxscalemul = spriteScale.X / tex->Scale.X;
	double thingyscalemul = spriteScale.Y / tex->Scale.Y;

	if (flipTextureX)
		pos.X -= (tex->GetWidth() - tex->LeftOffset) * thingxscalemul;
	else
		pos.X -= tex->LeftOffset * thingxscalemul;

	double spriteHalfWidth = thingxscalemul * tex->GetWidth() * 0.5;
	double spriteHeight = thingyscalemul * tex->GetHeight();

	pos.X += spriteHalfWidth;

	left = DVector2(pos.X - viewpoint.Sin * spriteHalfWidth, pos.Y + viewpoint.Cos * spriteHalfWidth);
	right = DVector2(pos.X + viewpoint.Sin * spriteHalfWidth, pos.Y - viewpoint.Cos * spriteHalfWidth);
	return true;
}
