#version 330 core
out vec4 FragColor;

in vec3 Normal;  
in vec3 FragPos;  
in vec2 TexCoord;
  

struct Light
{
	vec3 direction;
	
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

uniform Light light;
uniform vec3 viewPos;

// Material
uniform sampler2D baseTex;
uniform sampler2D shinyTex;

uniform float shininess;

void main()
{
    vec4 col = texture(baseTex, TexCoord);
	
	// ambient
    vec3 ambient = light.ambient * texture(baseTex, TexCoord).rgb;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-light.direction);			// IF A POINT LIGHT: normalize(lightpos - fragpos)
	
    float diff = dot(norm, lightDir)*0.5+0.5; 	// half lambertian
	//float diff = max(dot(norm,lightDir),0);
    vec3 diffuse = light.diffuse * diff * texture(baseTex, TexCoord).rgb;
	
	// specular
	float specular_strength = 0.5;
	vec3 viewDir = normalize(viewPos - FragPos);
	
	// Blinnphong:
	vec3 halfway_dir = normalize(lightDir+viewDir);
	float spec = pow(max(dot(norm,halfway_dir),0.0),36);
	vec3 specular = light.specular * spec*0.2;
	
	//vec3 reflectDir = reflect(-lightDir, norm);
	//float spec = pow(max(dot(viewDir, reflectDir), 0.0),120);
	//vec3 specular = light.specular * spec * specular_strength; //* texture(shinyTex, TexCoord).g;
	
	
	// rim (from TF2 shader)
	float NdotE = max(dot(norm, viewDir), 0.0);
	vec3 rim = vec3(0.3) * pow(1.0-NdotE, 6.0);
	rim *= 0.8;
	rim = vec3(0.0);
	
    // lambertian
	float lambert = dot(norm, lightDir);
	lambert = clamp(lambert,0.01,0.99);
	
    //vec3 result = (ambient + diffuse + specular) * objectColor + rim;
	
	//vec3 result = ambient + diffuse + specular + rim;
	//vec3 result = (lambert * objectColor) + specular + rim;
	
	//vec3 result = (lambert * diffuse) + ambient + specular + rim;
	
	vec3 result = diffuse + specular + ambient*0.2;
	
    FragColor = vec4(result, 1.0);
} 