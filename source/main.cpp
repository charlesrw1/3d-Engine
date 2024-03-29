﻿#include <iostream>
#include <SDL2/SDL.h>

#include "app.h"

#include "Light.h"
#include <cstring>
#include <fstream>

void output_to_obj(worldmodel_t* wm)
{
	printf("Writing .obj file\n");
	std::ofstream outfile(wm->name + ".obj");
	for (int i = 0; i < wm->verts.size(); i++) {
		outfile << "v " << wm->verts[i][0] << " " << wm->verts[i][1] << " " << wm->verts[i][2] << "\n";
	}
	for (int i = 0; i < wm->faces.size(); i++) {
		vec3 normal = wm->faces[i].plane.normal;
		outfile << "vn " << normal[0] << " " << normal[1] << " " << normal[2] << '\n';
	}
	for (int i = 0; i < wm->faces.size(); i++) {
		face_t* face = &wm->faces[i];
		if (face->dont_draw)
			continue;
		int first_vert = face->v_start;
		for (int j = 0; j < face->v_count - 2; j++) {
			outfile << "f " << first_vert + 1<< "//" << i + 1
				<< " " << first_vert + 1 + j + 1 << "//" << i +1
				<< " " << first_vert + (2 + j) % face->v_count + 1 << "//" << i + 1 << '\n';
		}
	}
	outfile.close();
}


void print_help()
{
	printf("usage: [options...] .map/.cmf\n\n"
		"Options:\n"
		" -bounces #       : number of bounces for radiosity (default 64)\n"
		" -patchgrid #     : size of radiosity patch in the grid, lower sizes dramatically slow performance (default 2.0)\n"
		" -lmdensity #     : number of lightmap texels per map unit (32 quake units = 1 unit), each texel is supersampled with an additional 4 samples (default 4.0)\n"
		" -norad           : disables radiosity, only direct lighting\n"
		" -outdoors        : don't cull away faces that point outwards\n"
		" -nopatchvistest  : disables the patch-to-face raycast to avoid lightbleeds, breaks on Quake maps\n"
		" -quakefmt        : parses the .map file using the Quake format instead of the Valve format\n"
		" -reflectivity #  : how much light the default surface reflects (default 0.5)\n"
		" -sampleofs #     : how far each supersample is from original sample (default 0.1)\n"
		" -samples #       : how many samples (raycasts) are used per radiosity patch (default 100)\n"
		" -onlycompile     : only compile the map and write to disk\n"
		" -help            : prints the help menu\n\n"

		"Notes:\n"
		"* Last parameter is just the .map/.cmf file name which will be looked for under root/resources/maps\n"
		"	If a .map file is given, the lighting calculations will be done and a .cmf file will be written to disk\n"
		"	If a .cmf file is given, the program will just load the map\n"
		"* Recommended settings for Quake maps: -patchgrid 4.0 -texeldensity 4.0 -quakefmt -nopatchvistest\n"
		"* Recommended settings for smaller test maps: -patchgrid 0.5 -texeldensity 8.0\n"
	);


}


int main(int argc, char* argv[])
{
	LightmapSettings lm_settings;
	bool quake_format = false;
	bool compile = true;
	bool dont_run = false;
	std::string file;

	for (int i = 1; i < argc; i++) {
		if (i == argc-1) {
			file = argv[i];
			break;
		}

		if (strcmp(argv[i], "-bounces")==0) {
			lm_settings.num_bounces = atoi(argv[i + 1]);
			i++;
		}
		else if (strcmp(argv[i], "-patchgrid") == 0) {
			lm_settings.patch_grid = atof(argv[i + 1]);
			i++;
		}
		else if (strcmp(argv[i], "-lmdensity") == 0) {
			lm_settings.pixel_density = atof(argv[i + 1]);
			i++;
		}
		else if (strcmp(argv[i], "-norad") == 0) {
			lm_settings.enable_radiosity = false;
		}
		else if (strcmp(argv[i], "-outdoors") == 0) {
			lm_settings.inside_map = false;
		}
		else if (strcmp(argv[i], "-nopatchvistest")==0) {
			lm_settings.test_patch_visibility = false;
		}
		else if (strcmp(argv[i], "-quakefmt") == 0) {
			quake_format = true;
		}
		else if (strcmp(argv[i], "-reflectivity") == 0) {
			lm_settings.default_reflectivity = vec3(atof(argv[i + 1]));
			i++;
		}
		else if (strcmp(argv[i], "-sampleofs") == 0) {
			lm_settings.sample_ofs = atof(argv[i + 1]);
			i++;
		}
		else if (strcmp(argv[i], "-onlycompile") == 0) {
			dont_run = true;
		}
		else if (strcmp(argv[i], "-nodirect") == 0) {
			lm_settings.no_direct = true;
		}
		else if (strcmp(argv[i], "-samples") == 0) {
			lm_settings.samples_per_patch = atoi(argv[i + 1]);
			i++;
		}
		else if (strcmp(argv[i], "-help")==0) {
			print_help();
			return 1;
		}
		else {
			printf("Unknown command: %s\n", argv[i]);
			print_help();
			return 1;
		}
	}
	size_t extnesion_end = file.rfind('.');
	if (extnesion_end == std::string::npos) {
		printf("Unknown file\n");
		print_help();
		return 1;
	}

	std::string extension = file.substr(extnesion_end);
	
	if (extension == ".cmf") {
		compile = false;
	}
	else if(extension!=".map"){
		printf("Unknown file extension, only .map and .cmf accepted\n");
		print_help();
		return 1;
	}


	App& app = global_app;
	app.init();

	if (compile) {
		app.compile_map(file, lm_settings, quake_format);
		if (dont_run) {
			app.quit();
			return 0;
		}
	}
	else {
		app.load_map(file);
	}
	output_to_obj(&app.world);
	app.setup_new_map();


	while (app.running)
	{
		app.delta_t = SDL_GetTicks() - app.last_t;
		app.last_t = SDL_GetTicks();
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			app.handle_event(event);
		}
		app.update_loop();
		app.on_render();

		SDL_GL_SwapWindow(app.window);
	}

	app.quit();
	return 0;
}
