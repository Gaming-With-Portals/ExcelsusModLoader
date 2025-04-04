#include "modMgmt.h"
#include <Windows.h>

bool LoadASIMod(std::string asiPath) {
    char origPath[MAX_PATH];
    GetCurrentDirectoryA(sizeof(origPath), origPath);

    auto h = LoadLibraryA(asiPath.c_str());

    if (h != nullptr)
    {
        if (auto init = (void(*)())GetProcAddress(h, "InitializeASI"); init)
            init();
    }
    else {
        return false;
    }

    SetCurrentDirectoryA(origPath);
    return true;
}


int Mod::Initalize(std::string modRoot) {

	rootDirectory = modRoot;

	std::ifstream file(modRoot + "\\eml.json");
    if (!file) {
        return -2; // Fail condition
    }

    json j;
    file >> j;

    if (j.contains("ModDirectories") && j["ModDirectories"].is_array()) {
        for (const auto& modDir : j["ModDirectories"]) {
            std::string path = modDir.value("path", "");  
            std::string condition = modDir.value("condition", "None");


            // Check if "asiFiles" exists
            if (modDir.contains("asiFiles") && modDir["asiFiles"].is_array()) {
                for (const auto& asiFile : modDir["asiFiles"]) {
                    LoadASIMod(rootDirectory + "\\" + path + "\\" + (std::string)asiFile);
                }
            }
        }
    }
    else {
        return -1; // Fail condition
    }

    return 0;
}

	


