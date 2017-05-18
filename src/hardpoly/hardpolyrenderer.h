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
#include "hardpoly/levelmeshbuilder.h"
#include "swrenderer/r_memory.h"
#include <set>
#include <unordered_map>

struct LevelMeshDrawRun;
struct subsector_t;
struct particle_t;
class HardpolyRenderer;

class HardpolyTranslucentObject
{
public:
	virtual void Setup(HardpolyRenderer *renderer) = 0;
	virtual void Render(HardpolyRenderer *renderer) = 0;

	bool operator<(const HardpolyTranslucentObject &other) const
	{
		return subsectorDepth != other.subsectorDepth ? subsectorDepth < other.subsectorDepth : DistanceSquared < other.DistanceSquared;
	}

	uint32_t subsectorDepth = 0;
	double DistanceSquared = 0.0;
};

/*
class HardpolyRenderParticle : public HardpolyTranslucentObject
{
public:
	HardpolyRenderParticle(particle_t *particle, subsector_t *sub, uint32_t initSubsectorDepth) : particle(particle), sub(sub) { subsectorDepth = initSubsectorDepth; }

	particle_t *particle = nullptr;
	subsector_t *sub = nullptr;
};

class HardpolyTranslucentWall : public HardpolyTranslucentObject
{
public:
	HardpolyTranslucentWall(HardpolyRenderWall wall) : wall(wall), subsectorDepth(wall.SubsectorDepth), DistanceSquared(1.e6) { }

	HardpolyRenderWall wall;
};
*/

struct TranslucentVertex
{
	Vec4f Position;
	Vec2f UV;
	float LightLevel;
	float Padding;
};

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

	GPUTexture2DPtr GetTexture(FTexture *texture);

	std::vector<TranslucentVertex> TranslucentVertices;
	GPUContextPtr mContext;

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
	void UpdateAutoMap();
	void RenderLevelMesh(const GPUVertexArrayPtr &vertexArray, const GPUIndexBufferPtr &indexBuffer, const std::vector<LevelMeshDrawRun> &drawRuns, float meshId);

	void RenderTranslucent();
	void RenderSprite(AActor *thing, double sortDistance, const DVector2 &left, const DVector2 &right);
	void RenderSprite(AActor *thing, double sortDistance, DVector2 left, DVector2 right, double t1, double t2, void *node);

	RenderMemory FrameMemory;

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
	GPUProgramPtr mTranslucentProgram;
	GPUSamplerPtr mSamplerLinear;
	GPUSamplerPtr mSamplerNearest;

	GPUVertexArrayPtr mDynamicVertexArray;
	std::vector<LevelMeshDrawRun> mDynamicDrawRuns;
	std::set<sector_t*> dynamicSectors;
	std::vector<subsector_t*> dynamicSubsectors;
	HardpolyRenderPlayerSprites mPlayerSprites;
	HardpolyBSPCull mBspCull;
	LevelMeshBuilder mBspMesh;

	std::set<sector_t *> SeenSectors;
	std::unordered_map<subsector_t *, uint32_t> SubsectorDepths;
	std::vector<HardpolyTranslucentObject *> TranslucentObjects;
};
