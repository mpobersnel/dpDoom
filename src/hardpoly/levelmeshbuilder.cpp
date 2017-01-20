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
	
	auto vertices = std::make_shared<GPUVertexBuffer>(mCPUVertices.data(), (int)(mCPUVertices.size() * sizeof(Vec3f)));
	auto texcoords = std::make_shared<GPUVertexBuffer>(mCPUTexcoords.data(), (int)(mCPUTexcoords.size() * sizeof(Vec2f)));
	
	std::vector<GPUVertexAttributeDesc> attributes =
	{
		{ 0, 3, GPUVertexAttributeType::Float, false, 3 * sizeof(float), 0, vertices },
		{ 1, 2, GPUVertexAttributeType::Float, false, 2 * sizeof(float), 0, texcoords }
	};
	
	VertexArray = std::make_shared<GPUVertexArray>(attributes);
	NumVertices = (int)mCPUVertices.size();
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
				ProcessWall(line->v1->fPos(), line->v2->fPos(), frontceilz1, frontfloorz1, frontceilz2, frontfloorz2);
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
					ProcessWall(line->v1->fPos(), line->v2->fPos(), topceilz1, topfloorz1, topceilz2, topfloorz2);
				}

				if ((bottomfloorz1 < bottomceilz1 || bottomfloorz2 < bottomceilz2) && line->sidedef)
				{
					ProcessWall(line->v1->fPos(), line->v2->fPos(), bottomceilz1, bottomfloorz1, bottomceilz2, bottomfloorz2);
				}
			}
		}
	}
	
	for (size_t i = 2; i < ceilingVertices.size(); i++)
	{
		mCPUVertices.push_back(ceilingVertices[i]);
		mCPUVertices.push_back(ceilingVertices[i - 1]);
		mCPUVertices.push_back(ceilingVertices[0]);
		mCPUTexcoords.push_back({ 0.0f, 1.0f });
		mCPUTexcoords.push_back({ 1.0f, 0.0f });
		mCPUTexcoords.push_back({ 0.0f, 0.0f });
	}
	
	for (size_t i = 2; i < floorVertices.size(); i++)
	{
		mCPUVertices.push_back(floorVertices[0]);
		mCPUVertices.push_back(floorVertices[i - 1]);
		mCPUVertices.push_back(floorVertices[i]);
		mCPUTexcoords.push_back({ 0.0f, 0.0f });
		mCPUTexcoords.push_back({ 1.0f, 0.0f });
		mCPUTexcoords.push_back({ 0.0f, 1.0f });
	}
}

void LevelMeshBuilder::ProcessWall(const DVector2 &v1, const DVector2 &v2, double ceilz1, double floorz1, double ceilz2, double floorz2)
{
	mCPUVertices.push_back({ (float)v1.X, (float)v1.Y, (float)ceilz1 });
	mCPUVertices.push_back({ (float)v2.X, (float)v2.Y, (float)ceilz2 });
	mCPUVertices.push_back({ (float)v1.X, (float)v1.Y, (float)floorz1 });
	
	mCPUVertices.push_back({ (float)v2.X, (float)v2.Y, (float)floorz2 });
	mCPUVertices.push_back({ (float)v1.X, (float)v1.Y, (float)floorz1 });
	mCPUVertices.push_back({ (float)v2.X, (float)v2.Y, (float)ceilz2 });
	
	mCPUTexcoords.push_back({ 0.0f, 0.0f });
	mCPUTexcoords.push_back({ 1.0f, 0.0f });
	mCPUTexcoords.push_back({ 0.0f, 1.0f });
	
	mCPUTexcoords.push_back({ 1.0f, 1.0f });
	mCPUTexcoords.push_back({ 0.0f, 1.0f });
	mCPUTexcoords.push_back({ 1.0f, 0.0f });
}
