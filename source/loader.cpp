#include "loader.h"
#include <SDL2/SDL_image.h>

//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image/stb_image.h"

#include <fstream>
#include <cassert>
#include <iostream>

#include "assimp/importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

const std::string texture_path = "resources/textures/";
const std::string model_path = "resources/models/";
/*
Texture load_texture_file(const char* file, bool flip_y)
{
	std::string true_path = texture_path + file;
	int x, y, n;
	stbi_set_flip_vertically_on_load(flip_y);
	uint8_t* data = stbi_load(true_path.c_str(), &x, &y, &n, STBI_rgb_alpha);
	if (data == NULL) {
		printf("\x1B[31mCouldn't load texture: %s\x1B[37m\n", true_path.c_str());
		stbi_image_free(data);
		return Texture();
	}
	printf("%s loaded. W: %d, H: %d\n", file, x, y);
	
	Texture ret;
	ret.init_standard(data, x, y);

	stbi_image_free(data);
	return ret;
}

Mesh load_qobj_mesh(const char* file)
{
	unsigned int verticies_count;
	unsigned int elements_count;

	std::vector<Vertex> vertex_buffer;
	std::vector<unsigned int> element_buffer;
	std::string true_path = model_path + file;
	std::ifstream infile(true_path, std::ios::binary);
	if (!infile) {
		printf("Couldn't open file: %s", file);
		exit(1);
	}

	char buffer[sizeof(vec3)];

	infile.read(buffer, sizeof(unsigned int));
	memcpy(&verticies_count, buffer, sizeof(unsigned int));
	vertex_buffer.resize(verticies_count);
	infile.read((char*)vertex_buffer.data(), (long long)verticies_count * sizeof(Vertex));

	infile.read(buffer, sizeof(unsigned int));
	memcpy(&elements_count, buffer, sizeof(unsigned int));
	element_buffer.resize(elements_count);
	infile.read((char*)element_buffer.data(), (long long)elements_count * sizeof(unsigned int));

	infile.read(buffer, sizeof(vec3));
	vec3 dimensions;
	memcpy(&dimensions, &buffer, sizeof(vec3));

	infile.close();

	printf("%s loaded. Total verticies: %i. Dimensions: %fm, %fm, %fm\n", file, (int)vertex_buffer.size(), dimensions.x, dimensions.y, dimensions.z);

	return Mesh(vertex_buffer.data(), element_buffer.data(), verticies_count, elements_count);
}
Model::SubMesh process_mesh(aiMesh* mesh, const aiScene* scene);
void process_assimp_node(aiNode* node, Model* model, const aiScene* scene);
void load_model_assimp(Model* model, const char* file, bool pretransform_verts)
{
	assert(model != nullptr);
	std::string true_path = model_path + file;
	Assimp::Importer imp;
	const aiScene* scene = imp.ReadFile(true_path, aiProcess_Triangulate | aiProcess_FlipUVs | ((pretransform_verts)?aiProcess_PreTransformVertices:0));
	if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
		printf("\x1B[31mCouldn't load model: %s\x1B[37m: %s\n", file, imp.GetErrorString());
		return;
	}
	process_assimp_node(scene->mRootNode, model, scene);

	imp.FreeScene();
}
void process_assimp_node(aiNode* node, Model* model, const aiScene* scene)
{
	//std::cout << std::string(depth, ' ') << "NODE: " << node->mName.C_Str() << std::endl;
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		//std::cout << std::string(depth, ' ') << "-MESH: " <<mesh->mName.C_Str() << std::endl;
		model->meshes.push_back(process_mesh(mesh, scene));
		
		model->aabb.max.x = max(model->aabb.max.x, (float)mesh->mAABB.mMax.x);
		model->aabb.max.y = max(model->aabb.max.y, (float)mesh->mAABB.mMax.y);
		model->aabb.max.z = max(model->aabb.max.z, (float)mesh->mAABB.mMax.z);

		model->aabb.min.x = min(model->aabb.min.x, (float)mesh->mAABB.mMin.x);
		model->aabb.min.y = min(model->aabb.min.y, (float)mesh->mAABB.mMin.y);
		model->aabb.min.z = min(model->aabb.min.z, (float)mesh->mAABB.mMin.z);

	}
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		process_assimp_node(node->mChildren[i], model, scene);
	}
}
inline vec3 to_vec3(aiVector3D vec) {
	return vec3(vec.x, vec.y, vec.z);
}
inline vec2 to_vec2(aiVector3D vec) {
	return vec2(vec.x, vec.y);
}
inline bool process_texture(aiMaterial* mat, aiTextureType type, Texture** texture) {

	// TEMPORARY
	//assert(texture != nullptr);
	if (mat->GetTextureCount(type) == 0) return false;
	if (type == aiTextureType_SPECULAR) {
		printf("df");
	}

	aiString str;
	mat->GetTexture(type, 0, &str);
// MEMORY LEAK!!
*texture = new Texture();
	**texture = load_texture_file(str.C_Str(),false);
	return true;
}
Model::SubMesh process_mesh(aiMesh* mesh, const aiScene* scene)
{
	std::vector<Vertex> verts;
	std::vector<uint32_t> indicies;

	for (int i = 0; i < mesh->mNumVertices;i++) {
		verts.push_back(
			Vertex(to_vec3(mesh->mVertices[i]),
			to_vec3(mesh->mNormals[i]),
			to_vec2(mesh->mTextureCoords[0][i])));
	}
	for (int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (int v = 0; v < face.mNumIndices; v++) {
			indicies.push_back(face.mIndices[v]);
		}
	}

	Model::SubMesh submesh;
	submesh.mesh = Mesh(verts.data(), indicies.data(), verts.size(), indicies.size());

	if (mesh->mMaterialIndex >= 0) {
		aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
		// DIFFUSE
		process_texture(mat, aiTextureType_DIFFUSE, &submesh.mat.diffuse);
		// SPECULAR
		process_texture(mat, aiTextureType_SPECULAR, &submesh.mat.specular);

	}

	return submesh;

} 

struct meshdata_t
{
	qobj_mesh header;
	std::vector<Vertex> verts;
	std::vector<uint32_t> indicies;
};
struct filestate_t
{
	std::vector<meshdata_t> meshes;
	std::vector<std::string> textures;

	qobj_header model_header;
};

const uint16_t CURRENT_VERSION = 2;

meshdata_t process_mesh(aiMesh* mesh, const aiScene* scene, filestate_t* state);
void process_assimp_node(aiNode* node, const aiScene* scene, filestate_t* state);
void serialize_to_disk(filestate_t* state,std::string name);
void make_qobj_from_assimp(std::string model_name, std::string new_name, bool pretransform_verts)
{
	std::string true_path = model_path + model_name;
	Assimp::Importer imp;
	const aiScene* scene = imp.ReadFile(true_path, aiProcess_Triangulate | aiProcess_FlipUVs | ((pretransform_verts) ? aiProcess_PreTransformVertices : 0));
	if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
		printf("\x1B[31mCouldn't load model: %s\x1B[37m: %s\n", new_name.c_str(), imp.GetErrorString());
		return;
	}

	filestate_t state;

	process_assimp_node(scene->mRootNode, scene, &state);

	state.model_header.qobj_version = CURRENT_VERSION;
	assert(new_name.length() < 63);
	memcpy(state.model_header.model_name, new_name.c_str(), new_name.length() + 1);
	state.model_header.submesh_count = state.meshes.size();
	state.model_header.texture_count = state.textures.size();

	serialize_to_disk(&state,new_name);

}
void process_assimp_node(aiNode* node, const aiScene* scene, filestate_t* state)
{
	//std::cout << std::string(depth, ' ') << "NODE: " << node->mName.C_Str() << std::endl;
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		//std::cout << std::string(depth, ' ') << "-MESH: " <<mesh->mName.C_Str() << std::endl;
		state->meshes.push_back(process_mesh(mesh, scene,state));
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		process_assimp_node(node->mChildren[i], scene,state);
	}
}

int get_texture_index(filestate_t* state, aiMaterial* mat, aiTextureType type)
{
	if (mat->GetTextureCount(type) == 0) return -1;
	aiString str;
	mat->GetTexture(type, 0, &str);

	std::string comp(str.C_Str());	// does std string allocate?

	for (int i = 0; i < state->textures.size(); i++) {
		if (state->textures.at(i) == comp) {
			return i;
		}
	}

	state->textures.push_back(comp);
	return state->textures.size() - 1;
}

meshdata_t process_mesh(aiMesh* mesh, const aiScene* scene, filestate_t* state)
{
	std::vector<Vertex> verts;
	std::vector<uint32_t> indicies;

	for (int i = 0; i < mesh->mNumVertices; i++) {
		verts.push_back(
			Vertex(to_vec3(mesh->mVertices[i]),
				to_vec3(mesh->mNormals[i]),
				to_vec2(mesh->mTextureCoords[0][i])));
	}
	for (int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (int v = 0; v < face.mNumIndices; v++) {
			indicies.push_back(face.mIndices[v]);
		}
	}

	meshdata_t meshdata;
	meshdata.header.element_count = indicies.size();
	meshdata.header.vertex_count = verts.size();

	meshdata.indicies = std::move(indicies);
	meshdata.verts = std::move(verts);

	assert(mesh->mName.length < 63);
	memcpy(meshdata.header.mesh_name, mesh->mName.C_Str(), mesh->mName.length+1);


	if (mesh->mMaterialIndex >= 0) {
		aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
		// DIFFUSE
		meshdata.header.diffuse_index = get_texture_index(state, mat, aiTextureType_DIFFUSE);
		// SPECULAR
		meshdata.header.spec_index = get_texture_index(state, mat, aiTextureType_SPECULAR);
		meshdata.header.bump_index = -1;
	}

	return meshdata;
}
// not portable at all, temporary for now
void serialize_to_disk(filestate_t* state,std::string name)
{
	std::ofstream outfile(model_path+name+".qobj", std::ios::binary);

	outfile.write((char*)&state->model_header, sizeof(qobj_header));
	for (int i = 0; i < state->textures.size(); i++) {
		outfile.write((char*)state->textures.at(i).c_str(), state->textures.at(i).length() + 1);
	}
	for (int i = 0; i < state->meshes.size(); i++) {
		outfile.write((char*)&state->meshes.at(i).header, sizeof(qobj_mesh));
		outfile.write((char*)state->meshes.at(i).verts.data(), state->meshes.at(i).verts.size() * sizeof(Vertex));
		outfile.write((char*)state->meshes.at(i).indicies.data(), state->meshes.at(i).indicies.size() * sizeof(uint32_t));
	}

	outfile.close();
}
void load_textures(std::vector<Texture*>& textures, std::ifstream& stream, int texture_count)
{
	for (int i = 0; i < texture_count; i++) {
		std::string res;
		res.reserve(64);
		while (stream.peek() != '\0') {
			res.push_back(stream.get());
		} 
		// advance stream
		stream.get();
		std::cout << "TEX: " << res << '\n';

		textures.push_back(new Texture(load_texture_file(res.c_str(),false)));
	}
}
void load_model_qobj(Model* m, std::string name)
{
	std::ifstream infile(model_path + name, std::ios::binary);
	if (!infile) {
		printf("Couldn't open QOBJ file: %s\n", name.c_str());
		return;
	}
	qobj_header header;
	infile.read((char*)&header, sizeof(qobj_header));
	bool constant = false;
	char qobj_const[] = { 'Q','O','B','J' };
	for (int i = 0; i < 4; i++) {
		constant |= qobj_const[i] != header.constant[i];
	}
	if (constant) {
		printf("Not recognized as QOBJ file\n");
		return;
	}
	if (header.qobj_version != CURRENT_VERSION) {
		printf("QOBJ version out-of-date: %hu (current: %hu)\n", header.qobj_version, CURRENT_VERSION);
	}

	std::vector<Texture*> textures;
	load_textures(textures,infile,header.texture_count);

	for (int i = 0; i < header.submesh_count; i++) {
		Model::SubMesh sm;
		qobj_mesh sm_header;
		infile.read((char*)&sm_header, sizeof(qobj_mesh));
		std::cout << "Submesh: " << sm_header.mesh_name << '\n';
		std::vector<Vertex> verts;
		verts.resize(sm_header.vertex_count);
		std::vector<uint32_t> indicies;
		indicies.resize(sm_header.element_count);
		infile.read((char*)verts.data(), sm_header.vertex_count*sizeof(Vertex));
		infile.read((char*)indicies.data(), sm_header.element_count*sizeof(uint32_t));


		sm.mesh = Mesh(verts.data(), indicies.data(), verts.size(), indicies.size());

		if (sm_header.diffuse_index != -1) {
			sm.mat.diffuse = textures.at(sm_header.diffuse_index);
		}
		if (sm_header.spec_index != -1) {
			sm.mat.specular = textures.at(sm_header.spec_index);
		}

		m->meshes.push_back(sm);
	}
}*/