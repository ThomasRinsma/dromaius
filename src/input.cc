#include "input.h"
#include "gbemu.h"

extern settings_t settings;

void Input::handleGameInput(int state, SDL_Keycode key)
{
	if (state == 1) {
		// key up
		if (key == settings.keymap.start && settings.keymap.startDown) {
			inputRow[0] |= 0x08;
			settings.keymap.startDown = 0;
		}
		else if (key == settings.keymap.select && settings.keymap.selectDown) {
			inputRow[0] |= 0x04;
			settings.keymap.selectDown = 0;
		}
		else if (key == settings.keymap.b && settings.keymap.bDown) {
			inputRow[0] |= 0x02;
			settings.keymap.bDown = 0;
		}
		else if (key == settings.keymap.a && settings.keymap.aDown) {
			inputRow[0] |= 0x01;
			settings.keymap.aDown = 0;
		}
		
		else if (key == settings.keymap.down && settings.keymap.downDown) {
			inputRow[1] |= 0x08;
			settings.keymap.downDown = 0;
		}
		else if (key == settings.keymap.up && settings.keymap.upDown) {
			inputRow[1] |= 0x04;
			settings.keymap.upDown = 0;
		}
		else if (key == settings.keymap.left && settings.keymap.leftDown) {
			inputRow[1] |= 0x02;
			settings.keymap.leftDown = 0;
		}
		else if (key == settings.keymap.right && settings.keymap.rightDown) {
			inputRow[1] |= 0x01;
			settings.keymap.rightDown = 0;
		}
	} else {
		// key down
		if (key == settings.keymap.start && !settings.keymap.startDown) {
			inputRow[0] &= ~0x08 & 0x0F;
			settings.keymap.startDown = 1;
		}
		else if (key == settings.keymap.select && !settings.keymap.selectDown) {
			inputRow[0] &= ~0x04 & 0x0F;
			settings.keymap.selectDown = 1;
		}
		else if (key == settings.keymap.b && !settings.keymap.bDown) {
			inputRow[0] &= ~0x02 & 0x0F;
			settings.keymap.bDown = 1;
		}
		else if (key == settings.keymap.a && !settings.keymap.aDown) {
			inputRow[0] &= ~0x01 & 0x0F;
			settings.keymap.aDown = 1;
		}
		
		else if (key == settings.keymap.down && !settings.keymap.downDown) {
			inputRow[1] &= ~0x08 & 0x0F;
			settings.keymap.downDown = 1;
		}
		else if (key == settings.keymap.up && !settings.keymap.upDown) {
			inputRow[1] &= ~0x04 & 0x0F;
			settings.keymap.upDown = 1;
		}
		else if (key == settings.keymap.left && !settings.keymap.leftDown) {
			inputRow[1] &= ~0x02 & 0x0F;
			settings.keymap.leftDown = 1;
		}
		else if (key == settings.keymap.right && !settings.keymap.rightDown) {
			inputRow[1] &= ~0x01 & 0x0F;
			settings.keymap.rightDown = 1;
		}
	}
}