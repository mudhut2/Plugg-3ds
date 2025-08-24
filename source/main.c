#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include "main.h"

// Define global variables
GameMode mode = MODE_PLAY;
int selectedPad = 0;
PadRect pads[NUM_PADS] = {};

static Entry entries[MAX_FILES];
static int entryCount = 0;
static char currentPath[PATH_MAX];
static int fileBrowserSelected = 0;
static int fileBrowserScroll = 0;

C3D_RenderTarget* bottom;

static inline u32 bytes_to_nsamples(const AudioSample* s) {
    return (u32)(s->size / (s->channels * sizeof(int16_t)));
}

void init_pad(PadRect* pad, float x, float y, float w, float h, u32 idle, u32 pressed, AudioSample* sample) {
    pad->x = x; pad->y = y;
    pad->w = w; pad->h = h;
    pad->colorIdle = idle;
    pad->colorPressed = pressed;
    pad->pressed = false;
    pad->sample = sample;
}

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
    if (hwChannel >= 0 && hwChannel < 8) {
        ndspChnWaveBufClear(hwChannel);
    } else {
        hwChannel = -1;
        for (int i = 0; i < 8; i++) {
            if (!ndspChnIsPlaying(i)) {
                hwChannel = i;
                break;
            }
        }
        if (hwChannel < 0) hwChannel = 0;
        s->channel = hwChannel;
    }

    ndspChnReset(hwChannel);
    ndspChnSetInterp(hwChannel, NDSP_INTERP_LINEAR);
    ndspChnSetRate(hwChannel, s->sampleRate);
    ndspChnSetFormat(hwChannel, s->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);

    float mix[12] = {0};
    mix[0] = mix[1] = 1.0f;
    ndspChnSetMix(hwChannel, mix);

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

void loadDirectory(const char* path) {
    entryCount = 0;
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL && entryCount < MAX_FILES) {
        snprintf(entries[entryCount].name, sizeof(entries[entryCount].name), "%s", ent->d_name);
        if (ent->d_type == DT_DIR) {
            entries[entryCount].isDir = 1;
        } else if (ent->d_type == DT_UNKNOWN) {
            char fullpath[PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);
            struct stat st;
            if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
                entries[entryCount].isDir = 1;
            } else {
                entries[entryCount].isDir = 0;
            }
        } else {
            entries[entryCount].isDir = 0;
        }
        entryCount++;
    }
    closedir(dir);
}

