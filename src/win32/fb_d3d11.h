
#pragma once

#include "polyrenderer/hardpoly/d3d11_context.h"

class D3D11FB : public BaseWinFB
{
public:
	D3D11FB(int width, int height, bool bgra, bool fullscreen);
	~D3D11FB();

	// BaseWinFB interface (legacy junk we don't need to support):
	bool IsValid() override { return true; }

	// DFrameBuffer interface:
	bool IsOpenGL() const override { return false; }
	bool Lock(bool buffered) override;
	void Unlock() override;
	bool Begin2D(bool copy3d) override;
	void Update() override;
	PalEntry *GetPalette() override;
	void GetFlashedPalette(PalEntry palette[256]) override;
	void UpdatePalette() override;
	bool SetGamma(float gamma) override;
	bool SetFlash(PalEntry rgb, int amount) override;
	void GetFlash(PalEntry &rgb, int &amount) override;
	int GetPageCount() override { return 2; }
	// void SetVSync(bool vsync) override;
	// void NewRefreshRate() override;
	// void SetBlendingRect(int x1, int y1, int x2, int y2) override;
	// void DrawBlendingRect() override;
	// FNativeTexture *CreateTexture(FTexture *gametex, bool wrapping) override;
	// FNativePalette *CreatePalette(FRemapTable *remap) override;
	// void GameRestart() override;
	// bool WipeStartScreen(int type) override;
	// void WipeEndScreen() override;
	// bool WipeDo(int ticks) override;
	// void WipeCleanup() override;

	GPUContext *GetContext() override { return &mContext; }

private:
	void SetInitialWindowLocation();
	void CreateFBTexture();
	void CreateFBStagingTexture();

	PalEntry mPalette[256];
	PalEntry mFlashColor = 0;
	int mFlashAmount = 0;

	float mGamma = 1.0f;
	float mLastGamma = 0.0f;

	D3D11Context mContext;
	std::shared_ptr<GPUTexture2D> mFBTexture;
	std::shared_ptr<GPUStagingTexture> mFBStaging;
};

typedef HRESULT(WINAPI *FuncD3D11CreateDeviceAndSwapChain)(
	__in_opt IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	__in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	__in_opt CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	__out_opt IDXGISwapChain** ppSwapChain,
	__out_opt ID3D11Device** ppDevice,
	__out_opt D3D_FEATURE_LEVEL* pFeatureLevel,
	__out_opt ID3D11DeviceContext** ppImmediateContext);

typedef HRESULT(WINAPI *FuncCreateDXGIFactory)(REFIID riid, _COM_Outptr_ void **ppFactory);

extern HMODULE D3D11_dll, DXGI_dll;
extern FuncD3D11CreateDeviceAndSwapChain D3D11_createdeviceandswapchain;
extern FuncCreateDXGIFactory D3D11_createdxgifactory;
