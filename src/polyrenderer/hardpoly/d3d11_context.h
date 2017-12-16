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

#include "gpu_context.h"

#undef APIENTRY
#include <d3d11.h>
#include <D3Dcompiler.h>

template <typename Type>
class ComPtr
{
public:
	ComPtr() : ptr(0) { }
	explicit ComPtr(Type *ptr) : ptr(ptr) { }
	ComPtr(const ComPtr &copy) : ptr(copy.ptr) { if (ptr) ptr->AddRef(); }
	~ComPtr() { Clear(); }
	ComPtr &operator =(const ComPtr &copy)
	{
		if (this == &copy)
			return *this;
		if (copy.ptr)
			copy.ptr->AddRef();
		if (ptr)
			ptr->Release();
		ptr = copy.ptr;
		return *this;
	}

	template<typename That>
	explicit ComPtr(const ComPtr<That> &that)
		: ptr(static_cast<Type*>(that.ptr))
	{
		if (ptr)
			ptr->AddRef();
	}

	bool operator ==(const ComPtr &other) const { return ptr == other.ptr; }
	bool operator !=(const ComPtr &other) const { return ptr != other.ptr; }
	bool operator <(const ComPtr &other) const { return ptr < other.ptr; }
	bool operator <=(const ComPtr &other) const { return ptr <= other.ptr; }
	bool operator >(const ComPtr &other) const { return ptr > other.ptr; }
	bool operator >=(const ComPtr &other) const { return ptr >= other.ptr; }

	// const does not exist in COM, so we drop the const qualifier on returned objects to avoid needing mutable variables elsewhere

	Type * const operator ->() const { return const_cast<Type*>(ptr); }
	Type *operator ->() { return ptr; }
	operator Type *() const { return const_cast<Type*>(ptr); }
	operator bool() const { return ptr != 0; }

	bool IsNull() const { return ptr == 0; }
	void Clear() { if (ptr) ptr->Release(); ptr = 0; }
	Type *Get() const { return const_cast<Type*>(ptr); }
	Type **OutputVariable() { Clear(); return &ptr; }

private:
	Type *ptr;
};

class D3D11Context;

class D3D11StagingTexture : public GPUStagingTexture
{
public:
	D3D11StagingTexture(D3D11Context *context, int width, int height, GPUPixelFormat format, const void *pixels = nullptr);
	~D3D11StagingTexture();

	void Upload(int x, int y, int width, int height, const void *pixels);

	int Width() const override { return mWidth; }
	int Height() const override { return mHeight; }
	GPUPixelFormat Format() const override { return mFormat; }

	ID3D11Texture2D *Handle() const { return mHandle.Get(); }

	void *Map() override;
	void Unmap() override;
	int GetMappedRowPitch() override { return mMappedSubresource.RowPitch; }

private:
	D3D11StagingTexture(const D3D11StagingTexture &) = delete;
	D3D11StagingTexture &operator =(const D3D11StagingTexture &) = delete;

	D3D11Context *mContext = nullptr;
	int mWidth = 0;
	int mHeight = 0;
	GPUPixelFormat mFormat;
	ComPtr<ID3D11Texture2D> mHandle;
	D3D11_MAPPED_SUBRESOURCE mMappedSubresource;
};

class D3D11Texture2D : public GPUTexture2D
{
public:
	D3D11Texture2D(D3D11Context *context, int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels = nullptr);
	~D3D11Texture2D();

	void Upload(int x, int y, int width, int height, int level, const void *pixels);

	int SampleCount() const override { return mSampleCount; }
	int Width() const override { return mWidth; }
	int Height() const override { return mHeight; }
	GPUPixelFormat Format() const override { return mFormat; }

	ID3D11Texture2D *Handle() const { return mHandle.Get(); }

	static int NumLevels(int width, int height);
	static DXGI_FORMAT ToD3DFormat(GPUPixelFormat format);
	static int GetBytesPerPixel(GPUPixelFormat format);
	static bool IsStencilOrDepthFormat(GPUPixelFormat format);

private:
	D3D11Texture2D(const D3D11Texture2D &) = delete;
	D3D11Texture2D &operator =(const D3D11Texture2D &) = delete;

