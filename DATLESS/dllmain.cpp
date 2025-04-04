#include "pch.h"
#include <assert.h>
#include "gui.h"
#include <Events.h>

#include "tools.h"
#include "imgui/imgui.h"
#include "include/MinHook.h"
#include <string>
#include <filesystem>
#include "modMgmt.h"

std::vector<Mod*> mods;


namespace fs = std::filesystem;

bool debug = true;
bool modLoaderFailed = false;

class Plugin
{
public:
	static inline void InitGUI()
	{
		Events::OnDeviceReset.before += gui::OnReset::Before;
		Events::OnDeviceReset.after += gui::OnReset::After;
		Events::OnEndScene += gui::OnEndScene; 
		Events::OnGameStartupEvent += [] {


		};
		/* // Or if you want to switch it to Present
		Events::OnPresent += gui::OnEndScene;
		*/
	}

	Plugin()
	{
		InitGUI();

		std::string exeDir = GetExeDirectory();

		if (fs::exists(exeDir + "\\cpkredir.ini")) {
			MessageBoxA(nullptr, "RMM Detected! This may cause issues, it's recommended you remove all RMM files before using this modloader.",  "ExcelsusModLoader - DATLESS", 0);
			modLoaderFailed = true;
			return;
		}

		if (!fs::exists(exeDir + "\\Mods")) {
			MessageBoxA(nullptr, "No 'Mods' folder, ExcelsusModLoader was not set up correctly.", "ExcelsusModLoader - DATLESS", 0);
			modLoaderFailed = true;
			return;
		}

		if (!fs::exists(exeDir + "\\Mods\\database.json")) {
			MessageBoxA(nullptr, "No 'database.json' file, ExcelsusModLoader was not set up correctly.", "ExcelsusModLoader - DATLESS", 0);
			modLoaderFailed = true;
			return;
		}


		std::ifstream file(exeDir + "\\Mods\\database.json");

		json j;
		file >> j;


		for (const auto& mod : j["mods"]) {
			std::string id = mod["id"];
			int priority = mod["priority"];

			if (fs::exists(exeDir + "\\Mods\\" + id)) {
				Mod* mod = new Mod();
				if (mod->Initalize(exeDir + "\\Mods\\" + id) == 0) {
					mods.insert(mods.begin() + priority, mod);
				}
			}

		}


	}
} plugin;

void gui::RenderWindow()
{
	if (modLoaderFailed) {
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(470, 20));
		ImGui::Begin("error_display", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
		ImGui::Text("ExcelsusModLoader Critical Error, please correct and restart MGR");
		ImGui::End();
	}
	if (debug) {
		ImGui::Begin("Debug");

		for (Mod* mod : mods) {
			ImGui::Text(mod->rootDirectory.c_str());
		}


		ImGui::End();
	}
}