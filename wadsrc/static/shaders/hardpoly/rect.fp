
layout(std140) uniform RectUniforms
{
	float X0, Y0, U0, V0;
	float X1, Y1, U1, V1;
	float Light;
};

in vec2 UV;
out vec4 FragColor;

uniform sampler2D DiffuseTexture;
uniform sampler2D BasecolormapTexture;
				
void main()
{
#if defined(TRUECOLOR)
	FragColor = texture(DiffuseTexture, UV) * (Light / 255.0);
	FragColor.rgb *= FragColor.a;
	if (FragColor.a < 0.5) discard;
#else
	int shade = 31 - int(Light * 31.0 / 255.0 + 0.5);
	int fg = int(texture(DiffuseTexture, UV).r * 255.0 + 0.5);
	if (fg == 0) discard;
	FragColor = texelFetch(BasecolormapTexture, ivec2(fg, shade), 0);
#endif
}
