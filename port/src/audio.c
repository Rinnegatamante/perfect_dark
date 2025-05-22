#include <PR/ultratypes.h>
#include <stdio.h>
#include <SDL.h>
#include "platform.h"
#include "config.h"
#include "audio.h"
#include "system.h"

#ifdef __vita__
#define AUDIO_SAMPLERATE (22050)
#else
#define AUDIO_SAMPLERATE (22020)
#endif

#ifdef __vita__
#include <AL/al.h>
#include <AL/alc.h>
static ALCdevice *ALDevice;
static ALvoid *ALContext;
static ALuint ALSource;
static uint32_t ALQueuedSize = 0;
#endif

static SDL_AudioDeviceID dev;
static const s16 *nextBuf;
static u32 nextSize = 0;

static s32 bufferSize = 512;
static s32 queueLimit = 8192;

s32 audioInit(void)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		sysLogPrintf(LOG_ERROR, "SDL audio init error: %s", SDL_GetError());
		return -1;
	}

	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = AUDIO_SAMPLERATE;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = bufferSize;
	want.callback = NULL;

	nextBuf = NULL;

	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (dev == 0) {
		sysLogPrintf(LOG_ERROR, "SDL_OpenAudio error: %s", SDL_GetError());
		return -1;
	}

	SDL_PauseAudioDevice(dev, 0);
	return 0;
}

s32 audioGetBytesBuffered(void)
{
	return SDL_GetQueuedAudioSize(dev);
}

s32 audioGetSamplesBuffered(void)
{
	return audioGetBytesBuffered() / 4;
}

void audioSetNextBuffer(const s16 *buf, u32 len)
{
	nextBuf = buf;
	nextSize = len;
}

void audioEndFrame(void)
{
	if (nextBuf && nextSize) {
		if (audioGetSamplesBuffered() < queueLimit) {
			SDL_QueueAudio(dev, nextBuf, nextSize);
		}
		nextBuf = NULL;
		nextSize = 0;
	}
}

PD_CONSTRUCTOR static void audioConfigInit(void)
{
	configRegisterInt("Audio.BufferSize", &bufferSize, 0, 1 * 1024 * 1024);
	configRegisterInt("Audio.QueueLimit", &queueLimit, 0, 1 * 1024 * 1024);
}
