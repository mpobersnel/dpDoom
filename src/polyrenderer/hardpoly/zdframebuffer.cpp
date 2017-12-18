/*
** 2D GPU drawing code
**
**---------------------------------------------------------------------------
** Copyright 1998-2011 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stdlib.h>
#include "zdframebuffer.h"
#include "r_videoscale.h"

#ifdef WIN32
extern cycle_t BlitCycles;
#endif

CVAR(Bool, vid_hwaalines, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Bool, vid_hw2d, true, CVAR_NOINITCALL)
{
	V_SetBorderNeedRefresh();
}

CVAR(Int, r_showpacks, 0, 0)

const std::vector<const char *> ZDFrameBuffer::ShaderDefines[ZDFrameBuffer::NUM_SHADERS] =
{
	{ "ENORMALCOLOR" }, // NormalColor
	{ "ENORMALCOLOR", "PALTEX" }, // NormalColorPal
	{ "ENORMALCOLOR", "INVERT" }, // NormalColorInv
	{ "ENORMALCOLOR", "PALTEX", "INVERT" }, // NormalColorPalInv

	{ "EREDTOALPHA" }, // RedToAlpha
	{ "EREDTOALPHA" , "INVERT" }, // RedToAlphaInv

	{ "EVERTEXCOLOR" }, // VertexColor

	{ "ESPECIALCOLORMAP" }, // SpecialColormap
	{ "ESPECIALCOLORMAP", "PALTEX" }, // SpecialColorMapPal

	{ "EINGAMECOLORMAP" }, // InGameColormap
	{ "EINGAMECOLORMAP", "DESAT" }, // InGameColormapDesat
	{ "EINGAMECOLORMAP", "INVERT" }, // InGameColormapInv
	{ "EINGAMECOLORMAP", "INVERT", "DESAT" }, // InGameColormapInvDesat
	{ "EINGAMECOLORMAP", "PALTEX" }, // InGameColormapPal
	{ "EINGAMECOLORMAP", "PALTEX", "DESAT" }, // InGameColormapPalDesat
	{ "EINGAMECOLORMAP", "PALTEX", "INVERT" }, // InGameColormapPalInv
	{ "EINGAMECOLORMAP", "PALTEX", "INVERT", "DESAT" }, // InGameColormapPalInvDesat

	{ "EBURNWIPE" }, // BurnWipe
	{ "EGAMMACORRECTION" } // GammaCorrection
};

ZDFrameBuffer::ZDFrameBuffer(int width, int height, bool bgra) : DFrameBuffer(width, height, bgra)
{
}

ZDFrameBuffer::~ZDFrameBuffer()
{
	ReleaseResources();
}

void ZDFrameBuffer::Init()
{
	Accel2D = true;

	BlendingRect.left = 0;
	BlendingRect.top = 0;
	BlendingRect.right = GetWidth();
	BlendingRect.bottom = GetHeight();

	memcpy(SourcePalette, GPalette.BaseColors, sizeof(PalEntry) * 256);

	CreateResources();
	SetInitialState();
}

void ZDFrameBuffer::GetLetterboxFrame(int &letterboxX, int &letterboxY, int &letterboxWidth, int &letterboxHeight)
{
	int clientWidth = GetClientWidth();
	int clientHeight = GetClientHeight();

	float scaleX, scaleY;
	if (ViewportIsScaled43())
	{
		scaleX = MIN(clientWidth / (float)Width, clientHeight / (Height * 1.2f));
		scaleY = scaleX * 1.2f;
	}
	else
	{
		scaleX = MIN(clientWidth / (float)Width, clientHeight / (float)Height);
		scaleY = scaleX;
	}

	letterboxWidth = (int)round(Width * scaleX);
	letterboxHeight = (int)round(Height * scaleY);
	letterboxX = (clientWidth - letterboxWidth) / 2;
	letterboxY = (clientHeight - letterboxHeight) / 2;
}

void ZDFrameBuffer::GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma)
{
	Super::GetScreenshotBuffer(buffer, pitch, color_type, gamma);
	/*
	LockedRect lrect;

	if (!Accel2D)
	{
	Super::GetScreenshotBuffer(buffer, pitch, color_type, gamma);
	return;
	}
	buffer = nullptr;
	if ((ScreenshotTexture = GetCurrentScreen()) != nullptr)
	{
	if (!ScreenshotTexture->GetSurfaceLevel(0, &ScreenshotSurface))
	{
	delete ScreenshotTexture;
	ScreenshotTexture = nullptr;
	}
	else if (!ScreenshotSurface->LockRect(&lrect, nullptr, false))
	{
	delete ScreenshotSurface;
	ScreenshotSurface = nullptr;
	delete ScreenshotTexture;
	ScreenshotTexture = nullptr;
	}
	else
	{
	buffer = (const uint8_t *)lrect.pBits;
	pitch = lrect.Pitch;
	color_type = SS_BGRA;
	gamma = Gamma;
	}
	}
	*/
}

void ZDFrameBuffer::ReleaseScreenshotBuffer()
{
	Super::ReleaseScreenshotBuffer();
	//ScreenshotTexture.reset();
}

bool ZDFrameBuffer::Begin2D(bool copy3d)
{
	Super::Begin2D(copy3d);
	if (!Accel2D)
	{
		return false;
	}
	if (In2D)
	{
		return true;
	}
	In2D = 2 - copy3d;
	Update();
	In2D = 3;

	return true;
}

bool ZDFrameBuffer::Lock(bool buffered)
{
	if (m_Lock++ > 0)
	{
		return false;
	}
	assert(!In2D);
	Accel2D = vid_hw2d;
	if (UseMappedMemBuffer)
	{
		if (!MappedMemBuffer)
		{
			BindFBBuffer();

			MappedMemBuffer = FBTexture->Buffers[FBTexture->CurrentBuffer]->Map();
			Pitch = Width;
			if (MappedMemBuffer == nullptr)
				return true;
		}
		Buffer = (uint8_t*)MappedMemBuffer;
	}
	else
	{
		Buffer = MemBuffer;
	}
	return false;
}

void ZDFrameBuffer::Unlock()
{
	if (m_Lock == 0)
	{
		return;
	}

	if (UpdatePending && m_Lock == 1)
	{
		Update();
	}
	else if (--m_Lock == 0)
	{
		Buffer = nullptr;

		if (MappedMemBuffer)
		{
			BindFBBuffer();
			FBTexture->Buffers[FBTexture->CurrentBuffer]->Unmap();
			MappedMemBuffer = nullptr;
		}
	}
}

void ZDFrameBuffer::Update()
{
	// When In2D == 0: Copy buffer to screen and present
	// When In2D == 1: Copy buffer to screen but do not present
	// When In2D == 2: Set up for 2D drawing but do not draw anything
	// When In2D == 3: Present and set In2D to 0

	if (In2D == 3)
	{
		if (InScene)
		{
			DrawRateStuff();
			DrawPackedTextures(r_showpacks);
			EndBatch();		// Make sure all batched primitives are drawn.
			Flip();
		}
		In2D = 0;
		return;
	}

	if (m_Lock != 1)
	{
		I_FatalError("Framebuffer must have exactly 1 lock to be updated");
		if (m_Lock > 0)
		{
			UpdatePending = true;
			--m_Lock;
		}
		return;
	}

	if (In2D == 0)
	{
		DrawRateStuff();
	}

	if (NeedGammaUpdate)
	{
		float igamma;

		NeedGammaUpdate = false;
		igamma = 1 / Gamma;
		if (IsFullscreen())
		{
			GammaRamp ramp;

			for (int i = 0; i < 256; ++i)
			{
				ramp.blue[i] = ramp.green[i] = ramp.red[i] = uint16_t(65535.f * powf(i / 255.f, igamma));
			}
			SetGammaRamp(&ramp);
		}
		ShaderConstants.Gamma = { igamma, igamma, igamma, 0.5f /* For SM14 version */ };
		ShaderConstantsModified = true;
	}

	if (NeedPalUpdate)
	{
		UploadPalette();
		NeedPalUpdate = false;
	}

#ifdef WIN32
	BlitCycles.Reset();
	BlitCycles.Clock();
#endif

	m_Lock = 0;
	Draw3DPart(In2D <= 1);
	if (In2D == 0)
	{
		Flip();
	}

#ifdef WIN32
	BlitCycles.Unclock();
	//LOG1 ("cycles = %d\n", BlitCycles);
#endif

	Buffer = nullptr;
	UpdatePending = false;
}

void ZDFrameBuffer::Flip()
{
	assert(InScene);

	Present();
	InScene = false;

	if (!IsFullscreen())
	{
		int clientWidth = ViewportScaledWidth(GetClientWidth(), GetClientHeight());
		int clientHeight = ViewportScaledHeight(GetClientWidth(), GetClientHeight());
		if (clientWidth > 0 && clientHeight > 0 && (Width != clientWidth || Height != clientHeight))
		{
			Resize(clientWidth, clientHeight);

			Reset();

			V_OutputResized(Width, Height);
		}
	}
}

void ZDFrameBuffer::BindFBBuffer()
{
	if (!FBTexture->Buffers[0])
	{
		FBTexture->Buffers[0] = GetContext()->CreateStagingTexture(Width, Height, IsBgra() ? GPUPixelFormat::BGRA8 : GPUPixelFormat::R8);
		FBTexture->Buffers[1] = GetContext()->CreateStagingTexture(Width, Height, IsBgra() ? GPUPixelFormat::BGRA8 : GPUPixelFormat::R8);
	}
}

void ZDFrameBuffer::Draw3DPart(bool copy3d)
{
	if (copy3d /*&& ViewFBHandle == 0*/)
	{
		BindFBBuffer();

		if (!UseMappedMemBuffer)
		{
			int pixelsize = IsBgra() ? 4 : 1;

			uint8_t *dest = (uint8_t*)FBTexture->Buffers[FBTexture->CurrentBuffer]->Map();
			if (dest)
			{
				if (Pitch == Width)
				{
					memcpy(dest, MemBuffer, Width * Height * pixelsize);
				}
				else
				{
					uint8_t *src = MemBuffer;
					for (int y = 0; y < Height; y++)
					{
						memcpy(dest, src, Width * pixelsize);
						dest += Width * pixelsize;
						src += Pitch * pixelsize;
					}
				}
				FBTexture->Buffers[FBTexture->CurrentBuffer]->Unmap();
			}
		}
		else if (MappedMemBuffer)
		{
			FBTexture->Buffers[FBTexture->CurrentBuffer]->Unmap();
			MappedMemBuffer = nullptr;
		}

		GetContext()->CopyTexture(FBTexture->Texture, FBTexture->Buffers[FBTexture->CurrentBuffer]);

		FBTexture->CurrentBuffer = (FBTexture->CurrentBuffer + 1) & 1;
	}
	InScene = true;

	GetContext()->SetLineSmooth(vid_hwaalines);

	/*if (ViewFBHandle != 0)
	{
		SetPaletteTexture(PaletteTexture.get(), 256, BorderColor);
		memset(Constant, 0, sizeof(Constant));
		SetAlphaBlend(0);
		EnableAlphaTest(false);
		
		if (copy3d)
		{
			GLint oldReadFramebufferBinding = 0;
			glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebufferBinding);
		
			glBindFramebuffer(GL_READ_FRAMEBUFFER, ViewFBHandle);
			glBlitFramebuffer(0, 0, Width, Height, 0, 0, Width, Height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFramebufferBinding);
		}
		
		if (IsBgra())
			SetPixelShader(Shaders[SHADER_NormalColor].get());
		else
			SetPixelShader(Shaders[SHADER_NormalColorPal].get());
		
		return;
	}*/

	SetTexture(0, FBTexture.get());
	SetPaletteTexture(PaletteTexture.get(), 256, BorderColor);
	memset(Constant, 0, sizeof(Constant));
	SetAlphaBlend(0);
	EnableAlphaTest(false);
	if (IsBgra())
		SetPixelShader(Shaders[SHADER_NormalColor].get());
	else
		SetPixelShader(Shaders[SHADER_NormalColorPal].get());
	if (copy3d)
	{
		FBVERTEX verts[4];
		uint32_t color0, color1;
		if (Accel2D)
		{
			auto map = swrenderer::CameraLight::Instance()->ShaderColormap();
			if (map == nullptr)
			{
				color0 = 0;
				color1 = 0xFFFFFFF;
			}
			else
			{
				color0 = ColorValue(map->ColorizeStart[0] / 2, map->ColorizeStart[1] / 2, map->ColorizeStart[2] / 2, 0);
				color1 = ColorValue(map->ColorizeEnd[0] / 2, map->ColorizeEnd[1] / 2, map->ColorizeEnd[2] / 2, 1);
				if (IsBgra())
					SetPixelShader(Shaders[SHADER_SpecialColormap].get());
				else
					SetPixelShader(Shaders[SHADER_SpecialColormapPal].get());
			}
		}
		else
		{
			color0 = FlashColor0;
			color1 = FlashColor1;
		}
		CalcFullscreenCoords(verts, Accel2D, color0, color1);
		DrawTriangleFans(2, verts);
	}
	if (IsBgra())
		SetPixelShader(Shaders[SHADER_NormalColor].get());
	else
		SetPixelShader(Shaders[SHADER_NormalColorPal].get());
}

