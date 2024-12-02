#include "ModLoader.h"
#include <stdio.h>
#include <filesystem>
#include "injector/injector.hpp"
#include <shared.h>
#include <common.h>
#include "IniReader.h"

extern BOOL FileExists(const char* filename);

namespace fs = std::filesystem;

template <typename T>
void reallocateVector(Hw::cFixedVector<T>& vector, size_t newSize)
{
	auto newVector = (T*)malloc(sizeof(T) * newSize);
	
	if (newVector)
	{
		for (int i = 0; i < vector.m_nSize; i++)
			newVector[i] = vector.m_pBegin[i];

		vector.m_nCapacity = newSize;
		free(vector.m_pBegin);
		vector.m_pBegin = newVector;
	}
}

void openProfiles(const char *directory)
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = (HANDLE)-1;
	char searchPath[MAX_PATH];

	LOGINFO("Reading profiles...");

	int iProfileEnum = 0;

	sprintf(searchPath, "%s\\*", directory);

	hFind = FindFirstFileA(searchPath, &fd);
	if (hFind == (HANDLE)-1)
	{
		ModLoader::bInitFailed = true;
		LOGERROR("Reading failed!");
		return;
	}
	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (fd.cFileName[0] != '.' && fd.cFileName[1] != '.')
			{
				if (strlen(fd.cFileName) < 64u)
				{
					auto prof = new ModLoader::ModProfile(fd.cFileName);
					if (prof)
					{
						ModLoader::Profiles.push_back(prof);
						prof->Load(prof->m_name);

						if (prof->m_nPriority == -1) // If it doesn't have set priority, just set it as enumeration
							prof->m_nPriority = iProfileEnum;

						++iProfileEnum;

						LOGINFO("Loading %s(%s, %d)", prof->m_name.c_str(), prof->m_bEnabled ? "Enabled" : "Disabled", prof->m_nPriority);
					}
				}
				else
				{
					LOGINFO("Profile name exceeds 64 characters, simplify the name for %s!", fd.cFileName);
				}
			}
		}
	} while (FindNextFileA(hFind, &fd) != 0);

	FindClose(hFind);
	LOGINFO("Profiles reading done!");
}

void openScripts()
{
	if (ModLoader::bIgnoreScripts)
		return;

	char origPath[MAX_PATH];
	GetCurrentDirectoryA(sizeof(origPath), origPath);

	LOGINFO("Loading scripts...");

	int scriptsAmount = 0;

	for (auto& prof : ModLoader::Profiles)
	{
		if (!prof->m_bEnabled)
			continue;

		prof->FileWalk([&](ModLoader::ModProfile::File* file)
			{
				// Using strlow just to be sure if the asi may contain high case ".asi"
				if ((strrchr(file->m_path, '\\') + 1)[0] == '.' || (strrchr(file->m_path, '\\') + 1)[1] == '.')
					return;

				if (strcmp(Utils::strlow((const char*)&file->m_path[strlen(file->m_path) - 4]), ".asi") || file->m_bInSubFolder) // Do not load if not asi or asi is in sub folder // changed to strcmp, since if we just set .asi before extension, it will load anyway
					return;

				auto h = LoadLibraryA(file->m_path);

				if (h != NULL)
				{
					if (auto init = (void(*)())GetProcAddress(h, "InitializeASI"); init)
						init();

					auto asiName = strrchr(file->m_path, '\\') + 1;

					LOGINFO("Loading %s plugin...", asiName);

					++scriptsAmount;
				}
				else
				{
					LOGERROR("%s -> Failed to load %s!", prof->m_name, file->m_path);
				}

				SetCurrentDirectoryA(origPath);
			});
	}
	LOGINFO("%d scripts loaded!", scriptsAmount);
}

inline LONGLONG GetLongFromLargeInteger(DWORD LowPart, DWORD HighPart)
{
	LARGE_INTEGER l;
	l.LowPart = LowPart;
	l.HighPart = HighPart;
	return l.QuadPart;
}

