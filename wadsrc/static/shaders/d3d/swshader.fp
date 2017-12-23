
cbuffer ShaderUniforms
{
	float4 Desaturation; // { Desat, 1 - Desat }
	float4 PaletteMod;
	float4 Weights; // RGB->Gray weighting { 77/256.0, 143/256.0, 37/256.0, 1 }
	float4 Gamma;
	float4 ScreenSize;
};

struct PixelIn
{
	float4 ScreenPos : SV_Position;
	float4 PixelColor0 : PixelColor0;
	float4 PixelColor1 : PixelColor1;
	float4 PixelTexCoord0 : PixelTexCoord0;
};

struct PixelOut
{
	float4 FragColor : SV_Target0;
};

Texture2D Image;
Texture2D Palette;
Texture2D NewScreen;
Texture2D Burn;

SamplerState ImageSampler;
SamplerState PaletteSampler;
SamplerState NewScreenSampler;
SamplerState BurnSampler;

float4 TextureLookup(float2 tex_coord)
{
#if defined(PALTEX)
	float index = Image.Sample(ImageSampler, tex_coord).x;
	index = index * PaletteMod.x + PaletteMod.y;
	return Palette.Sample(PaletteSampler, float2(index, 0.5));
#else
	return Image.Sample(ImageSampler, tex_coord);
#endif
}

float4 Invert(float4 rgb)
{
#if defined(INVERT)
	rgb.rgb = Weights.www - rgb.xyz;
#endif
	return rgb;
}

float Grayscale(float4 rgb)
{
	return dot(rgb.rgb, Weights.rgb);
}

float4 SampleTexture(float2 tex_coord)
{
	return Invert(TextureLookup(tex_coord));
}

// Normal color calculation for most drawing modes.

float4 NormalColor(float2 tex_coord, float4 Flash, float4 InvFlash)
{
	return Flash + SampleTexture(tex_coord) * InvFlash;
}

// Copy the red channel to the alpha channel. Pays no attention to palettes.

float4 RedToAlpha(float2 tex_coord, float4 Flash, float4 InvFlash)
{
	float4 color = Invert(Image.Sample(ImageSampler, tex_coord));
	color.a = color.r;
	return Flash + color * InvFlash;
}

// Just return the value of c0.

float4 VertexColor(float4 color)
{
	return color;
}

// Emulate one of the special colormaps. (Invulnerability, gold, etc.)

float4 SpecialColormap(float2 tex_coord, float4 start, float4 end)
{
	float4 color = SampleTexture(tex_coord);
	float4 range = end - start;
	// We can't store values greater than 1.0 in a color register, so we multiply
	// the final result by 2 and expect the caller to divide the start and end by 2.
	color.rgb = 2.0 * (start + Grayscale(color) * range).rgb;
	// Duplicate alpha semantics of NormalColor.
	color.a = start.a + color.a * end.a;
	return color;
}

// In-game colormap effect: fade to a particular color and multiply by another, with 
// optional desaturation of the original color. Desaturation is stored in c1.
// Fade level is packed int fade.a. Fade.rgb has been premultiplied by alpha.
// Overall alpha is in color.a.
float4 InGameColormap(float2 tex_coord, float4 color, float4 fade)
{
	float4 rgb = SampleTexture(tex_coord);

	// Desaturate
#if defined(DESAT)
	float foo = Grayscale(rgb) * Desaturation.x;
	float3 intensity;
	intensity = float3(foo, foo, foo);
	rgb.rgb = intensity.rgb + rgb.rgb * Desaturation.y;
#endif

	// Fade
	rgb.rgb = rgb.rgb * fade.aaa + fade.rgb;

	// Shade and Alpha
	rgb = rgb * color;

	return rgb;
}

// Windowed gamma correction.

float4 GammaCorrection(float2 tex_coord)
{
	float4 color = Image.Sample(ImageSampler, float2(tex_coord.x, 1.0 - tex_coord.y));
	color.rgb = pow(color.rgb, Gamma.rgb);
	return color;
}

// The burn wipe effect.

float4 BurnWipe(float4 coord)
{
	float4 color = NewScreen.Sample(NewScreenSampler, coord.xy);
	float4 alpha = Burn.Sample(BurnSampler, coord.zw);
	color.a = alpha.r * 2.0;
	return color;
}

PixelOut main(PixelIn input)
{
	PixelOut output;
#if defined(ENORMALCOLOR)
	output.FragColor = NormalColor(input.PixelTexCoord0.xy, input.PixelColor0, input.PixelColor1);
#elif defined(EREDTOALPHA)
	output.FragColor = RedToAlpha(input.PixelTexCoord0.xy, input.PixelColor0, input.PixelColor1);
#elif defined(EVERTEXCOLOR)
	output.FragColor = VertexColor(input.PixelColor0);
#elif defined(ESPECIALCOLORMAP)
	output.FragColor = SpecialColormap(input.PixelTexCoord0.xy, input.PixelColor0, input.PixelColor1);
#elif defined(EINGAMECOLORMAP)
	output.FragColor = InGameColormap(input.PixelTexCoord0.xy, input.PixelColor0, input.PixelColor1);
#elif defined(EBURNWIPE)
	output.FragColor = BurnWipe(input.PixelTexCoord0);
#elif defined(EGAMMACORRECTION)
	output.FragColor = GammaCorrection(input.PixelTexCoord0.xy);
#else
	#error Entry point define is missing
#endif
	return output;
}
