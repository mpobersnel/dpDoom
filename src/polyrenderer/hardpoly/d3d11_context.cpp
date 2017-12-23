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

std::shared_ptr<GPUStagingTexture> D3D11Context::CreateStagingTexture(int width, int height, GPUPixelFormat format, const void *pixels)
{
	return std::make_shared<D3D11StagingTexture>(this, width, height, format, pixels);
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

void D3D11Context::CopyTexture(const std::shared_ptr<GPUTexture2D> &dest, const std::shared_ptr<GPUStagingTexture> &source)
{
	auto destimpl = std::static_pointer_cast<D3D11Texture2D>(dest);
	auto sourceimpl = std::static_pointer_cast<D3D11StagingTexture>(source);
	DeviceContext->CopyResource(destimpl->Handle(), sourceimpl->Handle());
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
	if (fb)
	{
		auto d3dfb = static_cast<D3D11FrameBuffer*>(fb.get());

		std::vector<ID3D11RenderTargetView*> views(d3dfb->RenderTargetViews.size());
		for (size_t i = 0; i < views.size(); i++)
			views[i] = d3dfb->RenderTargetViews[i].Get();
		ID3D11DepthStencilView *depthStencilView = d3dfb->DepthStencilView ? d3dfb->DepthStencilView.Get() : nullptr;
		DeviceContext->OMSetRenderTargets((UINT)views.size(), views.data(), depthStencilView);
	}
	else
	{
		ComPtr<ID3D11Texture2D> backbuffer;
		HRESULT result = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backbuffer.OutputVariable());
		if (SUCCEEDED(result))
		{
			D3D11_TEXTURE2D_DESC texturedesc;
			backbuffer->GetDesc(&texturedesc);

			D3D11_RENDER_TARGET_VIEW_DESC desc;
			desc.Format = texturedesc.Format;
			desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = 0;

			ComPtr<ID3D11RenderTargetView> view;
			result = Device->CreateRenderTargetView(backbuffer, &desc, view.OutputVariable());
			if (SUCCEEDED(result))
			{
				ID3D11RenderTargetView *backbuffer_rtvs[] = { view };
				DeviceContext->OMSetRenderTargets(1, backbuffer_rtvs, nullptr);
			}
		}
	}
}

void D3D11Context::SetViewport(int x, int y, int width, int height)
{
	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = x;
	viewport.TopLeftY = y;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	DeviceContext->RSSetViewports(1, &viewport);
}

void D3D11Context::SetProgram(const std::shared_ptr<GPUProgram> &program)
{
	if (program)
	{
		mCurrentProgram = std::static_pointer_cast<D3D11Program>(program);

		if (mCurrentProgram->ShaderHandle[GPUShaderType::Vertex])
			DeviceContext->VSSetShader(mCurrentProgram->ShaderHandle[GPUShaderType::Vertex]->HandleVertex(), nullptr, 0);
		if (mCurrentProgram->ShaderHandle[GPUShaderType::Fragment])
			DeviceContext->PSSetShader(mCurrentProgram->ShaderHandle[GPUShaderType::Fragment]->HandlePixel(), nullptr, 0);

		if (mCurrentVertexArray)
			DeviceContext->IASetInputLayout(mCurrentVertexArray->GetInputLayout(mCurrentProgram.get()));
	}
	else
	{
		mCurrentProgram.reset();
		DeviceContext->VSSetShader(nullptr, nullptr, 0);
		DeviceContext->PSSetShader(nullptr, nullptr, 0);
	}
}

void D3D11Context::SetSampler(int index, const std::shared_ptr<GPUSampler> &sampler)
{
	mBoundSamplers[index] = sampler;

	if (mCurrentProgram)
		mCurrentProgram->ApplySampler(index);
}

void D3D11Context::SetTexture(int index, const std::shared_ptr<GPUTexture> &texture)
{
	mBoundTextures[index] = texture;

	if (mCurrentProgram)
		mCurrentProgram->ApplyTexture(index);
}

