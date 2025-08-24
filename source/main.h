#ifndef MAIN_H
#define MAIN_H

#include <3ds.h>
#include <citro2d.h>
#include <limits.h>

// Constants
#define BOT_SCREEN_WIDTH 320
#define BOT_SCREEN_HEIGHT 240
#define NUM_PADS 8
#define MAX_FILES 512

// Enums
typedef enum { MODE_PLAY, MODE_MENU_MAIN, MODE_MENU_SOUND } GameMode;

// Data Structures
typedef struct {
    u8* data;
    u32 size;
    u8 channels;
    u32 sampleRate;
    ndspWaveBuf waveBuf[2];
    u8 which;
    int channel;
} AudioSample;

typedef struct {
    float x, y, w, h;
    u32 colorIdle;
    u32 colorPressed;
    bool pressed;
    AudioSample* sample;
} PadRect;

typedef struct {
    char name[256];
    int isDir;
} Entry;

// Global Variables
extern GameMode mode;
extern int selectedPad;
extern PadRect pads[NUM_PADS];

// Function Declarations
void init_pad(PadRect* pad, float x, float y, float w, float h, u32 idle, u32 pressed, AudioSample* sample);
bool load_wav(const char* path, AudioSample* out);
void play_sample(AudioSample* s);
void assignSoundToPad(int padIndex, const char* filepath);
void loadDirectory(const char* path);

#endif // MAIN_H