#version 130

precision highp float;

const float sqrt_3 = 1.7320508075688772935274463415059;

out int Index;
out int Color;
uniform int iBrightnessWindowSize;
uniform int iNumCodes;
uniform int iSrcInvert;
uniform ivec2 iGlyphSize;
uniform ivec2 iGlyphMatrixSize;
uniform sampler2D iSrcColor;
uniform sampler2D iAvrColor;
uniform sampler2D iGlyphImageMatrix;
uniform isampler2D iBrightnessIndex;
uniform sampler2D iModulusValue;
uniform vec3 iPalette[16] = vec3[16]
(
    vec3(0.0, 0.0, 0.0),
    vec3(0.0, 0.0, 0.5),
    vec3(0.0, 0.5, 0.0),
    vec3(0.0, 0.5, 0.5),
    vec3(0.5, 0.0, 0.0),
    vec3(0.5, 0.0, 0.5),
    vec3(0.5, 0.5, 0.0),
    vec3(0.5, 0.5, 0.5),
    vec3(0.3, 0.3, 0.3),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 1.0, 1.0),
    vec3(1.0, 0.0, 0.0),
    vec3(1.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0),
    vec3(1.0, 1.0, 1.0)
);

void main()
{
	ivec2 outputXY = ivec2(gl_FragCoord.xy);
	int brightnessIndex = texelFetch(iBrightnessIndex, outputXY, 0).r;
	float modulusValue = texelFetch(iModulusValue, outputXY, 0).r;
	int halfWindow = iBrightnessWindowSize / 2;
	int maxCode = iNumCodes - 1;
	int start = brightnessIndex - halfWindow;
	int end = start + iBrightnessWindowSize;
	if (start < 0) start = 0;
	else if (start > maxCode) start = maxCode;
	if (end < start) end = start;
	else if (end > maxCode) end = maxCode;
	int bestCode = start;
	float bestCodeScore = -9999999.0;
	ivec2 origXY = outputXY * iGlyphSize;
	for (int code = start; code < end; code ++)
	{
		ivec2 codeXY = ivec2(code % iGlyphMatrixSize.x, code / iGlyphMatrixSize.x);
		ivec2 codeGlyphXY = codeXY * iGlyphSize;
		float score = 0;
		float glyphModulus = 0;
		for (int y = 0; y < iGlyphSize.y; y++)
		{
			for (int x = 0; x < iGlyphSize.x; x++)
			{
				ivec2 xy = ivec2(x, y);
				float s = length(texelFetch(iSrcColor, origXY + xy, 0).rgb) / sqrt_3 - 0.5;
				float g = length(texelFetch(iGlyphImageMatrix, codeGlyphXY + xy, 0).rgb) / sqrt_3 - 0.5;
				score += (s / modulusValue) * g;
				glyphModulus += g * g;
			}
		}
		glyphModulus = sqrt(glyphModulus);
		score /= glyphModulus;
		if (score >= bestCodeScore)
		{
			bestCodeScore = score;
			bestCode = code;
		}
	}
	vec3 avrColorVector = normalize(texelFetch(iAvrColor, outputXY, 0).rgb - vec3(0.5));
	if (iSrcInvert != 0) avrColorVector = -avrColorVector;
	int bestColor = 8;
	float bestColorScore = -9999999.0;
	for(int i = 0; i < 16; i++)
	{
		float score = dot(normalize(iPalette[i] - vec3(0.5)), avrColorVector);
		if (score >= bestColorScore)
		{
			bestColor = i;
			bestColorScore = score;
		}
	}
	Index = bestCode;
	Color = bestColor;
}

