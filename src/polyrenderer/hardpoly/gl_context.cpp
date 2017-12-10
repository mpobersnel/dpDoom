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
#include "gl_context.h"
#include "gl/system/gl_system.h"
#include "doomtype.h"
#include "i_system.h"
#include "w_wad.h"
#include <string>

GLContext::GLContext()
{
}

GLContext::~GLContext()
{
}

std::shared_ptr<GPUStagingTexture> GLContext::CreateStagingTexture(int width, int height, GPUPixelFormat format, const void *pixels)
{
	return std::make_shared<GLStagingTexture>(width, height, format, pixels);
}

std::shared_ptr<GPUTexture2D> GLContext::CreateTexture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels)
{
	return std::make_shared<GLTexture2D>(width, height, mipmap, sampleCount, format, pixels);
}

std::shared_ptr<GPUFrameBuffer> GLContext::CreateFrameBuffer(const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil)
{
	return std::make_shared<GLFrameBuffer>(color, depthstencil);
}

std::shared_ptr<GPUIndexBuffer> GLContext::CreateIndexBuffer(const void *data, int size)
{
	return std::make_shared<GLIndexBuffer>(data, size);
}

std::shared_ptr<GPUProgram> GLContext::CreateProgram()
{
	return std::make_shared<GLProgram>();
}

std::shared_ptr<GPUSampler> GLContext::CreateSampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV)
{
	return std::make_shared<GLSampler>(minfilter, magfilter, mipmap, wrapU, wrapV);
}

std::shared_ptr<GPUStorageBuffer> GLContext::CreateStorageBuffer(const void *data, int size)
{
	return std::make_shared<GLStorageBuffer>(data, size);
}

std::shared_ptr<GPUUniformBuffer> GLContext::CreateUniformBuffer(const void *data, int size)
{
	return std::make_shared<GLUniformBuffer>(data, size);
}

std::shared_ptr<GPUVertexArray> GLContext::CreateVertexArray(const std::vector<GPUVertexAttributeDesc> &attributes)
{
	return std::make_shared<GLVertexArray>(attributes);
}

std::shared_ptr<GPUVertexBuffer> GLContext::CreateVertexBuffer(const void *data, int size)
{
	return std::make_shared<GLVertexBuffer>(data, size);
}

void GLContext::CopyTexture(const std::shared_ptr<GPUTexture2D> &dest, const std::shared_ptr<GPUStagingTexture> &source)
{
	auto destimpl = std::static_pointer_cast<GLTexture2D>(dest);
	auto sourceimpl = std::static_pointer_cast<GLStagingTexture>(source);

	GLint oldBinding = 0, oldBufferBinding = 0;
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &oldBufferBinding);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldBinding);

	glBindTexture(GL_TEXTURE_2D, destimpl->Handle());
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sourceimpl->Handle());

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
		sourceimpl->Width(), sourceimpl->Height(),
		GLTexture2D::ToUploadFormat(sourceimpl->Format()),
		GLTexture2D::ToUploadType(sourceimpl->Format()),
		nullptr);

	glBindTexture(GL_TEXTURE_2D, oldBinding);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, oldBufferBinding);
}

void GLContext::Begin()
{
	ClearError();
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebufferBinding);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebufferBinding);
	glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTextureBinding0);
	glActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTextureBinding1);

	// To do: move elsewhere
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	//glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

void GLContext::End()
{
	// To do: move elsewhere
	glDisable(GL_DEPTH_TEST);
	//glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendColor(1.0f, 1.0f, 1.0f, 1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFramebufferBinding);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFramebufferBinding);
	glUseProgram(oldProgram);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, oldTextureBinding1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, oldTextureBinding0);
	CheckError();
}

void GLContext::ClearError()
{
	while (glGetError() != GL_NO_ERROR);
}

void GLContext::CheckError()
{
	if (glGetError() != GL_NO_ERROR)
		Printf("OpenGL error detected\n");
}