void D3D11Context::SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer)
{
	mBoundUniformBuffers[index] = buffer;

	if (mCurrentProgram)
		mCurrentProgram->ApplyUniforms(index);
}

void D3D11Context::SetStorage(int index, const std::shared_ptr<GPUStorageBuffer> &storage)
{
	mBoundStorageBuffers[index] = storage;

	if (mCurrentProgram)
		mCurrentProgram->ApplyStorage(index);
}

void D3D11Context::SetClipDistance(int index, bool enable)
{
}

void D3D11Context::SetLineSmooth(bool enable)
{
}

void D3D11Context::SetScissor(int x, int y, int width, int height)
{
}

void D3D11Context::ClearScissorBox(float r, float g, float b, float a)
{
}

void D3D11Context::ResetScissor()
{
}

void D3D11Context::SetBlend(int op, int srcblend, int destblend)
{
}

void D3D11Context::ResetBlend()
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
	if (vertexarray)
	{
		mCurrentVertexArray = std::static_pointer_cast<D3D11VertexArray>(vertexarray);

		for (const auto &attr : mCurrentVertexArray->Attributes())
		{
			ID3D11Buffer *buffers[] = { static_cast<D3D11VertexBuffer*>(attr.second.Buffer.get())->Handle() };
			UINT strides[] = { (UINT)attr.second.Stride };
			UINT offsets[] = { 0 }; // attr.second.Offset is specified in the input layout

			if (strides[0] == 0) // Stride is not optional in D3D
			{
				switch (attr.second.Type)
				{
				default:
				case GPUVertexAttributeType::Int8:
				case GPUVertexAttributeType::Uint8:
					strides[0] = attr.second.Size;
					break;
				case GPUVertexAttributeType::Int16:
				case GPUVertexAttributeType::Uint16:
				case GPUVertexAttributeType::HalfFloat:
					strides[0] = attr.second.Size * 2;
					break;
				case GPUVertexAttributeType::Int32:
				case GPUVertexAttributeType::Uint32:
				case GPUVertexAttributeType::Float:
					strides[0] = attr.second.Size * 4;
					break;
				}
			}

			DeviceContext->IASetVertexBuffers(attr.second.Index, 1, buffers, strides, offsets);
		}

		if (mCurrentProgram)
			DeviceContext->IASetInputLayout(mCurrentVertexArray->GetInputLayout(mCurrentProgram.get()));
	}
	else
	{
		if (mCurrentVertexArray)
		{
			for (const auto &attr : mCurrentVertexArray->Attributes())
			{
				ID3D11Buffer *buffers[] = { nullptr };
				UINT strides[] = { 0 };
				UINT offsets[] = { 0 };
				DeviceContext->IASetVertexBuffers(attr.second.Index, 1, buffers, strides, offsets);
			}
			mCurrentVertexArray.reset();
		}
		DeviceContext->IASetInputLayout(nullptr);
	}
}

