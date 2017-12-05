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
#include "d3d11_context.h"
#include "gl/system/gl_system.h"
#include "doomtype.h"
#include "i_system.h"
#include "w_wad.h"
#include <string>

D3D11Context::D3D11Context()
{
}

D3D11Context::~D3D11Context()
{
}

std::shared_ptr<GPUTexture2D> D3D11Context::CreateTexture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels)
{
	return std::make_shared<D3D11Texture2D>(width, height, mipmap, sampleCount, format, pixels);
}

std::shared_ptr<GPUFrameBuffer> D3D11Context::CreateFrameBuffer(const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil)
{
	return std::make_shared<D3D11FrameBuffer>(color, depthstencil);
}

std::shared_ptr<GPUIndexBuffer> D3D11Context::CreateIndexBuffer(const void *data, int size)
{
	return std::make_shared<D3D11IndexBuffer>(data, size);
}

std::shared_ptr<GPUProgram> D3D11Context::CreateProgram()
{
	return std::make_shared<D3D11Program>();
}

std::shared_ptr<GPUSampler> D3D11Context::CreateSampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV)
{
	return std::make_shared<D3D11Sampler>(minfilter, magfilter, mipmap, wrapU, wrapV);
}

std::shared_ptr<GPUStorageBuffer> D3D11Context::CreateStorageBuffer(const void *data, int size)
{
	return std::make_shared<D3D11StorageBuffer>(data, size);
}

std::shared_ptr<GPUUniformBuffer> D3D11Context::CreateUniformBuffer(const void *data, int size)
{
	return std::make_shared<D3D11UniformBuffer>(data, size);
}

std::shared_ptr<GPUVertexArray> D3D11Context::CreateVertexArray(const std::vector<GPUVertexAttributeDesc> &attributes)
{
	return std::make_shared<D3D11VertexArray>(attributes);
}

std::shared_ptr<GPUVertexBuffer> D3D11Context::CreateVertexBuffer(const void *data, int size)
{
	return std::make_shared<D3D11VertexBuffer>(data, size);
}

void D3D11Context::Begin()
{
}

void D3D11Context::End()
{
}

void D3D11Context::ClearError()
{
}

void D3D11Context::CheckError()
{
}

void D3D11Context::SetFrameBuffer(const std::shared_ptr<GPUFrameBuffer> &fb)
{
}

void D3D11Context::SetViewport(int x, int y, int width, int height)
{
}

void D3D11Context::SetProgram(const std::shared_ptr<GPUProgram> &program)
{
}

void D3D11Context::SetSampler(int index, const std::shared_ptr<GPUSampler> &sampler)
{
}

void D3D11Context::SetTexture(int index, const std::shared_ptr<GPUTexture> &texture)
{
}

void D3D11Context::SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer)
{
}

void D3D11Context::SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer, ptrdiff_t offset, size_t size)
{
}

void D3D11Context::SetStorage(int index, const std::shared_ptr<GPUStorageBuffer> &storage)
{
}

void D3D11Context::SetVertexArray(const std::shared_ptr<GPUVertexArray> &vertexarray)
{
}

void D3D11Context::SetIndexBuffer(const std::shared_ptr<GPUIndexBuffer> &indexbuffer, GPUIndexFormat format)
{
	mIndexFormat = format;
}

void D3D11Context::Draw(GPUDrawMode mode, int vertexStart, int vertexCount)
{
}

void D3D11Context::DrawIndexed(GPUDrawMode mode, int indexStart, int indexCount)
{
}

void D3D11Context::DrawInstanced(GPUDrawMode mode, int vertexStart, int vertexCount, int instanceCount)
{
}

void D3D11Context::DrawIndexedInstanced(GPUDrawMode mode, int indexStart, int indexCount, int instanceCount)
{
}

void D3D11Context::ClearColorBuffer(int index, float r, float g, float b, float a)
{
}

void D3D11Context::ClearDepthBuffer(float depth)
{
}

void D3D11Context::ClearStencilBuffer(int stencil)
{
}

void D3D11Context::ClearDepthStencilBuffer(float depth, int stencil)
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11FrameBuffer::D3D11FrameBuffer(const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil)
{
}

D3D11FrameBuffer::~D3D11FrameBuffer()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11IndexBuffer::D3D11IndexBuffer(const void *data, int size)
{
}

D3D11IndexBuffer::~D3D11IndexBuffer()
{
}

void D3D11IndexBuffer::Upload(const void *data, int size)
{
}

void *D3D11IndexBuffer::MapWriteOnly()
{
	return nullptr;
}

void D3D11IndexBuffer::Unmap()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11Program::D3D11Program()
{
}

D3D11Program::~D3D11Program()
{
}

std::string D3D11Program::PrefixCode() const
{
	std::string prefix;
	for (auto it : mDefines)
	{
		prefix += "#define " + it.first + " " + it.second + "\n";
	}
	prefix += "#line 1\n";
	return prefix;
}

void D3D11Program::Compile(GPUShaderType type, const char *name, const std::string &code)
{
}

void D3D11Program::SetAttribLocation(const std::string &name, int index)
{
}

void D3D11Program::SetFragOutput(const std::string &name, int index)
{
}

void D3D11Program::SetUniformBlock(const std::string &name, int index)
{
}

void D3D11Program::Link(const std::string &name)
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11Sampler::D3D11Sampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV)
{
	mMinfilter = minfilter;
	mMagfilter = magfilter;
	mMipmap = mipmap;
	mWrapU = wrapU;
	mWrapV = wrapV;
}

D3D11Sampler::~D3D11Sampler()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11StorageBuffer::D3D11StorageBuffer(const void *data, int size)
{
}

void D3D11StorageBuffer::Upload(const void *data, int size)
{
}

D3D11StorageBuffer::~D3D11StorageBuffer()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11Texture2D::D3D11Texture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels)
{
	mWidth = width;
	mHeight = height;
	mMipmap = mipmap;
	mSampleCount = sampleCount;
	mFormat = format;
}

D3D11Texture2D::~D3D11Texture2D()
{
}

void D3D11Texture2D::Upload(int x, int y, int width, int height, int level, const void *pixels)
{
}

int D3D11Texture2D::NumLevels(int width, int height)
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

/////////////////////////////////////////////////////////////////////////////

D3D11UniformBuffer::D3D11UniformBuffer(const void *data, int size)
{
}

void D3D11UniformBuffer::Upload(const void *data, int size)
{
}

void *D3D11UniformBuffer::MapWriteOnly()
{
	return nullptr;
}

void D3D11UniformBuffer::Unmap()
{
}

D3D11UniformBuffer::~D3D11UniformBuffer()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11VertexArray::D3D11VertexArray(const std::vector<GPUVertexAttributeDesc> &attributes) : mAttributes(attributes)
{
}

D3D11VertexArray::~D3D11VertexArray()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11VertexBuffer::D3D11VertexBuffer(const void *data, int size)
{
}

D3D11VertexBuffer::~D3D11VertexBuffer()
{
}

void D3D11VertexBuffer::Upload(const void *data, int size)
{
}

void *D3D11VertexBuffer::MapWriteOnly()
{
	return nullptr;
}

void D3D11VertexBuffer::Unmap()
{
}
