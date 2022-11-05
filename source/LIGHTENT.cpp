#include "Light.h"
#include <string>
#include "WorldGeometry.h"

std::vector<light_t> lights;
Skylight sky_light;

Skylight& GetSky() { return sky_light; }
std::vector<light_t>& LightList() { return lights; }

void AddLightEntities(worldmodel_t* wm)
{
	std::string work_str;
	work_str.reserve(64);
	for (int i = 0; i < wm->entities.size(); i++) {
		entity_t* ent = &wm->entities.at(i);
		auto find = ent->properties.find("classname");
		vec3 color = vec3(1.0);
		float brightness = 200;
		light_t l;

		if (find->second == "light_torch_small_walltorch") {
			color = vec3(1.0, 0.3, 0.0);
			brightness = 100;
			l.type = LIGHT_POINT;

			//continue;
		}
		else if (find->second == "light_flame_large_yellow") {
			color = vec3(1.0, 0.5, 0.0);
			brightness = 400;
			l.type = LIGHT_POINT;

			//continue;
		}
		else if (find->second == "light") {
			l.type = LIGHT_POINT;
		}
		else if (find->second == "light_fluoro") {
			color = vec3(0.8, 0.8, 1.0);
			l.type = LIGHT_POINT;

		}
		else if (find->second == "spot_light") {
			l.type = LIGHT_SPOT;
		}
		else {
			continue;
		}

		work_str = ent->properties.find("origin")->second;
		vec3 org;
		sscanf_s(work_str.c_str(), "%f %f %f", &org.x, &org.y, &org.z);
		org /= 32.f;	// scale down

		org = vec3(-org.x, org.z, org.y);

		auto brightness_str = ent->properties.find("light");
		if (brightness_str != ent->properties.end()) {
			work_str = brightness_str->second;
			brightness = std::stoi(work_str);
		}
		brightness /= 32.f;

		// "color" isn't in Quake's light entities, but it is in my own format
		auto color_str = ent->properties.find("color");
		if (color_str != ent->properties.end()) {
			work_str = ent->properties.find("color")->second;
			sscanf_s(work_str.c_str(), "%f %f %f", &color.r, &color.g, &color.b);
		}
		l.pos = org;
		l.color = color;
		l.brightness = brightness;

		if (l.type == LIGHT_SPOT) {
			float width = std::stof(ent->properties.find("width")->second);
			width = cos(radians(width));
			l.width = width;

			std::string angle = ent->properties.find("mangle")->second;
			int angles[3];
			sscanf_s(angle.c_str(), "%d %d %d", &angles[0], &angles[1], &angles[2]);

			vec3 dir = vec3(0);
			dir.x = cos(radians((float)angles[1])) * cos((radians((float)angles[0])));
			dir.y = sin((radians((float)angles[0])));
			dir.z = -sin(radians((float)angles[1])) * cos(radians((float)angles[0]));

			l.normal = -dir;
		}

		lights.push_back(l);
	}
	
	for (const auto& e : wm->entities)
	{
		if (e.get_classname() != "enviorment_light")
			continue;

		std::string angle = e.properties.find("mangle")->second;
		int angles[3];
		sscanf_s(angle.c_str(), "%d %d %d", &angles[0], &angles[1], &angles[2]);

		vec3 sun_direction = vec3(0);
		sun_direction.x = cos(radians((float)angles[1])) * cos((radians((float)angles[0])));
		sun_direction.y = sin((radians((float)angles[0])));
		sun_direction.z = -sin(radians((float)angles[1])) * cos(radians((float)angles[0]));

		sky_light.has_sun = true;
		//sky_light.sky_color = vec3(1);
		sky_light.sun_dir = -normalize(sun_direction);
		break;
	}


}