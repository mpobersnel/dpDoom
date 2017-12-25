
#pragma once

#include "polyrenderer/hardpoly/d3d11_context.h"

class D3D11FB : public BaseWinFB
{
public:
	D3D11FB(int width, int height, bool bgra, bool fullscreen);
	~D3D11FB();

	GPUContext *GetContext() override { return &mContext; }
	void SwapBuffers() override;

	int GetClientWidth() override;
	int GetClientHeight() override;

private:
	void SetInitialWindowLocation();

	D3D11Context mContext;
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
