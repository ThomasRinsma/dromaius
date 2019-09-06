#include "dromaius.h"

void Input::initialize()
{
	wire = 0x10;
	row[0] = 0x0F;
	row[1] = 0x0F;
}

void Input::handleGameInput(int state, SDL_Keycode key)
{
	if (state == 1) {
		// key up
		if (key == emu->settings.keymap.start && emu->settings.keymap.startDown) {
			row[0] |= 0x08;
			emu->settings.keymap.startDown = 0;
		}
		else if (key == emu->settings.keymap.select && emu->settings.keymap.selectDown) {
			row[0] |= 0x04;
			emu->settings.keymap.selectDown = 0;
		}
		else if (key == emu->settings.keymap.b && emu->settings.keymap.bDown) {
			row[0] |= 0x02;
			emu->settings.keymap.bDown = 0;
		}
		else if (key == emu->settings.keymap.a && emu->settings.keymap.aDown) {
			row[0] |= 0x01;
			emu->settings.keymap.aDown = 0;
		}
		
		else if (key == emu->settings.keymap.down && emu->settings.keymap.downDown) {
			row[1] |= 0x08;
			emu->settings.keymap.downDown = 0;
		}
		else if (key == emu->settings.keymap.up && emu->settings.keymap.upDown) {
			row[1] |= 0x04;
			emu->settings.keymap.upDown = 0;
		}
		else if (key == emu->settings.keymap.left && emu->settings.keymap.leftDown) {
			row[1] |= 0x02;
			emu->settings.keymap.leftDown = 0;
		}
		else if (key == emu->settings.keymap.right && emu->settings.keymap.rightDown) {
			row[1] |= 0x01;
			emu->settings.keymap.rightDown = 0;
		}
	} else {
		// key down
		if (key == emu->settings.keymap.start && !emu->settings.keymap.startDown) {
			row[0] &= ~0x08 & 0x0F;
			emu->settings.keymap.startDown = 1;
		}
		else if (key == emu->settings.keymap.select && !emu->settings.keymap.selectDown) {
			row[0] &= ~0x04 & 0x0F;
			emu->settings.keymap.selectDown = 1;
		}
		else if (key == emu->settings.keymap.b && !emu->settings.keymap.bDown) {
			row[0] &= ~0x02 & 0x0F;
			emu->settings.keymap.bDown = 1;
		}
		else if (key == emu->settings.keymap.a && !emu->settings.keymap.aDown) {
			row[0] &= ~0x01 & 0x0F;
			emu->settings.keymap.aDown = 1;
		}
		
		else if (key == emu->settings.keymap.down && !emu->settings.keymap.downDown) {
			row[1] &= ~0x08 & 0x0F;
			emu->settings.keymap.downDown = 1;
		}
		else if (key == emu->settings.keymap.up && !emu->settings.keymap.upDown) {
			row[1] &= ~0x04 & 0x0F;
			emu->settings.keymap.upDown = 1;
		}
		else if (key == emu->settings.keymap.left && !emu->settings.keymap.leftDown) {
			row[1] &= ~0x02 & 0x0F;
			emu->settings.keymap.leftDown = 1;
		}
		else if (key == emu->settings.keymap.right && !emu->settings.keymap.rightDown) {
			row[1] &= ~0x01 & 0x0F;
			emu->settings.keymap.rightDown = 1;
		}
	}
}