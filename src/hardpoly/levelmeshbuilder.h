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
#include <set>
#include <thread>
#include <mutex>
#include <condition_variable>

class HardpolyRenderer;
struct LevelMeshThread;

struct LevelMeshDrawRun
{
	FTexture *Texture = nullptr;
	int Start = 0;
	int NumVertices = 0;
};

struct LevelMeshVertex
{
	Vec3f Position;
	Vec4f TexCoord;
};

struct LevelMeshBatch
{
	GPUVertexArrayPtr VertexArray;
	GPUIndexBufferPtr IndexBuffer;
	std::vector<LevelMeshDrawRun> DrawRuns;
	int StartSkyIndex = 0;
	int NumSkyIndices = 0;
	GPUVertexBufferPtr Vertices;

	std::vector<LevelMeshVertex> CpuVertices;
	std::vector<int32_t> CpuIndexBuffer;
	int WrittenVertexCount = 0;
	int WrittenIndexCount = 0;
};

struct LevelMeshThread
{
	struct MaterialVertices
	{
		std::vector<int32_t> Indices;
	};

	std::map<FTexture*, MaterialVertices> mMaterials;
	std::vector<int32_t> SkyIndices;

	LevelMeshVertex *mVertices = nullptr;
	int mNextVertex = 0;
	int mNextElementIndex = 0;
	LevelMeshBatch *mCurrentBatch = nullptr;

	std::thread mThread;
	int mCore = 0;
	int mNumCores = 1;

	std::vector<std::unique_ptr<LevelMeshBatch>> mCurrentFrameBatches;
	std::vector<std::unique_ptr<LevelMeshBatch>> mLastFrameBatches;
	size_t mNextBatch = 0;
};

class LevelMeshBuilder
{
public:
	LevelMeshBuilder();
	~LevelMeshBuilder();

	void Render(HardpolyRenderer *renderer, const std::vector<subsector_t*> &subsectors, const std::vector<sector_t *> &seenSectors, float skyZCeiling, float skyZFloor);

private:
	void WorkerMain(LevelMeshThread *thread);
	void RenderThread(LevelMeshThread *thread);
	void Flush(LevelMeshThread *thread);
	void ProcessPlanes(LevelMeshThread *thread, subsector_t *sub);
	void ProcessOpaquePlane(LevelMeshThread *thread, subsector_t *sub, bool ceiling, FTexture *texture);
	void ProcessSkyPlane(LevelMeshThread *thread, subsector_t *sub, bool ceiling);
	void ProcessSkyWalls(LevelMeshThread *thread, subsector_t *sub, bool ceiling);
	void ProcessLines(LevelMeshThread *thread, sector_t *sector);
	void ProcessWall(LevelMeshThread *thread, float sectornum, FTexture *texture, const line_t *line, const side_t *side, side_t::ETexpart texpart, double ceilz1, double floorz1, double ceilz2, double floorz2, double unpeggedceil1, double unpeggedceil2, double topTexZ, double bottomTexZ, bool masked);
	static void ClampWallHeight(Vec3f &v1, Vec3f &v2, Vec4f &uv1, Vec4f &uv2);

	void GetVertices(LevelMeshThread *thread, int numVertices, int numIndices);
	void CreateGPUObjects(LevelMeshBatch *batch);

	FTexture *GetWallTexture(line_t *line, side_t *side, side_t::ETexpart texpart);

	LevelMeshThread mMainThread;
	std::vector<LevelMeshThread> mWorkerThreads;
	std::condition_variable mStartCondition;
	std::condition_variable mEndCondition;
	std::mutex mMutex;
	bool mStopFlag = false;
	size_t mStartThreadCount = 0;
	size_t mEndThreadCount = 0;

	const std::vector<subsector_t*> *subsectors;
	const std::vector<sector_t *> *sectors;
	float mSkyZCeiling = 0.0f, mSkyZFloor = 0.0f;

	enum { MaxVertices = 16*1024, MaxIndices = 3*16*1024 };

	HardpolyRenderer *mRenderer;

	int mTotalDrawRuns = 0;

	LevelMeshBuilder(const LevelMeshBuilder &) = delete;
	LevelMeshBuilder &operator=(const LevelMeshBuilder &) = delete;
};

class PlaneUVTransform
{
public:
	PlaneUVTransform(const FTransform &transform, FTexture *tex);

	Vec2f GetUV(const Vec3f &pos) const;

	float GetU(float x, float y) const;
	float GetV(float x, float y) const;

private:
	float xscale;
	float yscale;
	float cosine;
	float sine;
	float xOffs, yOffs;
};

class WallTextureCoordsU
{
public:
	WallTextureCoordsU(FTexture *tex, const seg_t *lineseg, const line_t *line, const side_t *side, side_t::ETexpart texpart);
	WallTextureCoordsU(FTexture *tex, const line_t *line, const side_t *side, side_t::ETexpart texpart);

	double u1, u2;
};

class WallTextureCoordsV
{
public:
	WallTextureCoordsV(FTexture *tex, const line_t *line, const side_t *side, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil, double topTexZ, double bottomTexZ);

	double v1, v2;

private:
	void CalcVTopPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double vscale, double yoffset);
	void CalcVMidPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double vscale, double yoffset);
	void CalcVBottomPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double unpeggedceil, double vscale, double yoffset);
};
