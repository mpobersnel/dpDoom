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
#include "gl/system/gl_system.h"
#include "gl/system/gl_swframebuffer.h"
#include "gl/data/gl_data.h"
#include "po_man.h"
#include "r_data/r_interpolate.h"
#include "p_effect.h"
#include "g_levellocals.h"

HardpolyRenderer::HardpolyRenderer()
{
	mContext = std::make_shared<GPUContext>();
}

HardpolyRenderer::~HardpolyRenderer()
{
}

void HardpolyRenderer::Begin()
{
	mContext->Begin();
	SetupFramebuffer();
	CompileShaders();
	CreateSamplers();
	for (auto &thread : PolyRenderer::Instance()->Threads.Threads)
	{
		thread->DrawBatcher.NextFrame();
	}
}

void HardpolyRenderer::End()
{
	for (auto &thread : PolyRenderer::Instance()->Threads.Threads)
	{
		thread->DrawBatcher.DrawBatches(this);
		thread->DrawBatcher.NextFrame();
	}

	mContext->SetViewport(0, 0, screen->GetWidth(), screen->GetHeight());
	mContext->End();

	auto swframebuffer = static_cast<OpenGLSWFrameBuffer*>(screen);
	swframebuffer->SetViewFB(mSceneFB->Handle());
}

void HardpolyRenderer::ClearBuffers(DCanvas *canvas)
{
	for (auto &thread : PolyRenderer::Instance()->Threads.Threads)
	{
		thread->DrawBatcher.DrawBatches(this);
	}

	mContext->ClearColorBuffer(0, 0.5f, 0.5f, 0.2f, 1.0f);
	mContext->ClearColorBuffer(1, 0.0f, 0.0f, 0.0f, 0.0f);
	mContext->ClearColorBuffer(2, 1.0f, 0.0f, 0.0f, 0.0f);
	mContext->ClearDepthStencilBuffer(1.0f, 0);
}

void HardpolyRenderer::SetViewport(int x, int y, int width, int height, DCanvas *canvas)
{
	for (auto &thread : PolyRenderer::Instance()->Threads.Threads)
	{
		thread->DrawBatcher.DrawBatches(this);
	}

	mContext->SetViewport(x, y, width, height);
}

void HardpolyRenderer::DrawElements(PolyRenderThread *thread, const PolyDrawArgs &drawargs)
{
	if (!drawargs.WriteColor())
		return;

	bool ccw = drawargs.FaceCullCCW();
	int totalvcount = drawargs.VertexCount();
	if (totalvcount < 3)
		return;

	DrawRun run;
	run.Texture = drawargs.Texture();
	if (!run.Texture)
	{
		run.Pixels = drawargs.TexturePixels();
		run.PixelsWidth = drawargs.TextureWidth();
		run.PixelsHeight = drawargs.TextureHeight();
	}
	run.Translation = drawargs.Translation();
	run.BaseColormap = drawargs.BaseColormap();
	run.BlendMode = drawargs.BlendMode();
	run.SrcAlpha = drawargs.SrcAlpha();
	run.DestAlpha = drawargs.DestAlpha();
	run.DepthTest = drawargs.DepthTest();
	run.WriteDepth = drawargs.WriteDepth();

	FaceUniforms uniforms;
	uniforms.AlphaTest = 0.5f;
	uniforms.Light = drawargs.Light();
	if (drawargs.FixedLight())
		uniforms.Light = -uniforms.Light - 1;
	uniforms.Mode = GetSamplerMode(drawargs.BlendMode());
	uniforms.FillColor.X = RPART(drawargs.Color()) / 255.0f;
	uniforms.FillColor.Y = GPART(drawargs.Color()) / 255.0f;
	uniforms.FillColor.Z = BPART(drawargs.Color()) / 255.0f;
	uniforms.FillColor.W = drawargs.Color();
	uniforms.ClipPlane0 = { drawargs.ClipPlane(0).A, drawargs.ClipPlane(0).B, drawargs.ClipPlane(0).C, drawargs.ClipPlane(0).D };
	uniforms.ClipPlane1 = { drawargs.ClipPlane(1).A, drawargs.ClipPlane(1).B, drawargs.ClipPlane(1).C, drawargs.ClipPlane(1).D };
	uniforms.ClipPlane2 = { drawargs.ClipPlane(2).A, drawargs.ClipPlane(2).B, drawargs.ClipPlane(2).C, drawargs.ClipPlane(2).D };

	DrawRunKey key(run);

	const int indexBatchCount = 500 * 3;
	for (int indexRun = 0; indexRun < totalvcount; indexRun += indexBatchCount)
	{
		int vcount = MIN(indexRun + indexBatchCount, totalvcount) - indexRun;
		thread->DrawBatcher.GetVertices(vcount);

		if (thread->DrawBatcher.mCurrentBatch->MatrixUpdateGeneration != thread->DrawBatcher.MatrixUpdateGeneration || thread->DrawBatcher.mCurrentBatch->CpuMatrices.empty())
		{
			thread->DrawBatcher.mCurrentBatch->CpuMatrices.push_back(thread->DrawBatcher.GetMatrixUniforms());
		}

		run.MatrixUniformsIndex = (int)(thread->DrawBatcher.mCurrentBatch->CpuMatrices.size() - 1);
		key.MatrixUniformsIndex = run.MatrixUniformsIndex;

		float faceIndex = (float)thread->DrawBatcher.mCurrentBatch->CpuFaceUniforms.size();
		auto vsrc = drawargs.Vertices();
		auto vdest = thread->DrawBatcher.mVertices + thread->DrawBatcher.mNextVertex;
		auto elements = drawargs.Elements();
		for (int i = 0; i < vcount; i++)
		{
			vdest[i] = vsrc[elements[indexRun + i]];
			vdest[i].w = faceIndex;
		}

		int startv = thread->DrawBatcher.mNextVertex;
		auto &indexBuffer = thread->DrawBatcher.mCurrentBatch->CpuIndexBuffer;
		run.Start = (int)indexBuffer.size();
		for (int i = 0; i < vcount; i++)
			indexBuffer.push_back(startv + i);
		run.NumVertices = vcount;

		auto &drawRuns = thread->DrawBatcher.mCurrentBatch->SortedDrawRuns[key];
		drawRuns.push_back(run);
		thread->DrawBatcher.mCurrentBatch->HasSortedDrawRuns = true;

		thread->DrawBatcher.mNextVertex += vcount;
		thread->DrawBatcher.mCurrentBatch->CpuFaceUniforms.push_back(uniforms);
	}
}

