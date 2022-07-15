#ifndef TYPES_H
#define TYPES_H

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "camera.h"
#include "opengl_api.h"
#include "geometry.h"
#include <vector>

#include "Model_def.h"

using namespace glm;



class GameObject
{
public:
	GameObject() {}

	GameObject(Model* model) : model(model) {}

	uint32_t ID{};

	// TRANSFORM
	vec3 position = vec3(0, 0, 0);
	float euler_y = 0, euler_x = 0;
	vec3 scale = vec3(1.f);

	mat4 model_matrix;
	mat4 inverse_matrix;
	bool matrix_needs_update = true;

	// RENDER INFO
	bool draw_wireframe = false;
	Model* model = nullptr;

	vec3 color = vec3(1.f);
	bool transparent = false;
	float transparency = 1.f;

	bool has_shading = true;
	bool has_shadows = true;
	bool is_drawn = true;

	bool outline_object = false;		// stencil test

	// just testing
	bool is_leaf = false;


	// Temporary data
	int closest_light;
	int closest_ambient_cube;


	void update_matrix() {
		model_matrix = mat4(1);
		model_matrix = translate(model_matrix, position);
		model_matrix = glm::rotate(model_matrix, euler_y, vec3(0, -1, 0));
		model_matrix = glm::rotate(model_matrix, euler_x, vec3(1, 0, 0));
		model_matrix = glm::scale(model_matrix, scale);

		inverse_matrix = inverse(model_matrix);
	}
};
struct DirectionalLight
{
	vec3 direction;
	vec3 ambient=vec3(0.3), diffuse=vec3(1.0), specular=vec3(1.0);
};
struct PointLight
{
	vec3 position;
	vec3 color;
	float radius;
};
#define MAX_CAMERAS 4
struct SceneData
{
	Camera cams[MAX_CAMERAS];
	uint8_t cam_num = 0;

	Camera* active_camera() {
		return &cams[cam_num];
	}

	std::vector<GameObject*> objects;

	std::vector<PointLight> point_lights;


	DirectionalLight sun;
	PointLight lights[8];
	uint8_t num_lights = 0;

	float wind_strength = 1;
	float wind_speed = 1;
	vec2 wind_dir = vec2(1, 0);

	VertexArray map_geo;
	VertexArray map_geo_edges;

	void free_objects() {
		for (int i = 0; i < objects.size(); i++) {
			delete objects[i];
		}
		objects.clear();
	}
};


#endif