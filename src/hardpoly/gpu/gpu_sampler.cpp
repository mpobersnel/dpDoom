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
#include "gpu_sampler.h"
#include "gl/system/gl_system.h"

GPUSampler::GPUSampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV)
{
	mMinfilter = minfilter;
	mMagfilter = magfilter;
	mMipmap = mipmap;
	mWrapU = wrapU;
	mWrapV = wrapV;

	glGenSamplers(1, (GLuint*)&mHandle);

	switch (mipmap)
	{
	default:
	case GPUMipmapMode::None:
		glSamplerParameteri(mHandle, GL_TEXTURE_MIN_FILTER, minfilter == GPUSampleMode::Linear ? GL_LINEAR : GL_NEAREST);
		glSamplerParameteri(mHandle, GL_TEXTURE_MAG_FILTER, magfilter == GPUSampleMode::Linear ? GL_LINEAR : GL_NEAREST);
		break;
	case GPUMipmapMode::Nearest:
		glSamplerParameteri(mHandle, GL_TEXTURE_MIN_FILTER, minfilter == GPUSampleMode::Linear ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST);
		glSamplerParameteri(mHandle, GL_TEXTURE_MAG_FILTER, magfilter == GPUSampleMode::Linear ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST);
		break;
	case GPUMipmapMode::Linear:
		glSamplerParameteri(mHandle, GL_TEXTURE_MIN_FILTER, minfilter == GPUSampleMode::Linear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST);
		glSamplerParameteri(mHandle, GL_TEXTURE_MAG_FILTER, magfilter == GPUSampleMode::Linear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST);
		break;
	}

	switch (wrapU)
	{
	default:
	case GPUWrapMode::Repeat: glSamplerParameteri(mHandle, GL_TEXTURE_WRAP_S, GL_REPEAT); break;
	case GPUWrapMode::Mirror: glSamplerParameteri(mHandle, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT); break;
	case GPUWrapMode::ClampToEdge: glSamplerParameteri(mHandle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); break;
	}

	switch (wrapV)
	{
	default:
	case GPUWrapMode::Repeat: glSamplerParameteri(mHandle, GL_TEXTURE_WRAP_T, GL_REPEAT); break;
	case GPUWrapMode::Mirror: glSamplerParameteri(mHandle, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT); break;
	case GPUWrapMode::ClampToEdge: glSamplerParameteri(mHandle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); break;
	}
}

GPUSampler::~GPUSampler()
{
	glDeleteSamplers(1, (GLuint*)&mHandle);
}
