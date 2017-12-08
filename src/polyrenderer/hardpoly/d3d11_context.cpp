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

#define CREATE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) EXTERN_C const GUID DECLSPEC_SELECTANY name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
CREATE_GUID(IID_ID3D11ShaderReflection, 0x8d536ca1, 0x0cca, 0x4956, 0xa8, 0x37, 0x78, 0x69, 0x63, 0x75, 0x55, 0x84);
#pragma comment(lib, "d3dcompiler.lib")

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
	for (const auto &texture : color)
	{
		auto d3dtexture = std::static_pointer_cast<D3D11Texture2D>(texture);

		D3D11_TEXTURE2D_DESC texturedesc;
		d3dtexture->Handle()->GetDesc(&texturedesc);

		D3D11_RENDER_TARGET_VIEW_DESC desc;
		desc.Format = texturedesc.Format;
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;

		ComPtr<ID3D11RenderTargetView> view;
		HRESULT result = context->Device->CreateRenderTargetView(d3dtexture->Handle(), &desc, view.OutputVariable());
		if (FAILED(result))
			I_FatalError("ID3D11Device.CreateRenderTargetView failed");
		RenderTargetViews.push_back(view);
	}

	if (depthstencil)
	{
		auto d3dtexture = std::static_pointer_cast<D3D11Texture2D>(depthstencil);

		D3D11_TEXTURE2D_DESC texturedesc;
		d3dtexture->Handle()->GetDesc(&texturedesc);

		D3D11_DEPTH_STENCIL_VIEW_DESC desc;
		desc.Format = texturedesc.Format;
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;
		desc.Flags = 0;

		HRESULT result = context->Device->CreateDepthStencilView(d3dtexture->Handle(), &desc, DepthStencilView.OutputVariable());
		if (FAILED(result))
			I_FatalError("ID3D11Device.CreateDepthStencilView failed");
	}
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

static std::string GetShaderModel(D3D_FEATURE_LEVEL featureLevel, GPUShaderType type)
{
	int major = 5;
	int minor = 1;
	switch (featureLevel)
	{
	case D3D_FEATURE_LEVEL_11_1: major = 5; minor = 1; break;
	case D3D_FEATURE_LEVEL_11_0: major = 5; minor = 0; break;
	case D3D_FEATURE_LEVEL_10_1: major = 4; minor = 1; break;
	case D3D_FEATURE_LEVEL_10_0: major = 4; minor = 0; break;
	case D3D_FEATURE_LEVEL_9_1: major = 3; minor = 1; break;
	case D3D_FEATURE_LEVEL_9_2: major = 3; minor = 2; break;
	case D3D_FEATURE_LEVEL_9_3: major = 3; minor = 3; break;
	}
	std::string prefix = (type == GPUShaderType::Vertex) ? "vs_" : "ps_";
	return prefix + std::to_string(major) + "_" + std::to_string(minor);
}

D3D11Shader::D3D11Shader(D3D11Context *context, GPUShaderType type, const char *name, const std::string &source)
{
	std::string entryPoint = "main";
	std::string shaderModel = GetShaderModel(context->FeatureLevel, type);

	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> log;
	HRESULT result = D3DCompile(
		source.data(),
		source.length(),
		0,
		0,
		0,
		entryPoint.c_str(),
		shaderModel.c_str(),
		D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_OPTIMIZATION_LEVEL3,
		0,
		blob.OutputVariable(),
		log.OutputVariable());

	std::string logText;
	if (log)
		logText = std::string(reinterpret_cast<char*>(log->GetBufferPointer()), log->GetBufferSize());

	if (SUCCEEDED(result))
	{
		Bytecode.resize(blob->GetBufferSize());
		memcpy(Bytecode.data(), blob->GetBufferPointer(), Bytecode.size());
	}
	else
	{
		I_FatalError("Could not compile %s: %s", name, logText.c_str());
	}

	CreateShader(context, type);
}

