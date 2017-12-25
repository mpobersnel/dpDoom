
cbuffer FrameUniforms
{
	float4x4 WorldToView;
	float4x4 ViewToProjection;
	float GlobVis;
};

struct PixelIn
{
	float4 ScreenPos : SV_Position;
	float2 UV : PixelUV;
	float3 PositionInView : PixelPositionInView;
	float Light : PixelLight;
	float AlphaTest : PixelAlphaTest;
	int Mode : PixelMode;
	float4 FillColor : PixelFillColor;
};

struct PixelOut
{
	float4 FragColor : SV_Target0;
};

Texture2D DiffuseTexture;
Texture2D BasecolormapTexture;
Texture2D TranslationTexture;

SamplerState DiffuseSampler;

float SoftwareLight(PixelIn input)
{
	if (input.Light < 0)
		return -input.Light / 255.0;

	float z = -input.PositionInView.z;
	float vis = GlobVis / z;
	float shade = 64.0 - (input.Light + 12.0) * 32.0/128.0;
	float lightscale = clamp((shade - min(24.0, vis)) / 32.0, 0.0, 31.0/32.0);
	return 1.0 - lightscale;
}

float SoftwareLightPal(PixelIn input)
{
	if (input.Light < 0)
		return 31 - int((-1.0 - input.Light) * 31.0 / 255.0 + 0.5);

	float z = -input.PositionInView.z;
	float vis = GlobVis / z;
	float shade = 64.0 - (input.Light + 12.0) * 32.0/128.0;
	float lightscale = clamp((shade - min(24.0, vis)), 0.0, 31.0);
	return lightscale;
}

int SampleFg(PixelIn input)
{
	return int(DiffuseTexture.Sample(DiffuseSampler, input.UV).r * 255.0 + 0.5);
}

float4 LightShade(PixelIn input, float4 fg)
{
	return float4((fg * SoftwareLight(input)).rgb, fg.a);
}

float4 LightShadePal(PixelIn input, int fg)
{
	float light = max(SoftwareLightPal(input) - 0.5, 0.0);
	float t = frac(light);
	int index0 = int(light);
	int index1 = min(index0 + 1, 31);
	float4 color0 = BasecolormapTexture.Load(int3(fg, index0, 0));
	float4 color1 = BasecolormapTexture.Load(int3(fg, index1, 0));
	color0.rgb = pow(color0.rgb, float3(2.2, 2.2, 2.2));
	color1.rgb = pow(color1.rgb, float3(2.2, 2.2, 2.2));
	float4 mixcolor = lerp(color0, color1, t);
	mixcolor.rgb = pow(mixcolor.rgb, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
	return mixcolor;
}

float4 Translate(int fg)
{
	return TranslationTexture.Load(int3(fg, 0, 0));
}

int TranslatePal(int fg)
{
	return int(TranslationTexture.Load(int3(fg, 0, 0)).r * 255.0 + 0.5);
}

int FillColorPal(PixelIn input)
{
	return int(input.FillColor.a);
}

#if defined(TRUECOLOR)

PixelOut TextureSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	float4 fg = DiffuseTexture.Sample(DiffuseSampler, input.UV);
	if (fg.a < 0.5) alphatest = true;
	output.FragColor = LightShade(input, fg);
	output.FragColor.a = 1.0;

	return output;
}

PixelOut TranslatedSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	int fg = SampleFg(input);
	if (fg == 0) alphatest = true;

	output.FragColor = LightShade(input, Translate(fg));
	output.FragColor.rgb *= output.FragColor.a;

	return output;
}

PixelOut ShadedSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	float alpha = DiffuseTexture.Sample(DiffuseSampler, input.UV).r;
	if (alpha < 0.5) alphatest = true;
	output.FragColor = LightShade(input, float4(input.FillColor.rgb, 1.0)) * alpha;

	return output;
}

