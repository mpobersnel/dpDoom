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
#include "r_utility.h"
#include "gpu_framebuffer.h"
#include "gl/system/gl_system.h"

GPUFrameBuffer::GPUFrameBuffer(const std::vector<GPUTexture2DPtr> &color, const GPUTexture2DPtr &depthstencil)
{
	GLint oldHandle;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldHandle);

	glGenFramebuffers(1, (GLuint*)&mHandle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mHandle);

	for (size_t i = 0; i < color.size(); i++)
	{
		const auto &texture = color[i];
		if (texture->SampleCount() > 1)
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, GL_TEXTURE_2D_MULTISAMPLE, texture->Handle(), 0);
		else
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, GL_TEXTURE_2D, texture->Handle(), 0);
	}

	if (depthstencil && depthstencil->SampleCount() > 1)
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depthstencil->Handle(), 0);
	else if (depthstencil)
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthstencil->Handle(), 0);

	GLenum result = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if (result != GL_FRAMEBUFFER_COMPLETE)
	{
		I_FatalError("Framebuffer setup is not compatible with this graphics card or driver");
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldHandle);
}

GPUFrameBuffer::~GPUFrameBuffer()
{
	glDeleteFramebuffers(1, (GLuint*)&mHandle);
}
