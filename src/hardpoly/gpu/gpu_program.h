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
#include <string>
#include <map>

enum class GPUShaderType
{
	Vertex,
	Fragment,
	Compute
};

class GPUProgram
{
public:
	GPUProgram();
	~GPUProgram();

	void SetDefine(const std::string &name);
	void SetDefine(const std::string &name, int value);
	void SetDefine(const std::string &name, float value);
	void SetDefine(const std::string &name, double value);
	void SetDefine(const std::string &name, const std::string &value);
	void Compile(GPUShaderType type, const char *lumpName);
	void Compile(GPUShaderType type, const char *name, const std::string &code);
	void SetFragOutput(const std::string &name, int index);
	void Link(const std::string &name);

private:
	GPUProgram(const GPUProgram &) = delete;
	GPUProgram &operator =(const GPUProgram &) = delete;

	std::string PrefixCode() const;
	std::string GetShaderInfoLog(int handle) const;
	std::string GetProgramInfoLog() const;

	int mHandle = 0;
	std::map<int, int> mShaderHandle;
	std::map<std::string, std::string> mDefines;
};

typedef std::shared_ptr<GPUProgram> GPUProgramPtr;
