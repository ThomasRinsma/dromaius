#ifndef INCLUDED_GUI_H
#define INCLUDED_GUI_H

#include <cstdint>
struct Dromaius;

struct GUI
{
	// Up-reference
	Dromaius *emu;
	
	// window states
	bool showCPUDebugWindow = true;
	bool showGraphicsDebugWindow = true;
	bool showAudioWindow = true;
	bool showMemoryViewerWindow = true;
	bool showGameSpecificWindow = true;
	bool showImguiDemoWindow = false;

	// SDL/gl contexts
	SDL_Window *window;
	SDL_GLContext glcontext;
	const char* glsl_version;
	ImGui::FileBrowser openRomDialog;

	GUI();
	~GUI();
	void render();

private:
	void initializeImgui();
	void triggerRomLoadDialog();
	void renderHoverText(const char *fmt, ...);
	void renderInfoWindow();
	void renderSettingsWindow();
	void renderCPUDebugWindow();
	void renderAudioWindow();
	void renderGraphicsDebugWindow();
	void renderGBScreenWindow();
	void renderMemoryViewerWindow();
	void renderConsoleWindow();
	

	std::string getPokeStringAt(uint16_t addr, uint16_t length);
	void renderGameSpecificWindow();
};

#endif // include guard