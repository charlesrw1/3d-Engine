#ifndef  OBJECT_H
#define OBJECT_H

#include "types.h"
#include "Model_def.h"

class Object
{
public:
private:
	// Index into scene array
	u16 index{};
	u16 ID{};

	vec3 position;
	vec3 scale;

	bool hidden;
	bool selected;

	Model* render_model=nullptr;
	vec3 color;
	mat4 model_transform;
	bool transparent=false;
	float transparency=1.f;
};

#endif // ! OBJECT_H
