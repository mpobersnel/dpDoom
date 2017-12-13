/*
** win32video.cpp
** Code to let ZDoom draw to the screen
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifdef _DEBUG
#define D3D_DEBUG_INFO
#endif
#define DIRECTDRAW_VERSION 0x0300
#define DIRECT3D_VERSION 0x0900

#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <ctype.h>

#include "doomtype.h"

#include "c_dispatch.h"
#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "doomerrors.h"
#include "m_argv.h"
#include "r_defs.h"
#include "v_text.h"
#include "swrenderer/r_swrenderer.h"
#include "version.h"

#include "win32iface.h"

#include "optwin32.h"
#include "fb_d3d11.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void StopFPSLimit();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern IVideo *Video;
extern BOOL AppActive;
extern int SessionState;
extern bool FullscreenReset;
extern bool VidResizing;

EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Bool, cl_capfps)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static UINT FPSLimitTimer;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

HANDLE FPSLimitEvent;

CVAR (Int, vid_adapter, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR (Int, vid_maxfps, 200, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (vid_maxfps < TICRATE && vid_maxfps != 0)
	{
		vid_maxfps = TICRATE;
	}
	else if (vid_maxfps > 1000)
	{
		vid_maxfps = 1000;
	}
	else if (cl_capfps == 0)
	{
		I_SetFPSLimit(vid_maxfps);
	}
}

// CODE --------------------------------------------------------------------

Win32Video::Win32Video (int parm)
{
	I_SetWndProc();
	InitD3D11();
}

Win32Video::~Win32Video ()
{
}

bool Win32Video::InitD3D11()
{
	if ((D3D11_dll = LoadLibraryW(L"d3d11.dll")) == 0)
	{
		return false;
	}

	if ((DXGI_dll = LoadLibraryW(L"dxgi.dll")) == 0)
	{
		FreeLibrary(D3D11_dll);
		return false;
	}

	D3D11_createdeviceandswapchain = reinterpret_cast<FuncD3D11CreateDeviceAndSwapChain>(GetProcAddress(D3D11_dll, "D3D11CreateDeviceAndSwapChain"));
	if (D3D11_createdeviceandswapchain == 0)
	{
		I_FatalError("GetProcAddress(D3D11CreateDeviceAndSwapChain) failed\n");
		return false;
	}

	D3D11_createdxgifactory = reinterpret_cast<FuncCreateDXGIFactory>(GetProcAddress(DXGI_dll, "CreateDXGIFactory"));
	if (D3D11_createdxgifactory == 0)
	{
		I_FatalError("GetProcAddress(CreateDXGIFactory) failed\n");
	}

	ComPtr<IDXGIFactory> factory;
	HRESULT result = D3D11_createdxgifactory(__uuidof(IDXGIFactory), (void**)factory.OutputVariable());
	if (FAILED(result))
	{
		I_FatalError("CreateDXGIFactory failed\n");
	}

	// Grab primary screen adapter
	ComPtr<IDXGIAdapter> adapter;
	result = factory->EnumAdapters(0, adapter.OutputVariable());
	if (FAILED(result))
	{
		I_FatalError("IDXGIAdapter.EnumAdapters failed\n");
	}

	// And the output used for the primary screen, unless overriden by vid_adapter
	ComPtr<IDXGIOutput> output;
	result = adapter->EnumOutputs(vid_adapter - 1, output.OutputVariable());
	if (result == DXGI_ERROR_NOT_FOUND)
		result = adapter->EnumOutputs(0, output.OutputVariable());

	// Fetch mode list
	while (true)
	{
		UINT numModes = 0;
		result = output->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &numModes, nullptr);
		if (result == DXGI_ERROR_MORE_DATA)
			continue;
		if (FAILED(result))
			I_FatalError("GetDisplayModeList failed\n");

		std::vector<DXGI_MODE_DESC> descs(numModes);
		result = output->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &numModes, descs.data());
		if (result == DXGI_ERROR_MORE_DATA)
			continue;
		if (FAILED(result))
			I_FatalError("GetDisplayModeList failed\n");

		for (const auto &desc : descs)
		{
			AddMode(desc.Width, desc.Height);
		}

		break;
	}

	return true;
}

// Returns true if fullscreen, false otherwise
bool Win32Video::GoFullscreen (bool yes)
{
	return yes;
}

void Win32Video::AddMode (int x, int y)
{
	// Reject modes that do not meet certain criteria.
	if ((x & 1) != 0 ||
		y > MAXHEIGHT ||
		x > MAXWIDTH ||
		y < 200 ||
		x < 320)
	{
		return;
	}

	// This mode may have been already added to the list because it is
	// enumerated multiple times at different refresh rates. If it's
	// not present, add it to the right spot in the list; otherwise, do nothing.
	// Modes are sorted first by width, then by height, then by depth. In each
	// case the order is ascending.
	unsigned int i;
	for (i = 0; i < m_Modes.Size(); i++)
	{
		const auto &probe = m_Modes[i];
		if (probe.width > x)		break;
		if (probe.width < x)		continue;
		// Width is equal
		if (probe.height > y)		break;
		if (probe.height < y)		continue;
		// Height is equal
		return;
	}

	m_Modes.Insert(i, ModeInfo(x, y));
}

void Win32Video::StartModeIterator (int, bool)
{
	m_IteratorPos = 0;
}

bool Win32Video::NextMode (int *width, int *height, bool *letterbox)
{
	if (m_IteratorPos < m_Modes.Size())
	{
		*width = m_Modes[m_IteratorPos].width;
		*height = m_Modes[m_IteratorPos].height;
		if (letterbox)
			*letterbox = false; // To do: remove this feature completely
		m_IteratorPos++;
		return true;
	}
	return false;
}

DFrameBuffer *Win32Video::CreateFrameBuffer (int width, int height, bool bgra, bool fullscreen, DFrameBuffer *old)
{
	BaseWinFB *fb;
	PalEntry flashColor;
	int flashAmount;

	if (fullscreen)
	{
		I_ClosestResolution(&width, &height, 32);
	}

	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		BaseWinFB *fb = static_cast<BaseWinFB *> (old);
		if (fb->Width == width &&
			fb->Height == height &&
			fb->Windowed == !fullscreen &&
			fb->Bgra == bgra)
		{
			return old;
		}
		old->GetFlash (flashColor, flashAmount);
		if (old == screen) screen = nullptr;
		delete old;
	}
	else
	{
		flashColor = 0;
		flashAmount = 0;
	}

	if (D3D11_createdeviceandswapchain)
	{
		fb = new D3D11FB (width, height, bgra, fullscreen);
	}

	if (fb == nullptr || !fb->IsValid())
	{
		I_FatalError("Could not create new screen (%d x %d): %08lx", width, height);
	}

	fb->SetFlash (flashColor, flashAmount);
	return fb;
}

void Win32Video::SetWindowedScale (float scale)
{
	// FIXME
}

//==========================================================================
//
// SetFPSLimit
//
// Initializes an event timer to fire at a rate of <limit>/sec. The video
// update will wait for this timer to trigger before updating.
//
// Pass 0 as the limit for unlimited.
// Pass a negative value for the limit to use the value of vid_maxfps.
//
//==========================================================================

void I_SetFPSLimit(int limit)
{
	if (limit < 0)
	{
		limit = vid_maxfps;
	}
	// Kill any leftover timer.
	if (FPSLimitTimer != 0)
	{
		timeKillEvent(FPSLimitTimer);
		FPSLimitTimer = 0;
	}
	if (limit == 0)
	{ // no limit
		if (FPSLimitEvent != NULL)
		{
			CloseHandle(FPSLimitEvent);
			FPSLimitEvent = NULL;
		}
		DPrintf(DMSG_NOTIFY, "FPS timer disabled\n");
	}
	else
	{
		if (FPSLimitEvent == NULL)
		{
			FPSLimitEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
			if (FPSLimitEvent == NULL)
			{ // Could not create event, so cannot use timer.
				Printf(DMSG_WARNING, "Failed to create FPS limitter event\n");
				return;
			}
		}
		atterm(StopFPSLimit);
		// Set timer event as close as we can to limit/sec, in milliseconds.
		UINT period = 1000 / limit;
		FPSLimitTimer = timeSetEvent(period, 0, (LPTIMECALLBACK)FPSLimitEvent, 0, TIME_PERIODIC | TIME_CALLBACK_EVENT_SET);
		if (FPSLimitTimer == 0)
		{
			CloseHandle(FPSLimitEvent);
			FPSLimitEvent = NULL;
			Printf("Failed to create FPS limiter timer\n");
			return;
		}
		DPrintf(DMSG_NOTIFY, "FPS timer set to %u ms\n", period);
	}
}

//==========================================================================
//
// StopFPSLimit
//
// Used for cleanup during application shutdown.
//
//==========================================================================

static void StopFPSLimit()
{
	I_SetFPSLimit(0);
}

void I_FPSLimit()
{
	if (FPSLimitEvent != NULL)
	{
		WaitForSingleObject(FPSLimitEvent, 1000);
	}
}