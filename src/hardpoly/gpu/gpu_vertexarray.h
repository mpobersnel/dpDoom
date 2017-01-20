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
#include <cstddef>
#include "gpu_vertexbuffer.h"

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
	GPUVertexAttributeDesc(int index, int size, GPUVertexAttributeType type, bool normalized, int stride, std::size_t offset, GPUVertexBufferPtr buffer)
		: Index(index), Size(size), Type(type), Normalized(normalized), Stride(stride), Offset(offset), Buffer(buffer) { }

	int Index;
	int Size;
	GPUVertexAttributeType Type;
	bool Normalized;
	int Stride;
	std::size_t Offset;
	GPUVertexBufferPtr Buffer;
};

class GPUVertexArray
{
public:
	GPUVertexArray(const std::vector<GPUVertexAttributeDesc> &attributes);
	~GPUVertexArray();

	int Handle() const { return mHandle; }

private:
	GPUVertexArray(const GPUVertexArray &) = delete;
	GPUVertexArray &operator =(const GPUVertexArray &) = delete;

	static int FromType(GPUVertexAttributeType type);

	int mHandle = 0;
	std::vector<GPUVertexAttributeDesc> mAttributes;
};

typedef std::shared_ptr<GPUVertexArray> GPUVertexArrayPtr;
