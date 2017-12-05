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
#include "doomtype.h"
#include "i_system.h"
#include "w_wad.h"
#include <string>

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

void GPUProgram::Compile(GPUShaderType type, const char *lumpName)
{
	int lump = Wads.CheckNumForFullName(lumpName);
	if (lump == -1) I_FatalError("Unable to load '%s'", lumpName);
	FString code = Wads.ReadLump(lump).GetString().GetChars();
	Compile(type, lumpName, code.GetChars());
}
