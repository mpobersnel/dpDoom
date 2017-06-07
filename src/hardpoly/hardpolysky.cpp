/*
**  Sky dome rendering
**  Copyright(C) 2003-2016 Christoph Oelckers
**  All rights reserved.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU Lesser General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU Lesser General Public License for more details.
**
**  You should have received a copy of the GNU Lesser General Public License
**  along with this program.  If not, see http:**www.gnu.org/licenses/
**
**  Loosely based on the JDoom sky and the ZDoomGL 0.66.2 sky.
*/

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "hardpolysky.h"
#include "r_sky.h" // for skyflatnum
#include "g_levellocals.h"
#include "r_utility.h"
#include "v_palette.h"
#include "hardpolyrenderer.h"
#include "gl/system/gl_system.h"

HardpolySkyDome::HardpolySkyDome()
{
	CreateDome();
}

void HardpolySkyDome::Render(HardpolyRenderer *renderer)
{
	HardpolySkySetup frameSetup;
	frameSetup.Update();

	// frontcyl = pixels for full 360 degrees, front texture
	// backcyl = pixels for full 360 degrees, back texture
	// skymid = Y scaled pixel offset
	// sky1pos = unscaled X offset, front
	// sky2pos = unscaled X offset, back
	// frontpos = scaled X pixel offset (fixed point)
	// backpos = scaled X pixel offset (fixed point)
	// skyflip = flip X direction

	float scaleBaseV = 1.42f;
	float offsetBaseV = 0.25f;

	float scaleFrontU = frameSetup.frontcyl / (float)frameSetup.frontskytex->GetWidth();
	float scaleFrontV = (float)frameSetup.frontskytex->Scale.Y * scaleBaseV;
	float offsetFrontU = (float)((frameSetup.frontpos / 65536.0 + frameSetup.frontcyl / 2) / frameSetup.frontskytex->GetWidth());
	float offsetFrontV = (float)((frameSetup.skymid / frameSetup.frontskytex->GetHeight() + offsetBaseV) * scaleBaseV);

	if (!mVertexArray)
	{
		mVertexBuffer = std::make_shared<GPUVertexBuffer>(&mVertices[0], mVertices.Size() * (int)sizeof(Vertex));

		std::vector<GPUVertexAttributeDesc> attributes =
		{
			{ 0, 4, GPUVertexAttributeType::Float, false, sizeof(Vertex), offsetof(Vertex, x), mVertexBuffer },
			{ 1, 2, GPUVertexAttributeType::Float, false, sizeof(Vertex), offsetof(Vertex, u), mVertexBuffer }
		};

		mVertexArray = std::make_shared<GPUVertexArray>(attributes);
	}

	renderer->mContext->SetVertexArray(mVertexArray);
	renderer->mContext->SetProgram(renderer->mSkyProgram);
	renderer->mContext->SetUniforms(0, renderer->mSkyFrameUniforms[renderer->mCurrentFrameUniforms]);

	glUniform1i(glGetUniformLocation(renderer->mSkyProgram->Handle(), "FrontTexture"), 0);
	glUniform4f(glGetUniformLocation(renderer->mSkyProgram->Handle(), "OffsetScale"), offsetFrontU, offsetFrontV, scaleFrontU, scaleFrontV);

	renderer->mContext->SetSampler(0, renderer->mSamplerLinear);
	renderer->mContext->SetTexture(0, renderer->GetTexture(frameSetup.frontskytex));

	int rc = mRows + 1;

	//args.SetSubsectorDepth(RenderHardpolyScene::SkySubsectorDepth);
	//args.SetStencilTestValue(255);
	//args.SetWriteStencil(true, 1);

	PalEntry topcapcolor = frameSetup.frontskytex->GetSkyCapColor(false);
	PalEntry bottomcapcolor = frameSetup.frontskytex->GetSkyCapColor(true);
	//uint8_t topcapindex = RGB256k.All[((RPART(topcapcolor) >> 2) << 12) | ((GPART(topcapcolor) >> 2) << 6) | (BPART(topcapcolor) >> 2)];
	//uint8_t bottomcapindex = RGB256k.All[((RPART(bottomcapcolor) >> 2) << 12) | ((GPART(bottomcapcolor) >> 2) << 6) | (BPART(bottomcapcolor) >> 2)];

	glUniform4f(glGetUniformLocation(renderer->mSkyProgram->Handle(), "CapColor"), topcapcolor.r / 255.0f, topcapcolor.g / 255.0f, topcapcolor.b / 255.0f, 1.0f);

	renderer->mContext->Draw(GPUDrawMode::TriangleFan, mPrimStart[0], mPrimStart[1] - mPrimStart[0]);

	for (int i = 1; i <= mRows; i++)
	{
		int row = i;
		renderer->mContext->Draw(GPUDrawMode::TriangleStrip, mPrimStart[row], mPrimStart[row + 1] - mPrimStart[row]);
	}

	glUniform4f(glGetUniformLocation(renderer->mSkyProgram->Handle(), "CapColor"), bottomcapcolor.r / 255.0f, bottomcapcolor.g / 255.0f, bottomcapcolor.b / 255.0f, 1.0f);

	renderer->mContext->Draw(GPUDrawMode::TriangleFan, mPrimStart[rc], mPrimStart[rc + 1] - mPrimStart[rc]);

	for (int i = 1; i <= mRows; i++)
	{
		int row = rc + i;
		renderer->mContext->Draw(GPUDrawMode::TriangleStrip, mPrimStart[row], mPrimStart[row + 1] - mPrimStart[row]);
	}

	renderer->mContext->SetTexture(0, nullptr);
	renderer->mContext->SetSampler(0, nullptr);

	renderer->mContext->SetUniforms(0, nullptr);
	renderer->mContext->SetVertexArray(nullptr);
	renderer->mContext->SetProgram(nullptr);
}

