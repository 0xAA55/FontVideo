#version 130

precision highp float;

const float sqrt_3 = 1.7320508075688772935274463415059;

out int BrightnessIndex;
out float ModulusValue;
out vec3 AverageColor;
uniform ivec2 iGlyphSize;
uniform ivec2 iGlyphMatrixSize;
uniform sampler2D iSrcColor;
uniform sampler2D iGlyphBrightnessMatrix;

float getBrightness(int i)
{
    ivec2 xy = ivec2(i % iGlyphMatrixSize.x, i / iGlyphMatrixSize.x);
    return texelFetch(iGlyphBrightnessMatrix, xy, 0).r;
}

void main()
{
    ivec2 Orig = ivec2(gl_FragCoord.xy) * iGlyphSize;
    float l = 0;
    float modulus = 0;
    vec3 colorSum = vec3(0);
    float numPixels = float(iGlyphSize.x * iGlyphSize.y);
    for(int y = 0; y < iGlyphSize.y; y++)
    {
        for(int x = 0; x < iGlyphSize.x; x++)
        {
            ivec2 xy = ivec2(x, y);
            vec3 sample_rgb = texelFetch(iSrcColor, Orig + xy, 0).rgb;
            colorSum += sample_rgb;
            float sample_brg = length(sample_rgb) / sqrt_3;
            l += sample_brg;
            sample_brg -= 0.5;
            modulus += sample_brg * sample_brg;
        }
    }
    float brightness = l / numPixels;
    int left = 0;
    int right = iGlyphMatrixSize.x * iGlyphMatrixSize.y - 1;
    int mid = iGlyphMatrixSize.x * iGlyphMatrixSize.y / 2;
    float left_brightness = getBrightness(left);
    float right_brightness = getBrightness(right);
    for(int approx = 0; approx < 32; approx ++)
    {
        float mid_brightness = getBrightness(mid);
        if (brightness < mid_brightness)
        {
            right = mid;
            right_brightness = getBrightness(right);
        }
        else if (brightness > mid_brightness)
        {
            left = mid;
            left_brightness = getBrightness(left);
        }
        else
        {
            break;
        }
        mid = (right + left) / 2;
        if (mid == left)
        {
            break;
        }
    }

    BrightnessIndex = left;
    ModulusValue = sqrt(modulus);
    AverageColor = colorSum / numPixels;
}

