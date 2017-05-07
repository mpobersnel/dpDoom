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
#include "gpu_context.h"
#include "gl/system/gl_system.h"
#include "doomtype.h"

GPUContext::GPUContext()
{
}

GPUContext::~GPUContext()
{
}

void GPUContext::Begin()
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
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

void GPUContext::End()
{
	// To do: move elsewhere
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFramebufferBinding);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFramebufferBinding);
	glUseProgram(oldProgram);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, oldTextureBinding1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, oldTextureBinding0);
	CheckError();
}

void GPUContext::ClearError()
{
	while (glGetError() != GL_NO_ERROR);
}

void GPUContext::CheckError()
{
	if (glGetError() != GL_NO_ERROR)
		Printf("OpenGL error detected\n");
}

void GPUContext::SetFrameBuffer(const GPUFrameBufferPtr &fb)
{
	if (fb)
		glBindFramebuffer(GL_FRAMEBUFFER, fb->Handle());
	else
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GPUContext::SetViewport(int x, int y, int width, int height)
{
	glViewport(x, y, width, height);
}

void GPUContext::SetProgram(const GPUProgramPtr &program)
{
	if (program)
		glUseProgram(program->Handle());
	else
		glUseProgram(0);
}

void GPUContext::SetSampler(int index, const GPUSamplerPtr &sampler)
{
	if (sampler)
		glBindSampler(index, sampler->Handle());
	else
		glBindSampler(index, 0);
}

void GPUContext::SetTexture(int index, const GPUTexturePtr &texture)
{
	glActiveTexture(GL_TEXTURE0 + index);
	if (texture)
		glBindTexture(GL_TEXTURE_2D, texture->Handle());
	else
		glBindTexture(GL_TEXTURE_2D, 0);
}

void GPUContext::SetUniforms(int index, const GPUUniformBufferPtr &buffer)
{
	if (buffer)
		glBindBufferBase(GL_UNIFORM_BUFFER, index, buffer->Handle());
	else
		glBindBufferBase(GL_UNIFORM_BUFFER, index, 0);
}

void GPUContext::SetStorage(int index, const GPUStorageBufferPtr &storage)
{
	if (storage)
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, storage->Handle());
	else
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, 0);
}

void GPUContext::SetVertexArray(const GPUVertexArrayPtr &vertexarray)
{
	if (vertexarray)
		glBindVertexArray(vertexarray->Handle());
	else
		glBindVertexArray(0);
}

void GPUContext::SetIndexBuffer(const GPUIndexBufferPtr &indexbuffer, GPUIndexFormat format)
{
	if (indexbuffer)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexbuffer->Handle());
	else
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	mIndexFormat = format;
}

void GPUContext::Draw(GPUDrawMode mode, int vertexStart, int vertexCount)
{
	glDrawArrays(FromDrawMode(mode), vertexStart, vertexCount);
}

void GPUContext::DrawIndexed(GPUDrawMode mode, int indexStart, int indexCount)
{
	int type = (mIndexFormat == GPUIndexFormat::Uint16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
	int size = (mIndexFormat == GPUIndexFormat::Uint16) ? 2 : 4;
	glDrawElements(FromDrawMode(mode), indexCount, type, (const void *)(ptrdiff_t)(indexStart * size));
}

void GPUContext::DrawInstanced(GPUDrawMode mode, int vertexStart, int vertexCount, int instanceCount)
{
	glDrawArraysInstanced(FromDrawMode(mode), vertexStart, vertexCount, instanceCount);
}

void GPUContext::DrawIndexedInstanced(GPUDrawMode mode, int indexStart, int indexCount, int instanceCount)
{
	int type = (mIndexFormat == GPUIndexFormat::Uint16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
	int size = (mIndexFormat == GPUIndexFormat::Uint16) ? 2 : 4;
	glDrawElementsInstanced(FromDrawMode(mode), indexCount, type, (const void *)(ptrdiff_t)(indexStart * size), instanceCount);
}

int GPUContext::FromDrawMode(GPUDrawMode mode)
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

void GPUContext::ClearColorBuffer(int index, float r, float g, float b, float a)
{
	GLfloat value[4] = { r, g, b, a };
	glClearBufferfv(GL_COLOR, index, value);
}

void GPUContext::ClearDepthBuffer(float depth)
{
	glClearBufferfv(GL_DEPTH, 0, &depth);
}

void GPUContext::ClearStencilBuffer(int stencil)
{
	glClearBufferiv(GL_STENCIL, 0, &stencil);
}

void GPUContext::ClearDepthStencilBuffer(float depth, int stencil)
{
	glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
}
