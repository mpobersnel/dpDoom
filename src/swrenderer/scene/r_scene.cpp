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

#include <stdlib.h>
#include <float.h>

#include "templates.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "r_sky.h"
#include "stats.h"
#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "p_effect.h"
#include "po_man.h"
#include "st_stuff.h"
#include "r_data/r_interpolate.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_viewport.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_translucent_pass.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/segments/r_clipsegment.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/segments/r_portalsegment.h"
#include "swrenderer/plane/r_visibleplanelist.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/drawers/r_thread.h"
#include "swrenderer/r_memory.h"

EXTERN_CVAR(Bool, r_shadercolormaps)
EXTERN_CVAR(Int, r_clearbuffer)

namespace swrenderer
{
	cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;
	
	RenderScene *RenderScene::Instance()
	{
		static RenderScene instance;
		return &instance;
	}

	void RenderScene::SetClearColor(int color)
	{
		clearcolor = color;
	}

	void RenderScene::RenderView(player_t *player)
	{
		RenderTarget = screen;

		int width = SCREENWIDTH;
		int height = SCREENHEIGHT;
		float trueratio;
		ActiveRatio(width, height, &trueratio);
		RenderViewport::Instance()->SetViewport(width, height, trueratio);

		if (r_swtruecolor != screen->IsBgra())
		{
			r_swtruecolor = screen->IsBgra();
			R_InitColumnDrawers();
		}

		if (r_clearbuffer != 0)
		{
			if (!r_swtruecolor)
			{
				memset(RenderTarget->GetBuffer(), clearcolor, RenderTarget->GetPitch() * RenderTarget->GetHeight());
			}
			else
			{
				uint32_t bgracolor = GPalette.BaseColors[clearcolor].d;
				int size = RenderTarget->GetPitch() * RenderTarget->GetHeight();
				uint32_t *dest = (uint32_t *)RenderTarget->GetBuffer();
				for (int i = 0; i < size; i++)
					dest[i] = bgracolor;
			}
		}

		R_BeginDrawerCommands();

		RenderActorView(player->mo);

		// Apply special colormap if the target cannot do it
		if (CameraLight::Instance()->realfixedcolormap && r_swtruecolor && !(r_shadercolormaps && screen->Accel2D))
		{
			DrawerCommandQueue::QueueCommand<ApplySpecialColormapRGBACommand>(CameraLight::Instance()->realfixedcolormap, screen);
		}

		R_EndDrawerCommands();
	}

	void RenderScene::RenderActorView(AActor *actor, bool dontmaplines)
	{
		WallCycles.Reset();
		PlaneCycles.Reset();
		MaskedCycles.Reset();
		WallScanCycles.Reset();
		
		RenderMemory::Clear();

		Clip3DFloors *clip3d = Clip3DFloors::Instance();
		clip3d->Cleanup();
		clip3d->ResetClip(); // reset clips (floor/ceiling)

		R_SetupFrame(actor);
		CameraLight::Instance()->SetCamera(actor);
		RenderViewport::Instance()->SetupFreelook();

		RenderPortal::Instance()->CopyStackedViewParameters();

		// Clear buffers.
		RenderClipSegment::Instance()->Clear(0, viewwidth);
		DrawSegmentList::Instance()->Clear();
		VisiblePlaneList::Instance()->Clear();
		RenderTranslucentPass::Instance()->Clear();

		// opening / clipping determination
		RenderOpaquePass::Instance()->ClearClip();

		NetUpdate();

		colfunc = basecolfunc;
		spanfunc = &SWPixelFormatDrawers::DrawSpan;

		RenderPortal::Instance()->SetMainPortal();

		this->dontmaplines = dontmaplines;

		// [RH] Hack to make windows into underwater areas possible
		RenderOpaquePass::Instance()->ResetFakingUnderwater();

		// [RH] Setup particles for this frame
		P_FindParticleSubsectors();

		WallCycles.Clock();
		ActorRenderFlags savedflags = camera->renderflags;
		// Never draw the player unless in chasecam mode
		if (!r_showviewer)
		{
			camera->renderflags |= RF_INVISIBLE;
		}
		// Link the polyobjects right before drawing the scene to reduce the amounts of calls to this function
		PO_LinkToSubsectors();
		RenderOpaquePass::Instance()->RenderScene();
		Clip3DFloors::Instance()->ResetClip(); // reset clips (floor/ceiling)
		camera->renderflags = savedflags;
		WallCycles.Unclock();

		NetUpdate();

		if (viewactive)
		{
			PlaneCycles.Clock();
			VisiblePlaneList::Instance()->Render();
			RenderPortal::Instance()->RenderPlanePortals();
			PlaneCycles.Unclock();

			RenderPortal::Instance()->RenderLinePortals();

			NetUpdate();

			MaskedCycles.Clock();
			RenderTranslucentPass::Instance()->Render();
			MaskedCycles.Unclock();

			NetUpdate();
		}
		interpolator.RestoreInterpolations();

		// If we don't want shadered colormaps, NULL it now so that the
		// copy to the screen does not use a special colormap shader.
		if (!r_shadercolormaps && !r_swtruecolor)
		{
			CameraLight::Instance()->realfixedcolormap = NULL;
		}
	}