// Draws the black bars at the top and bottom of the screen for letterboxed modes.
void ZDFrameBuffer::DrawLetterbox(int x, int y, int width, int height)
{
	int clientWidth = GetClientWidth();
	int clientHeight = GetClientHeight();
	if (clientWidth == 0 || clientHeight == 0)
		return;

	if (y > 0)
	{
		GetContext()->SetScissor(0, 0, clientWidth, y);
		GetContext()->ClearScissorBox(0.0f, 0.0f, 0.0f, 1.0f);
	}
	if (clientHeight - y - height > 0)
	{
		GetContext()->SetScissor(0, y + height, clientWidth, clientHeight - y - height);
		GetContext()->ClearScissorBox(0.0f, 0.0f, 0.0f, 1.0f);
	}
	if (x > 0)
	{
		GetContext()->SetScissor(0, y, x, height);
		GetContext()->ClearScissorBox(0.0f, 0.0f, 0.0f, 1.0f);
	}
	if (clientWidth - x - width > 0)
	{
		GetContext()->SetScissor(x + width, y, clientWidth - x - width, height);
		GetContext()->ClearScissorBox(0.0f, 0.0f, 0.0f, 1.0f);
	}
	GetContext()->ResetScissor();
}

void ZDFrameBuffer::DrawBlendingRect()
{
	if (!In2D || !Accel2D)
	{
		return;
	}
	Dim(FlashColor, FlashAmount / 256.f, viewwindowx, viewwindowy, viewwidth, viewheight);
}

void ZDFrameBuffer::DoClear(int left, int top, int right, int bottom, int palcolor, uint32_t color)
{
	if (In2D < 2)
	{
		Super::DoClear(left, top, right, bottom, palcolor, color);
		return;
	}
	if (!InScene)
	{
		return;
	}

	if (palcolor >= 0 && color == 0)
	{
		color = GPalette.BaseColors[palcolor];
	}
	else if (APART(color) < 255)
	{
		Dim(color, APART(color) / 255.f, left, top, right - left, bottom - top);
		return;
	}

	AddColorOnlyQuad(left, top, right - left, bottom - top, color | 0xFF000000);
}

void ZDFrameBuffer::DoDim(PalEntry color, float amount, int x1, int y1, int w, int h)
{
	if (amount <= 0)
	{
		return;
	}
	if (In2D < 2)
	{
		Super::DoDim(color, amount, x1, y1, w, h);
		return;
	}
	if (!InScene)
	{
		return;
	}

	if (amount > 1)
	{
		amount = 1;
	}
	AddColorOnlyQuad(x1, y1, w, h, color | (int(amount * 255) << 24));
}

void ZDFrameBuffer::DrawLine(int x0, int y0, int x1, int y1, int palcolor, uint32_t color)
{
	if (In2D < 2)
	{
		Super::DrawLine(x0, y0, x1, y1, palcolor, color);
		return;
	}
	if (!InScene)
	{
		return;
	}

	if (BatchType != BATCH_Lines)
	{
		BeginLineBatch();
	}
	if (VertexPos == NUM_VERTS)
	{ // Flush the buffer and refill it.
		EndLineBatch();
		BeginLineBatch();
	}
	// Add the endpoints to the vertex buffer.
	VertexData[VertexPos].x = float(x0);
	VertexData[VertexPos].y = float(y0);
	VertexData[VertexPos].z = 0;
	VertexData[VertexPos].rhw = 1;
	VertexData[VertexPos].color0 = color;
	VertexData[VertexPos].color1 = 0;
	VertexData[VertexPos].tu = 0;
	VertexData[VertexPos].tv = 0;

	VertexData[VertexPos + 1].x = float(x1);
	VertexData[VertexPos + 1].y = float(y1);
	VertexData[VertexPos + 1].z = 0;
	VertexData[VertexPos + 1].rhw = 1;
	VertexData[VertexPos + 1].color0 = color;
	VertexData[VertexPos + 1].color1 = 0;
	VertexData[VertexPos + 1].tu = 0;
	VertexData[VertexPos + 1].tv = 0;

	VertexPos += 2;
}