void GLContext::SetFrameBuffer(const std::shared_ptr<GPUFrameBuffer> &fb)
{
	if (fb)
	{
		auto fbgl = std::static_pointer_cast<GLFrameBuffer>(fb);
		glBindFramebuffer(GL_FRAMEBUFFER, fbgl->Handle());

		int count = MIN(fbgl->NumColorBuffers(), 16);
		GLenum drawbuffers[16];
		for (int i = 0; i < count; i++)
			drawbuffers[i] = GL_COLOR_ATTACHMENT0 + i;
		glDrawBuffers(count, drawbuffers);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

void GLContext::SetViewport(int x, int y, int width, int height)
{
	glViewport(x, y, width, height);
}

void GLContext::SetProgram(const std::shared_ptr<GPUProgram> &program)
{
	if (program)
		glUseProgram(std::static_pointer_cast<GLProgram>(program)->Handle());
	else
		glUseProgram(0);
}

void GLContext::SetUniform1i(int location, int value)
{
	if (location != -1)
		glUniform1i(location, value);
}

void GLContext::SetSampler(int index, const std::shared_ptr<GPUSampler> &sampler)
{
	if (sampler)
		glBindSampler(index, std::static_pointer_cast<GLSampler>(sampler)->Handle());
	else
		glBindSampler(index, 0);
}

void GLContext::SetTexture(int index, const std::shared_ptr<GPUTexture> &texture)
{
	glActiveTexture(GL_TEXTURE0 + index);
	if (texture)
		glBindTexture(GL_TEXTURE_2D, std::static_pointer_cast<GLTexture2D>(texture)->Handle());
	else
		glBindTexture(GL_TEXTURE_2D, 0);
}

void GLContext::SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer)
{
	if (buffer)
		glBindBufferBase(GL_UNIFORM_BUFFER, index, std::static_pointer_cast<GLUniformBuffer>(buffer)->Handle());
	else
		glBindBufferBase(GL_UNIFORM_BUFFER, index, 0);
}

void GLContext::SetUniforms(int index, const std::shared_ptr<GPUUniformBuffer> &buffer, ptrdiff_t offset, size_t size)
{
	if (buffer)
		glBindBufferRange(GL_UNIFORM_BUFFER, index, std::static_pointer_cast<GLUniformBuffer>(buffer)->Handle(), offset, size);
	else
		glBindBufferBase(GL_UNIFORM_BUFFER, index, 0);
}

void GLContext::SetStorage(int index, const std::shared_ptr<GPUStorageBuffer> &storage)
{
	if (storage)
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, std::static_pointer_cast<GLStorageBuffer>(storage)->Handle());
	else
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, 0);
}

void GLContext::SetClipDistance(int index, bool enable)
{
	if (enable)
		glEnable(GL_CLIP_DISTANCE0 + index);
	else
		glDisable(GL_CLIP_DISTANCE0 + index);
}

void GLContext::SetOpaqueBlend(int srcalpha, int destalpha)
{
	glDisable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ZERO);
}

void GLContext::SetMaskedBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void GLContext::SetAlphaBlendFunc(int srcalpha, int destalpha)
{
	int srcblend;
	if (srcalpha == 0.0f)
		srcblend = GL_ZERO;
	else if (srcalpha == 1.0f)
		srcblend = GL_ONE;
	else
		srcblend = GL_CONSTANT_ALPHA;

	int destblend;
	if (destalpha == 0.0f)
		destblend = GL_ZERO;
	else if (destalpha == 1.0f)
		destblend = GL_ONE;
	else if (srcalpha + destalpha >= 255)
		destblend = GL_ONE_MINUS_CONSTANT_ALPHA;
	else
		destblend = GL_CONSTANT_COLOR;

	glBlendColor(destalpha / 256.0f, destalpha / 256.0f, destalpha / 256.0f, srcalpha / 256.0f);
	glBlendFunc(srcblend, destblend);
}

void GLContext::SetAddClampBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	SetAlphaBlendFunc(srcalpha, destalpha);
}

void GLContext::SetSubClampBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_SUBTRACT);
	SetAlphaBlendFunc(srcalpha, destalpha);
}

void GLContext::SetRevSubClampBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	SetAlphaBlendFunc(srcalpha, destalpha);
}

void GLContext::SetAddSrcColorBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
}

void GLContext::SetShadedBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void GLContext::SetAddClampShadedBlend(int srcalpha, int destalpha)
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
}

void GLContext::SetVertexArray(const std::shared_ptr<GPUVertexArray> &vertexarray)
{
	if (vertexarray)
		glBindVertexArray(std::static_pointer_cast<GLVertexArray>(vertexarray)->Handle());
	else
		glBindVertexArray(0);
}

