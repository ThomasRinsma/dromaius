#include <cstring>
#include <iostream>
#include <cmath>
#include "dromaius.h"

Audio::~Audio()
{
	SDL_CloseAudioDevice(dev);
	SDL_CloseAudio();
}

void Audio::initialize()
{
	if (not initialized) {
		memset(&want, 0, sizeof(want));

		want.freq = 48000;
		want.format = AUDIO_S8;
		want.channels = 1;
		want.samples = 128;
		want.userdata = this; // store reference to this for the callback
		want.callback = reinterpret_cast<void (*)(void*, unsigned char*, int)>(&Audio::play_audio);

		dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
		if (not dev) {
			std::cout << "Failed to open audio: " << SDL_GetError() << std::endl;
		}

		// Unpause audio device
		SDL_PauseAudioDevice(dev, 0);

		// Reset sample counter
		sample_ctr = 0;

		initialized = true;
	}
}

void Audio::writeByte(uint8_t b, uint16_t addr)
{
	switch (addr) {
		// Channel 1
		case 0xFF10:
			ch1.sweepExp = b & 0x7; // bit 0-2
			ch1.sweepDir = (b & 0x4) ? 1 : 0; // bit 3
			ch1.sweepTime = (b & 0x70) >> 4; // bit 4-6 
			break;

		case 0xFF11:
			ch1.soundLen = b & 0x3F; // bit 0-5
			ch1.duty = (b & 0xC0) >> 6; // bit 6-7
			break;

		case 0xFF12:
			ch1.envSteps = b & 0x7; // bit 0-2
			ch1.envDir = (b & 0x4) ? 1 : 0; // bit 3
			ch1.envVol = (b & 0xF0) >> 4; // bit 4-7
			break;

		case 0xFF13:
			// lower 8 bits of 11
			ch1.freq = (ch1.freq & 0xFF00) | b;
			break;

		case 0xFF14:
			if (b & 0x80) { // bit 7
				ch1.isRestarted = 1;
				ch1.ctr = 0;
			}
			ch1.isCont = (b & 0x40) == 0; // bit 6

			// upper 3 bits of 11
			ch1.freq = (ch1.freq & 0xFF) | ((b & 0x7) << 8);
			break;

		// Channel 2
		case 0xFF16:
			ch2.soundLen = b & 0x3F; // bit 0-5
			ch2.duty = (b & 0xC0) >> 6; // bit 6-7
			break;

		case 0xFF17:
			ch2.envSteps = b & 0x7; // bit 0-2
			ch2.envDir = (b & 0x4) ? 1 : 0; // bit 3
			ch2.envVol = (b & 0xF0) >> 4; // bit 4-7
			break;

		case 0xFF18:
			// lower 8 bits of 11
			ch2.freq = (ch2.freq & 0xFF00) | b;
			break;

		case 0xFF19:
			if (b & 0x80) { // bit 7
				ch2.isRestarted = 1;
				ch2.ctr = 0;
			}
			ch2.isCont = (b & 0x40) == 0; // bit 6

			// upper 3 bits of 11
			ch2.freq = (ch2.freq & 0xFF) | ((b & 0x7) << 8);
			break;

		// Channel 3
		case 0xFF1A:
			ch3.isEnabled = (b & 0x80) ? 1 : 0; // bit 7
			break;

		case 0xFF1B:
			ch3.soundLen = b;
			break;

		case 0xFF1C:
			ch3.volume = (b & 0x60) >> 5;
			break;

		case 0xFF1D:
			// lower 8 bits of 11
			ch3.freq = (ch3.freq & 0x0700) | b;
			//printf("lower ch3: %x\n", );
			break;

		case 0xFF1E:
			if (b & 0x80) { // bit 7
				ch3.isRestarted = 1;
				ch3.ctr = 0;
				ch3.waveCtr = 0;
			}
			ch3.isCont = (b & 0x40) == 0; // bit 6

			// upper 3 bits of 11
			ch3.freq = (ch3.freq & 0xFF) | ((b & 0x7) << 8);
			//printf("lower ch3\n");

			break;

		// Channel 4
		case 0xFF20:
			ch4.soundLen = b & 0x1F;
			break;

		case 0xFF21:
			ch4.envSteps = b & 0x7; // bit 0-2
			ch4.envDir = (b & 0x4) ? 1 : 0; // bit 3
			ch4.envVol = (b & 0xF0) >> 4; // bit 4-7
			break;

		case 0xFF22:
			ch4.r = b & 0x7; // bit 0-2
			ch4.s = (b & 0xF0) >> 4; // bit 4-7
			break;

		case 0xFF23:
			if (b & 0x80) { // bit 7
				ch4.isRestarted = 1;
				ch4.ctr = 0;
			}

			ch4.isCont = (b & 0x40) == 0; // bit 6

			break;


		// Control regs
		case 0xFF24:
			//printf("TODO: write to master channel volumes / Vin\n");
			break;

		case 0xFF25:
			ch1.isEnabled = (b & 0x01) | (b & 0x10);
			ch2.isEnabled = (b & 0x02) | (b & 0x20);
			ch3.isEnabled = (b & 0x04) | (b & 0x40);
			ch4.isEnabled = (b & 0x08) | (b & 0x80);
			break;

		case 0xFF26:
			isEnabled = (b & 0x80) ? 1 : 0;
			break;
	}
}