void HardpolySkyDome::CreateDome()
{
	mColumns = 128;
	mRows = 4;
	CreateSkyHemisphere(false);
	CreateSkyHemisphere(true);
	mPrimStart.Push(mVertices.Size());
}

void HardpolySkyDome::CreateSkyHemisphere(bool zflip)
{
	int r, c;

	mPrimStart.Push(mVertices.Size());

	for (c = 0; c < mColumns; c++)
	{
		SkyVertex(1, c, zflip);
	}

	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for (r = 0; r < mRows; r++)
	{
		mPrimStart.Push(mVertices.Size());
		for (c = 0; c <= mColumns; c++)
		{
			SkyVertex(r + zflip, c, zflip);
			SkyVertex(r + 1 - zflip, c, zflip);
		}
	}
}

HardpolySkyDome::Vertex HardpolySkyDome::SetVertexXYZ(float xx, float yy, float zz, float uu, float vv)
{
	Vertex v;
	v.x = xx;
	v.y = zz;
	v.z = yy;
	v.w = 1.0f;
	v.u = uu;
	v.v = vv;
	return v;
}

void HardpolySkyDome::SkyVertex(int r, int c, bool zflip)
{
	static const FAngle maxSideAngle = 60.f;
	static const float scale = 10000.;

	FAngle topAngle = (c / (float)mColumns * 360.f);
	FAngle sideAngle = maxSideAngle * (float)(mRows - r) / (float)mRows;
	float height = sideAngle.Sin();
	float realRadius = scale * sideAngle.Cos();
	FVector2 pos = topAngle.ToVector(realRadius);
	float z = (!zflip) ? scale * height : -scale * height;

	float u, v;

	// And the texture coordinates.
	if (!zflip)	// Flipped Y is for the lower hemisphere.
	{
		u = (-c / (float)mColumns);
		v = (r / (float)mRows);
	}
	else
	{
		u = (-c / (float)mColumns);
		v = 1.0f + ((mRows - r) / (float)mRows);
	}

	if (r != 4) z += 300;

	// And finally the vertex.
	mVertices.Push(SetVertexXYZ(-pos.X, z - 1.f, pos.Y, u, v - 0.5f));
}

/////////////////////////////////////////////////////////////////////////////

void HardpolySkySetup::Update()
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
	skyangle = 0;

	int sectorSky = 0;// sector->sky;

	if (!(sectorSky & PL_SKYFLAT))
	{	// use sky1
	sky1:
		frontskytex = TexMan(sky1tex, true);
		if (level.flags & LEVEL_DOUBLESKY)
			backskytex = TexMan(sky2tex, true);
		else
			backskytex = nullptr;
		skyflip = false;
		frontdpos = sky1pos;
		backdpos = sky2pos;
		frontcyl = sky1cyl;
		backcyl = sky2cyl;
	}
	else if (sectorSky == PL_SKYFLAT)
	{	// use sky2
		frontskytex = TexMan(sky2tex, true);
		backskytex = nullptr;
		frontcyl = sky2cyl;
		skyflip = false;
		frontdpos = sky2pos;
	}
	else
	{	// MBF's linedef-controlled skies
		// Sky Linedef
		const line_t *l = &level.lines[(sectorSky & ~PL_SKYFLAT) - 1];

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
		if (frontskytex == nullptr || frontskytex->UseType == FTexture::TEX_Null)
		{ // [RH] The blank texture: Use normal sky instead.
			goto sky1;
		}
		backskytex = nullptr;

		// Horizontal offset is turned into an angle offset,
		// to allow sky rotation as well as careful positioning.
		// However, the offset is scaled very small, so that it
		// allows a long-period of sky rotation.
		skyangle += FLOAT2FIXED(s->GetTextureXOffset(pos));

		// Vertical offset allows careful sky positioning.
		skymid = s->GetTextureYOffset(pos);

		// We sometimes flip the picture horizontally.
		//
		// Doom always flipped the picture, so we make it optional,
		// to make it easier to use the new feature, while to still
		// allow old sky textures to be used.
		skyflip = l->args[2] ? false : true;

		int frontxscale = int(frontskytex->Scale.X * 1024);
		frontcyl = MAX(frontskytex->GetWidth(), frontxscale);
	}

	frontpos = int(fmod(frontdpos, sky1cyl * 65536.0));
	if (backskytex != nullptr)
	{
		backpos = int(fmod(backdpos, sky2cyl * 65536.0));
	}
}