void GLContext::SetIndexBuffer(const std::shared_ptr<GPUIndexBuffer> &indexbuffer, GPUIndexFormat format)
{
	if (indexbuffer)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, std::static_pointer_cast<GLIndexBuffer>(indexbuffer)->Handle());
	else
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	mIndexFormat = format;
}

void GLContext::Draw(GPUDrawMode mode, int vertexStart, int vertexCount)
{
	glDrawArrays(FromDrawMode(mode), vertexStart, vertexCount);
}

void GLContext::DrawIndexed(GPUDrawMode mode, int indexStart, int indexCount)
{
	int type = (mIndexFormat == GPUIndexFormat::Uint16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
	int size = (mIndexFormat == GPUIndexFormat::Uint16) ? 2 : 4;
	glDrawElements(FromDrawMode(mode), indexCount, type, (const void *)(ptrdiff_t)(indexStart * size));
}

/*
void GLContext::DrawRangeIndexed(GPUDrawMode mode, int minIndexArrayValue, int maxIndexArrayValue, int indexStart, int indexCount)
{
	int type = (mIndexFormat == GPUIndexFormat::Uint16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
	int size = (mIndexFormat == GPUIndexFormat::Uint16) ? 2 : 4;
	glDrawRangeElements(FromDrawMode(mode), minIndexArrayValue, maxIndexArrayValue, indexCount, type, (const void *)(ptrdiff_t)(indexStart * size));
}
*/

void GLContext::DrawInstanced(GPUDrawMode mode, int vertexStart, int vertexCount, int instanceCount)
{
	glDrawArraysInstanced(FromDrawMode(mode), vertexStart, vertexCount, instanceCount);
}

void GLContext::DrawIndexedInstanced(GPUDrawMode mode, int indexStart, int indexCount, int instanceCount)
{
	int type = (mIndexFormat == GPUIndexFormat::Uint16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
	int size = (mIndexFormat == GPUIndexFormat::Uint16) ? 2 : 4;
	glDrawElementsInstanced(FromDrawMode(mode), indexCount, type, (const void *)(ptrdiff_t)(indexStart * size), instanceCount);
}

int GLContext::FromDrawMode(GPUDrawMode mode)
{
	switch (mode)
	{
	default:
	case GPUDrawMode::Points: return GL_POINTS;
	case GPUDrawMode::LineStrip: return GL_LINE_STRIP;
	case GPUDrawMode::LineLoop: return GL_LINE_LOOP;
	case GPUDrawMode::Lines: return GL_LINES;
	case GPUDrawMode::TriangleStrip: return GL_TRIANGLE_STRIP;
	case GPUDrawMode::TriangleFan: return GL_TRIANGLE_FAN;
	case GPUDrawMode::Triangles: return GL_TRIANGLES;
	}
}

void GLContext::ClearColorBuffer(int index, float r, float g, float b, float a)
{
	GLfloat value[4] = { r, g, b, a };
	glClearBufferfv(GL_COLOR, index, value);
}

void GLContext::ClearDepthBuffer(float depth)
{
	glClearBufferfv(GL_DEPTH, 0, &depth);
}

void GLContext::ClearStencilBuffer(int stencil)
{
	glClearBufferiv(GL_STENCIL, 0, &stencil);
}

void GLContext::ClearDepthStencilBuffer(float depth, int stencil)
{
	glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
}

/////////////////////////////////////////////////////////////////////////////

GLFrameBuffer::GLFrameBuffer(const std::vector<std::shared_ptr<GPUTexture2D>> &color, const std::shared_ptr<GPUTexture2D> &depthstencil) : mNumColorBuffers((int)color.size())
{
	GLint oldHandle;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldHandle);

	glGenFramebuffers(1, (GLuint*)&mHandle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mHandle);

	for (size_t i = 0; i < color.size(); i++)
	{
		const auto &texture = std::static_pointer_cast<GLTexture2D>(color[i]);
		if (texture->SampleCount() > 1)
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, GL_TEXTURE_2D_MULTISAMPLE, texture->Handle(), 0);
		else
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, GL_TEXTURE_2D, texture->Handle(), 0);
	}

	if (depthstencil && depthstencil->SampleCount() > 1)
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, std::static_pointer_cast<GLTexture2D>(depthstencil)->Handle(), 0);
	else if (depthstencil)
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, std::static_pointer_cast<GLTexture2D>(depthstencil)->Handle(), 0);

	GLenum result = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if (result != GL_FRAMEBUFFER_COMPLETE)
	{
		I_FatalError("Framebuffer setup is not compatible with this graphics card or driver");
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldHandle);
}

