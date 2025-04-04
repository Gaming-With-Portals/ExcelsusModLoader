#pragma once
#include "string"
#include "vector"
#include "unordered_map"
#include "include/nlohmann/json.hpp"
#include <fstream>

using json = nlohmann::json;

class Mod {
public:
	std::unordered_map<std::string, std::string> fileMap; // Maps in game file request calls (i.e em/em0310.dat) to absolute mod files (i.e F:\Metal Gear Rising\Mods\testMod\modFiles\em0310.dat)
	std::string rootDirectory;
	bool isValid;
	std::vector<std::string> parameters;

	int Initalize(std::string);



};