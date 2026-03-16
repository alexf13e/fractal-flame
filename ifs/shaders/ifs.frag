
#version 460 core


in vec2 texCoord;

uniform uint texWidth, texHeight;
uniform float gamma;
uniform float brightness;

layout (std430) buffer TexOutput
{
    vec4 texOutput[];
};

out vec4 outColour;


void main()
{
    uint x = int(texCoord.x * texWidth);
    uint y = int(texCoord.y * texHeight);
    uint i = y * texWidth + x;

    vec4 pix = texOutput[i];

    float alphaScale = log2(pix.w) * 0.301 / pix.w;
    pix = brightness * alphaScale * pix;
    pix = vec4(pow(pix.xyz, vec3(1.0f / gamma)), pix.w);
    outColour = pix;

    //uint pix = texOutput[i];
    //float r = pix & 0xFF;
    //float g = (pix >> 8) & 0xFF;
    //float b = (pix >> 16) & 0xFF;
    //float a = (pix >> 24) & 0xFF;
    //outColour = vec4(r, g, b, a) / 255.0f;
}