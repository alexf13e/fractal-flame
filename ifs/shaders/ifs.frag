
#version 460 core


in vec2 texCoord;

uniform uint texWidth, texHeight;
uniform float gamma;
uniform float brightness;
uniform uint guideLineNum;
uniform uvec2 mousePos;
uniform uint drawMouseLine;

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

    if (guideLineNum == 0)
    {
        //no guidelines
        outcolor = texOutput[i];
        return;
    }

    if (guideLineNum == 1)
    {
        //guidelines through centre of screen (2x2 grid)
        if (x == texWidth / 2 || y == texHeight / 2)
        {
            outcolor = vec4(1.0f - texOutput[i].xyz, texOutput[i].w);
            return;
        }
    }
    else if (guideLineNum == 2)
    {
        //guidelines at screen thirds (3x3 grid)
        if (x == texWidth / 3 || y == texHeight / 3 || x == texWidth * 2 / 3 || y == texHeight * 2 / 3)
        {
            outcolor = vec4(1.0f - texOutput[i].xyz, texOutput[i].w);
            return;
        }
    }

    if (drawMouseLine == 1)
    {
        if (x == mousePos.x && y == mousePos.y)
        {
            outcolor = vec4(1.0f - texOutput[i].xyz, texOutput[i].w);
        }

        outcolor = vec4(1.0f);
    }
    else
    {
        outcolor = texOutput[i];
    }
}