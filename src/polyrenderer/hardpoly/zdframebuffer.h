
#pragma once

#include <memory>
#include "gpu_context.h"
#include "Rect.h"
#include "SkylineBinPack.h"
#include "textures/textures.h"

class ZDFrameBuffer : public DFrameBuffer
{
public:
	ZDFrameBuffer(int width, int height, bool bgra);
	~ZDFrameBuffer();

	void Init();

	bool IsValid() override { return true; }
	bool Lock(bool buffered) override;
	void Unlock() override;
	void Update() override;
	PalEntry *GetPalette() override;
	void GetFlashedPalette(PalEntry palette[256]) override;
	void UpdatePalette() override;
	bool SetGamma(float gamma) override;
	bool SetFlash(PalEntry rgb, int amount) override;
	void GetFlash(PalEntry &rgb, int &amount) override;
	int GetPageCount() override { return 2; }
	void NewRefreshRate() override { }
	void GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma) override;
	void ReleaseScreenshotBuffer() override;
	void SetBlendingRect(int x1, int y1, int x2, int y2) override;
	bool Begin2D(bool copy3d) override;
	void DrawBlendingRect() override;
	FNativeTexture *CreateTexture(FTexture *gametex, bool wrapping) override;
	FNativePalette *CreatePalette(FRemapTable *remap) override;
	void DrawTextureParms(FTexture *img, DrawParms &parms) override;
	void DoClear(int left, int top, int right, int bottom, int palcolor, uint32_t color) override;
	void DoDim(PalEntry color, float amount, int x1, int y1, int w, int h) override;
	void FlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin) override;
	void DrawLine(int x0, int y0, int x1, int y1, int palColor, uint32_t realcolor) override;
	void DrawPixel(int x, int y, int palcolor, uint32_t rgbcolor) override;
	void FillSimplePoly(FTexture *tex, FVector2 *points, int npoints, double originx, double originy, double scalex, double scaley, DAngle rotation, const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip) override;
	bool WipeStartScreen(int type) override;
	void WipeEndScreen() override;
	bool WipeDo(int ticks) override;
	void WipeCleanup() override;

	void ScaleCoordsFromWindow(int16_t &x, int16_t &y);

	virtual int GetTrueHeight() = 0;
	virtual int GetClientWidth() = 0;
	virtual int GetClientHeight() = 0;
	virtual void SwapBuffers() = 0;

	std::shared_ptr<GPUFrameBuffer> OutputFB;
	std::shared_ptr<GPUTexture2D> OutputTexture;

