
struct PixelIn
{
	float4 ScreenPos : SV_Position;
	float2 UV : PixelUV;
};

struct PixelOut
{
	float4 FragColor : SV_Target0;
};

PixelOut main(PixelIn input)
{
	PixelOut output;
	output.FragColor = float4(1.0, 1.0, 1.0, 1.0);
	return output;
}
