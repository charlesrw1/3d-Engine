#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in ivec3 aBoneIDs;
layout (location = 4) in vec3 aWeights;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

const int MAX_BONES = 100;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 normal_mat;
uniform mat4 gBones[MAX_BONES];

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(normal_mat)) * aNormal;  
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
	
	TexCoord = aTexture;
}