private:
	typedef DFrameBuffer Super;

	struct FBVERTEX
	{
		float x, y, z, rhw;
		uint32_t color0, color1;
		float tu, tv;
	};

	struct BURNVERTEX
	{
		float x, y, z, rhw;
		float tu0, tv0;
		float tu1, tv1;
	};

	struct ShaderUniforms
	{
		Vec4f Desaturation;
		Vec4f PaletteMod;
		Vec4f Weights;
		Vec4f Gamma;
		Vec4f ScreenSize;
	};

	struct GammaRamp
	{
		uint16_t red[256], green[256], blue[256];
	};

	struct LTRBRect
	{
		int left, top, right, bottom;
	};

	class HWTexture
	{
	public:
		std::shared_ptr<GPUTexture2D> Texture;
		std::shared_ptr<GPUStagingTexture> Buffers[2];
		int CurrentBuffer = 0;

		std::vector<uint8_t> MapBuffer;
	};

	class HWVertexBuffer
	{
	public:
		std::shared_ptr<GPUVertexArray> VertexArray;
		std::shared_ptr<GPUVertexBuffer> Buffer;
	};

	// The number of points for the vertex buffer.
	enum { NUM_VERTS = 10240 };

	// The number of indices for the index buffer.
	enum { NUM_INDEXES = ((NUM_VERTS * 6) / 4) };

	// The number of quads we can batch together.
	enum { MAX_QUAD_BATCH = (NUM_INDEXES / 6) };

	// The default size for a texture atlas.
	enum { DEF_ATLAS_WIDTH = 512 };
	enum { DEF_ATLAS_HEIGHT = 512 };

	struct Atlas;

	struct PackedTexture
	{
		Atlas *Owner;

		PackedTexture **Prev, *Next;

		// Pixels this image covers
		LTRBRect Area;

		// Texture coordinates for this image
		float Left, Top, Right, Bottom;

		// Texture has extra space on the border?
		bool Padded;
	};

	struct Atlas
	{
		Atlas(ZDFrameBuffer *fb, int width, int height, GPUPixelFormat format);
		~Atlas();

		PackedTexture *AllocateImage(const Rect &rect, bool padded);
		void FreeBox(PackedTexture *box);

		SkylineBinPack Packer;
		Atlas *Next;
		std::unique_ptr<HWTexture> Tex;
		GPUPixelFormat Format;
		PackedTexture *UsedList;	// Boxes that contain images
		int Width, Height;
		bool OneUse;
	};

	class OpenGLTex : public FNativeTexture
	{
	public:
		OpenGLTex(FTexture *tex, ZDFrameBuffer *fb, bool wrapping);
		~OpenGLTex();

		FTexture *GameTex;
		PackedTexture *Box;

		OpenGLTex **Prev;
		OpenGLTex *Next;

		bool IsGray;

		bool Create(ZDFrameBuffer *fb, bool wrapping);
		bool Update();
		bool CheckWrapping(bool wrapping);
		GPUPixelFormat GetTexFormat();
		FTextureFormat ToTexFmt(GPUPixelFormat fmt);
	};

	class OpenGLPal : public FNativePalette
	{
	public:
		OpenGLPal(FRemapTable *remap, ZDFrameBuffer *fb);
		~OpenGLPal();

		OpenGLPal **Prev;
		OpenGLPal *Next;

		std::unique_ptr<HWTexture> Tex;
		uint32_t BorderColor;
		bool DoColorSkip;

		bool Update() override;

		FRemapTable *Remap;
		ZDFrameBuffer *fb;
		int RoundedPaletteSize;
	};

	// Flags for a buffered quad
	enum
	{
		BQF_GamePalette = 1,
		BQF_CustomPalette = 7,
		BQF_Paletted = 7,
		BQF_Bilinear = 8,
		BQF_WrapUV = 16,
		BQF_InvertSource = 32,
		BQF_DisableAlphaTest = 64,
		BQF_Desaturated = 128,
	};

	// Shaders for a buffered quad
	enum
	{
		BQS_PalTex,
		BQS_Plain,
		BQS_RedToAlpha,
		BQS_ColorOnly,
		BQS_SpecialColormap,
		BQS_InGameColormap,
	};

	struct BufferedTris
	{
		uint8_t Flags = 0;
		uint8_t ShaderNum = 0;
		int BlendOp = 0;
		int SrcBlend = 0;
		int DestBlend = 0;

		uint8_t Desat = 0;
		OpenGLPal *Palette = nullptr;
		HWTexture *Texture = nullptr;
		uint16_t NumVerts = 0;		// Number of _unique_ vertices used by this set.
		uint16_t NumTris = 0;		// Number of triangles used by this set.

		void ClearSetup()
		{
			Flags = 0;
			ShaderNum = 0;
			BlendOp = 0;
			SrcBlend = 0;
			DestBlend = 0;
		}

		bool IsSameSetup(const BufferedTris &other) const
		{
			return Flags == other.Flags && ShaderNum == other.ShaderNum && BlendOp == other.BlendOp && SrcBlend == other.SrcBlend && DestBlend == other.DestBlend;
		}
	};

	enum
	{
		SHADER_NormalColor,
		SHADER_NormalColorPal,
		SHADER_NormalColorInv,
		SHADER_NormalColorPalInv,

		SHADER_RedToAlpha,
		SHADER_RedToAlphaInv,

		SHADER_VertexColor,

		SHADER_SpecialColormap,
		SHADER_SpecialColormapPal,

		SHADER_InGameColormap,
		SHADER_InGameColormapDesat,
		SHADER_InGameColormapInv,
		SHADER_InGameColormapInvDesat,
		SHADER_InGameColormapPal,
		SHADER_InGameColormapPalDesat,
		SHADER_InGameColormapPalInv,
		SHADER_InGameColormapPalInvDesat,

		SHADER_BurnWipe,
		SHADER_GammaCorrection,

		NUM_SHADERS
	};

	class Wiper
	{
	public:
		virtual ~Wiper();
		virtual bool Run(int ticks, ZDFrameBuffer *fb) = 0;

		void DrawScreen(ZDFrameBuffer *fb, HWTexture *tex, int blendop = 0, uint32_t color0 = 0, uint32_t color1 = 0xFFFFFFF);
	};

	static uint32_t ColorARGB(uint32_t a, uint32_t r, uint32_t g, uint32_t b) { return ((a & 0xff) << 24) | ((r & 0xff) << 16) | ((g & 0xff) << 8) | ((b) & 0xff); }
	static uint32_t ColorRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a) { return ColorARGB(a, r, g, b); }
	static uint32_t ColorXRGB(uint32_t r, uint32_t g, uint32_t b) { return ColorARGB(0xff, r, g, b); }
	static uint32_t ColorValue(float r, float g, float b, float a) { return ColorRGBA((uint32_t)(r * 255.0f), (uint32_t)(g * 255.0f), (uint32_t)(b * 255.0f), (uint32_t)(a * 255.0f)); }

	void GetLetterboxFrame(int &x, int &y, int &width, int &height);

	void BindFBBuffer();
	void *MappedMemBuffer = nullptr;
	bool UseMappedMemBuffer = true;

	void DrawLetterbox(int x, int y, int width, int height);
	void Draw3DPart(bool copy3d);

	std::unique_ptr<HWVertexBuffer> CreateVertexBuffer(int size);
	std::unique_ptr<HWTexture> CreateTexture(const FString &name, int width, int height, int levels, GPUPixelFormat format);

	void SetGammaRamp(const GammaRamp *ramp);
	void DrawTriangles(int count, const FBVERTEX *vertices);
	void DrawTriangles(int count, const BURNVERTEX *vertices);
	void DrawPoints(int count, const FBVERTEX *vertices);
	void DrawLineList(int count);
	void DrawTriangleList(int minIndex, int numVertices, int startIndex, int primitiveCount);
	void Present();

	void Flip();
	void SetInitialState();
	void LoadShaders();
	void CreateFBTexture();
	void CreatePaletteTexture();
	void CreateVertexes();
	void UploadPalette();
	void CalcFullscreenCoords(FBVERTEX verts[6], bool viewarea_only, uint32_t color0, uint32_t color1) const;
	std::unique_ptr<HWTexture> CopyCurrentScreen();
	void KillNativePals();
	void KillNativeTexs();
	PackedTexture *AllocPackedTexture(int width, int height, bool wrapping, GPUPixelFormat format);
	void DrawPackedTextures(int packnum);
	bool SetStyle(OpenGLTex *tex, DrawParms &parms, uint32_t &color0, uint32_t &color1, BufferedTris &quad);
	static int GetStyleAlpha(int type);
	static void SetColorOverlay(uint32_t color, float alpha, uint32_t &color0, uint32_t &color1);
	void AddColorOnlyQuad(int left, int top, int width, int height, uint32_t color);
	void AddColorOnlyRect(int left, int top, int width, int height, uint32_t color);
	void CheckQuadBatch(int numtris = 2, int numverts = 4);
	void BeginQuadBatch();
	void EndQuadBatch();
	void BeginLineBatch();
	void EndLineBatch();
	void EndBatch();

	void EnableAlphaTest(bool enabled);
	void SetAlphaBlend(int op, int srcblend = 0, int destblend = 0);
	void SetPixelShader(const std::shared_ptr<GPUProgram> &shader);
	void SetPaletteTexture(HWTexture *texture, int count, uint32_t border_color);

	static void BgraToRgba(uint32_t *dest, const uint32_t *src, int width, int height, int srcpitch);

	static const std::vector<const char *> ShaderDefines[NUM_SHADERS];

	int m_Lock = 0;
	bool UpdatePending;
	int In2D = 0;
	bool InScene = false;

	uint32_t BorderColor;
	uint32_t FlashColor0 = 0;
	uint32_t FlashColor1 = 0xFFFFFFFF;
	PalEntry FlashColor = 0;
	int FlashAmount = 0;

	std::unique_ptr<HWVertexBuffer> StreamVertexBuffer, StreamVertexBufferBurn;
	ShaderUniforms ShaderConstants;
	bool ShaderConstantsModified = true;
	std::shared_ptr<GPUUniformBuffer> GpuShaderUniforms;
	std::shared_ptr<GPUProgram> CurrentShader;

	uint32_t CurBorderColor;

	PalEntry SourcePalette[256];
	float Gamma = 1.0f;
	bool NeedPalUpdate = false;
	bool NeedGammaUpdate = false;
	LTRBRect BlendingRect;

	bool GatheringWipeScreen = false;
	bool AALines;
	uint8_t BlockNum;
	OpenGLPal *Palettes = nullptr;
	OpenGLTex *Textures = nullptr;
	Atlas *Atlases = nullptr;

	std::shared_ptr<GPUSampler> SamplerRepeat;
	std::shared_ptr<GPUSampler> SamplerClampToEdge;
	std::shared_ptr<GPUSampler> SamplerClampToEdgeLinear;

	std::unique_ptr<HWTexture> FBTexture;
	std::unique_ptr<HWTexture> PaletteTexture;
	std::unique_ptr<HWTexture> ScreenshotTexture;

	std::unique_ptr<HWVertexBuffer> VertexBuffer;
	FBVERTEX *VertexData = nullptr;
	std::shared_ptr<GPUIndexBuffer> IndexBuffer;
	uint16_t *IndexData = nullptr;
	std::vector<BufferedTris> QuadExtra = std::vector<BufferedTris>(MAX_QUAD_BATCH);
	int VertexPos = -1;
	int IndexPos = -1;
	int QuadBatchPos = -1;
	enum { BATCH_None, BATCH_Quads, BATCH_Lines } BatchType = BATCH_None;

	std::shared_ptr<GPUProgram> Shaders[NUM_SHADERS];

	std::unique_ptr<HWTexture> InitialWipeScreen, FinalWipeScreen;
	std::unique_ptr<Wiper> ScreenWipe;

	class Wiper_Melt;			friend class Wiper_Melt;
	class Wiper_Burn;			friend class Wiper_Burn;
	class Wiper_Crossfade;		friend class Wiper_Crossfade;
};
