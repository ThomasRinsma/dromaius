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

	// SDL/gl contexts
	SDL_Window *window;
	SDL_GLContext glcontext;
	const char* glsl_version;

	GUI();
	~GUI();
	void render();

private:
	void initializeImgui();
	void renderHoverText(const char *fmt, ...);
	void renderInfoWindow();
	void renderSettingsWindow();
	void renderCPUDebugWindow();
	void renderAudioWindow();
	void renderGraphicsDebugWindow();
	void renderGBScreenWindow();
	void renderMemoryViewerWindow();
};

#endif // include guard