#pragma once
#include <windows.h>
#include <string>

std::string GetExeDirectory()
{
    char path[MAX_PATH];
    if (GetModuleFileName(NULL, path, MAX_PATH) == 0) {
        return ""; // Error handling
    }

    std::string fullPath(path);
    size_t pos = fullPath.find_last_of("\\/");
    return (pos != std::string::npos) ? fullPath.substr(0, pos) : "";
}

