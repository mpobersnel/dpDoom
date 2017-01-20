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
#include "gpu_vertexarray.h"
#include "gl/system/gl_system.h"

GPUVertexArray::GPUVertexArray(const std::vector<GPUVertexAttributeDesc> &attributes) : mAttributes(attributes)
{
	glGenVertexArrays(1, (GLuint*)&mHandle);

	GLint oldHandle, oldArrayBinding;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldHandle);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldArrayBinding);
	glBindVertexArray(mHandle);

	for (const auto &attr : attributes)
	{
		glBindBuffer(GL_ARRAY_BUFFER, attr.Buffer->Handle());
		glEnableVertexAttribArray(attr.Index);
		glVertexAttribPointer(attr.Index, attr.Size, FromType(attr.Type), attr.Normalized ? GL_TRUE : GL_FALSE, attr.Stride, (const GLvoid *)attr.Offset);
	}

	glBindBuffer(GL_ARRAY_BUFFER, oldArrayBinding);
	glBindVertexArray(oldHandle);
}

GPUVertexArray::~GPUVertexArray()
{
	glDeleteVertexArrays(1, (GLuint*)&mHandle);
}

int GPUVertexArray::FromType(GPUVertexAttributeType type)
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
