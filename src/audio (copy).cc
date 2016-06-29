#include <stdio.h>
#include <stdlib.h>
#include "gbemu.h"

extern settings_t settings;
extern mem_t mem;
extern cpu_t cpu;
extern gpu_t gpu;
extern audio_t audio;

SDL_AudioSpec want, have;
SDL_AudioDeviceID dev;

uint32_t sample_ctr;

void initAudio() {
	memset(&want, 0, sizeof(want));
	want.freq = 48000;
	want.format = AUDIO_S8;
	want.channels = 1;
	want.samples = 128;
	want.callback = play_audio;

	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (!dev) {
		printf("Failed to open audio: %s\n", SDL_GetError());
	}

	// Unpause audio device
    SDL_PauseAudioDevice(dev, 0);

    // Reset sample counter
    sample_ctr = 0;
}

void freeAudio() {
	SDL_CloseAudioDevice(dev);
	SDL_CloseAudio();
}

void audioWriteByte(uint8_t b, uint16_t addr) {
	switch (addr) {
		// Channel 1
		case 0xFF10:
			audio.ch1.sweepExp = b & 0x7; // bit 0-2
			audio.ch1.sweepDir = (b & 0x4) ? 1 : 0; // bit 3
			audio.ch1.sweepTime = (b & 0x70) >> 4; // bit 4-6 
			break;

		case 0xFF11:
			audio.ch1.soundLen = b & 0x3F; // bit 0-5
			audio.ch1.duty = (b & 0xC0) >> 6; // bit 6-7
			break;

		case 0xFF12:
			audio.ch1.envSteps = b & 0x7; // bit 0-2
			audio.ch1.envDir = (b & 0x4) ? 1 : 0; // bit 3
			audio.ch1.envVol = (b & 0xF0) >> 4; // bit 4-7
			break;

		case 0xFF13:
			// lower 8 bits of 11
			audio.ch1.freq = (audio.ch1.freq & 0xFF00) | b;
			break;

		case 0xFF14:
			if (b & 0x80) { // bit 7
				audio.ch1.isRestarted = 1;
				audio.ch1.ctr = 0;
			}
			audio.ch1.isCont = (b & 0x40) == 0; // bit 6

			// upper 3 bits of 11
			audio.ch1.freq = (audio.ch1.freq & 0xFF) | ((b & 0x7) << 8);
			break;

		// Channel 2
		case 0xFF16:
			audio.ch2.soundLen = b & 0x3F; // bit 0-5
			audio.ch2.duty = (b & 0xC0) >> 6; // bit 6-7
			break;

		case 0xFF17:
			audio.ch2.envSteps = b & 0x7; // bit 0-2
			audio.ch2.envDir = (b & 0x4) ? 1 : 0; // bit 3
			audio.ch2.envVol = (b & 0xF0) >> 4; // bit 4-7
			break;

		case 0xFF18:
			// lower 8 bits of 11
			audio.ch2.freq = (audio.ch2.freq & 0xFF00) | b;
			break;

		case 0xFF19:
			if (b & 0x80) { // bit 7
				audio.ch2.isRestarted = 1;
				audio.ch2.ctr = 0;
			}
			audio.ch2.isCont = (b & 0x40) == 0; // bit 6

			// upper 3 bits of 11
			audio.ch2.freq = (audio.ch2.freq & 0xFF) | ((b & 0x7) << 8);
			break;

		// Channel 3
		case 0xFF1A:
			audio.ch3.isEnabled = (b & 0x80) ? 1 : 0; // bit 7
			break;

		case 0xFF1B:
			audio.ch3.soundLen = b;
			break;

		case 0xFF1C:
			audio.ch3.volume = (b & 0x60) >> 5;
			break;

		case 0xFF1D:
			// lower 8 bits of 11
			audio.ch3.freq = (audio.ch3.freq & 0x0700) | b;
			//printf("lower ch3: %x\n", );
			break;

		case 0xFF1E:
			if (b & 0x80) { // bit 7
				audio.ch3.isRestarted = 1;
				audio.ch3.ctr = 0;
				audio.ch3.waveCtr = 0;
			}
			audio.ch3.isCont = (b & 0x40) == 0; // bit 6

			// upper 3 bits of 11
			audio.ch3.freq = (audio.ch3.freq & 0xFF) | ((b & 0x7) << 8);
			//printf("lower ch3\n");

			break;

		// Channel 4
		case 0xFF20:
			audio.ch4.soundLen = b & 0x1F;
			break;

		case 0xFF21:
			audio.ch4.envSteps = b & 0x7; // bit 0-2
			audio.ch4.envDir = (b & 0x4) ? 1 : 0; // bit 3
			audio.ch4.envVol = (b & 0xF0) >> 4; // bit 4-7
			break;

		case 0xFF22:
			audio.ch4.r = b & 0x7; // bit 0-2
			audio.ch4.s = (b & 0xF0) >> 4; // bit 4-7
			break;

		case 0xFF23:
			if (b & 0x80) { // bit 7
				audio.ch4.isRestarted = 1;
				audio.ch4.ctr = 0;
			}

			audio.ch4.isCont = (b & 0x40) == 0; // bit 6

			break;


		// Control regs
		case 0xFF24:
			printf("TODO: write to master channel volumes / Vin\n");
			break;

		case 0xFF25:
			audio.ch1.isEnabled = (b & 0x01) | (b & 0x10);
			audio.ch2.isEnabled = (b & 0x02) | (b & 0x20);
			audio.ch3.isEnabled = (b & 0x04) | (b & 0x40);
			audio.ch4.isEnabled = (b & 0x08) | (b & 0x80);
			break;

		case 0xFF26:
			audio.isEnabled = (b & 0x80) ? 1 : 0;
			break;
	}
}


