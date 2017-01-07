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

#pragma once

#include <memory>

enum class GPUPixelFormat
{
	RGBA8,
	sRGB8_Alpha8,
	RGBA16,
	RGBA16f,
	RGBA32f,
	Depth24_Stencil8
};

class GPUTexture
{
public:
	virtual ~GPUTexture() { }
};

class GPUTexture2D : public GPUTexture
{
public:
	GPUTexture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels = nullptr);
	~GPUTexture2D();

	void Upload(int x, int y, int width, int height, int level, const void *pixels);

	int Handle() const { return mHandle; }
	int SampleCount() const { return mSampleCount; }

private:
	GPUTexture2D(const GPUTexture2D &) = delete;
	GPUTexture2D &operator =(const GPUTexture2D &) = delete;

	static int NumLevels(int width, int height);
	static int ToInternalFormat(GPUPixelFormat format);
	static int ToUploadFormat(GPUPixelFormat format);
	static int ToUploadType(GPUPixelFormat format);

	int mHandle = 0;
	int mWidth = 0;
	int mHeight = 0;
	bool mMipmap = false;
	int mSampleCount = 0;
	GPUPixelFormat mFormat;
};

typedef std::shared_ptr<GPUTexture> GPUTexturePtr;
typedef std::shared_ptr<GPUTexture2D> GPUTexture2DPtr;