D3D11Shader::D3D11Shader(D3D11Context *context, GPUShaderType type, const char *name, const void *bytecode, int bytecodeSize)
{
	Bytecode.resize(bytecodeSize);
	memcpy(Bytecode.data(), bytecode, Bytecode.size());
	CreateShader(context, type);
}

void D3D11Shader::CreateShader(D3D11Context *context, GPUShaderType type)
{
	if (type == GPUShaderType::Vertex)
	{
		ID3D11VertexShader *shaderObj = nullptr;
		HRESULT result = context->Device->CreateVertexShader(Bytecode.data(), Bytecode.size(), nullptr, &shaderObj);
		if (FAILED(result))
			I_FatalError("CreateVertexShader failed");
		mHandle = ComPtr<ID3D11DeviceChild>(shaderObj);
	}
	else
	{
		ID3D11PixelShader *shaderObj = nullptr;
		HRESULT result = context->Device->CreatePixelShader(Bytecode.data(), Bytecode.size(), nullptr, &shaderObj);
		if (FAILED(result))
			I_FatalError("CreatePixelShader failed");
		mHandle = ComPtr<ID3D11DeviceChild>(shaderObj);
	}

	FindLocations();
}

void D3D11Shader::FindLocations()
{
	SamplerLocations.clear();
	TextureLocations.clear();
	ImageLocations.clear();
	UniformBufferLocations.clear();
	StorageBufferSrvLocations.clear();
	StorageBufferUavLocations.clear();

	ComPtr<ID3D11ShaderReflection> reflect;
	HRESULT result = D3DReflect(Bytecode.data(), Bytecode.size(), IID_ID3D11ShaderReflection, (void**)reflect.OutputVariable());
	if (FAILED(result))
		I_FatalError("D3DReflect failed");

	D3D11_SHADER_DESC desc;
	result = reflect->GetDesc(&desc);
	if (FAILED(result))
		I_FatalError("D3DReflect.GetDesc failed");

	for (UINT i = 0; i < desc.BoundResources; i++)
	{
		D3D11_SHADER_INPUT_BIND_DESC binding;
		result = reflect->GetResourceBindingDesc(i, &binding);
		if (FAILED(result))
			I_FatalError("D3DReflect.GetResourceBindingDesc failed");

		switch (binding.Type)
		{
		case D3D_SIT_CBUFFER: // constant buffer
			UniformBufferLocations[binding.Name] = binding.BindPoint;
			break;

		case D3D_SIT_TBUFFER: // texture buffer
			// What does this map to in OpenGL? glTexBuffer?
			break;

		case D3D_SIT_TEXTURE: // texture
			TextureLocations[binding.Name] = binding.BindPoint;
			break;

		case D3D_SIT_SAMPLER: // sampler
			SamplerLocations[binding.Name] = binding.BindPoint;
			break;

		case D3D_SIT_UAV_RWTYPED: // read-and-write buffer
			ImageLocations[binding.Name] = binding.BindPoint;
			break;

		case D3D_SIT_UAV_RWSTRUCTURED: // read-and-write structured buffer
		case D3D_SIT_UAV_RWBYTEADDRESS: // read-and-write byte-address buffer
		case D3D_SIT_UAV_APPEND_STRUCTURED: // append-structured buffer
		case D3D_SIT_UAV_CONSUME_STRUCTURED: // consume-structured buffer
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: // read-and-write structured buffer that uses the built-in counter to append or consume
			StorageBufferUavLocations[binding.Name] = binding.BindPoint;
			break;

		case D3D_SIT_STRUCTURED: // structured buffer
		case D3D_SIT_BYTEADDRESS: // byte-address buffer
			StorageBufferSrvLocations[binding.Name] = binding.BindPoint;
			break;
		}
	}
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
	ShaderHandle[type].reset(new D3D11Shader(mContext, type, name, code));
}

