/*
** win32iface.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#pragma once

#include "hardware.h"
#include "polyrenderer/hardpoly/zdframebuffer.h"

EXTERN_CVAR (Bool, vid_vsync)

struct FSoftwareRenderer;

class Win32Video : public IVideo
{
 public:
	Win32Video (int parm);
	~Win32Video ();

	void SetWindowedScale (float scale);

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool bgra, bool fs, DFrameBuffer *old);

	void StartModeIterator (int bits, bool fs);
	bool NextMode (int *width, int *height, bool *letterbox);

	bool GoFullscreen (bool yes);

 private:
	struct ModeInfo
	{
		ModeInfo() { }
		ModeInfo (int inX, int inY) : width (inX), height (inY) { }

		int width = 0;
		int height = 0;
	};

	bool InitD3D11();
	void AddMode(int x, int y);

	TArray<ModeInfo> m_Modes;

	unsigned int m_IteratorPos;
	bool m_IsFullscreen = false;
};

class BaseWinFB : public ZDFrameBuffer
{
	typedef ZDFrameBuffer Super;
public:
	BaseWinFB(int width, int height, bool bgra) : ZDFrameBuffer(width, height, bgra), Windowed(true) {}

	bool IsFullscreen () { return !Windowed; }
	int GetTrueHeight() override { return GetHeight(); }

protected:
	bool Windowed;

	friend class Win32Video;
};
