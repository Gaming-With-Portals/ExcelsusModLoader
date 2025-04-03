#include "pch.h"
#include <assert.h>
#include "gui.h"
#include <Events.h>

#include "imgui/imgui.h"
#include "include/MinHook.h"
#include <string>

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

		// and here's your code
	}
} plugin;

void gui::RenderWindow()
{

}