GLFrameBuffer::~GLFrameBuffer()
{
	glDeleteFramebuffers(1, (GLuint*)&mHandle);
}

/////////////////////////////////////////////////////////////////////////////

GLIndexBuffer::GLIndexBuffer(const void *data, int size)
{
	glGenBuffers(1, (GLuint*)&mHandle);

	GLint oldHandle;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mHandle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_STREAM_DRAW);
	//glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT /*| GL_CLIENT_STORAGE_BIT*/);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldHandle);
}

GLIndexBuffer::~GLIndexBuffer()
{
	glDeleteBuffers(1, (GLuint*)&mHandle);
}

void GLIndexBuffer::Upload(const void *data, int size)
{
	GLint oldHandle;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldHandle);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mHandle);

	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size, data);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldHandle);
}

void *GLIndexBuffer::MapWriteOnly()
{
	GLint oldHandle;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mHandle);
	void *data = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldHandle);

	return data;
}

void GLIndexBuffer::Unmap()
{
	GLint oldHandle;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mHandle);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldHandle);
}

/////////////////////////////////////////////////////////////////////////////

GLProgram::GLProgram()
{
}

GLProgram::~GLProgram()
{
	if (mHandle)
		glDeleteProgram(mHandle);
	for (auto it : mShaderHandle)
		glDeleteShader(it.second);
}

std::string GLProgram::PrefixCode() const
{
	std::string prefix = "#version 330\n";
	for (auto it : mDefines)
	{
		prefix += "#define " + it.first + " " + it.second + "\n";
	}
	prefix += "#line 1\n";
	return prefix;
}

void GLProgram::Compile(GPUShaderType type, const char *name, const std::string &code)
{
	int &shaderHandle = mShaderHandle[(int)type];
	switch (type)
	{
	default:
	case GPUShaderType::Vertex: shaderHandle = glCreateShader(GL_VERTEX_SHADER); break;
	case GPUShaderType::Fragment: shaderHandle = glCreateShader(GL_FRAGMENT_SHADER); break;
	}

	std::string shaderCode = PrefixCode() + code;

	int lengths[1] = { (int)shaderCode.length() };
	const char *sources[1] = { shaderCode.c_str() };
	glShaderSource(shaderHandle, 1, sources, lengths);

	glCompileShader(shaderHandle);

	GLint status = 0;
	glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		I_FatalError("Compile Shader '%s':\n%s\n", name, GetShaderInfoLog(shaderHandle).c_str());
	}
	else
	{
		if (mHandle == 0)
			mHandle = glCreateProgram();
		glAttachShader(mHandle, shaderHandle);
	}
}

void GLProgram::SetAttribLocation(const std::string &name, int index)
{
	glBindAttribLocation(mHandle, index, name.c_str());
}

void GLProgram::SetFragOutput(const std::string &name, int index)
{
	glBindFragDataLocation(mHandle, index, name.c_str());
}

void GLProgram::SetUniformBlock(const std::string &name, int index)
{
	GLuint uniformBlockIndex = glGetUniformBlockIndex(mHandle, name.c_str());
	if (uniformBlockIndex != GL_INVALID_INDEX)
		glUniformBlockBinding(mHandle, uniformBlockIndex, index);
}

int GLProgram::GetUniformLocation(const char *name)
{
	return glGetUniformLocation(mHandle, name);
}

void GLProgram::Link(const std::string &name)
{
	glLinkProgram(mHandle);

	GLint status = 0;
	glGetProgramiv(mHandle, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		I_FatalError("Link Shader '%s':\n%s\n", name.c_str(), GetProgramInfoLog().c_str());
	}
}

std::string GLProgram::GetShaderInfoLog(int handle) const
{
	static char buffer[10000];
	GLsizei length = 0;
	buffer[0] = 0;
	glGetShaderInfoLog(handle, 10000, &length, buffer);
	return buffer;
}

