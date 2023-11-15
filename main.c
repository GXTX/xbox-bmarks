#include "src/tools/globals.h"

int main()
{
    XVideoSetMode(1280, 720, 32, REFRESH_DEFAULT);

    if (!initsdl())
        running = false;

    int position = 0;
    menu.item[position].selected = true;

    ULONG counter = 0;
    while (running) {
        XVideoWaitForVBlank();
        debugResetCursor();
        debugPrint("%lu\n", counter++);
        for (int i = 0; i < MAX_MENU_ITEMS; i++) {
            debugPrint("%s %s\n", menu.item[i].selected ? "->" :"  ", menu.item[i].name);
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: menu.item[position].selected = false; position--; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: menu.item[position].selected = false; position++; break;
                    case SDL_CONTROLLER_BUTTON_A:
                        debugClearScreen();
                        menu.item[position].func(menu.item[position].argument);
                        debugClearScreen();
                        break;
                    case SDL_CONTROLLER_BUTTON_BACK: running = false; break;
                    default: break;
                }
            }
        }

        if (position < 0)
            position = MAX_MENU_ITEMS - 1;
        else if (position > MAX_MENU_ITEMS - 1)
            position = 0;
        menu.item[position].selected = TRUE;
    }

    SDL_Quit();
    return 0;
}
