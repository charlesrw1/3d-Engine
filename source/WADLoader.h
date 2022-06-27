#ifndef WADLOADER_H
#define WADLOADER_H

// Loads a .wad file (the texture file for Quake + Half-Life) and uploads the data to the GPU
// Textures are stored in the texture manager under (texture_name).wad
bool load_wad_file(const char* wad_name);

#endif // !WADLOADER_H
