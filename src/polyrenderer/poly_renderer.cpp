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
#include "sbar.h"
#include "st_stuff.h"
#include "r_data/r_translate.h"
#include "r_data/r_interpolate.h"
#include "poly_renderer.h"
#include "d_net.h"
#include "po_man.h"
#include "st_stuff.h"
#include "g_levellocals.h"
#include "p_effect.h"
#include "polyrenderer/scene/poly_light.h"
#include "polyrenderer/hardpoly/hardpolyrenderer.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/r_swcolormaps.h"
#include "gl/system/gl_swframebuffer.h"

EXTERN_CVAR(Bool, r_shadercolormaps)
EXTERN_CVAR(Int, screenblocks)
EXTERN_CVAR(Float, r_visibility)

CVAR(Bool, r_hardpoly, false, 0)

void InitGLRMapinfoData();

/////////////////////////////////////////////////////////////////////////////

PolyRenderer *PolyRenderer::Instance()
{
	static PolyRenderer scene;
	return &scene;
}

PolyRenderer::PolyRenderer()
{
	Hardpoly = std::make_shared<HardpolyRenderer>();
}

void PolyRenderer::RenderView(player_t *player)
{
	using namespace swrenderer;
	
	RenderTarget = screen;

	int width = SCREENWIDTH;
	int height = SCREENHEIGHT;
	float trueratio;
	ActiveRatio(width, height, &trueratio);
	//viewport->SetViewport(&Thread, width, height, trueratio);

	if (r_hardpoly)
	{
		RedirectToHardpoly = true;
		Hardpoly->Begin();
	}
	else
	{
		screen->SetUseHardwareScene(false);
		screen->LockBuffer();
	}

	RenderActorView(player->mo, false);

	if (RedirectToHardpoly)
	{
		Hardpoly->End();
		RedirectToHardpoly = false;
	}
	else
	{
		// Apply special colormap if the target cannot do it
		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->ShaderColormap() && RenderTarget->IsBgra() && !(r_shadercolormaps && screen->Accel2D))
		{
			Threads.MainThread()->DrawQueue->Push<ApplySpecialColormapRGBACommand>(cameraLight->ShaderColormap(), screen);
		}

		Threads.MainThread()->FlushDrawQueue();
		PolyDrawerWaitCycles.Clock();
		DrawerThreads::WaitForWorkers();
		PolyDrawerWaitCycles.Unclock();
	}
}

void PolyRenderer::RenderViewToCanvas(AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines)
{
	const bool savedviewactive = viewactive;

	viewwidth = width;
	RenderTarget = canvas;
	R_SetWindow(Viewpoint, Viewwindow, 12, width, height, height, true);
	//viewport->SetViewport(&Thread, width, height, Viewwindow.WidescreenRatio);
	viewwindowx = x;
	viewwindowy = y;
	viewactive = true;
	
	canvas->LockBuffer();
	
	RenderActorView(actor, dontmaplines);
	Threads.MainThread()->FlushDrawQueue();
	DrawerThreads::WaitForWorkers();
	
	RenderTarget = screen;
	R_ExecuteSetViewSize(Viewpoint, Viewwindow);
	float trueratio;
	ActiveRatio(width, height, &trueratio);
	//viewport->SetViewport(&Thread, width, height, viewport->viewwindow.WidescreenRatio);
	viewactive = savedviewactive;
}

void PolyRenderer::RenderActorView(AActor *actor, bool dontmaplines)
{
	PolyTotalBatches = 0;
	PolyTotalTriangles = 0;
	PolyTotalDrawCalls = 0;
	PolyCullCycles.Reset();
	PolyOpaqueCycles.Reset();
	PolyMaskedCycles.Reset();
	PolyDrawerWaitCycles.Reset();

	NetUpdate();
	
	DontMapLines = dontmaplines;
	
	R_SetupFrame(Viewpoint, Viewwindow, actor);
	P_FindParticleSubsectors();
	PO_LinkToSubsectors();

	swrenderer::R_UpdateFuzzPosFrameStart();

	if (APART(R_OldBlend)) NormalLight.Maps = realcolormaps.Maps;
	else NormalLight.Maps = realcolormaps.Maps + NUMCOLORMAPS * 256 * R_OldBlend;

	Light.SetVisibility(Viewwindow, r_visibility);

	PolyCameraLight::Instance()->SetCamera(Viewpoint, RenderTarget, actor);
	//Viewport->SetupFreelook();

	ActorRenderFlags savedflags = Viewpoint.camera->renderflags;
	// Never draw the player unless in chasecam mode
	if (!Viewpoint.showviewer)
		Viewpoint.camera->renderflags |= RF_INVISIBLE;

	ScreenTriangle::FuzzStart = (ScreenTriangle::FuzzStart + 14) % FUZZTABLE;

	ClearBuffers();
	SetSceneViewport();
	SetupPerspectiveMatrix();

	PolyPortalViewpoint mainViewpoint;
	mainViewpoint.WorldToView = WorldToView;
	mainViewpoint.WorldToClip = WorldToClip;
	mainViewpoint.StencilValue = GetNextStencilValue();
	mainViewpoint.PortalPlane = PolyClipPlane(0.0f, 0.0f, 0.0f, 1.0f);
	Scene.Render(&mainViewpoint);
	Skydome.Render(Threads.MainThread(), WorldToView, WorldToClip);
	Scene.RenderTranslucent(&mainViewpoint);
	PlayerSprites.Render(Threads.MainThread());

	Viewpoint.camera->renderflags = savedflags;
	interpolator.RestoreInterpolations ();
	
	NetUpdate();
}

