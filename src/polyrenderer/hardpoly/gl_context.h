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

class GLStagingTexture : public GPUStagingTexture
{
public:
	GLStagingTexture(int width, int height, GPUPixelFormat format, const void *pixels = nullptr);
	~GLStagingTexture();

	//void Upload(int x, int y, int width, int height, const void *pixels) override;

	int Width() const override { return mWidth; }
	int Height() const override { return mHeight; }
	GPUPixelFormat Format() const override { return mFormat; }

	int Handle() const { return mHandle; }

	void *Map() override;
	void Unmap() override;
	int GetMappedRowPitch() override { return GetBytesPerPixel(mFormat) * mWidth; }

private:
	GLStagingTexture(const GLStagingTexture &) = delete;
	GLStagingTexture &operator =(const GLStagingTexture &) = delete;

	static int GetBytesPerPixel(GPUPixelFormat format);

	int mHandle = 0;
	int mWidth = 0;
	int mHeight = 0;
	GPUPixelFormat mFormat;
};

class GLTexture2D : public GPUTexture2D
{
public:
	GLTexture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels = nullptr);
	~GLTexture2D();

	void Upload(int x, int y, int width, int height, int level, const void *pixels);

	int Handle() const { return mHandle; }
	int SampleCount() const override { return mSampleCount; }
	int Width() const override { return mWidth; }
	int Height() const override { return mHeight; }
	GPUPixelFormat Format() const override { return mFormat; }

	static int NumLevels(int width, int height);
	static int ToInternalFormat(GPUPixelFormat format);
	static int ToUploadFormat(GPUPixelFormat format);
	static int ToUploadType(GPUPixelFormat format);

private:
	GLTexture2D(const GLTexture2D &) = delete;
	GLTexture2D &operator =(const GLTexture2D &) = delete;

	int mHandle = 0;
	int mWidth = 0;
	int mHeight = 0;
	bool mMipmap = false;
	int mSampleCount = 0;
	GPUPixelFormat mFormat;
};

class GLFrameBuffer : public GPUFrameBuffer
{
public:
	GLFrameBuffer(const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil);
	~GLFrameBuffer();

	int Handle() const { return mHandle; }
	int NumColorBuffers() const { return mNumColorBuffers; }

private:
	GLFrameBuffer(const GLFrameBuffer &) = delete;
	GLFrameBuffer &operator =(const GLFrameBuffer &) = delete;

	int mHandle = 0;
	int mNumColorBuffers = 0;
};

class GLIndexBuffer : public GPUIndexBuffer
{
public:
	GLIndexBuffer(const void *data, int size);
	~GLIndexBuffer();

	int Handle() const { return mHandle; }

	void Upload(const void *data, int size) override;

	void *MapWriteOnly() override;
	void Unmap() override;

private:
	GLIndexBuffer(const GLIndexBuffer &) = delete;
	GLIndexBuffer &operator =(const GLIndexBuffer &) = delete;

	int mHandle = 0;
};

class GLProgram : public GPUProgram
{
public:
	GLProgram();
	~GLProgram();

	int Handle() const { return mHandle; }

	void Compile(GPUShaderType type, const char *name, const std::string &code) override;
	void SetAttribLocation(const std::string &name, int index) override;
	void SetFragOutput(const std::string &name, int index) override;
	void SetUniformBlockLocation(const std::string &name, int index) override;
	void SetTextureLocation(const std::string &name, int index) override;
	void SetTextureLocation(const std::string &texturename, const std::string &samplername, int index) override;
	void Link(const std::string &name) override;

private:
	GLProgram(const GLProgram &) = delete;
	GLProgram &operator =(const GLProgram &) = delete;

	std::string PrefixCode() const;
	std::string GetShaderInfoLog(int handle) const;
	std::string GetProgramInfoLog() const;

	int mHandle = 0;
	std::map<int, int> mShaderHandle;
	std::map<std::string, int> mUniform1iBindings;
	std::map<std::string, int> mUniformBlockBindings;
};

class GLSampler : public GPUSampler
{
public:
	GLSampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV);
	~GLSampler();

	int Handle() const { return mHandle; }

private:
	GLSampler(const GLSampler &) = delete;
	GLSampler &operator =(const GLSampler &) = delete;

	int mHandle = 0;
	GPUSampleMode mMinfilter = GPUSampleMode::Nearest;
	GPUSampleMode mMagfilter = GPUSampleMode::Nearest;
	GPUMipmapMode mMipmap = GPUMipmapMode::None;
	GPUWrapMode mWrapU = GPUWrapMode::Repeat;
	GPUWrapMode mWrapV = GPUWrapMode::Repeat;
};

class GLStorageBuffer : public GPUStorageBuffer
{
public:
	GLStorageBuffer(const void *data, int size);
	~GLStorageBuffer();

	void Upload(const void *data, int size) override;

	int Handle() const { return mHandle; }

private:
	GLStorageBuffer(const GLStorageBuffer &) = delete;
	GLStorageBuffer &operator =(const GLStorageBuffer &) = delete;

	int mHandle = 0;
};

class GLUniformBuffer : public GPUUniformBuffer
{
public:
	GLUniformBuffer(const void *data, int size);
	~GLUniformBuffer();

	void Upload(const void *data, int size) override;

	//void *MapWriteOnly() override;
	//void Unmap() override;

	int Handle() const { return mHandle; }

private:
	GLUniformBuffer(const GLUniformBuffer &) = delete;
	GLUniformBuffer &operator =(const GLUniformBuffer &) = delete;

	int mHandle = 0;
};

class GLVertexArray : public GPUVertexArray
{
public:
	GLVertexArray(const std::vector<GPUVertexAttributeDesc> &attributes);
	~GLVertexArray();

	int Handle() const { return mHandle; }

private:
	GLVertexArray(const GLVertexArray &) = delete;
	GLVertexArray &operator =(const GLVertexArray &) = delete;

	static int FromType(GPUVertexAttributeType type);

	int mHandle = 0;
	std::vector<GPUVertexAttributeDesc> mAttributes;
};

class GLVertexBuffer : public GPUVertexBuffer
{
public:
	GLVertexBuffer(const void *data, int size);
	~GLVertexBuffer();

	//void Upload(const void *data, int size) override;

	void *MapWriteOnly() override;
	void Unmap() override;

	int Handle() const { return mHandle; }

private:
	GLVertexBuffer(const GLVertexBuffer &) = delete;
	GLVertexBuffer &operator =(const GLVertexBuffer &) = delete;

	int mHandle = 0;
};

class GLContext : public GPUContext
{
public:
	GLContext();
	~GLContext();
	
	ClipZRange GetClipZRange() const override { return ClipZRange::NegativePositiveW; }

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

	void SetSampler(int index, const std::shared_ptr<GPUSampler> &sampler) override;
	void SetTexture(int index, const std::shared_ptr<GPUTexture> &texture) override;
	void SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer) override;
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

private:
	GLContext(const GLContext &) = delete;
	GLContext &operator =(const GLContext &) = delete;

	static int FromDrawMode(GPUDrawMode mode);

	GPUIndexFormat mIndexFormat = GPUIndexFormat::Uint16;
	
	int oldDrawFramebufferBinding = 0;
	int oldReadFramebufferBinding = 0;
	int oldProgram = 0;
	int oldTextureBinding0 = 0;
	int oldTextureBinding1 = 0;
};
