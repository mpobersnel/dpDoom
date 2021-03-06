/*
**  Polygon Doom software renderer
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
#include "p_maputl.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "polyrenderer/poly_renderer.h"
#include "polyrenderer/scene/poly_scene.h"
#include "polyrenderer/scene/poly_light.h"
#include "polyrenderer/scene/poly_wall.h"
#include "polyrenderer/scene/poly_wallsprite.h"
#include "polyrenderer/scene/poly_plane.h"
#include "polyrenderer/scene/poly_particle.h"
#include "polyrenderer/scene/poly_sprite.h"

EXTERN_CVAR(Int, r_portal_recursions)

/////////////////////////////////////////////////////////////////////////////

RenderPolyScene::RenderPolyScene()
{
}

RenderPolyScene::~RenderPolyScene()
{
}

void RenderPolyScene::Render(PolyPortalViewpoint *viewpoint)
{
	CurrentViewpoint = viewpoint;

	PolyRenderThread *thread = PolyRenderer::Instance()->Threads.MainThread();

	CurrentViewpoint->ObjectsStart = thread->TranslucentObjects.size();
	CurrentViewpoint->SectorPortalsStart = thread->SectorPortals.size();
	CurrentViewpoint->LinePortalsStart = thread->LinePortals.size();

	PolyCullCycles.Clock();
	Cull.CullScene(CurrentViewpoint->PortalPlane);
	PolyCullCycles.Unclock();

	RenderSectors();

	PolyMaskedCycles.Clock();
	const auto &rviewpoint = PolyRenderer::Instance()->Viewpoint;
	for (sector_t *sector : Cull.SeenSectors)
	{
		for (AActor *thing = sector->thinglist; thing != nullptr; thing = thing->snext)
		{
			DVector2 left, right;
			if (!RenderPolySprite::GetLine(thing, left, right))
				continue;
			double distanceSquared = (thing->Pos() - rviewpoint.Pos).LengthSquared();
			AddSprite(thread, thing, distanceSquared, left, right);
		}
	}
	PolyMaskedCycles.Unclock();

	CurrentViewpoint->ObjectsEnd = thread->TranslucentObjects.size();
	CurrentViewpoint->SectorPortalsEnd = thread->SectorPortals.size();
	CurrentViewpoint->LinePortalsEnd = thread->LinePortals.size();

	RenderPortals();

	CurrentViewpoint = nullptr;
}

void RenderPolyScene::RenderSectors()
{
	PolyRenderThread *mainthread = PolyRenderer::Instance()->Threads.MainThread();

	int totalcount = (int)Cull.PvsSectors.size();
	auto subsectors = Cull.PvsSectors.data();

	PolyOpaqueCycles.Clock();

	PolyRenderer::Instance()->Threads.RenderThreadSlices(totalcount, [&](PolyRenderThread *thread)
	{
		if (PolyRenderer::Instance()->RedirectToHardpoly)
		{
			thread->DrawBatcher.WorldToView = CurrentViewpoint->WorldToView;
			thread->DrawBatcher.MatrixUpdated();
		}
		else
		{
			PolyTriangleDrawer::SetTransform(thread->DrawQueue, thread->FrameMemory->NewObject<Mat4f>(CurrentViewpoint->WorldToClip));
		}

		if (thread != mainthread)
		{
			thread->TranslucentObjects.clear();
			thread->SectorPortals.clear();
			thread->LinePortals.clear();
		}

		int start = thread->Start;
		int end = thread->End;
		for (int i = start; i < end; i++)
		{
			RenderSubsector(thread, subsectors[i], i);
		}
	}, [&](PolyRenderThread *thread)
	{
		const auto &objects = thread->TranslucentObjects;
		mainthread->TranslucentObjects.insert(mainthread->TranslucentObjects.end(), objects.begin(), objects.end());
	});

	PolyOpaqueCycles.Unclock();
}

void RenderPolyScene::RenderSubsector(PolyRenderThread *thread, subsector_t *sub, uint32_t subsectorDepth)
{
	sector_t *frontsector = sub->sector;
	frontsector->MoreFlags |= SECF_DRAWN;

	bool mainBSP = sub->polys == nullptr;

	if (sub->polys)
	{
		if (sub->BSP == nullptr || sub->BSP->bDirty)
		{
			sub->BuildPolyBSP();

			// This is done by the GL renderer, but not the sw renderer. No idea what the purpose is..
			for (unsigned i = 0; i < sub->BSP->Segs.Size(); i++)
			{
				sub->BSP->Segs[i].Subsector = sub;
				sub->BSP->Segs[i].PartnerSeg = nullptr;
			}
		}

		if (sub->BSP->Nodes.Size() == 0)
		{
			RenderPolySubsector(thread, &sub->BSP->Subsectors[0], subsectorDepth, frontsector);
		}
		else
		{
			RenderPolyNode(thread, &sub->BSP->Nodes.Last(), subsectorDepth, frontsector);
		}

		Render3DFloorPlane::RenderPlanes(thread, CurrentViewpoint->PortalPlane, sub, CurrentViewpoint->StencilValue, subsectorDepth, thread->TranslucentObjects);
		RenderPolyPlane::RenderPlanes(thread, CurrentViewpoint->PortalPlane, sub, CurrentViewpoint->StencilValue, Cull.MaxCeilingHeight, Cull.MinFloorHeight, thread->SectorPortals);
	}
	else
	{
		PolyTransferHeights fakeflat(sub);

		Render3DFloorPlane::RenderPlanes(thread, CurrentViewpoint->PortalPlane, sub, CurrentViewpoint->StencilValue, subsectorDepth, thread->TranslucentObjects);
		RenderPolyPlane::RenderPlanes(thread, CurrentViewpoint->PortalPlane, fakeflat, CurrentViewpoint->StencilValue, Cull.MaxCeilingHeight, Cull.MinFloorHeight, thread->SectorPortals);

		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			if (Cull.IsLineSegVisible(subsectorDepth, i))
			{
				seg_t *line = &sub->firstline[i];
				RenderLine(thread, sub, line, fakeflat.FrontSector, subsectorDepth);
			}
		}
	}

	if (mainBSP)
	{
		int subsectorIndex = sub->Index();
		for (int i = ParticlesInSubsec[subsectorIndex]; i != NO_PARTICLE; i = Particles[i].snext)
		{
			particle_t *particle = Particles + i;
			thread->TranslucentObjects.push_back(thread->FrameMemory->NewObject<PolyTranslucentParticle>(particle, sub, subsectorDepth, CurrentViewpoint->StencilValue));
		}
	}
}

void RenderPolyScene::RenderPolyNode(PolyRenderThread *thread, void *node, uint32_t subsectorDepth, sector_t *frontsector)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = PointOnSide(PolyRenderer::Instance()->Viewpoint.Pos, bsp);

		// Recursively divide front space (toward the viewer).
		RenderPolyNode(thread, bsp->children[side], subsectorDepth, frontsector);

		// Possibly divide back space (away from the viewer).
		side ^= 1;

		// Don't bother culling on poly objects
		//if (!CheckBBox(bsp->bbox[side]))
		//	return;

		node = bsp->children[side];
	}

	subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
	RenderPolySubsector(thread, sub, subsectorDepth, frontsector);
}

void RenderPolyScene::RenderPolySubsector(PolyRenderThread *thread, subsector_t *sub, uint32_t subsectorDepth, sector_t *frontsector)
{
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;

	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if (line->linedef)
		{
			// Reject lines not facing viewer
			DVector2 pt1 = line->v1->fPos() - viewpoint.Pos;
			DVector2 pt2 = line->v2->fPos() - viewpoint.Pos;
			if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
				continue;

			// Tell automap we saw this
			if (!PolyRenderer::Instance()->DontMapLines && line->linedef)
			{
				line->linedef->flags |= ML_MAPPED;
				sub->flags |= SSECF_DRAWN;
			}

			RenderPolyWall::RenderLine(thread, CurrentViewpoint->PortalPlane, line, frontsector, subsectorDepth, CurrentViewpoint->StencilValue, thread->TranslucentObjects, thread->LinePortals, CurrentViewpoint->LastPortalLine);
		}
	}
}

int RenderPolyScene::PointOnSide(const DVector2 &pos, const node_t *node)
{
	return DMulScale32(FLOAT2FIXED(pos.Y) - node->y, node->dx, node->x - FLOAT2FIXED(pos.X), node->dy) > 0;
}

void RenderPolyScene::AddSprite(PolyRenderThread *thread, AActor *thing, double sortDistance, const DVector2 &left, const DVector2 &right)
{
	if (level.nodes.Size() == 0)
	{
		subsector_t *sub = &level.subsectors[0];
		if (Cull.SubsectorDepths[sub->Index()] != 0xffffffff)
			thread->TranslucentObjects.push_back(thread->FrameMemory->NewObject<PolyTranslucentThing>(thing, sub, Cull.SubsectorDepths[sub->Index()], sortDistance, 0.0f, 1.0f, CurrentViewpoint->StencilValue));
	}
	else
	{
		AddSprite(thread, thing, sortDistance, left, right, 0.0, 1.0, level.HeadNode());
	}
}

void RenderPolyScene::AddSprite(PolyRenderThread *thread, AActor *thing, double sortDistance, DVector2 left, DVector2 right, double t1, double t2, void *node)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;
		
		DVector2 planePos(FIXED2DBL(bsp->x), FIXED2DBL(bsp->y));
		DVector2 planeNormal = DVector2(FIXED2DBL(-bsp->dy), FIXED2DBL(bsp->dx));
		double planeD = planeNormal | planePos;
		
		int sideLeft = (left | planeNormal) > planeD;
		int sideRight = (right | planeNormal) > planeD;
		
		if (sideLeft != sideRight)
		{
			double dotLeft = planeNormal | left;
			double dotRight = planeNormal | right;
			double t = (planeD - dotLeft) / (dotRight - dotLeft);
			
			DVector2 mid = left * (1.0 - t) + right * t;
			double tmid = t1 * (1.0 - t) + t2 * t;
			
			AddSprite(thread, thing, sortDistance, mid, right, tmid, t2, bsp->children[sideRight]);
			right = mid;
			t2 = tmid;
		}
		node = bsp->children[sideLeft];
	}
	
	subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
	
	if (Cull.SubsectorDepths[sub->Index()] != 0xffffffff)
		thread->TranslucentObjects.push_back(thread->FrameMemory->NewObject<PolyTranslucentThing>(thing, sub, Cull.SubsectorDepths[sub->Index()], sortDistance, (float)t1, (float)t2, CurrentViewpoint->StencilValue));
}

void RenderPolyScene::RenderLine(PolyRenderThread *thread, subsector_t *sub, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth)
{
	// Tell automap we saw this
	if (!PolyRenderer::Instance()->DontMapLines && line->linedef)
	{
		line->linedef->flags |= ML_MAPPED;
		sub->flags |= SSECF_DRAWN;
	}

	// Render 3D floor sides
	if (line->sidedef && line->backsector && line->backsector->e && line->backsector->e->XFloor.ffloors.Size())
	{
		for (unsigned int i = 0; i < line->backsector->e->XFloor.ffloors.Size(); i++)
		{
			F3DFloor *fakeFloor = line->backsector->e->XFloor.ffloors[i];
			RenderPolyWall::Render3DFloorLine(thread, CurrentViewpoint->PortalPlane, line, frontsector, subsectorDepth, CurrentViewpoint->StencilValue, fakeFloor, thread->TranslucentObjects);
		}
	}

	// Render wall, and update culling info if its an occlusion blocker
	RenderPolyWall::RenderLine(thread, CurrentViewpoint->PortalPlane, line, frontsector, subsectorDepth, CurrentViewpoint->StencilValue, thread->TranslucentObjects, thread->LinePortals, CurrentViewpoint->LastPortalLine);
}

void RenderPolyScene::RenderPortals()
{
	PolyRenderThread *thread = PolyRenderer::Instance()->Threads.MainThread();

	bool foggy = false;
	if (CurrentViewpoint->PortalDepth < r_portal_recursions)
	{
		for (auto &portal : thread->SectorPortals)
			portal->Render(CurrentViewpoint->PortalDepth + 1);

		for (auto &portal : thread->LinePortals)
			portal->Render(CurrentViewpoint->PortalDepth + 1);
	}
	else // Fill with black
	{
		PolyDrawArgs args;
		args.SetLight(&NormalLight, 255, PolyRenderer::Instance()->Light.WallGlobVis(foggy), true);
		args.SetColor(0, 0);
		args.SetClipPlane(0, CurrentViewpoint->PortalPlane);
		args.SetStyle(TriBlendMode::FillOpaque);

		for (auto &portal : thread->SectorPortals)
		{
			args.SetStencilTestValue(portal->StencilValue);
			args.SetWriteStencil(true, portal->StencilValue + 1);
			for (const auto &verts : portal->Shape)
			{
				args.DrawArray(thread, verts.Vertices, verts.Count, PolyDrawMode::TriangleFan);
			}
		}

		for (auto &portal : thread->LinePortals)
		{
			args.SetStencilTestValue(portal->StencilValue);
			args.SetWriteStencil(true, portal->StencilValue + 1);
			for (const auto &verts : portal->Shape)
			{
				args.DrawArray(thread, verts.Vertices, verts.Count, PolyDrawMode::TriangleFan);
			}
		}
	}
}

void RenderPolyScene::RenderTranslucent(PolyPortalViewpoint *viewpoint)
{
	CurrentViewpoint = viewpoint;

	PolyRenderThread *thread = PolyRenderer::Instance()->Threads.MainThread();

	if (PolyRenderer::Instance()->RedirectToHardpoly)
	{
		thread->DrawBatcher.WorldToView = CurrentViewpoint->WorldToView;
		thread->DrawBatcher.MatrixUpdated();
	}
	else
	{
		PolyTriangleDrawer::SetTransform(thread->DrawQueue, thread->FrameMemory->NewObject<Mat4f>(CurrentViewpoint->WorldToClip));
	}

	if (CurrentViewpoint->PortalDepth < r_portal_recursions)
	{
		for (auto it = thread->SectorPortals.rbegin(); it != thread->SectorPortals.rend(); ++it)
		{
			auto &portal = *it;
			portal->RenderTranslucent();
		
			PolyDrawArgs args;
			args.SetStencilTestValue(portal->StencilValue + 1);
			args.SetWriteStencil(true, CurrentViewpoint->StencilValue + 1);
			args.SetClipPlane(0, CurrentViewpoint->PortalPlane);
			for (const auto &verts : portal->Shape)
			{
				args.SetWriteColor(false);
				args.DrawArray(thread, verts.Vertices, verts.Count, PolyDrawMode::TriangleFan);
			}
		}

		for (auto it = thread->LinePortals.rbegin(); it != thread->LinePortals.rend(); ++it)
		{
			auto &portal = *it;
			portal->RenderTranslucent();
		
			PolyDrawArgs args;
			args.SetStencilTestValue(portal->StencilValue + 1);
			args.SetWriteStencil(true, CurrentViewpoint->StencilValue + 1);
			args.SetClipPlane(0, CurrentViewpoint->PortalPlane);
			for (const auto &verts : portal->Shape)
			{
				args.SetWriteColor(false);
				args.DrawArray(thread, verts.Vertices, verts.Count, PolyDrawMode::TriangleFan);
			}
		}
	}

	PolyMaskedCycles.Clock();

	// Draw all translucent objects back to front
	if (CurrentViewpoint->ObjectsEnd > CurrentViewpoint->ObjectsStart)
	{
		std::stable_sort(
			thread->TranslucentObjects.begin() + CurrentViewpoint->ObjectsStart,
			thread->TranslucentObjects.begin() + CurrentViewpoint->ObjectsEnd,
			[](auto a, auto b) { return *a < *b; });

		size_t i = CurrentViewpoint->ObjectsEnd - 1;
		size_t start = CurrentViewpoint->ObjectsStart;
		auto objects = thread->TranslucentObjects.data();
		while (true)
		{
			PolyTranslucentObject *obj = objects[i];
			obj->Render(thread, CurrentViewpoint->PortalPlane);
			obj->~PolyTranslucentObject();
			if (i == start)
				break;
			i--;
		}
	}

	thread->TranslucentObjects.clear();

	PolyMaskedCycles.Unclock();

	CurrentViewpoint = nullptr;
}

/////////////////////////////////////////////////////////////////////////////

PolyTransferHeights::PolyTransferHeights(subsector_t *sub) : Subsector(sub)
{
	sector_t *sec = sub->sector;

	// If player's view height is underneath fake floor, lower the
	// drawn ceiling to be just under the floor height, and replace
	// the drawn floor and ceiling textures, and light level, with
	// the control sector's.
	//
	// Similar for ceiling, only reflected.

	// [RH] allow per-plane lighting
	FloorLightLevel = sec->GetFloorLight();
	CeilingLightLevel = sec->GetCeilingLight();

	FakeSide = PolyWaterFakeSide::Center;

	const sector_t *s = sec->GetHeightSec();
	if (s != nullptr)
	{
		sector_t *heightsec = PolyRenderer::Instance()->Viewpoint.sector->heightsec;
		bool underwater = (heightsec && heightsec->floorplane.PointOnSide(PolyRenderer::Instance()->Viewpoint.Pos) <= 0);
		bool doorunderwater = false;
		int diffTex = (s->MoreFlags & SECF_CLIPFAKEPLANES);

		// Replace sector being drawn with a copy to be hacked
		tempsec = *sec;

		// Replace floor and ceiling height with control sector's heights.
		if (diffTex)
		{
			if (s->floorplane.CopyPlaneIfValid(&tempsec.floorplane, &sec->ceilingplane))
			{
				tempsec.SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
			}
			else if (s->MoreFlags & SECF_FAKEFLOORONLY)
			{
				if (underwater)
				{
					tempsec.Colormap = s->Colormap;
					if (!(s->MoreFlags & SECF_NOFAKELIGHT))
					{
						tempsec.lightlevel = s->lightlevel;

						FloorLightLevel = s->GetFloorLight();
						CeilingLightLevel = s->GetCeilingLight();
					}
					FakeSide = PolyWaterFakeSide::BelowFloor;
					FrontSector = &tempsec;
					return;
				}
				FrontSector = sec;
				return;
			}
		}
		else
		{
			tempsec.floorplane = s->floorplane;
		}

		if (!(s->MoreFlags & SECF_FAKEFLOORONLY))
		{
			if (diffTex)
			{
				if (s->ceilingplane.CopyPlaneIfValid(&tempsec.ceilingplane, &sec->floorplane))
				{
					tempsec.SetTexture(sector_t::ceiling, s->GetTexture(sector_t::ceiling), false);
				}
			}
			else
			{
				tempsec.ceilingplane = s->ceilingplane;
			}
		}

		double refceilz = s->ceilingplane.ZatPoint(PolyRenderer::Instance()->Viewpoint.Pos);
		double orgceilz = sec->ceilingplane.ZatPoint(PolyRenderer::Instance()->Viewpoint.Pos);

		if (underwater || doorunderwater)
		{
			tempsec.floorplane = sec->floorplane;
			tempsec.ceilingplane = s->floorplane;
			tempsec.ceilingplane.FlipVert();
			tempsec.ceilingplane.ChangeHeight(-1 / 65536.);
			tempsec.Colormap = s->Colormap;
		}

		// killough 11/98: prevent sudden light changes from non-water sectors:
		if (underwater || doorunderwater)
		{
			// head-below-floor hack
			tempsec.SetTexture(sector_t::floor, diffTex ? sec->GetTexture(sector_t::floor) : s->GetTexture(sector_t::floor), false);
			tempsec.planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;

			tempsec.ceilingplane = s->floorplane;
			tempsec.ceilingplane.FlipVert();
			tempsec.ceilingplane.ChangeHeight(-1 / 65536.);
			if (s->GetTexture(sector_t::ceiling) == skyflatnum)
			{
				tempsec.floorplane = tempsec.ceilingplane;
				tempsec.floorplane.FlipVert();
				tempsec.floorplane.ChangeHeight(+1 / 65536.);
				tempsec.SetTexture(sector_t::ceiling, tempsec.GetTexture(sector_t::floor), false);
				tempsec.planes[sector_t::ceiling].xform = tempsec.planes[sector_t::floor].xform;
			}
			else
			{
				tempsec.SetTexture(sector_t::ceiling, diffTex ? s->GetTexture(sector_t::floor) : s->GetTexture(sector_t::ceiling), false);
				tempsec.planes[sector_t::ceiling].xform = s->planes[sector_t::ceiling].xform;
			}

			if (!(s->MoreFlags & SECF_NOFAKELIGHT))
			{
				tempsec.lightlevel = s->lightlevel;

				FloorLightLevel = s->GetFloorLight();
				CeilingLightLevel = s->GetCeilingLight();
			}
			FakeSide = PolyWaterFakeSide::BelowFloor;
		}
		else if (heightsec && heightsec->ceilingplane.PointOnSide(PolyRenderer::Instance()->Viewpoint.Pos) <= 0 && orgceilz > refceilz && !(s->MoreFlags & SECF_FAKEFLOORONLY))
		{
			// Above-ceiling hack
			tempsec.ceilingplane = s->ceilingplane;
			tempsec.floorplane = s->ceilingplane;
			tempsec.floorplane.FlipVert();
			tempsec.floorplane.ChangeHeight(+1 / 65536.);
			tempsec.Colormap = s->Colormap;

			tempsec.SetTexture(sector_t::ceiling, diffTex ? sec->GetTexture(sector_t::ceiling) : s->GetTexture(sector_t::ceiling), false);
			tempsec.SetTexture(sector_t::floor, s->GetTexture(sector_t::ceiling), false);
			tempsec.planes[sector_t::ceiling].xform = tempsec.planes[sector_t::floor].xform = s->planes[sector_t::ceiling].xform;

			if (s->GetTexture(sector_t::floor) != skyflatnum)
			{
				tempsec.ceilingplane = sec->ceilingplane;
				tempsec.SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
				tempsec.planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;
			}

			if (!(s->MoreFlags & SECF_NOFAKELIGHT))
			{
				tempsec.lightlevel = s->lightlevel;

				FloorLightLevel = s->GetFloorLight();
				CeilingLightLevel = s->GetCeilingLight();
			}
			FakeSide = PolyWaterFakeSide::AboveCeiling;
		}
		sec = &tempsec;
	}
	FrontSector = sec;
}