void HardpolyRenderer::DrawArray(PolyRenderThread *thread, const PolyDrawArgs &drawargs)
{
	if (!drawargs.WriteColor())
		return;

	bool ccw = drawargs.FaceCullCCW();
	int vcount = drawargs.VertexCount();
	if (vcount < 3)
		return;

	thread->DrawBatcher.GetVertices(vcount);

	if (thread->DrawBatcher.mCurrentBatch->MatrixUpdateGeneration != thread->DrawBatcher.MatrixUpdateGeneration || thread->DrawBatcher.mCurrentBatch->CpuMatrices.empty())
	{
		thread->DrawBatcher.mCurrentBatch->CpuMatrices.push_back(thread->DrawBatcher.GetMatrixUniforms());
	}

	DrawRun run;
	run.Texture = drawargs.Texture();
	if (!run.Texture)
	{
		run.Pixels = drawargs.TexturePixels();
		run.PixelsWidth = drawargs.TextureWidth();
		run.PixelsHeight = drawargs.TextureHeight();
	}
	run.Translation = drawargs.Translation();
	run.BaseColormap = drawargs.BaseColormap();
	run.BlendMode = drawargs.BlendMode();
	run.SrcAlpha = drawargs.SrcAlpha();
	run.DestAlpha = drawargs.DestAlpha();
	run.DepthTest = drawargs.DepthTest();
	run.WriteDepth = drawargs.WriteDepth();
	run.MatrixUniformsIndex = (int)(thread->DrawBatcher.mCurrentBatch->CpuMatrices.size() - 1);

	FaceUniforms uniforms;
	uniforms.AlphaTest = 0.5f;
	uniforms.Light = drawargs.Light();
	if (drawargs.FixedLight())
		uniforms.Light = -uniforms.Light - 1;
	uniforms.Mode = GetSamplerMode(drawargs.BlendMode());
	uniforms.FillColor.X = RPART(drawargs.Color()) / 255.0f;
	uniforms.FillColor.Y = GPART(drawargs.Color()) / 255.0f;
	uniforms.FillColor.Z = BPART(drawargs.Color()) / 255.0f;
	uniforms.FillColor.W = drawargs.Color();
	uniforms.ClipPlane0 = { drawargs.ClipPlane(0).A, drawargs.ClipPlane(0).B, drawargs.ClipPlane(0).C, drawargs.ClipPlane(0).D };
	uniforms.ClipPlane1 = { drawargs.ClipPlane(1).A, drawargs.ClipPlane(1).B, drawargs.ClipPlane(1).C, drawargs.ClipPlane(1).D };
	uniforms.ClipPlane2 = { drawargs.ClipPlane(2).A, drawargs.ClipPlane(2).B, drawargs.ClipPlane(2).C, drawargs.ClipPlane(2).D };

	float faceIndex = (float)thread->DrawBatcher.mCurrentBatch->CpuFaceUniforms.size();
	auto vdest = thread->DrawBatcher.mVertices + thread->DrawBatcher.mNextVertex;
	memcpy(vdest, drawargs.Vertices(), sizeof(TriVertex) * vcount);
	for (int i = 0; i < vcount; i++)
		vdest[i].w = faceIndex;

	int startv = thread->DrawBatcher.mNextVertex;
	auto &indexBuffer = thread->DrawBatcher.mCurrentBatch->CpuIndexBuffer;
	run.Start = (int)indexBuffer.size();
	switch (drawargs.DrawMode())
	{
	case PolyDrawMode::Triangles:
		for (int i = 0; i < vcount; i++)
			indexBuffer.push_back(startv + i);
		run.NumVertices = vcount;
		break;
	case PolyDrawMode::TriangleStrip:
		{
			int i;
			for (i = 0; i < vcount - 3; i += 2)
			{
				indexBuffer.push_back(startv + i);
				indexBuffer.push_back(startv + i + 1);
				indexBuffer.push_back(startv + i + 2);

				indexBuffer.push_back(startv + i + 3);
				indexBuffer.push_back(startv + i + 2);
				indexBuffer.push_back(startv + i + 1);
			}
			if (i < vcount - 2)
			{
				indexBuffer.push_back(startv + i);
				indexBuffer.push_back(startv + i + 1);
				indexBuffer.push_back(startv + i + 2);
			}
			run.NumVertices = MAX(vcount - 2, 0) * 3;
		}
		break;
	case PolyDrawMode::TriangleFan:
		for (int i = 1; i < vcount - 1; i++)
		{
			indexBuffer.push_back(startv);
			indexBuffer.push_back(startv + i);
			indexBuffer.push_back(startv + i + 1);
		}
		run.NumVertices = MAX(vcount - 2, 0) * 3;
		break;
	}

	DrawRunKey key(run);

	auto &drawRuns = thread->DrawBatcher.mCurrentBatch->SortedDrawRuns[key];
	drawRuns.push_back(run);
	thread->DrawBatcher.mCurrentBatch->HasSortedDrawRuns = true;
/*
	if (!thread->DrawBatcher.mCurrentBatch->DrawRuns.empty())
	{
		auto &prevRun = thread->DrawBatcher.mCurrentBatch->DrawRuns.back();
		if (prevRun.Start + prevRun.NumVertices == run.Start && key == thread->DrawBatcher.mCurrentBatch->LastRunKey)
		{
			prevRun.NumVertices += run.NumVertices;
		}
		else
		{
			thread->DrawBatcher.mCurrentBatch->DrawRuns.push_back(run);
			thread->DrawBatcher.mCurrentBatch->LastRunKey = key;
		}
	}
	else
	{
		thread->DrawBatcher.mCurrentBatch->DrawRuns.push_back(run);
		thread->DrawBatcher.mCurrentBatch->LastRunKey = key;
	}
*/

	thread->DrawBatcher.mNextVertex += vcount;
	thread->DrawBatcher.mCurrentBatch->CpuFaceUniforms.push_back(uniforms);
}

