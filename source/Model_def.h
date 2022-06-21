#ifndef MODEL_H
#define MODEL_H
#include <string>
#include <vector>
#include "glm/glm.hpp"

using u32 = uint32_t;
using namespace glm;

class Texture;
struct RenderVert
{
	vec3 position;
	vec3 normal;
	vec2 uv;

	RenderVert() {}
	RenderVert(vec3 pos, vec3 normal, vec2 uv) 
		: position{ pos }, normal{ normal }, uv{ uv } {}
};
struct RenderMesh
{
	// Material data
	Texture* diffuse = nullptr;
	Texture* specular = nullptr;
	Texture* normal = nullptr;

	bool loaded = false;

	// Surface data
	int num_verts{};
	RenderVert* verts=nullptr;
	int num_indices{};
	u32* indicies=nullptr;

	// OpenGL specific
	u32 vbo{};
	u32 vao{};
	u32 ebo{};
};
struct LightmapVert;
// Creates a VBO,VAO,EBO and uploads data to gpu, init_from_file calls this, but it can be called on its own as well
void M_upload_mesh(RenderMesh* rm, const RenderVert* verticies, const uint32_t* elements, int vert_count, int element_count);
void M_upload_lightmap_mesh(RenderMesh* rm, const LightmapVert* verticies, const uint32_t* elements, int vert_count, int element_count);


class Model
{
public:
	~Model() {}

	bool init_from_file(const char* filename);

	const std::string& get_name() {
		return name;
	}
	void print();
	void load_model();
	void purge_model();

	int num_meshes() { return meshes.size(); }
	const RenderMesh* mesh(int n) { return &meshes.at(n); }
	bool is_loaded() const { return loaded; }

	void append_mesh(RenderMesh mesh) { meshes.push_back(mesh); }

private:
	std::vector<RenderMesh> meshes;
	std::string name;
	bool loaded=false;
};

#endif // !MODEL_H