void ZDFrameBuffer::DrawPixel(int x, int y, int palcolor, uint32_t color)
{
	if (In2D < 2)
	{
		Super::DrawPixel(x, y, palcolor, color);
		return;
	}
	if (!InScene)
	{
		return;
	}

	FBVERTEX pt =
	{
		float(x), float(y), 0, 1, color
	};
	EndBatch();		// Draw out any batched operations.
	SetPixelShader(Shaders[SHADER_VertexColor].get());
	SetAlphaBlend(GL_FUNC_ADD, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	DrawPoints(1, &pt);
}

void ZDFrameBuffer::DrawTextureParms(FTexture *img, DrawParms &parms)
{
	if (In2D < 2)
	{
		Super::DrawTextureParms(img, parms);
		return;
	}
	if (!InScene)
	{
		return;
	}

	OpenGLTex *tex = static_cast<OpenGLTex *>(img->GetNative(false));

	if (tex == nullptr)
	{
		assert(tex != nullptr);
		return;
	}

	CheckQuadBatch();

	double xscale = parms.destwidth / parms.texwidth;
	double yscale = parms.destheight / parms.texheight;
	double x0 = parms.x - parms.left * xscale;
	double y0 = parms.y - parms.top * yscale;
	double x1 = x0 + parms.destwidth;
	double y1 = y0 + parms.destheight;
	float u0 = tex->Box->Left;
	float v0 = tex->Box->Top;
	float u1 = tex->Box->Right;
	float v1 = tex->Box->Bottom;
	double uscale = 1.f / tex->Box->Owner->Width;
	bool scissoring = false;
	FBVERTEX *vert;

	if (parms.flipX)
	{
		swapvalues(u0, u1);
	}
	if (parms.windowleft > 0 || parms.windowright < parms.texwidth)
	{
		double wi = MIN(parms.windowright, parms.texwidth);
		x0 += parms.windowleft * xscale;
		u0 = float(u0 + parms.windowleft * uscale);
		x1 -= (parms.texwidth - wi) * xscale;
		u1 = float(u1 - (parms.texwidth - wi) * uscale);
	}

#if 0
	float vscale = 1.f / tex->Box->Owner->Height / yscale;
	if (y0 < parms.uclip)
	{
		v0 += (float(parms.uclip) - y0) * vscale;
		y0 = float(parms.uclip);
	}
	if (y1 > parms.dclip)
	{
		v1 -= (y1 - float(parms.dclip)) * vscale;
		y1 = float(parms.dclip);
	}
	if (x0 < parms.lclip)
	{
		u0 += float(parms.lclip - x0) * uscale / xscale * 2;
		x0 = float(parms.lclip);
	}
	if (x1 > parms.rclip)
	{
		u1 -= (x1 - parms.rclip) * uscale / xscale * 2;
		x1 = float(parms.rclip);
	}
#else
	// Use a scissor test because the math above introduces some jitter
	// that is noticeable at low resolutions. Unfortunately, this means this
	// quad has to be in a batch by itself.
	if (y0 < parms.uclip || y1 > parms.dclip || x0 < parms.lclip || x1 > parms.rclip)
	{
		scissoring = true;
		if (QuadBatchPos > 0)
		{
			EndQuadBatch();
			BeginQuadBatch();
		}
		GetContext()->SetScissor(parms.lclip, parms.uclip, parms.rclip - parms.lclip, parms.dclip - parms.uclip);
	}
#endif
	parms.bilinear = false;

	uint32_t color0, color1;
	BufferedTris *quad = &QuadExtra[QuadBatchPos];

	if (!SetStyle(tex, parms, color0, color1, *quad))
	{
		goto done;
	}

	quad->Texture = tex->Box->Owner->Tex.get();
	if (parms.bilinear)
	{
		quad->Flags |= BQF_Bilinear;
	}
	quad->NumTris = 2;
	quad->NumVerts = 4;

	vert = &VertexData[VertexPos];

	{
		PalEntry color = color1;
		color = PalEntry((color.a * parms.color.a) / 255, (color.r * parms.color.r) / 255, (color.g * parms.color.g) / 255, (color.b * parms.color.b) / 255);
		color1 = color;
	}

	// Fill the vertex buffer.
	vert[0].x = float(x0);
	vert[0].y = float(y0);
	vert[0].z = 0;
	vert[0].rhw = 1;
	vert[0].color0 = color0;
	vert[0].color1 = color1;
	vert[0].tu = u0;
	vert[0].tv = v0;

	vert[1].x = float(x1);
	vert[1].y = float(y0);
	vert[1].z = 0;
	vert[1].rhw = 1;
	vert[1].color0 = color0;
	vert[1].color1 = color1;
	vert[1].tu = u1;
	vert[1].tv = v0;

	vert[2].x = float(x1);
	vert[2].y = float(y1);
	vert[2].z = 0;
	vert[2].rhw = 1;
	vert[2].color0 = color0;
	vert[2].color1 = color1;
	vert[2].tu = u1;
	vert[2].tv = v1;

	vert[3].x = float(x0);
	vert[3].y = float(y1);
	vert[3].z = 0;
	vert[3].rhw = 1;
	vert[3].color0 = color0;
	vert[3].color1 = color1;
	vert[3].tu = u0;
	vert[3].tv = v1;

	// Fill the vertex index buffer.
	IndexData[IndexPos] = VertexPos;
	IndexData[IndexPos + 1] = VertexPos + 1;
	IndexData[IndexPos + 2] = VertexPos + 2;
	IndexData[IndexPos + 3] = VertexPos;
	IndexData[IndexPos + 4] = VertexPos + 2;
	IndexData[IndexPos + 5] = VertexPos + 3;

	// Batch the quad.
	QuadBatchPos++;
	VertexPos += 4;
	IndexPos += 6;
done:
	if (scissoring)
	{
		EndQuadBatch();
		GetContext()->ResetScissor();
	}
}

void ZDFrameBuffer::FlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{
	if (In2D < 2)
	{
		Super::FlatFill(left, top, right, bottom, src, local_origin);
		return;
	}
	if (!InScene)
	{
		return;
	}

	OpenGLTex *tex = static_cast<OpenGLTex *>(src->GetNative(true));
	if (tex == nullptr)
	{
		return;
	}
	float x0 = float(left);
	float y0 = float(top);
	float x1 = float(right);
	float y1 = float(bottom);
	float itw = 1.f / float(src->GetWidth());
	float ith = 1.f / float(src->GetHeight());
	float xo = local_origin ? x0 : 0;
	float yo = local_origin ? y0 : 0;
	float u0 = (x0 - xo) * itw;
	float v0 = (y0 - yo) * ith;
	float u1 = (x1 - xo) * itw;
	float v1 = (y1 - yo) * ith;

	CheckQuadBatch();

	BufferedTris *quad = &QuadExtra[QuadBatchPos];
	FBVERTEX *vert = &VertexData[VertexPos];

	quad->ClearSetup();
	if (tex->GetTexFormat() == GPUPixelFormat::R8 && !tex->IsGray)
	{
		quad->Flags = BQF_WrapUV | BQF_GamePalette; // | BQF_DisableAlphaTest;
		quad->ShaderNum = BQS_PalTex;
	}
	else
	{
		quad->Flags = BQF_WrapUV; // | BQF_DisableAlphaTest;
		quad->ShaderNum = BQS_Plain;
	}
	quad->Palette = nullptr;
	quad->Texture = tex->Box->Owner->Tex.get();
	quad->NumVerts = 4;
	quad->NumTris = 2;

	vert[0].x = x0;
	vert[0].y = y0;
	vert[0].z = 0;
	vert[0].rhw = 1;
	vert[0].color0 = 0;
	vert[0].color1 = 0xFFFFFFFF;
	vert[0].tu = u0;
	vert[0].tv = v0;

	vert[1].x = x1;
	vert[1].y = y0;
	vert[1].z = 0;
	vert[1].rhw = 1;
	vert[1].color0 = 0;
	vert[1].color1 = 0xFFFFFFFF;
	vert[1].tu = u1;
	vert[1].tv = v0;

	vert[2].x = x1;
	vert[2].y = y1;
	vert[2].z = 0;
	vert[2].rhw = 1;
	vert[2].color0 = 0;
	vert[2].color1 = 0xFFFFFFFF;
	vert[2].tu = u1;
	vert[2].tv = v1;

	vert[3].x = x0;
	vert[3].y = y1;
	vert[3].z = 0;
	vert[3].rhw = 1;
	vert[3].color0 = 0;
	vert[3].color1 = 0xFFFFFFFF;
	vert[3].tu = u0;
	vert[3].tv = v1;

	IndexData[IndexPos] = VertexPos;
	IndexData[IndexPos + 1] = VertexPos + 1;
	IndexData[IndexPos + 2] = VertexPos + 2;
	IndexData[IndexPos + 3] = VertexPos;
	IndexData[IndexPos + 4] = VertexPos + 2;
	IndexData[IndexPos + 5] = VertexPos + 3;

	QuadBatchPos++;
	VertexPos += 4;
	IndexPos += 6;
}

// Here, "simple" means that a simple triangle fan can draw it.
void ZDFrameBuffer::FillSimplePoly(FTexture *texture, FVector2 *points, int npoints,
	double originx, double originy, double scalex, double scaley,
	DAngle rotation, const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip)
{
	if (npoints < 3)
	{ // This is no polygon.
		return;
	}
	if (In2D < 2)
	{
		Super::FillSimplePoly(texture, points, npoints, originx, originy, scalex, scaley, rotation, colormap, lightlevel, flatcolor, bottomclip);
		return;
	}
	if (!InScene)
	{
		return;
	}

	// Use an equation similar to player sprites to determine shade
	double fadelevel = clamp((swrenderer::LightVisibility::LightLevelToShade(lightlevel, true) / 65536. - 12) / NUMCOLORMAPS, 0.0, 1.0);

	BufferedTris *quad;
	FBVERTEX *verts;
	OpenGLTex *tex;
	float uscale, vscale;
	int i, ipos;
	uint32_t color0, color1;
	float ox, oy;
	float cosrot, sinrot;
	bool dorotate = rotation != 0;

	if (npoints < 3)
	{ // This is no polygon.
		return;
	}
	tex = static_cast<OpenGLTex *>(texture->GetNative(true));
	if (tex == nullptr)
	{
		return;
	}

	cosrot = (float)cos(rotation.Radians());
	sinrot = (float)sin(rotation.Radians());

	CheckQuadBatch(npoints - 2, npoints);
	quad = &QuadExtra[QuadBatchPos];
	verts = &VertexData[VertexPos];

	color0 = 0;
	color1 = 0xFFFFFFFF;

	quad->ClearSetup();
	if (tex->GetTexFormat() == GPUPixelFormat::R8 && !tex->IsGray)
	{
		quad->Flags = BQF_WrapUV | BQF_GamePalette | BQF_DisableAlphaTest;
		quad->ShaderNum = BQS_PalTex;
		if (colormap.Desaturation != 0)
		{
			quad->Flags |= BQF_Desaturated;
		}
		quad->ShaderNum = BQS_InGameColormap;
		quad->Desat = colormap.Desaturation;
		color0 = ColorARGB(255, colormap.LightColor.r, colormap.LightColor.g, colormap.LightColor.b);
		color1 = ColorARGB(uint32_t((1 - fadelevel) * 255),
			uint32_t(colormap.FadeColor.r * fadelevel),
			uint32_t(colormap.FadeColor.g * fadelevel),
			uint32_t(colormap.FadeColor.b * fadelevel));
	}
	else
	{
		quad->Flags = BQF_WrapUV | BQF_DisableAlphaTest;
		quad->ShaderNum = BQS_Plain;
	}
	quad->Palette = nullptr;
	quad->Texture = tex->Box->Owner->Tex.get();
	quad->NumVerts = npoints;
	quad->NumTris = npoints - 2;

	uscale = float(1.f / (texture->GetScaledWidth() * scalex));
	vscale = float(1.f / (texture->GetScaledHeight() * scaley));
	ox = float(originx);
	oy = float(originy);

	for (i = 0; i < npoints; ++i)
	{
		verts[i].x = points[i].X;
		verts[i].y = points[i].Y;
		verts[i].z = 0;
		verts[i].rhw = 1;
		verts[i].color0 = color0;
		verts[i].color1 = color1;
		float u = points[i].X - ox;
		float v = points[i].Y - oy;
		if (dorotate)
		{
			float t = u;
			u = t * cosrot - v * sinrot;
			v = v * cosrot + t * sinrot;
		}
		verts[i].tu = u * uscale;
		verts[i].tv = v * vscale;
	}
	for (ipos = IndexPos, i = 2; i < npoints; ++i, ipos += 3)
	{
		IndexData[ipos] = VertexPos;
		IndexData[ipos + 1] = VertexPos + i - 1;
		IndexData[ipos + 2] = VertexPos + i;
	}

	QuadBatchPos++;
	VertexPos += npoints;
	IndexPos = ipos;
}

void ZDFrameBuffer::ScaleCoordsFromWindow(int16_t &x, int16_t &y)
{
	int letterboxX, letterboxY, letterboxWidth, letterboxHeight;
	GetLetterboxFrame(letterboxX, letterboxY, letterboxWidth, letterboxHeight);

	// Subtract the LB video mode letterboxing
	if (IsFullscreen())
		y -= (GetTrueHeight() - VideoHeight) / 2;

	x = int16_t((x - letterboxX) * Width / letterboxWidth);
	y = int16_t((y - letterboxY) * Height / letterboxHeight);
}

PalEntry *ZDFrameBuffer::GetPalette()
{
	return SourcePalette;
}

void ZDFrameBuffer::GetFlashedPalette(PalEntry pal[256])
{
	memcpy(pal, GetPalette(), 256 * sizeof(PalEntry));
	if (FlashAmount)
	{
		DoBlending(pal, pal, 256, FlashColor.r, FlashColor.g, FlashColor.b, FlashAmount);
	}
}

void ZDFrameBuffer::UpdatePalette()
{
	NeedPalUpdate = true;
}

bool ZDFrameBuffer::SetGamma(float gamma)
{
	Gamma = gamma;
	NeedGammaUpdate = true;
	return true;
}

bool ZDFrameBuffer::SetFlash(PalEntry rgb, int amount)
{
	FlashColor = rgb;
	FlashAmount = amount;

	// Fill in the constants for the pixel shader to do linear interpolation between the palette and the flash:
	float r = rgb.r / 255.f, g = rgb.g / 255.f, b = rgb.b / 255.f, a = amount / 256.f;
	FlashColor0 = ColorValue(r * a, g * a, b * a, 0);
	a = 1 - a;
	FlashColor1 = ColorValue(a, a, a, 1);
	return true;
}

void ZDFrameBuffer::GetFlash(PalEntry &rgb, int &amount)
{
	rgb = FlashColor;
	amount = FlashAmount;
}

std::unique_ptr<ZDFrameBuffer::HWFrameBuffer> ZDFrameBuffer::CreateFrameBuffer(const FString &name, int width, int height)
{
	std::unique_ptr<HWFrameBuffer> fb(new HWFrameBuffer());
	fb->Texture = CreateTexture(name, width, height, 1, GPUPixelFormat::RGBA16f);
	fb->Framebuffer = GetContext()->CreateFrameBuffer({ fb->Texture->Texture }, nullptr);
	return fb;
}

std::unique_ptr<ZDFrameBuffer::HWPixelShader> ZDFrameBuffer::CreatePixelShader(FString vertexsrc, FString fragmentsrc, const std::vector<const char *> &defines)
{
	std::unique_ptr<HWPixelShader> shader(new HWPixelShader());

	shader->Program = GetContext()->CreateProgram();
	for (const auto &define : defines)
		shader->Program->SetDefine(define);
	shader->Program->Compile(GPUShaderType::Vertex, "swshader.vp", vertexsrc.GetChars());
	shader->Program->Compile(GPUShaderType::Fragment, "swshader.fp", fragmentsrc.GetChars());
	shader->Program->SetAttribLocation("AttrPosition", 0);
	shader->Program->SetAttribLocation("AttrColor0", 1);
	shader->Program->SetAttribLocation("AttrColor1", 2);
	shader->Program->SetAttribLocation("AttrTexCoord0", 3);
	shader->Program->SetFragOutput("FragColor", 0);
	shader->Program->Link("swshader");

	shader->ImageLocation = shader->Program->GetUniformLocation("Image");
	shader->PaletteLocation = shader->Program->GetUniformLocation("Palette");
	shader->NewScreenLocation = shader->Program->GetUniformLocation("NewScreen");
	shader->BurnLocation = shader->Program->GetUniformLocation("Burn");

	return shader;
}

std::unique_ptr<ZDFrameBuffer::HWVertexBuffer> ZDFrameBuffer::CreateVertexBuffer(int size)
{
	std::unique_ptr<HWVertexBuffer> obj(new HWVertexBuffer());
	obj->Size = size;
	obj->Buffer = GetContext()->CreateVertexBuffer(nullptr, size);
	obj->VertexArray = GetContext()->CreateVertexArray(
	{
		{ 0, 4, GPUVertexAttributeType::Float, false, sizeof(FBVERTEX), offsetof(FBVERTEX, x), obj->Buffer },
		{ 1, 4, GPUVertexAttributeType::Uint8, true, sizeof(FBVERTEX), offsetof(FBVERTEX, color0), obj->Buffer },
		{ 2, 4, GPUVertexAttributeType::Uint8, true, sizeof(FBVERTEX), offsetof(FBVERTEX, color1), obj->Buffer },
		{ 3, 2, GPUVertexAttributeType::Float, false, sizeof(FBVERTEX), offsetof(FBVERTEX, tu), obj->Buffer },
	});
	return obj;
}

std::unique_ptr<ZDFrameBuffer::HWIndexBuffer> ZDFrameBuffer::CreateIndexBuffer(int size)
{
	std::unique_ptr<HWIndexBuffer> obj(new HWIndexBuffer());
	obj->Size = size;
	obj->Buffer = GetContext()->CreateIndexBuffer(nullptr, size);
	return obj;
}

std::unique_ptr<ZDFrameBuffer::HWTexture> ZDFrameBuffer::CreateTexture(const FString &name, int width, int height, int levels, GPUPixelFormat format)
{
	std::unique_ptr<HWTexture> obj(new HWTexture());
	obj->Format = format;
	obj->Texture = GetContext()->CreateTexture2D(width, height, levels > 1, 1, format);
	return obj;
}

std::unique_ptr<ZDFrameBuffer::HWTexture> ZDFrameBuffer::CopyCurrentScreen()
{
	std::unique_ptr<HWTexture> obj(new HWTexture());
	obj->Format = GPUPixelFormat::RGBA16f;
	obj->Texture = GetContext()->CreateTexture2D(Width, Height, false, 1, obj->Format, nullptr);
	//GetContext()->CopyTexture(obj->Texture, FBTexture->Texture);
	//glCopyTexImage2D(GL_TEXTURE_2D, 0, obj->Format, 0, 0, Width, Height, 0);
	return obj;
}

void ZDFrameBuffer::SetHWPixelShader(HWPixelShader *shader)
{
	if (shader != CurrentShader)
	{
		if (shader)
		{
			GetContext()->SetProgram(shader->Program);
		}
		else
		{
			GetContext()->SetProgram(nullptr);
		}
	}
	CurrentShader = shader;

	if (ShaderConstantsModified)
	{
		if (!GpuShaderUniforms)
		{
			GpuShaderUniforms = GetContext()->CreateUniformBuffer(&ShaderConstants, sizeof(ShaderConstants));
		}
		else
		{
			GpuShaderUniforms->Upload(&ShaderConstants, sizeof(ShaderConstants));
		}
		ShaderConstantsModified = false;
	}
	GetContext()->SetUniforms(0, GpuShaderUniforms);
}

void ZDFrameBuffer::SetStreamSource(HWVertexBuffer *vertexBuffer)
{
	if (vertexBuffer)
		GetContext()->SetVertexArray(vertexBuffer->VertexArray);
	else
		GetContext()->SetVertexArray(nullptr);
}

void ZDFrameBuffer::SetIndices(HWIndexBuffer *indexBuffer)
{
	if (indexBuffer)
		GetContext()->SetIndexBuffer(indexBuffer->Buffer);
	else
		GetContext()->SetIndexBuffer(nullptr);
}

void ZDFrameBuffer::DrawTriangleFans(int count, const FBVERTEX *vertices)
{
	count = 2 + count;

	if (!StreamVertexBuffer)
	{
		StreamVertexBuffer.reset(new HWVertexBuffer());
		StreamVertexBuffer->Buffer = GetContext()->CreateVertexBuffer(vertices, count * sizeof(FBVERTEX));
		StreamVertexBuffer->VertexArray = GetContext()->CreateVertexArray(
		{
			{ 0, 4, GPUVertexAttributeType::Float, false, sizeof(FBVERTEX), offsetof(FBVERTEX, x), StreamVertexBuffer->Buffer },
			{ 1, 4, GPUVertexAttributeType::Uint8, true, sizeof(FBVERTEX), offsetof(FBVERTEX, color0), StreamVertexBuffer->Buffer },
			{ 2, 4, GPUVertexAttributeType::Uint8, true, sizeof(FBVERTEX), offsetof(FBVERTEX, color1), StreamVertexBuffer->Buffer },
			{ 3, 2, GPUVertexAttributeType::Float, false, sizeof(FBVERTEX), offsetof(FBVERTEX, tu), StreamVertexBuffer->Buffer }
		});
	}
	else
	{
		StreamVertexBuffer->Buffer->Upload(vertices, count * sizeof(FBVERTEX));
	}

	GetContext()->SetVertexArray(StreamVertexBuffer->VertexArray);
	GetContext()->Draw(GPUDrawMode::TriangleFan, 0, count);
	GetContext()->SetVertexArray(nullptr);
}

void ZDFrameBuffer::DrawTriangleFans(int count, const BURNVERTEX *vertices)
{
	count = 2 + count;

	if (!StreamVertexBufferBurn)
	{
		StreamVertexBufferBurn.reset(new HWVertexBuffer());
		StreamVertexBufferBurn->Buffer = GetContext()->CreateVertexBuffer(vertices, count * sizeof(BURNVERTEX));
		StreamVertexBufferBurn->VertexArray = GetContext()->CreateVertexArray(
		{
			{ 0, 4, GPUVertexAttributeType::Float, false, sizeof(BURNVERTEX), offsetof(BURNVERTEX, x), StreamVertexBufferBurn->Buffer },
			{ 1, 4, GPUVertexAttributeType::Float, false, sizeof(BURNVERTEX), offsetof(BURNVERTEX, tu0), StreamVertexBufferBurn->Buffer }
		});
	}
	else
	{
		StreamVertexBufferBurn->Buffer->Upload(vertices, count * sizeof(BURNVERTEX));
	}

	GetContext()->SetVertexArray(StreamVertexBufferBurn->VertexArray);
	GetContext()->Draw(GPUDrawMode::TriangleFan, 0, count);
	GetContext()->SetVertexArray(nullptr);
}

void ZDFrameBuffer::DrawPoints(int count, const FBVERTEX *vertices)
{
	if (!StreamVertexBuffer)
	{
		StreamVertexBuffer.reset(new HWVertexBuffer());
		StreamVertexBuffer->Buffer = GetContext()->CreateVertexBuffer(vertices, count * sizeof(FBVERTEX));
		StreamVertexBuffer->VertexArray = GetContext()->CreateVertexArray(
		{
			{ 0, 4, GPUVertexAttributeType::Float, false, sizeof(FBVERTEX), offsetof(FBVERTEX, x), StreamVertexBuffer->Buffer },
			{ 1, 4, GPUVertexAttributeType::Uint8, true, sizeof(FBVERTEX), offsetof(FBVERTEX, color0), StreamVertexBuffer->Buffer },
			{ 2, 4, GPUVertexAttributeType::Uint8, true, sizeof(FBVERTEX), offsetof(FBVERTEX, color1), StreamVertexBuffer->Buffer },
			{ 3, 2, GPUVertexAttributeType::Float, false, sizeof(FBVERTEX), offsetof(FBVERTEX, tu), StreamVertexBuffer->Buffer }
		});
	}
	else
	{
		StreamVertexBuffer->Buffer->Upload(vertices, count * sizeof(FBVERTEX));
	}

	GetContext()->SetVertexArray(StreamVertexBufferBurn->VertexArray);
	GetContext()->Draw(GPUDrawMode::Points, 0, count);
	GetContext()->SetVertexArray(nullptr);
}

void ZDFrameBuffer::DrawLineList(int count)
{
	GetContext()->Draw(GPUDrawMode::Lines, 0, count * 2);
}

void ZDFrameBuffer::DrawTriangleList(int minIndex, int numVertices, int startIndex, int primitiveCount)
{
	GetContext()->DrawIndexed(GPUDrawMode::Triangles, startIndex, primitiveCount * 3);
	//GetContext()->DrawRangeIndexed(GPUDrawMode::Triangles, minIndex, minIndex + numVertices - 1, startIndex, primitiveCount * 3);
}

void ZDFrameBuffer::Present()
{
	int clientWidth = GetClientWidth();
	int clientHeight = GetClientHeight();
	if (clientWidth > 0 && clientHeight > 0)
	{
		GetContext()->SetFrameBuffer(nullptr);
		GetContext()->SetViewport(0, 0, clientWidth, clientHeight);

		int letterboxX, letterboxY, letterboxWidth, letterboxHeight;
		GetLetterboxFrame(letterboxX, letterboxY, letterboxWidth, letterboxHeight);
		DrawLetterbox(letterboxX, letterboxY, letterboxWidth, letterboxHeight);
		GetContext()->SetViewport(letterboxX, letterboxY, letterboxWidth, letterboxHeight);

		FBVERTEX verts[4];
		CalcFullscreenCoords(verts, false, 0, 0xFFFFFFFF);
		SetTexture(0, OutputFB->Texture.get());

		if (ViewportLinearScale())
		{
			GetContext()->SetSampler(0, SamplerClampToEdgeLinear);
		}
		else
		{
			GetContext()->SetSampler(0, SamplerClampToEdge);
		}

		SetPixelShader(Shaders[SHADER_GammaCorrection].get());
		SetAlphaBlend(0);
		EnableAlphaTest(false);
		DrawTriangleFans(2, verts);

		if (ViewportLinearScale())
			GetContext()->SetSampler(0, SamplerClampToEdge);
	}

	SwapBuffers();
	//Debug->Update();

	ShaderConstants.ScreenSize = { (float)GetWidth(), (float)GetHeight(), 1.0f, 1.0f };
	ShaderConstantsModified = true;

	GetContext()->SetFrameBuffer(OutputFB->Framebuffer);
	GetContext()->SetViewport(0, 0, GetWidth(), GetHeight());
}

void ZDFrameBuffer::SetInitialState()
{
	CurPixelShader = nullptr;
	memset(Constant, 0, sizeof(Constant));

	SamplerRepeat = GetContext()->CreateSampler(GPUSampleMode::Nearest, GPUSampleMode::Nearest, GPUMipmapMode::None, GPUWrapMode::Repeat, GPUWrapMode::Repeat);
	SamplerClampToEdge = GetContext()->CreateSampler(GPUSampleMode::Nearest, GPUSampleMode::Nearest, GPUMipmapMode::None, GPUWrapMode::ClampToEdge, GPUWrapMode::ClampToEdge);
	SamplerClampToEdgeLinear = GetContext()->CreateSampler(GPUSampleMode::Linear, GPUSampleMode::Linear, GPUMipmapMode::None, GPUWrapMode::ClampToEdge, GPUWrapMode::ClampToEdge);

	for (unsigned i = 0; i < countof(Texture); ++i)
	{
		Texture[i] = nullptr;
		GetContext()->SetSampler(i, SamplerClampToEdge);
	}

	NeedGammaUpdate = true;
	NeedPalUpdate = true;

	ShaderConstants.Desaturation = { 0.0f, 0.0f, 0.0f, 0.0f };
	ShaderConstants.PaletteMod = { 0.0f, 0.0f, 0.0f, 0.0f };
	ShaderConstants.Weights = { 77 / 256.f, 143 / 256.f, 37 / 256.f, 1 }; // This constant is used for grayscaling weights (.xyz) and color inversion (.w)
	ShaderConstants.Gamma = { 0.0f, 0.0f, 0.0f, 0.0f };
	ShaderConstants.ScreenSize = { (float)GetWidth(), (float)GetHeight(), 1.0f, 1.0f };
	ShaderConstantsModified = true;

	CurBorderColor = 0;

	// Clear to black, just in case it wasn't done already.
	GetContext()->ClearColorBuffer(0, 0.0f, 0.0f, 0.0f, 1.0f);
}

bool ZDFrameBuffer::CreateResources()
{
	Atlases = nullptr;
	if (!LoadShaders())
		return false;

	OutputFB = CreateFrameBuffer("OutputFB", GetWidth(), GetHeight());
	if (!OutputFB)
		return false;

	GetContext()->SetFrameBuffer(OutputFB->Framebuffer);

	if (!CreateFBTexture() ||
		!CreatePaletteTexture())
	{
		return false;
	}
	if (!CreateVertexes())
	{
		return false;
	}
	return true;
}

bool ZDFrameBuffer::LoadShaders()
{
	int lumpvert, lumpfrag;

	if (IsOpenGL())
	{
		lumpvert = Wads.CheckNumForFullName("shaders/glsl/swshader.vp");
		lumpfrag = Wads.CheckNumForFullName("shaders/glsl/swshader.fp");
	}
	else
	{
		lumpvert = Wads.CheckNumForFullName("shaders/d3d/swshader.vp");
		lumpfrag = Wads.CheckNumForFullName("shaders/d3d/swshader.fp");
	}
	if (lumpvert < 0 || lumpfrag < 0)
		return false;

	FString vertsource = Wads.ReadLump(lumpvert).GetString();
	FString fragsource = Wads.ReadLump(lumpfrag).GetString();

	FString shaderdir, shaderpath;
	unsigned int i;

	for (i = 0; i < NUM_SHADERS; ++i)
	{
		shaderpath = shaderdir;
		Shaders[i] = CreatePixelShader(vertsource, fragsource, ShaderDefines[i]);
		if (!Shaders[i] && i < SHADER_BurnWipe)
		{
			break;
		}

		GetContext()->SetProgram(Shaders[i]->Program);
		if (Shaders[i]->ImageLocation != -1) GetContext()->SetUniform1i(Shaders[i]->ImageLocation, 0);
		if (Shaders[i]->PaletteLocation != -1) GetContext()->SetUniform1i(Shaders[i]->PaletteLocation, 1);
		if (Shaders[i]->NewScreenLocation != -1) GetContext()->SetUniform1i(Shaders[i]->NewScreenLocation, 0);
		if (Shaders[i]->BurnLocation != -1) GetContext()->SetUniform1i(Shaders[i]->BurnLocation, 1);
		GetContext()->SetProgram(nullptr);
	}
	if (i == NUM_SHADERS)
	{ // Success!
		return true;
	}
	// Failure. Release whatever managed to load (which is probably nothing.)
	for (i = 0; i < NUM_SHADERS; ++i)
	{
		Shaders[i].reset();
	}
	return false;
}

void ZDFrameBuffer::ReleaseResources()
{
#ifdef WIN32
	I_SaveWindowedPos();
#endif
	KillNativeTexs();
	KillNativePals();
	ReleaseDefaultPoolItems();
	ScreenshotTexture.reset();
	PaletteTexture.reset();
	for (int i = 0; i < NUM_SHADERS; ++i)
	{
		Shaders[i].reset();
	}
	if (ScreenWipe != nullptr)
	{
		delete ScreenWipe;
		ScreenWipe = nullptr;
	}
	Atlas *pack, *next;
	for (pack = Atlases; pack != nullptr; pack = next)
	{
		next = pack->Next;
		delete pack;
	}
	GatheringWipeScreen = false;
}

void ZDFrameBuffer::ReleaseDefaultPoolItems()
{
	FBTexture.reset();
	FinalWipeScreen.reset();
	InitialWipeScreen.reset();
	VertexBuffer.reset();
	IndexBuffer.reset();
	OutputFB.reset();
}

bool ZDFrameBuffer::Reset()
{
	ReleaseDefaultPoolItems();

	OutputFB = CreateFrameBuffer("OutputFB", GetWidth(), GetHeight());
	if (!OutputFB || !CreateFBTexture() || !CreateVertexes())
	{
		return false;
	}

	GetContext()->SetFrameBuffer(OutputFB->Framebuffer);
	GetContext()->SetViewport(0, 0, GetWidth(), GetHeight());

	SetInitialState();
	return true;
}

void ZDFrameBuffer::SetGammaRamp(const GammaRamp *ramp)
{
}

void ZDFrameBuffer::KillNativePals()
{
	while (Palettes != nullptr)
	{
		delete Palettes;
	}
}

void ZDFrameBuffer::KillNativeTexs()
{
	while (Textures != nullptr)
	{
		delete Textures;
	}
}

bool ZDFrameBuffer::CreateFBTexture()
{
	FBTexture = CreateTexture("FBTexture", GetWidth(), GetHeight(), 1, IsBgra() ? GPUPixelFormat::BGRA8 : GPUPixelFormat::R8);
	return FBTexture != nullptr;
}

bool ZDFrameBuffer::CreatePaletteTexture()
{
	PaletteTexture = CreateTexture("PaletteTexture", 256, 1, 1, GPUPixelFormat::BGRA8);
	return PaletteTexture != nullptr;
}

bool ZDFrameBuffer::CreateVertexes()
{
	VertexPos = -1;
	IndexPos = -1;
	QuadBatchPos = -1;
	BatchType = BATCH_None;
	VertexBuffer = CreateVertexBuffer(sizeof(FBVERTEX)*NUM_VERTS);
	if (!VertexBuffer)
	{
		return false;
	}
	IndexBuffer = CreateIndexBuffer(sizeof(uint16_t)*NUM_INDEXES);
	if (!IndexBuffer)
	{
		return false;
	}
	return true;
}

void ZDFrameBuffer::CalcFullscreenCoords(FBVERTEX verts[4], bool viewarea_only, uint32_t color0, uint32_t color1) const
{
	float mxl, mxr, myt, myb, tmxl, tmxr, tmyt, tmyb;

	if (viewarea_only)
	{ // Just calculate vertices for the viewarea/BlendingRect
		mxl = float(BlendingRect.left);
		mxr = float(BlendingRect.right);
		myt = float(BlendingRect.top);
		myb = float(BlendingRect.bottom);
		tmxl = float(BlendingRect.left) / float(GetWidth());
		tmxr = float(BlendingRect.right) / float(GetWidth());
		tmyt = float(BlendingRect.top) / float(GetHeight());
		tmyb = float(BlendingRect.bottom) / float(GetHeight());
	}
	else
	{ // Calculate vertices for the whole screen
		mxl = 0.0f;
		mxr = float(GetWidth());
		myt = 0.0f;
		myb = float(GetHeight());
		tmxl = 0;
		tmxr = 1.0f;
		tmyt = 0;
		tmyb = 1.0f;
	}

	//{   mxl, myt, 0, 1, 0, 0xFFFFFFFF,    tmxl,    tmyt },
	//{   mxr, myt, 0, 1, 0, 0xFFFFFFFF,    tmxr,    tmyt },
	//{   mxr, myb, 0, 1, 0, 0xFFFFFFFF,    tmxr,    tmyb },
	//{   mxl, myb, 0, 1, 0, 0xFFFFFFFF,    tmxl,    tmyb },

	verts[0].x = mxl;
	verts[0].y = myt;
	verts[0].z = 0;
	verts[0].rhw = 1;
	verts[0].color0 = color0;
	verts[0].color1 = color1;
	verts[0].tu = tmxl;
	verts[0].tv = tmyt;

	verts[1].x = mxr;
	verts[1].y = myt;
	verts[1].z = 0;
	verts[1].rhw = 1;
	verts[1].color0 = color0;
	verts[1].color1 = color1;
	verts[1].tu = tmxr;
	verts[1].tv = tmyt;

	verts[2].x = mxr;
	verts[2].y = myb;
	verts[2].z = 0;
	verts[2].rhw = 1;
	verts[2].color0 = color0;
	verts[2].color1 = color1;
	verts[2].tu = tmxr;
	verts[2].tv = tmyb;

	verts[3].x = mxl;
	verts[3].y = myb;
	verts[3].z = 0;
	verts[3].rhw = 1;
	verts[3].color0 = color0;
	verts[3].color1 = color1;
	verts[3].tu = tmxl;
	verts[3].tv = tmyb;
}

void ZDFrameBuffer::BgraToRgba(uint32_t *dest, const uint32_t *src, int width, int height, int srcpitch)
{
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			uint32_t r = RPART(src[x]);
			uint32_t g = GPART(src[x]);
			uint32_t b = BPART(src[x]);
			uint32_t a = APART(src[x]);
			dest[x] = r | (g << 8) | (b << 16) | (a << 24);
		}
		dest += width;
		src += srcpitch;
	}
}

