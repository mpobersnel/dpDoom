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
#include <vector>
#include <map>
#include "polyrenderer/math/gpu_types.h"

enum class GPUPixelFormat
{
	RGBA8,
	BGRA8,
	sRGB8_Alpha8,
	RGBA16,
	RGBA16f,
	RGBA32f,
	Depth24_Stencil8,
	R32f,
	R8
};

class GPUTexture
{
public:
	virtual ~GPUTexture() { }
};

class GPUTexture2D : public GPUTexture
{
public:
	virtual void Upload(int x, int y, int width, int height, int level, const void *pixels) = 0;
	virtual int SampleCount() const = 0;
	virtual int Width() const = 0;
	virtual int Height() const = 0;
	virtual GPUPixelFormat Format() const = 0;
};

class GPUStagingTexture
{
public:
	virtual ~GPUStagingTexture() { }

	//virtual void Upload(int x, int y, int width, int height, const void *pixels) = 0;
	virtual int Width() const = 0;
	virtual int Height() const = 0;
	virtual GPUPixelFormat Format() const = 0;

	virtual void *Map() = 0;
	virtual void Unmap() = 0;
	virtual int GetMappedRowPitch() = 0;
};

class GPUFrameBuffer
{
public:
	virtual ~GPUFrameBuffer() { }
};

class GPUIndexBuffer
{
public:
	virtual ~GPUIndexBuffer() { }

	virtual void Upload(const void *data, int size) = 0;
	virtual void *MapWriteOnly() = 0;
	virtual void Unmap() = 0;
};

enum class GPUShaderType
{
	Vertex,
	Fragment
};

class GPUProgram
{
public:
	virtual ~GPUProgram() { }

	void SetDefine(const std::string &name);
	void SetDefine(const std::string &name, int value);
	void SetDefine(const std::string &name, float value);
	void SetDefine(const std::string &name, double value);
	void SetDefine(const std::string &name, const std::string &value);

	void Compile(GPUShaderType type, const char *lumpName);

	virtual void Compile(GPUShaderType type, const char *name, const std::string &code) = 0;
	virtual void SetAttribLocation(const std::string &name, int index) = 0;
	virtual void SetFragOutput(const std::string &name, int index) = 0;
	virtual void SetUniformBlockLocation(const std::string &name, int index) = 0;
	virtual void SetTextureLocation(const std::string &name, int index) = 0;
	virtual void SetTextureLocation(const std::string &texturename, const std::string &samplername, int index) = 0;
	virtual void Link(const std::string &name) = 0;

protected:
	std::map<std::string, std::string> mDefines;
};

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
	virtual ~GPUSampler() { }
};

class GPUStorageBuffer
{
public:
	virtual ~GPUStorageBuffer() { }
	virtual void Upload(const void *data, int size) = 0;
};

class GPUUniformBuffer
{
public:
	virtual ~GPUUniformBuffer() { }
	virtual void Upload(const void *data, int size) = 0;
	//virtual void *MapWriteOnly() = 0;
	//virtual void Unmap() = 0;
};

class GPUVertexBuffer;

enum class GPUVertexAttributeType
{
	Int8,
	Uint8,
	Int16,
	Uint16,
	Int32,
	Uint32,
	HalfFloat,
	Float
};

class GPUVertexAttributeDesc
{
public:
	GPUVertexAttributeDesc() { }
	GPUVertexAttributeDesc(int index, int size, GPUVertexAttributeType type, bool normalized, int stride, std::size_t offset, std::shared_ptr<GPUVertexBuffer> buffer)
		: Index(index), Size(size), Type(type), Normalized(normalized), Stride(stride), Offset(offset), Buffer(buffer) { }

	int Index;
	int Size;
	GPUVertexAttributeType Type;
	bool Normalized;
	int Stride;
	std::size_t Offset;
	std::shared_ptr<GPUVertexBuffer> Buffer;
};

class GPUVertexArray
{
public:
	virtual ~GPUVertexArray() { }
};

/////////////////////////////////////////////////////////////////////////////

class GPUVertexBuffer
{
public:
	virtual ~GPUVertexBuffer() { }
	//virtual void Upload(const void *data, int size) = 0;
	virtual void *MapWriteOnly() = 0;
	virtual void Unmap() = 0;
};

/////////////////////////////////////////////////////////////////////////////

enum class GPUIndexFormat
{
	Uint16,
	Uint32
};

enum class GPUDrawMode
{
	Points,
	LineStrip,
	LineLoop,
	Lines,
	TriangleStrip,
	//TriangleFan,
	Triangles
};

enum class GPUBlendEquation
{
	Add,
	Subtract,
	ReverseSubtract,
	Min,
	Max
};

