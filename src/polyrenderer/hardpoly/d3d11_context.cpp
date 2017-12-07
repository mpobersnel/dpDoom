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
	return std::make_shared<D3D11Texture2D>(this, width, height, mipmap, sampleCount, format, pixels);
}

std::shared_ptr<GPUFrameBuffer> D3D11Context::CreateFrameBuffer(const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil)
{
	return std::make_shared<D3D11FrameBuffer>(this, color, depthstencil);
}

std::shared_ptr<GPUIndexBuffer> D3D11Context::CreateIndexBuffer(const void *data, int size)
{
	return std::make_shared<D3D11IndexBuffer>(this, data, size);
}

std::shared_ptr<GPUProgram> D3D11Context::CreateProgram()
{
	return std::make_shared<D3D11Program>(this);
}

std::shared_ptr<GPUSampler> D3D11Context::CreateSampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV)
{
	return std::make_shared<D3D11Sampler>(this, minfilter, magfilter, mipmap, wrapU, wrapV);
}

std::shared_ptr<GPUStorageBuffer> D3D11Context::CreateStorageBuffer(const void *data, int size)
{
	return std::make_shared<D3D11StorageBuffer>(this, data, size);
}

std::shared_ptr<GPUUniformBuffer> D3D11Context::CreateUniformBuffer(const void *data, int size)
{
	return std::make_shared<D3D11UniformBuffer>(this, data, size);
}

std::shared_ptr<GPUVertexArray> D3D11Context::CreateVertexArray(const std::vector<GPUVertexAttributeDesc> &attributes)
{
	return std::make_shared<D3D11VertexArray>(this, attributes);
}

