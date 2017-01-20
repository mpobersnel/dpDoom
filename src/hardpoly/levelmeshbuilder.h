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

#include "r_renderer.h"
#include "hardpoly/gpu/gpu_context.h"

struct LevelMeshDrawRun
{
	FTexture *Texture = nullptr;
	int Start = 0;
	int NumVertices = 0;
};

class LevelMeshBuilder
{
public:
	void Generate();
	
	GPUVertexArrayPtr VertexArray;
	std::vector<LevelMeshDrawRun> DrawRuns;
	
private:
	void ProcessBSP();
	void ProcessNode(void *node);
	void ProcessSubsector(subsector_t *sub);
	void ProcessWall(FTexture *texture, const DVector2 &v1, const DVector2 &v2, double ceilz1, double floorz1, double ceilz2, double floorz2);
	
	FTexture *GetWallTexture(line_t *line, side_t *side, side_t::ETexpart texpart);

	struct MaterialVertices
	{
		std::vector<Vec3f> Vertices;
		std::vector<Vec2f> Texcoords;
	};

	std::map<FTexture*, MaterialVertices> mMaterials;
};
