#include "Texture_def.h"
#include "glad/glad.h"

//#include "stb_image/stb_image.h"
static const std::string resouce_path = "resources/textures/";

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
	printf("%s, %i x %i, %u KB\n", name.c_str(), width, height, memory_usage());
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
void Texture::init_from_file(const char* filename, int params)
{
	std::string true_path = resouce_path + filename;
	int x{}, y{}, n{};
//	stbi_set_flip_vertically_on_load(params&TParams::FILP_Y);
	//uint8_t* data = stbi_load(true_path.c_str(), &x, &y, &n, STBI_rgb_alpha);
//	if (data == NULL) {
	//	printf("\x1B[31mCouldn't load texture: %s\x1B[37m\n", true_path.c_str());
	///	stbi_image_free(data);
		//return;
	//}
	printf("%s loaded. W: %d, H: %d\n", filename, x, y);

	if (n == 4) {
		params |= TParams::HAS_ALPHA;
	}

	width = x;
	height = y;
	this->params = params;
	name = filename;

	//internal_init(data);

	//stbi_image_free(data);
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

	}
	if (params & TParams::CLAMP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}