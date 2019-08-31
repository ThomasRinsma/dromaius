#include "gbemu.h"

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
		if (key == settings.keymap.start && settings.keymap.startDown) {
			row[0] |= 0x08;
			settings.keymap.startDown = 0;
		}
		else if (key == settings.keymap.select && settings.keymap.selectDown) {
			row[0] |= 0x04;
			settings.keymap.selectDown = 0;
		}
		else if (key == settings.keymap.b && settings.keymap.bDown) {
			row[0] |= 0x02;
			settings.keymap.bDown = 0;
		}
		else if (key == settings.keymap.a && settings.keymap.aDown) {
			row[0] |= 0x01;
			settings.keymap.aDown = 0;
		}
		
		else if (key == settings.keymap.down && settings.keymap.downDown) {
			row[1] |= 0x08;
			settings.keymap.downDown = 0;
		}
		else if (key == settings.keymap.up && settings.keymap.upDown) {
			row[1] |= 0x04;
			settings.keymap.upDown = 0;
		}
		else if (key == settings.keymap.left && settings.keymap.leftDown) {
			row[1] |= 0x02;
			settings.keymap.leftDown = 0;
		}
		else if (key == settings.keymap.right && settings.keymap.rightDown) {
			row[1] |= 0x01;
			settings.keymap.rightDown = 0;
		}
	} else {
		// key down
		if (key == settings.keymap.start && !settings.keymap.startDown) {
			row[0] &= ~0x08 & 0x0F;
			settings.keymap.startDown = 1;
		}
		else if (key == settings.keymap.select && !settings.keymap.selectDown) {
			row[0] &= ~0x04 & 0x0F;
			settings.keymap.selectDown = 1;
		}
		else if (key == settings.keymap.b && !settings.keymap.bDown) {
			row[0] &= ~0x02 & 0x0F;
			settings.keymap.bDown = 1;
		}
		else if (key == settings.keymap.a && !settings.keymap.aDown) {
			row[0] &= ~0x01 & 0x0F;
			settings.keymap.aDown = 1;
		}
		
		else if (key == settings.keymap.down && !settings.keymap.downDown) {
			row[1] &= ~0x08 & 0x0F;
			settings.keymap.downDown = 1;
		}
		else if (key == settings.keymap.up && !settings.keymap.upDown) {
			row[1] &= ~0x04 & 0x0F;
			settings.keymap.upDown = 1;
		}
		else if (key == settings.keymap.left && !settings.keymap.leftDown) {
			row[1] &= ~0x02 & 0x0F;
			settings.keymap.leftDown = 1;
		}
		else if (key == settings.keymap.right && !settings.keymap.rightDown) {
			row[1] &= ~0x01 & 0x0F;
			settings.keymap.rightDown = 1;
		}
	}
}