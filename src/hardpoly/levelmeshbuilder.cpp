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

void LevelMeshBuilder::Generate()
{
	ProcessBSP();

	std::vector<Vec3f> cpuVertices;
	std::vector<Vec2f> cpuTexcoords;

	for (auto it : mMaterials)
	{
		auto &material = it.second;

		LevelMeshDrawRun run;
		run.Start = (int)cpuVertices.size();
		run.NumVertices = (int)material.Vertices.size();
		run.Texture = it.first;
		DrawRuns.push_back(run);

		cpuVertices.insert(cpuVertices.end(), material.Vertices.begin(), material.Vertices.end());
		cpuTexcoords.insert(cpuTexcoords.end(), material.Texcoords.begin(), material.Texcoords.end());
	}

	auto vertices = std::make_shared<GPUVertexBuffer>(cpuVertices.data(), (int)(cpuVertices.size() * sizeof(Vec3f)));
	auto texcoords = std::make_shared<GPUVertexBuffer>(cpuTexcoords.data(), (int)(cpuTexcoords.size() * sizeof(Vec2f)));
	
	std::vector<GPUVertexAttributeDesc> attributes =
	{
		{ 0, 3, GPUVertexAttributeType::Float, false, 3 * sizeof(float), 0, vertices },
		{ 1, 2, GPUVertexAttributeType::Float, false, 2 * sizeof(float), 0, texcoords }
	};
	
	VertexArray = std::make_shared<GPUVertexArray>(attributes);
}

void LevelMeshBuilder::ProcessBSP()
{
	if (numnodes == 0)
		ProcessSubsector(subsectors);
	else
		ProcessNode(nodes + numnodes - 1);	// The head node is the last node output.
}

void LevelMeshBuilder::ProcessNode(void *node)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		ProcessNode(bsp->children[0]);
		node = bsp->children[1];
	}

	subsector_t *sub = (subsector_t *)((BYTE *)node - 1);
	ProcessSubsector(sub);
}

void LevelMeshBuilder::ProcessSubsector(subsector_t *sub)
{
	sector_t *frontsector = sub->sector;
	
	std::vector<Vec3f> ceilingVertices;
	std::vector<Vec3f> floorVertices;
	
	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if (line->sidedef == nullptr || !(line->sidedef->Flags & WALLF_POLYOBJ))
		{
			double frontceilz1 = frontsector->ceilingplane.ZatPoint(line->v1);
			double frontfloorz1 = frontsector->floorplane.ZatPoint(line->v1);
			double frontceilz2 = frontsector->ceilingplane.ZatPoint(line->v2);
			double frontfloorz2 = frontsector->floorplane.ZatPoint(line->v2);
			
			ceilingVertices.push_back({ (float)line->v1->fX(), (float)line->v1->fY(), (float)frontceilz1 });
			floorVertices.push_back({ (float)line->v1->fX(), (float)line->v1->fY(), (float)frontfloorz1 });
			
			if (!line->backsector)
			{
				FTexture *texture = GetWallTexture(line->linedef, line->sidedef, side_t::mid);
				ProcessWall(texture, line->v1->fPos(), line->v2->fPos(), frontceilz1, frontfloorz1, frontceilz2, frontfloorz2);
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
					ProcessWall(texture, line->v1->fPos(), line->v2->fPos(), topceilz1, topfloorz1, topceilz2, topfloorz2);
				}

				if ((bottomfloorz1 < bottomceilz1 || bottomfloorz2 < bottomceilz2) && line->sidedef)
				{
					FTexture *texture = GetWallTexture(line->linedef, line->sidedef, side_t::bottom);
					ProcessWall(texture, line->v1->fPos(), line->v2->fPos(), bottomceilz1, bottomfloorz1, bottomceilz2, bottomfloorz2);
				}
			}
		}
	}
	
	FTexture *ceilingTexture = TexMan(frontsector->GetTexture(sector_t::ceiling));
	if (ceilingTexture->UseType == FTexture::TEX_Null) ceilingTexture = nullptr;

	auto &ceilingRun = mMaterials[ceilingTexture];
	for (size_t i = 2; i < ceilingVertices.size(); i++)
	{
		ceilingRun.Vertices.push_back(ceilingVertices[i]);
		ceilingRun.Vertices.push_back(ceilingVertices[i - 1]);
		ceilingRun.Vertices.push_back(ceilingVertices[0]);
		ceilingRun.Texcoords.push_back({ 0.0f, 1.0f });
		ceilingRun.Texcoords.push_back({ 1.0f, 0.0f });
		ceilingRun.Texcoords.push_back({ 0.0f, 0.0f });
	}
	
	FTexture *floorTexture = TexMan(frontsector->GetTexture(sector_t::floor));
	if (floorTexture->UseType == FTexture::TEX_Null) floorTexture = nullptr;

	auto &floorRun = mMaterials[floorTexture];
	for (size_t i = 2; i < floorVertices.size(); i++)
	{
		floorRun.Vertices.push_back(floorVertices[0]);
		floorRun.Vertices.push_back(floorVertices[i - 1]);
		floorRun.Vertices.push_back(floorVertices[i]);
		floorRun.Texcoords.push_back({ 0.0f, 0.0f });
		floorRun.Texcoords.push_back({ 1.0f, 0.0f });
		floorRun.Texcoords.push_back({ 0.0f, 1.0f });
	}
}

void LevelMeshBuilder::ProcessWall(FTexture *texture, const DVector2 &v1, const DVector2 &v2, double ceilz1, double floorz1, double ceilz2, double floorz2)
{
	auto &run = mMaterials[texture];

	run.Vertices.push_back({ (float)v1.X, (float)v1.Y, (float)ceilz1 });
	run.Vertices.push_back({ (float)v2.X, (float)v2.Y, (float)ceilz2 });
	run.Vertices.push_back({ (float)v1.X, (float)v1.Y, (float)floorz1 });
	
	run.Vertices.push_back({ (float)v2.X, (float)v2.Y, (float)floorz2 });
	run.Vertices.push_back({ (float)v1.X, (float)v1.Y, (float)floorz1 });
	run.Vertices.push_back({ (float)v2.X, (float)v2.Y, (float)ceilz2 });
	
	run.Texcoords.push_back({ 0.0f, 0.0f });
	run.Texcoords.push_back({ 1.0f, 0.0f });
	run.Texcoords.push_back({ 0.0f, 1.0f });
	
	run.Texcoords.push_back({ 1.0f, 1.0f });
	run.Texcoords.push_back({ 0.0f, 1.0f });
	run.Texcoords.push_back({ 1.0f, 0.0f });
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
