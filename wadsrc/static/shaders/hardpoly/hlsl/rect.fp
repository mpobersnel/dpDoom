
cbuffer RectUniforms
{
	float X0, Y0, U0, V0;
	float X1, Y1, U1, V1;
	float Light;
};

struct PixelIn
{
	float4 ScreenPos : SV_Position;
	float2 UV : PixelUV;
};

struct PixelOut
{
	float4 FragColor : SV_Target0;
};

Texture2D DiffuseTexture;
Texture2D BasecolormapTexture;

SamplerState DiffuseSampler;
				
PixelOut main(PixelIn input)
{
	PixelOut output;

#if defined(TRUECOLOR)
	output.FragColor = DiffuseTexture.Sample(DiffuseSampler, input.UV) * (Light / 255.0);
	output.FragColor.rgb *= output.FragColor.a;
	if (output.FragColor.a < 0.5) discard;
#else
	int shade = 31 - int(Light * 31.0 / 255.0 + 0.5);
	int fg = int(texture(DiffuseTexture, input.UV).r * 255.0 + 0.5);
	if (fg == 0) discard;
	output.FragColor = BasecolormapTexture.Load(uint3(fg, shade, 0));
#endif

	return output;
}