void ZDFrameBuffer::UploadPalette()
{
	if (!PaletteTexture->Buffers[0])
	{
		PaletteTexture->Buffers[0] = GetContext()->CreateStagingTexture(256, 1, GPUPixelFormat::BGRA8);
		PaletteTexture->Buffers[1] = GetContext()->CreateStagingTexture(256, 1, GPUPixelFormat::BGRA8);
	}

	auto current = PaletteTexture->Buffers[PaletteTexture->CurrentBuffer];
	PaletteTexture->CurrentBuffer = (PaletteTexture->CurrentBuffer + 1) & 1;

	uint8_t *pix = (uint8_t*)current->Map();
	if (pix)
	{
		int i;

		for (i = 0; i < 256; ++i, pix += 4)
		{
			pix[0] = SourcePalette[i].b;
			pix[1] = SourcePalette[i].g;
			pix[2] = SourcePalette[i].r;
			pix[3] = (i == 0 ? 0 : 255);
			// To let masked textures work, the first palette entry's alpha is 0.
		}
		pix += 4;
		for (; i < 255; ++i, pix += 4)
		{
			pix[0] = SourcePalette[i].b;
			pix[1] = SourcePalette[i].g;
			pix[2] = SourcePalette[i].r;
			pix[3] = 255;
		}

		current->Unmap();
		GetContext()->CopyTexture(PaletteTexture->Texture, current);
		BorderColor = ColorXRGB(SourcePalette[255].r, SourcePalette[255].g, SourcePalette[255].b);
	}
}