void HardpolyRenderer::DrawRect(PolyRenderThread *thread, const RectDrawArgs &args)
{
	MatrixUniforms cpuMatrixUniforms = thread->DrawBatcher.GetMatrixUniforms();
	if (!mMatrixUniforms)
		mMatrixUniforms = std::make_shared<GPUUniformBuffer>(nullptr, (int)sizeof(MatrixUniforms));
	mMatrixUniforms->Upload(&cpuMatrixUniforms, (int)sizeof(MatrixUniforms));

	if (!mScreenQuad)
	{
		Vec2f quad[4] =
		{
			{ 0.0f, 0.0f },
			{ 1.0f, 0.0f },
			{ 0.0f, 1.0f },
			{ 1.0f, 1.0f },
		};
		mScreenQuadVertexBuffer = std::make_shared<GPUVertexBuffer>(quad, (int)(sizeof(Vec2f) * 4));
		std::vector<GPUVertexAttributeDesc> desc =
		{
			{ 0, 2, GPUVertexAttributeType::Float, false, 0, 0, mScreenQuadVertexBuffer }
		};
		mScreenQuad = std::make_shared<GPUVertexArray>(desc);
	}

	if (!mRectUniforms)
	{
		mRectUniforms = std::make_shared<GPUUniformBuffer>(nullptr, (int)sizeof(RectUniforms));
	}

	RectUniforms uniforms;
	uniforms.x0 = args.X0() / (float)screen->GetWidth() * 2.0f - 1.0f;
	uniforms.x1 = args.X1() / (float)screen->GetWidth() * 2.0f - 1.0f;
	uniforms.y0 = args.Y0() / (float)screen->GetHeight() * 2.0f - 1.0f;
	uniforms.y1 = args.Y1() / (float)screen->GetHeight() * 2.0f - 1.0f;
	uniforms.u0 = args.U0();
	uniforms.v0 = args.V0();
	uniforms.u1 = args.U1();
	uniforms.v1 = args.V1();
	uniforms.Light = args.Light();
	mRectUniforms->Upload(&uniforms, (int)sizeof(RectUniforms));

	mContext->SetVertexArray(mScreenQuad);
	mContext->SetProgram(mRectProgram);

	int loc = glGetUniformLocation(mRectProgram->Handle(), "DiffuseTexture");
	if (loc != -1)
		glUniform1i(loc, 0);

	loc = glGetUniformLocation(mRectProgram->Handle(), "BasecolormapTexture");
	if (loc != -1)
		glUniform1i(loc, 1);

	loc = glGetUniformLocation(mRectProgram->Handle(), "TranslationTexture");
	if (loc != -1)
		glUniform1i(loc, 2);

	mContext->SetUniforms(0, mMatrixUniforms);
	mContext->SetUniforms(1, mRectUniforms);
	mContext->SetSampler(0, mSamplerNearest);
	mContext->SetSampler(1, mSamplerNearest);
	mContext->SetTexture(0, GetTexture(args.Texture(), args.Translation() != nullptr));
	mContext->SetTexture(1, GetColormapTexturePal(args.BaseColormap()));

	mContext->Draw(GPUDrawMode::TriangleStrip, 0, 4);

	mContext->SetTexture(0, nullptr);
	mContext->SetTexture(1, nullptr);
	mContext->SetSampler(0, nullptr);
	mContext->SetSampler(1, nullptr);
	mContext->SetUniforms(0, nullptr);
	mContext->SetUniforms(1, nullptr);
	mContext->SetVertexArray(nullptr);
	mContext->SetProgram(nullptr);
}

