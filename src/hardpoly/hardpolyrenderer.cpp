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
#include "gl/data/gl_data.h"
#include "levelmeshbuilder.h"
#include "po_man.h"
#include "r_data/r_interpolate.h"
#include "p_effect.h"
#include "g_levellocals.h"
#include "levelmeshbuilder.h"

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

	SetupFramebuffer();
	SetupStaticLevelMesh();
	CompileShaders();
	CreateSamplers();
	UploadSectorTexture();
	RenderLevelMesh(mVertexArray, mDrawRuns, 1.0f);
	RenderDynamicMesh();

	mContext->SetFrameBuffer(nullptr);
	mContext->End();
	
	interpolator.RestoreInterpolations ();

	auto swframebuffer = static_cast<OpenGLSWFrameBuffer*>(screen);
	swframebuffer->SetViewFB(mSceneFB->Handle());

	FCanvasTextureInfo::UpdateAll();
}

void HardpolyRenderer::RenderDynamicMesh()
{
	std::vector<subsector_t*> subsectors;
	for (auto sector : dynamicSectors)
	{
		for (int i = 0; i < sector->subsectorcount; i++)
		{
			subsectors.push_back(sector->subsectors[i]);
		}
	}
	if (!subsectors.empty())
	{
		LevelMeshBuilder dynamicMesh;
		dynamicMesh.Generate(subsectors);
		RenderLevelMesh(dynamicMesh.VertexArray, dynamicMesh.DrawRuns, 0.0f);
	}
}

void HardpolyRenderer::UploadSectorTexture()
{
	mCurrentSectorTexture = (mCurrentSectorTexture + 1) % 3;
	if (!mSectorTexture[mCurrentSectorTexture])
		mSectorTexture[mCurrentSectorTexture] = std::make_shared<GPUTexture2D>(256, 16, false, 0, GPUPixelFormat::RGBA32f);

	dynamicSectors.clear();

	cpuSectors.resize((level.sectors.Size() / 256 + 1) * 256);
	for (unsigned int i = 0; i < level.sectors.Size(); i++)
	{
		sector_t *sector = &level.sectors[i];
		float dynamicSector = (cpuStaticSectorCeiling[i] == sector->ceilingplane.Zat0()) ? 1.0 : 0.0;
		dynamicSector *= (cpuStaticSectorFloor[i] == sector->floorplane.Zat0()) ? 1.0 : 0.0;
		for (unsigned int j = 0; j < sector->Lines.Size(); j++)
		{
			line_t *line = sector->Lines[j];
			if (line->frontsector != sector)
			{
				int k = line->frontsector->sectornum;
				dynamicSector *= (cpuStaticSectorCeiling[k] == line->frontsector->ceilingplane.Zat0()) ? 1.0 : 0.0;
				dynamicSector *= (cpuStaticSectorFloor[k] == line->frontsector->floorplane.Zat0()) ? 1.0 : 0.0;
			}
			else if (line->backsector && line->backsector != sector)
			{
				int k = line->backsector->sectornum;
				dynamicSector *= (cpuStaticSectorCeiling[k] == line->backsector->ceilingplane.Zat0()) ? 1.0 : 0.0;
				dynamicSector *= (cpuStaticSectorFloor[k] == line->backsector->floorplane.Zat0()) ? 1.0 : 0.0;
			}
		}
		if (dynamicSector == 0.0)
			dynamicSectors.insert(sector);
		cpuSectors[i] = Vec4f(sector->lightlevel, dynamicSector, 0.0f, 0.0f);
	}
	mSectorTexture[mCurrentSectorTexture]->Upload(0, 0, 256, MAX(((int)level.sectors.Size() + 255) / 256, 1), 0, cpuSectors.data());
}

void HardpolyRenderer::SetupFramebuffer()
{
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
}

void HardpolyRenderer::SetupStaticLevelMesh()
{
	if (!mVertexArray || mMeshLevel != level.levelnum)
	{
		mVertexArray.reset();

		LevelMeshBuilder builder;
		builder.Generate();

		mVertexArray = builder.VertexArray;
		mDrawRuns = builder.DrawRuns;
		mMeshLevel = level.levelnum;

		cpuStaticSectorCeiling.resize(level.sectors.Size());
		cpuStaticSectorFloor.resize(level.sectors.Size());
		for (unsigned int i = 0; i < level.sectors.Size(); i++)
		{
			sector_t *sector = &level.sectors[i];
			cpuStaticSectorCeiling[i] = sector->ceilingplane.Zat0();
			cpuStaticSectorFloor[i] = sector->floorplane.Zat0();
		}
	}
}

void HardpolyRenderer::CreateSamplers()
{
	if (!mSamplerNearest)
	{
		mSamplerLinear = std::make_shared<GPUSampler>(GPUSampleMode::Linear, GPUSampleMode::Nearest, GPUMipmapMode::None, GPUWrapMode::Repeat, GPUWrapMode::Repeat);
		mSamplerNearest = std::make_shared<GPUSampler>(GPUSampleMode::Nearest, GPUSampleMode::Nearest, GPUMipmapMode::None, GPUWrapMode::Repeat, GPUWrapMode::Repeat);
	}
}

