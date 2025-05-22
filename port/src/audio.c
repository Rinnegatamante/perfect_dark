#include <PR/ultratypes.h>
#include <stdio.h>
#include <SDL.h>
#include "platform.h"
#include "config.h"
#include "audio.h"
#include "system.h"

#define AUDIO_SAMPLERATE (22020)

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
#ifdef __vita__
	ALCint attrlist[6];
	attrlist[0] = ALC_FREQUENCY;
	attrlist[1] = 44100;
	attrlist[2] = ALC_SYNC;
	attrlist[3] = AL_FALSE;
	attrlist[4] = 0;
	ALDevice = alcOpenDevice(NULL);
	ALContext = alcCreateContext(ALDevice, attrlist);
	alcMakeContextCurrent(ALContext);
	
	ALfloat pos[] = { 0.0, 0.0, 0.0 };
	ALfloat vel[] = { 0.0, 0.0, 0.0 };
	ALfloat orient[]  = { 0.0, 0.0, 1.0, 0.0, -1.0, 0.0 };
	alListenerf(AL_GAIN, 1.0);
	alListenerfv(AL_POSITION, pos);
	alListenerfv(AL_VELOCITY, vel);
	alListenerfv(AL_ORIENTATION, orient);

	alGenSources(1, &ALSource);
#else
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		sysLogPrintf(LOG_ERROR, "SDL audio init error: %s", SDL_GetError());
		return -1;
	}

	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = AUDIO_SAMPLERATE; // TODO: this might cause trouble for some platforms
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
#endif
	return 0;
}

s32 audioGetBytesBuffered(void)
{
#ifdef __vita__
	return ALQueuedSize;
#else
	return SDL_GetQueuedAudioSize(dev);
#endif
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
#ifdef __vita__
		int processed;
		alGetSourcei(ALSource, AL_BUFFERS_PROCESSED, &processed);
		ALuint buffer;
		if (processed > 0) {
			alSourceUnqueueBuffers(ALSource, 1, &buffer);
			ALint bufferSize;
			alGetBufferi(buffer, AL_SIZE, &bufferSize);
			ALQueuedSize -= bufferSize;
			if (processed) {
				ALuint to_discard[128];
				alSourceUnqueueBuffers(ALSource, processed, to_discard);
				alDeleteBuffers(processed, to_discard);
			}
		} else {
			alGenBuffers(1, &buffer);
		}
		alBufferData(buffer, AL_FORMAT_STEREO16, nextBuf, nextSize, AUDIO_SAMPLERATE);
		ALQueuedSize += nextSize;
		alSourceQueueBuffers(ALSource, 1, &buffer);
		ALint state;
		alGetSourcei(ALSource, AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING) {
			alSourcePlay(ALSource);
		}
#else
		if (audioGetSamplesBuffered() < queueLimit) {
			SDL_QueueAudio(dev, nextBuf, nextSize);
		}
#endif
		nextBuf = NULL;
		nextSize = 0;
	}
}

PD_CONSTRUCTOR static void audioConfigInit(void)
{
	configRegisterInt("Audio.BufferSize", &bufferSize, 0, 1 * 1024 * 1024);
	configRegisterInt("Audio.QueueLimit", &queueLimit, 0, 1 * 1024 * 1024);
}