inline int8_t sinewave(uint32_t f, double t) {
	return sin((2.0 * M_PI) * f * t) * 128;
}

inline int8_t squarewave(uint32_t f, double t, int8_t dutyline) {
	return sinewave(f, t) > dutyline ? 127 : -127;
}

/*
	From the pandocs:

	00: 12.5% ( _-------_-------_------- )
	01: 25%   ( __------__------__------ )
	10: 50%   ( ____----____----____---- ) (normal)
	11: 75%   ( ______--______--______-- )
*/
uint8_t dutylines[4] = {-118, -90, 0, 90};

void play_audio(void *userdata, uint8_t *stream, int len) {
	int i;
	double t, f = 220;
	int8_t ch_sample[4] = {0, 0, 0, 0};
	int16_t tmp;

	for (i = 0; i < len; ++i) {
		if (audio.isEnabled) {
			if (audio.ch1.isEnabled) {
				t = sample_ctr * (1.0 / have.freq);

				if (audio.ch1.isRestarted) {
					audio.ch1.isRestarted = 0;
				} else {
					// Check if we've run out of time
					double ch1time = audio.ch1.ctr * (1.0 / have.freq);
					if (!audio.ch1.isCont &&
						ch1time > (64 - audio.ch1.soundLen) * (1.0 / 256)) {

						// Time is up, disable sound
						audio.ch1.isEnabled = 0;
					} else {
						// Sweep if enabled and enough time has passed
						if (audio.ch1.sweepTime != 0 &&
							t - audio.ch1.lastSweep > (audio.ch1.sweepTime / 128.0)) {
							// 
							// X(t) = X(t-1) +/- X(t-1)/2^n
							if (audio.ch1.sweepDir == 0) {
								audio.ch1.freq += audio.ch1.freq >> audio.ch1.sweepExp;
							} else {
								audio.ch1.freq -= audio.ch1.freq >> audio.ch1.sweepExp;
							}

							audio.ch1.lastSweep = t;
						}

						f = 131072.0 / (2048.0 - audio.ch1.freq);
						ch_sample[0] = squarewave(f, t, dutylines[audio.ch1.duty]);
						++audio.ch1.ctr;
					}
				}
			}

			if (audio.ch2.isEnabled) {
				// Check if we've run out of time
				double ch2time = audio.ch2.ctr * (1.0 / have.freq);
				if (!audio.ch2.isCont &&
					ch2time > (64 - audio.ch2.soundLen) * (1.0 / 256)) {

					// Time is up, disable sound
					audio.ch2.isEnabled = 0;
				} else {
					f = 131072.0 / (2048.0 - audio.ch2.freq);
					ch_sample[1] = squarewave(f, t, dutylines[audio.ch2.duty]);
					++audio.ch2.ctr;
				}
			}

			if (audio.ch3.isEnabled) {
				// Check if we've run out of time
				double ch3time = audio.ch3.ctr * (1.0 / have.freq);
				if (!audio.ch3.isCont &&
					ch3time > (64 - audio.ch3.soundLen) * (1.0 / 256)) {

					// Time is up, disable sound
					audio.ch3.isEnabled = 0;
				} else {

					double sps = have.freq / (65536.0 / (2048 - audio.ch3.freq));

					if (audio.ch3.ctr > sps) {
						++audio.ch3.waveCtr;
						audio.ch3.ctr = 0;
					}

					//printf("sps = %f\n", sps);

					ch_sample[2] = ((audio.ch3.waveCtr % 2 == 0)
						? (audio.waveRam[audio.ch3.waveCtr >> 1] & 0xF0) >> 8
						: (audio.waveRam[audio.ch3.waveCtr >> 1] & 0xF0));
					++audio.ch3.ctr;
				}
			}

			/*
			if (audio.ch4.isEnabled) {
				// Check if we've run out of time
				double ch4time = audio.ch4.ctr * (1.0 / have.freq);
				if (!audio.ch4.isCont &&
					ch4time > (64 - audio.ch4.soundLen) * (1.0 / 256)) {

					// Time is up, disable sound
					audio.ch4.isEnabled = 0;
				} else {
					if (audio.ch4.r == 0) {
						audio.ch4.r = 1;
					}
					double noisef = 524288.0 / (audio.ch4.r << (audio.ch4.s + 1));

					ch_sample[3] = rand() / 8;

					++audio.ch4.ctr;
				}
			}
			*/

		}


		// Add everything together
		tmp = ((double)ch_sample[0]
		    + (double)ch_sample[1]
		    + (audio.ch3.volume == 0 ? 0.0 :
		    	(double)(ch_sample[2] >> (audio.ch3.volume - 1)))
		    + (double)ch_sample[3]) * 32;
		//stream[i] = (tmp >= 127 ? 127 : (tmp <= -128 ? -128 : tmp));
		stream[i] = tmp;

		++sample_ctr;
	}
}