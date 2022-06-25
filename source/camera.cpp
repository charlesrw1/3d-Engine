#include "camera.h"
#include "glm/gtc/matrix_transform.hpp"
#include <SDL2/SDL_events.h>


#include "app.h";
#include "renderer.h"

constexpr float PI = 3.1415926536;
constexpr float HALFPI = 3.1415926536 /2;
constexpr float QUARTERPI = 3.1415926536 /4;
constexpr float TWOPI = 3.1415926536 *2;


Camera::Camera()
{
	yaw = pitch = 0;
	fov = 45;
	znear = 0.1;
	zfar = 100;
	target_distance = 1;
	move_speed = rot_speed = 1;
	free_cam = true;
	target_lock = false;

	position = vec3(0);

	up = vec3(0, 1, 0);
	front = vec3(0, 0, 1);
	right = vec3(1, 0, 0);

	update_vectors();
	//frust.update(*this);
}
void Camera::keyboard_update(const uint8_t* keys)
{
	float camera_speed = 0.1;
	if (free_cam) {
		if (keys[SDL_SCANCODE_W])
			position += camera_speed * front;
		if (keys[SDL_SCANCODE_S])
			position -= camera_speed * front;
		if (keys[SDL_SCANCODE_A])
			position += right * camera_speed;
		if (keys[SDL_SCANCODE_D])
			position -= right * camera_speed;
		if (keys[SDL_SCANCODE_Z])
			position += camera_speed * up;
		if (keys[SDL_SCANCODE_X])
			position -= camera_speed * up;

		update_view_matrix();
		//frust.update(*this);
	}
}
void Camera::mouse_update(int xpos, int ypos)
{

	float x_off = xpos;
	float y_off = ypos;

	float sensitivity = 0.1;

	x_off *= sensitivity * 0.1;
	y_off *= sensitivity * 0.1;

	yaw += x_off;
	pitch -= y_off;

	if (pitch > HALFPI - 0.01)
		pitch = HALFPI - 0.01;
	if (pitch < -HALFPI + 0.01)
		pitch = -HALFPI + 0.01;

	if (yaw > TWOPI) {
		yaw -= TWOPI;
	}
	if (yaw < 0) {
		yaw += TWOPI;
	}

	update_vectors();
}
void Camera::update_vectors()
{
	vec3 direction;
	// x and z multiplied by cos(pitch) -> cos(+-pi/2)=0 (x/z yaw vectors affected less as pitch goes to the top or bottom pole)
	direction.x = cos(yaw) * cos(pitch);
	direction.y = sin(pitch);
	direction.z = sin(yaw) * cos(pitch);

	front = glm::normalize(direction);
	right = glm::cross(up, front);

	if (target_lock) {
		position = -front * target_distance + look_target;
		front = direction;
		/* insert collision code here */
	}
	update_view_matrix();
	//frust.update(*this);
	/* insert update frustum */
}
void Camera::scroll_wheel_update(int amt)
{
	if (free_cam) {
		fov -= amt * 2;
		global_app.r->set_fov(fov);
	}
	else {
		target_distance -= amt;
		target_distance = glm::clamp(target_distance, 1.f, 20.f);
	}
}
void Camera::update_view_matrix()
{
	view_matrix = glm::lookAt(position, position + front, up);
}
void Frustum::update(Camera& cam)
{
	right = normalize(cross(cam.up, cam.front));
	up = normalize(cross(right, cam.front));

	// position of far plane
	vec3 far_center = cam.front * cam.zfar;
	float far_h = tan(cam.fov / 2) * cam.zfar;
	float far_w = far_h * global_app.aspect_r;

	// clockwise from top left
	corners[0] = far_center - right * far_w + up * far_h;	// top left
	corners[1] = far_center + right * far_w + up * far_h;	// top right
	corners[2] = far_center + right * far_w - up * far_h;	// bottom right
	corners[3] = far_center - right * far_w - up * far_h;	// bottom left

	normals[0] = normalize(cross(corners[0], corners[1])); // top
	normals[1] = normalize(cross(corners[1], corners[2])); // right
	normals[2] = normalize(cross(corners[2], corners[3])); // bottom
	normals[3] = normalize(cross(corners[3], corners[0])); // left

	normals[4] = normalize(-cam.front); // back

	for (int i = 0; i < 4; i++) {
		offsets[i] = -dot(normals[i], cam.position);
	}
	offsets[4] = -dot(far_center+cam.position, normals[4]);

}
bool Frustum::sphere_in_frustum(vec3 pos, float radius)
{
	for (int i = 0; i < 5; i++) {
		if (dot(pos, normals[i]) + radius + offsets[i] < 0) {
			return false;
		}
	}
	return true;
}