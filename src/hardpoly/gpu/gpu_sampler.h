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

enum class GPUSampleMode
{
	Nearest,
	Linear
};

enum class GPUMipmapMode
{
	None,
	Nearest,
	Linear
};

enum class GPUWrapMode
{
	Repeat,
	Mirror,
	ClampToEdge
};

class GPUSampler
{
public:
	GPUSampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV);
	~GPUSampler();

	int Handle() const { return mHandle; }

private:
	GPUSampler(const GPUSampler &) = delete;
	GPUSampler &operator =(const GPUSampler &) = delete;

	int mHandle = 0;
	GPUSampleMode mMinfilter = GPUSampleMode::Nearest;
	GPUSampleMode mMagfilter = GPUSampleMode::Nearest;
	GPUMipmapMode mMipmap = GPUMipmapMode::None;
	GPUWrapMode mWrapU = GPUWrapMode::Repeat;
	GPUWrapMode mWrapV = GPUWrapMode::Repeat;
};

typedef std::shared_ptr<GPUSampler> GPUSamplerPtr;