#ifndef LOADER_H
#define LOADER_H

#include "opengl_api.h"
#include <unordered_map>
#include <string>

struct qobj_header
{
	char constant[4] = { 'Q','O','B','J' };
	uint16_t qobj_version;
	char model_name[64];

	uint32_t submesh_count;
	uint32_t texture_count;

	char padding[200];
};
struct qobj_mesh
{
	// name of mesh
	char mesh_name[64];
	// -1 if DNE 
	int diffuse_index;
	int spec_index;
	int bump_index;

	uint32_t vertex_count;
	uint32_t element_count;

	// bounding box
	float max_x, max_y, max_z;
	float min_x, min_y, min_z;

	// vertex data of vertex_count length * sizeof(Vertex);
	// element data of element_count * sizeof(int);
};

Texture load_texture_file(const char* file_name,bool flip_y=true);
Mesh load_qobj_mesh(const char* file);

void load_model_assimp(Model* m, const char* file, bool pretransform_verts);

void make_qobj_from_assimp(std::string model_name,std::string output_name, bool pretransform_verts);

void load_model_qobj(Model* m, std::string file_name);

#endif // !LOADER_H