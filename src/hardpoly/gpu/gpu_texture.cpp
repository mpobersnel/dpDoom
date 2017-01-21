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
#include "gpu_texture.h"
#include "gl/system/gl_system.h"

GPUTexture2D::GPUTexture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels)
{
	mWidth = width;
	mHeight = height;
	mMipmap = mipmap;
	mSampleCount = sampleCount;
	mFormat = format;

	glGenTextures(1, (GLuint*)&mHandle);

	GLint target = sampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	GLint binding = sampleCount > 1 ? GL_TEXTURE_BINDING_2D_MULTISAMPLE : GL_TEXTURE_BINDING_2D;

	GLint oldHandle;
	glGetIntegerv(binding, &oldHandle);
	glBindTexture(target, mHandle);

	if (sampleCount > 1)
	{
		glTexImage2DMultisample(target, sampleCount, ToInternalFormat(format), width, height, GL_FALSE);
	}
	else
	{
		glTexImage2D(target, 0, ToInternalFormat(format), width, height, 0, ToUploadFormat(format), ToUploadType(format), pixels);
	}

	if (mipmap && sampleCount <= 1)
	{
		if (pixels)
		{
			glGenerateMipmap(target);
		}
		else
		{
			GLint levels = NumLevels(width, height);
			GLint internalformat = ToInternalFormat(format);
			GLint uploadformat = ToUploadFormat(format);
			GLint uploadtype = ToUploadType(format);
			for (int i = 0; i < levels; i++)
			{
				glTexImage2D(target, i, internalformat, width, height, 0, uploadformat, uploadtype, nullptr);
				width = MAX(1, width / 2);
				height = MAX(1, height / 2);
			}
		}
	}

	glBindTexture(target, oldHandle);
}

GPUTexture2D::~GPUTexture2D()
{
	glDeleteTextures(1, (GLuint*)&mHandle);
}

void GPUTexture2D::Upload(int x, int y, int width, int height, int level, const void *pixels)
{
	if (mSampleCount <= 1)
	{
		GLint oldHandle, oldUnpackHandle;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldHandle);
		glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &oldUnpackHandle);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, mHandle);
		glTexSubImage2D(GL_TEXTURE_2D, level, x, y, width, height, ToUploadFormat(mFormat), ToUploadType(mFormat), pixels);
		glBindTexture(GL_TEXTURE_2D, oldHandle);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, oldUnpackHandle);
	}
}

int GPUTexture2D::NumLevels(int width, int height)
{
	int levels = 1;
	while (width > 1 || height > 1)
	{
		levels++;
		width /= 2;
		height /= 2;
	}
	return levels;
}

int GPUTexture2D::ToInternalFormat(GPUPixelFormat format)
{
	switch (format)
	{
	default:
	case GPUPixelFormat::RGBA8: return GL_RGBA8;
	case GPUPixelFormat::sRGB8_Alpha8: return GL_SRGB8_ALPHA8;
	case GPUPixelFormat::RGBA16: return GL_RGBA16;
	case GPUPixelFormat::RGBA16f: return GL_RGBA16F;
	case GPUPixelFormat::RGBA32f: return GL_RGBA32F;
	case GPUPixelFormat::Depth24_Stencil8: return GL_DEPTH24_STENCIL8;
	}
}

int GPUTexture2D::ToUploadFormat(GPUPixelFormat format)
{
	switch (format)
	{
	default:
	case GPUPixelFormat::RGBA8: return GL_RGBA;
	case GPUPixelFormat::sRGB8_Alpha8: return GL_RGBA;
	case GPUPixelFormat::RGBA16: return GL_RGBA;
	case GPUPixelFormat::RGBA16f: return GL_RGBA;
	case GPUPixelFormat::RGBA32f: return GL_RGBA;
	case GPUPixelFormat::Depth24_Stencil8: return GL_DEPTH_STENCIL;
	}
}

int GPUTexture2D::ToUploadType(GPUPixelFormat format)
{
	switch (format)
	{
	default:
	case GPUPixelFormat::RGBA8: return GL_UNSIGNED_BYTE;
	case GPUPixelFormat::sRGB8_Alpha8: return GL_UNSIGNED_BYTE;
	case GPUPixelFormat::RGBA16: return GL_UNSIGNED_SHORT;
	case GPUPixelFormat::RGBA16f: return GL_HALF_FLOAT;
	case GPUPixelFormat::RGBA32f: return GL_FLOAT;
	case GPUPixelFormat::Depth24_Stencil8: return GL_UNSIGNED_INT_24_8;
	}
}
