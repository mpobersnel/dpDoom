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
#include "gpu_indexbuffer.h"
#include "gl/system/gl_system.h"

GPUIndexBuffer::GPUIndexBuffer(const void *data, int size)
{
	glGenBuffers(1, (GLuint*)&mHandle);

	GLint oldHandle;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mHandle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_STREAM_DRAW);
	//glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT /*| GL_CLIENT_STORAGE_BIT*/);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldHandle);
}

GPUIndexBuffer::~GPUIndexBuffer()
{
	glDeleteBuffers(1, (GLuint*)&mHandle);
}

void GPUIndexBuffer::Upload(const void *data, int size)
{
	GLint oldHandle;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldHandle);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mHandle);

	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size, data);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldHandle);
}

void *GPUIndexBuffer::MapWriteOnly()
{
	GLint oldHandle;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mHandle);
	void *data = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldHandle);

	return data;
}

void GPUIndexBuffer::Unmap()
{
	GLint oldHandle;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldHandle);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mHandle);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldHandle);
}
