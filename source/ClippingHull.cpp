#include "MapParser.h"


vec3	hull_size[3][2] = {
{ {0, 0, 0}, {0, 0, 0} },
{ {-16,-16,-32}, {16,16,24} },
{ {-32,-32,-64}, {32,32,24} }

};

#define MAX_HULL_POINTS 32
static vec3 hull_points[MAX_HULL_POINTS];
int num_hull_points = 0;

static vec3 hull_corners[MAX_HULL_POINTS*8];
int num_hull_corners = 0;