PixelOut StencilSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	float alpha = step(0.5, DiffuseTexture.Sample(DiffuseSampler, input.UV).a);
	if (alpha < 0.5) alphatest = true;
	output.FragColor = LightShade(input, float4(input.FillColor.rgb, 1.0)) * alpha;

	return output;
}

PixelOut FillSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	output.FragColor = LightShade(input, float4(input.FillColor.rgb, 1.0));

	return output;
}

PixelOut SkycapSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	float4 capcolor = LightShade(input, float4(input.FillColor.rgb, 1.0));

	float4 fg = DiffuseTexture.Sample(DiffuseSampler, input.UV);
	float4 skycolor = LightShade(input, fg);

	float startFade = 4.0; // How fast it should fade out
	float alphaTop = clamp(input.UV.y * startFade, 0.0, 1.0);
	float alphaBottom = clamp((2.0 - input.UV.y) * startFade, 0.0, 1.0);
	float alpha = min(alphaTop, alphaBottom);

	output.FragColor = lerp(capcolor, skycolor, alpha);

	return output;
}

PixelOut FuzzSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	float alpha = (SampleFg(input) != 0) ? 1.0 : 0.0;
	output.FragColor = LightShade(input, float4(input.FillColor.rgb, 1.0)) * alpha;

	return output;
}

PixelOut FogBoundarySampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	output.FragColor = LightShade(input, input.FillColor);

	return output;
}

#else

PixelOut TextureSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	int fg = SampleFg(input);
	if (fg == 0) alphatest = true;
	output.FragColor = LightShadePal(input, fg);
	output.FragColor.rgb *= output.FragColor.a;

	return output;
}

PixelOut TranslatedSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	int fg = SampleFg(input);
	if (fg == 0) alphatest = true;

	output.FragColor = LightShadePal(input, TranslatePal(fg));
	output.FragColor.rgb *= output.FragColor.a;

	return output;
}

PixelOut ShadedSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	output.FragColor = LightShadePal(input, FillColorPal(input)) * DiffuseTexture.Sample(DiffuseSampler, input.UV).r;

	return output;
}

PixelOut StencilSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	float alpha = (SampleFg(input) != 0) ? 1.0 : 0.0;
	output.FragColor = LightShadePal(input, FillColorPal(input)) * alpha;

	return output;
}

PixelOut FillSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	output.FragColor = LightShadePal(input, FillColorPal(input));

	return output;
}

PixelOut SkycapSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	float4 capcolor = LightShadePal(input, FillColorPal(input));

	int fg = SampleFg(input);
	float4 skycolor = LightShadePal(input, fg);

	float startFade = 4.0; // How fast it should fade out
	float alphaTop = clamp(input.UV.y * startFade, 0.0, 1.0);
	float alphaBottom = clamp((2.0 - input.UV.y) * startFade, 0.0, 1.0);
	float alpha = min(alphaTop, alphaBottom);

	output.FragColor = lerp(capcolor, skycolor, alpha);

	return output;
}

PixelOut FuzzSampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	float alpha = (SampleFg(input) != 0) ? 1.0 : 0.0;
	output.FragColor = LightShadePal(input, FillColorPal(input)) * alpha;

	return output;
}

PixelOut FogBoundarySampler(PixelIn input, inout bool alphatest)
{
	PixelOut output;

	output.FragColor = LightShadePal(input, FillColorPal(input));

	return output;
}

#endif

PixelOut main(PixelIn input)
{
	PixelOut output;

	bool alphatest = false;

	switch (input.Mode)
	{
	case 0: output = TextureSampler(input, alphatest); break;
	case 1: output = TranslatedSampler(input, alphatest); break;
	case 2: output = ShadedSampler(input, alphatest); break;
	case 3: output = StencilSampler(input, alphatest); break;
	case 4: output = FillSampler(input, alphatest); break;
	case 5: output = SkycapSampler(input, alphatest); break;
	case 6: output = FuzzSampler(input, alphatest); break;
	case 7: output = FogBoundarySampler(input, alphatest); break;
	}

	if (alphatest) discard;

	return output;
}
