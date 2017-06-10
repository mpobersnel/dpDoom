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
#include "hardpolysprite.h"

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

void HardpolyRenderer::Precache(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist)
{
}

void HardpolyRenderer::RenderView(player_t *player)
{
	R_SetupFrame(r_viewpoint, r_viewwindow, player->mo);

	P_FindParticleSubsectors();
	PO_LinkToSubsectors();

	// Never draw the player unless in chasecam mode
	ActorRenderFlags savedflags = r_viewpoint.camera->renderflags;
	if (!r_viewpoint.showviewer)
		r_viewpoint.camera->renderflags |= RF_INVISIBLE;

	FrameMemory.Clear();

	mContext->Begin();

	SetupFramebuffer();
	CompileShaders();
	CreateSamplers();
	UploadSectorTexture();
	SetupPerspectiveMatrix();
	RenderBspMesh();
	mSkyDome.Render(this);
	UpdateAutoMap();
	RenderTranslucent();

	mContext->SetFrameBuffer(nullptr);
	mContext->End();

	mPlayerSprites.Render();

	r_viewpoint.camera->renderflags = savedflags;
	interpolator.RestoreInterpolations ();

	auto swframebuffer = static_cast<OpenGLSWFrameBuffer*>(screen);
	swframebuffer->SetViewFB(mSceneFB->Handle());

	FCanvasTextureInfo::UpdateAll();
}

void HardpolyRenderer::RenderBspMesh()
{
	mBspCull.ClearSolidSegments();
	mBspCull.MarkViewFrustum();
	mBspCull.CullScene({ 0.0f, 0.0f, 0.0f, 1.0f });
	mBspCull.ClearSolidSegments();

	FindSeenSectors();

	if (!mBspCull.PvsSectors.empty())
	{
		mBspMesh.Render(this, mBspCull.PvsSectors, SeenSectors, mBspCull.MaxCeilingHeight, mBspCull.MinFloorHeight);
	}
}

void HardpolyRenderer::RenderLevelMesh(const GPUVertexArrayPtr &vertexArray, const GPUIndexBufferPtr &indexBuffer, const std::vector<LevelMeshDrawRun> &drawRuns, int skyIndexStart, int numSkyIndices)
{
	mContext->SetVertexArray(vertexArray);
	mContext->SetIndexBuffer(indexBuffer, GPUIndexFormat::Uint32);
	mContext->SetProgram(mOpaqueProgram);
	mContext->SetUniforms(0, mFrameUniforms[mCurrentFrameUniforms]);

	glUniform1i(glGetUniformLocation(mOpaqueProgram->Handle(), "SectorTexture"), 0);
	glUniform1i(glGetUniformLocation(mOpaqueProgram->Handle(), "DiffuseTexture"), 1);

	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilFunc(GL_ALWAYS, 1, 0xffffffff);

	mContext->SetSampler(0, mSamplerNearest);
	mContext->SetTexture(0, mSectorTexture[mCurrentSectorTexture]);
	mContext->SetSampler(1, mSamplerLinear);
	for (const auto &run : drawRuns)
	{
		mContext->SetTexture(1, GetTexture(run.Texture));
		mContext->DrawIndexed(GPUDrawMode::Triangles, run.Start, run.NumVertices);
	}
	mContext->SetTexture(0, nullptr);
	mContext->SetTexture(1, nullptr);
	mContext->SetSampler(0, nullptr);
	mContext->SetSampler(1, nullptr);

	if (numSkyIndices > 0)
	{
		glStencilFunc(GL_ALWAYS, 255, 0xffffffff);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		mContext->SetProgram(mStencilProgram);
		mContext->DrawIndexed(GPUDrawMode::Triangles, skyIndexStart, numSkyIndices);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	glDisable(GL_STENCIL_TEST);

	mContext->SetUniforms(0, nullptr);
	mContext->SetIndexBuffer(nullptr);
	mContext->SetVertexArray(nullptr);
	mContext->SetProgram(nullptr);
}

void HardpolyRenderer::UpdateAutoMap()
{
	// It is not decisive that the automap is fully updated every single frame.
	//
	// To reduce the time marking this for very complex maps (infamous Frozen Time)
	// we skip most of the subsectors seen. After a few frames all the lines should
	// be marked.
	
	uint32_t skip = 10; // The number of rendered frames it takes to mark all seen sectors

	uint32_t count = (uint32_t)mBspCull.PvsSectors.size();
	subsector_t **subsectors = mBspCull.PvsSectors.data();
	for (uint32_t subIndex = NextAutomapUpdate; subIndex < count; subIndex += skip)
	{
		subsector_t *sub = subsectors[subIndex];
		sector_t *frontsector = sub->sector;
		frontsector->MoreFlags |= SECF_DRAWN;
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];

			// To do: somehow only mark lines seen by the clipper
			if (line->linedef)
			{
				line->linedef->flags |= ML_MAPPED;
				sub->flags |= SSECF_DRAWN;
			}
		}
	}

	NextAutomapUpdate = (NextAutomapUpdate + 1) % skip;
}