void HardpolyRenderer::RenderLevelMesh(const GPUVertexArrayPtr &vertexArray, const std::vector<LevelMeshDrawRun> &drawRuns, float meshId)
{
	SetupPerspectiveMatrix(meshId);

	mContext->SetVertexArray(vertexArray);
	mContext->SetProgram(mProgram);
	mContext->SetUniforms(0, mFrameUniforms[mCurrentFrameUniforms]);

	glUniform1i(glGetUniformLocation(mProgram->Handle(), "SectorTexture"), 0);
	glUniform1i(glGetUniformLocation(mProgram->Handle(), "DiffuseTexture"), 1);

	mContext->SetSampler(0, mSamplerNearest);
	mContext->SetTexture(0, mSectorTexture[mCurrentSectorTexture]);
	mContext->SetSampler(1, mSamplerLinear);
	for (const auto &run : drawRuns)
	{
		mContext->SetTexture(1, GetTexture(run.Texture));
		mContext->Draw(GPUDrawMode::Triangles, run.Start, run.NumVertices);
	}
	mContext->SetTexture(0, nullptr);
	mContext->SetTexture(1, nullptr);
	mContext->SetSampler(0, nullptr);
	mContext->SetSampler(1, nullptr);

	mContext->SetUniforms(0, nullptr);
	mContext->SetVertexArray(nullptr);
	mContext->SetProgram(nullptr);
}

GPUTexture2DPtr HardpolyRenderer::GetTexture(FTexture *ztexture)
{
	auto &texture = mTextures[ztexture];
	if (!texture)
	{
		int width, height;
		bool mipmap;
		std::vector<uint32_t> pixels;
		if (ztexture)
		{
			width = ztexture->GetWidth();
			height = ztexture->GetHeight();
			mipmap = true;

			pixels.resize(width * height);
			const uint32_t *src = ztexture->GetPixelsBgra();
			uint32_t *dest = pixels.data();
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					uint32_t pixel = src[y + x * height];
					uint32_t red = RPART(pixel);
					uint32_t green = GPART(pixel);
					uint32_t blue = BPART(pixel);
					uint32_t alpha = APART(pixel);
					dest[x + y * width] = red | (green << 8) | (blue << 16) | (alpha << 24);
				}
			}
		}
		else
		{
			width = 1;
			height = 1;
			mipmap = false;
			pixels.push_back(0xff00ffff);
		}

		texture = std::make_shared<GPUTexture2D>(width, height, mipmap, 0, GPUPixelFormat::RGBA8, pixels.data());
	}
	return texture;
}

void HardpolyRenderer::SetupPerspectiveMatrix(float meshId)
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
	
	mCurrentFrameUniforms = (mCurrentFrameUniforms + 1) % 3;

	if (!mFrameUniforms[mCurrentFrameUniforms])
		mFrameUniforms[mCurrentFrameUniforms] = std::make_shared<GPUUniformBuffer>(nullptr, (int)sizeof(FrameUniforms));

	FrameUniforms frameUniforms;
	frameUniforms.WorldToView = worldToView;
	frameUniforms.ViewToProjection = viewToClip;
	frameUniforms.MeshId = meshId;

	mFrameUniforms[mCurrentFrameUniforms]->Upload(&frameUniforms, (int)sizeof(FrameUniforms));
}

void HardpolyRenderer::CompileShaders()
{
	if (!mProgram)
	{
		mProgram = std::make_shared<GPUProgram>();

		mProgram->Compile(GPUShaderType::Vertex, "vertex", R"(
				layout(std140) uniform FrameUniforms
				{
					mat4 WorldToView;
					mat4 ViewToProjection;
					float MeshId;
					float Padding1, Padding2, Padding3;
				};

				in vec4 Position;
				in vec4 Texcoord;
				out vec2 UV;
				out float LightLevel;
				out vec3 PositionInView;
				uniform sampler2D SectorTexture;

				void main()
				{
					vec4 posInView = WorldToView * Position;
					PositionInView = posInView.xyz;
					gl_Position = ViewToProjection * posInView;
					UV = Texcoord.xy;
					int sector = int(Texcoord.z);
					vec4 sectorData = texelFetch(SectorTexture, ivec2(sector % 256, sector / 256), 0);
					LightLevel = sectorData.x;
					if (sectorData.y != MeshId) gl_Position = vec4(0.0);
				}
			)");
		mProgram->Compile(GPUShaderType::Fragment, "fragment", R"(
				in vec2 UV;
				in float LightLevel;
				in vec3 PositionInView;
				out vec4 FragAlbedo;
				uniform sampler2D DiffuseTexture;
				
				float SoftwareLight()
				{
					float globVis = 1706.0;
					float z = -PositionInView.z;
					float vis = globVis / z;
					float shade = 64.0 - (LightLevel + 12.0) * 32.0/128.0;
					float lightscale = clamp((shade - min(24.0, vis)) / 32.0, 0.0, 31.0/32.0);
					return 1.0 - lightscale;
				}
				
				void main()
				{
					FragAlbedo = texture(DiffuseTexture, UV);
					FragAlbedo.rgb *= SoftwareLight();
				}
			)");

		mProgram->SetAttribLocation("Position", 0);
		mProgram->SetAttribLocation("UV", 1);
		mProgram->SetFragOutput("FragAlbedo", 0);
		mProgram->SetFragOutput("FragNormal", 1);
		mProgram->Link("program");
	}
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
