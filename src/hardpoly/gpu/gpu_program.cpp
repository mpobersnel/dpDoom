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
#include "gpu_program.h"
#include "gl/system/gl_system.h"
#include "w_wad.h"

GPUProgram::GPUProgram()
{
}

GPUProgram::~GPUProgram()
{
	if (mHandle)
		glDeleteProgram(mHandle);
	for (auto it : mShaderHandle)
		glDeleteShader(it.second);
}

void GPUProgram::SetDefine(const std::string &name)
{
	SetDefine(name, "1");
}

void GPUProgram::SetDefine(const std::string &name, int value)
{
	SetDefine(name, std::to_string(value));
}

void GPUProgram::SetDefine(const std::string &name, float value)
{
	SetDefine(name, std::to_string(value));
}

void GPUProgram::SetDefine(const std::string &name, double value)
{
	SetDefine(name, std::to_string(value));
}

void GPUProgram::SetDefine(const std::string &name, const std::string &value)
{
	if (!value.empty())
	{
		mDefines[name] = value;
	}
	else
	{
		auto it = mDefines.find(name);
		if (it != mDefines.end())
			mDefines.erase(it);
	}
}

std::string GPUProgram::PrefixCode() const
{
	std::string prefix = "#version 430\n";
	for (auto it : mDefines)
	{
		prefix += "#define " + it.first + " " + it.second + "\n";
	}
	prefix += "#line 1\n";
	return prefix;
}

void GPUProgram::Compile(GPUShaderType type, const char *lumpName)
{
	int lump = Wads.CheckNumForFullName(lumpName);
	if (lump == -1) I_FatalError("Unable to load '%s'", lumpName);
	FString code = Wads.ReadLump(lump).GetString().GetChars();
	Compile(type, lumpName, code.GetChars());
}

void GPUProgram::Compile(GPUShaderType type, const char *name, const std::string &code)
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

void GPUProgram::SetFragOutput(const std::string &name, int index)
{
	glBindFragDataLocation(mHandle, index, name.c_str());
}

void GPUProgram::Link(const std::string &name)
{
	glLinkProgram(mHandle);

	GLint status = 0;
	glGetProgramiv(mHandle, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		I_FatalError("Link Shader '%s':\n%s\n", name.c_str(), GetProgramInfoLog().c_str());
	}
}

std::string GPUProgram::GetShaderInfoLog(int handle) const
{
	static char buffer[10000];
	GLsizei length = 0;
	buffer[0] = 0;
	glGetShaderInfoLog(handle, 10000, &length, buffer);
	return buffer;
}

std::string GPUProgram::GetProgramInfoLog() const
{
	static char buffer[10000];
	GLsizei length = 0;
	buffer[0] = 0;
	glGetProgramInfoLog(mHandle, 10000, &length, buffer);
	return buffer;
}
