#version 330 core
out vec4 FragColor;

in vec3 Normal;  
in vec3 FragPos;  
in vec2 TexCoord;
  
uniform vec3 viewPos;

void main()
{
  
	vec3 viewDir = normalize(viewPos - FragPos);
	vec3 norm = normalize(Normal);
	// rim (from TF2 shader)
	float NdotE = max(dot(norm, viewDir), 0.0);
	vec3 rim = vec3(0.3) * pow(1.0-NdotE, 6.0);
	rim *= 2;
	if(rim.r <= 0)
		discard;
    FragColor = vec4(rim, rim.r);
} 