void D3D11Program::SetAttribLocation(const std::string &name, int index)
{
	// Numbers at the end of a semantic name in HLSL maps to the semantic index.
	if (!name.empty() && name[name.length() - 1] >= '0' && name[name.length() - 1] <= '9')
	{
		std::string::size_type numberStart = name.length() - 1;
		while (numberStart > 0 && name[numberStart - 1] >= '0' && name[numberStart - 1] <= '9')
			numberStart--;

		AttributeBindings[index].SemanticName = name.substr(0, numberStart);
		AttributeBindings[index].SemanticIndex = std::stoi(name.substr(numberStart));
	}
	else
	{
		AttributeBindings[index].SemanticName = name;
		AttributeBindings[index].SemanticIndex = 0;
	}
}

void D3D11Program::SetFragOutput(const std::string &name, int index)
{
	// This isn't relevant for Direct3D.  The output semantic names (SV_TargetN) have hardcoded locations in HLSL.
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

D3D11VertexArray::D3D11VertexArray(D3D11Context *context, const std::vector<GPUVertexAttributeDesc> &attributes) : mContext(context)
{
	for (const auto &attr : attributes)
	{
		mAttributes[attr.Index] = attr;
	}
}

D3D11VertexArray::~D3D11VertexArray()
{
}

ID3D11InputLayout *D3D11VertexArray::GetInputLayout(D3D11Program *program)
{
	std::map<void *, ComPtr<ID3D11InputLayout> >::iterator it = mInputLayouts.find(program);
	if (it != mInputLayouts.end())
	{
		return it->second;
	}
	else
	{
		ComPtr<ID3D11InputLayout> layout = CreateInputLayout(program);
		mInputLayouts[program] = layout;
		return layout;
	}
}

ComPtr<ID3D11InputLayout> D3D11VertexArray::CreateInputLayout(D3D11Program *program)
{
	const auto &bytecode = program->ShaderHandle[GPUShaderType::Vertex]->Bytecode;

	std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
	for (const auto &it : program->AttributeBindings)
	{
		int attribLocation = it.first;
		auto itAttrDesc = mAttributes.find(attribLocation);
		if (itAttrDesc != mAttributes.end())
		{
			D3D11_INPUT_ELEMENT_DESC desc;
			desc.SemanticName = it.second.SemanticName.c_str();
			desc.SemanticIndex = it.second.SemanticIndex;
			desc.Format = ToD3DFormat(itAttrDesc->second);
			desc.InputSlot = attribLocation;
			desc.AlignedByteOffset = (UINT)itAttrDesc->second.Offset;
			desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			desc.InstanceDataStepRate = 0;
			elements.push_back(desc);
		}
	}

	ComPtr<ID3D11InputLayout> inputLayout;
	HRESULT result = mContext->Device->CreateInputLayout(elements.data(), (UINT)elements.size(), bytecode.data(), (UINT)bytecode.size(), inputLayout.OutputVariable());
	if (FAILED(result))
		I_FatalError("CreateInputLayout failed");
	return inputLayout;
}

DXGI_FORMAT D3D11VertexArray::ToD3DFormat(const GPUVertexAttributeDesc &data)
{
	bool normalize = data.Normalized;
	switch (data.Size)
	{
	case 1:
		switch (data.Type)
		{
		case GPUVertexAttributeType::Uint8: return normalize ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8_UINT;
		case GPUVertexAttributeType::Uint16: return normalize ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R16_UINT;
		case GPUVertexAttributeType::Uint32: return /*normalize ? DXGI_FORMAT_R32_UNORM :*/ DXGI_FORMAT_R32_UINT;
		case GPUVertexAttributeType::Int8: return normalize ? DXGI_FORMAT_R8_SNORM : DXGI_FORMAT_R8_SINT;
		case GPUVertexAttributeType::Int16: return normalize ? DXGI_FORMAT_R16_SNORM : DXGI_FORMAT_R16_SINT;
		case GPUVertexAttributeType::Int32: return /*normalize ? DXGI_FORMAT_R32_SNORM :*/ DXGI_FORMAT_R32_SINT;
		case GPUVertexAttributeType::HalfFloat: return DXGI_FORMAT_R16_FLOAT;
		case GPUVertexAttributeType::Float: return DXGI_FORMAT_R32_FLOAT;
		default: return DXGI_FORMAT_R32_TYPELESS; // DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R8_TYPELESS
		}

		break;
	case 2:
		switch (data.Type)
		{
		case GPUVertexAttributeType::Uint8: return normalize ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R8G8_UINT;
		case GPUVertexAttributeType::Uint16: return normalize ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16G16_UINT;
		case GPUVertexAttributeType::Uint32: return /*normalize ? DXGI_FORMAT_R32G32_UNORM :*/ DXGI_FORMAT_R32G32_UINT;
		case GPUVertexAttributeType::Int8: return normalize ? DXGI_FORMAT_R8G8_SNORM : DXGI_FORMAT_R8G8_SINT;
		case GPUVertexAttributeType::Int16: return normalize ? DXGI_FORMAT_R16G16_SNORM : DXGI_FORMAT_R16G16_SINT;
		case GPUVertexAttributeType::Int32: return DXGI_FORMAT_R32G32_SINT;
		case GPUVertexAttributeType::HalfFloat: return DXGI_FORMAT_R16G16_FLOAT;
		case GPUVertexAttributeType::Float: return DXGI_FORMAT_R32G32_FLOAT;
		default: return DXGI_FORMAT_R32G32_TYPELESS; // DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R8G8_TYPELESS
		}
		break;
	case 3:
		switch (data.Type)
		{
		case GPUVertexAttributeType::Uint8: break;
		case GPUVertexAttributeType::Uint16: break;
		case GPUVertexAttributeType::Uint32: return /*normalize ? DXGI_FORMAT_R32G32B32_UNORM :*/ DXGI_FORMAT_R32G32B32_UINT;
		case GPUVertexAttributeType::Int8: break;
		case GPUVertexAttributeType::Int16: break;
		case GPUVertexAttributeType::Int32: return /*normalize ? DXGI_FORMAT_R32G32B32_SNORM :*/ DXGI_FORMAT_R32G32B32_SINT;
		case GPUVertexAttributeType::HalfFloat: break;
		case GPUVertexAttributeType::Float: return DXGI_FORMAT_R32G32B32_FLOAT;
		default: return DXGI_FORMAT_R32G32B32_TYPELESS; // DXGI_FORMAT_R8G8B8A8_TYPELESS
		}
		break;
	case 4:
		switch (data.Type)
		{
		case GPUVertexAttributeType::Uint8: return normalize ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UINT;
		case GPUVertexAttributeType::Uint16: return normalize ? DXGI_FORMAT_R16G16B16A16_UNORM : DXGI_FORMAT_R16G16B16A16_UINT;
		case GPUVertexAttributeType::Uint32: return /*normalize ? DXGI_FORMAT_R32G32B32A32_UNORM :*/ DXGI_FORMAT_R32G32B32A32_UINT;
		case GPUVertexAttributeType::Int8: return normalize ? DXGI_FORMAT_R8G8B8A8_SNORM : DXGI_FORMAT_R8G8B8A8_SINT;
		case GPUVertexAttributeType::Int16: return normalize ? DXGI_FORMAT_R16G16B16A16_SNORM : DXGI_FORMAT_R16G16B16A16_SINT;
		case GPUVertexAttributeType::Int32: return /*normalize ? DXGI_FORMAT_R32G32B32A32_SNORM :*/ DXGI_FORMAT_R32G32B32A32_SINT;
		case GPUVertexAttributeType::HalfFloat: return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case GPUVertexAttributeType::Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default: return DXGI_FORMAT_R32G32B32A32_TYPELESS; // DXGI_FORMAT_R16G16B16A16_TYPELESS
		}
		break;
	}
	I_FatalError("Cannot decode the datatype");
	return DXGI_FORMAT_R8G8B8A8_UNORM;
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
