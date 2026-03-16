
#version 460 core


in vec2 texCoord;

uniform uint texWidth, texHeight;
uniform float gamma;
uniform float brightness;

layout (std430) buffer TexOutput
{
    vec4 texOutput[];
};

out vec4 outcolor;


void main()
{
    uint x = int(texCoord.x * texWidth);
    uint y = int(texCoord.y * texHeight);
    uint i = y * texWidth + x;
    outcolor = texOutput[i];
}