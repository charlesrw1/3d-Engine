#version 330 core

const vec2 corners[4] = { 
    vec2(0.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(1.0, 0.0) };

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;


in vec4 colorv[];
out vec4 color;

uniform mat4 projection;
uniform mat4 view;
uniform float spriteSize;

out vec2 texCoord;

void main()
{
    for(int i=0; i<4; ++i)
    {
        vec4 eyePos = gl_in[0].gl_Position;           //start with point position
        eyePos.xy += spriteSize * (corners[i] - vec2(0.5)); //add corner position
        gl_Position = projection * eyePos;             //complete transformation
        texCoord = corners[i];                         //use corner as texCoord
		
		color = colorv[0];
        EmitVertex();
    }
}