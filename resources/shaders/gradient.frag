#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform sampler2D gradient123;

uniform vec3 sun_direction;

void main()
{
	vec3 sky_dir = normalize(TexCoords);
	float x = clamp(-((sky_dir.y-1)/2.0), 0.01,0.99);
	vec2 grad_color = vec2(x, 0.5);
	 
	vec3 sky_color = texture(gradient123, grad_color).rgb;
	
	// Add sun
	vec3 compensated = normalize(sun_direction + sky_dir*9/10);
	vec3 sun = vec3(100,100,70) * pow(dot(compensated,sun_direction),100);
	
	FragColor = vec4(sky_color + sun,1.0);
} 