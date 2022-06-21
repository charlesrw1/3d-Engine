#include "Model_def.h"

#include "assimp/importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"


#include "TextureManager.h"
#include "Texture_def.h"

#include "glad/glad.h"

#include "Map_def.h"

const std::string model_path = "resources/models/";



void M_upload_mesh(RenderMesh* rm, const RenderVert* verticies, const uint32_t* elements, int vert_count, int element_count)
{
	assert(verticies && elements);
	rm->num_indices = element_count;
	rm->num_verts = vert_count;

	glGenVertexArrays(1, &rm->vao);
	glGenBuffers(1, &rm->vbo);
	glGenBuffers(1, &rm->ebo);

	glBindVertexArray(rm->vao);
	glBindBuffer(GL_ARRAY_BUFFER, rm->vbo);
	glBufferData(GL_ARRAY_BUFFER, vert_count * sizeof(RenderVert), verticies, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rm->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, element_count * sizeof(uint32_t), elements, GL_STATIC_DRAW);
	// POSITION
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// NORMALS
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// UV
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}



void M_upload_lightmap_mesh(RenderMesh* rm, const LightmapVert* verticies, const uint32_t* elements, int vert_count, int element_count)
{
	assert(verticies && elements);
	rm->num_indices = element_count;
	rm->num_verts = vert_count;

	glGenVertexArrays(1, &rm->vao);
	glGenBuffers(1, &rm->vbo);
	glGenBuffers(1, &rm->ebo);

	glBindVertexArray(rm->vao);
	glBindBuffer(GL_ARRAY_BUFFER, rm->vbo);
	glBufferData(GL_ARRAY_BUFFER, vert_count * sizeof(LightmapVert), verticies, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rm->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, element_count * sizeof(uint32_t), elements, GL_STATIC_DRAW);
	// POSITION
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// NORMALS
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// UV
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
	// LIGHTMAP UV
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(8 * sizeof(float)));
	glEnableVertexAttribArray(3);

	glBindVertexArray(0);
}

RenderMesh process_mesh(aiMesh* mesh, const aiScene* scene);
void process_assimp_node(aiNode* node, Model* model, const aiScene* scene);
void load_model_assimp(Model* model, const char* file, bool pretransform_verts)
{
	assert(model != nullptr);
	std::string true_path = model_path + file;
	Assimp::Importer imp;
	const aiScene* scene = imp.ReadFile(true_path, aiProcess_Triangulate | aiProcess_FlipUVs | ((pretransform_verts) ? aiProcess_PreTransformVertices : 0));
	if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
		printf("\x1B[31mCouldn't load model: %s\x1B[37m: %s\n", file, imp.GetErrorString());
		return;
	}
	// Get textures loading on different threads
	for (int i = 0; i < scene->mNumMaterials; i++) {
		aiMaterial* m = scene->mMaterials[i];
		if (m->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString str;
			m->GetTexture(aiTextureType_DIFFUSE, 0, &str);
			global_textures.find_or_load(str.C_Str(), GEN_MIPS);
		}
		if (m->GetTextureCount(aiTextureType_SPECULAR) != 0) {
			aiString str;
			m->GetTexture(aiTextureType_SPECULAR, 0, &str);
			global_textures.find_or_load(str.C_Str(), GEN_MIPS);
		}
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
		model->append_mesh(process_mesh(mesh, scene));

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

	aiString str;
	mat->GetTexture(type, 0, &str);
	
	*texture = global_textures.find_or_load(str.C_Str(), GEN_MIPS);
	return true;
}
RenderMesh process_mesh(aiMesh* mesh, const aiScene* scene)
{
	std::vector<RenderVert> verts;
	std::vector<uint32_t> indicies;

	for (int i = 0; i < mesh->mNumVertices; i++) {
		verts.push_back(
			RenderVert(to_vec3(mesh->mVertices[i]),
				to_vec3(mesh->mNormals[i]),
				to_vec2(mesh->mTextureCoords[0][i])));
	}
	for (int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (int v = 0; v < face.mNumIndices; v++) {
			indicies.push_back(face.mIndices[v]);
		}
	}

	RenderMesh submesh;
	M_upload_mesh(&submesh, verts.data(), indicies.data(), verts.size(), indicies.size());
	

	if (mesh->mMaterialIndex >= 0) {
		aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
		// DIFFUSE
		process_texture(mat, aiTextureType_DIFFUSE, &submesh.diffuse);
		// SPECULAR
		process_texture(mat, aiTextureType_SPECULAR, &submesh.specular);

	}

	return submesh;

}

bool Model::init_from_file(const char* file)
{
	load_model_assimp(this, file, false);
	name = file;
	loaded = true;

	return true;
}