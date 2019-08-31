#ifndef INCLUDED_GUI_H
#define INCLUDED_GUI_H

#include <cstdint>
#include "gbemu.h"

struct GUI
{
	// window states
	bool showDebugCPU = true;
	bool showDebugGraphics = true;
	bool showDebugAudio = true;

	~GUI();
	void initialize();

	void renderHoverText(const char *fmt, ...);
	void renderInfoWindow();
	void renderSettingsWindow();
	void renderCPUDebugWindow();
	void renderAudioWindow();
	void renderGraphicsDebugWindow();
	void renderGBScreenWindow();
	void render();
};

#endif // include guard