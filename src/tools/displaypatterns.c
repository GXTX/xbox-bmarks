/*
	xbox_oc
	----------------
	Copyright (C) 2025 wutno (https://github.com/GXTX)
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "globals.h"

static SDL_Window *window = NULL;
static SDL_Surface *screenSurface = NULL;

void GenerateColorSteps()
{
    SDL_LockSurface(screenSurface);

    uint32_t* pixels = (uint32_t*)screenSurface->pixels;
    SDL_PixelFormat *format = screenSurface->format;

    uint8_t percentMargin = 8;

    const size_t xFrame = screenSurface->w;
    const size_t xMargin = screenSurface->w / percentMargin;
    const size_t yFrame = screenSurface->h;
    const size_t yMargin = screenSurface->h / percentMargin;

    uint8_t totalSteps = 32;
    uint8_t colorStep = 0xFF / totalSteps + 1;
    unsigned int colorBarXSize = (xFrame - xMargin - xMargin) / totalSteps;

    uint8_t totalBars = 4;
    unsigned int colorBarYSize = (yFrame - yMargin - yMargin) / totalBars;

    uint8_t barIndex = 0;
    for (int y = 0; y < yFrame; y++) {
        uint16_t color = 0;

        for (int x = 0; x < xFrame; x++) {

            // Generate border
            if (y == 0 || y == yFrame - 1 ||
                x == 0 || x == xFrame - 1)
                pixels[(y * xFrame + x)] = SDL_MapRGB(format,  255, 255, 255);


            // In the range we need to do stuff
            if (x >= xMargin - 1 && x <= xFrame - xMargin) {
                if ((x - xMargin) % colorBarXSize == 0) {
                    color += colorStep;
                    if (color > 0xFF)
                        color = 0xFF;
                }

                // We're in between both the margins
                if (y >= yMargin && y <= yFrame - yMargin + 1) {
                    if (barIndex == 0) {
                        pixels[(y * xFrame + x)] = SDL_MapRGB(format, color, 0, 0);
                    }
                    else if (barIndex == 1) {
                        pixels[(y * xFrame + x)] = SDL_MapRGB(format, 0, color, 0);
                    }
                    else if (barIndex == 2) {
                        pixels[(y * xFrame + x)] = SDL_MapRGB(format, 0, 0, color);
                    }
                    else if (barIndex == 3) {
                        pixels[(y * xFrame + x)] = SDL_MapRGB(format, color, color, color);
                    }

                    // Draw the inner box
                    if (y == yMargin || y == yFrame - yMargin + 1 || x == xMargin - 1 || x == xFrame - xMargin)
                        pixels[(y * xFrame + x)] = SDL_MapRGB(format,  255, 255, 255);
                }
            }
        }

        if ((y - yMargin) % colorBarYSize == 0 && y > yMargin) {
            barIndex++;
            if (barIndex > totalBars)
                barIndex = totalBars;
        }
    }
    SDL_UnlockSurface(screenSurface);
}

void GenerateCrossHatch()
{
    SDL_LockSurface(screenSurface);

    uint32_t* pixels = (uint32_t*)screenSurface->pixels;
    SDL_PixelFormat *format = screenSurface->format;

    uint8_t spacing = 32;
    size_t xStart = spacing / 2;
    size_t yStart = spacing / 2;

    for (size_t y = 0; y < screenSurface->h; ++y) {
        for (size_t x = 0; x < screenSurface->w; ++x) {
            if ((x >= xStart && (x - xStart) % spacing == 0) ||
                (y >= yStart && (y - yStart) % spacing == 0))
            {
                pixels[y * screenSurface->w + x] = SDL_MapRGB(format,  255, 255, 255);
            }
            else {
                pixels[y * screenSurface->w + x] = SDL_MapRGB(format,  0, 0, 0);
            }
        }
    }
    SDL_UnlockSurface(screenSurface);
}

void GenerateAlertnatingPixels()
{
    SDL_LockSurface(screenSurface);

    uint32_t* pixels = (uint32_t*)screenSurface->pixels;
    SDL_PixelFormat *format = screenSurface->format;

    for (size_t y = 0; y < screenSurface->h; ++y) {
        bool alt = true;
        for (size_t x = 0; x < screenSurface->w; ++x) {
            if (alt) {
                pixels[y * screenSurface->w + x] = SDL_MapRGB(format,  255, 255, 255);
            }
            else {
                pixels[y * screenSurface->w + x] = SDL_MapRGB(format,  0, 0, 0);
            }

            // 2px margin with gray border
            if (y <= 1 || y >= screenSurface->h - 2||
                x <= 1 || x >= screenSurface->w - 2)
                pixels[(y * screenSurface->w + x)] = SDL_MapRGB(format,  0x88, 0x88, 0x88);

            alt = !alt;
        }
    }
    SDL_UnlockSurface(screenSurface);
}

static size_t currentLine = 0;
static SDL_Surface *hLine = NULL;
void GenerateHorizontalLineScroll()
{
    if (hLine == NULL) {
        hLine = SDL_CreateRGBSurface(0, 1, screenSurface->h, 32, 0,0,0,0);
        SDL_FillRect(hLine, NULL, 0xFFFFFF);
    }

    SDL_PixelFormat *format = screenSurface->format;

    if (currentLine > screenSurface->w - 1)
        currentLine = 0;

    const SDL_Rect xy = {
        currentLine, 0,
        0, 0
    };
    SDL_BlitSurface(hLine, NULL, screenSurface, &xy);

    currentLine++;
}

static SDL_Surface *vLine = NULL;
void GenerateVerticalLineScroll()
{
    if (vLine == NULL) {
        vLine = SDL_CreateRGBSurface(0, screenSurface->w, 1, 32, 0,0,0,0);
        SDL_FillRect(vLine, NULL, 0xFFFFFF);
    }

    SDL_PixelFormat *format = screenSurface->format;

    if (currentLine > screenSurface->h - 1)
        currentLine = 0;

    const SDL_Rect xy = {
        0, currentLine,
        0, 0
    };
    SDL_BlitSurface(vLine, NULL, screenSurface, &xy);

    currentLine++;
}

#define MAX_PATTERNS 6
static Menu patterns = {
    {
        {"Color Steps", 0, GenerateColorSteps, NULL},
        {"Crosshatch", 0, GenerateCrossHatch, NULL},
        {"Alertnating Pixels", 0, GenerateAlertnatingPixels, NULL},
        {"Horizontal Linescroll", 0, GenerateHorizontalLineScroll, NULL},
        {"Vertical Linescroll", 0, GenerateVerticalLineScroll, NULL},
        //{"Circle", 0, shutdown, NULL},
        {"Exit...", 0, shutdown, NULL}
    }
};

void displaypatterns(char *)
{
    // We only init gamecontroller, so we need to start the display subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return;
    
    window = SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

    // We're directly modifying the screen screenSurface, so let's grab it
    screenSurface = SDL_GetWindowSurface(window);
    SDL_FillRect(screenSurface, NULL, 0);

    int position = 0;
    patterns.item[position].selected = true;

    ULONG counter = 0;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: patterns.item[position].selected = false; position--; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: patterns.item[position].selected = false; position++; break;
                    case SDL_CONTROLLER_BUTTON_BACK: running = false; break;
                    default: break;
                }
            }
        }

        if (position < 0)
            position = MAX_MENU_ITEMS - 1;
        else if (position > MAX_MENU_ITEMS - 1)
            position = 0;
        patterns.item[position].selected = TRUE;

        SDL_FillRect(screenSurface, NULL, 0);
        patterns.item[position].func(patterns.item[position].argument);
        XVideoWaitForVBlank();
        SDL_UpdateWindowSurface(window);
#if 0
        XVideoWaitForVBlank();
        debugResetCursor();
        debugPrint("%lu\n", counter++);
        for (int i = 0; i < MAX_PATTERNS; i++) {
            debugPrint("%s %s\n", patterns.item[i].selected ? "->" :"  ", patterns.item[i].name);
        }
#endif
    }
}
