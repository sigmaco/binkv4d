// QWADRO (c) 2017 SIGMA TECHNOLOGY GROUP

TEXTURE(0, 0, samplerBuffer, texY);
TEXTURE(0, 1, samplerBuffer, texU);
TEXTURE(0, 2, samplerBuffer, texV);
TEXTURE(0, 3, samplerBuffer, texA);
UNIFORM(1, 0)
{
    bool hasAlpha;
    int imageWidth;
    int imageHeight;
};

in block
{
    vec2 uv0;
} sgl_v;

OUT(0, vec4, sgl_rgba);

vec3 yuvToRgb(float y, float u, float v)
{
    y = y * 1.1643;
    u = u - 0.5;
    v = v - 0.5;

    float r = y + 1.5958 * v;
    float g = y - 0.39173 * u - 0.81290 * v;
    float b = y + 2.017 * u;

    return vec3(r, g, b);
}

void main()
{
    int x = int(texCoord.x * float(imageWidth));
    int y = int(texCoord.y * float(imageHeight));
    int index = y * imageWidth + x;

    float yVal = texelFetch(texY, index).r;
    float uVal = texelFetch(texU, index).r;
    float vVal = texelFetch(texV, index).r;
    float aVal = hasAlpha ? texelFetch(texA, index).r : 1.0;

    vec3 rgb = yuvToRgb(yVal, uVal, vVal);
    sgl_rgba = vec4(rgb, aVal);
}