void HardpolyRenderer::RenderBatch(DrawBatch *batch)
{
	if (!mMatrixUniforms)
		mMatrixUniforms = std::make_shared<GPUUniformBuffer>(nullptr, (int)sizeof(MatrixUniforms));

	mContext->SetVertexArray(batch->VertexArray);
	mContext->SetIndexBuffer(batch->IndexBuffer);
	mContext->SetProgram(mOpaqueProgram);
	mContext->SetUniforms(0, mMatrixUniforms);
	//mContext->SetUniforms(1, batch->FaceUniforms);

	int loc = glGetUniformLocation(mOpaqueProgram->Handle(), "FaceUniformsTexture");
	if (loc != -1)
		glUniform1i(loc, 0);

	loc = glGetUniformLocation(mOpaqueProgram->Handle(), "DiffuseTexture");
	if (loc != -1)
		glUniform1i(loc, 1);

	loc = glGetUniformLocation(mOpaqueProgram->Handle(), "BasecolormapTexture");
	if (loc != -1)
		glUniform1i(loc, 2);

	loc = glGetUniformLocation(mOpaqueProgram->Handle(), "TranslationTexture");
	if (loc != -1)
		glUniform1i(loc, 3);

	//glEnable(GL_STENCIL_TEST);
	//glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	//glStencilFunc(GL_ALWAYS, 1, 0xffffffff);

	glEnable(GL_CLIP_DISTANCE0);
	glEnable(GL_CLIP_DISTANCE1);
	glEnable(GL_CLIP_DISTANCE2);

	mContext->SetSampler(0, mSamplerNearest);
	mContext->SetSampler(1, mSamplerNearest);
	mContext->SetSampler(2, mSamplerNearest);
	mContext->SetSampler(3, mSamplerNearest);

	mContext->SetTexture(0, batch->FaceUniformsTexture);

	DrawRunKey last;

	for (const auto &run : batch->DrawRuns)
	{
		//glDepthFunc(run.DepthTest ? GL_LESS : GL_ALWAYS);
		//glDepthMask(run.WriteDepth ? GL_TRUE : GL_FALSE);

		DrawRunKey current(run);
		if (current != last)
		{
			if (run.MatrixUniformsIndex != last.MatrixUniformsIndex || last.MatrixUniformsIndex == -1)
			{
				mMatrixUniforms->Upload(batch->CpuMatrices.data() + run.MatrixUniformsIndex, (int)sizeof(MatrixUniforms));
			}

			if (current.BlendMode != last.BlendMode || (current.BlendMode == last.BlendMode && (current.SrcAlpha != last.SrcAlpha || current.DestAlpha != last.DestAlpha)))
			{
				BlendSetterFunc blendSetter = GetBlendSetter(run.BlendMode);
				(*this.*blendSetter)(run.SrcAlpha, run.DestAlpha);
			}

			if (run.Texture && last.Texture != current.Texture)
				mContext->SetTexture(1, GetTexture(run.Texture, run.Translation != nullptr));
			else if (run.Pixels && last.Pixels != current.Pixels)
				mContext->SetTexture(1, GetEngineTexturePal(run.Pixels, run.PixelsWidth, run.PixelsHeight));

			if (last.BaseColormap != current.BaseColormap)
				mContext->SetTexture(2, GetColormapTexturePal(run.BaseColormap));

			if (run.Translation && last.Translation != current.Translation)
				mContext->SetTexture(3, GetTranslationTexture(run.Translation));

			last = current;
		}

		mContext->DrawIndexed(GPUDrawMode::Triangles, run.Start, run.NumVertices);

		PolyTotalTriangles += run.NumVertices / 3;
		PolyTotalDrawCalls++;
	}
	mContext->SetTexture(0, nullptr);
	mContext->SetTexture(1, nullptr);
	mContext->SetTexture(2, nullptr);
	mContext->SetTexture(3, nullptr);
	mContext->SetSampler(0, nullptr);
	mContext->SetSampler(1, nullptr);
	mContext->SetSampler(2, nullptr);
	mContext->SetSampler(3, nullptr);

	glDisable(GL_CLIP_DISTANCE0);
	glDisable(GL_CLIP_DISTANCE1);
	glDisable(GL_CLIP_DISTANCE2);

	//glDisable(GL_STENCIL_TEST);

	mContext->SetUniforms(0, nullptr);
	//mContext->SetUniforms(1, nullptr);
	mContext->SetIndexBuffer(nullptr);
	mContext->SetVertexArray(nullptr);
	mContext->SetProgram(nullptr);
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
		mSpriteDepthBuffer.reset();

		mAlbedoBuffer = std::make_shared<GPUTexture2D>(width, height, false, 0, GPUPixelFormat::RGBA16f);
		mNormalBuffer = std::make_shared<GPUTexture2D>(width, height, false, 0, GPUPixelFormat::RGBA16f);
		mDepthStencilBuffer = std::make_shared<GPUTexture2D>(width, height, false, 0, GPUPixelFormat::Depth24_Stencil8);
		mSpriteDepthBuffer = std::make_shared<GPUTexture2D>(width, height, false, 0, GPUPixelFormat::R32f);

		std::vector<std::shared_ptr<GPUTexture2D>> colorbuffers = { mAlbedoBuffer, mNormalBuffer, mSpriteDepthBuffer };
		mSceneFB = std::make_shared<GPUFrameBuffer>(colorbuffers, mDepthStencilBuffer);

		std::vector<std::shared_ptr<GPUTexture2D>> translucentcolorbuffers = { mAlbedoBuffer, mNormalBuffer };
		mTranslucentFB = std::make_shared<GPUFrameBuffer>(translucentcolorbuffers, mDepthStencilBuffer);
	}

	mContext->SetFrameBuffer(mSceneFB);

	GLenum drawbuffers[3] = { GL_COLOR_ATTACHMENT0 + 0, GL_COLOR_ATTACHMENT0 + 1, GL_COLOR_ATTACHMENT0 + 2 };
	glDrawBuffers(3, drawbuffers);
}