void ZDFrameBuffer::SetBlendingRect(int x1, int y1, int x2, int y2)
{
	BlendingRect.left = x1;
	BlendingRect.top = y1;
	BlendingRect.right = x2;
	BlendingRect.bottom = y2;
}

/**************************************************************************/
/*                                  2D Stuff                              */
/**************************************************************************/

// DEBUG: Draws the texture atlases to the screen, starting with the
// 1-based packnum. Ignores atlases that are flagged for use by one
// texture only.

void ZDFrameBuffer::DrawPackedTextures(int packnum)
{
	uint32_t empty_colors[8] =
	{
		0x50FF0000, 0x5000FF00, 0x500000FF, 0x50FFFF00,
		0x50FF00FF, 0x5000FFFF, 0x50FF8000, 0x500080FF
	};
	Atlas *pack;
	int x = 8, y = 8;

	if (packnum <= 0)
	{
		return;
	}
	pack = Atlases;
	// Find the first texture atlas that is an actual atlas.
	while (pack != nullptr && pack->OneUse)
	{ // Skip textures that aren't used as atlases
		pack = pack->Next;
	}
	// Skip however many atlases we would have otherwise drawn
	// until we've skipped <packnum> of them.
	while (pack != nullptr && packnum != 1)
	{
		if (!pack->OneUse)
		{ // Skip textures that aren't used as atlases
			packnum--;
		}
		pack = pack->Next;
	}
	// Draw atlases until we run out of room on the screen.
	while (pack != nullptr)
	{
		if (pack->OneUse)
		{ // Skip textures that aren't used as atlases
			pack = pack->Next;
			continue;
		}

		AddColorOnlyRect(x - 1, y - 1, 258, 258, ColorXRGB(255, 255, 0));
		int back = 0;
		for (PackedTexture *box = pack->UsedList; box != nullptr; box = box->Next)
		{
			AddColorOnlyQuad(
				x + box->Area.left * 256 / pack->Width,
				y + box->Area.top * 256 / pack->Height,
				(box->Area.right - box->Area.left) * 256 / pack->Width,
				(box->Area.bottom - box->Area.top) * 256 / pack->Height, empty_colors[back]);
			back = (back + 1) & 7;
		}
		//		AddColorOnlyQuad(x, y-LBOffsetI, 256, 256, ColorARGB(180,0,0,0));

		CheckQuadBatch();

		BufferedTris *quad = &QuadExtra[QuadBatchPos];
		FBVERTEX *vert = &VertexData[VertexPos];

		quad->ClearSetup();
		if (pack->Format == GPUPixelFormat::R8/* && !tex->IsGray*/)
		{
			quad->Flags = BQF_WrapUV | BQF_GamePalette/* | BQF_DisableAlphaTest*/;
			quad->ShaderNum = BQS_PalTex;
		}
		else
		{
			quad->Flags = BQF_WrapUV/* | BQF_DisableAlphaTest*/;
			quad->ShaderNum = BQS_Plain;
		}
		quad->Palette = nullptr;
		quad->Texture = pack->Tex.get();
		quad->NumVerts = 4;
		quad->NumTris = 2;

		float x0 = float(x);
		float y0 = float(y);
		float x1 = x0 + 256.f;
		float y1 = y0 + 256.f;

		vert[0].x = x0;
		vert[0].y = y0;
		vert[0].z = 0;
		vert[0].rhw = 1;
		vert[0].color0 = 0;
		vert[0].color1 = 0xFFFFFFFF;
		vert[0].tu = 0;
		vert[0].tv = 0;

		vert[1].x = x1;
		vert[1].y = y0;
		vert[1].z = 0;
		vert[1].rhw = 1;
		vert[1].color0 = 0;
		vert[1].color1 = 0xFFFFFFFF;
		vert[1].tu = 1;
		vert[1].tv = 0;

		vert[2].x = x1;
		vert[2].y = y1;
		vert[2].z = 0;
		vert[2].rhw = 1;
		vert[2].color0 = 0;
		vert[2].color1 = 0xFFFFFFFF;
		vert[2].tu = 1;
		vert[2].tv = 1;

		vert[3].x = x0;
		vert[3].y = y1;
		vert[3].z = 0;
		vert[3].rhw = 1;
		vert[3].color0 = 0;
		vert[3].color1 = 0xFFFFFFFF;
		vert[3].tu = 0;
		vert[3].tv = 1;

		IndexData[IndexPos] = VertexPos;
		IndexData[IndexPos + 1] = VertexPos + 1;
		IndexData[IndexPos + 2] = VertexPos + 2;
		IndexData[IndexPos + 3] = VertexPos;
		IndexData[IndexPos + 4] = VertexPos + 2;
		IndexData[IndexPos + 5] = VertexPos + 3;

		QuadBatchPos++;
		VertexPos += 4;
		IndexPos += 6;

		x += 256 + 8;
		if (x > GetWidth() - 256)
		{
			x = 8;
			y += 256 + 8;
			if (y > GetHeight() - 256)
			{
				return;
			}
		}
		pack = pack->Next;
	}
}