void HardpolyRenderer::FindSeenSectors()
{
	SubsectorDepthsGeneration = ((SubsectorDepthsGeneration >> 24) + 1) << 24;

	SeenSectorFlags.clear();
	SeenSectorFlags.resize(level.sectors.Size(), false);
	SeenSectors.clear();
	SubsectorDepths.resize(level.subsectors.Size(), 0);

	uint32_t nextSubsectorDepth = 0;
	for (subsector_t *sub : mBspCull.PvsSectors)
	{
		if (!SeenSectorFlags[sub->sector->Index()])
		{
			SeenSectorFlags[sub->sector->Index()] = true;
			SeenSectors.push_back(sub->sector);
		}
		SubsectorDepths[sub->Index()] = SubsectorDepthsGeneration | (nextSubsectorDepth++);
	}
}

void HardpolyRenderer::RenderTranslucent()
{
	TranslucentObjects.clear();
	TranslucentVertices.clear();

	const auto &viewpoint = r_viewpoint;
	for (sector_t *sector : SeenSectors)
	{
		for (AActor *thing = sector->thinglist; thing != nullptr; thing = thing->snext)
		{
			DVector2 left, right;
			if (!HardpolyRenderSprite::GetLine(thing, left, right))
				continue;
			double distanceSquared = (thing->Pos() - viewpoint.Pos).LengthSquared();
			RenderSprite(thing, distanceSquared, left, right);
		}
	}

	std::stable_sort(TranslucentObjects.begin(), TranslucentObjects.end(), [](auto a, auto b) { return *a < *b; });

	for (auto it = TranslucentObjects.rbegin(); it != TranslucentObjects.rend(); ++it)
	{
		(*it)->Setup(this);
	}

	auto vertices = std::make_shared<GPUVertexBuffer>(TranslucentVertices.data(), (int)(TranslucentVertices.size() * sizeof(TranslucentVertex)));

	std::vector<GPUVertexAttributeDesc> attributes =
	{
		{ 0, 4, GPUVertexAttributeType::Float, false, sizeof(TranslucentVertex), offsetof(TranslucentVertex, Position), vertices },
		{ 1, 4, GPUVertexAttributeType::Float, false, sizeof(TranslucentVertex), offsetof(TranslucentVertex, UV), vertices }
	};

	auto vertexArray = std::make_shared<GPUVertexArray>(attributes);

	mContext->SetVertexArray(vertexArray);
	mContext->SetProgram(mTranslucentProgram);
	mContext->SetUniforms(0, mFrameUniforms[mCurrentFrameUniforms]);
	glUniform1i(glGetUniformLocation(mTranslucentProgram->Handle(), "DiffuseTexture"), 0);
	mContext->SetSampler(0, mSamplerLinear);

	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	for (auto it = TranslucentObjects.rbegin(); it != TranslucentObjects.rend(); ++it)
	{
		(*it)->Render(this);
	}

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	mContext->SetTexture(0, nullptr);
	mContext->SetSampler(0, nullptr);
	mContext->SetUniforms(0, nullptr);
	mContext->SetVertexArray(nullptr);
	mContext->SetProgram(nullptr);
}

