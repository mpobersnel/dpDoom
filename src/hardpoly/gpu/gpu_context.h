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
#include "gpu_framebuffer.h"
#include "gpu_indexbuffer.h"
#include "gpu_program.h"
#include "gpu_sampler.h"
#include "gpu_storagebuffer.h"
#include "gpu_texture.h"
#include "gpu_uniformbuffer.h"
#include "gpu_vertexarray.h"
#include "gpu_vertexbuffer.h"
#include "gpu_types.h"

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
	TriangleFan,
	Triangles
};

class GPUContext
{
public:
	GPUContext();
	~GPUContext();
	
	void Begin();
	void End();
	
	void ClearError();
	void CheckError();

	void SetFrameBuffer(const GPUFrameBufferPtr &fb);
	void SetViewport(int x, int y, int width, int height);

	void SetProgram(const GPUProgramPtr &program);

	void SetSampler(int index, const GPUSamplerPtr &sampler);
	void SetTexture(int index, const GPUTexturePtr &texture);
	void SetUniforms(int index, const GPUUniformBufferPtr &buffer);
	void SetStorage(int index, const GPUStorageBufferPtr &storage);

	void SetVertexArray(const GPUVertexArrayPtr &vertexarray);
	void SetIndexBuffer(const GPUIndexBufferPtr &indexbuffer, GPUIndexFormat format);

	void Draw(GPUDrawMode mode, int vertexStart, int vertexCount);
	void DrawIndexed(GPUDrawMode mode, int indexStart, int indexCount);
	void DrawInstanced(GPUDrawMode mode, int vertexStart, int vertexCount, int instanceCount);
	void DrawIndexedInstanced(GPUDrawMode mode, int indexStart, int indexCount, int instanceCount);

	void ClearColorBuffer(int index, float r, float g, float b, float a);
	void ClearDepthBuffer(float depth);
	void ClearStencilBuffer(int stencil);
	void ClearDepthStencilBuffer(float depth, int stencil);

private:
	GPUContext(const GPUContext &) = delete;
	GPUContext &operator =(const GPUContext &) = delete;

	static int FromDrawMode(GPUDrawMode mode);

	GPUIndexFormat mIndexFormat = GPUIndexFormat::Uint16;
	
	int oldDrawFramebufferBinding = 0, oldReadFramebufferBinding = 0;
};

typedef std::shared_ptr<GPUContext> GPUContextPtr;
