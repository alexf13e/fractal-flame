
#version 460 core


in vec2 texCoord;

uniform uint texWidth, texHeight;

layout (std430) buffer TexOutput
{
    vec4 texOutput[];
};

out vec4 outcolor;


void main()
{
    uint x = uint(texCoord.x * texWidth);
    uint y = uint(texCoord.y * texHeight);
    uint i = y * texWidth + x;
    outcolor = texOutput[i];
}