#version 330 core
out vec4 FragColor;

in vec3 Normal;  
in vec3 FragPos;  
in vec2 TexCoord;
  
uniform vec3 lightPos; 
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform vec3 viewPos;

uniform sampler2D baseTex;
uniform sampler2D shinyMap;

uniform float shininess;

void main()
{
    vec4 col = texture(baseTex, TexCoord);
	vec4 spec_col = texture(shinyMap, TexCoord);
	
	// ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
	
	// specular
	float specular_strength = 0.5;
	vec3 viewDir = normalize(viewPos - FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0),50);
	vec3 specular = spec * lightColor * 0.1;
	
	// rim (from TF2 shader)
	float NdotE = max(dot(norm, viewDir), 0.0);
	vec3 rim = ambient * pow(1.0-NdotE, 6.0);
	//rim *= 0.8;
	
    // lambertian
	float lambert = dot(norm, lightDir);
	lambert = clamp(lambert,0.01,0.99);
	
    //vec3 result = (ambient + diffuse + specular) * objectColor + rim;
	
	vec3 result = (ambient + lambert) * col.rgb + rim + specular;
	//vec3 result = (lambert * objectColor) + specular + rim;
    FragColor = vec4(result, 1.0);
} 