/*
**  Potential visible set (PVS) handling
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
#include "r_utility.h"
#include "bspcull.h"
#include "g_levellocals.h"

void HardpolyBSPCull::CullScene(const HardpolyPlane &portalClipPlane)
{
	PvsSectors.clear();
	PortalClipPlane = portalClipPlane;

	// Cull front to back
	FirstSkyHeight = true;
	MaxCeilingHeight = 0.0;
	MinFloorHeight = 0.0;
	if (level.nodes.Size() == 0)
		CullSubsector(&level.subsectors[0]);
	else
		CullNode(level.HeadNode());
}

void HardpolyBSPCull::CullNode(void *node)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = PointOnSide(r_viewpoint.Pos, bsp);

		// Recursively divide front space (toward the viewer).
		CullNode(bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		side ^= 1;

		if (!CheckBBox(bsp->bbox[side]))
			return;

		node = bsp->children[side];
	}

	subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
	CullSubsector(sub);
}

void HardpolyBSPCull::CullSubsector(subsector_t *sub)
{
	// Update sky heights for the scene
	if (!FirstSkyHeight)
	{
		MaxCeilingHeight = MAX(MaxCeilingHeight, sub->sector->ceilingplane.Zat0());
		MinFloorHeight = MIN(MinFloorHeight, sub->sector->floorplane.Zat0());
	}
	else
	{
		MaxCeilingHeight = sub->sector->ceilingplane.Zat0();
		MinFloorHeight = sub->sector->floorplane.Zat0();
		FirstSkyHeight = false;
	}

	// Mark that we need to render this
	PvsSectors.push_back(sub);

	// Update culling info for further bsp clipping
	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if ((line->sidedef == nullptr || !(line->sidedef->Flags & WALLF_POLYOBJ)) && line->backsector == nullptr)
		{
			// Skip lines not facing viewer
			DVector2 pt1 = line->v1->fPos() - r_viewpoint.Pos;
			DVector2 pt2 = line->v2->fPos() - r_viewpoint.Pos;
			if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
				continue;

			angle_t angle1, angle2;
			if (GetAnglesForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), angle1, angle2))
			{
				MarkSegmentCulled(angle1, angle2);
			}
		}
	}
}

void HardpolyBSPCull::ClearSolidSegments()
{
	SolidSegments.clear();
}

void HardpolyBSPCull::InvertSegments()
{
	TempInvertSolidSegments.swap(SolidSegments);
	ClearSolidSegments();
	angle_t cur = 0;
	for (const auto &segment : TempInvertSolidSegments)
	{
		if (segment.Start != 0 || segment.End != ANGLE_MAX)
			MarkSegmentCulled(cur, segment.Start - 1);
		cur = segment.End + 1;
	}
	if (cur != 0)
		MarkSegmentCulled(cur, ANGLE_MAX);
}

bool HardpolyBSPCull::IsSegmentCulled(angle_t startAngle, angle_t endAngle) const
{
	if (startAngle > endAngle)
	{
		return IsSegmentCulled(startAngle, ANGLE_MAX) && IsSegmentCulled(0, endAngle);
	}

	for (const auto &segment : SolidSegments)
	{
		if (startAngle >= segment.Start && endAngle <= segment.End)
			return true;
		else if (endAngle < segment.Start)
			return false;
	}
	return false;
}

void HardpolyBSPCull::MarkSegmentCulled(angle_t startAngle, angle_t endAngle)
{
	if (startAngle > endAngle)
	{
		MarkSegmentCulled(startAngle, ANGLE_MAX);
		MarkSegmentCulled(0, endAngle);
		return;
	}

	int count = (int)SolidSegments.size();
	int cur = 0;
	while (cur < count)
	{
		if (SolidSegments[cur].Start <= startAngle && SolidSegments[cur].End >= endAngle) // Already fully marked
		{
			return;
		}
		else if (SolidSegments[cur].End >= startAngle && SolidSegments[cur].Start <= endAngle) // Merge segments
		{
			// Find last segment
			int merge = cur;
			while (merge + 1 != count && SolidSegments[merge + 1].Start <= endAngle)
				merge++;

			// Apply new merged range
			SolidSegments[cur].Start = MIN(SolidSegments[cur].Start, startAngle);
			SolidSegments[cur].End = MAX(SolidSegments[merge].End, endAngle);

			// Remove additional segments we merged with
			if (merge > cur)
				SolidSegments.erase(SolidSegments.begin() + (cur + 1), SolidSegments.begin() + (merge + 1));

			return;
		}
		else if (SolidSegments[cur].Start > startAngle) // Insert new segment
		{
			SolidSegments.insert(SolidSegments.begin() + cur, { startAngle, endAngle });
			return;
		}
		cur++;
	}
	SolidSegments.push_back({ startAngle, endAngle });

#if 0
	count = (int)SolidSegments.size();
	for (int i = 1; i < count; i++)
	{
		if (SolidSegments[i - 1].Start >= SolidSegments[i].Start ||
			SolidSegments[i - 1].End >= SolidSegments[i].Start ||
			SolidSegments[i - 1].End + 1 == SolidSegments[i].Start ||
			SolidSegments[i].Start > SolidSegments[i].End)
		{
			I_FatalError("MarkSegmentCulled is broken!");
		}
	}
#endif
}

int HardpolyBSPCull::PointOnSide(const DVector2 &pos, const node_t *node)
{
	return DMulScale32(FLOAT2FIXED(pos.Y) - node->y, node->dx, node->x - FLOAT2FIXED(pos.X), node->dy) > 0;
}

bool HardpolyBSPCull::CheckBBox(float *bspcoord)
{
	// Occlusion test using solid segments:
	static const uint8_t checkcoord[12][4] =
	{
		{ 3,0,2,1 },
		{ 3,0,2,0 },
		{ 3,1,2,0 },
		{ 0 },
		{ 2,0,2,1 },
		{ 0,0,0,0 },
		{ 3,1,3,0 },
		{ 0 },
		{ 2,0,3,1 },
		{ 2,1,3,1 },
		{ 2,1,3,0 }
	};

	// Find the corners of the box that define the edges from current viewpoint.
	const auto &viewpoint = r_viewpoint;
	int boxpos = (viewpoint.Pos.X <= bspcoord[BOXLEFT] ? 0 : viewpoint.Pos.X < bspcoord[BOXRIGHT] ? 1 : 2) +
		(viewpoint.Pos.Y >= bspcoord[BOXTOP] ? 0 : viewpoint.Pos.Y > bspcoord[BOXBOTTOM] ? 4 : 8);

	if (boxpos == 5) return true;

	const uint8_t *check = checkcoord[boxpos];
	angle_t angle1 = PointToPseudoAngle(bspcoord[check[0]], bspcoord[check[1]]);
	angle_t angle2 = PointToPseudoAngle(bspcoord[check[2]], bspcoord[check[3]]);

	return !IsSegmentCulled(angle2, angle1);
}

bool HardpolyBSPCull::GetAnglesForLine(double x1, double y1, double x2, double y2, angle_t &angle1, angle_t &angle2) const
{
#if 0
	// Clip line to the portal clip plane
	float distance1 = Vec4f::dot(PortalClipPlane, Vec4f((float)x1, (float)y1, 0.0f, 1.0f));
	float distance2 = Vec4f::dot(PortalClipPlane, Vec4f((float)x2, (float)y2, 0.0f, 1.0f));
	if (distance1 < 0.0f && distance2 < 0.0f)
	{
		return false;
	}
	else if (distance1 < 0.0f || distance2 < 0.0f)
	{
		double t1 = 0.0f, t2 = 1.0f;
		if (distance1 < 0.0f)
			t1 = clamp(distance1 / (distance1 - distance2), 0.0f, 1.0f);
		else
			t2 = clamp(distance2 / (distance1 - distance2), 0.0f, 1.0f);
		double nx1 = x1 * (1.0 - t1) + x2 * t1;
		double ny1 = y1 * (1.0 - t1) + y2 * t1;
		double nx2 = x1 * (1.0 - t2) + x2 * t2;
		double ny2 = y1 * (1.0 - t2) + y2 * t2;
		x1 = nx1;
		x2 = nx2;
		y1 = ny1;
		y2 = ny2;
	}
#endif

	angle2 = PointToPseudoAngle(x1, y1);
	angle1 = PointToPseudoAngle(x2, y2);
	return !IsSegmentCulled(angle1, angle2);
}

void HardpolyBSPCull::MarkViewFrustum()
{
	// Clips things outside the viewing frustum.
	auto &viewpoint = r_viewpoint;
	auto &viewwindow = r_viewwindow;
	double tilt = fabs(viewpoint.Angles.Pitch.Degrees);
	if (tilt > 46.0) // If the pitch is larger than this you can look all around
		return;

	double floatangle = 2.0 + (45.0 + ((tilt / 1.9)))*viewpoint.FieldOfView.Degrees*48.0 / AspectMultiplier(viewwindow.WidescreenRatio) / 90.0;
	angle_t a1 = DAngle(floatangle).BAMs();
	if (a1 < ANGLE_180)
	{
		MarkSegmentCulled(AngleToPseudo(viewpoint.Angles.Yaw.BAMs() + a1), AngleToPseudo(viewpoint.Angles.Yaw.BAMs() - a1));
	}
}

//-----------------------------------------------------------------------------
//
// ! Returns the pseudoangle between the line p1 to (infinity, p1.y) and the 
// line from p1 to p2. The pseudoangle has the property that the ordering of 
// points by true angle around p1 and ordering of points by pseudoangle are the 
// same.
//
// For clipping exact angles are not needed. Only the ordering matters.
// This is about as fast as the fixed point R_PointToAngle2 but without
// the precision issues associated with that function.
//
//-----------------------------------------------------------------------------

angle_t HardpolyBSPCull::PointToPseudoAngle(double x, double y)
{
	const auto &viewpoint = r_viewpoint;
	double vecx = x - viewpoint.Pos.X;
	double vecy = y - viewpoint.Pos.Y;

	if (vecx == 0 && vecy == 0)
	{
		return 0;
	}
	else
	{
		double result = vecy / (fabs(vecx) + fabs(vecy));
		if (vecx < 0)
		{
			result = 2. - result;
		}
		return xs_Fix<30>::ToFix(result);
	}
}

angle_t HardpolyBSPCull::AngleToPseudo(angle_t ang)
{
	double vecx = cos(ang * M_PI / ANGLE_180);
	double vecy = sin(ang * M_PI / ANGLE_180);

	double result = vecy / (fabs(vecx) + fabs(vecy));
	if (vecx < 0)
	{
		result = 2.f - result;
	}
	return xs_Fix<30>::ToFix(result);
}