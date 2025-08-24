#pragma once
#include <3ds.h>
#include <stdbool.h>
#include <citro2d.h>

typedef struct {
    u8* data;
    size_t size;
    int sampleRate;
    int channels;
    ndspWaveBuf waveBuf[2];
    int which;
    int channel;
} AudioSample;

typedef enum {
    MODE_PLAY,
    MODE_MENU_MAIN,
    MODE_MENU_SOUND
} GameMode;

extern GameMode mode;
extern int selectedPad;

extern C3D_RenderTarget* bottom;

bool load_wav(const char* path, AudioSample* out);
void play_sample(AudioSample* s);
void assignSoundToPad(int padIndex, const char* path);