int main(int argc, char** argv) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    consoleInit(GFX_TOP, NULL);
    ndspInit();

    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    u32 clrButtonIdle = C2D_Color32(0x93, 0x93, 0x93, 0xFF);
    u32 clrButtonPressed = C2D_Color32(0xF0, 0x7D, 0x00, 0xFF);
    u32 clrClear = C2D_Color32(0x4B, 0x36, 0x9D, 0x68);
    touchPosition touch;

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
    
    consoleClear();
    printf("\x1b[0;0HSELECT to change modes   ");

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
        u32 kUp = hidKeysUp();

        if (kDown & KEY_START) break;

        if (kDown & KEY_SELECT) {
            mode = MODE_MENU_MAIN;
            consoleClear();
            printf("\x1b[0;0HPad %d selected.\nPress A to change sound, B to return.\n", selectedPad);
        }
        
        switch (mode) {
            case MODE_PLAY: {
                for (int i = 0; i < NUM_PADS; i++) {
                    PadRect* pad = &pads[i];
                    bool isTouched = (kHeld & KEY_TOUCH) &&
                                     touch.px >= pad->x && touch.px <= pad->x+pad->w &&
                                     touch.py >= pad->y && touch.py <= pad->y+pad->h;
                    if (isTouched && !pad->pressed) {
                        pad->pressed = true;
                        play_sample(pad->sample);
                    } else if (kDown & mapped_keys[i]) {
                        pad->pressed = true;
                        play_sample(pad->sample);
                    } else if (isTouched || (kHeld & mapped_keys[i])) {
                        pad->pressed = true;
                    } else if (kUp & mapped_keys[i]) {
                        pad->pressed = false;
                    } else if (!isTouched && pad->pressed) {
                        pad->pressed = false;
                    }
                }
                break;
            }
            case MODE_MENU_MAIN: {
                if (kDown & KEY_DLEFT) {
                    selectedPad = (selectedPad - 1 + NUM_PADS) % NUM_PADS;
                    printf("\x1b[0;0HPad %d selected.\nPress A to change sound, B to return.\n", selectedPad);
                }
                if (kDown & KEY_DRIGHT) {
                    selectedPad = (selectedPad + 1) % NUM_PADS;
                    printf("\x1b[0;0HPad %d selected.\nPress A to change sound, B to return.\n", selectedPad);
                }
                if (kDown & KEY_A) {
                    mode = MODE_MENU_SOUND;
                    strcpy(currentPath, "sdmc:/sounds");
                    loadDirectory(currentPath);
                    fileBrowserSelected = 0;
                    fileBrowserScroll = 0;
                    consoleClear();
                }
                if (kDown & KEY_B) {
                    mode = MODE_PLAY;
                    consoleClear();
                    printf("\x1b[0;0HSELECT to change modes   ");
                }
                break;
            }
            case MODE_MENU_SOUND: {
                if (kDown & KEY_B) {
                    char* lastSlash = strrchr(currentPath, '/');
                    if (lastSlash && lastSlash > currentPath + strlen("sdmc:/sounds")) {
                        *lastSlash = '\0';
                        loadDirectory(currentPath);
                        fileBrowserSelected = 0;
                        fileBrowserScroll = 0;
                        consoleClear();
                    } 
                    else {
                        mode = MODE_MENU_MAIN;
                        consoleClear();
                        printf("\x1b[0;0HPad %d selected.\nPress A to change sound, B to return.\n", selectedPad);
                    }
                }
                if (kDown & KEY_X) {
                    char fullpath[PATH_MAX];
                    snprintf(fullpath, sizeof(fullpath), "%s/%s", currentPath, entries[fileBrowserSelected].name);
                    AudioSample preview = {0};
                    if (load_wav(fullpath, &preview)) {
                        play_sample(&preview);
                        if (preview.data) linearFree(preview.data);
                    }
                }
                
                if (kDown & KEY_DOWN) fileBrowserSelected = (fileBrowserSelected + 1) % entryCount;
                if (kDown & KEY_UP) fileBrowserSelected = (fileBrowserSelected - 1 + entryCount) % entryCount;
                
                if (fileBrowserSelected >= fileBrowserScroll + 20) {
                    fileBrowserScroll++;
                }
                if (fileBrowserSelected < fileBrowserScroll) {
                    fileBrowserScroll--;
                }
                if (kDown & KEY_A) {
                    if (entries[fileBrowserSelected].isDir) {
                        char newPath[PATH_MAX];
                        snprintf(newPath, sizeof(newPath), "%s/%s", currentPath, entries[fileBrowserSelected].name);
                        strcpy(currentPath, newPath);
                        loadDirectory(currentPath);
                        fileBrowserSelected = 0;
                        fileBrowserScroll = 0;
                        consoleClear();
                    } else {
                        char chosenFile[PATH_MAX];
                        snprintf(chosenFile, sizeof(chosenFile), "%s/%s", currentPath, entries[fileBrowserSelected].name);
                        assignSoundToPad(selectedPad, chosenFile);
                        consoleClear();
                        printf("\x1b[0;0HSELECT to change modes   ");
                        mode = MODE_PLAY;
                    }
                }
                
                printf("\x1b[0;0HBrowsing: %s\n", currentPath);
                int y_offset = 2;
                for (int i = 0; i < 20 && (i + fileBrowserScroll) < entryCount; i++) {
                    int entryIndex = i + fileBrowserScroll;
                    printf("\x1b[%d;0H", y_offset + i);
                    if (entryIndex == fileBrowserSelected) {
                        printf("-> %s      ", entries[entryIndex].name);
                    } else {
                        printf("   %s      ", entries[entryIndex].name);
                    }
                }
                printf("\x1b[%d;0H\nPress X to preview sounds, B to go back", y_offset + 20);
                break;
            }
        }

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

    for (int i = 0; i < NUM_PADS; i++) {
        if (sounds[i].data) linearFree(sounds[i].data);
    }

    C2D_Fini(); C3D_Fini(); ndspExit(); gfxExit();
    return 0;
}