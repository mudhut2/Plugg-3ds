#include <citro2d.h>
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#define NUM_PADS 2

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
// Load a simple uncompressed 16-bit stereo WAV file
bool load_wav(const char* path, AudioSample* out) {
    printf("Loading WAV from path: %s\n", path);
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Error: Failed to open file %s\n", path);
        return false;
    }

    char fourcc[5];
    u32 chunk_size;

    // RIFF chunk
    fread(fourcc, 1, 4, f);
    fourcc[4] = '\0';
    if (strcmp(fourcc, "RIFF") != 0) {
        printf("Error: Not a valid RIFF file.\n");
        fclose(f);
        return false;
    }
    fread(&chunk_size, sizeof(u32), 1, f);
    fread(fourcc, 1, 4, f);
    fourcc[4] = '\0';
    if (strcmp(fourcc, "WAVE") != 0) {
        printf("Error: Not a valid WAVE file.\n");
        fclose(f);
        return false;
    }

    // Iterate through chunks until 'fmt ' and 'data' are found
    bool fmt_found = false;
    bool data_found = false;
    u32 dataSize = 0;

    while (!feof(f) && (!fmt_found || !data_found)) {
        fread(fourcc, 1, 4, f);
        fourcc[4] = '\0';
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

            // Basic validation
            if (audioFormat != 1 || bitsPerSample != 16) {
                printf("Error: Only uncompressed 16-bit PCM is supported.\n");
                fclose(f);
                return false;
            }

            out->channels = numChannels;
            out->sampleRate = sampleRate;
            fmt_found = true;
        } else if (strcmp(fourcc, "data") == 0) {
            dataSize = chunk_size;
            data_found = true;
            break; // We found the data chunk, break the loop
        } else {
            fseek(f, chunk_size, SEEK_CUR); // Skip unknown chunks
        }
    }

    if (!fmt_found || !data_found) {
        printf("Error: Missing 'fmt ' or 'data' chunk.\n");
        fclose(f);
        return false;
    }

    out->data = (u8*)linearAlloc(dataSize);
    if (!out->data) {
        printf("Error: Failed to allocate linear memory.\n");
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
    printf("Initializing gfx...\n");
    gfxInitDefault();

    printf("Initializing C3D...\n");
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    printf("Initializing C2D...\n");
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);

    printf("Preparing C2D...\n");
    C2D_Prepare();

    printf("Initializing console...\n");
    consoleInit(GFX_TOP, NULL);

    printf("Initializing ndsp...\n");
    ndspInit();

    printf("Creating screen target...\n");
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    printf("All services initialized!\n");

    u32 clrButtonIdle = C2D_Color32(0x93, 0x93, 0x93, 0xFF);
    u32 clrButtonPressed = C2D_Color32(0x00, 0x95, 0xFF, 0xFF);
    u32 clrClear = C2D_Color32(0x4B, 0x36, 0x9D, 0x68);

    touchPosition touch;

// Load samples
    AudioSample kick1;
    if (!load_wav("sdmc:/sounds/oh1.wav", &kick1)) {
        printf("Failed to load kick.wav\n");
        gfxExit();
        ndspExit();
        return 1;
    }
    AudioSample snare1;
    if (!load_wav("sdmc:/sounds/perc1.wav", &snare1)) {
        printf("Failed to load snare.wav\n");
        gfxExit();
        ndspExit();
        return 1;
    }

// draw pads
    init_pad(&pads[0], SCREEN_WIDTH /2 - 120, 90, 90, 90, clrButtonIdle, clrButtonPressed, &kick1);
    init_pad(&pads[1], SCREEN_WIDTH /2 + 30, 90, 90, 90, clrButtonIdle, clrButtonPressed, &snare1);

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
            if (i == 0) { // Kick Pad
                play_sample(pad->sample, 0); // Play kick on Channel 0
                printf("Playing kick pad on channel 0\n");
            } else if (i == 1) { // Snare Pad
                play_sample(pad->sample, 1); // Play snare on Channel 1
                printf("Playing snare pad on channel 1\n");
            } 
        }
         else if (!isTouched && pad->pressed) {
            pad->pressed = false;
        }
    }

    if (kDown & KEY_A) {
        play_sample(&kick1, 0); // Play kick with A on Channel 0
        printf("Played kick with A button on channel 0\n");
    }
    if (kDown & KEY_B) {
        play_sample(&snare1, 1); // Play snare with B on Channel 1
        printf("Played snare with B button on channel 1\n");
    }
    /* if (kDown & KEY_Y) {
        play_sample(&hh1, 1); 
        printf("Played hh with Y button on channel 2\n");
    } */
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
    //ndspChnWaveBufClear(2);
    linearFree(kick1.data);
    linearFree(snare1.data);
    //linearFree(hh1.data);
    C2D_Fini();
    C3D_Fini();
    ndspExit();
    gfxExit();
    return 0;
}