// Finds space to pack an image inside a texture atlas and returns it.
// Large images and those that need to wrap always get their own textures.
ZDFrameBuffer::PackedTexture *ZDFrameBuffer::AllocPackedTexture(int w, int h, bool wrapping, GPUPixelFormat format)
{
	Atlas *pack;
	Rect box;
	bool padded;

	// The - 2 to account for padding
	if (w > 256 - 2 || h > 256 - 2 || wrapping)
	{ // Create a new texture atlas.
		pack = new Atlas(this, w, h, format);
		pack->OneUse = true;
		box = pack->Packer.Insert(w, h);
		padded = false;
	}
	else
	{ // Try to find space in an existing texture atlas.
		w += 2; // Add padding
		h += 2;
		for (pack = Atlases; pack != nullptr; pack = pack->Next)
		{
			// Use the first atlas it fits in.
			if (pack->Format == format)
			{
				box = pack->Packer.Insert(w, h);
				if (box.width != 0)
				{
					break;
				}
			}
		}
		if (pack == nullptr)
		{ // Create a new texture atlas.
			pack = new Atlas(this, DEF_ATLAS_WIDTH, DEF_ATLAS_HEIGHT, format);
			box = pack->Packer.Insert(w, h);
		}
		padded = true;
	}
	assert(box.width != 0 && box.height != 0);
	return pack->AllocateImage(box, padded);
}

//==========================================================================
//
// Atlas Constructor
//
//==========================================================================

ZDFrameBuffer::Atlas::Atlas(ZDFrameBuffer *fb, int w, int h, GPUPixelFormat format)
	: Packer(w, h, true)
{
	Format = format;
	UsedList = nullptr;
	OneUse = false;
	Width = 0;
	Height = 0;
	Next = nullptr;

	// Attach to the end of the atlas list
	Atlas **prev = &fb->Atlases;
	while (*prev != nullptr)
	{
		prev = &((*prev)->Next);
	}
	*prev = this;

	Tex = fb->CreateTexture("Atlas", w, h, 1, format);
	Width = w;
	Height = h;
}

//==========================================================================
//
// Atlas Destructor
//
//==========================================================================

ZDFrameBuffer::Atlas::~Atlas()
{
	PackedTexture *box, *next;

	Tex.reset();
	for (box = UsedList; box != nullptr; box = next)
	{
		next = box->Next;
		delete box;
	}
}

//==========================================================================
//
// Atlas :: AllocateImage
//
// Moves the box from the empty list to the used list, sizing it to the
// requested dimensions and adding additional boxes to the empty list if
// needed.
//
// The passed box *MUST* be in this texture atlas's empty list.
//
//==========================================================================

ZDFrameBuffer::PackedTexture *ZDFrameBuffer::Atlas::AllocateImage(const Rect &rect, bool padded)
{
	PackedTexture *box = new PackedTexture;

	box->Owner = this;
	box->Area.left = rect.x;
	box->Area.top = rect.y;
	box->Area.right = rect.x + rect.width;
	box->Area.bottom = rect.y + rect.height;

	box->Left = float(box->Area.left + padded) / Width;
	box->Right = float(box->Area.right - padded) / Width;
	box->Top = float(box->Area.top + padded) / Height;
	box->Bottom = float(box->Area.bottom - padded) / Height;

	box->Padded = padded;

	// Add it to the used list.
	box->Next = UsedList;
	if (box->Next != nullptr)
	{
		box->Next->Prev = &box->Next;
	}
	UsedList = box;
	box->Prev = &UsedList;

	return box;
}

//==========================================================================
//
// Atlas :: FreeBox
//
// Removes a box from the used list and deletes it. Space is returned to the
// waste list. Once all boxes for this atlas are freed, the entire bin
// packer is reinitialized for maximum efficiency.
//
//==========================================================================

void ZDFrameBuffer::Atlas::FreeBox(ZDFrameBuffer::PackedTexture *box)
{
	*(box->Prev) = box->Next;
	if (box->Next != nullptr)
	{
		box->Next->Prev = box->Prev;
	}
	Rect waste;
	waste.x = box->Area.left;
	waste.y = box->Area.top;
	waste.width = box->Area.right - box->Area.left;
	waste.height = box->Area.bottom - box->Area.top;
	box->Owner->Packer.AddWaste(waste);
	delete box;
	if (UsedList == nullptr)
	{
		Packer.Init(Width, Height, true);
	}
}

//==========================================================================
//
// OpenGLTex Constructor
//
//==========================================================================

ZDFrameBuffer::OpenGLTex::OpenGLTex(FTexture *tex, ZDFrameBuffer *fb, bool wrapping)
{
	// Attach to the texture list for the ZDFrameBuffer
	Next = fb->Textures;
	if (Next != nullptr)
	{
		Next->Prev = &Next;
	}
	Prev = &fb->Textures;
	fb->Textures = this;

	GameTex = tex;
	Box = nullptr;
	IsGray = false;

	Create(fb, wrapping);
}

//==========================================================================
//
// OpenGLTex Destructor
//
//==========================================================================

ZDFrameBuffer::OpenGLTex::~OpenGLTex()
{
	if (Box != nullptr)
	{
		Box->Owner->FreeBox(Box);
		Box = nullptr;
	}
	// Detach from the texture list
	*Prev = Next;
	if (Next != nullptr)
	{
		Next->Prev = Prev;
	}
	// Remove link from the game texture
	if (GameTex != nullptr)
	{
		GameTex->Native = nullptr;
	}
}

//==========================================================================
//
// OpenGLTex :: CheckWrapping
//
// Returns true if the texture is compatible with the specified wrapping
// mode.
//
//==========================================================================

bool ZDFrameBuffer::OpenGLTex::CheckWrapping(bool wrapping)
{
	// If it doesn't need to wrap, then it works.
	if (!wrapping)
	{
		return true;
	}
	// If it needs to wrap, then it can't be packed inside another texture.
	return Box->Owner->OneUse;
}

//==========================================================================
//
// OpenGLTex :: Create
//
// Creates an HWTexture for the texture and copies the image data
// to it. Note that unlike FTexture, this image is row-major.
//
//==========================================================================

bool ZDFrameBuffer::OpenGLTex::Create(ZDFrameBuffer *fb, bool wrapping)
{
	assert(Box == nullptr);
	if (Box != nullptr)
	{
		Box->Owner->FreeBox(Box);
	}

	Box = fb->AllocPackedTexture(GameTex->GetWidth(), GameTex->GetHeight(), wrapping, GetTexFormat());

	if (Box == nullptr)
	{
		return false;
	}
	if (!Update())
	{
		Box->Owner->FreeBox(Box);
		Box = nullptr;
		return false;
	}
	return true;
}

//==========================================================================
//
// OpenGLTex :: Update
//
// Copies image data from the underlying FTexture to the OpenGL texture.
//
//==========================================================================

bool ZDFrameBuffer::OpenGLTex::Update()
{
	LTRBRect rect;
	uint8_t *dest;

	assert(Box != nullptr);
	assert(Box->Owner != nullptr);
	assert(Box->Owner->Tex != nullptr);
	assert(GameTex != nullptr);

	GPUPixelFormat format = Box->Owner->Tex->Format;

	rect = Box->Area;

	int bytesPerPixel = (format == GPUPixelFormat::R8) ? 1 : 4;

	int uploadX = rect.left;
	int uploadY = rect.top;
	int uploadWidth = rect.right - rect.left;
	int uploadHeight = rect.bottom - rect.top;
	int pitch = uploadWidth * bytesPerPixel;
	int buffersize = pitch * uploadHeight;

	Box->Owner->Tex->CurrentBuffer = (Box->Owner->Tex->CurrentBuffer + 1) & 1;

	static std::vector<uint8_t> tempbuffer;
	tempbuffer.resize(buffersize);

	uint8_t *bits = tempbuffer.data();
	dest = bits;
	if (!dest)
	{
		return false;
	}
	if (Box->Padded)
	{
		dest += pitch + bytesPerPixel;
	}
	GameTex->FillBuffer(dest, pitch, GameTex->GetHeight(), ToTexFmt(format));
	if (Box->Padded)
	{
		// Clear top padding row.
		dest = bits;
		int numbytes = (GameTex->GetWidth() + 2) * bytesPerPixel;
		memset(dest, 0, numbytes);
		dest += pitch;
		// Clear left and right padding columns.
		if (bytesPerPixel == 1)
		{
			for (int y = Box->Area.bottom - Box->Area.top - 2; y > 0; --y)
			{
				dest[0] = 0;
				dest[numbytes - 1] = 0;
				dest += pitch;
			}
		}
		else
		{
			for (int y = Box->Area.bottom - Box->Area.top - 2; y > 0; --y)
			{
				*(uint32_t *)dest = 0;
				*(uint32_t *)(dest + numbytes - 4) = 0;
				dest += pitch;
			}
		}
		// Clear bottom padding row.
		memset(dest, 0, numbytes);
	}

	Box->Owner->Tex->Texture->Upload(uploadX, uploadY, uploadWidth, uploadHeight, 0, bits);
	return true;
}

//==========================================================================
//
// OpenGLTex :: GetTexFormat
//
// Returns the texture format that would best fit this texture.
//
//==========================================================================

GPUPixelFormat ZDFrameBuffer::OpenGLTex::GetTexFormat()
{
	FTextureFormat fmt = GameTex->GetFormat();

	IsGray = false;

	switch (fmt)
	{
	case TEX_Pal:	return GPUPixelFormat::R8;
	case TEX_Gray:	IsGray = true; return GPUPixelFormat::R8;
	case TEX_RGB:	return GPUPixelFormat::BGRA8;
	case TEX_DXT1:	I_FatalError("TEX_DXT1 is currently not supported.");
	case TEX_DXT2:	I_FatalError("TEX_DXT2 is currently not supported.");
	case TEX_DXT3:	I_FatalError("TEX_DXT3 is currently not supported.");
	case TEX_DXT4:	I_FatalError("TEX_DXT4 is currently not supported.");
	case TEX_DXT5:	I_FatalError("TEX_DXT5 is currently not supported.");
	default:		I_FatalError("GameTex->GetFormat() returned invalid format.");
	}
	return GPUPixelFormat::R8;
}

//==========================================================================
//
// OpenGLTex :: ToTexFmt
//
// Converts an OpenGL internal format constant to something the FTexture system
// understands.
//
//==========================================================================

FTextureFormat ZDFrameBuffer::OpenGLTex::ToTexFmt(GPUPixelFormat fmt)
{
	switch (fmt)
	{
	case GPUPixelFormat::R8: return IsGray ? TEX_Gray : TEX_Pal;
	case GPUPixelFormat::RGBA8: return TEX_RGB;
	case GPUPixelFormat::BGRA8: return TEX_RGB;
	default: return TEX_Pal;
	}
}

//==========================================================================
//
// OpenGLPal Constructor
//
//==========================================================================

ZDFrameBuffer::OpenGLPal::OpenGLPal(FRemapTable *remap, ZDFrameBuffer *fb)
	: Remap(remap), fb(fb)
{
	int count;

	// Attach to the palette list for the ZDFrameBuffer
	Next = fb->Palettes;
	if (Next != nullptr)
	{
		Next->Prev = &Next;
	}
	Prev = &fb->Palettes;
	fb->Palettes = this;

	int pow2count;

	// Round up to the nearest power of 2.
	for (pow2count = 1; pow2count < remap->NumEntries; pow2count <<= 1)
	{
	}
	count = pow2count;
	DoColorSkip = false;

	BorderColor = 0;
	RoundedPaletteSize = count;
	Tex = fb->CreateTexture("Pal", count, 1, 1, GPUPixelFormat::BGRA8);
	if (Tex)
	{
		if (!Update())
		{
			Tex.reset();
		}
	}
}

//==========================================================================
//
// OpenGLPal Destructor
//
//==========================================================================

ZDFrameBuffer::OpenGLPal::~OpenGLPal()
{
	Tex.reset();
	// Detach from the palette list
	*Prev = Next;
	if (Next != nullptr)
	{
		Next->Prev = Prev;
	}
	// Remove link from the remap table
	if (Remap != nullptr)
	{
		Remap->Native = nullptr;
	}
}

//==========================================================================
//
// OpenGLPal :: Update
//
// Copies the palette to the texture.
//
//==========================================================================