void findFiles(const char* directory, Hw::cFixedVector<ModLoader::ModProfile::File *>& files, ModLoader::ModProfile *profile, const bool bInSubFolder = false, const bool bRecursiveSearch = false)
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	char searchPath[MAX_PATH];

	sprintf(searchPath, "%s\\*", directory);

	hFind = FindFirstFileA(searchPath, &fd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		LOGERROR("Cannot read files for %s", directory);
		return;
	}
	do
	{
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			if (fd.cFileName[0] != '.' && fd.cFileName[1] != '.')
			{
				auto file = new ModLoader::ModProfile::File();
				if (file)
				{
					char path[MAX_PATH];

					sprintf(path, "%s\\%s", directory, fd.cFileName);

					file->File::File(GetLongFromLargeInteger(fd.nFileSizeLow, fd.nFileSizeHigh), path);
					file->m_bInSubFolder = bInSubFolder;

					files.push_back(file);
					// LOGINFO("%s -> %s [%s]", profile->m_name.c_str(), path, Utils::getProperSize(file->m_nSize).c_str();

					profile->m_nTotalSize += file->m_nSize;
				}
			}
		}
		else
		{
			if (bRecursiveSearch){
				if (fd.cFileName[0] != '.' && fd.cFileName[1] != '.')
				{
					char directoryPath[MAX_PATH];

					sprintf(directoryPath, "%s\\%s", directory, fd.cFileName);
					findFiles(directoryPath, files, profile, true);
				}
			}

		}

	} while (FindNextFileA(hFind, &fd) != 0);

	FindClose(hFind);
}

void ModLoader::startup()
{
	if (bInitFailed)
		return;

	Load();

	openProfiles(getModFolder().c_str());

	if (Profiles.m_nSize)
		SortProfiles();

	int enabledMods = 0;
	for (auto& profile : Profiles)
	{
		LOGINFO("%s -> Startup", profile->m_name.c_str());
		profile->Startup();
		if (profile->m_bEnabled) enabledMods++;
	}
	openScripts();
	LOGINFO("%d mod profiles loaded!", Profiles.m_nSize);
	LOGINFO("Only %d was enabled", enabledMods);

	bInit = true;
}

void ModLoader::SortProfiles()
{
	for (int i = 0; i < Profiles.m_nSize - 1; i++)
	{
		for (int j = 0; j < Profiles.m_nSize - i - 1; j++)
		{
			auto arr = Profiles.m_pBegin;
			if (arr[j]->m_nPriority < arr[j + 1]->m_nPriority)
			{
				auto temp = arr[j];
				arr[j] = arr[j + 1];
				arr[j + 1] = temp;
			}
		}
	}
}

FILE *profileFile = nullptr;

void ModLoader::Load()
{
	CIniReader ini("MGRModLoaderSettings.ini");


	bIgnoreScripts = ini.ReadBoolean("ModLoader", "IgnoreScripts", false);
	bIgnoreDATLoad = ini.ReadBoolean("ModLoader", "IgnoreFiles", false);
	bEnableLogging = ini.ReadBoolean("ModLoader", "EnableLogging", true);
	aKeys[0] = ini.ReadInteger("ModLoader", "MainKeyGUI", 0x72);
	aKeys[1] = ini.ReadInteger("ModLoader", "AdditionalKeyGUI", 0);

	for (auto& profile : Profiles)
		profile->Load(profile->m_name);
}

void ModLoader::Save()
{
	remove((getModFolder() + "\\profiles.ini").c_str());
	profileFile = fopen((getModFolder() + "\\profiles.ini").c_str(), "a");
	CIniReader ini("MGRModLoaderSettings.ini");

	ini.WriteBoolean("ModLoader", "IgnoreScripts", bIgnoreScripts);
	ini.WriteBoolean("ModLoader", "IgnoreFiles", bIgnoreDATLoad);
	ini.WriteBoolean("ModLoader", "EnableLogging", bEnableLogging);
	ini.WriteInteger("ModLoader", "MainKeyGUI", aKeys[0]);
	ini.WriteInteger("ModLoader", "AdditionalKeyGUI", aKeys[1]);

	for (auto& profile : Profiles)
		profile->Save();

	fclose(profileFile);
}

bool ModLoader::IsGUIKeyPressed()
{
	if (aKeys[0] != 0)
	{
		if (aKeys[1] != 0)
			return shared::IsKeyPressed(aKeys[0]) && shared::IsKeyPressed(aKeys[1], false);

		return shared::IsKeyPressed(aKeys[0], false);
	}

	return false;
}

Utils::String ModLoader::getModFolder()
{
	Utils::String res = "ExcelsusModLoader\\";
	return res;
}

void ModLoader::ModProfile::Load(const char* name)
{


}