	D3D11Context *mContext = nullptr;
	int mWidth = 0;
	int mHeight = 0;
	bool mMipmap = false;
	int mSampleCount = 0;
	GPUPixelFormat mFormat;
	ComPtr<ID3D11Texture2D> mHandle;
};

class D3D11FrameBuffer : public GPUFrameBuffer
{
public:
	D3D11FrameBuffer(D3D11Context *context, const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil);
	~D3D11FrameBuffer();

	std::vector<ComPtr<ID3D11RenderTargetView>> RenderTargetViews;
	ComPtr<ID3D11DepthStencilView> DepthStencilView;

private:
	D3D11FrameBuffer(const D3D11FrameBuffer &) = delete;
	D3D11FrameBuffer &operator =(const D3D11FrameBuffer &) = delete;
};

class D3D11IndexBuffer : public GPUIndexBuffer
{
public:
	D3D11IndexBuffer(D3D11Context *context, const void *data, int size);
	~D3D11IndexBuffer();

	void Upload(const void *data, int size) override;

	void *MapWriteOnly() override;
	void Unmap() override;

private:
	D3D11IndexBuffer(const D3D11IndexBuffer &) = delete;
	D3D11IndexBuffer &operator =(const D3D11IndexBuffer &) = delete;

	D3D11Context *mContext;
	int mSize;
	ComPtr<ID3D11Buffer> mHandle;
	D3D11_MAPPED_SUBRESOURCE mMappedSubresource;
};

class D3D11Shader
{
public:
	D3D11Shader(D3D11Context *context, GPUShaderType type, const char *name, const std::string &source);
	D3D11Shader(D3D11Context *context, GPUShaderType type, const char *name, const void *bytecode, int bytecodeSize);

	ID3D11VertexShader *HandleVertex() { return static_cast<ID3D11VertexShader*>(mHandle.Get()); }
	ID3D11PixelShader *HandlePixel() { return static_cast<ID3D11PixelShader*>(mHandle.Get()); }
	ID3D11GeometryShader *HandleGeometry() { return static_cast<ID3D11GeometryShader*>(mHandle.Get()); }
	ID3D11DomainShader *HandleDomain() { return static_cast<ID3D11DomainShader*>(mHandle.Get()); }
	ID3D11HullShader *HandleHull() { return static_cast<ID3D11HullShader*>(mHandle.Get()); }
	ID3D11ComputeShader *HandleCompute() { return static_cast<ID3D11ComputeShader*>(mHandle.Get()); }

	std::vector<uint8_t> Bytecode;

	std::map<std::string, int> SamplerLocations;
	std::map<std::string, int> TextureLocations;
	std::map<std::string, int> ImageLocations;
	std::map<std::string, int> UniformBufferLocations;
	std::map<std::string, int> StorageBufferSrvLocations;
	std::map<std::string, int> StorageBufferUavLocations;

private:
	D3D11Shader(const D3D11Shader &) = delete;
	D3D11Shader &operator =(const D3D11Shader &) = delete;

	void CreateShader(D3D11Context *context, GPUShaderType type);
	void FindLocations();

	ComPtr<ID3D11DeviceChild> mHandle;
};

class D3D11Program : public GPUProgram
{
public:
	D3D11Program(D3D11Context *context);
	~D3D11Program();

	void Compile(GPUShaderType type, const char *name, const std::string &code) override;
	void SetAttribLocation(const std::string &name, int index) override;
	void SetFragOutput(const std::string &name, int index) override;
	void Link(const std::string &name) override;
	void SetUniformBlock(const std::string &name, int index) override;
	int GetUniformLocation(const char *name) override;

	struct AttributeBinding
	{
		std::string SemanticName;
		int SemanticIndex = 0;
	};

	std::map<int, AttributeBinding> AttributeBindings;
	std::map<GPUShaderType, std::unique_ptr<D3D11Shader>> ShaderHandle;

private:
	D3D11Program(const D3D11Program &) = delete;
	D3D11Program &operator =(const D3D11Program &) = delete;

