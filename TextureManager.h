#ifndef TEXTUREMANAGER_H
#define TEXTUREMANAGER_H
#include <unordered_map>
#include <string>

class Texture;
class TextureManager
{
public:
	void init();
	void shutdown();

	void add_texture(Texture* texture);

	Texture* find_or_load(const char* name, int params=0);

	Texture* find(const char* name);

	Texture* remove_texture(const char* name);

	Texture* create_scratch(const char* internal_name, int width, int height, int params = 0);

	void print_info();

	void reload_all();

private:
	std::unordered_map<std::string, Texture*> textures;
};

//extern TextureManager global_textures;


#endif // !TEXTUREMANAGER_H
