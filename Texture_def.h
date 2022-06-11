#ifndef TEXTUREDEF_H
#define TEXTUREDEF_H
#include <string>

using u32 = uint32_t;
using u8 = uint8_t;

enum TParams
{
	FILP_Y		=1,
	GEN_MIPS	=2,
	HAS_ALPHA	=4,
	NEAREST		=8,
	CLAMP		=16,
};


class Texture
{
public:
	~Texture();

	void init_empty(const char* internal_name, int width, int height, int params = 0);
	void init_from_file(const char* filename, int params = 0);

	void bind(int binding) const;

	void purge();

	// Estimate of memory storage on the GPU
	u32 memory_usage() const;
	void print() const;

	u32 get_ID() const { return ID; }
	bool is_loaded() const { return loaded; }
	const std::string& get_name() const { return name; }

private:
	// OpenGL calls, thread unsafe
	void internal_init(u8* data);

	int width{};
	int height{};
	int params{};

	std::string name;
	bool loaded=false;
	u8* raw_data=nullptr;

	// OpenGL specific
	u32 ID{};

};

#endif // !TEXTUREDEF_H