	std::string PrefixCode() const;

	D3D11Context *mContext;
};

class D3D11Sampler : public GPUSampler
{
public:
	D3D11Sampler(D3D11Context *context, GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV);
	~D3D11Sampler();

private:
	static D3D11_TEXTURE_ADDRESS_MODE ToD3DWrap(GPUWrapMode mode);
	static D3D11_FILTER ToD3DFilter(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap);

	D3D11Sampler(const D3D11Sampler &) = delete;
	D3D11Sampler &operator =(const D3D11Sampler &) = delete;

	ComPtr<ID3D11SamplerState> mHandle;
	GPUSampleMode mMinfilter = GPUSampleMode::Nearest;
	GPUSampleMode mMagfilter = GPUSampleMode::Nearest;
	GPUMipmapMode mMipmap = GPUMipmapMode::None;
	GPUWrapMode mWrapU = GPUWrapMode::Repeat;
	GPUWrapMode mWrapV = GPUWrapMode::Repeat;
};

class D3D11StorageBuffer : public GPUStorageBuffer
{
public:
	D3D11StorageBuffer(D3D11Context *context, const void *data, int size);
	~D3D11StorageBuffer();

	void Upload(const void *data, int size) override;

private:
	D3D11StorageBuffer(const D3D11StorageBuffer &) = delete;
	D3D11StorageBuffer &operator =(const D3D11StorageBuffer &) = delete;

	D3D11Context *mContext;
	int mSize;
	ComPtr<ID3D11Buffer> mHandle;
};

class D3D11UniformBuffer : public GPUUniformBuffer
{
public:
	D3D11UniformBuffer(D3D11Context *context, const void *data, int size);
	~D3D11UniformBuffer();

	void Upload(const void *data, int size) override;

	//void *MapWriteOnly() override;
	//void Unmap() override;

private:
	D3D11UniformBuffer(const D3D11UniformBuffer &) = delete;
	D3D11UniformBuffer &operator =(const D3D11UniformBuffer &) = delete;

	D3D11Context *mContext;
	int mSize;
	ComPtr<ID3D11Buffer> mHandle;
};

class D3D11VertexArray : public GPUVertexArray
{
public:
	D3D11VertexArray(D3D11Context *context, const std::vector<GPUVertexAttributeDesc> &attributes);
	~D3D11VertexArray();

	ID3D11InputLayout *GetInputLayout(D3D11Program *program);

private:
	D3D11VertexArray(const D3D11VertexArray &) = delete;
	D3D11VertexArray &operator =(const D3D11VertexArray &) = delete;

	ComPtr<ID3D11InputLayout> CreateInputLayout(D3D11Program *program);
	static DXGI_FORMAT ToD3DFormat(const GPUVertexAttributeDesc &data);

	D3D11Context *mContext;
	std::map<int, GPUVertexAttributeDesc> mAttributes;
	std::map<void *, ComPtr<ID3D11InputLayout>> mInputLayouts;
};

class D3D11VertexBuffer : public GPUVertexBuffer
{
public:
	D3D11VertexBuffer(D3D11Context *context, const void *data, int size);
	~D3D11VertexBuffer();

	void Upload(const void *data, int size) override;

	void *MapWriteOnly() override;
	void Unmap() override;

private:
	D3D11VertexBuffer(const D3D11VertexBuffer &) = delete;
	D3D11VertexBuffer &operator =(const D3D11VertexBuffer &) = delete;

	D3D11Context *mContext;
	int mSize;
	ComPtr<ID3D11Buffer> mHandle;
	D3D11_MAPPED_SUBRESOURCE mMappedSubresource;
};

class D3D11Context : public GPUContext
{
public:
	D3D11Context();
	~D3D11Context();

