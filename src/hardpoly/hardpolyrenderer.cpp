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
#include "hardpolyrenderer.h"
#include "v_palette.h"
#include "v_video.h"
#include "m_png.h"
#include "textures/textures.h"
#include "r_data/voxels.h"
#include "p_setup.h"
#include "r_utility.h"
#include "d_player.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "gl/system/gl_system.h"
#include "gl/system/gl_swframebuffer.h"
#include "levelmeshbuilder.h"
#include "po_man.h"
#include "r_data/r_interpolate.h"
#include "p_effect.h"

EXTERN_CVAR(Float, maxviewpitch)

void gl_ParseDefs();
void gl_SetActorLights(AActor *);
void gl_PreprocessLevel();
void gl_CleanLevelData();
void InitGLRMapinfoData();

HardpolyRenderer::HardpolyRenderer()
{
}

HardpolyRenderer::~HardpolyRenderer()
{
}

bool HardpolyRenderer::UsesColormap() const
{
	return false;
}

void HardpolyRenderer::Precache(BYTE *texhitlist, TMap<PClassActor*, bool> &actorhitlist)
{
}

void HardpolyRenderer::RenderView(player_t *player)
{
	P_FindParticleSubsectors();
	PO_LinkToSubsectors();
	R_SetupFrame(player->mo);

	mContext->Begin();

	int width = screen->GetWidth();
	int height = screen->GetHeight();
	if (!mSceneFB || mAlbedoBuffer->Width() != width || mAlbedoBuffer->Height() != height)
	{
		mSceneFB.reset();
		mAlbedoBuffer.reset();
		mDepthStencilBuffer.reset();
		mNormalBuffer.reset();
		
		mAlbedoBuffer = std::make_shared<GPUTexture2D>(width, height, false, 0, GPUPixelFormat::RGBA16f);
		mNormalBuffer = std::make_shared<GPUTexture2D>(width, height, false, 0, GPUPixelFormat::RGBA16f);
		mDepthStencilBuffer = std::make_shared<GPUTexture2D>(width, height, false, 0, GPUPixelFormat::Depth24_Stencil8);
		
		std::vector<GPUTexture2DPtr> colorbuffers = { mAlbedoBuffer, mNormalBuffer };
		mSceneFB = std::make_shared<GPUFrameBuffer>(colorbuffers, mDepthStencilBuffer);
	}
	
	mContext->SetFrameBuffer(mSceneFB);
	mContext->SetViewport(0, 0, width, height);

	mContext->ClearColorBuffer(0, 0.5f, 0.5f, 0.2f, 1.0f);
	mContext->ClearColorBuffer(1, 0.0f, 0.0f, 0.0f, 0.0f);
	mContext->ClearDepthStencilBuffer(1.0f, 0);

	SetupPerspectiveMatrix();

	if (!mVertexArray)
	{
		LevelMeshBuilder builder;
		builder.Generate();
		
		mVertexArray = builder.VertexArray;
		mNumVertices = builder.NumVertices;
	}

	if (!mProgram)
	{
		mProgram = std::make_shared<GPUProgram>();

		mProgram->Compile(GPUShaderType::Vertex, "vertex", R"(
				layout(std140) uniform FrameUniforms
				{
					mat4 WorldToView;
					mat4 ViewToProjection;
				};

				in vec4 Position;
				in vec2 Texcoord;
				out vec2 UV;

				void main()
				{
					gl_Position = ViewToProjection * WorldToView * Position;
					UV = Texcoord;
				}
			)");
		mProgram->Compile(GPUShaderType::Fragment, "fragment", R"(
				in vec2 UV;
				out vec4 FragAlbedo;
				void main()
				{
					FragAlbedo = vec4(UV, 0.0, 1.0);
				}
			)");

		mProgram->SetAttribLocation("Position", 0);
		mProgram->SetAttribLocation("UV", 1);
		mProgram->SetFragOutput("FragAlbedo", 0);
		mProgram->SetFragOutput("FragNormal", 1);
		mProgram->Link("program");
	}

	mContext->SetVertexArray(mVertexArray);
	mContext->SetProgram(mProgram);
	mContext->SetUniforms(0, mFrameUniforms);

	mContext->Draw(GPUDrawMode::Triangles, 0, mNumVertices);

	mContext->SetUniforms(0, nullptr);
	mContext->SetVertexArray(nullptr);
	mContext->SetProgram(nullptr);

	mContext->SetFrameBuffer(nullptr);
	mContext->End();
	
	interpolator.RestoreInterpolations ();

	auto swframebuffer = static_cast<OpenGLSWFrameBuffer*>(screen);
	swframebuffer->SetViewFB(mSceneFB->Handle());

	FCanvasTextureInfo::UpdateAll();
}

