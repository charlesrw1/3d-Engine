#ifndef  OBJECT_H
#define OBJECT_H

#include "types.h"
#include "Model_def.h"

#include <unordered_map>
// Not implemented yet
class Entity
{
public:

protected:
	// Index into scene array
	u16 index{};
	u16 ID{};

	vec3 position;
	vec3 scale;

	bool hidden;
	bool selected;

	vec3 color;
	mat4 model_transform;
	bool transparent=false;
	float transparency=1.f;

	Model* render_model = nullptr;

	// Key value pairs from 
	std::unordered_map<std::string, std::string> spawn_args;
};

class Trigger : Entity
{

};

class Door : public Entity
{

};

class Light : public Entity
{
	u8 dynamic_light_handle{};
};

#endif // ! OBJECT_H
