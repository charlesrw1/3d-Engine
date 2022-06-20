#include "Texture_def.h"
#include "glad/glad.h"

#include <iostream>
#include <mutex>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
static const std::string resouce_path = "resources/textures/";

#include "TextureManager.h"

Texture::~Texture()
{
	glDeleteTextures(1, &ID);
}
void Texture::bind(int binding) const
{
	glActiveTexture(GL_TEXTURE0 + binding);
	glBindTexture(GL_TEXTURE_2D, ID);
}
void Texture::purge()
{
	glDeleteTextures(1, &ID);
	loaded = false;
}
u32 Texture::memory_usage() const
{
	u32 total = 0;
	total += loaded * width * height * (bool(params & TParams::HAS_ALPHA) + 3);
	if (params & TParams::GEN_MIPS) total *= 1.3334f;
	return total;
}
void Texture::print() const
{
	printf("%s, %i x %i, %u KB\n", name.c_str(), width, height, memory_usage()/1000);
}
void Texture::init_empty(const char* internal_name, int width, int height, int params)
{	
	if (internal_name) {
		name = internal_name;
	}
	else {
		name = "empty";
	}

	this->width = width;
	this->height = height;
	this->params = params;
	
	internal_init(NULL);
}

static std::mutex tmanager_queue;

void Texture::init_from_file(std::string filename, int params)
{
	std::string true_path = resouce_path + filename;
	int x{}, y{}, n{};
	stbi_set_flip_vertically_on_load(params&TParams::FILP_Y);
	uint8_t* data = stbi_load(true_path.c_str(), &x, &y, &n, STBI_rgb_alpha);
	if (data == NULL) {
		printf("\x1B[31mCouldn't load texture: %s\x1B[37m\n", true_path.c_str());
		stbi_image_free(data);
		return;
	}

	params |= TParams::HAS_ALPHA;

	width = x;
	height = y;
	this->params = params;
	name = filename;

	if (params & TParams::LOAD_NOW) {
		internal_init(data);
		stbi_image_free(data);
	}
	else {
		raw_data = data;

		std::lock_guard<std::mutex> lock(tmanager_queue);
		global_textures.queue_for_upload(this);
	}
}

void Texture::internal_init(u8* data) 
{
	if (loaded) purge();

	glGenTextures(1, &ID);
	glBindTexture(GL_TEXTURE_2D, ID);

	bool has_alpha = params & TParams::HAS_ALPHA;
	loaded = true;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB + has_alpha, width, height, 0, GL_RGB + has_alpha, GL_UNSIGNED_BYTE, data);

	if (params & TParams::GEN_MIPS) {
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if (params & TParams::NEAREST) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	if (params & TParams::CLAMP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
}
void Texture::upload_raw_data()
{
	internal_init(raw_data);
	stbi_image_free(raw_data);
	raw_data = nullptr;
	print();
}