void HardpolyRenderer::SetupPerspectiveMatrix()
{
	static bool bDidSetup = false;

	if (!bDidSetup)
	{
		InitGLRMapinfoData();
		bDidSetup = true;
	}

	double radPitch = ViewPitch.Normalized180().Radians();
	double angx = cos(radPitch);
	double angy = sin(radPitch) * glset.pixelstretch;
	double alen = sqrt(angx*angx + angy*angy);
	float adjustedPitch = (float)asin(angy / alen);
	float adjustedViewAngle = (float)(ViewAngle - 90).Radians();

	float ratio = WidescreenRatio;
	float fovratio = (WidescreenRatio >= 1.3f) ? 1.333333f : ratio;
	float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(FieldOfView.Radians() / 2) / fovratio)).Degrees);

	Mat4f worldToView =
		Mat4f::Rotate(adjustedPitch, 1.0f, 0.0f, 0.0f) *
		Mat4f::Rotate(adjustedViewAngle, 0.0f, -1.0f, 0.0f) *
		Mat4f::Scale(1.0f, glset.pixelstretch, 1.0f) *
		Mat4f::SwapYZ() *
		Mat4f::Translate((float)-ViewPos.X, (float)-ViewPos.Y, (float)-ViewPos.Z);

	Mat4f viewToClip = Mat4f::Perspective(fovy, ratio, 5.0f, 65535.0f);
	
	if (!mFrameUniforms)
		mFrameUniforms = std::make_shared<GPUUniformBuffer>(nullptr, (int)sizeof(FrameUniforms));

	FrameUniforms frameUniforms;
	frameUniforms.WorldToView = worldToView;
	frameUniforms.ViewToProjection = viewToClip;

	mFrameUniforms->Upload(&frameUniforms, (int)sizeof(FrameUniforms));
}

void HardpolyRenderer::RemapVoxels()
{
}

void HardpolyRenderer::WriteSavePic(player_t *player, FileWriter *file, int width, int height)
{
}

void HardpolyRenderer::DrawRemainingPlayerSprites()
{
}

int HardpolyRenderer::GetMaxViewPitch(bool down)
{
	return int(maxviewpitch);
}

bool HardpolyRenderer::RequireGLNodes()
{
	return true;
}

void HardpolyRenderer::OnModeSet()
{
}

void HardpolyRenderer::Init()
{
	gl_ParseDefs();
	
	mContext = std::make_shared<GPUContext>();
}

void HardpolyRenderer::SetClearColor(int color)
{
}

void HardpolyRenderer::RenderTextureView(FCanvasTexture *tex, AActor *viewpoint, int fov)
{
	tex->SetUpdated();
}

sector_t *HardpolyRenderer::FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel)
{
	return swrenderer::RenderOpaquePass::Instance()->FakeFlat(sec, tempsec, floorlightlevel, ceilinglightlevel, nullptr, 0, 0, 0, 0);
}

void HardpolyRenderer::StateChanged(AActor *actor)
{
	gl_SetActorLights(actor);
}

void HardpolyRenderer::PreprocessLevel()
{
	gl_PreprocessLevel();
}

void HardpolyRenderer::CleanLevelData()
{
	gl_CleanLevelData();
}