void HardpolyRenderer::RenderSprite(AActor *thing, double sortDistance, const DVector2 &left, const DVector2 &right)
{
	if (level.nodes.Size() == 0)
	{
		subsector_t *sub = &level.subsectors[0];
		if ((SubsectorDepths[0] & 0xff000000) == SubsectorDepthsGeneration)
			TranslucentObjects.push_back(FrameMemory.NewObject<HardpolyRenderSprite>(thing, sub, SubsectorDepths[0] & 0x00ffffff, sortDistance, 0.0f, 1.0f));
	}
	else
	{
		RenderSprite(thing, sortDistance, left, right, 0.0, 1.0, level.HeadNode());
	}
}

void HardpolyRenderer::RenderSprite(AActor *thing, double sortDistance, DVector2 left, DVector2 right, double t1, double t2, void *node)
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

			RenderSprite(thing, sortDistance, mid, right, tmid, t2, bsp->children[sideRight]);
			right = mid;
			t2 = tmid;
		}
		node = bsp->children[sideLeft];
	}

	subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);

	int subsectorindex = sub->Index();
	if ((SubsectorDepths[subsectorindex] & 0xff000000) == SubsectorDepthsGeneration)
		TranslucentObjects.push_back(FrameMemory.NewObject<HardpolyRenderSprite>(thing, sub, SubsectorDepths[subsectorindex] & 0x00ffffff, sortDistance, (float)t1, (float)t2));
}

