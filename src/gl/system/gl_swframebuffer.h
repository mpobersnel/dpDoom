#ifndef __GL_SWFRAMEBUFFER
#define __GL_SWFRAMEBUFFER

#ifdef _WIN32
#include "win32iface.h"
#include "win32gliface.h"
#endif

#include "SkylineBinPack.h"
#include "textures.h"

#include "polyrenderer/hardpoly/gl_context.h"
#include "polyrenderer/hardpoly/zdframebuffer.h"

#include <memory>

class FGLDebug;

#ifdef _WIN32
class OpenGLSWFrameBuffer : public Win32GLFrameBuffer
{
	typedef Win32GLFrameBuffer Super;
#else
#include "sdlglvideo.h"
class OpenGLSWFrameBuffer : public SDLGLFB
{
	typedef SDLGLFB Super;	//[C]commented, DECLARE_CLASS defines this in linux
#endif


public:

	OpenGLSWFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen, bool bgra);
	~OpenGLSWFrameBuffer();

	void SetVSync(bool vsync) override;
	GPUContext *GetContext() override { return &mContext; }

private:
	GLContext mContext;
	std::shared_ptr<FGLDebug> Debug;
};


#endif //__GL_SWFRAMEBUFFER