void ModLoader::ModProfile::Startup()
{
	if (!m_bStarted)
	{
		LOGINFO("Starting up profile...");
		if (!m_files.m_pBegin)
		{
			m_files.m_pBegin = (File**)malloc(sizeof(File*) * 1024);
			if (m_files.m_pBegin)
			{
				m_files.m_nCapacity = 1024;
				m_files.m_nSize = 0;
				m_files.field_10 = 1;
			}
			else
			{
				LOGERROR("Failed to startup the mod!");
			}
		}

		ReadFiles();
		auto config_file = FindFile("mod.ini");
		if (config_file != nullptr)
		{
			LOGINFO("Loading RMM Mod");
			m_bIsRMMMod = true;

		}
		else {
			LOGINFO("Loading EML Mod");
			m_bIsRMMMod = false;
		}

		if (m_bIsRMMMod) {
			ReadFilesRMM();
			
		}
		else {
			ReadFiles();
		}

		
		auto newArray = (File**)malloc(sizeof(File*) * m_files.m_nSize);
		
		if (newArray)
		{
			for (int i = 0; i < m_files.m_nSize; i++)
				newArray[i] = m_files.m_pBegin[i];

			free(m_files.m_pBegin);

			m_files.m_pBegin = newArray;
			m_files.m_nCapacity = m_files.m_nSize;
		}

		if (m_bEnabled && m_ModInfo && m_ModInfo->m_pDLLs)
		{
			for (const auto& dll : *m_ModInfo->m_pDLLs)
			{
				auto file = FindFile(dll.c_str());

				if (file)
				{
					if (LoadLibraryA(file->m_path))
						LOGINFO("%s -> LoadLibrary(%s) successful", m_name, dll.c_str());
					else
						LOGINFO("%s -> Failed to load %s", m_name, dll.c_str());
				}
			}
		}
	}

	m_bStarted = true;
}

void ModLoader::ModProfile::Restart()
{
	m_bStarted = false;

	Shutdown();

	Startup();
}

void ModLoader::ModProfile::ReadFiles()
{
	findFiles(getMyPath().c_str(), m_files, this, false, true);
}

void ModLoader::ModProfile::ReadFilesRMM()
{
	CIniReader rmm_config((getMyPath() + "\\mod.ini").c_str());
	int include_dir_count = rmm_config.ReadInteger("Main", "IncludeDirCount", 1);
	std::vector< std::string > include_directories;
	for (int x = 0; x < include_dir_count; x++) {
		std::string include_dir = rmm_config.ReadString("Main", "IncludeDir" + std::to_string(x), "data000");
		if (include_dir == ".") {
			struct stat sb;
			if (stat(getMyPath() + "\\data000", &sb) == 0) {
				include_directories.push_back("data000");
			}
			if (stat(getMyPath() + "\\data001", &sb) == 0) {
				include_directories.push_back("data001");
			}
			if (stat(getMyPath() + "\\data003", &sb) == 0) {
				include_directories.push_back("data003");
			}
			if (stat(getMyPath() + "\\data004", &sb) == 0) {
				include_directories.push_back("data004");
			}
			if (stat(getMyPath() + "\\data005", &sb) == 0) {
				include_directories.push_back("data005");
			}
			if (stat(getMyPath() + "\\data006", &sb) == 0) {
				include_directories.push_back("data006");
			}
			if (stat(getMyPath() + "\\data104", &sb) == 0) {
				include_directories.push_back("data104");
			}
			if (stat(getMyPath() + "\\data105", &sb) == 0) {
				include_directories.push_back("data105");
			}
			if (stat(getMyPath() + "\\data106", &sb) == 0) {
				include_directories.push_back("data106");
			}
			if (stat(getMyPath() + "\\data107", &sb) == 0) {
				include_directories.push_back("data107");
			}
			if (stat(getMyPath() + "\\data108", &sb) == 0) {
				include_directories.push_back("data108");
			}
			LOGINFO("[RMM SUPPORT] Added required default paths");
		}
		else {
			struct stat sb;
			
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data000", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data000");
			}
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data001", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data001");
			}
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data003", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data003");
			}
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data004", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data004");
			}
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data005", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data005");
			}
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data006", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data006");
			}
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data104", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data104");
			}
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data105", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data105");
			}
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data106", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data106");
			}
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data107", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data107");
			}
			if (stat(getMyPath() + "\\" + include_dir.c_str() + "\\data108", &sb) == 0) {
				include_directories.push_back(include_dir + "\\" + "data108");
			}
			std::string debug_string = "[RMM SUPPORT] [INFO] Added required custom include directory (" + include_dir + ")";
			dbgPrint(debug_string.c_str());
		}

		

		
	}

	for (std::string dir : include_directories) {
		findFiles((getMyPath() + "\\" + dir.c_str()).c_str(), m_files, this, false, true);
		dbgPrint((getMyPath() + "\\" + dir.c_str()).c_str());
	}
	
}

void ModLoader::ModProfile::Save()
{
	if (!ModLoader::bInit)
		return;

	LOGINFO("Saving %s...", this->m_name.c_str());


	fprintf(profileFile, "\n"); // at least not stacking up on each other
	fflush(profileFile);
}