void HardpolyRenderer::UploadSectorTexture()
{
	mCurrentSectorTexture = (mCurrentSectorTexture + 1) % 3;
	if (!mSectorTexture[mCurrentSectorTexture])
		mSectorTexture[mCurrentSectorTexture] = std::make_shared<GPUTexture2D>(256, 16, false, 0, GPUPixelFormat::RGBA32f);

	cpuSectors.resize((level.sectors.Size() / 256 + 1) * 256);
	for (unsigned int i = 0; i < level.sectors.Size(); i++)
	{
		sector_t *sector = &level.sectors[i];
		cpuSectors[i] = Vec4f(sector->lightlevel, 0.0f, 0.0f, 0.0f);
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

void HardpolyRenderer::CreateSamplers()
{
	if (!mSamplerNearest)
	{
		mSamplerLinear = std::make_shared<GPUSampler>(GPUSampleMode::Linear, GPUSampleMode::Nearest, GPUMipmapMode::None, GPUWrapMode::Repeat, GPUWrapMode::Repeat);
		mSamplerNearest = std::make_shared<GPUSampler>(GPUSampleMode::Nearest, GPUSampleMode::Nearest, GPUMipmapMode::None, GPUWrapMode::Repeat, GPUWrapMode::Repeat);
	}
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

void HardpolyRenderer::SetupPerspectiveMatrix()
{
	static bool bDidSetup = false;

	if (!bDidSetup)
	{
		InitGLRMapinfoData();
		bDidSetup = true;
	}

	double radPitch = r_viewpoint.Angles.Pitch.Normalized180().Radians();
	double angx = cos(radPitch);
	double angy = sin(radPitch) * level.info->pixelstretch;
	double alen = sqrt(angx*angx + angy*angy);
	float adjustedPitch = (float)asin(angy / alen);
	float adjustedViewAngle = (float)(r_viewpoint.Angles.Yaw - 90).Radians();

	float ratio = r_viewwindow.WidescreenRatio;
	float fovratio = (r_viewwindow.WidescreenRatio >= 1.3f) ? 1.333333f : ratio;
	float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(r_viewpoint.FieldOfView.Radians() / 2) / fovratio)).Degrees);

	Mat4f worldToView =
		Mat4f::Rotate(adjustedPitch, 1.0f, 0.0f, 0.0f) *
		Mat4f::Rotate(adjustedViewAngle, 0.0f, -1.0f, 0.0f) *
		Mat4f::Scale(1.0f, level.info->pixelstretch, 1.0f) *
		Mat4f::SwapYZ() *
		Mat4f::Translate((float)-r_viewpoint.Pos.X, (float)-r_viewpoint.Pos.Y, (float)-r_viewpoint.Pos.Z);

	Mat4f viewToClip = Mat4f::Perspective(fovy, ratio, 5.0f, 65535.0f);
	
	mCurrentFrameUniforms = (mCurrentFrameUniforms + 1) % 3;

	if (!mFrameUniforms[mCurrentFrameUniforms])
		mFrameUniforms[mCurrentFrameUniforms] = std::make_shared<GPUUniformBuffer>(nullptr, (int)sizeof(FrameUniforms));

	FrameUniforms frameUniforms;
	frameUniforms.WorldToView = worldToView;
	frameUniforms.ViewToProjection = viewToClip;

	mFrameUniforms[mCurrentFrameUniforms]->Upload(&frameUniforms, (int)sizeof(FrameUniforms));

	if (!mSkyFrameUniforms[mCurrentFrameUniforms])
		mSkyFrameUniforms[mCurrentFrameUniforms] = std::make_shared<GPUUniformBuffer>(nullptr, (int)sizeof(FrameUniforms));

	FrameUniforms skyFrameUniforms;
	skyFrameUniforms.WorldToView = worldToView * Mat4f::Translate((float)r_viewpoint.Pos.X, (float)r_viewpoint.Pos.Y, (float)r_viewpoint.Pos.Z);
	skyFrameUniforms.ViewToProjection = viewToClip;

	mSkyFrameUniforms[mCurrentFrameUniforms]->Upload(&skyFrameUniforms, (int)sizeof(FrameUniforms));
}

