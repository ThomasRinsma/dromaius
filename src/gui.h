#ifndef INCLUDED_GUI_H
#define INCLUDED_GUI_H

#include <cstdint>
#include "dromaius.h"

struct GUI
{
	// window states
	bool showCPUDebugWindow = true;
	bool showGraphicsDebugWindow = true;
	bool showAudioWindow = true;
	bool showMemoryViewerWindow = true;

	~GUI();
	void initialize();

	void renderHoverText(const char *fmt, ...);
	void renderInfoWindow();
	void renderSettingsWindow();
	void renderCPUDebugWindow();
	void renderAudioWindow();
	void renderGraphicsDebugWindow();
	void renderGBScreenWindow();
	void renderMemoryViewerWindow();
	void render();
};

#endif // include guard