inline int8_t Audio::sinewave(uint32_t f, double t) const
{
	return sin((2.0 * M_PI) * f * t) * 128;
}

inline int8_t Audio::squarewave(uint32_t f, double t, int8_t dutyline) const
{
	return sinewave(f, t) > dutyline ? 127 : -127;
}

int8_t dutylines[4] = {-118, -90, 0, 90};

void Audio::play_audio(uint8_t *stream, int len)
{
	double t, f = 220;
	int8_t ch_sample[4] = {0, 0, 0, 0};
	int16_t tmp;

	for (int i = 0; i < len; ++i) {
		if (isEnabled) {
			if (ch1.isEnabled) {
				t = sample_ctr * (1.0 / have.freq);

				if (ch1.isRestarted) {
					ch1.isRestarted = 0;
				} else {
					// Check if we've run out of time
					double ch1time = ch1.ctr * (1.0 / have.freq);
					if (!ch1.isCont &&
						ch1time > (64 - ch1.soundLen) * (1.0 / 256)) {

						// Time is up, disable sound
						ch1.isEnabled = 0;
					} else {
						// Sweep if enabled and enough time has passed
						if (ch1.sweepTime != 0 &&
							t - ch1.lastSweep > (ch1.sweepTime / 128.0)) {
							// 
							// X(t) = X(t-1) +/- X(t-1)/2^n
							if (ch1.sweepDir == 0) {
								ch1.freq += ch1.freq >> ch1.sweepExp;
							} else {
								ch1.freq -= ch1.freq >> ch1.sweepExp;
							}

							ch1.lastSweep = t;
						}

						f = 131072.0 / (2048.0 - ch1.freq);
						ch_sample[0] = squarewave(f, t, dutylines[ch1.duty]);
						++ch1.ctr;
					}
				}
			}

			if (ch2.isEnabled) {
				// Check if we've run out of time
				double ch2time = ch2.ctr * (1.0 / have.freq);
				if (!ch2.isCont &&
					ch2time > (64 - ch2.soundLen) * (1.0 / 256)) {

					// Time is up, disable sound
					ch2.isEnabled = 0;
				} else {
					f = 131072.0 / (2048.0 - ch2.freq);
					ch_sample[1] = squarewave(f, t, dutylines[ch2.duty]);
					++ch2.ctr;
				}
			}

			if (ch3.isEnabled) {
				// Check if we've run out of time
				double ch3time = ch3.ctr * (1.0 / have.freq);
				if (!ch3.isCont &&
					ch3time > (64 - ch3.soundLen) * (1.0 / 256)) {

					// Time is up, disable sound
					ch3.isEnabled = 0;
				} else {

					double sps = have.freq / (65536.0 / (2048 - ch3.freq));

					if (ch3.ctr > sps) {
						++ch3.waveCtr;
						ch3.ctr = 0;
					}

					//printf("sps = %f\n", sps);

					ch_sample[2] = ((ch3.waveCtr % 2 == 0)
						? (waveRam[ch3.waveCtr >> 1] & 0xF0) >> 8
						: (waveRam[ch3.waveCtr >> 1] & 0xF0));
					++ch3.ctr;
				}
			}

			/*
			if (ch4.isEnabled) {
				// Check if we've run out of time
				double ch4time = ch4.ctr * (1.0 / have.freq);
				if (!ch4.isCont &&
					ch4time > (64 - ch4.soundLen) * (1.0 / 256)) {

					// Time is up, disable sound
					ch4.isEnabled = 0;
				} else {
					if (ch4.r == 0) {
						ch4.r = 1;
					}
					double noisef = 524288.0 / (ch4.r << (ch4.s + 1));

					ch_sample[3] = rand() / 8;

					++ch4.ctr;
				}
			}
			*/

		}


		// Add everything together
		tmp = ((double)ch_sample[0]
			+ (double)ch_sample[1]
			+ (ch3.volume == 0 ? 0.0 :
				(double)(ch_sample[2] >> (ch3.volume - 1)))
			+ (double)ch_sample[3]) * 32;
		//stream[i] = (tmp >= 127 ? 127 : (tmp <= -128 ? -128 : tmp));
		stream[i] = tmp;

		// store for debugging
		sampleHistory[0][emu->cpu.c % AUDIO_SAMPLE_HISTORY_SIZE] = ch_sample[0];
		sampleHistory[1][emu->cpu.c % AUDIO_SAMPLE_HISTORY_SIZE] = ch_sample[1];
		sampleHistory[2][emu->cpu.c % AUDIO_SAMPLE_HISTORY_SIZE] = ch_sample[2];
		sampleHistory[3][emu->cpu.c % AUDIO_SAMPLE_HISTORY_SIZE] = ch_sample[3];

		++sample_ctr;
	}
}
