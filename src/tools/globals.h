#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL.h>

#pragma once

bool initsdl();
void togglel2cache(char *);
void toggleudmamode(char *);
void setspindlespeed(char *);
void doswread(char* dev_name);
void dorawread(char *dev_name);
void runlinpack(char *);
void memspeed(char *);
void xboxoverclock(char *);
void shutdown(char *);

typedef struct MenuItem {
    char *name;
    bool selected;
    void (*func)(char *);
    void *argument;
} MenuItem;

// Must match total items...
#define MAX_MENU_ITEMS 8
typedef struct Menu {
    MenuItem item[MAX_MENU_ITEMS];
} Menu;

static Menu menu = {
    {
        {"CPU SpeedTest (Linpack)", 0, runlinpack, NULL},
        {"Memory SpeedTest (anybench memoryspeed.c)", 0, memspeed, NULL},
        {"HDD Read SpeedTest", 0, doswread, "\\Device\\HardDisk0\\Partition0"},
        {"DVD Read SpeedTest", 0, doswread, "\\Device\\CdRom0"},
        {"Overclock Utilities (Does not return)", 0, xboxoverclock, NULL},
        {"Set UDMA Mode", 0, toggleudmamode, NULL},
        {"Set DVD Spindle Speed", 0, setspindlespeed, NULL},
        //{"Toggle L2 Cache", 0, togglel2cache, NULL}, // FIXME: Broken on hardware?
        {"Exit...", 0, shutdown, NULL}
    }
};

static bool l2State = true;
static ULONG udma = 0x42; // TODO: Read this from the drive
static ULONG spindle = 1;
static SDL_Event event;
static SDL_GameController *controller;
static bool running = true;