	void RenderScene::RenderViewToCanvas(AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines)
	{
		const bool savedviewactive = viewactive;
		const bool savedoutputformat = r_swtruecolor;

		if (r_swtruecolor != canvas->IsBgra())
		{
			r_swtruecolor = canvas->IsBgra();
			R_InitColumnDrawers();
		}

		R_BeginDrawerCommands();

		viewwidth = width;
		RenderTarget = canvas;
		bRenderingToCanvas = true;

		R_SetWindow(12, width, height, height, true);
		viewwindowx = x;
		viewwindowy = y;
		viewactive = true;
		RenderViewport::Instance()->SetViewport(width, height, WidescreenRatio);

		RenderActorView(actor, dontmaplines);

		R_EndDrawerCommands();

		RenderTarget = screen;
		bRenderingToCanvas = false;

		R_ExecuteSetViewSize();
		float trueratio;
		ActiveRatio(width, height, &trueratio);
		screen->Lock(true);
		RenderViewport::Instance()->SetViewport(width, height, trueratio);
		screen->Unlock();

		viewactive = savedviewactive;
		r_swtruecolor = savedoutputformat;

		if (r_swtruecolor != canvas->IsBgra())
		{
			R_InitColumnDrawers();
		}
	}

	void RenderScene::ScreenResized()
	{
		RenderTarget = screen;
		int width = SCREENWIDTH;
		int height = SCREENHEIGHT;
		float trueratio;
		ActiveRatio(width, height, &trueratio);
		screen->Lock(true);
		RenderViewport::Instance()->SetViewport(SCREENWIDTH, SCREENHEIGHT, trueratio);
		screen->Unlock();
	}

	void RenderScene::Init()
	{
		atterm([]() { RenderScene::Instance()->Deinit(); });
		// viewwidth / viewheight are set by the defaults
		fillshort(zeroarray, MAXWIDTH, 0);

		R_InitShadeMaps();
		R_InitColumnDrawers();
	}

	void RenderScene::Deinit()
	{
		RenderTranslucentPass::Instance()->Deinit();
		Clip3DFloors::Instance()->Cleanup();
		DrawSegmentList::Instance()->Deinit();
	}

	/////////////////////////////////////////////////////////////////////////

	ADD_STAT(fps)
	{
		FString out;
		out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms",
			FrameCycles.TimeMS(), WallCycles.TimeMS(), PlaneCycles.TimeMS(), MaskedCycles.TimeMS());
		return out;
	}

	static double f_acc, w_acc, p_acc, m_acc;
	static int acc_c;

	ADD_STAT(fps_accumulated)
	{
		f_acc += FrameCycles.TimeMS();
		w_acc += WallCycles.TimeMS();
		p_acc += PlaneCycles.TimeMS();
		m_acc += MaskedCycles.TimeMS();
		acc_c++;
		FString out;
		out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms  %d counts",
			f_acc / acc_c, w_acc / acc_c, p_acc / acc_c, m_acc / acc_c, acc_c);
		Printf(PRINT_LOG, "%s\n", out.GetChars());
		return out;
	}

	static double bestwallcycles = HUGE_VAL;

	ADD_STAT(wallcycles)
	{
		FString out;
		double cycles = WallCycles.Time();
		if (cycles && cycles < bestwallcycles)
			bestwallcycles = cycles;
		out.Format("%g", bestwallcycles);
		return out;
	}

	CCMD(clearwallcycles)
	{
		bestwallcycles = HUGE_VAL;
	}

#if 0
	// The replacement code for Build's wallscan doesn't have any timing calls so this does not work anymore.
	static double bestscancycles = HUGE_VAL;

	ADD_STAT(scancycles)
	{
		FString out;
		double scancycles = WallScanCycles.Time();
		if (scancycles && scancycles < bestscancycles)
			bestscancycles = scancycles;
		out.Format("%g", bestscancycles);
		return out;
	}

	CCMD(clearscancycles)
	{
		bestscancycles = HUGE_VAL;
	}
#endif
}