void HardpolyRenderer::CreateSamplers()
{
	if (!mSamplerNearest)
	{
		mSamplerLinear = std::make_shared<GPUSampler>(GPUSampleMode::Linear, GPUSampleMode::Nearest, GPUMipmapMode::None, GPUWrapMode::Repeat, GPUWrapMode::Repeat);
		mSamplerNearest = std::make_shared<GPUSampler>(GPUSampleMode::Nearest, GPUSampleMode::Nearest, GPUMipmapMode::None, GPUWrapMode::Repeat, GPUWrapMode::Repeat);
	}
}

std::shared_ptr<GPUTexture2D> HardpolyRenderer::GetTranslationTexture(const uint8_t *translation)
{
	if (PolyRenderer::Instance()->RenderTarget->IsBgra())
		return GetTranslationTextureBgra(translation);
	else
		return GetTranslationTexturePal(translation);
}

std::shared_ptr<GPUTexture2D> HardpolyRenderer::GetTranslationTextureBgra(const uint8_t *translation)
{
	auto &texture = mTranslationTexturesBgra[translation];
	if (!texture)
	{
		uint32_t *src = (uint32_t*)translation;
		uint32_t dest[256];
		for (int x = 0; x < 256; x++)
		{
			uint32_t pixel = src[x];
			uint32_t red = RPART(pixel);
			uint32_t green = GPART(pixel);
			uint32_t blue = BPART(pixel);
			uint32_t alpha = APART(pixel);
			dest[x] = red | (green << 8) | (blue << 16) | (alpha << 24);
		}

		texture = std::make_shared<GPUTexture2D>(256, 1, false, 0, GPUPixelFormat::RGBA8, dest);
	}
	return texture;
}

std::shared_ptr<GPUTexture2D> HardpolyRenderer::GetTranslationTexturePal(const uint8_t *translation)
{
	auto &texture = mTranslationTexturesPal[translation];
	if (!texture)
	{
		texture = std::make_shared<GPUTexture2D>(256, 1, false, 0, GPUPixelFormat::R8, translation);
	}
	return texture;
}

std::shared_ptr<GPUTexture2D> HardpolyRenderer::GetEngineTexturePal(const uint8_t *src, int width, int height)
{
	auto &texture = mEngineTexturesPal[src];
	if (!texture)
	{
		std::vector<uint8_t> pixels;
		if (src)
		{
			pixels.resize(width * height);
			uint8_t *dest = pixels.data();
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					dest[x + y * width] = src[y + x * height];
				}
			}
		}
		else
		{
			width = 1;
			height = 1;
			pixels.push_back(0);
		}

		texture = std::make_shared<GPUTexture2D>(width, height, false, 0, GPUPixelFormat::R8, pixels.data());
	}
	return texture;
}

