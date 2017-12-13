/*
** gl_swframebuffer.cpp
** Code to let ZDoom use OpenGL as a simple framebuffer
**
**---------------------------------------------------------------------------
** Copyright 1998-2011 Randy Heit
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
** This file does _not_ implement hardware-acclerated 3D rendering. It is
** just a means of getting the pixel data to the screen in a more reliable
** method on modern hardware by copying the entire frame to a texture,
** drawing that to the screen, and presenting.
**
** That said, it does implement hardware-accelerated 2D rendering.
*/

#include "gl/system/gl_system.h"
#include "m_swap.h"
#include "v_video.h"
#include "doomstat.h"
#include "m_png.h"
#include "m_crc32.h"
#include "vectors.h"
#include "v_palette.h"
#include "templates.h"

#include "c_dispatch.h"
#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "doomerrors.h"
#include "r_data/r_translate.h"
#include "f_wipe.h"
#include "sbar.h"
#include "w_wad.h"
#include "r_data/colormaps.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_swframebuffer.h"
#include "gl/data/gl_data.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/gl_functions.h"
#include "gl_debug.h"
#include "r_videoscale.h"

#include "swrenderer/scene/r_light.h"

#ifndef NO_SSE
#include <immintrin.h>
#endif

EXTERN_CVAR(Bool, fullscreen)
EXTERN_CVAR(Float, Gamma)
EXTERN_CVAR(Bool, vid_vsync)
EXTERN_CVAR(Float, transsouls)
EXTERN_CVAR(Int, vid_refreshrate)

void gl_LoadExtensions();
void gl_PrintStartupLog();

#ifndef WIN32
// This has to be in this file because system headers conflict Doom headers
DFrameBuffer *CreateGLSWFrameBuffer(int width, int height, bool bgra, bool fullscreen)
{
	return new OpenGLSWFrameBuffer(NULL, width, height, 32, 60, fullscreen, bgra);
}
#endif

OpenGLSWFrameBuffer::OpenGLSWFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen, bool bgra) :
	Super(hMonitor, width, height, bits, refreshHz, fullscreen, bgra)
{
	if (MemBuffer == nullptr)
	{
		return;
	}

	// To do: this needs to cooperate with the same static in OpenGLFrameBuffer::InitializeState
	static bool first = true;
	if (first)
	{
		if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
		{
			Printf("OpenGL load failed. No OpenGL acceleration will be used.\n");
			return;
		}
	}
	
	const char *glversion = (const char*)glGetString(GL_VERSION);
	bool isGLES = (glversion && strlen(glversion) > 10 && memcmp(glversion, "OpenGL ES ", 10) == 0);

	if (!isGLES && ogl_IsVersionGEQ(3, 0) == 0)
	{
		Printf("OpenGL acceleration requires at least OpenGL 3.0. No Acceleration will be used.\n");
		return;
	}
	gl_LoadExtensions();
	if (gl.legacyMode)
	{
		Printf("Legacy OpenGL path is active. No Acceleration will be used.\n");
		return;
	}
	InitializeState();
	if (first)
	{
		gl_PrintStartupLog();
		first = false;
	}

	if (!glGetString)
		return;

	// SetVSync needs to be at the very top to workaround a bug in Nvidia's OpenGL driver.
	// If wglSwapIntervalEXT is called after glBindFramebuffer in a frame the setting is not changed!
	Super::SetVSync(vid_vsync);

	//Debug = std::make_shared<FGLDebug>();
	//Debug->Update();

	//Windowed = !(static_cast<Win32Video *>(Video)->GoFullscreen(fullscreen));

	Init();
}

OpenGLSWFrameBuffer::~OpenGLSWFrameBuffer()
{
}


void OpenGLSWFrameBuffer::SetVSync(bool vsync)
{
	// Switch to the default frame buffer because Nvidia's driver associates the vsync state with the bound FB object.
	GLint oldDrawFramebufferBinding = 0, oldReadFramebufferBinding = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebufferBinding);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebufferBinding);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	Super::SetVSync(vsync);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFramebufferBinding);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFramebufferBinding);
}
