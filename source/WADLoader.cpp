#include "WADLoader.h"
#include "TextureManager.h"
#include <vector>
#include <fstream>
#include <cassert>
typedef unsigned char u8;

struct wadheader_t
{
	char magic[4];
	int num_entries;
	int directory_offset;
};
struct wadentry_t
{
	int offset;				// 4
	int disk_size;			// 8
	int uncompressed_size;	// 12
	char type;				// 13
	char compression;		// 14
	short dummy;			// 16
	char name[16];			// 32
};
static_assert(sizeof(wadentry_t) == 32,"Wadentry size incorrect");
static std::vector<wadentry_t> entries;
static std::vector<u8> data_buffer;

bool read_entry(std::ifstream& file) 
{
	wadentry_t entry;
	std::vector<u8> buffer(32);
	file.read((char*)buffer.data(), 32);
	memcpy(&entry, &buffer[0], 32);
	assert(entry.name[15] == '\0');
	printf("Entry read: %s", entry.name);

	entries.push_back(entry);
	
	return true;
}

static const std::string resouce_path = "resources/textures/";
bool load_wad_file(const char* wad_name)
{
	printf("Loading .wad file: %s\n", wad_name);
	std::ifstream infile(resouce_path + wad_name, std::ios::binary);
	if (!infile) {
		printf("Couldn't open .wad file\n");
		return false;
	}
	wadheader_t header;
	// Read header
	infile.read((char*)&header.magic, 4);
	static const char magic[] = { 'W','A','D','2' };
	for (int i = 0; i < 4; i++) {
		if (header.magic[i] != magic[i]) {
			printf("Couldn't recognize .wad file\n");
			return false;
		}
	}
	infile.read((char*)&header.num_entries, 4);
	infile.read((char*)&header.directory_offset, 4);
	printf("Num entries: %i\n", header.num_entries);

	infile.seekg(header.directory_offset);
	for (int i = 0; i < header.num_entries; i++) {
		if (!read_entry(infile)) {
			return false;
		}
	}
	for (int i = 0; i < header.num_entries; i++) {

	}
	return true;
}
