#ifndef VOXELIZER_H
#define VOXELIZER_H
#include <vector>
#include <glm/glm.hpp>

struct Voxel
{
	enum Flags
	{
		SOLID=1, // empty =0
		OUTSIDE=2,	// Voxel is marked as outside
		USED_FOR_AMBIENT=4	// selected for ambient already
	};
	unsigned char flags=0;
	unsigned char r, g, b;
};
struct worldmodel_t;
struct VertexArray;
const int MAX_BUNDLE_SIZE = 5;
class Voxelizer
{
public:
	Voxelizer(const worldmodel_t* wm,float voxel_size, int bundle_size);

	void move_final_samples(std::vector<glm::vec3>& spot_vec) {
		spot_vec = std::move(final_sample_spots);
	}
	void add_points(VertexArray* va);
private:
	void compute_bounds();
	void voxelize();
	void flood_fill_outside();

	void create_samples();


	Voxel& get_voxel(glm::ivec3 coord) {

		assert(coord.x >= 0 && coord.x < grid.x);
		assert(coord.y >= 0 && coord.y < grid.y);
		assert(coord.z >= 0 && coord.z < grid.z);

		return voxels.at(coord.z * (grid.x * grid.y) + coord.y * (grid.x) + coord.x);
	}

	std::vector<glm::vec3> final_sample_spots;

	std::vector<Voxel> voxels;
	glm::ivec3 grid;
	glm::ivec3 grid_start;

	// these should probablly be calculated already, oh well
	glm::vec3 map_min, map_size;

	float voxel_size;		// size of voxels in the grid
	int bundle_size;	// size of "bundle" voxel for ambient piece

	int total_solid;
	int total_outdoors;

	const worldmodel_t* wm=nullptr;
};


#endif // !VOXELIZER_H
