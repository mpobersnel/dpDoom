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

#pragma once

#include "r_planerenderer.h"

namespace swrenderer
{
	class RenderSlopePlane : PlaneRenderer
	{
	public:
		void Render(visplane_t *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked, FDynamicColormap *basecolormap);

	private:
		void RenderLine(int y, int x1, int x2) override;

		FVector3 plane_sz, plane_su, plane_sv;
		float planelightfloat;
		bool plane_shade;
		int planeshade;
		fixed_t pviewx, pviewy;
		fixed_t xscale, yscale;
		FDynamicColormap *basecolormap;
	};
}
