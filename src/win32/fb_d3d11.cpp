
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include "doomtype.h"
#include "c_dispatch.h"
#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "i_input.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "doomerrors.h"
#include "r_data/r_translate.h"
#include "f_wipe.h"
#include "sbar.h"
#include "win32iface.h"
#include "doomstat.h"
#include "v_palette.h"
#include "w_wad.h"
#include "textures.h"
#include "r_data/colormaps.h"
#include "SkylineBinPack.h"
#include "swrenderer/scene/r_light.h"
#include "fb_d3d11.h"
#include "r_videoscale.h"

extern HWND Window;
extern IVideo *Video;
extern BOOL AppActive;
extern int SessionState;
extern bool VidResizing;

CVAR(Bool, vid_palettehack, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vid_noblitter, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, vid_displaybits, 8, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Float, rgamma, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma(Gamma);
	}
}
CUSTOM_CVAR(Float, ggamma, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma(Gamma);
	}
}
CUSTOM_CVAR(Float, bgamma, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma(Gamma);
	}
}

cycle_t BlitCycles;

ADD_STAT(blit)
{
	FString out;
	out.Format("blit=%04.1f ms", BlitCycles.TimeMS());
	return out;
}

HMODULE D3D11_dll, DXGI_dll;
FuncD3D11CreateDeviceAndSwapChain D3D11_createdeviceandswapchain;
FuncCreateDXGIFactory D3D11_createdxgifactory;


D3D11FB::D3D11FB(int width, int height, bool bgra, bool fullscreen) : BaseWinFB(width, height, bgra)
{
	std::vector<D3D_FEATURE_LEVEL> requestLevels =
	{
		//D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;// DXGI_FORMAT_R8G8B8A8_UNORM; // DXGI_FORMAT_B8G8R8A8_UNORM (except 10.x on Windows Vista)
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = Window;
	swapChainDesc.Windowed = TRUE; // Seems the MSDN documentation wants us to call IDXGISwapChain::SetFullscreenState afterwards
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	HRESULT result = D3D11_createdeviceandswapchain(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, requestLevels.data(), (UINT)requestLevels.size(), D3D11_SDK_VERSION, &swapChainDesc, mContext.SwapChain.OutputVariable(), mContext.Device.OutputVariable(), &mContext.FeatureLevel, mContext.DeviceContext.OutputVariable());
	if (FAILED(result))
		I_FatalError("D3D11CreateDeviceAndSwapChain failed");

	Init();
	SetInitialWindowLocation();

	if (!Windowed)
		mContext.SwapChain->SetFullscreenState(TRUE, nullptr);
}

D3D11FB::~D3D11FB()
{
}

void D3D11FB::SwapBuffers()
{
	I_FPSLimit();
	mContext.SwapChain->Present(1, 0);
}

int D3D11FB::GetClientWidth()
{
	RECT rect = { 0 };
	GetClientRect(Window, &rect);
	return rect.right - rect.left;
}

int D3D11FB::GetClientHeight()
{
	RECT rect = { 0 };
	GetClientRect(Window, &rect);
	return rect.bottom - rect.top;
}

#if 0
bool D3D11FB::Lock(bool buffered)
{
	if (LockCount++ > 0) return false;

	int pixelsize = IsBgra() ? 4 : 1;
	Buffer = (uint8_t*)mFBStaging->Map();
	Pitch = mFBStaging->GetMappedRowPitch() / pixelsize;
	return true;
}

void D3D11FB::Unlock()
{
	if (LockCount == 0) return;
	if (--LockCount == 0)
	{
		Buffer = nullptr;
		mFBStaging->Unmap();
	}
}

bool D3D11FB::Begin2D(bool copy3d)
{
	// To do: call Update if Lock was called
	return BaseWinFB::Begin2D(copy3d);
}

void D3D11FB::Update()
{
	DrawRateStuff();

	bool isLocked = LockCount > 0;
	if (isLocked)
	{
		Buffer = nullptr;
		mFBStaging->Unmap();
	}

	int pixelsize = IsBgra() ? 4 : 1;
	mContext.CopyTexture(mFBTexture, mFBStaging);

	ComPtr<ID3D11Texture2D> backbuffer;
	HRESULT result = mContext.SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backbuffer.OutputVariable());
	if (SUCCEEDED(result))
		mContext.DeviceContext->CopyResource(backbuffer, std::static_pointer_cast<D3D11Texture2D>(mFBTexture)->Handle());
	backbuffer.Clear();

	// Limiting the frame rate is as simple as waiting for the timer to signal this event.
	I_FPSLimit();
	mContext.SwapChain->Present(1, 0);

	if (Windowed)
	{
		RECT box;
		GetClientRect(Window, &box);
		int clientWidth = ViewportScaledWidth(box.right, box.bottom);
		int clientHeight = ViewportScaledHeight(box.right, box.bottom);
		if (clientWidth > 0 && clientHeight > 0 && (Width != clientWidth || Height != clientHeight))
		{
			Resize(clientWidth, clientHeight);

			result = mContext.SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
			if (FAILED(result))
				I_FatalError("ResizeBuffers failed");
			CreateFBTexture();
			CreateFBStagingTexture();

			V_OutputResized(Width, Height);
		}
	}

	if (isLocked)
	{
		Buffer = (uint8_t*)mFBStaging->Map();
		Pitch = mFBStaging->GetMappedRowPitch() / pixelsize;
	}
}

void D3D11FB::CreateFBTexture()
{
	mFBTexture = mContext.CreateTexture2D(Width, Height, false, 1, IsBgra() ? GPUPixelFormat::BGRA8 : GPUPixelFormat::R8);
}

void D3D11FB::CreateFBStagingTexture()
{
	mFBStaging = mContext.CreateStagingTexture(Width, Height, IsBgra() ? GPUPixelFormat::BGRA8 : GPUPixelFormat::R8);
}
#endif

void D3D11FB::SetInitialWindowLocation()
{
	RECT r;
	LONG style, exStyle;

	ShowWindow(Window, SW_SHOW);
	GetWindowRect(Window, &r);
	style = WS_VISIBLE | WS_CLIPSIBLINGS;
	exStyle = 0;

	if (!Windowed)
	{
		style |= WS_POPUP;
	}
	else
	{
		style |= WS_OVERLAPPEDWINDOW;
		exStyle |= WS_EX_WINDOWEDGE;
	}

	SetWindowLong(Window, GWL_STYLE, style);
	SetWindowLong(Window, GWL_EXSTYLE, exStyle);
	SetWindowPos(Window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	if (!Windowed)
	{
		int monX = 0;
		int monY = 0;
		MoveWindow(Window, monX, monY, Width, GetTrueHeight(), FALSE);
	}
	else
	{
		RECT windowRect;
		windowRect.left = r.left;
		windowRect.top = r.top;
		windowRect.right = windowRect.left + Width;
		windowRect.bottom = windowRect.top + Height;
		AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);
		MoveWindow(Window, windowRect.left, windowRect.top, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, FALSE);

		I_RestoreWindowedPos();
	}
}