std::shared_ptr<GPUTexture2D> HardpolyRenderer::GetColormapTexturePal(const uint8_t *basecolormap)
{
	auto &texture = mColormapsPal[basecolormap];
	if (!texture)
	{
		uint32_t rgbacolormap[256 * NUMCOLORMAPS];
		for (int i = 0; i < 256 * NUMCOLORMAPS; i++)
		{
			const auto &entry = GPalette.BaseColors[basecolormap[i]];
			uint32_t red = entry.r;
			uint32_t green = entry.g;
			uint32_t blue = entry.b;
			uint32_t alpha = 255;
			rgbacolormap[i] = red | (green << 8) | (blue << 16) | (alpha << 24);
		}
		texture = std::make_shared<GPUTexture2D>(256, NUMCOLORMAPS, false, 0, GPUPixelFormat::RGBA8, rgbacolormap);
	}
	return texture;
}

std::shared_ptr<GPUTexture2D> HardpolyRenderer::GetTextureBgra(FTexture *ztexture)
{
	auto &texture = mTexturesBgra[ztexture];
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

std::shared_ptr<GPUTexture2D> HardpolyRenderer::GetTexture(FTexture *texture, bool translated)
{
	if (!translated && PolyRenderer::Instance()->RenderTarget->IsBgra())
		return GetTextureBgra(texture);
	else
		return GetTexturePal(texture);
}

std::shared_ptr<GPUTexture2D> HardpolyRenderer::GetTexturePal(FTexture *ztexture)
{
	auto &texture = mTexturesPal[ztexture];
	if (!texture)
	{
		int width, height;
		std::vector<uint8_t> pixels;
		if (ztexture)
		{
			width = ztexture->GetWidth();
			height = ztexture->GetHeight();

			pixels.resize(width * height);
			const uint8_t *src = ztexture->GetPixels();
			uint8_t *dest = pixels.data();
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					dest[x + y * width] = src[y + x * height];
				}
			}
		}
		else
		{
			width = 1;
			height = 1;
			pixels.push_back(0);
		}

		texture = std::make_shared<GPUTexture2D>(width, height, false, 0, GPUPixelFormat::R8, pixels.data());
	}
	return texture;
}

void HardpolyRenderer::CompileShaders()
{
	if (!mOpaqueProgram)
	{
		mOpaqueProgram = std::make_shared<GPUProgram>();
		if (PolyRenderer::Instance()->RenderTarget->IsBgra()) mOpaqueProgram->SetDefine("TRUECOLOR");
		mOpaqueProgram->Compile(GPUShaderType::Vertex, "shaders/hardpoly/opaque.vp");
		mOpaqueProgram->Compile(GPUShaderType::Fragment, "shaders/hardpoly/opaque.fp");
		mOpaqueProgram->SetAttribLocation("Position", 0);
		mOpaqueProgram->SetAttribLocation("UV", 1);
		mOpaqueProgram->SetFragOutput("FragColor", 0);
		mOpaqueProgram->Link("program");
		mOpaqueProgram->SetUniformBlock("FrameUniforms", 0);
	}

	if (!mRectProgram)
	{
		mRectProgram = std::make_shared<GPUProgram>();
		if (PolyRenderer::Instance()->RenderTarget->IsBgra()) mRectProgram->SetDefine("TRUECOLOR");
		mRectProgram->Compile(GPUShaderType::Vertex, "shaders/hardpoly/rect.vp");
		mRectProgram->Compile(GPUShaderType::Fragment, "shaders/hardpoly/rect.fp");
		mRectProgram->SetAttribLocation("Position", 0);
		mRectProgram->SetAttribLocation("UV", 1);
		mRectProgram->SetFragOutput("FragColor", 0);
		mRectProgram->Link("program");
		mRectProgram->SetUniformBlock("FrameUniforms", 0);
		mRectProgram->SetUniformBlock("RectUniforms", 1);
	}

	if (!mStencilProgram)
	{
		mStencilProgram = std::make_shared<GPUProgram>();
		if (PolyRenderer::Instance()->RenderTarget->IsBgra()) mStencilProgram->SetDefine("TRUECOLOR");
		mStencilProgram->Compile(GPUShaderType::Vertex, "shaders/hardpoly/stencil.vp");
		mStencilProgram->Compile(GPUShaderType::Fragment, "shaders/hardpoly/stencil.fp");
		mStencilProgram->SetAttribLocation("Position", 0);
		mStencilProgram->SetFragOutput("FragColor", 0);
		mStencilProgram->SetFragOutput("FragNormal", 1);
		mStencilProgram->Link("program");
	}
}

