#version 330 core
in vec2 v_texCoord;
in float v_light;
in float v_texLayer;

out vec4 f_color;

uniform sampler2DArray tex1;

void main( void ) 
{
    f_color = texture(tex1, vec3(v_texCoord, v_texLayer)) * v_light;
}