#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include "Object.h"
#include "camera.h"

struct GameScene
{
	Camera main_cam;
	std::vector<Entity*> objects;

	DirectionalLight sun;
	PointLight lights[8];
	uint8_t num_lights = 0;
};


#endif // !SCENE_H