void HardpolyRenderer::CompileShaders()
{
	if (!mOpaqueProgram)
	{
		mOpaqueProgram = std::make_shared<GPUProgram>();

		mOpaqueProgram->Compile(GPUShaderType::Vertex, "vertex", R"(
				layout(std140) uniform FrameUniforms
				{
					mat4 WorldToView;
					mat4 ViewToProjection;
				};

				in vec4 Position;
				in vec4 Texcoord;
				out vec2 UV;
				out float LightLevel;
				out vec3 PositionInView;
				out float AlphaTest;
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
					AlphaTest = Texcoord.w;
				}
			)");
		mOpaqueProgram->Compile(GPUShaderType::Fragment, "fragment", R"(
				in vec2 UV;
				in float LightLevel;
				in vec3 PositionInView;
				in float AlphaTest;
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
					if (FragAlbedo.a < AlphaTest) discard;
					FragAlbedo.rgb *= SoftwareLight();
				}
			)");

		mOpaqueProgram->SetAttribLocation("Position", 0);
		mOpaqueProgram->SetAttribLocation("UV", 1);
		mOpaqueProgram->SetFragOutput("FragAlbedo", 0);
		mOpaqueProgram->SetFragOutput("FragNormal", 1);
		mOpaqueProgram->Link("program");
	}

	if (!mTranslucentProgram)
	{
		mTranslucentProgram = std::make_shared<GPUProgram>();

		mTranslucentProgram->Compile(GPUShaderType::Vertex, "vertex", R"(
				layout(std140) uniform FrameUniforms
				{
					mat4 WorldToView;
					mat4 ViewToProjection;
				};

				in vec4 Position;
				in vec4 Texcoord;
				out vec2 UV;
				out float LightLevel;
				out vec3 PositionInView;

				void main()
				{
					vec4 posInView = WorldToView * Position;
					PositionInView = posInView.xyz;
					gl_Position = ViewToProjection * posInView;
					UV = Texcoord.xy;
					LightLevel = Texcoord.z;
				}
			)");
		mTranslucentProgram->Compile(GPUShaderType::Fragment, "fragment", R"(
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

		mTranslucentProgram->SetAttribLocation("Position", 0);
		mTranslucentProgram->SetAttribLocation("UV", 1);
		mTranslucentProgram->SetFragOutput("FragAlbedo", 0);
		mTranslucentProgram->SetFragOutput("FragNormal", 1);
		mTranslucentProgram->Link("program");
	}

	if (!mSkyProgram)
	{
		mSkyProgram = std::make_shared<GPUProgram>();

		mSkyProgram->Compile(GPUShaderType::Vertex, "vertex", R"(
				layout(std140) uniform FrameUniforms
				{
					mat4 WorldToView;
					mat4 ViewToProjection;
				};

				in vec4 Position;
				in vec4 Texcoord;
				out vec2 UV;
				out vec3 PositionInView;

				void main()
				{
					vec4 posInView = WorldToView * Position;
					PositionInView = posInView.xyz;
					gl_Position = ViewToProjection * posInView;
					UV = Texcoord.xy;
				}
			)");
		mSkyProgram->Compile(GPUShaderType::Fragment, "fragment", R"(
				in vec2 UV;
				in vec3 PositionInView;
				out vec4 FragAlbedo;
				uniform sampler2D FrontTexture;
				uniform vec4 CapColor;
				uniform vec4 OffsetScale;
				
				void main()
				{
					vec2 finalUV = OffsetScale.xy + OffsetScale.zw * UV;
					float skyfade = min(clamp(finalUV.y * 4.0, 0.0, 1.0), clamp((2.0 - finalUV.y) * 4.0, 0.0, 1.0));
					FragAlbedo = mix(CapColor, texture(FrontTexture, finalUV), skyfade);
				}
			)");

		mSkyProgram->SetAttribLocation("Position", 0);
		mSkyProgram->SetAttribLocation("UV", 1);
		mSkyProgram->SetFragOutput("FragAlbedo", 0);
		mSkyProgram->SetFragOutput("FragNormal", 1);
		mSkyProgram->Link("program");
	}

	if (!mStencilProgram)
	{
		mStencilProgram = std::make_shared<GPUProgram>();

		mStencilProgram->Compile(GPUShaderType::Vertex, "vertex", R"(
				layout(std140) uniform FrameUniforms
				{
					mat4 WorldToView;
					mat4 ViewToProjection;
				};

				in vec4 Position;

				void main()
				{
					vec4 posInView = WorldToView * Position;
					gl_Position = ViewToProjection * posInView;
				}
			)");
		mStencilProgram->Compile(GPUShaderType::Fragment, "fragment", R"(
				out vec4 FragAlbedo;
				void main()
				{
					FragAlbedo = vec4(1.0);
				}
			)");

		mStencilProgram->SetAttribLocation("Position", 0);
		mStencilProgram->SetFragOutput("FragAlbedo", 0);
		mStencilProgram->SetFragOutput("FragNormal", 1);
		mStencilProgram->Link("program");
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
	mPlayerSprites.RenderRemainingSprites();
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
	//gl_ParseDefs();
	mContext = std::make_shared<GPUContext>();
}

void HardpolyRenderer::SetClearColor(int color)
{
}

void HardpolyRenderer::RenderTextureView(FCanvasTexture *tex, AActor *viewpoint, int fov)
{
	tex->SetUpdated();
}

void HardpolyRenderer::PreprocessLevel()
{
	gl_PreprocessLevel();
}

void HardpolyRenderer::CleanLevelData()
{
	gl_CleanLevelData();
}
