#ifndef INCLUDED_AUDIO_H
#define INCLUDED_AUDIO_H

#include <cstdint>
struct Dromaius;

#define AUDIO_SAMPLE_HISTORY_SIZE 256

struct Audio
{
	Dromaius *emu;

	struct channel1_t {
		bool isEnabled;
		bool isRestarted;
		uint32_t ctr;

		// Duty (4 bits)
		uint8_t duty;

		// Sweep
		uint8_t sweepTime;
		uint8_t sweepDir;
		uint8_t sweepExp;
		double lastSweep;

		// Envelope
		uint8_t envVol;
		uint8_t envDir;
		uint8_t envSteps;

		// General
		uint8_t soundLen;
		uint16_t freq;
		bool isCont;
	} ch1;

	struct channel2_t {
		bool isEnabled;
		bool isRestarted;
		uint32_t ctr;

		// Duty (4 bits)
		uint8_t duty;

		// Envelope
		uint8_t envVol;
		uint8_t envDir;
		uint8_t envSteps;

		// General
		uint8_t soundLen;
		uint16_t freq;
		bool isCont;
	} ch2;

	struct channel3_t {
		bool isEnabled;
		bool isRestarted;
		uint32_t ctr;
		uint32_t waveCtr;

		// General
		uint8_t volume;
		uint8_t soundLen;
		uint16_t freq;
		bool isCont;
	} ch3;

	struct channel4_t {
		bool isEnabled;
		bool isRestarted;
		uint32_t ctr;

		// Freq spec
		uint8_t s;
		uint8_t r;
		// TODO: bit 3 (softness?)

		// Envelope
		uint8_t envVol;
		uint8_t envDir;
		uint8_t envSteps;

		// General
		uint8_t soundLen;
		bool isCont;
	} ch4;

	// TODO: channel / sound selection (FF25)
	// TODO: master channel volumes / Vin (FF24)

	SDL_AudioSpec want;
	SDL_AudioSpec have;
	SDL_AudioDeviceID dev;

	bool isEnabled;
	uint8_t waveRam[16]; // 32 nibbles
	uint32_t sample_ctr;

	uint8_t sampleHistory[4][AUDIO_SAMPLE_HISTORY_SIZE]; // for debugging

	bool initialized = false;

	~Audio();

	void initialize();

	void writeByte(uint8_t b, uint16_t addr);

	inline int8_t sinewave(uint32_t f, double t) const;
	inline int8_t squarewave(uint32_t f, double t, int8_t dutyline) const;

	// SDL callback
	void play_audio(uint8_t *stream, int len);
};


#endif