std::shared_ptr<GPUVertexBuffer> D3D11Context::CreateVertexBuffer(const void *data, int size)
{
	return std::make_shared<D3D11VertexBuffer>(this, data, size);
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

void D3D11Context::SetUniform1i(int location, int value)
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

void D3D11Context::SetClipDistance(int index, bool enable)
{
}

void D3D11Context::SetOpaqueBlend(int srcalpha, int destalpha)
{
}

void D3D11Context::SetMaskedBlend(int srcalpha, int destalpha)
{
}

void D3D11Context::SetAlphaBlendFunc(int srcalpha, int destalpha)
{
}

void D3D11Context::SetAddClampBlend(int srcalpha, int destalpha)
{
}

void D3D11Context::SetSubClampBlend(int srcalpha, int destalpha)
{
}

void D3D11Context::SetRevSubClampBlend(int srcalpha, int destalpha)
{
}

void D3D11Context::SetAddSrcColorBlend(int srcalpha, int destalpha)
{
}

void D3D11Context::SetShadedBlend(int srcalpha, int destalpha)
{
}

void D3D11Context::SetAddClampShadedBlend(int srcalpha, int destalpha)
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

D3D11FrameBuffer::D3D11FrameBuffer(D3D11Context *context, const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil)
{
}

D3D11FrameBuffer::~D3D11FrameBuffer()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11IndexBuffer::D3D11IndexBuffer(D3D11Context *context, const void *data, int size)
{
	mSize = size;

	D3D11_SUBRESOURCE_DATA resource_data;
	resource_data.pSysMem = data;
	resource_data.SysMemPitch = 0;
	resource_data.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	HRESULT result = context->Device->CreateBuffer(&desc, &resource_data, mHandle.OutputVariable());
	if (FAILED(result))
		I_FatalError("ID3D11Device.CreateBuffer(index) failed");
}

D3D11IndexBuffer::~D3D11IndexBuffer()
{
}

void D3D11IndexBuffer::Upload(const void *data, int size)
{
	if (mSize != size)
		I_FatalError("Upload data size does not match index buffer");

	mContext->DeviceContext->UpdateSubresource(mHandle, 0, 0, data, 0, 0);
}

void *D3D11IndexBuffer::MapWriteOnly()
{
	return nullptr;
}

void D3D11IndexBuffer::Unmap()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11Program::D3D11Program(D3D11Context *context) : mContext(context)
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

int D3D11Program::GetUniformLocation(const char *name)
{
	return -1;
}

void D3D11Program::Link(const std::string &name)
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11Sampler::D3D11Sampler(D3D11Context *context, GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV)
{
	mMinfilter = minfilter;
	mMagfilter = magfilter;
	mMipmap = mipmap;
	mWrapU = wrapU;
	mWrapV = wrapV;

	D3D11_SAMPLER_DESC desc;
	desc.Filter = ToD3DFilter(minfilter, magfilter, mipmap);
	desc.AddressU = ToD3DWrap(wrapU);
	desc.AddressV = ToD3DWrap(wrapV);
	desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.MipLODBias = 0.0f;
	desc.MinLOD = -FLT_MAX;
	desc.MaxLOD = FLT_MAX;
	desc.MaxAnisotropy = 16;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.BorderColor[0] = 0.0f;
	desc.BorderColor[1] = 0.0f;
	desc.BorderColor[2] = 0.0f;
	desc.BorderColor[3] = 0.0f;

	HRESULT result = context->Device->CreateSamplerState(&desc, mHandle.OutputVariable());
	if (FAILED(result))
		I_FatalError("ID3D11Device.CreateSamplerState failed");
}

D3D11Sampler::~D3D11Sampler()
{
}

D3D11_TEXTURE_ADDRESS_MODE D3D11Sampler::ToD3DWrap(GPUWrapMode mode)
{
	switch (mode)
	{
	default:
	case GPUWrapMode::Repeat: return D3D11_TEXTURE_ADDRESS_WRAP;
	case GPUWrapMode::Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
	case GPUWrapMode::ClampToEdge: return D3D11_TEXTURE_ADDRESS_CLAMP;
	}
}

D3D11_FILTER D3D11Sampler::ToD3DFilter(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap)
{
	int filter = 0;
	if (mipmap == GPUMipmapMode::Linear)
		filter |= 1;
	if (magfilter == GPUSampleMode::Linear)
		filter |= 4;
	if (minfilter == GPUSampleMode::Linear)
		filter |= 16;
	return (D3D11_FILTER)filter;
}

/////////////////////////////////////////////////////////////////////////////

D3D11StorageBuffer::D3D11StorageBuffer(D3D11Context *context, const void *data, int size) : mContext(context)
{
	mSize = size;

	D3D11_SUBRESOURCE_DATA resource_data;
	resource_data.pSysMem = data;
	resource_data.SysMemPitch = 0;
	resource_data.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = size;

	HRESULT result = context->Device->CreateBuffer(&desc, data ? &resource_data : nullptr, mHandle.OutputVariable());
	if (FAILED(result))
		I_FatalError("ID3D11Device.CreateBuffer(storage) failed");
}

void D3D11StorageBuffer::Upload(const void *data, int size)
{
	if (mSize != size)
		I_FatalError("Upload data size does not match storage buffer");

	mContext->DeviceContext->UpdateSubresource(mHandle, 0, 0, data, 0, 0);
}

D3D11StorageBuffer::~D3D11StorageBuffer()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11Texture2D::D3D11Texture2D(D3D11Context *context, int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels)
{
	mContext = context;
	mWidth = width;
	mHeight = height;
	mMipmap = mipmap;
	mSampleCount = sampleCount;
	mFormat = format;

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = mipmap ? NumLevels(width, height) : 1;
	desc.Format = ToD3DFormat(format);
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.ArraySize = 1;

	UINT bindFlagsWithCompute = 0;
	UINT bindFlagsWithoutCompute = 0;

	if (IsStencilOrDepthFormat(format))
	{
		desc.MipLevels = 1;

		bindFlagsWithoutCompute = D3D11_BIND_DEPTH_STENCIL;
		bindFlagsWithCompute = bindFlagsWithoutCompute;
	}
	else
	{
		bindFlagsWithoutCompute = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		bindFlagsWithCompute = bindFlagsWithoutCompute | D3D11_BIND_UNORDERED_ACCESS;

		desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	desc.BindFlags = bindFlagsWithCompute;

	D3D11_SUBRESOURCE_DATA initdata;
	initdata.pSysMem = pixels;
	initdata.SysMemPitch = width * GetBytesPerPixel(format);
	initdata.SysMemSlicePitch = height * initdata.SysMemPitch;

	HRESULT result = context->Device->CreateTexture2D(&desc, pixels ? &initdata : nullptr, mHandle.OutputVariable());
	if (result == E_INVALIDARG && bindFlagsWithCompute != bindFlagsWithoutCompute)
	{
		desc.BindFlags = bindFlagsWithoutCompute;
		result = context->Device->CreateTexture2D(&desc, pixels ? &initdata : nullptr, mHandle.OutputVariable());
	}

	if (FAILED(result))
		I_FatalError("ID3D11Device.CreateTexture2D failed");
}

D3D11Texture2D::~D3D11Texture2D()
{
}

void D3D11Texture2D::Upload(int x, int y, int width, int height, int level, const void *pixels)
{
	D3D11_TEXTURE2D_DESC desc;
	mHandle->GetDesc(&desc);

	int texWidth = max(desc.Width >> level, (UINT)1);
	int texHeight = max(desc.Height >> level, (UINT)1);

	if (x < 0 || x + width > texWidth || y < 0 || y + height > texHeight)
		I_FatalError("D3D11Texture2D.Upload: out of bounds!");

	int destSubresource = D3D11CalcSubresource(level, 0, desc.MipLevels);

	D3D11_BOX box;
	box.left = x;
	box.top = y;
	box.right = x + width;
	box.bottom = y + height;
	box.front = 0;
	box.back = 1;

	int rowPitch = width * GetBytesPerPixel(mFormat);
	int slicePitch = rowPitch * height;

	mContext->DeviceContext->UpdateSubresource(mHandle, destSubresource, &box, pixels, rowPitch, slicePitch);
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

DXGI_FORMAT D3D11Texture2D::ToD3DFormat(GPUPixelFormat format)
{
	switch (format)
	{
	default:
	case GPUPixelFormat::RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case GPUPixelFormat::sRGB8_Alpha8: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case GPUPixelFormat::RGBA16: return DXGI_FORMAT_R16G16B16A16_UNORM;
	case GPUPixelFormat::RGBA16f: return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case GPUPixelFormat::RGBA32f: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case GPUPixelFormat::Depth24_Stencil8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case GPUPixelFormat::R32f: return DXGI_FORMAT_R32_FLOAT;
	case GPUPixelFormat::R8: return DXGI_FORMAT_R8_UNORM;
	}
}

int D3D11Texture2D::GetBytesPerPixel(GPUPixelFormat format)
{
	switch (format)
	{
	default:
	case GPUPixelFormat::R8:
		return 1;
	case GPUPixelFormat::RGBA8:
	case GPUPixelFormat::sRGB8_Alpha8:
	case GPUPixelFormat::Depth24_Stencil8:
	case GPUPixelFormat::R32f:
		return 4;
	case GPUPixelFormat::RGBA16:
	case GPUPixelFormat::RGBA16f:
		return 8;
	case GPUPixelFormat::RGBA32f:
		return 16;
	}
}

bool D3D11Texture2D::IsStencilOrDepthFormat(GPUPixelFormat format)
{
	return format == GPUPixelFormat::Depth24_Stencil8;
}

/////////////////////////////////////////////////////////////////////////////

D3D11UniformBuffer::D3D11UniformBuffer(D3D11Context *context, const void *data, int size) : mContext(context)
{
	mSize = size;

	D3D11_SUBRESOURCE_DATA resource_data;
	resource_data.pSysMem = data;
	resource_data.SysMemPitch = 0;
	resource_data.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	HRESULT result = context->Device->CreateBuffer(&desc, &resource_data, mHandle.OutputVariable());
	if (FAILED(result))
		I_FatalError("ID3D11Device.CreateBuffer(uniform block) failed");
}

D3D11UniformBuffer::~D3D11UniformBuffer()
{
}

void D3D11UniformBuffer::Upload(const void *data, int size)
{
	if (mSize != size)
		I_FatalError("Upload data size does not match uniform buffer");

	mContext->DeviceContext->UpdateSubresource(mHandle, 0, 0, data, 0, 0);
}

void *D3D11UniformBuffer::MapWriteOnly()
{
	return nullptr;
}

void D3D11UniformBuffer::Unmap()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11VertexArray::D3D11VertexArray(D3D11Context *context, const std::vector<GPUVertexAttributeDesc> &attributes) : mAttributes(attributes)
{
}

D3D11VertexArray::~D3D11VertexArray()
{
}

/////////////////////////////////////////////////////////////////////////////

D3D11VertexBuffer::D3D11VertexBuffer(D3D11Context *context, const void *data, int size) : mContext(context)
{
	mSize = size;

	D3D11_SUBRESOURCE_DATA resource_data;
	resource_data.pSysMem = data;
	resource_data.SysMemPitch = 0;
	resource_data.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	HRESULT result = context->Device->CreateBuffer(&desc, &resource_data, mHandle.OutputVariable());
	if (FAILED(result))
		I_FatalError("ID3D11Device.CreateBuffer(vertex) failed");
}

D3D11VertexBuffer::~D3D11VertexBuffer()
{
}

void D3D11VertexBuffer::Upload(const void *data, int size)
{
	if (mSize != size)
		I_FatalError("Upload data size does not match vertex buffer");

	mContext->DeviceContext->UpdateSubresource(mHandle, 0, 0, data, 0, 0);
}

void *D3D11VertexBuffer::MapWriteOnly()
{
	return nullptr;
}

void D3D11VertexBuffer::Unmap()
{
}