	std::shared_ptr<GPUStagingTexture> CreateStagingTexture(int width, int height, GPUPixelFormat format, const void *pixels = nullptr) override;
	std::shared_ptr<GPUTexture2D> CreateTexture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels = nullptr) override;
	std::shared_ptr<GPUFrameBuffer> CreateFrameBuffer(const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil) override;
	std::shared_ptr<GPUIndexBuffer> CreateIndexBuffer(const void *data, int size) override;
	std::shared_ptr<GPUProgram> CreateProgram() override;
	std::shared_ptr<GPUSampler> CreateSampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV) override;
	std::shared_ptr<GPUStorageBuffer> CreateStorageBuffer(const void *data, int size) override;
	std::shared_ptr<GPUUniformBuffer> CreateUniformBuffer(const void *data, int size) override;
	std::shared_ptr<GPUVertexArray> CreateVertexArray(const std::vector<GPUVertexAttributeDesc> &attributes) override;
	std::shared_ptr<GPUVertexBuffer> CreateVertexBuffer(const void *data, int size) override;

	void CopyTexture(const std::shared_ptr<GPUTexture2D> &dest, const std::shared_ptr<GPUStagingTexture> &source) override;

	void Begin() override;
	void End() override;

	void ClearError() override;
	void CheckError() override;

	void SetFrameBuffer(const std::shared_ptr<GPUFrameBuffer> &fb) override;
	void SetViewport(int x, int y, int width, int height) override;

	void SetProgram(const std::shared_ptr<GPUProgram> &program) override;
	void SetUniform1i(int location, int value) override;

	void SetSampler(int index, const std::shared_ptr<GPUSampler> &sampler) override;
	void SetTexture(int index, const std::shared_ptr<GPUTexture> &texture) override;
	void SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer) override;
	void SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer, ptrdiff_t offset, size_t size) override;
	void SetStorage(int index, const std::shared_ptr<GPUStorageBuffer> &storage) override;

	void SetClipDistance(int index, bool enable) override;

	void SetLineSmooth(bool enable) override;
	void SetScissor(int x, int y, int width, int height) override;
	void ClearScissorBox(float r, float g, float b, float a) override;
	void ResetScissor() override;

	void SetBlend(int op, int srcblend, int destblend) override;
	void ResetBlend() override;

	void SetOpaqueBlend(int srcalpha, int destalpha) override;
	void SetMaskedBlend(int srcalpha, int destalpha) override;
	void SetAlphaBlendFunc(int srcalpha, int destalpha) override;
	void SetAddClampBlend(int srcalpha, int destalpha) override;
	void SetSubClampBlend(int srcalpha, int destalpha) override;
	void SetRevSubClampBlend(int srcalpha, int destalpha) override;
	void SetAddSrcColorBlend(int srcalpha, int destalpha) override;
	void SetShadedBlend(int srcalpha, int destalpha) override;
	void SetAddClampShadedBlend(int srcalpha, int destalpha) override;

	void SetVertexArray(const std::shared_ptr<GPUVertexArray> &vertexarray) override;
	void SetIndexBuffer(const std::shared_ptr<GPUIndexBuffer> &indexbuffer, GPUIndexFormat format = GPUIndexFormat::Uint16) override;

	void Draw(GPUDrawMode mode, int vertexStart, int vertexCount) override;
	void DrawIndexed(GPUDrawMode mode, int indexStart, int indexCount) override;
	void DrawInstanced(GPUDrawMode mode, int vertexStart, int vertexCount, int instanceCount) override;
	void DrawIndexedInstanced(GPUDrawMode mode, int indexStart, int indexCount, int instanceCount) override;

	void ClearColorBuffer(int index, float r, float g, float b, float a) override;
	void ClearDepthBuffer(float depth) override;
	void ClearStencilBuffer(int stencil) override;
	void ClearDepthStencilBuffer(float depth, int stencil) override;

	ComPtr<ID3D11Device> Device;
	ComPtr<ID3D11DeviceContext> DeviceContext;
	ComPtr<IDXGISwapChain> SwapChain;
	D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL();

private:
	D3D11Context(const D3D11Context &) = delete;
	D3D11Context &operator =(const D3D11Context &) = delete;

	GPUIndexFormat mIndexFormat = GPUIndexFormat::Uint16;
};