int HardpolyRenderer::GetSamplerMode(TriBlendMode triblend)
{
	enum { Texture, Translated, Shaded, Stencil, Fill, Skycap, Fuzz, FogBoundary };
	static int modes[] =
	{
		Texture,         // TextureOpaque
		Texture,         // TextureMasked
		Texture,         // TextureAdd
		Texture,         // TextureSub
		Texture,         // TextureRevSub
		Texture,         // TextureAddSrcColor
		Translated,      // TranslatedOpaque
		Translated,      // TranslatedMasked
		Translated,      // TranslatedAdd
		Translated,      // TranslatedSub
		Translated,      // TranslatedRevSub
		Translated,      // TranslatedAddSrcColor
		Shaded,          // Shaded
		Shaded,          // AddShaded
		Stencil,         // Stencil
		Stencil,         // AddStencil
		Fill,            // FillOpaque
		Fill,            // FillAdd
		Fill,            // FillSub
		Fill,            // FillRevSub
		Fill,            // FillAddSrcColor
		Skycap,          // Skycap
		Fuzz,            // Fuzz
		FogBoundary,     // FogBoundary
	};
	return modes[(int)triblend];
}

HardpolyRenderer::BlendSetterFunc HardpolyRenderer::GetBlendSetter(TriBlendMode triblend)
{
	static BlendSetterFunc blendsetters[] =
	{
		&HardpolyRenderer::SetOpaqueBlend,         // TextureOpaque
		&HardpolyRenderer::SetMaskedBlend,         // TextureMasked
		&HardpolyRenderer::SetAddClampBlend,       // TextureAdd
		&HardpolyRenderer::SetSubClampBlend,       // TextureSub
		&HardpolyRenderer::SetRevSubClampBlend,    // TextureRevSub
		&HardpolyRenderer::SetAddSrcColorBlend,    // TextureAddSrcColor
		&HardpolyRenderer::SetOpaqueBlend,         // TranslatedOpaque
		&HardpolyRenderer::SetMaskedBlend,         // TranslatedMasked
		&HardpolyRenderer::SetAddClampBlend,       // TranslatedAdd
		&HardpolyRenderer::SetSubClampBlend,       // TranslatedSub
		&HardpolyRenderer::SetRevSubClampBlend,    // TranslatedRevSub
		&HardpolyRenderer::SetAddSrcColorBlend,    // TranslatedAddSrcColor
		&HardpolyRenderer::SetShadedBlend,         // Shaded
		&HardpolyRenderer::SetAddClampShadedBlend, // AddShaded
		&HardpolyRenderer::SetShadedBlend,         // Stencil
		&HardpolyRenderer::SetAddClampShadedBlend, // AddStencil
		&HardpolyRenderer::SetOpaqueBlend,         // FillOpaque
		&HardpolyRenderer::SetAddClampBlend,       // FillAdd
		&HardpolyRenderer::SetSubClampBlend,       // FillSub
		&HardpolyRenderer::SetRevSubClampBlend,    // FillRevSub
		&HardpolyRenderer::SetAddSrcColorBlend,    // FillAddSrcColor
		&HardpolyRenderer::SetOpaqueBlend,         // Skycap
		&HardpolyRenderer::SetShadedBlend,         // Fuzz
		&HardpolyRenderer::SetOpaqueBlend          // FogBoundary
	};
	return blendsetters[(int)triblend];
}

void HardpolyRenderer::SetOpaqueBlend(int srcalpha, int destalpha)
{
	glDisable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ZERO);
}

void HardpolyRenderer::SetMaskedBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void HardpolyRenderer::SetAlphaBlendFunc(int srcalpha, int destalpha)
{
	int srcblend;
	if (srcalpha == 0.0f)
		srcblend = GL_ZERO;
	else if (srcalpha == 1.0f)
		srcblend = GL_ONE;
	else
		srcblend = GL_CONSTANT_ALPHA;

	int destblend;
	if (destalpha == 0.0f)
		destblend = GL_ZERO;
	else if (destalpha == 1.0f)
		destblend = GL_ONE;
	else if (srcalpha + destalpha >= 255)
		destblend = GL_ONE_MINUS_CONSTANT_ALPHA;
	else
		destblend = GL_CONSTANT_COLOR;

	glBlendColor(destalpha / 256.0f, destalpha / 256.0f, destalpha / 256.0f, srcalpha / 256.0f);
	glBlendFunc(srcblend, destblend);
}

void HardpolyRenderer::SetAddClampBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	SetAlphaBlendFunc(srcalpha, destalpha);
}

void HardpolyRenderer::SetSubClampBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_SUBTRACT);
	SetAlphaBlendFunc(srcalpha, destalpha);
}

void HardpolyRenderer::SetRevSubClampBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	SetAlphaBlendFunc(srcalpha, destalpha);
}

void HardpolyRenderer::SetAddSrcColorBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
}

void HardpolyRenderer::SetShadedBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void HardpolyRenderer::SetAddClampShadedBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
}

/////////////////////////////////////////////////////////////////////////////

