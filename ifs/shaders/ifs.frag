
#version 460 core


in vec2 texCoord;

out vec4 outColor;

uniform uint texWidth, texHeight;
uniform mat4 matPausedCamTransform;
uniform bool paused;

layout (std430) buffer TexOutput
{
    vec4 texOutput[];
};


void main()
{
    vec4 uv = vec4(texCoord, 0.0f, 1.0f);
    if (paused)
    {
        uv = matPausedCamTransform * uv;
    }

    int x = int(uv.x * texWidth);
    int y = int(uv.y * texHeight);

    if (x < 0 || x >= texWidth || y < 0 || y >= texHeight)
    {
        outColor = vec4(vec3(0.0f), 1.0f);
    }
    else
    {
        uint i = y * texWidth + x;
        outColor = texOutput[i];
    }
}