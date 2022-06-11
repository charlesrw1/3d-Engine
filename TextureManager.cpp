#include "TextureManager.h"
#include "Texture_def.h"
#include <cassert>

//TextureManager global_textures;

void TextureManager::init() {

}
void TextureManager::shutdown() {
	for (auto t : textures) {
		delete t.second;
	}
}
Texture* TextureManager::find_or_load(const char* filename, int params) {
	auto res = textures.find(filename);
	if (res != textures.end()) {
		return res->second;
	}
	// Model not found, load it
	Texture* t = new Texture;

	t->init_from_file(filename, params);
	// Error on loading image
	if (!t->is_loaded()) {
		delete t;
		return nullptr;
	}

	textures.insert({ std::string(filename), t });

	return t;
}
void TextureManager::add_texture(Texture* texture) {
	assert(texture);

	auto res = textures.find(texture->get_name());
	if (res != textures.end()) {
		return;
	}
	textures.insert({ texture->get_name(), texture });
}
void TextureManager::print_info() {

	printf("\nLoaded textures (%d): \n", (int)textures.size());
	for (auto t : textures) {
		t.second->print();
	}
	printf("---------------------\n\n");

}
Texture* TextureManager::create_scratch(const char* internal_name, int width, int height, int params)
{
	auto res = textures.find(internal_name);
	assert(res == textures.end()); 

	Texture* t = new Texture;

	t->init_empty(internal_name, width, height, params);

	textures.insert({ internal_name,t });

	return t;
}