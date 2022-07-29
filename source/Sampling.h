#ifndef SAMPLING_H
#define SAMPLING_H
#include "glm/glm.hpp"
#include <random>
using glm::vec3;
using glm::vec2;


inline float random_float()
{
	return rand() / (RAND_MAX + 1.0f);
}
inline float random_float(float min, float max)
{
	return min + (max - min) * random_float();
}
inline vec3 random_vec3(float min, float max)
{
	return vec3(random_float(min, max), random_float(min, max), random_float(min, max));
}
inline vec3 random_in_unit_sphere()
{
	while (1) {
		vec3 v = random_vec3(-1.f, 1.f);
		if (dot(v, v) >= 1) continue;
		return v;
	}
}
inline vec3 random_unit_vector()
{
	return normalize(random_in_unit_sphere());
}

inline float radical_inverse_VDC(unsigned int bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

inline vec2 hammersley_2d(unsigned int i, unsigned int N)
{
	return vec2((float)i / N, radical_inverse_VDC(i));
}

inline vec3 uniform_hemisphere(float u, float v)
{
	float phi = v * 2.0 * 3.14159;
	float cos_theta = 1.f - u;
	float sin_theta = sqrt(1.f - cos_theta * cos_theta);
	return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}
inline vec3 cosine_hemisphere(float u, float v)
{
	float phi = v * 2.0 * 3.14159;
	float cos_theta = sqrt(1.f - u);
	float sin_theta = sqrt(1.f - cos_theta * cos_theta);
	return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

inline vec3 sample_hemisphere_cos(vec3 N, int i, int num)
{
	vec2 Xi = hammersley_2d(i, num);
	vec3 tangent_sample = cosine_hemisphere(Xi.x, Xi.y);

	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0,0.0,0.0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 world_sample = tangent * tangent_sample.x + bitangent * tangent_sample.y + N * tangent_sample.z;
	return normalize(world_sample);
}

inline vec3 sample_hemisphere_uniform(vec3 N, int i, int num)
{
	vec2 Xi = hammersley_2d(i, num);
	vec3 tangent_sample = uniform_hemisphere(Xi.x, Xi.y);

	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 world_sample = tangent * tangent_sample.x + bitangent * tangent_sample.y + N * tangent_sample.z;
	return normalize(world_sample);
}



#endif // !SAMPLING_H