std::string GLProgram::GetProgramInfoLog() const
{
	static char buffer[10000];
	GLsizei length = 0;
	buffer[0] = 0;
	glGetProgramInfoLog(mHandle, 10000, &length, buffer);
	return buffer;
}

/////////////////////////////////////////////////////////////////////////////

GLSampler::GLSampler(GPUSampleMode minfilter, GPUSampleMode magfilter, GPUMipmapMode mipmap, GPUWrapMode wrapU, GPUWrapMode wrapV)
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

GLSampler::~GLSampler()
{
	glDeleteSamplers(1, (GLuint*)&mHandle);
}

/////////////////////////////////////////////////////////////////////////////

GLStorageBuffer::GLStorageBuffer(const void *data, int size)
{
	glGenBuffers(1, (GLuint*)&mHandle);

	GLint oldHandle;
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mHandle);
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_STATIC_DRAW);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, oldHandle);
}

void GLStorageBuffer::Upload(const void *data, int size)
{
	GLint oldHandle;
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mHandle);
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_STATIC_DRAW);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, oldHandle);
}

GLStorageBuffer::~GLStorageBuffer()
{
	glDeleteBuffers(1, (GLuint*)&mHandle);
}

/////////////////////////////////////////////////////////////////////////////

GLStagingTexture::GLStagingTexture(int width, int height, GPUPixelFormat format, const void *pixels)
{
	mWidth = width;
	mHeight = height;
	mFormat = format;

	GL_PIXEL_UNPACK_BUFFER_BINDING;
	GL_PIXEL_UNPACK_BUFFER;

	glGenBuffers(1, (GLuint*)&mHandle);

	GLint oldHandle;
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mHandle);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, GetBytesPerPixel(format) * width * height, pixels, GL_DYNAMIC_COPY);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, oldHandle);
}

GLStagingTexture::~GLStagingTexture()
{
	glDeleteBuffers(1, (GLuint*)&mHandle);
}

int GLStagingTexture::GetBytesPerPixel(GPUPixelFormat format)
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

void GLStagingTexture::Upload(int x, int y, int width, int height, const void *pixels)
{
	if (x == 0 && y == 0 && width == mWidth && height == mHeight)
	{
		GLint oldHandle;
		glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &oldHandle);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mHandle);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, GetBytesPerPixel(mFormat) * width * height, pixels, GL_DYNAMIC_COPY);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, oldHandle);
	}
	else
	{
		int bytesperpixel = GetBytesPerPixel(mFormat);
		int destpitch = mWidth * bytesperpixel;
		int srcpitch = width * bytesperpixel;
		uint8_t *src = (uint8_t*)pixels;
		uint8_t *dest = (uint8_t*)Map();
		if (dest)
		{
			dest += x * bytesperpixel + y * destpitch;
			int count = width * bytesperpixel;
			for (int i = 0; i < height; i++)
			{
				memcpy(dest, src, count);
				dest += destpitch;
				src += srcpitch;
			}
			Unmap();
		}
	}
}

void *GLStagingTexture::Map()
{
	GLint oldHandle;
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &oldHandle);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mHandle);

	void *data = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, oldHandle);
	return data;
}

void GLStagingTexture::Unmap()
{
	GLint oldHandle;
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &oldHandle);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mHandle);

	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, oldHandle);
}

/////////////////////////////////////////////////////////////////////////////

GLTexture2D::GLTexture2D(int width, int height, bool mipmap, int sampleCount, GPUPixelFormat format, const void *pixels)
{
	mWidth = width;
	mHeight = height;
	mMipmap = mipmap;
	mSampleCount = sampleCount;
	mFormat = format;

	glGenTextures(1, (GLuint*)&mHandle);

	GLint target = sampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	GLint binding = sampleCount > 1 ? GL_TEXTURE_BINDING_2D_MULTISAMPLE : GL_TEXTURE_BINDING_2D;

	GLint oldHandle;
	glGetIntegerv(binding, &oldHandle);
	glBindTexture(target, mHandle);

	if (sampleCount > 1)
	{
		glTexImage2DMultisample(target, sampleCount, ToInternalFormat(format), width, height, GL_FALSE);
	}
	else
	{
		glTexImage2D(target, 0, ToInternalFormat(format), width, height, 0, ToUploadFormat(format), ToUploadType(format), pixels);
	}

	if (mipmap && sampleCount <= 1)
	{
		if (pixels)
		{
			glGenerateMipmap(target);
		}
		else
		{
			GLint levels = NumLevels(width, height);
			GLint internalformat = ToInternalFormat(format);
			GLint uploadformat = ToUploadFormat(format);
			GLint uploadtype = ToUploadType(format);
			for (int i = 0; i < levels; i++)
			{
				glTexImage2D(target, i, internalformat, width, height, 0, uploadformat, uploadtype, nullptr);
				width = MAX(1, width / 2);
				height = MAX(1, height / 2);
			}
		}
	}

	glBindTexture(target, oldHandle);
}

