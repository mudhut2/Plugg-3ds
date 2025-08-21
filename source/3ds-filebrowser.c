#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>
#include <dirent.h>
#include <sys/stat.h>
#include "3ds-filebrowser.h"

#define MAX_FILES 512

typedef struct {
    char name[256];
    int isDir;
} Entry;

static Entry entries[MAX_FILES];
static int entryCount = 0;

void loadDirectory(const char* path) {
    entryCount = 0;
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL && entryCount < MAX_FILES) {
        snprintf(entries[entryCount].name, sizeof(entries[entryCount].name), "%s", ent->d_name);

        // Check if directory
        if (ent->d_type == DT_DIR) {
            entries[entryCount].isDir = 1;
        } else if (ent->d_type == DT_UNKNOWN) {
            char fullpath[512];
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

char* openFileBrowser(const char* startPath) {
    static char chosenFile[512];  // keeps result after return
    char currentPath[512];
    strcpy(currentPath, startPath);

    loadDirectory(currentPath);
    int selected = 0;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) return NULL; // quit program
        if (kDown & KEY_B) {
            if (strcmp(currentPath, startPath) == 0) {
                return NULL;  
            } else {
                // go up one directory
                char* lastSlash = strrchr(currentPath, '/');
                if (lastSlash && lastSlash > currentPath) {
                    *lastSlash = '\0';
                    loadDirectory(currentPath);
                    selected = 0;
                }
            }
        }

        if (kDown & KEY_DOWN) selected = (selected + 1) % entryCount;
        if (kDown & KEY_UP)   selected = (selected - 1 + entryCount) % entryCount;

        if (kDown & KEY_A) {
            if (entries[selected].isDir) {
                char newPath[512];
                snprintf(newPath, sizeof(newPath), "%s/%s", currentPath, entries[selected].name);
                loadDirectory(newPath);
                strcpy(currentPath, newPath);
                selected = 0;
            } else {
                snprintf(chosenFile, sizeof(chosenFile), "%s/%s",
                         currentPath, entries[selected].name);
                return chosenFile; 
            }
        }

        // draw
        consoleClear();
        printf("Browsing: %s\n", currentPath);
        for (int i = 0; i < entryCount; i++) {
            if (i == selected) printf("-> %s\n", entries[i].name);
            else               printf("   %s\n", entries[i].name);
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    return NULL;
}

/* Uncomment for standalone test

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    char* chosen = openFileBrowser("sdmc:/sounds");
    if(chosen){
        printf("User picked %s\n", chosen);
    }

    gfxExit();
    return 0;
}
*/