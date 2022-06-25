#ifndef LIGHT_H
#define LIGHT_H

struct worldmodel_t;
void create_light_map(worldmodel_t* wm);
void draw_lightmap_debug();

int light_main(int argc, char** argv);

#endif // !LIGHT_H