bool ZDFrameBuffer::OpenGLPal::Update()
{
	uint32_t *buff;
	const PalEntry *pal;
	int skipat, i;

	assert(Tex != nullptr);

	if (!Tex->Buffers[0])
	{
		Tex->Buffers[0] = fb->GetContext()->CreateStagingTexture(RoundedPaletteSize, 1, GPUPixelFormat::BGRA8);
		Tex->Buffers[1] = fb->GetContext()->CreateStagingTexture(RoundedPaletteSize, 1, GPUPixelFormat::BGRA8);
	}

	auto current = Tex->Buffers[Tex->CurrentBuffer];
	Tex->CurrentBuffer = (Tex->CurrentBuffer + 1) & 1;

	int numEntries = MIN(Remap->NumEntries, RoundedPaletteSize);

	std::vector<uint8_t> &tempbuffer = Tex->MapBuffer;
	tempbuffer.resize(numEntries * 4);

	buff = (uint32_t*)tempbuffer.data();
	if (buff == nullptr)
	{
		return false;
	}
	pal = Remap->Palette;

	// See explanation in UploadPalette() for skipat rationale.
	skipat = MIN(numEntries, DoColorSkip ? 256 - 8 : 256);

#ifndef NO_SSE
	// Manual SSE vectorized version here to workaround a bug in GCC's auto-vectorizer

	int sse_count = skipat / 4 * 4;
	for (i = 0; i < sse_count; i += 4)
	{
		_mm_storeu_si128((__m128i*)(&buff[i]), _mm_loadu_si128((__m128i*)(&pal[i])));
	}
	switch (skipat - i)
	{
		// fall through is intentional
	case 3: buff[i] = pal[i].d; i++;
	case 2: buff[i] = pal[i].d; i++;
	case 1: buff[i] = pal[i].d; i++;
	default: i++;
	}
	sse_count = numEntries / 4 * 4;
	__m128i alphamask = _mm_set1_epi32(0xff000000);
	while (i < sse_count)
	{
		__m128i lastcolor = _mm_loadu_si128((__m128i*)(&pal[i - 1]));
		__m128i color = _mm_loadu_si128((__m128i*)(&pal[i]));
		_mm_storeu_si128((__m128i*)(&buff[i]), _mm_or_si128(_mm_and_si128(alphamask, color), _mm_andnot_si128(alphamask, lastcolor)));
		i += 4;
	}
	switch (numEntries - i)
	{
		// fall through is intentional
	case 3: buff[i] = ColorARGB(pal[i].a, pal[i - 1].r, pal[i - 1].g, pal[i - 1].b); i++;
	case 2: buff[i] = ColorARGB(pal[i].a, pal[i - 1].r, pal[i - 1].g, pal[i - 1].b); i++;
	case 1: buff[i] = ColorARGB(pal[i].a, pal[i - 1].r, pal[i - 1].g, pal[i - 1].b); i++;
	default: break;
	}

#else
	for (i = 0; i < skipat; ++i)
	{
		buff[i] = ColorARGB(pal[i].a, pal[i].r, pal[i].g, pal[i].b);
	}
	for (++i; i < numEntries; ++i)
	{
		buff[i] = ColorARGB(pal[i].a, pal[i - 1].r, pal[i - 1].g, pal[i - 1].b);
	}
#endif
	if (numEntries > 1)
	{
		i = numEntries - 1;
		BorderColor = ColorARGB(pal[i].a, pal[i - 1].r, pal[i - 1].g, pal[i - 1].b);
	}

	current->Upload(0, 0, numEntries, 1, buff);
	fb->GetContext()->CopyTexture(Tex->Texture, current);

	return true;
}

FNativeTexture *ZDFrameBuffer::CreateTexture(FTexture *gametex, bool wrapping)
{
	if (!GetContext())
		return DFrameBuffer::CreateTexture(gametex, wrapping);

	OpenGLTex *tex = new OpenGLTex(gametex, this, wrapping);
	if (tex->Box == nullptr)
	{
		delete tex;
		return nullptr;
	}
	return tex;
}

FNativePalette *ZDFrameBuffer::CreatePalette(FRemapTable *remap)
{
	if (!GetContext())
		return DFrameBuffer::CreatePalette(remap);

	OpenGLPal *tex = new OpenGLPal(remap, this);
	if (tex->Tex == nullptr)
	{
		delete tex;
		return nullptr;
	}
	return tex;
}

void ZDFrameBuffer::BeginLineBatch()
{
	if (In2D < 2 || !InScene || BatchType == BATCH_Lines)
	{
		return;
	}
	EndQuadBatch();		// Make sure all quads have been drawn first.
	VertexData = VertexBuffer->Lock();
	VertexPos = 0;
	BatchType = BATCH_Lines;
}