void DrawBatcher::DrawBatches(HardpolyRenderer *hardpoly)
{
	Flush();

	PolyDrawerWaitCycles.Clock();

	for (size_t i = mDrawStart; i < mNextBatch; i++)
	{
		DrawBatch *current = mCurrentFrameBatches[i].get();
		if (current->DrawRuns.empty() && !current->HasSortedDrawRuns)
			continue;

		if (!current->Vertices)
		{
			current->Vertices = std::make_shared<GPUVertexBuffer>(nullptr, MaxVertices * (int)sizeof(TriVertex));

			std::vector<GPUVertexAttributeDesc> attributes =
			{
				{ 0, 4, GPUVertexAttributeType::Float, false, sizeof(TriVertex), offsetof(TriVertex, x), current->Vertices },
				{ 1, 2, GPUVertexAttributeType::Float, false, sizeof(TriVertex), offsetof(TriVertex, u), current->Vertices }
			};

			current->VertexArray = std::make_shared<GPUVertexArray>(attributes);
			current->IndexBuffer = std::make_shared<GPUIndexBuffer>(nullptr, (int)(MaxIndices * sizeof(int32_t)));
			//current->FaceUniforms = std::make_shared<GPUUniformBuffer>(nullptr, (int)(DrawBatcher::MaxFaceUniforms * sizeof(FaceUniforms)));
			current->FaceUniformsTexture = std::make_shared<GPUTexture2D>(256, 128, false, 0, GPUPixelFormat::RGBA32f);
		}

		TriVertex *gpuVertices = (TriVertex*)current->Vertices->MapWriteOnly();
		memcpy(gpuVertices, current->CpuVertices.data(), sizeof(TriVertex) * current->CpuVertices.size());
		current->Vertices->Unmap();

		uint16_t *gpuIndices = (uint16_t*)current->IndexBuffer->MapWriteOnly();

		uint16_t *cpuIndices = current->CpuIndexBuffer.data();
		uint16_t currentPos = 0;
		for (const auto &sortIt : current->SortedDrawRuns)
		{
			const auto &runs = sortIt.second;
			if (runs.empty())
				continue;

			DrawRun gpuRun = runs.front();
			gpuRun.Start = currentPos;

			for (const auto &run : runs)
			{
				memcpy(gpuIndices + currentPos, cpuIndices + run.Start, sizeof(uint16_t) * run.NumVertices);
				currentPos += run.NumVertices;
			}

			gpuRun.NumVertices = currentPos - gpuRun.Start;
			current->DrawRuns.push_back(gpuRun);
		}

		current->IndexBuffer->Unmap();

		//FaceUniforms *gpuUniforms = (FaceUniforms*)current->FaceUniforms->MapWriteOnly();
		//memcpy(gpuUniforms, current->CpuFaceUniforms.data(), sizeof(FaceUniforms) * current->CpuFaceUniforms.size());
		//current->FaceUniforms->Unmap();
		int uploadHeight = (int)((current->CpuFaceUniforms.size() + 50) / 51);
		current->CpuFaceUniforms.resize(uploadHeight * 51);
		current->FaceUniformsTexture->Upload(0, 0, 255, uploadHeight, 0, current->CpuFaceUniforms.data());

		hardpoly->RenderBatch(current);
		PolyTotalBatches++;
	}

	mDrawStart = mNextBatch;

	PolyDrawerWaitCycles.Unclock();
}

void DrawBatcher::GetVertices(int numVertices)
{
	if (mNextVertex + numVertices > MaxVertices || (mCurrentBatch && mCurrentBatch->CpuFaceUniforms.size() == MaxFaceUniforms))
	{
		Flush();
	}

	if (!mVertices)
	{
		if (mNextBatch == mCurrentFrameBatches.size())
		{
			auto newBatch = std::unique_ptr<DrawBatch>(new DrawBatch());
			mCurrentFrameBatches.push_back(std::move(newBatch));
		}

		mCurrentBatch = mCurrentFrameBatches[mNextBatch++].get();
		mCurrentBatch->DrawRuns.clear();
		mCurrentBatch->SortedDrawRuns.clear();
		mCurrentBatch->HasSortedDrawRuns = false;
		mCurrentBatch->CpuVertices.resize(MaxVertices);
		mCurrentBatch->CpuFaceUniforms.clear();// resize(MaxFaceUniforms);
		mCurrentBatch->CpuIndexBuffer.clear();
		mCurrentBatch->CpuMatrices.clear();
		mVertices = mCurrentBatch->CpuVertices.data();
	}
}

void DrawBatcher::Flush()
{
	mVertices = nullptr;
	mNextVertex = 0;
	mCurrentBatch = nullptr;
}

void DrawBatcher::NextFrame()
{
	mCurrentFrameBatches.swap(mLastFrameBatches);
	mNextBatch = 0;
	mDrawStart = 0;
}

MatrixUniforms DrawBatcher::GetMatrixUniforms()
{
	MatrixUniforms uniforms;
	uniforms.WorldToView = WorldToView;
	uniforms.ViewToProjection = ViewToClip;
	uniforms.GlobVis = R_GetGlobVis(PolyRenderer::Instance()->Viewwindow, r_visibility);
	return uniforms;
}