GLTexture2D::~GLTexture2D()
{
	glDeleteTextures(1, (GLuint*)&mHandle);
}

void GLTexture2D::Upload(int x, int y, int width, int height, int level, const void *pixels)
{
	if (mSampleCount <= 1)
	{
		GLint oldHandle, oldUnpackHandle;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldHandle);
		glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &oldUnpackHandle);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, mHandle);
		glTexSubImage2D(GL_TEXTURE_2D, level, x, y, width, height, ToUploadFormat(mFormat), ToUploadType(mFormat), pixels);
		glBindTexture(GL_TEXTURE_2D, oldHandle);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, oldUnpackHandle);
	}
}

int GLTexture2D::NumLevels(int width, int height)
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

int GLTexture2D::ToInternalFormat(GPUPixelFormat format)
{
	switch (format)
	{
	default:
	case GPUPixelFormat::RGBA8: return GL_RGBA8;
	case GPUPixelFormat::BGRA8: return GL_RGBA8;
	case GPUPixelFormat::sRGB8_Alpha8: return GL_SRGB8_ALPHA8;
	case GPUPixelFormat::RGBA16: return GL_RGBA16;
	case GPUPixelFormat::RGBA16f: return GL_RGBA16F;
	case GPUPixelFormat::RGBA32f: return GL_RGBA32F;
	case GPUPixelFormat::Depth24_Stencil8: return GL_DEPTH24_STENCIL8;
	case GPUPixelFormat::R32f: return GL_R32F;
	case GPUPixelFormat::R8: return GL_R8;
	}
}

int GLTexture2D::ToUploadFormat(GPUPixelFormat format)
{
	switch (format)
	{
	default:
	case GPUPixelFormat::RGBA8: return GL_RGBA;
	case GPUPixelFormat::BGRA8: return GL_BGRA;
	case GPUPixelFormat::sRGB8_Alpha8: return GL_RGBA;
	case GPUPixelFormat::RGBA16: return GL_RGBA;
	case GPUPixelFormat::RGBA16f: return GL_RGBA;
	case GPUPixelFormat::RGBA32f: return GL_RGBA;
	case GPUPixelFormat::Depth24_Stencil8: return GL_DEPTH_STENCIL;
	case GPUPixelFormat::R32f: return GL_RED;
	case GPUPixelFormat::R8: return GL_RED;
	}
}

int GLTexture2D::ToUploadType(GPUPixelFormat format)
{
	switch (format)
	{
	default:
	case GPUPixelFormat::RGBA8: return GL_UNSIGNED_BYTE;
	case GPUPixelFormat::BGRA8: return GL_UNSIGNED_BYTE;
	case GPUPixelFormat::sRGB8_Alpha8: return GL_UNSIGNED_BYTE;
	case GPUPixelFormat::RGBA16: return GL_UNSIGNED_SHORT;
	case GPUPixelFormat::RGBA16f: return GL_HALF_FLOAT;
	case GPUPixelFormat::RGBA32f: return GL_FLOAT;
	case GPUPixelFormat::Depth24_Stencil8: return GL_UNSIGNED_INT_24_8;
	case GPUPixelFormat::R32f: return GL_FLOAT;
	case GPUPixelFormat::R8: return GL_UNSIGNED_BYTE;
	}
}

/////////////////////////////////////////////////////////////////////////////

GLUniformBuffer::GLUniformBuffer(const void *data, int size)
{
	glGenBuffers(1, (GLuint*)&mHandle);

	GLint oldHandle;
	glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_UNIFORM_BUFFER, mHandle);
	glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STREAM_DRAW);

	glBindBuffer(GL_UNIFORM_BUFFER, oldHandle);
}

