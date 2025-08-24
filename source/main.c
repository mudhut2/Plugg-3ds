#include <citro2d.h>
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include "filebrowser.h"
#include "main.h"

#define BOT_SCREEN_WIDTH  320
#define BOT_SCREEN_HEIGHT 240
#define NUM_PADS 8

GameMode mode = MODE_PLAY;
int selectedPad = 0;

static inline u32 bytes_to_nsamples(const AudioSample* s) {
    return (u32)(s->size / (s->channels * sizeof(int16_t)));
}

typedef struct {
    float x, y, w, h;
    u32 colorIdle;
    u32 colorPressed;
    bool pressed;
    AudioSample* sample;
} PadRect;

PadRect pads[NUM_PADS] = {};

void init_pad(PadRect* pad, float x, float y, float w, float h, u32 idle, u32 pressed, AudioSample* sample) {
    pad->x = x; pad->y = y;
    pad->w = w; pad->h = h;
    pad->colorIdle = idle;
    pad->colorPressed = pressed;
    pad->pressed = false;
    pad->sample = sample;
}

// Load uncompressed 16-bit WAV
bool load_wav(const char* path, AudioSample* out) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("Error opening %s\n", path); return false; }

    char fourcc[5]; u32 chunk_size;
    fread(fourcc, 1, 4, f); fourcc[4] = '\0';
    if (strcmp(fourcc, "RIFF") != 0) { fclose(f); return false; }

    fread(&chunk_size, sizeof(u32), 1, f);
    fread(fourcc, 1, 4, f); fourcc[4] = '\0';
    if (strcmp(fourcc, "WAVE") != 0) { fclose(f); return false; }

    bool fmt_found = false, data_found = false;
    u32 dataSize = 0;

    while (!feof(f) && (!fmt_found || !data_found)) {
        fread(fourcc, 1, 4, f); fourcc[4] = '\0';
        fread(&chunk_size, sizeof(u32), 1, f);

        if (strcmp(fourcc, "fmt ") == 0) {
            u16 audioFormat, numChannels, bitsPerSample, blockAlign;
            u32 sampleRate, byteRate;
            fread(&audioFormat, sizeof(u16), 1, f);
            fread(&numChannels, sizeof(u16), 1, f);
            fread(&sampleRate, sizeof(u32), 1, f);
            fread(&byteRate, sizeof(u32), 1, f);
            fread(&blockAlign, sizeof(u16), 1, f);
            fread(&bitsPerSample, sizeof(u16), 1, f);

            if (audioFormat != 1 || bitsPerSample != 16) {
                printf("Only PCM16 WAV supported.\n");
                fclose(f); return false;
            }
            out->channels = numChannels;
            out->sampleRate = sampleRate;
            fmt_found = true;
        } else if (strcmp(fourcc, "data") == 0) {
            dataSize = chunk_size;
            data_found = true;
            break;
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
    }

    if (!fmt_found || !data_found) { fclose(f); return false; }

    out->data = (u8*)linearAlloc(dataSize);
    if (!out->data) { fclose(f); return false; }

    fread(out->data, 1, dataSize, f);
    fclose(f);
    out->size = dataSize;
    out->which = 0;
    out->channel = -1;
    return true;
}

void play_sample(AudioSample* s) {
    int hwChannel = s->channel;

    // If this pad was already playing, clear its buffer to stop it
    if (hwChannel >= 0 && hwChannel < 8) {
        ndspChnWaveBufClear(hwChannel);
    } else {
        // Find a free channel for new sound
        hwChannel = -1;
        for (int i = 0; i < 8; i++) {
            if (!ndspChnIsPlaying(i)) {
                hwChannel = i;
                break;
            }
        }
        if (hwChannel < 0) hwChannel = 0; // fallback
        s->channel = hwChannel;
    }

    // Reset channel and configure format
    ndspChnReset(hwChannel);
    ndspChnSetInterp(hwChannel, NDSP_INTERP_LINEAR);
    ndspChnSetRate(hwChannel, s->sampleRate);
    ndspChnSetFormat(hwChannel, s->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);

    // Full left/right mix
    float mix[12] = {0};
    mix[0] = mix[1] = 1.0f;
    ndspChnSetMix(hwChannel, mix);

    // Toggle wave buffer (double buffering)
    s->which ^= 1;
    ndspWaveBuf* buf = &s->waveBuf[s->which];
    memset(buf, 0, sizeof(*buf));

    buf->data_vaddr = s->data;
    buf->nsamples = bytes_to_nsamples(s);
    buf->looping = false;

    DSP_FlushDataCache(s->data, s->size);
    ndspChnWaveBufAdd(hwChannel, buf);
}

void assignSoundToPad(int padIndex, const char* filepath) {
    AudioSample* s = pads[padIndex].sample;
    if (s->data) linearFree(s->data);
    if (!load_wav(filepath, s)) {
        printf("Failed loading %s\n", filepath);
    } else {
        printf("Assigned %s to pad %d\n", filepath, padIndex);
    }
}

