#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
#define CORE_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define MATH_IMPLEMENTATION

#include <iostream>
#include "core.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

int main()
{
    srand(time(NULL));

    Window_t win;
    windowInit(&win);
    win.title = "X";

    if (!createWindow(&win)) return 1;

    Input input;
    inputInit(&input);

    SDL_Texture *tex = SDL_CreateTexture(win.renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        win.bWidth, win.bHeight);

    while (true)
    {
        updateFrame(&win);
        if (pollEvents(&win, &input)) break;
        if (isKeyPressed(&input, KEY_ESCAPE)) break;

        double dt = getDelta(&win);

        updateFramebuffer(&win, tex);

        imguiNewFrame();
            ImGui::Begin("STATE");
            ImGui::Text("FPS: %.1f", getFPS(&win));
            ImGui::End();
        imguiEndFrame(&win);

        SDL_RenderPresent(win.renderer);
    }

    SDL_DestroyTexture(tex);
    destroyWindow(&win);
}
