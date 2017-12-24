
layout(std140) uniform FrameUniforms
{
	mat4 WorldToView;
	mat4 ViewToProjection;
	float GlobVis;
};

in vec2 UV;
in vec3 PositionInView;
flat in float Light;
flat in float AlphaTest;
flat in int Mode;
flat in vec4 FillColor;

out vec4 FragColor;

uniform sampler2D DiffuseTexture;
uniform sampler2D BasecolormapTexture;
uniform sampler2D TranslationTexture;
				
float SoftwareLight()
{
	if (Light < 0)
		return -Light / 255.0;

	float z = -PositionInView.z;
	float vis = GlobVis / z;
	float shade = 64.0 - (Light + 12.0) * 32.0/128.0;
	float lightscale = clamp((shade - min(24.0, vis)) / 32.0, 0.0, 31.0/32.0);
	return 1.0 - lightscale;
}

float SoftwareLightPal()
{
	if (Light < 0)
		return 31 - int((-1.0 - Light) * 31.0 / 255.0 + 0.5);

	float z = -PositionInView.z;
	float vis = GlobVis / z;
	float shade = 64.0 - (Light + 12.0) * 32.0/128.0;
	float lightscale = clamp((shade - min(24.0, vis)), 0.0, 31.0);
	return lightscale;
}

int SampleFg()
{
	return int(texture(DiffuseTexture, UV).r * 255.0 + 0.5);
}

vec4 LightShade(vec4 fg)
{
	return vec4((fg * SoftwareLight()).rgb, fg.a);
}

vec4 LightShadePal(int fg)
{
	float light = max(SoftwareLightPal() - 0.5, 0.0);
	float t = fract(light);
	int index0 = int(light);
	int index1 = min(index0 + 1, 31);
	vec4 color0 = texelFetch(BasecolormapTexture, ivec2(fg, index0), 0);
	vec4 color1 = texelFetch(BasecolormapTexture, ivec2(fg, index1), 0);
	color0.rgb = pow(color0.rgb, vec3(2.2));
	color1.rgb = pow(color1.rgb, vec3(2.2));
	vec4 mixcolor = mix(color0, color1, t);
	mixcolor.rgb = pow(mixcolor.rgb, vec3(1.0/2.2));
	return mixcolor;
}

vec4 Translate(int fg)
{
	return texelFetch(TranslationTexture, ivec2(fg, 0), 0);
}

int TranslatePal(int fg)
{
	return int(texelFetch(TranslationTexture, ivec2(fg, 0), 0).r * 255.0 + 0.5);
}

int FillColorPal()
{
	return int(FillColor.a);
}

#if defined(TRUECOLOR)

void TextureSampler()
{
	vec4 fg = texture(DiffuseTexture, UV);
	if (fg.a < 0.5) discard;
	FragColor = LightShade(fg);
	FragColor.a = 1.0;
}

void TranslatedSampler()
{
	int fg = SampleFg();
	if (fg == 0) discard;

	FragColor = LightShade(Translate(fg));
	FragColor.rgb *= FragColor.a;
}

void ShadedSampler()
{
	float alpha = texture(DiffuseTexture, UV).r;
	if (alpha < 0.5) discard;
	FragColor = LightShade(vec4(FillColor.rgb, 1.0)) * alpha;
}

void StencilSampler()
{
	float alpha = step(0.5, texture(DiffuseTexture, UV).a);
	if (FragColor.a < 0.5) discard;
	FragColor = LightShade(vec4(FillColor.rgb, 1.0)) * alpha;
}

void FillSampler()
{
	FragColor = LightShade(vec4(FillColor.rgb, 1.0));
}

void SkycapSampler()
{
	vec4 capcolor = LightShade(vec4(FillColor.rgb, 1.0));

	vec4 fg = texture(DiffuseTexture, UV);
	vec4 skycolor = LightShade(fg);

	float startFade = 4.0; // How fast it should fade out
	float alphaTop = clamp(UV.y * startFade, 0.0, 1.0);
	float alphaBottom = clamp((2.0 - UV.y) * startFade, 0.0, 1.0);
	float alpha = min(alphaTop, alphaBottom);

	FragColor = mix(capcolor, skycolor, alpha);
}

void FuzzSampler()
{
	float alpha = (SampleFg() != 0) ? 1.0 : 0.0;
	FragColor = LightShade(vec4(FillColor.rgb, 1.0)) * alpha;
}

void FogBoundarySampler()
{
	FragColor = LightShade(FillColor);
}

#else

void TextureSampler()
{
	int fg = SampleFg();
	if (fg == 0) discard;
	FragColor = LightShadePal(fg);
	FragColor.rgb *= FragColor.a;
}

void TranslatedSampler()
{
	int fg = SampleFg();
	if (fg == 0) discard;

	FragColor = LightShadePal(TranslatePal(fg));
	FragColor.rgb *= FragColor.a;
}

void ShadedSampler()
{
	FragColor = LightShadePal(FillColorPal()) * texture(DiffuseTexture, UV).r;
}

void StencilSampler()
{
	float alpha = (SampleFg() != 0) ? 1.0 : 0.0;
	FragColor = LightShadePal(FillColorPal()) * alpha;
}

void FillSampler()
{
	FragColor = LightShadePal(FillColorPal());
}

void SkycapSampler()
{
	vec4 capcolor = LightShadePal(FillColorPal());

	int fg = SampleFg();
	vec4 skycolor = LightShadePal(fg);

	float startFade = 4.0; // How fast it should fade out
	float alphaTop = clamp(UV.y * startFade, 0.0, 1.0);
	float alphaBottom = clamp((2.0 - UV.y) * startFade, 0.0, 1.0);
	float alpha = min(alphaTop, alphaBottom);

	FragColor = mix(capcolor, skycolor, alpha);
}

void FuzzSampler()
{
	float alpha = (SampleFg() != 0) ? 1.0 : 0.0;
	FragColor = LightShadePal(FillColorPal()) * alpha;
}

void FogBoundarySampler()
{
	FragColor = LightShadePal(FillColorPal());
}

#endif

void main()
{
	switch (Mode)
	{
	case 0: TextureSampler(); break;
	case 1: TranslatedSampler(); break;
	case 2: ShadedSampler(); break;
	case 3: StencilSampler(); break;
	case 4: FillSampler(); break;
	case 5: SkycapSampler(); break;
	case 6: FuzzSampler(); break;
	case 7: FogBoundarySampler(); break;
	}
}
