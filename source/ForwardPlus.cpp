#include "renderer.h"
#include "glad/glad.h"
#include "app.h"
#include "types.h"
void Renderer::init_tiled_rendering()
{
	glGenBuffers(1, &visible_buffer);
	glGenBuffers(1, &light_buffer);
	glGenBuffers(1, &offset_buffer);

	fruxel_width = ceil(view.width / (float)FRUXEL_SIZE);
	fruxel_height = ceil(view.height / (float)FRUXEL_SIZE);

	int total_fruxels = fruxel_width * fruxel_height;

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, light_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 256 * sizeof(PointLight_GPU),NULL, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, offset_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, total_fruxels * sizeof(FruxelOffset_GPU), NULL, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, visible_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, total_fruxels * sizeof(VisibleIndex_GPU), NULL, GL_DYNAMIC_DRAW);


	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Renderer::push_lights_to_buffer()
{
	SceneData* scene = global_app.scene;
	
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, light_buffer);
	PointLight_GPU* point_lights_gpu = (PointLight_GPU*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);
	for (int i = 0; i < 256 && i<scene->point_lights.size(); i++) {
		const auto& Plcpu = scene->point_lights.at(i);
		point_lights_gpu[i].color = vec4(Plcpu.color,1.0);
		point_lights_gpu[i].pos = vec4(Plcpu.position, 1.0);
		point_lights_gpu[i].radius_and_padding = vec4(Plcpu.radius, 1.0, 1.0, 1.0);
	}
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
void Renderer::calc_fruxel_visibility()
{
	// Generate the fruxel grid

	SceneData* scene = global_app.scene;
	Camera* cam = &scene->cams[0];

	vec3 front = cam->front;
	vec3 right = normalize(cross(front, vec3(0.0, 1.0, 0.0)));
	vec3 up = normalize(cross(right, front));
	
	float half_height = tan((view.fov_y / 2.0)*(3.14159/180))*view.znear;
	float half_width = half_height * (view.width / (float)view.height);
	vec3 bottom_left = cam->position + front * view.znear - up * half_height - right * half_width;

	vec3 dir = normalize(bottom_left - cam->position);


	float x_step = (half_width * 2) / (fruxel_width - 1);
	float y_step = (half_height * 2) / (fruxel_height - 1);

	plane_t xplanes[30];
	plane_t yplanes[30];

	debug_point(cam->position, vec3(1, 1, 0));

	debug_line(cam->position,cam->position+dir*100.f, vec3(0, 1, 1));


	for (int i = 0; i < fruxel_width; i++) {
		
		
		vec3 point = bottom_left + x_step * i * right;

		debug_line(cam->position, cam->position + normalize(point - cam->position) * 10.f,vec3(0,0,1));

		xplanes[i].normal = normalize(cross(point - cam->position,up));
		xplanes[i].d = -dot(xplanes[i].normal, point);
		debug_line(point + front * 2.f, point + front * 2.f + xplanes[i].normal, vec3(1, 0, 0));
	}
	for (int i = 0; i < fruxel_height; i++) {
		vec3 point = bottom_left + y_step * i * up;
		
		debug_line(cam->position, cam->position + normalize(point - cam->position) * 10.f, vec3(0, 0, 1));
		
		yplanes[i].normal = normalize(cross(point - cam->position, -right));
		yplanes[i].d = -dot(yplanes[i].normal, point);

		debug_line(point + front * 2.f, point + front * 2.f + yplanes[i].normal, vec3(0, 1, 0));
	}
}