#include "ModelManager.h"
#include "Model_def.h"

#include <cassert>

//ModelManager global_models;

void ModelManager::init() {

}
void ModelManager::shutdown() {
	for (auto model : models) {
		delete model.second;
	}
}
Model* ModelManager::find_or_load(const char* model_name) {
	auto res = models.find(model_name);
	if (res != models.end()) {
		return res->second;
	}
	// Model not found, load it
	Model* model = new Model;
	
	// Load here
	
	models.insert({ std::string(model_name), model });
}
void ModelManager::add_model(Model* model) {
	assert(model);

	auto res = models.find(model->get_name());
	if (res != models.end()) {
		return;
	}
	models.insert({ model->get_name(), model });
}
void ModelManager::print_info() {

	printf("\nLoaded models (%d): \n", models.size());
	for (auto model : models) {
		printf("%s\n", model.second->get_name().c_str());
	}
	printf("---------------------\n\n");

}