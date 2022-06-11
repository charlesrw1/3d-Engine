#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include <unordered_map>
#include <string>

class Model;
class ModelManager
{
public:
	void init();
	void shutdown();

	void add_model(Model* model);

	Model* find_or_load(const char* model_name);
	void remove_model(const char* model_name);

	void print_info();

	void reload_all();
private:
	std::unordered_map<std::string, Model*> models;
};

extern ModelManager global_models;

#endif // !MODELMANAGER_H

