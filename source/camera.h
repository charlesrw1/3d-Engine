#ifndef CAMERA_H
#define CAMERA_H

#include "glm/glm.hpp"
using namespace glm;

struct View
{
	int x = 0, y = 0;
	int width = 256, height = 256;

	bool orthographic=false;
	float fov_y;

	float znear, zfar;
};

struct Camera;
struct Frustum
{
	vec3 normals[5];
	float offsets[5];

	vec3 corners[4];

	vec3 right;
	vec3 up;

	void update(Camera& cam);
	bool sphere_in_frustum(vec3 point, float radius);
};

struct Camera
{
	vec3 position=vec3(0);
	vec3 front; 
	vec3 look_target=vec3(0);
	vec3 up;
	vec3 right;
	
	mat4 view_matrix;
	
	float yaw, pitch;
	float target_distance;
	float move_speed, rot_speed;
	float znear, zfar;
	float fov;

	bool free_cam;
	bool target_lock;

	Frustum frust;

	Camera();

	mat4 get_view_matrix() {
		return view_matrix;
	}
	// input update
	void keyboard_update(const uint8_t* key_state);
	void mouse_update(int x_pos, int y_pos);
	void scroll_wheel_update(int amt);


	void update_vectors();
	inline void update_view_matrix();
};


#endif // !CAMERA_H
