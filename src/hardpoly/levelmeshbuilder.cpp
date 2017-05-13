/*
**  Hardpoly renderer
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
#include "r_utility.h"
#include "levelmeshbuilder.h"
#include "v_palette.h"
#include "v_video.h"
#include "m_png.h"
#include "textures/textures.h"
#include "r_data/voxels.h"
#include "p_setup.h"
#include "r_utility.h"
#include "d_player.h"
#include "r_sky.h"
#include "g_levellocals.h"

void LevelMeshBuilder::Generate()
{
	Clear();
	ProcessBSP();
	Upload();
}

void LevelMeshBuilder::Generate(const std::vector<subsector_t*> &subsectors)
{
	Clear();
	for (auto subsector : subsectors)
		ProcessSubsector(subsector);
	Upload();
}

void LevelMeshBuilder::Clear()
{
	VertexArray.reset();

	DrawRuns.clear();
	for (auto &it : mMaterials)
	{
		it.second.Vertices.clear();
		it.second.Texcoords.clear();
	}
}

void LevelMeshBuilder::Upload()
{
	int totalVertices = 0;
	for (auto &it : mMaterials)
	{
		auto &material = it.second;
		totalVertices += (int)material.Vertices.size();
	}

	auto vertices = std::make_shared<GPUVertexBuffer>(nullptr, (int)(totalVertices * sizeof(Vec3f)));
	auto texcoords = std::make_shared<GPUVertexBuffer>(nullptr, (int)(totalVertices * sizeof(Vec4f)));

	Vec3f *cpuVertices = (Vec3f*)vertices->MapWriteOnly();
	Vec4f *cpuTexcoords = (Vec4f*)texcoords->MapWriteOnly();

	int vertexStart = 0;
	for (auto &it : mMaterials)
	{
		auto &material = it.second;
		if (!material.Vertices.empty())
		{
			LevelMeshDrawRun run;
			run.Start = vertexStart;
			run.NumVertices = (int)material.Vertices.size();
			run.Texture = it.first;
			DrawRuns.push_back(run);

			memcpy(cpuVertices + run.Start, material.Vertices.data(), run.NumVertices * sizeof(Vec3f));
			memcpy(cpuTexcoords + run.Start, material.Texcoords.data(), run.NumVertices * sizeof(Vec4f));

			vertexStart += run.NumVertices;
		}
	}

	vertices->Unmap();
	texcoords->Unmap();

	std::vector<GPUVertexAttributeDesc> attributes =
	{
		{ 0, 3, GPUVertexAttributeType::Float, false, 3 * sizeof(float), 0, vertices },
		{ 1, 4, GPUVertexAttributeType::Float, false, 4 * sizeof(float), 0, texcoords }
	};

	VertexArray = std::make_shared<GPUVertexArray>(attributes);
}

void LevelMeshBuilder::ProcessBSP()
{
	if (level.nodes.Size() == 0)
		ProcessSubsector(&level.subsectors[0]);
	else
		ProcessNode(level.HeadNode());	// The head node is the last node output.
}

void LevelMeshBuilder::ProcessNode(void *node)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		ProcessNode(bsp->children[0]);
		node = bsp->children[1];
	}

	subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
	ProcessSubsector(sub);
}

void LevelMeshBuilder::ProcessSubsector(subsector_t *sub)
{
	sector_t *frontsector = sub->sector;
	float sectornum = (float)frontsector->sectornum;
	
	ceilingVertices.clear();
	floorVertices.clear();

	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if (line->sidedef == nullptr || !(line->sidedef->Flags & WALLF_POLYOBJ))
		{
			double frontceilz1 = frontsector->ceilingplane.ZatPoint(line->v1);
			double frontfloorz1 = frontsector->floorplane.ZatPoint(line->v1);
			double frontceilz2 = frontsector->ceilingplane.ZatPoint(line->v2);
			double frontfloorz2 = frontsector->floorplane.ZatPoint(line->v2);
			double topTexZ = frontsector->GetPlaneTexZ(sector_t::ceiling);
			double bottomTexZ = frontsector->GetPlaneTexZ(sector_t::floor);

			ceilingVertices.push_back({ (float)line->v1->fX(), (float)line->v1->fY(), (float)frontceilz1 });
			floorVertices.push_back({ (float)line->v1->fX(), (float)line->v1->fY(), (float)frontfloorz1 });
			
			if (!line->backsector)
			{
				if (line->sidedef)
				{
					FTexture *texture = GetWallTexture(line->linedef, line->sidedef, side_t::mid);
					if (texture && texture->UseType == FTexture::TEX_Null) texture = nullptr;
					if (texture)
						ProcessWall(sectornum, texture, line, line->linedef, line->sidedef, side_t::mid, frontceilz1, frontfloorz1, frontceilz2, frontfloorz2, frontceilz1, frontceilz2, topTexZ, bottomTexZ, false);
				}
			}
			else
			{
				sector_t *backsector = (line->backsector != line->frontsector) ? line->backsector : line->frontsector;

				double backceilz1 = backsector->ceilingplane.ZatPoint(line->v1);
				double backfloorz1 = backsector->floorplane.ZatPoint(line->v1);
				double backceilz2 = backsector->ceilingplane.ZatPoint(line->v2);
				double backfloorz2 = backsector->floorplane.ZatPoint(line->v2);

				double topceilz1 = frontceilz1;
				double topceilz2 = frontceilz2;
				double topfloorz1 = MIN(backceilz1, frontceilz1);
				double topfloorz2 = MIN(backceilz2, frontceilz2);
				double bottomceilz1 = MAX(frontfloorz1, backfloorz1);
				double bottomceilz2 = MAX(frontfloorz2, backfloorz2);
				double bottomfloorz1 = frontfloorz1;
				double bottomfloorz2 = frontfloorz2;
				double middleceilz1 = topfloorz1;
				double middleceilz2 = topfloorz2;
				double middlefloorz1 = MIN(bottomceilz1, middleceilz1);
				double middlefloorz2 = MIN(bottomceilz2, middleceilz2);

				bool bothSkyCeiling = frontsector->GetTexture(sector_t::ceiling) == skyflatnum && backsector->GetTexture(sector_t::ceiling) == skyflatnum;

				if ((topceilz1 > topfloorz1 || topceilz2 > topfloorz2) && line->sidedef && !bothSkyCeiling)
				{
					FTexture *texture = GetWallTexture(line->linedef, line->sidedef, side_t::top);
					if (texture && texture->UseType == FTexture::TEX_Null) texture = nullptr;
					if (texture)
						ProcessWall(sectornum, texture, line, line->linedef, line->sidedef, side_t::top, topceilz1, topfloorz1, topceilz2, topfloorz2, frontceilz1, frontceilz2, topTexZ, MIN(topfloorz1, topfloorz2), false);
				}

				if ((bottomfloorz1 < bottomceilz1 || bottomfloorz2 < bottomceilz2) && line->sidedef)
				{
					FTexture *texture = GetWallTexture(line->linedef, line->sidedef, side_t::bottom);
					if (texture && texture->UseType == FTexture::TEX_Null) texture = nullptr;
					if (texture)
						ProcessWall(sectornum, texture, line, line->linedef, line->sidedef, side_t::bottom, bottomceilz1, bottomfloorz1, bottomceilz2, bottomfloorz2, frontceilz1, frontceilz2, MAX(bottomceilz1, bottomceilz2), bottomTexZ, false);
				}

				if (line->sidedef)
				{
					FTexture *texture = TexMan(line->sidedef->GetTexture(side_t::mid), true);
					if (texture && texture->UseType == FTexture::TEX_Null) texture = nullptr;
					if (texture)
						ProcessWall(sectornum, texture, line, line->linedef, line->sidedef, side_t::mid, middleceilz1, middlefloorz1, middleceilz2, middlefloorz2, frontceilz1, frontceilz2, MAX(middleceilz1, middleceilz2), MIN(middlefloorz1, middlefloorz2), true);
				}
			}
		}
	}
	
	FTexture *ceilingTexture = TexMan(frontsector->GetTexture(sector_t::ceiling));
	if (ceilingTexture && ceilingTexture->UseType == FTexture::TEX_Null) ceilingTexture = nullptr;
	if (ceilingTexture)
	{
		PlaneUVTransform planeUV(frontsector->planes[sector_t::ceiling].xform, ceilingTexture);

		auto &ceilingRun = mMaterials[ceilingTexture];
		for (size_t i = 2; i < ceilingVertices.size(); i++)
		{
			ceilingRun.Vertices.push_back(ceilingVertices[i]);
			ceilingRun.Vertices.push_back(ceilingVertices[i - 1]);
			ceilingRun.Vertices.push_back(ceilingVertices[0]);
			ceilingRun.Texcoords.push_back(Vec4f(planeUV.GetUV(ceilingVertices[i]), sectornum, 0.0f));
			ceilingRun.Texcoords.push_back(Vec4f(planeUV.GetUV(ceilingVertices[i - 1]), sectornum, 0.0f));
			ceilingRun.Texcoords.push_back(Vec4f(planeUV.GetUV(ceilingVertices[0]), sectornum, 0.0f));
		}
	}
	
	FTexture *floorTexture = TexMan(frontsector->GetTexture(sector_t::floor));
	if (floorTexture && floorTexture->UseType == FTexture::TEX_Null) floorTexture = nullptr;
	if (floorTexture)
	{
		PlaneUVTransform planeUV(frontsector->planes[sector_t::floor].xform, floorTexture);

		auto &floorRun = mMaterials[floorTexture];
		for (size_t i = 2; i < floorVertices.size(); i++)
		{
			floorRun.Vertices.push_back(floorVertices[0]);
			floorRun.Vertices.push_back(floorVertices[i - 1]);
			floorRun.Vertices.push_back(floorVertices[i]);
			floorRun.Texcoords.push_back(Vec4f(planeUV.GetUV(floorVertices[0]), sectornum, 0.0f));
			floorRun.Texcoords.push_back(Vec4f(planeUV.GetUV(floorVertices[i - 1]), sectornum, 0.0f));
			floorRun.Texcoords.push_back(Vec4f(planeUV.GetUV(floorVertices[i]), sectornum, 0.0f));
		}
	}
}

void LevelMeshBuilder::ProcessWall(float sectornum, FTexture *texture, const seg_t *lineseg, const line_t *line, const side_t *side, side_t::ETexpart texpart, double ceilz1, double floorz1, double ceilz2, double floorz2, double unpeggedceil1, double unpeggedceil2, double topTexZ, double bottomTexZ, bool masked)
{
	DVector2 v1 = lineseg->v1->fPos();
	DVector2 v2 = lineseg->v2->fPos();

	WallTextureCoordsU texcoordsU(texture, lineseg, line, side, texpart);
	WallTextureCoordsV texcoordsVLeft(texture, line, side, texpart, ceilz1, floorz1, unpeggedceil1, topTexZ, bottomTexZ);
	WallTextureCoordsV texcoordsVRght(texture, line, side, texpart, ceilz2, floorz2, unpeggedceil2, topTexZ, bottomTexZ);

	Vec3f vertices[4] =
	{
		{ (float)v1.X, (float)v1.Y, (float)ceilz1 },
		{ (float)v2.X, (float)v2.Y, (float)ceilz2 },
		{ (float)v1.X, (float)v1.Y, (float)floorz1 },
		{ (float)v2.X, (float)v2.Y, (float)floorz2 }
	};

	Vec4f texcoords[4] =
	{
		{ (float)texcoordsU.u1, (float)texcoordsVLeft.v1, sectornum, 0.0f },
		{ (float)texcoordsU.u2, (float)texcoordsVRght.v1, sectornum, 0.0f },
		{ (float)texcoordsU.u1, (float)texcoordsVLeft.v2, sectornum, 0.0f },
		{ (float)texcoordsU.u2, (float)texcoordsVRght.v2, sectornum, 0.0f }
	};

	// Masked walls clamp to the 0-1 range (no texture repeat)
	if (masked)
	{
		ClampWallHeight(vertices[0], vertices[3], texcoords[0], texcoords[3]);
		ClampWallHeight(vertices[1], vertices[2], texcoords[1], texcoords[2]);
	}

	auto &run = mMaterials[texture];

	run.Vertices.push_back(vertices[0]);
	run.Vertices.push_back(vertices[1]);
	run.Vertices.push_back(vertices[2]);
	
	run.Vertices.push_back(vertices[3]);
	run.Vertices.push_back(vertices[2]);
	run.Vertices.push_back(vertices[1]);

	run.Texcoords.push_back(texcoords[0]);
	run.Texcoords.push_back(texcoords[1]);
	run.Texcoords.push_back(texcoords[2]);
	
	run.Texcoords.push_back(texcoords[3]);
	run.Texcoords.push_back(texcoords[2]);
	run.Texcoords.push_back(texcoords[1]);
}

void LevelMeshBuilder::ClampWallHeight(Vec3f &v1, Vec3f &v2, Vec4f &uv1, Vec4f &uv2)
{
	float top = v1.Z;
	float bottom = v2.Z;
	float texv1 = uv1.Y;
	float texv2 = uv2.Y;
	float delta = (texv2 - texv1);

	float t1 = texv1 < 0.0f ? -texv1 / delta : 0.0f;
	float t2 = texv2 > 1.0f ? (1.0f - texv1) / delta : 1.0f;
	float inv_t1 = 1.0f - t1;
	float inv_t2 = 1.0f - t2;

	v1.Z = top * inv_t1 + bottom * t1;
	uv1.Y = texv1 * inv_t1 + texv2 * t1;

	v2.Z = top * inv_t2 + bottom * t2;
	uv2.Y = texv1 * inv_t2 + texv2 * t2;

	// Enable alpha test
	uv1.W = 0.5f;
	uv2.W = 0.5f;
}

FTexture *LevelMeshBuilder::GetWallTexture(line_t *line, side_t *side, side_t::ETexpart texpart)
{
	FTexture *tex = TexMan(side->GetTexture(texpart), true);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
	{
		// Mapping error. Doom floodfills this with a plane.
		// This code doesn't do that, but at least it uses the "right" texture..

		if (line && line->backsector && line->sidedef[0] == side)
		{
			if (texpart == side_t::top)
				tex = TexMan(line->backsector->GetTexture(sector_t::ceiling), true);
			else if (texpart == side_t::bottom)
				tex = TexMan(line->backsector->GetTexture(sector_t::floor), true);
		}
		if (line && line->backsector && line->sidedef[1] == side)
		{
			if (texpart == side_t::top)
				tex = TexMan(line->frontsector->GetTexture(sector_t::ceiling), true);
			else if (texpart == side_t::bottom)
				tex = TexMan(line->frontsector->GetTexture(sector_t::floor), true);
		}

		if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
			return nullptr;
	}
	return tex;
}

/////////////////////////////////////////////////////////////////////////////

PlaneUVTransform::PlaneUVTransform(const FTransform &transform, FTexture *tex)
{
	if (tex)
	{
		xscale = (float)(transform.xScale * tex->Scale.X / tex->GetWidth());
		yscale = (float)(transform.yScale * tex->Scale.Y / tex->GetHeight());

		double planeang = (transform.Angle + transform.baseAngle).Radians();
		cosine = (float)cos(planeang);
		sine = (float)sin(planeang);

		xOffs = (float)transform.xOffs;
		yOffs = (float)transform.yOffs;
	}
	else
	{
		xscale = 1.0f / 64.0f;
		yscale = 1.0f / 64.0f;
		cosine = 1.0f;
		sine = 0.0f;
		xOffs = 0.0f;
		yOffs = 0.0f;
	}
}

Vec2f PlaneUVTransform::GetUV(const Vec3f &pos) const
{
	return { GetU(pos.X, pos.Y), GetV(pos.X, pos.Y) };
}

float PlaneUVTransform::GetU(float x, float y) const
{
	return (xOffs + x * cosine - y * sine) * xscale;
}

float PlaneUVTransform::GetV(float x, float y) const
{
	return (yOffs - x * sine - y * cosine) * yscale;
}

/////////////////////////////////////////////////////////////////////////////

WallTextureCoordsU::WallTextureCoordsU(FTexture *tex, const seg_t *lineseg, const line_t *line, const side_t *side, side_t::ETexpart texpart)
{
	double lineLength = side->TexelLength;
	double lineStart = 0.0;

	bool entireSegment = ((lineseg->v1 == line->v1) && (lineseg->v2 == line->v2)) || ((lineseg->v2 == line->v1) && (lineseg->v1 == line->v2));
	if (!entireSegment)
	{
		lineLength = (lineseg->v2->fPos() - lineseg->v1->fPos()).Length();
		lineStart = (lineseg->v1->fPos() - line->v1->fPos()).Length();
	}

	int texWidth = tex->GetWidth();
	double uscale = side->GetTextureXScale(texpart) * tex->Scale.X;
	u1 = lineStart + side->GetTextureXOffset(texpart);
	u2 = u1 + lineLength;
	u1 *= uscale;
	u2 *= uscale;
	u1 /= texWidth;
	u2 /= texWidth;
}

/////////////////////////////////////////////////////////////////////////////

WallTextureCoordsV::WallTextureCoordsV(FTexture *tex, const line_t *line, const side_t *side, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil, double topTexZ, double bottomTexZ)
{
	double vscale = side->GetTextureYScale(texpart) * tex->Scale.Y;

	double yoffset = side->GetTextureYOffset(texpart);
	if (tex->bWorldPanning)
		yoffset *= vscale;

	switch (texpart)
	{
	default:
	case side_t::mid:
		CalcVMidPart(tex, line, side, topTexZ, bottomTexZ, vscale, yoffset);
		break;
	case side_t::top:
		CalcVTopPart(tex, line, side, topTexZ, bottomTexZ, vscale, yoffset);
		break;
	case side_t::bottom:
		CalcVBottomPart(tex, line, side, topTexZ, bottomTexZ, unpeggedceil, vscale, yoffset);
		break;
	}

	int texHeight = tex->GetHeight();
	v1 /= texHeight;
	v2 /= texHeight;

	double texZHeight = (bottomTexZ - topTexZ);
	if (texZHeight > 0.0f || texZHeight < -0.0f)
	{
		double t1 = (topz - topTexZ) / texZHeight;
		double t2 = (bottomz - topTexZ) / texZHeight;
		double vorig1 = v1;
		double vorig2 = v2;
		v1 = vorig1 * (1.0f - t1) + vorig2 * t1;
		v2 = vorig1 * (1.0f - t2) + vorig2 * t2;
	}
}

void WallTextureCoordsV::CalcVTopPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double vscale, double yoffset)
{
	bool pegged = (line->flags & ML_DONTPEGTOP) == 0;
	if (pegged) // bottom to top
	{
		int texHeight = tex->GetHeight();
		v1 = -yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
		v1 = texHeight - v1;
		v2 = texHeight - v2;
		std::swap(v1, v2);
	}
	else // top to bottom
	{
		v1 = yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
	}
}

void WallTextureCoordsV::CalcVMidPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double vscale, double yoffset)
{
	bool pegged = (line->flags & ML_DONTPEGBOTTOM) == 0;
	if (pegged) // top to bottom
	{
		v1 = yoffset * vscale;
		v2 = (yoffset + (topz - bottomz)) * vscale;
	}
	else // bottom to top
	{
		int texHeight = tex->GetHeight();
		v1 = texHeight - (-yoffset + (topz - bottomz)) * vscale;
		v2 = texHeight + yoffset * vscale;
	}
}

void WallTextureCoordsV::CalcVBottomPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double unpeggedceil, double vscale, double yoffset)
{
	bool pegged = (line->flags & ML_DONTPEGBOTTOM) == 0;
	if (pegged) // top to bottom
	{
		v1 = yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
	}
	else
	{
		v1 = yoffset + (unpeggedceil - topz);
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
	}
}
