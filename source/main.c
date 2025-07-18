#include <citro2d.h>
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#define BUTTON_X (SCREEN_WIDTH - 60)
#define BUTTON_Y 10
#define BUTTON_W 50
#define BUTTON_H 50

#define NUM_PADS 2

#define PAD_CHANNEL_1 0
#define PAD_CHANNEL_2 1

bool pointInButton(touchPosition touch) {
    return (touch.px >= BUTTON_X && touch.px <= BUTTON_X + BUTTON_W &&
            touch.py >= BUTTON_Y && touch.py <= BUTTON_Y + BUTTON_H);
}

typedef struct {
    u8* data;
    size_t size;
    int sampleRate;
    int channels;
    ndspWaveBuf waveBuf;
} AudioSample;

typedef struct {
    float x, y, w, h;
    u32 colorIdle;
    u32 colorPressed;
    bool pressed;
    AudioSample* sample;
} PadRect;

PadRect pads[NUM_PADS] = {}; 

void init_pad(PadRect* pad, float x, float y, float w, float h, u32 idle, u32 pressed, AudioSample* sample) {
    pad->x = x;
    pad->y = y;
    pad->w = w;
    pad->h = h;
    pad->colorIdle = idle;
    pad->colorPressed = pressed;
    pad->pressed = false;
    pad->sample = sample;
}

// Load a simple uncompressed 16-bit stereo WAV file
bool load_wav(const char* path, AudioSample* out) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 22, SEEK_SET); // num channels
    fread(&out->channels, sizeof(u16), 1, f);
    fread(&out->sampleRate, sizeof(u32), 1, f);
    fseek(f, 40, SEEK_SET); // size of data
    u32 dataSize;
    fread(&dataSize, sizeof(u32), 1, f);

    out->data = (u8*)linearAlloc(dataSize);
    if (!out->data) {
        fclose(f);
        return false;
    }

    fread(out->data, 1, dataSize, f);
    fclose(f);
    out->size = dataSize;
    return true;
}

void play_sample(AudioSample* sample, int channel) {
    ndspWaveBuf* buf = &sample->waveBuf;

    if (buf->status != NDSP_WBUF_DONE && buf->status != NDSP_WBUF_FREE)
        return;

    ndspChnSetInterp(channel, NDSP_INTERP_LINEAR);
    ndspChnSetRate(channel, sample->sampleRate);
    ndspChnSetFormat(channel, NDSP_FORMAT_STEREO_PCM16);

    float mix[12] = {0};
    mix[0] = mix[1] = 1.0f;
    ndspChnSetMix(channel, mix);

    memset(buf, 0, sizeof(ndspWaveBuf));
    buf->data_vaddr = sample->data;
    buf->nsamples = sample->size / 4;
    buf->looping = false;

    DSP_FlushDataCache(sample->data, sample->size);
    ndspChnWaveBufAdd(channel, buf);
}

int main(int argc, char** argv) {
// Init services
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    consoleInit(GFX_TOP, NULL);
    ndspInit();
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

// colors
    u32 clrButtonIdle = C2D_Color32(0x93, 0x93, 0x93, 0xFF);
    u32 clrButtonPressed = C2D_Color32(0x00, 0x95, 0xFF, 0xFF);
    u32 clrClear = C2D_Color32(0x00, 0x33, 0x00, 0x68);

    touchPosition touch;

// Load the sample
    AudioSample cKick;
    if (!load_wav("sdmc:/audio/claps64.wav", &cKick)) {
        printf("Failed to load kick.wav\n");
        gfxExit();
        ndspExit();
        return 1;
    }
    AudioSample ySnare1;
    if (!load_wav("sdmc:/audio/yoshsnare1.wav", &ySnare1)) {
        printf("Failed to load snare.wav\n");
        gfxExit();
        ndspExit();
        return 1;
    }

// draw pads
    init_pad(&pads[0], SCREEN_WIDTH /2 - 120, 90, 90, 90, clrButtonIdle, clrButtonPressed, &cKick);
    init_pad(&pads[1], SCREEN_WIDTH /2 + 30, 90, 90, 90, clrButtonIdle, clrButtonPressed, &ySnare1);

    printf("Use the pads to play sounds\n");

// Main loop
   while (aptMainLoop()) {
    hidScanInput();
    hidTouchRead(&touch);

    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();

    if (kDown & KEY_START) break;

// update pad pressed state
    for (int i = 0; i < NUM_PADS; i++) {
        PadRect* pad = &pads[i];
        bool isTouched = (kHeld & KEY_TOUCH) &&
                         touch.px >= pad->x && touch.px <= pad->x + pad->w &&
                         touch.py >= pad->y && touch.py <= pad->y + pad->h;

        if (isTouched && !pad->pressed) {
            pad->pressed = true;
            play_sample(pad->sample, 0); // hacky, should be i , fix laterrrrrrrrrrrr
            printf("Playing pad %d\n", i);
        } else if (!isTouched && pad->pressed) {
            pad->pressed = false;
        }
    }

// start rendering
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(bottom, clrClear);
    C2D_SceneBegin(bottom);

// render pads
    for (int i = 0; i < NUM_PADS; i++) {
        PadRect* pad = &pads[i];
        u32 color = pad->pressed ? pad->colorPressed : pad->colorIdle;
        C2D_DrawRectSolid(pad->x, pad->y, 0.0f, pad->w, pad->h, color);
    }

    C3D_FrameEnd(0);
}

    ndspChnWaveBufClear(0);
    ndspChnWaveBufClear(1);
    linearFree(cKick.data);
    linearFree(ySnare1.data);
    C2D_Fini();
    C3D_Fini();
    ndspExit();
    gfxExit();
    return 0;
}