int main(int argc, char** argv) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    consoleInit(GFX_TOP, NULL);
    ndspInit();

    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);


    u32 clrButtonIdle = C2D_Color32(0x93, 0x93, 0x93, 0xFF);
    u32 clrButtonPressed = C2D_Color32(0xF0, 0x7D, 0x00, 0xFF);
    u32 clrClear = C2D_Color32(0x4B, 0x36, 0x9D, 0x68);
    touchPosition touch;

    // init default sounds so pad dont start empty
    AudioSample sounds[NUM_PADS];
    const char* paths[NUM_PADS] = {
        "sdmc:/sounds/plug/spinz.wav",
        "sdmc:/sounds/00.wav",
        "sdmc:/sounds/oh1.wav",
        "sdmc:/sounds/hh1.wav",
        "sdmc:/sounds/yoshsnare1.wav",
        "sdmc:/sounds/clap1.wav",
        "sdmc:/sounds/perc1.wav",
        "sdmc:/sounds/plug/clap1.wav"
    };
    for (int i = 0; i < NUM_PADS; i++) {
        load_wav(paths[i], &sounds[i]);
    }

    // pad grid
    float pad_size = 70.0f, padding = 10.0f;
    float start_x = (BOT_SCREEN_WIDTH - (pad_size * 4 + padding * 3)) / 2;
    float start_y = (BOT_SCREEN_HEIGHT - (pad_size * 2 + padding)) / 2;
    for (int i = 0; i < NUM_PADS; i++) {
        int row = i / 4, col = i % 4;
        init_pad(&pads[i], start_x + col*(pad_size+padding), start_y + row*(pad_size+padding),
                 pad_size, pad_size, clrButtonIdle, clrButtonPressed, &sounds[i]);
    }

    u32 mapped_keys[NUM_PADS] = { KEY_A, KEY_B, KEY_X, KEY_Y, KEY_L, KEY_R, KEY_DUP, KEY_DDOWN };

    while (aptMainLoop()) {
        hidScanInput();
        hidTouchRead(&touch);
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        if (mode == MODE_PLAY && (kDown & KEY_SELECT)) {
            mode = MODE_MENU_MAIN;
            printf("\x1b[0;0HPad %d selected.\n Press A to change sound, B to return.\n", selectedPad);
        }

        if (mode == MODE_MENU_MAIN) {
            if (kDown & KEY_DLEFT)  selectedPad = (selectedPad - 1 + NUM_PADS) % NUM_PADS;
            if (kDown & KEY_DRIGHT) selectedPad = (selectedPad + 1) % NUM_PADS;
            if (kDown & KEY_A) {
                mode = MODE_MENU_SOUND;
            }
            if (kDown & KEY_B) {
                mode = MODE_PLAY;
                consoleClear();
            } 
        }

        // File browser for sound selection
        if (mode == MODE_MENU_SOUND) {
            char* chosen = openFileBrowser("sdmc:/sounds");
            
            if(chosen){
                assignSoundToPad(selectedPad, chosen);
                consoleClear();
            }
            mode = MODE_PLAY;
        }

        // Play pads
        if (mode == MODE_PLAY) {
            printf("\x1b[0;0HSELECT to change modes   "); // spaces to clear remnants
        
            for (int i = 0; i < NUM_PADS; i++) {
                PadRect* pad = &pads[i];
                bool isTouched = (kHeld & KEY_TOUCH) &&
                                 touch.px >= pad->x && touch.px <= pad->x+pad->w &&
                                 touch.py >= pad->y && touch.py <= pad->y+pad->h;
                if (isTouched && !pad->pressed) {
                    pad->pressed = true;
                    play_sample(pad->sample);
                }
                else if (kDown & mapped_keys[i]) {
                    pad->pressed = true;
                    play_sample(pad->sample);
                }
                else if(hidKeysHeld() & mapped_keys[i]) {
                    pad->pressed = true;
                }
                else if(hidKeysUp() & mapped_keys[i]) {
                    pad->pressed = false;
                }
                else if (!isTouched && pad->pressed) {
                    pad->pressed = false;
                }
            } 
        }

        // Render pads
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(bottom, clrClear);
        C2D_SceneBegin(bottom);

        for (int i = 0; i < NUM_PADS; i++) {
            PadRect* pad = &pads[i];
            u32 color = pad->pressed ? pad->colorPressed : pad->colorIdle;
            if (mode == MODE_MENU_MAIN && i == selectedPad)
                color = C2D_Color32(255, 255, 0, 255);
            C2D_DrawRectSolid(pad->x, pad->y, 0.0f, pad->w, pad->h, color);
        }
        C3D_FrameEnd(0);
    }

    // Cleanup
    for (int i = 0; i < NUM_PADS; i++) {
        if (sounds[i].data) linearFree(sounds[i].data);
    }

    C2D_Fini(); C3D_Fini(); ndspExit(); gfxExit();
    return 0;
}