void GLUniformBuffer::Upload(const void *data, int size)
{
	GLint oldHandle;
	glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_UNIFORM_BUFFER, mHandle);
	glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STREAM_DRAW);

	glBindBuffer(GL_UNIFORM_BUFFER, oldHandle);
}

/*void *GLUniformBuffer::MapWriteOnly()
{
	GLint oldHandle;
	glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_UNIFORM_BUFFER, mHandle);
	void *data = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);

	glBindBuffer(GL_UNIFORM_BUFFER, oldHandle);

	return data;
}

void GLUniformBuffer::Unmap()
{
	GLint oldHandle;
	glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_UNIFORM_BUFFER, mHandle);
	glUnmapBuffer(GL_UNIFORM_BUFFER);

	glBindBuffer(GL_UNIFORM_BUFFER, oldHandle);
}*/

GLUniformBuffer::~GLUniformBuffer()
{
	glDeleteBuffers(1, (GLuint*)&mHandle);
}

/////////////////////////////////////////////////////////////////////////////

GLVertexArray::GLVertexArray(const std::vector<GPUVertexAttributeDesc> &attributes) : mAttributes(attributes)
{
	glGenVertexArrays(1, (GLuint*)&mHandle);

	GLint oldHandle, oldArrayBinding;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldHandle);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldArrayBinding);
	glBindVertexArray(mHandle);

	for (const auto &attr : attributes)
	{
		glBindBuffer(GL_ARRAY_BUFFER, std::static_pointer_cast<GLVertexBuffer>(attr.Buffer)->Handle());
		glEnableVertexAttribArray(attr.Index);
		glVertexAttribPointer(attr.Index, attr.Size, FromType(attr.Type), attr.Normalized ? GL_TRUE : GL_FALSE, attr.Stride, (const GLvoid *)attr.Offset);
	}

	glBindBuffer(GL_ARRAY_BUFFER, oldArrayBinding);
	glBindVertexArray(oldHandle);
}

GLVertexArray::~GLVertexArray()
{
	glDeleteVertexArrays(1, (GLuint*)&mHandle);
}

int GLVertexArray::FromType(GPUVertexAttributeType type)
{
	switch (type)
	{
	default:
	case GPUVertexAttributeType::Int8: return GL_BYTE;
	case GPUVertexAttributeType::Uint8: return GL_UNSIGNED_BYTE;
	case GPUVertexAttributeType::Int16: return GL_SHORT;
	case GPUVertexAttributeType::Uint16: return GL_UNSIGNED_SHORT;
	case GPUVertexAttributeType::Int32: return GL_INT;
	case GPUVertexAttributeType::Uint32: return GL_UNSIGNED_INT;
	case GPUVertexAttributeType::HalfFloat: return GL_HALF_FLOAT;
	case GPUVertexAttributeType::Float: return GL_FLOAT;
	}
}

/////////////////////////////////////////////////////////////////////////////

GLVertexBuffer::GLVertexBuffer(const void *data, int size)
{
	glGenBuffers(1, (GLuint*)&mHandle);

	GLint oldHandle;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_ARRAY_BUFFER, mHandle);
	glBufferData(GL_ARRAY_BUFFER, size, data, GL_STREAM_DRAW);
	//glBufferStorage(GL_ARRAY_BUFFER, size, data, GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT /*| GL_CLIENT_STORAGE_BIT*/);

	glBindBuffer(GL_ARRAY_BUFFER, oldHandle);
}

GLVertexBuffer::~GLVertexBuffer()
{
	glDeleteBuffers(1, (GLuint*)&mHandle);
}

void GLVertexBuffer::Upload(const void *data, int size)
{
	GLint oldHandle;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldHandle);
	glBindBuffer(GL_ARRAY_BUFFER, mHandle);

	glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);

	glBindBuffer(GL_ARRAY_BUFFER, oldHandle);
}

void *GLVertexBuffer::MapWriteOnly()
{
	GLint oldHandle;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_ARRAY_BUFFER, mHandle);
	void *data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

	glBindBuffer(GL_ARRAY_BUFFER, oldHandle);

	return data;
}

void GLVertexBuffer::Unmap()
{
	GLint oldHandle;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_ARRAY_BUFFER, mHandle);
	glUnmapBuffer(GL_ARRAY_BUFFER);

	glBindBuffer(GL_ARRAY_BUFFER, oldHandle);
}