void D3D11Context::SetIndexBuffer(const std::shared_ptr<GPUIndexBuffer> &indexbuffer, GPUIndexFormat format)
{
	mIndexFormat = format;
	if (indexbuffer)
	{
		DXGI_FORMAT d3dformat = (format == GPUIndexFormat::Uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		D3D11IndexBuffer *d3dindexbuffer = static_cast<D3D11IndexBuffer*>(indexbuffer.get());
		DeviceContext->IASetIndexBuffer(d3dindexbuffer->Handle(), d3dformat, 0);
	}
	else
	{
		DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	}
}

void D3D11Context::SetDrawMode(GPUDrawMode mode)
{
	D3D11_PRIMITIVE_TOPOLOGY topology;
	switch (mode)
	{
	default:
	case GPUDrawMode::Points: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
	case GPUDrawMode::LineStrip: topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
	case GPUDrawMode::LineLoop: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
	case GPUDrawMode::Lines: topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
	case GPUDrawMode::TriangleStrip: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
	case GPUDrawMode::Triangles: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
	}
	DeviceContext->IASetPrimitiveTopology(topology);
}

void D3D11Context::Draw(GPUDrawMode mode, int vertexStart, int vertexCount)
{
	SetDrawMode(mode);
	DeviceContext->Draw(vertexCount, vertexStart);
}

void D3D11Context::DrawIndexed(GPUDrawMode mode, int indexStart, int indexCount)
{
	SetDrawMode(mode);
	DeviceContext->DrawIndexed(indexCount, indexStart, 0);
}

void D3D11Context::DrawInstanced(GPUDrawMode mode, int vertexStart, int vertexCount, int instanceCount)
{
	SetDrawMode(mode);
	DeviceContext->DrawInstanced(vertexCount, instanceCount, vertexStart, 0);
}

void D3D11Context::DrawIndexedInstanced(GPUDrawMode mode, int indexStart, int indexCount, int instanceCount)
{
	SetDrawMode(mode);
	DeviceContext->DrawIndexedInstanced(indexCount, instanceCount, indexStart, 0, 0);
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

D3D11IndexBuffer::D3D11IndexBuffer(D3D11Context *context, const void *data, int size) : mContext(context)
{
	mSize = size;

	D3D11_SUBRESOURCE_DATA resource_data;
	resource_data.pSysMem = data;
	resource_data.SysMemPitch = 0;
	resource_data.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	HRESULT result = context->Device->CreateBuffer(&desc, data ? &resource_data : nullptr, mHandle.OutputVariable());
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
	HRESULT result = mContext->DeviceContext->Map(mHandle, 0, D3D11_MAP_WRITE_DISCARD, 0, &mMappedSubresource);
	if (FAILED(result))
		I_FatalError("ID3D11Device.MapWriteOnly(index) failed");
	return mMappedSubresource.pData;
}

void D3D11IndexBuffer::Unmap()
{
	mContext->DeviceContext->Unmap(mHandle, 0);
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

D3D11Shader::D3D11Shader(D3D11Context *context, GPUShaderType type, const char *name, const std::string &source) : Type(type)
{
	std::string entryPoint = "main";
	std::string shaderModel = GetShaderModel(context->FeatureLevel, type);

	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> log;
	HRESULT result = D3DCompile(
		source.data(),
		source.length(),
		nullptr,
		nullptr,
		nullptr,
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

D3D11Shader::D3D11Shader(D3D11Context *context, GPUShaderType type, const char *name, const void *bytecode, int bytecodeSize) : Type(type)
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
	Sampler.Locations.clear();
	Texture.Locations.clear();
	Image.Locations.clear();
	UniformBuffer.Locations.clear();
	StorageBufferSrv.Locations.clear();
	StorageBufferUav.Locations.clear();

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
			UniformBuffer.Locations[binding.Name] = binding.BindPoint;
			break;

		case D3D_SIT_TBUFFER: // texture buffer
			// What does this map to in OpenGL? glTexBuffer?
			break;

		case D3D_SIT_TEXTURE: // texture
			Texture.Locations[binding.Name] = binding.BindPoint;
			break;

		case D3D_SIT_SAMPLER: // sampler
			Sampler.Locations[binding.Name] = binding.BindPoint;
			break;

		case D3D_SIT_UAV_RWTYPED: // read-and-write buffer
			Image.Locations[binding.Name] = binding.BindPoint;
			break;

		case D3D_SIT_UAV_RWSTRUCTURED: // read-and-write structured buffer
		case D3D_SIT_UAV_RWBYTEADDRESS: // read-and-write byte-address buffer
		case D3D_SIT_UAV_APPEND_STRUCTURED: // append-structured buffer
		case D3D_SIT_UAV_CONSUME_STRUCTURED: // consume-structured buffer
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: // read-and-write structured buffer that uses the built-in counter to append or consume
			StorageBufferUav.Locations[binding.Name] = binding.BindPoint;
			break;

		case D3D_SIT_STRUCTURED: // structured buffer
		case D3D_SIT_BYTEADDRESS: // byte-address buffer
			StorageBufferSrv.Locations[binding.Name] = binding.BindPoint;
			break;
		}
	}
}

void D3D11Shader::ApplyAll(D3D11Context *context)
{
	for (int index : Sampler.Bindings)
		ApplySampler(context, index);
	for (int index : Texture.Bindings)
		ApplyTexture(context, index);
	for (int index : UniformBuffer.Bindings)
		ApplyUniforms(context, index);
	for (int index : StorageBufferSrv.Bindings)
		ApplyStorage(context, index);
}

void D3D11Shader::ApplySampler(D3D11Context *context, int index)
{
	int location = Sampler.BindingsToLocations[index];
	if (location == -1)
		return;
	D3D11Sampler *d3dsampler = context->GetSampler(index);
	ID3D11SamplerState *samplers[] = { d3dsampler ? d3dsampler->Handle() : nullptr };
	switch (Type)
	{
	default: break;
	case GPUShaderType::Vertex: context->DeviceContext->VSSetSamplers(location, 1, samplers);
	case GPUShaderType::Fragment: context->DeviceContext->PSSetSamplers(location, 1, samplers);
	}
}

void D3D11Shader::ApplyTexture(D3D11Context *context, int index)
{
	int location = Texture.BindingsToLocations[index];
	if (location == -1)
		return;
	D3D11Texture2D *d3dtex = context->GetTexture2D(index);
	ID3D11ShaderResourceView *srvs[] = { d3dtex ? d3dtex->SRVHandle() : nullptr };
	switch (Type)
	{
	default: break;
	case GPUShaderType::Vertex: context->DeviceContext->VSSetShaderResources(location, 1, srvs);
	case GPUShaderType::Fragment: context->DeviceContext->PSSetShaderResources(location, 1, srvs);
	}
}

void D3D11Shader::ApplyUniforms(D3D11Context *context, int index)
{
	int location = UniformBuffer.BindingsToLocations[index];
	if (location == -1)
		return;
	D3D11UniformBuffer *d3dbuffer = context->GetUniforms(index);
	ID3D11Buffer *buffers[] = { d3dbuffer ? d3dbuffer->Handle() : nullptr };
	switch (Type)
	{
	default: break;
	case GPUShaderType::Vertex: context->DeviceContext->VSSetConstantBuffers(location, 1, buffers);
	case GPUShaderType::Fragment: context->DeviceContext->PSSetConstantBuffers(location, 1, buffers);
	}
}

void D3D11Shader::ApplyStorage(D3D11Context *context, int index)
{
	int location = StorageBufferSrv.BindingsToLocations[index];
	if (location == -1)
		return;
	D3D11StorageBuffer *d3dstorage = context->GetStorage(index);
	ID3D11ShaderResourceView *srvs[] = { d3dstorage ? d3dstorage->SRVHandle() : nullptr };
	switch (Type)
	{
	default: break;
	case GPUShaderType::Vertex: context->DeviceContext->VSSetShaderResources(location, 1, srvs);
	case GPUShaderType::Fragment: context->DeviceContext->PSSetShaderResources(location, 1, srvs);
	}
}

void D3D11Shader::ClearAll(D3D11Context *context)
{
	ID3D11SamplerState *samplers[] = { nullptr };
	ID3D11ShaderResourceView *srvs[] = { nullptr };
	ID3D11Buffer *buffers[] = { nullptr };

	switch (Type)
	{
	default:
		break;
	case GPUShaderType::Vertex:
		for (int index : Sampler.Bindings)
			context->DeviceContext->VSSetSamplers(Sampler.BindingsToLocations[index], 1, samplers);
		for (int index : Texture.Bindings)
			context->DeviceContext->VSSetShaderResources(Texture.BindingsToLocations[index], 1, srvs);
		for (int index : UniformBuffer.Bindings)
			context->DeviceContext->VSSetConstantBuffers(UniformBuffer.BindingsToLocations[index], 1, buffers);
		for (int index : StorageBufferSrv.Bindings)
			context->DeviceContext->VSSetShaderResources(StorageBufferSrv.BindingsToLocations[index], 1, srvs);
		break;
	case GPUShaderType::Fragment:
		for (int index : Sampler.Bindings)
			context->DeviceContext->PSSetSamplers(Sampler.BindingsToLocations[index], 1, samplers);
		for (int index : Texture.Bindings)
			context->DeviceContext->PSSetShaderResources(Texture.BindingsToLocations[index], 1, srvs);
		for (int index : UniformBuffer.Bindings)
			context->DeviceContext->PSSetConstantBuffers(UniformBuffer.BindingsToLocations[index], 1, buffers);
		for (int index : StorageBufferSrv.Bindings)
			context->DeviceContext->PSSetShaderResources(StorageBufferSrv.BindingsToLocations[index], 1, srvs);
		break;
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
	ShaderHandle[type].reset(new D3D11Shader(mContext, type, name, PrefixCode() + code));
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

void D3D11Program::SetUniformBlockLocation(const std::string &name, int index)
{
	for (const auto &it : ShaderHandle)
	{
		const std::unique_ptr<D3D11Shader> &shader = it.second;
		shader->UniformBuffer.SetBinding(name, index);
	}
}

void D3D11Program::SetTextureLocation(const std::string &name, int index)
{
	for (const auto &it : ShaderHandle)
	{
		const std::unique_ptr<D3D11Shader> &shader = it.second;
		shader->Texture.SetBinding(name, index);
	}
}

void D3D11Program::SetTextureLocation(const std::string &texturename, const std::string &samplername, int index)
{
	for (const auto &it : ShaderHandle)
	{
		const std::unique_ptr<D3D11Shader> &shader = it.second;
		shader->Texture.SetBinding(texturename, index);
		shader->Sampler.SetBinding(samplername, index);
	}
}

void D3D11Program::Link(const std::string &name)
{
}

void D3D11Program::ApplyAll()
{
	for (const auto &it : ShaderHandle)
		it.second->ApplyAll(mContext);
}

void D3D11Program::ApplySampler(int index)
{
	for (const auto &it : ShaderHandle)
		it.second->ApplySampler(mContext, index);
}

void D3D11Program::ApplyTexture(int index)
{
	for (const auto &it : ShaderHandle)
		it.second->ApplyTexture(mContext, index);
}

void D3D11Program::ApplyUniforms(int index)
{
	for (const auto &it : ShaderHandle)
		it.second->ApplyUniforms(mContext, index);
}

void D3D11Program::ApplyStorage(int index)
{
	for (const auto &it : ShaderHandle)
		it.second->ApplyStorage(mContext, index);
}

void D3D11Program::ClearAll()
{
	for (const auto &it : ShaderHandle)
		it.second->ClearAll(mContext);
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

	result = context->Device->CreateShaderResourceView(mHandle, nullptr, mSRVHandle.OutputVariable());
	if (FAILED(result))
		I_FatalError("ID3D11Device.CreateShaderResourceView(storage) failed");
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

D3D11StagingTexture::D3D11StagingTexture(D3D11Context *context, int width, int height, GPUPixelFormat format, const void *pixels) : mContext(context)
{
	mContext = context;
	mWidth = width;
	mHeight = height;
	mFormat = format;

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.Format = D3D11Texture2D::ToD3DFormat(format);
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	desc.ArraySize = 1;
	desc.BindFlags = 0;

	D3D11_SUBRESOURCE_DATA initdata;
	initdata.pSysMem = pixels;
	initdata.SysMemPitch = width * D3D11Texture2D::GetBytesPerPixel(format);
	initdata.SysMemSlicePitch = height * initdata.SysMemPitch;

	HRESULT result = context->Device->CreateTexture2D(&desc, pixels ? &initdata : nullptr, mHandle.OutputVariable());
	if (FAILED(result))
		I_FatalError("ID3D11Device.CreateTexture2D(staging) failed");
}

D3D11StagingTexture::~D3D11StagingTexture()
{
}

void D3D11StagingTexture::Upload(int x, int y, int width, int height, const void *pixels)
{
	D3D11_TEXTURE2D_DESC desc;
	mHandle->GetDesc(&desc);

	if (x < 0 || x + width > desc.Width || y < 0 || y + height > desc.Height)
		I_FatalError("D3D11Texture2D.Upload: out of bounds!");

	D3D11_BOX box;
	box.left = x;
	box.top = y;
	box.right = x + width;
	box.bottom = y + height;
	box.front = 0;
	box.back = 1;

	int rowPitch = width * D3D11Texture2D::GetBytesPerPixel(mFormat);
	int slicePitch = rowPitch * height;

	mContext->DeviceContext->UpdateSubresource(mHandle, 0, &box, pixels, rowPitch, slicePitch);
}

void *D3D11StagingTexture::Map()
{
	HRESULT result = mContext->DeviceContext->Map(mHandle, 0, D3D11_MAP_READ_WRITE, 0, &mMappedSubresource);
	if (FAILED(result))
		I_FatalError("ID3D11Device.Map(staging texture) failed");
	return mMappedSubresource.pData;
}

void D3D11StagingTexture::Unmap()
{
	mContext->DeviceContext->Unmap(mHandle, 0);
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

	if (!!(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
	{
		result = context->Device->CreateShaderResourceView(mHandle, nullptr, mSRVHandle.OutputVariable());
		if (FAILED(result))
			I_FatalError("ID3D11Device.CreateShaderResourceView(Texture2D) failed");
	}
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
	case GPUPixelFormat::BGRA8: return DXGI_FORMAT_B8G8R8A8_UNORM;
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
	case GPUPixelFormat::BGRA8:
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
	HRESULT result = context->Device->CreateBuffer(&desc, data ? &resource_data : nullptr, mHandle.OutputVariable());
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

/*void *D3D11UniformBuffer::MapWriteOnly()
{
	return nullptr;
}

void D3D11UniformBuffer::Unmap()
{
}*/

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
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	HRESULT result = context->Device->CreateBuffer(&desc, data ? &resource_data : nullptr, mHandle.OutputVariable());
	if (FAILED(result))
		I_FatalError("ID3D11Device.CreateBuffer(vertex) failed");
}

D3D11VertexBuffer::~D3D11VertexBuffer()
{
}

void D3D11VertexBuffer::Upload(const void *data, int size)
{
	int offset = 0;
	if ((offset < 0) || (size < 0) || ((size + offset) > mSize))
		I_FatalError("Vertex buffer upload failed, invalid size");

	D3D11_BOX box;
	box.left = offset;
	box.right = offset + size;
	box.top = 0;
	box.bottom = 1;
	box.front = 0;
	box.back = 1;
	mContext->DeviceContext->UpdateSubresource(mHandle, 0, &box, data, 0, 0);
}

void *D3D11VertexBuffer::MapWriteOnly()
{
	HRESULT result = mContext->DeviceContext->Map(mHandle, 0, D3D11_MAP_WRITE_DISCARD, 0, &mMappedSubresource);
	if (FAILED(result))
		I_FatalError("ID3D11Device.MapWriteOnly(vertex) failed");
	return mMappedSubresource.pData;
}

void D3D11VertexBuffer::Unmap()
{
	mContext->DeviceContext->Unmap(mHandle, 0);
}