void PolyRenderer::RenderRemainingPlayerSprites()
{
	PlayerSprites.RenderRemainingSprites();
}

void PolyRenderer::ClearBuffers()
{
	Threads.Clear();
	if (RedirectToHardpoly)
	{
		Hardpoly->ClearBuffers();
	}
	else
	{
		PolyTriangleDrawer::ClearBuffers(RenderTarget);
	}
	NextStencilValue = 0;
	Threads.MainThread()->SectorPortals.clear();
	Threads.MainThread()->LinePortals.clear();
	Threads.MainThread()->TranslucentObjects.clear();
}

void PolyRenderer::SetSceneViewport()
{
	using namespace swrenderer;
	
	if (RenderTarget == screen) // Rendering to screen
	{
		int height;
		if (screenblocks >= 10)
			height = SCREENHEIGHT;
		else
			height = (screenblocks*SCREENHEIGHT / 10) & ~7;

		int bottom = SCREENHEIGHT - (height + viewwindowy - ((height - viewheight) / 2));
		if (RedirectToHardpoly)
			Hardpoly->SetViewport(viewwindowx, SCREENHEIGHT - bottom - height, viewwidth, height);
		else
			PolyTriangleDrawer::SetViewport(Threads.MainThread()->DrawQueue, viewwindowx, SCREENHEIGHT - bottom - height, viewwidth, height, RenderTarget);
	}
	else // Rendering to camera texture
	{
		if (RedirectToHardpoly)
			Hardpoly->SetViewport(0, 0, RenderTarget->GetWidth(), RenderTarget->GetHeight());
		else
			PolyTriangleDrawer::SetViewport(Threads.MainThread()->DrawQueue, 0, 0, RenderTarget->GetWidth(), RenderTarget->GetHeight(), RenderTarget);
	}
}

void PolyRenderer::SetupPerspectiveMatrix()
{
	static bool bDidSetup = false;

	if (!bDidSetup)
	{
		InitGLRMapinfoData();
		bDidSetup = true;
	}

	// We have to scale the pitch to account for the pixel stretching, because the playsim doesn't know about this and treats it as 1:1.
	double radPitch = Viewpoint.Angles.Pitch.Normalized180().Radians();
	double angx = cos(radPitch);
	double angy = sin(radPitch) * level.info->pixelstretch;
	double alen = sqrt(angx*angx + angy*angy);
	float adjustedPitch = (float)asin(angy / alen);
	float adjustedViewAngle = (float)(Viewpoint.Angles.Yaw - 90).Radians();

	float ratio = Viewwindow.WidescreenRatio;
	float fovratio = (Viewwindow.WidescreenRatio >= 1.3f) ? 1.333333f : ratio;
	float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(Viewpoint.FieldOfView.Radians() / 2) / fovratio)).Degrees);

	WorldToView =
		Mat4f::Rotate((float)Viewpoint.Angles.Roll.Radians(), 0.0f, 0.0f, 1.0f) *
		Mat4f::Rotate(adjustedPitch, 1.0f, 0.0f, 0.0f) *
		Mat4f::Rotate(adjustedViewAngle, 0.0f, -1.0f, 0.0f) *
		Mat4f::Scale(1.0f, level.info->pixelstretch, 1.0f) *
		Mat4f::SwapYZ() *
		Mat4f::Translate((float)-Viewpoint.Pos.X, (float)-Viewpoint.Pos.Y, (float)-Viewpoint.Pos.Z);

	WorldToClip = Mat4f::Perspective(fovy, ratio, 5.0f, 65535.0f, Handedness::Right, ClipZRange::NegativePositiveW) * WorldToView;

	if (RedirectToHardpoly)
	{
		Threads.MainThread()->DrawBatcher.WorldToView = WorldToView;
		Threads.MainThread()->DrawBatcher.ViewToClip = Mat4f::Perspective(fovy, ratio, 5.0f, 65535.0f, Handedness::Right, screen->GetContext()->GetClipZRange());
		Threads.MainThread()->DrawBatcher.MatrixUpdated();
	}
}

cycle_t PolyCullCycles, PolyOpaqueCycles, PolyMaskedCycles, PolyDrawerWaitCycles;
int PolyTotalBatches, PolyTotalTriangles, PolyTotalDrawCalls;

ADD_STAT(polyfps)
{
	FString out;
	out.Format("frame=%04.1f ms  cull=%04.1f ms  opaque=%04.1f ms  masked=%04.1f ms  drawers=%04.1f ms",
		FrameCycles.TimeMS(), PolyCullCycles.TimeMS(), PolyOpaqueCycles.TimeMS(), PolyMaskedCycles.TimeMS(), PolyDrawerWaitCycles.TimeMS());
	out.AppendFormat("\nbatches drawn: %d  triangles drawn: %d  drawcalls: %d", PolyTotalBatches, PolyTotalTriangles, PolyTotalDrawCalls);
	return out;
}
