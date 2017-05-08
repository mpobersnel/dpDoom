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
#include "hardpoly/playersprite.h"
#include "hardpoly/bspcull.h"
#include <set>

struct LevelMeshDrawRun;
struct subsector_t;

class HardpolyRenderer : public FRenderer
{
public:
	HardpolyRenderer();
	~HardpolyRenderer();

	void Precache(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist) override;
	void RenderView(player_t *player) override;
	void RemapVoxels() override;
	void WriteSavePic (player_t *player, FileWriter *file, int width, int height) override;
	void DrawRemainingPlayerSprites() override;
	int GetMaxViewPitch(bool down) override;
	bool RequireGLNodes() override;
	void OnModeSet () override;
	void Init() override;
	void RenderTextureView (FCanvasTexture *tex, AActor *viewpoint, int fov) override;
	void PreprocessLevel() override;
	void CleanLevelData() override;
	void SetClearColor(int color) override;
	
private:
	struct FrameUniforms
	{
		Mat4f WorldToView;
		Mat4f ViewToProjection;
		float MeshId;
		float Padding1, Padding2, Padding3;
	};
	
	void SetupFramebuffer();
	void SetupStaticLevelMesh();
	void SetupPerspectiveMatrix(float meshId);
	void CompileShaders();
	void CreateSamplers();
	void UploadSectorTexture();
	void RenderBspMesh();
	void RenderDynamicMesh();
	void RenderLevelMesh(const GPUVertexArrayPtr &vertexArray, const std::vector<LevelMeshDrawRun> &drawRuns, float meshId);
	GPUTexture2DPtr GetTexture(FTexture *texture);

	GPUContextPtr mContext;
	GPUTexture2DPtr mAlbedoBuffer;
	GPUTexture2DPtr mDepthStencilBuffer;
	GPUTexture2DPtr mNormalBuffer;
	GPUFrameBufferPtr mSceneFB;
	GPUUniformBufferPtr mFrameUniforms[3];
	int mCurrentFrameUniforms = 0;
	int mMeshLevel = 0;
	GPUVertexArrayPtr mVertexArray;
	std::vector<LevelMeshDrawRun> mDrawRuns;
	std::map<FTexture*, GPUTexture2DPtr> mTextures;
	std::vector<Vec4f> cpuSectors;
	std::vector<double> cpuStaticSectorCeiling;
	std::vector<double> cpuStaticSectorFloor;
	GPUTexture2DPtr mSectorTexture[3];
	int mCurrentSectorTexture = 0;
	GPUProgramPtr mProgram;
	GPUSamplerPtr mSamplerLinear;
	GPUSamplerPtr mSamplerNearest;

	GPUVertexArrayPtr mDynamicVertexArray;
	std::vector<LevelMeshDrawRun> mDynamicDrawRuns;
	std::set<sector_t*> dynamicSectors;
	std::vector<subsector_t*> dynamicSubsectors;
	HardpolyRenderPlayerSprites mPlayerSprites;
	HardpolyBSPCull mBspCull;
};
