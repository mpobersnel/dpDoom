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

class D3D11Texture2D : public GPUTexture2D
{
public:
	D3D11Texture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels = nullptr);
	~D3D11Texture2D();

	void Upload(int x, int y, int width, int height, int level, const void *pixels);

	int Handle() const override { return mHandle; }
	int SampleCount() const override { return mSampleCount; }
	int Width() const override { return mWidth; }
	int Height() const override { return mHeight; }

private:
	D3D11Texture2D(const D3D11Texture2D &) = delete;
	D3D11Texture2D &operator =(const D3D11Texture2D &) = delete;

	static int NumLevels(int width, int height);

	int mHandle = 0;
	int mWidth = 0;
	int mHeight = 0;
	bool mMipmap = false;
	int mSampleCount = 0;
	GPUPixelFormat mFormat;
};

class D3D11FrameBuffer : public GPUFrameBuffer
{
public:
	D3D11FrameBuffer(const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil);
	~D3D11FrameBuffer();

	int Handle() const override { return mHandle; }

private:
	D3D11FrameBuffer(const D3D11FrameBuffer &) = delete;
	D3D11FrameBuffer &operator =(const D3D11FrameBuffer &) = delete;

	int mHandle = 0;
};

class D3D11IndexBuffer : public GPUIndexBuffer
{
public:
	D3D11IndexBuffer(const void *data, int size);
	~D3D11IndexBuffer();

	int Handle() const override { return mHandle; }

	void Upload(const void *data, int size) override;

	void *MapWriteOnly() override;
	void Unmap() override;

private:
	D3D11IndexBuffer(const D3D11IndexBuffer &) = delete;
	D3D11IndexBuffer &operator =(const D3D11IndexBuffer &) = delete;

	int mHandle = 0;
};

class D3D11Program : public GPUProgram
{
public:
	D3D11Program();
	~D3D11Program();

	int Handle() const override { return mHandle; }

	void Compile(GPUShaderType type, const char *name, const std::string &code) override;
	void SetAttribLocation(const std::string &name, int index) override;
	void SetFragOutput(const std::string &name, int index) override;
	void Link(const std::string &name) override;
	void SetUniformBlock(const std::string &name, int index) override;

private:
	D3D11Program(const D3D11Program &) = delete;
	D3D11Program &operator =(const D3D11Program &) = delete;

	std::string PrefixCode() const;

	int mHandle = 0;
	std::map<std::string, std::string> mDefines;
};

class D3D11Sampler : public GPUSampler
{
public:
	D3D11Sampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV);
	~D3D11Sampler();

	int Handle() const override { return mHandle; }

private:
	D3D11Sampler(const D3D11Sampler &) = delete;
	D3D11Sampler &operator =(const D3D11Sampler &) = delete;

	int mHandle = 0;
	GPUSampleMode mMinfilter = GPUSampleMode::Nearest;
	GPUSampleMode mMagfilter = GPUSampleMode::Nearest;
	GPUMipmapMode mMipmap = GPUMipmapMode::None;
	GPUWrapMode mWrapU = GPUWrapMode::Repeat;
	GPUWrapMode mWrapV = GPUWrapMode::Repeat;
};

class D3D11StorageBuffer : public GPUStorageBuffer
{
public:
	D3D11StorageBuffer(const void *data, int size);
	~D3D11StorageBuffer();

	void Upload(const void *data, int size) override;

	int Handle() const override { return mHandle; }

private:
	D3D11StorageBuffer(const D3D11StorageBuffer &) = delete;
	D3D11StorageBuffer &operator =(const D3D11StorageBuffer &) = delete;

	int mHandle = 0;
};

class D3D11UniformBuffer : public GPUUniformBuffer
{
public:
	D3D11UniformBuffer(const void *data, int size);
	~D3D11UniformBuffer();

	void Upload(const void *data, int size) override;

	void *MapWriteOnly() override;
	void Unmap() override;

	int Handle() const override { return mHandle; }

private:
	D3D11UniformBuffer(const D3D11UniformBuffer &) = delete;
	D3D11UniformBuffer &operator =(const D3D11UniformBuffer &) = delete;

	int mHandle = 0;
};

class D3D11VertexArray : public GPUVertexArray
{
public:
	D3D11VertexArray(const std::vector<GPUVertexAttributeDesc> &attributes);
	~D3D11VertexArray();

	int Handle() const override { return mHandle; }

private:
	D3D11VertexArray(const D3D11VertexArray &) = delete;
	D3D11VertexArray &operator =(const D3D11VertexArray &) = delete;

	int mHandle = 0;
	std::vector<GPUVertexAttributeDesc> mAttributes;
};

class D3D11VertexBuffer : public GPUVertexBuffer
{
public:
	D3D11VertexBuffer(const void *data, int size);
	~D3D11VertexBuffer();

	void Upload(const void *data, int size) override;

	void *MapWriteOnly() override;
	void Unmap() override;

	int Handle() const override { return mHandle; }

private:
	D3D11VertexBuffer(const D3D11VertexBuffer &) = delete;
	D3D11VertexBuffer &operator =(const D3D11VertexBuffer &) = delete;

	int mHandle = 0;
};

class D3D11Context : public GPUContext
{
public:
	D3D11Context();
	~D3D11Context();

	std::shared_ptr<GPUTexture2D> CreateTexture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels = nullptr) override;
	std::shared_ptr<GPUFrameBuffer> CreateFrameBuffer(const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil) override;
	std::shared_ptr<GPUIndexBuffer> CreateIndexBuffer(const void *data, int size) override;
	std::shared_ptr<GPUProgram> CreateProgram() override;
	std::shared_ptr<GPUSampler> CreateSampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV) override;
	std::shared_ptr<GPUStorageBuffer> CreateStorageBuffer(const void *data, int size) override;
	std::shared_ptr<GPUUniformBuffer> CreateUniformBuffer(const void *data, int size) override;
	std::shared_ptr<GPUVertexArray> CreateVertexArray(const std::vector<GPUVertexAttributeDesc> &attributes) override;
	std::shared_ptr<GPUVertexBuffer> CreateVertexBuffer(const void *data, int size) override;

	void Begin() override;
	void End() override;

	void ClearError() override;
	void CheckError() override;

	void SetFrameBuffer(const std::shared_ptr<GPUFrameBuffer> &fb) override;
	void SetViewport(int x, int y, int width, int height) override;

	void SetProgram(const std::shared_ptr<GPUProgram> &program) override;

	void SetSampler(int index, const std::shared_ptr<GPUSampler> &sampler) override;
	void SetTexture(int index, const std::shared_ptr<GPUTexture> &texture) override;
	void SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer) override;
	void SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer, ptrdiff_t offset, size_t size) override;
	void SetStorage(int index, const std::shared_ptr<GPUStorageBuffer> &storage) override;

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

private:
	D3D11Context(const D3D11Context &) = delete;
	D3D11Context &operator =(const D3D11Context &) = delete;

	GPUIndexFormat mIndexFormat = GPUIndexFormat::Uint16;
};
