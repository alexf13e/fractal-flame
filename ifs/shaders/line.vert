
#version 460 core

in vec2 vs_position;

void main()
{
    gl_Position = vec4(vs_position, 0.0f, 1.0f);
}