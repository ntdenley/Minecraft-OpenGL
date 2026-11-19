#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in float aLight;
layout (location = 3) in float aTexLayer;

out vec3 v_color;
out vec2 v_texCoord;
out float v_light;
out float v_texLayer;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    v_texCoord = aTexCoord;
    v_light = aLight;
    v_texLayer = aTexLayer;
}