void ZDFrameBuffer::EndLineBatch()
{
	if (In2D < 2 || !InScene || BatchType != BATCH_Lines)
	{
		return;
	}
	VertexBuffer->Unlock();
	if (VertexPos > 0)
	{
		SetPixelShader(Shaders[SHADER_VertexColor].get());
		SetAlphaBlend(GL_FUNC_ADD, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		SetStreamSource(VertexBuffer.get());
		DrawLineList(VertexPos / 2);
	}
	VertexPos = -1;
	BatchType = BATCH_None;
}

void ZDFrameBuffer::AddColorOnlyQuad(int left, int top, int width, int height, uint32_t color)
{
	BufferedTris *quad;
	FBVERTEX *verts;

	CheckQuadBatch();
	quad = &QuadExtra[QuadBatchPos];
	verts = &VertexData[VertexPos];

	float x = float(left);
	float y = float(top);

	quad->ClearSetup();
	quad->ShaderNum = BQS_ColorOnly;
	if ((color & 0xFF000000) != 0xFF000000)
	{
		quad->BlendOp = GL_FUNC_ADD;
		quad->SrcBlend = GL_SRC_ALPHA;
		quad->DestBlend = GL_ONE_MINUS_SRC_ALPHA;
	}
	quad->Palette = nullptr;
	quad->Texture = nullptr;
	quad->NumVerts = 4;
	quad->NumTris = 2;

	verts[0].x = x;
	verts[0].y = y;
	verts[0].z = 0;
	verts[0].rhw = 1;
	verts[0].color0 = color;
	verts[0].color1 = 0;
	verts[0].tu = 0;
	verts[0].tv = 0;

	verts[1].x = x + width;
	verts[1].y = y;
	verts[1].z = 0;
	verts[1].rhw = 1;
	verts[1].color0 = color;
	verts[1].color1 = 0;
	verts[1].tu = 0;
	verts[1].tv = 0;

	verts[2].x = x + width;
	verts[2].y = y + height;
	verts[2].z = 0;
	verts[2].rhw = 1;
	verts[2].color0 = color;
	verts[2].color1 = 0;
	verts[2].tu = 0;
	verts[2].tv = 0;

	verts[3].x = x;
	verts[3].y = y + height;
	verts[3].z = 0;
	verts[3].rhw = 1;
	verts[3].color0 = color;
	verts[3].color1 = 0;
	verts[3].tu = 0;
	verts[3].tv = 0;

	IndexData[IndexPos] = VertexPos;
	IndexData[IndexPos + 1] = VertexPos + 1;
	IndexData[IndexPos + 2] = VertexPos + 2;
	IndexData[IndexPos + 3] = VertexPos;
	IndexData[IndexPos + 4] = VertexPos + 2;
	IndexData[IndexPos + 5] = VertexPos + 3;

	QuadBatchPos++;
	VertexPos += 4;
	IndexPos += 6;
}

void ZDFrameBuffer::AddColorOnlyRect(int left, int top, int width, int height, uint32_t color)
{
	AddColorOnlyQuad(left, top, width - 1, 1, color);					// top
	AddColorOnlyQuad(left + width - 1, top, 1, height - 1, color);		// right
	AddColorOnlyQuad(left + 1, top + height - 1, width - 1, 1, color);	// bottom
	AddColorOnlyQuad(left, top + 1, 1, height - 1, color);				// left
}

// Make sure there's enough room in the batch for one more set of triangles.
void ZDFrameBuffer::CheckQuadBatch(int numtris, int numverts)
{
	if (BatchType == BATCH_Lines)
	{
		EndLineBatch();
	}
	else if (QuadBatchPos == MAX_QUAD_BATCH ||
		VertexPos + numverts > NUM_VERTS ||
		IndexPos + numtris * 3 > NUM_INDEXES)
	{
		EndQuadBatch();
	}
	if (QuadBatchPos < 0)
	{
		BeginQuadBatch();
	}
}

// Locks the vertex buffer for quads and sets the cursor to 0.
void ZDFrameBuffer::BeginQuadBatch()
{
	if (In2D < 2 || !InScene || QuadBatchPos >= 0)
	{
		return;
	}
	EndLineBatch();		// Make sure all lines have been drawn first.
	VertexData = VertexBuffer->Lock();
	IndexData = IndexBuffer->Lock();
	VertexPos = 0;
	IndexPos = 0;
	QuadBatchPos = 0;
	BatchType = BATCH_Quads;
}

// Draws all the quads that have been batched up.
void ZDFrameBuffer::EndQuadBatch()
{
	if (In2D < 2 || !InScene || BatchType != BATCH_Quads)
	{
		return;
	}
	BatchType = BATCH_None;
	VertexBuffer->Unlock();
	IndexBuffer->Unlock();
	if (QuadBatchPos == 0)
	{
		QuadBatchPos = -1;
		VertexPos = -1;
		IndexPos = -1;
		return;
	}
	SetStreamSource(VertexBuffer.get());
	SetIndices(IndexBuffer.get());
	bool uv_wrapped = false;
	bool uv_should_wrap;
	int indexpos, vertpos;

	indexpos = vertpos = 0;
	for (int i = 0; i < QuadBatchPos; )
	{
		const BufferedTris *quad = &QuadExtra[i];
		int j;

		int startindex = indexpos;
		int startvertex = vertpos;

		indexpos += quad->NumTris * 3;
		vertpos += quad->NumVerts;

		// Quads with matching parameters should be done with a single
		// DrawPrimitive call.
		for (j = i + 1; j < QuadBatchPos; ++j)
		{
			const BufferedTris *q2 = &QuadExtra[j];
			if (quad->Texture != q2->Texture ||
				!quad->IsSameSetup(*q2) ||
				quad->Palette != q2->Palette)
			{
				break;
			}
			if (quad->ShaderNum == BQS_InGameColormap && (quad->Flags & BQF_Desaturated) && quad->Desat != q2->Desat)
			{
				break;
			}
			indexpos += q2->NumTris * 3;
			vertpos += q2->NumVerts;
		}

		// Set the palette (if one)
		if ((quad->Flags & BQF_Paletted) == BQF_GamePalette)
		{
			SetPaletteTexture(PaletteTexture.get(), 256, BorderColor);
		}
		else if ((quad->Flags & BQF_Paletted) == BQF_CustomPalette)
		{
			assert(quad->Palette != nullptr);
			SetPaletteTexture(quad->Palette->Tex.get(), quad->Palette->RoundedPaletteSize, quad->Palette->BorderColor);
		}

		// Set the alpha blending
		SetAlphaBlend(quad->BlendOp, quad->SrcBlend, quad->DestBlend);

		// Set the alpha test
		EnableAlphaTest(!(quad->Flags & BQF_DisableAlphaTest));

		// Set the pixel shader
		if (quad->ShaderNum == BQS_PalTex)
		{
			SetPixelShader(Shaders[(quad->Flags & BQF_InvertSource) ? SHADER_NormalColorPalInv : SHADER_NormalColorPal].get());
		}
		else if (quad->ShaderNum == BQS_Plain)
		{
			SetPixelShader(Shaders[(quad->Flags & BQF_InvertSource) ? SHADER_NormalColorInv : SHADER_NormalColor].get());
		}
		else if (quad->ShaderNum == BQS_RedToAlpha)
		{
			SetPixelShader(Shaders[(quad->Flags & BQF_InvertSource) ? SHADER_RedToAlphaInv : SHADER_RedToAlpha].get());
		}
		else if (quad->ShaderNum == BQS_ColorOnly)
		{
			SetPixelShader(Shaders[SHADER_VertexColor].get());
		}
		else if (quad->ShaderNum == BQS_SpecialColormap)
		{
			int select;

			select = !!(quad->Flags & BQF_Paletted);
			SetPixelShader(Shaders[SHADER_SpecialColormap + select].get());
		}
		else if (quad->ShaderNum == BQS_InGameColormap)
		{
			int select;

			select = !!(quad->Flags & BQF_Desaturated);
			select |= !!(quad->Flags & BQF_InvertSource) << 1;
			select |= !!(quad->Flags & BQF_Paletted) << 2;
			if (quad->Flags & BQF_Desaturated)
			{
				Vec4f desaturation = { quad->Desat / 255.f, (255 - quad->Desat) / 255.f, 0, 0 };
				if (desaturation != ShaderConstants.Desaturation)
				{
					ShaderConstants.Desaturation = desaturation;
					ShaderConstantsModified = true;
				}
			}
			SetPixelShader(Shaders[SHADER_InGameColormap + select].get());
		}

		// Set the texture clamp addressing mode
		uv_should_wrap = !!(quad->Flags & BQF_WrapUV);
		if (uv_wrapped != uv_should_wrap)
		{
			uv_wrapped = uv_should_wrap;
			GetContext()->SetSampler(0, uv_should_wrap ? SamplerRepeat : SamplerClampToEdge);
		}

		// Set the texture
		if (quad->Texture != nullptr)
		{
			SetTexture(0, quad->Texture);
		}

		// Draw the quad
		DrawTriangleList(
			startvertex,					// MinIndex
			vertpos - startvertex,			// NumVertices
			startindex,						// StartIndex
			(indexpos - startindex) / 3		// PrimitiveCount
		/*4 * i, 4 * (j - i), 6 * i, 2 * (j - i)*/);
		i = j;
	}
	if (uv_wrapped)
	{
		GetContext()->SetSampler(0, SamplerClampToEdge);
	}
	QuadBatchPos = -1;
	VertexPos = -1;
	IndexPos = -1;
}

// Draws whichever type of primitive is currently being batched.
void ZDFrameBuffer::EndBatch()
{
	if (BatchType == BATCH_Quads)
	{
		EndQuadBatch();
	}
	else if (BatchType == BATCH_Lines)
	{
		EndLineBatch();
	}
}

// Patterned after R_SetPatchStyle.
bool ZDFrameBuffer::SetStyle(OpenGLTex *tex, DrawParms &parms, uint32_t &color0, uint32_t &color1, BufferedTris &quad)
{
	GPUPixelFormat fmt = tex->GetTexFormat();
	FRenderStyle style = parms.style;
	float alpha;
	bool stencilling;

	if (style.Flags & STYLEF_TransSoulsAlpha)
	{
		alpha = transsouls;
	}
	else if (style.Flags & STYLEF_Alpha1)
	{
		alpha = 1;
	}
	else
	{
		alpha = clamp(parms.Alpha, 0.f, 1.f);
	}

	style.CheckFuzz();
	if (style.BlendOp == STYLEOP_Shadow)
	{
		style = LegacyRenderStyles[STYLE_TranslucentStencil];
		alpha = 0.3f;
		parms.fillcolor = 0;
	}

	// FIXME: Fuzz effect is not written
	if (style.BlendOp == STYLEOP_FuzzOrAdd || style.BlendOp == STYLEOP_Fuzz)
	{
		style.BlendOp = STYLEOP_Add;
	}
	else if (style.BlendOp == STYLEOP_FuzzOrSub)
	{
		style.BlendOp = STYLEOP_Sub;
	}
	else if (style.BlendOp == STYLEOP_FuzzOrRevSub)
	{
		style.BlendOp = STYLEOP_RevSub;
	}

	stencilling = false;
	quad.Palette = nullptr;
	quad.Flags = 0;
	quad.Desat = 0;

	switch (style.BlendOp)
	{
	default:
	case STYLEOP_Add:		quad.BlendOp = GL_FUNC_ADD;			break;
	case STYLEOP_Sub:		quad.BlendOp = GL_FUNC_SUBTRACT;		break;
	case STYLEOP_RevSub:	quad.BlendOp = GL_FUNC_REVERSE_SUBTRACT;	break;
	case STYLEOP_None:		return false;
	}
	quad.SrcBlend = GetStyleAlpha(style.SrcAlpha);
	quad.DestBlend = GetStyleAlpha(style.DestAlpha);

	if (style.Flags & STYLEF_InvertOverlay)
	{
		// Only the overlay color is inverted, not the overlay alpha.
		parms.colorOverlay = ColorARGB(APART(parms.colorOverlay),
			255 - RPART(parms.colorOverlay), 255 - GPART(parms.colorOverlay),
			255 - BPART(parms.colorOverlay));
	}

	SetColorOverlay(parms.colorOverlay, alpha, color0, color1);

	if (style.Flags & STYLEF_ColorIsFixed)
	{
		if (style.Flags & STYLEF_InvertSource)
		{ // Since the source color is a constant, we can invert it now
		  // without spending time doing it in the shader.
			parms.fillcolor = ColorXRGB(255 - RPART(parms.fillcolor),
				255 - GPART(parms.fillcolor), 255 - BPART(parms.fillcolor));
		}
		// Set up the color mod to replace the color from the image data.
		color0 = (color0 & ColorRGBA(0, 0, 0, 255)) | (parms.fillcolor & ColorRGBA(255, 255, 255, 0));
		color1 &= ColorRGBA(0, 0, 0, 255);

		if (style.Flags & STYLEF_RedIsAlpha)
		{
			// Note that if the source texture is paletted, the palette is ignored.
			quad.Flags = 0;
			quad.ShaderNum = BQS_RedToAlpha;
		}
		else if (fmt == GPUPixelFormat::R8)
		{
			quad.Flags = BQF_GamePalette;
			quad.ShaderNum = BQS_PalTex;
		}
		else
		{
			quad.Flags = 0;
			quad.ShaderNum = BQS_Plain;
		}
	}
	else
	{
		if (style.Flags & STYLEF_RedIsAlpha)
		{
			quad.Flags = 0;
			quad.ShaderNum = BQS_RedToAlpha;
		}
		else if (fmt == GPUPixelFormat::R8)
		{
			if (parms.remap != nullptr)
			{
				quad.Flags = BQF_CustomPalette;
				quad.Palette = reinterpret_cast<OpenGLPal *>(parms.remap->GetNative());
				quad.ShaderNum = BQS_PalTex;
			}
			else if (tex->IsGray)
			{
				quad.Flags = 0;
				quad.ShaderNum = BQS_Plain;
			}
			else
			{
				quad.Flags = BQF_GamePalette;
				quad.ShaderNum = BQS_PalTex;
			}
		}
		else
		{
			quad.Flags = 0;
			quad.ShaderNum = BQS_Plain;
		}
		if (style.Flags & STYLEF_InvertSource)
		{
			quad.Flags |= BQF_InvertSource;
		}

		if (parms.specialcolormap != nullptr)
		{ // Emulate an invulnerability or similar colormap.
			float *start, *end;
			start = parms.specialcolormap->ColorizeStart;
			end = parms.specialcolormap->ColorizeEnd;
			if (quad.Flags & BQF_InvertSource)
			{
				quad.Flags &= ~BQF_InvertSource;
				swapvalues(start, end);
			}
			quad.ShaderNum = BQS_SpecialColormap;
			color0 = ColorRGBA(uint32_t(start[0] / 2 * 255), uint32_t(start[1] / 2 * 255), uint32_t(start[2] / 2 * 255), color0 >> 24);
			color1 = ColorRGBA(uint32_t(end[0] / 2 * 255), uint32_t(end[1] / 2 * 255), uint32_t(end[2] / 2 * 255), color1 >> 24);
		}
		else if (parms.colormapstyle != nullptr)
		{ // Emulate the fading from an in-game colormap (colorized, faded, and desaturated)
			if (parms.colormapstyle->Desaturate != 0)
			{
				quad.Flags |= BQF_Desaturated;
			}
			quad.ShaderNum = BQS_InGameColormap;
			quad.Desat = parms.colormapstyle->Desaturate;
			color0 = ColorARGB(color1 >> 24,
				parms.colormapstyle->Color.r,
				parms.colormapstyle->Color.g,
				parms.colormapstyle->Color.b);
			double fadelevel = parms.colormapstyle->FadeLevel;
			color1 = ColorARGB(uint32_t((1 - fadelevel) * 255),
				uint32_t(parms.colormapstyle->Fade.r * fadelevel),
				uint32_t(parms.colormapstyle->Fade.g * fadelevel),
				uint32_t(parms.colormapstyle->Fade.b * fadelevel));
		}
	}

	// For unmasked images, force the alpha from the image data to be ignored.
	if (!parms.masked && quad.ShaderNum != BQS_InGameColormap)
	{
		color0 = (color0 & ColorRGBA(255, 255, 255, 0)) | ColorValue(0, 0, 0, alpha);
		color1 &= ColorRGBA(255, 255, 255, 0);

		// If our alpha is one and we are doing normal adding, then we can turn the blend off completely.
		if (quad.BlendOp == GL_FUNC_ADD &&
			((alpha == 1 && quad.SrcBlend == GL_SRC_ALPHA) || quad.SrcBlend == GL_ONE) &&
			((alpha == 1 && quad.DestBlend == GL_ONE_MINUS_SRC_ALPHA) || quad.DestBlend == GL_ZERO))
		{
			quad.BlendOp = 0;
		}
		quad.Flags |= BQF_DisableAlphaTest;
	}
	return true;
}

int ZDFrameBuffer::GetStyleAlpha(int type)
{
	switch (type)
	{
	case STYLEALPHA_Zero:		return GL_ZERO;
	case STYLEALPHA_One:		return GL_ONE;
	case STYLEALPHA_Src:		return GL_SRC_ALPHA;
	case STYLEALPHA_InvSrc:		return GL_ONE_MINUS_SRC_ALPHA;
	default:					return GL_ZERO;
	}
}

void ZDFrameBuffer::SetColorOverlay(uint32_t color, float alpha, uint32_t &color0, uint32_t &color1)
{
	if (APART(color) != 0)
	{
		int a = APART(color) * 256 / 255;
		color0 = ColorRGBA(
			(RPART(color) * a) >> 8,
			(GPART(color) * a) >> 8,
			(BPART(color) * a) >> 8,
			0);
		a = 256 - a;
		color1 = ColorRGBA(a, a, a, int(alpha * 255));
	}
	else
	{
		color0 = 0;
		color1 = ColorValue(1, 1, 1, alpha);
	}
}

void ZDFrameBuffer::EnableAlphaTest(bool enabled)
{
	//glEnable(GL_ALPHA_TEST); // To do: move to shader as this is only in the compatibility profile
}

void ZDFrameBuffer::SetAlphaBlend(int op, int srcblend, int destblend)
{
	if (op == 0)
	{
		GetContext()->ResetBlend();
	}
	else
	{
		GetContext()->SetBlend(op, srcblend, destblend);
	}
}

void ZDFrameBuffer::SetPixelShader(HWPixelShader *shader)
{
	if (CurPixelShader != shader)
	{
		CurPixelShader = shader;
		SetHWPixelShader(shader);
	}
}

void ZDFrameBuffer::SetTexture(int tnum, HWTexture *texture)
{
	assert(unsigned(tnum) < countof(Texture));
	if (texture)
	{
		if (Texture[tnum] != texture)
		{
			Texture[tnum] = texture;
			GetContext()->SetTexture(tnum, texture->Texture);
		}
	}
	else if (Texture[tnum] != texture)
	{
		Texture[tnum] = texture;
		GetContext()->SetTexture(tnum, nullptr);
	}
}

void ZDFrameBuffer::SetPaletteTexture(HWTexture *texture, int count, uint32_t border_color)
{
	// The pixel shader receives color indexes in the range [0.0,1.0].
	// The palette texture is also addressed in the range [0.0,1.0],
	// HOWEVER the coordinate 1.0 is the right edge of the texture and
	// not actually the texture itself. We need to scale and shift
	// the palette indexes so they lie exactly in the center of each
	// texel. For a normal palette with 256 entries, that means the
	// range we use should be [0.5,255.5], adjusted so the coordinate
	// is still within [0.0,1.0].
	//
	// The constant register c2 is used to hold the multiplier in the
	// x part and the adder in the y part.

	float fcount = 1 / float(count);
	Vec4f paletteMod = { 255 * fcount, 0.5f * fcount, 0, 0 };
	if (paletteMod != ShaderConstants.PaletteMod)
	{
		ShaderConstants.PaletteMod = paletteMod;
		ShaderConstantsModified = true;
	}

	SetTexture(1, texture);
}