enum class GPUBlendFunc
{
	Zero,
	One,
	SrcColor,
	InvSrcColor,
	SrcAlpha,
	InvSrcAlpha,
	DestAlpha,
	InvDestAlpha,
	DestColor,
	InvDestColor,
	BlendFactor,
	InvBlendFactor
};

inline int ToBlendKey(GPUBlendEquation opcolor, GPUBlendFunc srccolor, GPUBlendFunc destcolor, GPUBlendEquation opalpha, GPUBlendFunc srcalpha, GPUBlendFunc destalpha)
{
	return ((int)opcolor) | (((int)srccolor) << 4) | (((int)destcolor) << 8) | (((int)opalpha) << 12) | (((int)srcalpha) << 16) | (((int)destalpha) << 20);
}

class GPUContext
{
public:
	virtual ~GPUContext() { }

	virtual bool IsOpenGL() const = 0;
	virtual ClipZRange GetClipZRange() const = 0;

	virtual std::shared_ptr<GPUStagingTexture> CreateStagingTexture(int width, int height, GPUPixelFormat format, const void *pixels = nullptr) = 0;
	virtual std::shared_ptr<GPUTexture2D> CreateTexture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels = nullptr) = 0;
	virtual std::shared_ptr<GPUFrameBuffer> CreateFrameBuffer(const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil) = 0;
	virtual std::shared_ptr<GPUIndexBuffer> CreateIndexBuffer(const void *data, int size) = 0;
	virtual std::shared_ptr<GPUProgram> CreateProgram() = 0;
	virtual std::shared_ptr<GPUSampler> CreateSampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV) = 0;
	virtual std::shared_ptr<GPUStorageBuffer> CreateStorageBuffer(const void *data, int size) = 0;
	virtual std::shared_ptr<GPUUniformBuffer> CreateUniformBuffer(const void *data, int size) = 0;
	virtual std::shared_ptr<GPUVertexArray> CreateVertexArray(const std::vector<GPUVertexAttributeDesc> &attributes) = 0;
	virtual std::shared_ptr<GPUVertexBuffer> CreateVertexBuffer(const void *data, int size) = 0;

	virtual void CopyTexture(const std::shared_ptr<GPUTexture2D> &dest, const std::shared_ptr<GPUStagingTexture> &source) = 0;
	virtual void CopyColorBufferToTexture(const std::shared_ptr<GPUTexture2D> &dest) = 0;
	virtual void GetPixelsBgra(int width, int height, uint32_t *pixels) = 0;

	virtual void ClearError() = 0;
	virtual void CheckError() = 0;

	virtual void SetFrameBuffer(const std::shared_ptr<GPUFrameBuffer> &fb) = 0;
	virtual void SetViewport(int x, int y, int width, int height) = 0;

	virtual void SetProgram(const std::shared_ptr<GPUProgram> &program) = 0;

	virtual void SetSampler(int index, const std::shared_ptr<GPUSampler> &sampler) = 0;
	virtual void SetTexture(int index, const std::shared_ptr<GPUTexture> &texture) = 0;
	virtual void SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer) = 0;
	virtual void SetStorage(int index, const std::shared_ptr<GPUStorageBuffer> &storage) = 0;

	virtual void SetClipDistance(int index, bool enable) = 0;

	virtual void SetLineSmooth(bool enable) = 0;
	virtual void SetScissor(int x, int y, int width, int height) = 0;
	virtual void ClearScissorBox(float r, float g, float b, float a) = 0;
	virtual void ResetScissor() = 0;

	virtual void SetBlend(GPUBlendEquation opcolor, GPUBlendFunc srccolor, GPUBlendFunc destcolor, GPUBlendEquation opalpha, GPUBlendFunc srcalpha, GPUBlendFunc destalpha, const Vec4f &blendcolor = Vec4f(0.0f)) = 0;
	virtual void ResetBlend() = 0;

	virtual void SetVertexArray(const std::shared_ptr<GPUVertexArray> &vertexarray) = 0;
	virtual void SetIndexBuffer(const std::shared_ptr<GPUIndexBuffer> &indexbuffer, GPUIndexFormat format = GPUIndexFormat::Uint16) = 0;

	virtual void Draw(GPUDrawMode mode, int vertexStart, int vertexCount) = 0;
	virtual void DrawIndexed(GPUDrawMode mode, int indexStart, int indexCount) = 0;
	virtual void DrawInstanced(GPUDrawMode mode, int vertexStart, int vertexCount, int instanceCount) = 0;
	virtual void DrawIndexedInstanced(GPUDrawMode mode, int indexStart, int indexCount, int instanceCount) = 0;

	virtual void ClearColorBuffer(int index, float r, float g, float b, float a) = 0;
	virtual void ClearDepthBuffer(float depth) = 0;
	virtual void ClearStencilBuffer(int stencil) = 0;
	virtual void ClearDepthStencilBuffer(float depth, int stencil) = 0;
};
