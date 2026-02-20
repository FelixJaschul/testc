// ============================================================================
// core.h - Core window management (X11 or SDL3 backend)
// ============================================================================
#ifndef WRAPPER_CORE_H
#define WRAPPER_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SDL_IMPLEMENTATION
    #include <SDL3/SDL.h>
    #ifdef IMGUI_IMPLEMENTATION
        #include <imgui.h>
        #include <imgui_impl_sdl3.h>
        #include <imgui_impl_sdlrenderer3.h>
    #endif
#else
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SDL_IMPLEMENTATION
#define ASSERT(x) do { if(!(x)) { std::cout << "Error: " << #x << " " << SDL_GetError() << std::endl ; exit(1); } } while(0)
#else
#define ASSERT(x) do { if(!(x)) { std::cout << "Error: " << #x << " " << std::endl; exit(1); } } while(0)
#endif
#define LOG(x) do { std::cout << x << std::endl; } while(0)

typedef struct Window {
#ifdef SDL_IMPLEMENTATION
    SDL_Window   *window;
    SDL_Renderer *renderer;
#else
    Window   window;
    Display *display;
    XImage  *image;
    int      screen;
    GC       gc;
#endif

    int width;
    int height;
    int x, y;
    const char *title;
    uint32_t *buffer;
    int bWidth;
    int bHeight;
    double fps;
    double deltat;
    struct timespec lastt;

    bool buffer_valid;
    size_t buffer_size;
    bool resized;
    bool vsync;
} Window_t;

void windowInit(Window_t *w);
bool createWindow(Window_t *w);
void destroyWindow(Window_t *w);
void updateFrame(Window_t *w);
double getFPS(const Window_t *w);
double getDelta(const Window_t *w);
void drawPixel(const Window_t *w, int x, int y, uint32_t color);
void setVSync(Window_t *w, bool enable);

// Internal buffer management
bool resizeBuffer(Window_t *w);
void freeBuffer(Window_t *w);

#ifdef SDL_IMPLEMENTATION
bool updateFramebuffer(const Window_t *w, SDL_Texture *texture);
#else
bool updateFramebuffer(const Window_t *w);
#endif

#if defined(SDL_IMPLEMENTATION) && defined(IMGUI_IMPLEMENTATION)
inline void imguiNewFrame()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

inline void imguiEndFrame(Window_t *w)
{
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), w->renderer);
}
#else
#define imguiNewFrame() do {} while(0)
#define imguiEndFrame(w) do {} while(0)
#endif

#ifdef __cplusplus
}
#endif

// ============================================================================
// IMPLEMENTATION
// ============================================================================

#ifdef CORE_IMPLEMENTATION

inline void freeBuffer(Window_t *w)
{
    if (w->buffer) {
        free(w->buffer);
        w->buffer = NULL;
    }
    w->buffer_valid = false;
    w->buffer_size = 0;
}

inline bool resizeBuffer(Window_t *w)
{
    if (w->buffer_valid) freeBuffer(w);

    const size_t sz = w->bWidth * w->bHeight * sizeof(uint32_t);
    w->buffer = (uint32_t*)calloc(1, sz);
    if (!w->buffer) {
        fprintf(stderr, "Failed to allocate framebuffer (%dx%d)\n", w->bWidth, w->bHeight);
        return false;
    }

    w->buffer_size = sz;
    w->buffer_valid = true;

#ifdef SDL_IMPLEMENTATION
    // Texture handled in updateFramebuffer
#else
    if (w->image) {
        XDestroyImage(w->image);
        w->image = NULL;
    }

    w->image = XCreateImage(
        w->display,
        DefaultVisual(w->display, w->screen),
        DefaultDepth(w->display, w->screen),
        ZPixmap, 0, (char*)w->buffer,
        w->bWidth, w->bHeight, 32, 0
    );

    if (!w->image) {
        fprintf(stderr, "Failed to create XImage\n");
        freeBuffer(w);
        return false;
    }
#endif

    return true;
}

inline void windowInit(Window_t *w)
{
#ifdef SDL_IMPLEMENTATION
    w->window   = NULL;
    w->renderer = NULL;
#else
    w->display = NULL;
    w->screen  = 0;
    w->window  = 0;
    w->gc      = 0;
    w->image   = NULL;
#endif

    w->width  = 800;
    w->height = 600;
    w->x      = 100;
    w->y      = 100;
    w->title  = "DEMO WINDOW";
    w->buffer = NULL;
    w->bWidth = 800;
    w->bHeight= 600;
    w->fps    = 60.0;
    w->deltat = 0.0;
    w->buffer_valid = false;
    w->buffer_size = 0;
    w->resized = false;
    w->vsync = false;
    clock_gettime(CLOCK_MONOTONIC, &w->lastt);
}

inline bool createWindow(Window_t *w)
{
#ifdef SDL_IMPLEMENTATION
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }
    }

    w->window = SDL_CreateWindow(
        w->title,
        w->width,
        w->height,
        SDL_WINDOW_RESIZABLE
    );
    if (!w->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    w->renderer = SDL_CreateRenderer(w->window, NULL);
    if (!w->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(w->window);
        w->window = NULL;
        return false;
    }

    if (w->vsync) {
        SDL_SetRenderVSync(w->renderer, 1);
    }

    if (!resizeBuffer(w)) {
        SDL_DestroyRenderer(w->renderer);
        SDL_DestroyWindow(w->window);
        w->renderer = NULL;
        w->window = NULL;
        return false;
    }

#ifdef IMGUI_IMPLEMENTATION
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(w->window, w->renderer);
    ImGui_ImplSDLRenderer3_Init(w->renderer);
#endif

    clock_gettime(CLOCK_MONOTONIC, &w->lastt);
    return true;
#else
    w->display = XOpenDisplay(NULL);
    if (!w->display) {
        fprintf(stderr, "Failed to open X11 display\n");
        return false;
    }

    w->screen = DefaultScreen(w->display);
    w->window = XCreateSimpleWindow(
        w->display, RootWindow(w->display, w->screen),
        w->x, w->y, w->width, w->height, 0,
        BlackPixel(w->display, w->screen),
        WhitePixel(w->display, w->screen)
    );

    if (!w->window) {
        fprintf(stderr, "Failed to create X11 window\n");
        XCloseDisplay(w->display);
        w->display = NULL;
        return false;
    }

    XStoreName(w->display, w->window, w->title);
    XSelectInput(w->display, w->window,
        ExposureMask | KeyPressMask | KeyReleaseMask |
        StructureNotifyMask | PointerMotionMask |
        ButtonPressMask | ButtonReleaseMask);

    Atom wmDeleteMessage = XInternAtom(w->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(w->display, w->window, &wmDeleteMessage, 1);

    XMapWindow(w->display, w->window);
    XFlush(w->display);

    XEvent ev;
    XWindowEvent(w->display, w->window, ExposureMask, &ev);

    w->gc = DefaultGC(w->display, w->screen);

    if (!resizeBuffer(w)) {
        XDestroyWindow(w->display, w->window);
        XCloseDisplay(w->display);
        w->display = NULL;
        w->window = 0;
        return false;
    }

    clock_gettime(CLOCK_MONOTONIC, &w->lastt);
    return true;
#endif
}

inline void destroyWindow(Window_t *w)
{
#ifdef SDL_IMPLEMENTATION
#ifdef IMGUI_IMPLEMENTATION
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
#endif
    if (w->renderer) {
        SDL_DestroyRenderer(w->renderer);
        w->renderer = NULL;
    }
    if (w->window) {
        SDL_DestroyWindow(w->window);
        w->window = NULL;
    }
    freeBuffer(w);
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
#else
    if (!w->display) return;

    if (w->image) {
        XDestroyImage(w->image);
        w->image  = NULL;
        w->buffer = NULL;
    }

    freeBuffer(w);

    if (w->window) {
        XDestroyWindow(w->display, w->window);
        w->window = 0;
    }

    XSync(w->display, False);
    XCloseDisplay(w->display);
    w->display = NULL;
#endif
}

inline void updateFrame(Window_t *w)
{
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    double elapsed = (current_time.tv_sec - w->lastt.tv_sec) +
                     (current_time.tv_nsec - w->lastt.tv_nsec) / 1e9;

    const double target_frame_time = 1.0 / w->fps;
    if (elapsed < target_frame_time && !w->vsync) {
        const double sleep_time = target_frame_time - elapsed;
#ifdef SDL_IMPLEMENTATION
        SDL_Delay((Uint32)(sleep_time * 1000.0));
#else
        usleep((useconds_t)(sleep_time * 1e6));
#endif
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        elapsed = (current_time.tv_sec - w->lastt.tv_sec) +
                  (current_time.tv_nsec - w->lastt.tv_nsec) / 1e9;
    }

    w->deltat = elapsed;
    w->lastt  = current_time;
}

#ifdef SDL_IMPLEMENTATION
inline bool updateFramebuffer(const Window_t *w, SDL_Texture *texture)
{
    if (!w->renderer || !w->buffer_valid || !texture) return false;

    void* pixels; int pitch;
    if (!SDL_LockTexture(texture, NULL, &pixels, &pitch)) {
        fprintf(stderr, "SDL_LockTexture failed: %s\n", SDL_GetError());
        return false;
    }

    const int expected_pitch = w->bWidth * 4;
    if (pitch != expected_pitch) {
        memset(pixels, 0, w->bHeight * pitch);
    }

    memcpy(pixels, w->buffer, w->buffer_size);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(w->renderer);
    SDL_RenderTexture(w->renderer, texture, NULL, NULL);
    return true;
}
#else
inline bool updateFramebuffer(const Window_t *w)
{
    if (!w->buffer_valid || !w->image) return false;

    XPutImage(w->display, w->window, w->gc, w->image,
              0, 0, 0, 0, w->bWidth, w->bHeight);
    XFlush(w->display);
    return true;
}
#endif

inline double getFPS(const Window_t *w)
{
    if (w->deltat <= 0.0) return 0.0;
    const double fps = 1.0 / w->deltat;
    if (fps < 0.1) return 0.1;
    if (fps > 10000.0) return 10000.0;
    return fps;
}

inline double getDelta(const Window_t *w)
{
    return w->deltat;
}

inline void drawPixel(const Window_t *w, int x, int y, uint32_t color)
{
    if (w->buffer_valid && x >= 0 && x < w->bWidth && y >= 0 && y < w->bHeight)
        w->buffer[y * w->bWidth + x] = color;
}

inline void setVSync(Window_t *w, bool enable)
{
#ifdef SDL_IMPLEMENTATION
    w->vsync = enable;
    if (w->renderer) {
        SDL_SetRenderVSync(w->renderer, enable ? 1 : 0);
    }
#else
    (void)w; (void)enable;
#endif
}

#endif // CORE_IMPLEMENTATION
#endif // WRAPPER_CORE_H


// ============================================================================
// Keyboard and Mouse Input (X11 or SDL3 backend)
// ============================================================================
#ifndef WRAPPER_KEYS_H
#define WRAPPER_KEYS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEY_UNKNOWN = 0,

    KEY_ESCAPE, KEY_SPACE,
    KEY_ENTER,  KEY_TAB, KEY_BACKSPACE,

    KEY_LEFT, KEY_RIGHT,
    KEY_UP,   KEY_DOWN,

    KEY_A, KEY_B, KEY_C,
    KEY_D, KEY_E, KEY_F,
    KEY_G, KEY_H, KEY_I,
    KEY_J, KEY_K, KEY_L,
    KEY_M, KEY_N, KEY_O,
    KEY_P, KEY_Q, KEY_R,
    KEY_S, KEY_T, KEY_U,
    KEY_V, KEY_W, KEY_X,
    KEY_Y, KEY_Z,

    KEY_NUM0, KEY_NUM1, KEY_NUM2,
    KEY_NUM3, KEY_NUM4, KEY_NUM5,
    KEY_NUM6, KEY_NUM7, KEY_NUM8,
    KEY_NUM9,

    KEY_LSHIFT, KEY_RSHIFT,
    KEY_LCTRL,  KEY_RCTRL,
    KEY_LALT,   KEY_RALT,

    KEY_F1, KEY_F2,
    KEY_F3, KEY_F4,

    KEY_COUNT
} Key;

typedef enum {
    MOUSE_LEFT = 0,
    MOUSE_MIDDLE,
    MOUSE_RIGHT,
    MOUSE_COUNT
} MouseButton;

typedef struct InputState {
    bool key_curr[KEY_COUNT];
    bool key_prev[KEY_COUNT];
    bool mouse_curr[MOUSE_COUNT];
    bool mouse_prev[MOUSE_COUNT];
    int mouse_x, mouse_y;
    int mouse_dx, mouse_dy;
    int last_x, last_y;
    bool mouse_grabbed;
#ifdef SDL_IMPLEMENTATION
    void *grab_window;
#else
    Window grab_window;
#endif
    int center_x, center_y;
} Input;

// Initialize input state - call once at startup
void inputInit(Input *input);

// Poll events and update input state - call once per frame BEFORE processing input
// Returns true if window should close
// NOTE: This automatically calls updateInput() at the end, so you don't need to call it manually
#ifdef SDL_IMPLEMENTATION
bool pollEvents(Window_t *win, Input *input);
#else
bool pollEvents(Window_t *win, Input *input);
#endif

// Copy current to previous - ONLY call manually if not using pollEvents
void updateInput(Input *input);

bool isKeyDown(const Input *input, Key key);
bool isKeyPressed(const Input *input, Key key);
bool isKeyReleased(const Input *input, Key key);
bool isMouseDown(const Input *input, MouseButton btn);
bool isMousePressed(const Input *input, MouseButton btn);
bool isMouseReleased(const Input *input, MouseButton btn);
void getMousePosition(const Input *input, int *x, int *y);
void getMouseDelta(const Input *input, int *dx, int *dy);
bool isMouseGrabbed(const Input *input);

#ifdef SDL_IMPLEMENTATION
void grabMouse(SDL_Window *window, int width, int height, Input *input);
void releaseMouse(SDL_Window *window, Input *input);
#else
void grabMouse(Display *display, Window window, int width, int height, Input *input);
void releaseMouse(Display *display, Window window, Input *input);
#endif

#ifdef __cplusplus
}
#endif

#ifdef KEYS_IMPLEMENTATION

static void setKey(Input *input, const Key key, const bool down)
{
    if (key < KEY_COUNT) input->key_curr[key] = down;
}

static void setMouse(Input *input, const MouseButton btn, const bool down)
{
    if (btn < MOUSE_COUNT) input->mouse_curr[btn] = down;
}

inline void inputInit(Input *input)
{
    for (int i = 0; i < KEY_COUNT; i++) {
        input->key_curr[i] = false;
        input->key_prev[i] = false;
    }
    for (int i = 0; i < MOUSE_COUNT; i++) {
        input->mouse_curr[i] = false;
        input->mouse_prev[i] = false;
    }
    input->mouse_x = 0;
    input->mouse_y = 0;
    input->mouse_dx = 0;
    input->mouse_dy = 0;
    input->last_x = 0;
    input->last_y = 0;
    input->mouse_grabbed = false;
    input->grab_window = 0;
    input->center_x = 400;
    input->center_y = 300;
}

#ifdef SDL_IMPLEMENTATION

inline bool pollEvents(Window_t *win, Input *input)
{
    bool shouldClose = false;

    input->mouse_dx = 0;
    input->mouse_dy = 0;

    // Handle window resize
    if (win) {
        int nw, nh;
        SDL_GetWindowSize(win->window, &nw, &nh);
        if (nw != win->width || nh != win->height) {
            win->width = nw;
            win->height = nh;
            // Note: bWidth/bHeight should be updated in main.cpp based on RENDER_SCALE
            win->resized = true;
        }
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
#ifdef IMGUI_IMPLEMENTATION
        ImGui_ImplSDL3_ProcessEvent(&event);
#endif
        switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                shouldClose = true;
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (input->mouse_grabbed) {
                    input->mouse_dx += (int)event.motion.xrel;
                    input->mouse_dy += (int)event.motion.yrel;
                }
                input->mouse_x = (int)event.motion.x;
                input->mouse_y = (int)event.motion.y;
                input->last_x  = input->mouse_x;
                input->last_y  = input->mouse_y;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                const bool down = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT:   setMouse(input, MOUSE_LEFT,   down); break;
                    case SDL_BUTTON_MIDDLE: setMouse(input, MOUSE_MIDDLE, down); break;
                    case SDL_BUTTON_RIGHT:  setMouse(input, MOUSE_RIGHT,  down); break;
                    default: break;
                }
                break;
            }

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                const bool down = (event.type == SDL_EVENT_KEY_DOWN);
                const SDL_Keycode sym = event.key.key;
                Key k = KEY_UNKNOWN;

                switch (sym) {
                    case SDLK_ESCAPE:  k = KEY_ESCAPE; break;
                    case SDLK_SPACE:   k = KEY_SPACE; break;
                    case SDLK_RETURN:  k = KEY_ENTER; break;
                    case SDLK_TAB:     k = KEY_TAB; break;
                    case SDLK_BACKSPACE: k = KEY_BACKSPACE; break;

                    case SDLK_LEFT:  k = KEY_LEFT; break;
                    case SDLK_RIGHT: k = KEY_RIGHT; break;
                    case SDLK_UP:    k = KEY_UP; break;
                    case SDLK_DOWN:  k = KEY_DOWN; break;

                    case SDLK_A: k = KEY_A; break;
                    case SDLK_B: k = KEY_B; break;
                    case SDLK_C: k = KEY_C; break;
                    case SDLK_D: k = KEY_D; break;
                    case SDLK_E: k = KEY_E; break;
                    case SDLK_F: k = KEY_F; break;
                    case SDLK_G: k = KEY_G; break;
                    case SDLK_H: k = KEY_H; break;
                    case SDLK_I: k = KEY_I; break;
                    case SDLK_J: k = KEY_J; break;
                    case SDLK_K: k = KEY_K; break;
                    case SDLK_L: k = KEY_L; break;
                    case SDLK_M: k = KEY_M; break;
                    case SDLK_N: k = KEY_N; break;
                    case SDLK_O: k = KEY_O; break;
                    case SDLK_P: k = KEY_P; break;
                    case SDLK_Q: k = KEY_Q; break;
                    case SDLK_R: k = KEY_R; break;
                    case SDLK_S: k = KEY_S; break;
                    case SDLK_T: k = KEY_T; break;
                    case SDLK_U: k = KEY_U; break;
                    case SDLK_V: k = KEY_V; break;
                    case SDLK_W: k = KEY_W; break;
                    case SDLK_X: k = KEY_X; break;
                    case SDLK_Y: k = KEY_Y; break;
                    case SDLK_Z: k = KEY_Z; break;

                    case SDLK_0: k = KEY_NUM0; break;
                    case SDLK_1: k = KEY_NUM1; break;
                    case SDLK_2: k = KEY_NUM2; break;
                    case SDLK_3: k = KEY_NUM3; break;
                    case SDLK_4: k = KEY_NUM4; break;
                    case SDLK_5: k = KEY_NUM5; break;
                    case SDLK_6: k = KEY_NUM6; break;
                    case SDLK_7: k = KEY_NUM7; break;
                    case SDLK_8: k = KEY_NUM8; break;
                    case SDLK_9: k = KEY_NUM9; break;

                    case SDLK_LSHIFT: k = KEY_LSHIFT; break;
                    case SDLK_RSHIFT: k = KEY_RSHIFT; break;
                    case SDLK_LCTRL:  k = KEY_LCTRL; break;
                    case SDLK_RCTRL:  k = KEY_RCTRL; break;
                    case SDLK_LALT:   k = KEY_LALT; break;
                    case SDLK_RALT:   k = KEY_RALT; break;

                    case SDLK_F1: k = KEY_F1; break;
                    case SDLK_F2: k = KEY_F2; break;
                    case SDLK_F3: k = KEY_F3; break;
                    case SDLK_F4: k = KEY_F4; break;

                    default: break;
                }

                if (k != KEY_UNKNOWN) setKey(input, k, down);
                break;
            }

            default:
                break;
        }
    }

    updateInput(input);
    return shouldClose;
}

#else // X11 backend

inline bool pollEvents(Window_t *win, Input *input)
{
    const Atom wmDeleteMessage = XInternAtom(win->display, "WM_DELETE_WINDOW", False);
    bool shouldClose = false;

    input->mouse_dx = 0;
    input->mouse_dy = 0;

    // Handle window resize
    if (win) {
        XEvent ev;
        while (XCheckTypedWindowEvent(win->display, win->window, ConfigureNotify, &ev)) {
            if (ev.xconfigure.width != win->width || ev.xconfigure.height != win->height) {
                win->width = ev.xconfigure.width;
                win->height = ev.xconfigure.height;
                // Note: bWidth/bHeight should be updated in main.cpp based on RENDER_SCALE
                win->resized = true;
            }
        }
        XFlush(win->display);
    }

    while (XPending(win->display) > 0)
    {
        XEvent event;
        XNextEvent(win->display, &event);

        switch (event.type)
        {
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == wmDeleteMessage)
                    shouldClose = true;
                break;

            case MotionNotify:
                if (input->mouse_grabbed) {
                    input->mouse_dx += event.xmotion.x - input->last_x;
                    input->mouse_dy += event.xmotion.y - input->last_y;
                }
                input->mouse_x = event.xmotion.x;
                input->mouse_y = event.xmotion.y;
                input->last_x = input->mouse_x;
                input->last_y = input->mouse_y;
                break;

            case ButtonPress:
            case ButtonRelease:
            {
                const bool down = (event.type == ButtonPress);
                switch (event.xbutton.button)
                {
                    case Button1: setMouse(input, MOUSE_LEFT, down); break;
                    case Button2: setMouse(input, MOUSE_MIDDLE, down); break;
                    case Button3: setMouse(input, MOUSE_RIGHT, down); break;
                    default: break;
                }
                break;
            }

            case KeyPress:
            case KeyRelease:
            {
                const bool down = (event.type == KeyPress);
                const KeySym sym = XLookupKeysym(&event.xkey, 0);
                Key k = KEY_UNKNOWN;

                switch (sym)
                {
                    case XK_Escape: k = KEY_ESCAPE; break;
                    case XK_space: k = KEY_SPACE; break;
                    case XK_Return: k = KEY_ENTER; break;
                    case XK_Tab: k = KEY_TAB; break;
                    case XK_BackSpace: k = KEY_BACKSPACE; break;

                    case XK_Left: k = KEY_LEFT; break;
                    case XK_Right: k = KEY_RIGHT; break;
                    case XK_Up: k = KEY_UP; break;
                    case XK_Down: k = KEY_DOWN; break;

                    case XK_a: case XK_A: k = KEY_A; break;
                    case XK_b: case XK_B: k = KEY_B; break;
                    case XK_c: case XK_C: k = KEY_C; break;
                    case XK_d: case XK_D: k = KEY_D; break;
                    case XK_e: case XK_E: k = KEY_E; break;
                    case XK_f: case XK_F: k = KEY_F; break;
                    case XK_g: case XK_G: k = KEY_G; break;
                    case XK_h: case XK_H: k = KEY_H; break;
                    case XK_i: case XK_I: k = KEY_I; break;
                    case XK_j: case XK_J: k = KEY_J; break;
                    case XK_k: case XK_K: k = KEY_K; break;
                    case XK_l: case XK_L: k = KEY_L; break;
                    case XK_m: case XK_M: k = KEY_M; break;
                    case XK_n: case XK_N: k = KEY_N; break;
                    case XK_o: case XK_O: k = KEY_O; break;
                    case XK_p: case XK_P: k = KEY_P; break;
                    case XK_q: case XK_Q: k = KEY_Q; break;
                    case XK_r: case XK_R: k = KEY_R; break;
                    case XK_s: case XK_S: k = KEY_S; break;
                    case XK_t: case XK_T: k = KEY_T; break;
                    case XK_u: case XK_U: k = KEY_U; break;
                    case XK_v: case XK_V: k = KEY_V; break;
                    case XK_w: case XK_W: k = KEY_W; break;
                    case XK_x: case XK_X: k = KEY_X; break;
                    case XK_y: case XK_Y: k = KEY_Y; break;
                    case XK_z: case XK_Z: k = KEY_Z; break;

                    case XK_0: k = KEY_NUM0; break;
                    case XK_1: k = KEY_NUM1; break;
                    case XK_2: k = KEY_NUM2; break;
                    case XK_3: k = KEY_NUM3; break;
                    case XK_4: k = KEY_NUM4; break;
                    case XK_5: k = KEY_NUM5; break;
                    case XK_6: k = KEY_NUM6; break;
                    case XK_7: k = KEY_NUM7; break;
                    case XK_8: k = KEY_NUM8; break;
                    case XK_9: k = KEY_NUM9; break;

                    case XK_Shift_L: k = KEY_LSHIFT; break;
                    case XK_Shift_R: k = KEY_RSHIFT; break;
                    case XK_Control_L: k = KEY_LCTRL; break;
                    case XK_Control_R: k = KEY_RCTRL; break;
                    case XK_Alt_L: k = KEY_LALT; break;
                    case XK_Alt_R: k = KEY_RALT; break;

                    case XK_F1: k = KEY_F1; break;
                    case XK_F2: k = KEY_F2; break;
                    case XK_F3: k = KEY_F3; break;
                    case XK_F4: k = KEY_F4; break;

                    default: break;
                }

                if (k != KEY_UNKNOWN) setKey(input, k, down);
                break;
            }

            default: break;
        }
    }

    if (input->mouse_grabbed && (input->mouse_dx != 0 || input->mouse_dy != 0))
    {
        XWarpPointer(win->display, None, input->grab_window, 0, 0, 0, 0, input->center_x, input->center_y);
        XFlush(win->display);
        input->last_x = input->center_x;
        input->last_y = input->center_y;
    }

    updateInput(input);
    return shouldClose;
}

#endif

inline void updateInput(Input *input)
{
    for (int i = 0; i < KEY_COUNT; i++)
        input->key_prev[i] = input->key_curr[i];
    for (int i = 0; i < MOUSE_COUNT; i++)
        input->mouse_prev[i] = input->mouse_curr[i];
}

inline bool isKeyDown(const Input *input, const Key key)
{
    return key < KEY_COUNT ? input->key_curr[key] : false;
}

inline bool isKeyPressed(const Input *input, const Key key)
{
    return key < KEY_COUNT ? (input->key_curr[key] && !input->key_prev[key]) : false;
}

inline bool isKeyReleased(const Input *input, const Key key)
{
    return key < KEY_COUNT ? (!input->key_curr[key] && input->key_prev[key]) : false;
}

inline bool isMouseDown(const Input *input, const MouseButton btn)
{
    return btn < MOUSE_COUNT ? input->mouse_curr[btn] : false;
}

inline bool isMousePressed(const Input *input, const MouseButton btn)
{
    return btn < MOUSE_COUNT ? (input->mouse_curr[btn] && !input->mouse_prev[btn]) : false;
}

inline bool isMouseReleased(const Input *input, const MouseButton btn)
{
    return btn < MOUSE_COUNT ? (!input->mouse_curr[btn] && input->mouse_prev[btn]) : false;
}

inline void getMousePosition(const Input *input, int *x, int *y)
{
    if (x) *x = input->mouse_x;
    if (y) *y = input->mouse_y;
}

inline void getMouseDelta(const Input *input, int *dx, int *dy)
{
    if (dx) *dx = input->mouse_dx;
    if (dy) *dy = input->mouse_dy;
}

#ifdef SDL_IMPLEMENTATION

inline void grabMouse(SDL_Window *window, const int width, const int height, Input *input)
{
    if (input->mouse_grabbed) return;

    input->center_x = width / 2;
    input->center_y = height / 2;
    input->grab_window = (void*)window;

    SDL_SetWindowRelativeMouseMode(window, true);

    input->mouse_grabbed = true;
    input->mouse_dx = 0;
    input->mouse_dy = 0;
}

inline void releaseMouse(SDL_Window *window, Input *input)
{
    if (!input->mouse_grabbed) return;

    SDL_SetWindowRelativeMouseMode(window, false);

    input->mouse_grabbed = false;
    input->mouse_dx = 0;
    input->mouse_dy = 0;
}

#else

inline void grabMouse(Display *display, const Window window,
                      const int width, const int height, Input *input)
{
    if (input->mouse_grabbed) return;

    input->center_x = width / 2;
    input->center_y = height / 2;
    input->grab_window = window;

    XColor dummy;
    const char data[1] = {0};
    const Pixmap blank = XCreateBitmapFromData(display, window, data, 1, 1);
    const Cursor cursor = XCreatePixmapCursor(display, blank, blank,
                                              &dummy, &dummy, 0, 0);
    XFreePixmap(display, blank);

    XDefineCursor(display, window, cursor);
    XGrabPointer(display, window, True,
                 PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                 GrabModeAsync, GrabModeAsync, window, None, CurrentTime);

    XWarpPointer(display, None, window, 0, 0, 0, 0,
                 input->center_x, input->center_y);
    XFlush(display);

    input->last_x = input->center_x;
    input->last_y = input->center_y;
    input->mouse_grabbed = true;
    input->mouse_dx = 0;
    input->mouse_dy = 0;
}

inline void releaseMouse(Display *display, const Window window, Input *input)
{
    if (!input->mouse_grabbed) return;

    XUngrabPointer(display, CurrentTime);
    XDefineCursor(display, window, None);
    input->mouse_grabbed = false;
    input->mouse_dx = 0;
    input->mouse_dy = 0;
}

#endif

inline bool isMouseGrabbed(const Input *input)
{
    return input->mouse_grabbed;
}

#endif // KEYS_IMPLEMENTATION
#endif // WRAPPER_KEYS_H


// ============================================================================
// Vector math
// ============================================================================
#ifndef WRAPPER_MATH_H
#define WRAPPER_MATH_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    Vec3 origin, direction;
} Ray;

/*  -> Example:
 *  Vec3 v = vec3(1.0f, 2.0f, 3.0f);
 */
Vec3  vec3(float x, float y, float z);

/*  -> Example:
 *  Vec3 c = add(a, b);
 */
Vec3  add(Vec3 a, Vec3 b);

/*  -> Example:
 *  Vec3 c = sub(a, b);
 */
Vec3  sub(Vec3 a, Vec3 b);

/*  -> Example:
 *  Vec3 v = mul(a, 2.0f);
 */
Vec3  mul(Vec3 v, float t);

/*  -> Example:
 *  Vec3 c = vmul(a, b);
 */
Vec3  vmul(Vec3 a, Vec3 b);

/*  -> Example:
 *  Vec3 v = vdiv(a, 2.0f);
 */
Vec3  vdiv(Vec3 v, float t);

/*  -> Example:
 *  float d = dot(a, b);
 */
float dot(Vec3 a, Vec3 b);

/*  -> Example:
 *  Vec3 n = cross(a, b);
 */
Vec3  cross(Vec3 a, Vec3 b);

/*  -> Example:
 *  float length = len(v);
 */
float len(Vec3 v);

/*  -> Example:
 *  Vec3 unit = norm(v);
 */
Vec3  norm(Vec3 v);

/*  -> Example:
 *  Vec3 reflected = reflect(incident, normal);
 */
Vec3  reflect(Vec3 v, Vec3 n);

#ifdef MATH_IMPLEMENTATION

inline Vec3 vec3(const float x, const float y, const float z)
{
    return (Vec3) {x, y, z};
}

inline Vec3 add(const Vec3 a, const Vec3 b)
{
    return (Vec3) {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
    };
}

inline Vec3 sub(const Vec3 a, const Vec3 b)
{
    return (Vec3) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z,
    };
}

inline Vec3 mul(const Vec3 v, const float t)
{
    return (Vec3) {
        v.x * t,
        v.y * t,
        v.z * t,
    };
}

inline Vec3 vmul(const Vec3 a, const Vec3 b)
{
    return (Vec3) {
        a.x * b.x,
        a.y * b.y,
        a.z * b.z,
    };
}

inline Vec3 vdiv(const Vec3 v, const float t)
{
    const float inv = 1.0f / t;
    return (Vec3) {
        v.x * inv,
        v.y * inv,
        v.z * inv,
    };
}

inline float dot(const Vec3 a, const Vec3 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

inline Vec3 cross(const Vec3 a, const Vec3 b)
{
    return (Vec3) {
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x),
    };
}

inline float len(const Vec3 v)
{
    return sqrtf(dot(v,v));
}

inline Vec3 norm(const Vec3 v)
{
    const float l2 = dot(v,v);
    if (l2 > 0.0f)
    {
        const float inv = 1.0f / sqrtf(l2);
        return (Vec3) {
            v.x * inv,
            v.y * inv,
            v.z * inv,
        };
    }
    return v;
}

inline Vec3 reflect(const Vec3 v, const Vec3 n)
{
    const float d = 2.0f * dot(v,n);
    return (Vec3) {
        v.x - (d * n.x),
        v.y - (d * n.y),
        v.z - (d * n.z),
    };
}

#endif // MATH_IMPLEMENTATION
#endif // WRAPPER_MATH_H


#ifndef WRAPPER_CAMERA_H
#define WRAPPER_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Camera {
    Vec3 position;
    float yaw;      // Rotation in degrees
    float pitch;    // Rotation in degrees
    float fov;      // Field of view in degrees
    Vec3 front;     // Forward direction vector
    Vec3 right;     // Right direction vector
    Vec3 up;        // Up direction vector
} Camera;

// Initialize camera with default position and orientation
/*  -> Example:
 *  Camera camera;
 *  cameraInit(&camera);
 *  camera.position = vec3(0.0f, 1.0f, 5.0f);
 */
void cameraInit(Camera *cam);

// Update camera vectors based on yaw/pitch (automatically clamps pitch)
/*  -> Example:
 *  camera.yaw += 10.0f;
 *  cameraUpdate(&camera);
 */
void cameraUpdate(Camera *cam);

// Move camera in given direction by speed amount
/*  -> Example:
 *  if (isKeyDown(&input, KEY_W)) cameraMove(&camera, camera.front, 0.1f);
 */
void cameraMove(Camera *cam, Vec3 direction, float speed);

// Rotate camera by delta angles in degrees
/*  -> Example:
 *  cameraRotate(&camera, mouse_dx * sensitivity, -mouse_dy * sensitivity);
 */
void cameraRotate(Camera *cam, float dyaw, float dpitch);

// Generate ray from camera with pre-scaled viewport offsets
/*  -> Example:
 *  float* u_offsets = (float*)malloc(win.width * sizeof(float));
    float* v_offsets = (float*)malloc(win.height * sizeof(float));
    assert(u_offsets && v_offsets && "Failed to allocate viewport offset buffers");

    for (int x = 0; x < win.width; x++)  u_offsets[x] = ((float)x / (float)(win.width - 1) - 0.5f) * viewport_width;
    for (int y = 0; y < win.height; y++) v_offsets[y] = ((float)(win.height - 1 - y) / (float)(win.height - 1) - 0.5f) * viewport_height;

    while (1) {
        ...
        #pragma omp parallel for schedule(dynamic) default(none) shared(win, camera, u_offsets, v_offsets)
        for (int y = 0; y < win.height; y++) {
            for (int x = 0; x < win.width; x++) {
                const Ray ray = cameraGetRay(&camera, u_offsets[x], v_offsets[y]);
                const Vec3 color = calculate_ray_color(ray, MAX_BOUNCES);
                win.buffer[y * win.width + x] = uint32(color);
        }
    }
}*/
Ray cameraGetRay(const Camera* cam, float u_scaled, float v_scaled);

#ifdef __cplusplus
}
#endif

#ifdef CAMERA_IMPLEMENTATION

inline void cameraInit(Camera *cam)
{
    cam->position = vec3(0.0f, 0.0f, 0.0f);
    cam->yaw = 0.0f;
    cam->pitch = 0.0f;
    cam->fov = 60.0f;
    cameraUpdate(cam);
}

inline void cameraUpdate(Camera *cam)
{
    // Clamp pitch to avoid gimbal lock
    if (cam->pitch > 89.0f) cam->pitch = 89.0f;
    if (cam->pitch < -89.0f) cam->pitch = -89.0f;

    const float yaw_rad = cam->yaw * M_PI / 180.0f;
    const float pitch_rad = cam->pitch * M_PI / 180.0f;

    cam->front = vec3(
        cosf(yaw_rad) * cosf(pitch_rad),
        sinf(pitch_rad),
        sinf(yaw_rad) * cosf(pitch_rad)
    );

    cam->front = norm(cam->front);
    cam->right = norm(cross(cam->front, vec3(0, 1, 0)));
    cam->up    = cross(cam->right, cam->front);
}

inline void cameraMove(Camera *cam, const Vec3 direction, const float speed)
{
    cam->position = add(cam->position, mul(direction, speed));
}

inline void cameraRotate(Camera *cam, const float dyaw, const float dpitch)
{
    cam->yaw += dyaw;
    cam->pitch += dpitch;
    cameraUpdate(cam);
}

inline Ray cameraGetRay(const Camera* cam, const float u_scaled, const float v_scaled)
{
    Vec3 rd = add(cam->front, add(mul(cam->up, v_scaled), mul(cam->right, u_scaled)));
    rd = norm(rd);
    return (Ray){cam->position, rd};
}

#endif // CAMERA_IMPLEMENTATION
#endif // WRAPPER_CAMERA_H


// ============================================================================
// 3D model with materials and transforms
// ============================================================================
#ifndef WRAPPER_MODEL_H
#define WRAPPER_MODEL_H

#define INITIAL_VERTEX_CAPACITY 1024
#define INITIAL_TRIANGLE_CAPACITY 2048

#ifdef __cplusplus
extern "C" {
#endif

// Material properties for rendering
typedef struct {
    Vec3 color;
    float reflectivity;  // 0.0 = matte, 1.0 = mirror
	float specular;
} Material;

// Triangle primitive for meshes
typedef struct {
    Vec3 v0, v1, v2;
    Vec3 color;
} Triangle;

// 3D model with transform and material
typedef struct {
    Triangle* triangles;              // Original mesh triangles
    Triangle* transformed_triangles;  // World-space transformed triangles
    int num_triangles;
    int capacity;                      // Allocated triangle capacity
    Vec3 position;
    float rot_x, rot_y, rot_z;         // Euler angles in radians
    Vec3 scale;
    Material mat;
} Model;

// Create new model in storage array (returns NULL if array is full)
/*  -> Example:
 *  Model* cube = modelCreate(scene_models, &num_models, MAX_MODELS, vec3(1,0,0), 0.5f);
 */
Model* modelCreate(Model* storage, int* count, int max, Vec3 color, float refl, float spec);

// Load OBJ file into model (dynamically allocates triangles)
/*  -> Example:
 *  modelLoad(cube, "res/cube.obj");
 */
void modelLoad(Model* m, const char* path);

// Free model triangle data
/*  -> Example:
 *  modelFree(cube);
 */
void modelFree(Model* m);

// Set model transform (position, rotation in radians, scale)
/*  -> Example:
 *  modelTransform(cube, vec3(0,0,0), vec3(0, M_PI/4, 0), vec3(1,1,1));
 */
void modelTransform(Model* m, Vec3 pos, Vec3 rot, Vec3 scale);

// Apply transforms to all models in array (call after changing transforms)
/*  -> Example:
 *  modelUpdate(scene_models, num_models);
 */
void modelUpdate(const Model* models, int count);

#ifdef __cplusplus
}
#endif

#ifdef MODEL_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

// Rotation helpers
static inline Vec3 rotate_x(const Vec3 v, const float a)
{
    const float c = cosf(a), s = sinf(a);
    return vec3(v.x, v.y * c - v.z * s, v.y * s + v.z * c);
}

static inline Vec3 rotate_y(const Vec3 v, const float a)
{
    const float c = cosf(a), s = sinf(a);
    return vec3(v.x * c - v.z * s, v.y, v.x * s + v.z * c);
}

static inline Vec3 rotate_z(const Vec3 v, const float a)
{
    const float c = cosf(a), s = sinf(a);
    return vec3(v.x * c - v.y * s, v.x * s + v.y * c, v.z);
}

static inline Vec3 transform_vertex(Vec3 v, const Model* m)
{
    v = vmul(v, m->scale);
    if (m->rot_z) v = rotate_z(v, m->rot_z);
    if (m->rot_x) v = rotate_x(v, m->rot_x);
    if (m->rot_y) v = rotate_y(v, m->rot_y);
    return add(v, m->position);
}

inline Model* modelCreate(Model* storage, int* count, const int max, const Vec3 color, const float refl, const float spec)
{
    if (*count >= max) return NULL;
    Model* m = &storage[(*count)++];
    m->triangles = NULL;
    m->transformed_triangles = NULL;
    m->num_triangles = 0;
    m->capacity = 0;
    m->position = (Vec3){0, 0, 0};
    m->scale = (Vec3){1.0f, 1.0f, 1.0f};
    m->rot_x = 0; m->rot_y = 0; m->rot_z = 0;
    m->mat = (Material){color, refl, spec};
    return m;
}

inline void modelFree(Model* m)
{
    if (m->triangles)
    {
        free(m->triangles);
        m->triangles = NULL;
    }

    if (m->transformed_triangles)
    {
        free(m->transformed_triangles);
        m->transformed_triangles = NULL;
    }

    m->num_triangles = 0;
    m->capacity = 0;
}

inline void modelTransform(Model* m, const Vec3 pos, const Vec3 rot, const Vec3 scale)
{
    m->position = pos;
    m->rot_x = rot.x;
    m->rot_y = rot.y;
    m->rot_z = rot.z;
    m->scale = scale;
}

inline void modelUpdate(const Model* models, const int count)
{
    for (int i = 0; i < count; i++)
    {
        const Model* m = &models[i];
        for (int j = 0; j < m->num_triangles; j++)
        {
            m->transformed_triangles[j].v0 = transform_vertex(m->triangles[j].v0, m);
            m->transformed_triangles[j].v1 = transform_vertex(m->triangles[j].v1, m);
            m->transformed_triangles[j].v2 = transform_vertex(m->triangles[j].v2, m);
        }
    }
}

inline void modelLoad(Model* m, const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open OBJ file: %s\n", path);
        return;
    }

    // Dynamic arrays for vertices and triangles
    int vert_capacity = INITIAL_VERTEX_CAPACITY;
    int tri_capacity  = INITIAL_TRIANGLE_CAPACITY;

    Vec3* verts     = (Vec3*)malloc(vert_capacity * sizeof(Vec3));
    Triangle* tris = (Triangle*)malloc(tri_capacity * sizeof(Triangle));
    assert(verts && tris && "Failed to allocate OBJ parsing buffers");

    int nv = 0, nt = 0;
    char buf[256];

    while (fgets(buf, sizeof(buf), f))
    {
        if (buf[0] == 'v' && buf[1] == ' ')
        {
            // Vertex line
            if (nv >= vert_capacity)
            {
                vert_capacity *= 2;
                verts = (Vec3*)realloc(verts, vert_capacity * sizeof(Vec3));
                assert(verts && "Failed to reallocate vertex buffer");
            }

            float x, y, z;
            sscanf(buf + 2, "%f %f %f", &x, &y, &z);
            verts[nv++] = vec3(x, y, z);
        }
        else if (buf[0] == 'f')
        {
            // Face line (only supports triangulated meshes)
            if (nt >= tri_capacity)
            {
                tri_capacity *= 2;
                tris = (Triangle*)realloc(tris, tri_capacity * sizeof(Triangle));
                assert(tris && "Failed to reallocate triangle buffer");
            }

            int a, b, c;
            if (sscanf(buf + 2, "%d %d %d", &a, &b, &c) == 3)
            {
                // OBJ indices are 1-based
                if (a > 0 && a <= nv && b > 0 && b <= nv && c > 0 && c <= nv)
                {
                    tris[nt++] = (Triangle){verts[a-1], verts[b-1], verts[c-1]};
                }
            }
        }
    }
    fclose(f);

    // Allocate exact size for model
    m->triangles             = (Triangle*)malloc(nt * sizeof(Triangle));
    m->transformed_triangles = (Triangle*)malloc(nt * sizeof(Triangle));
    assert(m->triangles && m->transformed_triangles && "Failed to allocate model triangles");

    memcpy(m->triangles, tris, nt * sizeof(Triangle));
    m->num_triangles = nt;
    m->capacity = nt;

    // Free temporary buffers
    free(verts);
    free(tris);

    printf("Loaded %s: %d vertices, %d triangles\n", path, nv, nt);
}

#endif // MODEL_IMPLEMENTATION
#endif // WRAPPER_MODEL_H


// ============================================================================
// 3D Rasterization Renderer
// ============================================================================
#ifndef WRAPPER_RENDER3D_H
#define WRAPPER_RENDER3D_H

#ifdef __cplusplus
extern "C" {
#endif

// Depth buffer for 3D rendering
typedef struct {
    float* depths;
    int width;
    int height;
    bool valid;
} DepthBuffer;

// Rendering context combining window, camera, and depth buffer
typedef struct {
    Window_t* window;
    Camera* camera;
    DepthBuffer depth;
    bool wireframe;
    bool backface_culling;
    bool light;
    Vec3 light_dir;
} Renderer;

// Initialize 3D renderer
/*  -> Example:
 *  Renderer renderer;
 *  renderInit(&renderer, &window, &camera);
 */
void renderInit(Renderer* r, Window_t* win, Camera* cam);

// Free 3D renderer resources
void renderFree(Renderer* r);

// Clear depth buffer
void renderClear(Renderer* r);

// Render a single model
/*  -> Example:
 *  renderModel(&renderer, &cube_model);
 */
void renderModel(Renderer* r, const Model* m);

// Render array of models
/*  -> Example:
 *  renderScene(&renderer, scene_models, num_models);
 */
void renderScene(Renderer* r, const Model* models, int count);

#ifdef __cplusplus
}
#endif

#ifdef RENDER3D_IMPLEMENTATION

#include <float.h>

// Matrix 4x4 for transformations
typedef struct {
    float m[16];
} Mat4;

// Create perspective projection matrix
static inline Mat4 _perspective(float fov, float aspect, float near, float far)
{
    Mat4 mat = {0};
    const float tan_half_fov = tanf(fov * 0.5f * M_PI / 180.0f);
    mat.m[0] = 1.0f / (aspect * tan_half_fov);
    mat.m[5] = 1.0f / tan_half_fov;
    mat.m[10] = -(far + near) / (far - near);
    mat.m[11] = -1.0f;
    mat.m[14] = -(2.0f * far * near) / (far - near);
    return mat;
}

// Create view matrix from camera
static inline Mat4 _lookAt(Vec3 eye, Vec3 target, Vec3 up)
{
    Mat4 mat = {0};
    const Vec3 f = norm(sub(target, eye));
    const Vec3 r = norm(cross(f, up));
    const Vec3 u = cross(r, f);

    mat.m[0] = r.x; mat.m[4] = r.y; mat.m[8]  = r.z;  mat.m[12] = -dot(r, eye);
    mat.m[1] = u.x; mat.m[5] = u.y; mat.m[9]  = u.z;  mat.m[13] = -dot(u, eye);
    mat.m[2] =-f.x; mat.m[6] =-f.y; mat.m[10] =-f.z;  mat.m[14] =  dot(f, eye);
    mat.m[15] = 1.0f;
    return mat;
}

// Multiply matrix by vector (w=1)
static inline Vec3 _mat4_mul_vec3(const Mat4* m, Vec3 v, float* w_out)
{
    const float x = m->m[0]*v.x + m->m[4]*v.y + m->m[8]*v.z  + m->m[12];
    const float y = m->m[1]*v.x + m->m[5]*v.y + m->m[9]*v.z  + m->m[13];
    const float z = m->m[2]*v.x + m->m[6]*v.y + m->m[10]*v.z + m->m[14];
    const float w = m->m[3]*v.x + m->m[7]*v.y + m->m[11]*v.z + m->m[15];
    if (w_out) *w_out = w;
    return vec3(x, y, z);
}

// Multiply two matrices
static inline Mat4 _mat4_mul(const Mat4* a, const Mat4* b)
{
    Mat4 r = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            r.m[i + j*4] =
                a->m[i + 0*4] * b->m[0 + j*4] +
                a->m[i + 1*4] * b->m[1 + j*4] +
                a->m[i + 2*4] * b->m[2 + j*4] +
                a->m[i + 3*4] * b->m[3 + j*4];
        }
    }
    return r;
}

// Convert to screen space
static inline void _to_screen(Vec3* v, int width, int height)
{
    v->x = (v->x + 1.0f) * 0.5f * width;
    v->y = (1.0f - v->y) * 0.5f * height;
}

// Simple lighting calculation
static inline float _calculate_lighting(Vec3 normal, Vec3 light_dir)
{
    const float diffuse = fmaxf(0.0f, -dot(normal, light_dir));
    return fminf(1.0f, diffuse);
}

// Convert float color to uint32
static inline uint32_t _vec3_to_color(Vec3 c, float brightness)
{
    const int r = (int)(fminf(1.0f, c.x * brightness) * 255.0f);
    const int g = (int)(fminf(1.0f, c.y * brightness) * 255.0f);
    const int b = (int)(fminf(1.0f, c.z * brightness) * 255.0f);
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

// Draw line using Bresenham's algorithm
static inline void _draw_line(Window_t* w, int x0, int y0, int x1, int y1, uint32_t color)
{
    const int dx = abs(x1 - x0);
    const int dy = abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        drawPixel(w, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

// Fill triangle with flat top/bottom optimization
static inline void _fill_triangle(Window_t* w, DepthBuffer* db, Vec3 v0, Vec3 v1, Vec3 v2, float z0, float z1, float z2, uint32_t color)
{
    // Sort vertices by y-coordinate (v0.y <= v1.y <= v2.y)
    if (v0.y > v1.y) { Vec3 tmp = v0; v0 = v1; v1 = tmp; float tz = z0; z0 = z1; z1 = tz; }
    if (v1.y > v2.y) { Vec3 tmp = v1; v1 = v2; v2 = tmp; float tz = z1; z1 = z2; z2 = tz; }
    if (v0.y > v1.y) { Vec3 tmp = v0; v0 = v1; v1 = tmp; float tz = z0; z0 = z1; z1 = tz; }

    const int y0 = (int)v0.y, y1 = (int)v1.y, y2 = (int)v2.y;

    for (int y = y0; y <= y2; y++) {
        if (y < 0 || y >= w->bHeight) continue;

        float x_start, x_end, z_start, z_end;

        if (y < y1) {
            // Upper part
            if (y1 == y0) continue;
            const float t1 = (float)(y - y0) / (float)(y1 - y0);
            const float t2 = (float)(y - y0) / (float)(y2 - y0);
            x_start = v0.x + (v1.x - v0.x) * t1;
            x_end   = v0.x + (v2.x - v0.x) * t2;
            z_start = z0 + (z1 - z0) * t1;
            z_end   = z0 + (z2 - z0) * t2;
        } else {
            // Lower part
            if (y2 == y1) continue;
            const float t1 = (float)(y - y1) / (float)(y2 - y1);
            const float t2 = (float)(y - y0) / (float)(y2 - y0);
            x_start = v1.x + (v2.x - v1.x) * t1;
            x_end   = v0.x + (v2.x - v0.x) * t2;
            z_start = z1 + (z2 - z1) * t1;
            z_end   = z0 + (z2 - z0) * t2;
        }

        if (x_start > x_end) {
            float tmp = x_start; x_start = x_end; x_end = tmp;
            tmp = z_start; z_start = z_end; z_end = tmp;
        }

        const int x0 = (int)x_start;
        const int x1 = (int)x_end;

        for (int x = x0; x <= x1; x++) {
            if (x < 0 || x >= w->bWidth) continue;

            const float t = (x1 == x0) ? 0.0f : (float)(x - x0) / (float)(x1 - x0);
            const float z = z_start + (z_end - z_start) * t;

            const int idx = y * w->bWidth + x;
            if (z < db->depths[idx]) {
                db->depths[idx] = z;
                drawPixel(w, x, y, color);
            }
        }
    }
}

inline void renderInit(Renderer* r, Window_t* win, Camera* cam)
{
    r->window = win;
    r->camera = cam;
    r->wireframe = false;
    r->backface_culling = true;
    r->light_dir = norm(vec3(0.3f, -1.0f, 0.5f));

    r->depth.width = win->bWidth;
    r->depth.height = win->bHeight;
    r->depth.depths = (float*)malloc(win->bWidth * win->bHeight * sizeof(float));
    r->depth.valid = (r->depth.depths != NULL);

    if (r->depth.valid) {
        renderClear(r);
    }
}

inline void renderFree(Renderer* r)
{
    if (r->depth.depths) {
        free(r->depth.depths);
        r->depth.depths = NULL;
    }
    r->depth.valid = false;
}

inline void renderClear(Renderer* r)
{
    if (!r->depth.valid) return;
    const int size = r->depth.width * r->depth.height;
    for (int i = 0; i < size; i++) {
        r->depth.depths[i] = FLT_MAX;
        r->window->buffer[i] = 0x000000;
    }
}

inline void renderModel(Renderer* r, const Model* m)
{
    // one Color for whole Model -> support lighting
    if (!r->depth.valid || !m || m->num_triangles == 0) return;

    Mat4 view = {0};
    view.m[0] = r->camera->right.x;   view.m[4] = r->camera->right.y;   view.m[8] = r->camera->right.z;   view.m[12] = -dot(r->camera->right, r->camera->position);
    view.m[1] = r->camera->up.x;      view.m[5] = r->camera->up.y;      view.m[9] = r->camera->up.z;      view.m[13] = -dot(r->camera->up, r->camera->position);
    view.m[2] = -r->camera->front.x;  view.m[6] = -r->camera->front.y;  view.m[10] = -r->camera->front.z; view.m[14] = dot(r->camera->front, r->camera->position);
    view.m[15] = 1.0f;

    const float aspect = (float)r->window->bWidth / (float)r->window->bHeight;
    const Mat4 proj = _perspective(r->camera->fov, aspect, 0.1f, 1000.0f);
    const Mat4 vp = _mat4_mul(&proj, &view);

    for (int i = 0; i < m->num_triangles; i++)
    {
        const Triangle* tri = &m->transformed_triangles[i];

        float w0, w1, w2;
        Vec3 c0 = _mat4_mul_vec3(&vp, tri->v0, &w0);
        Vec3 c1 = _mat4_mul_vec3(&vp, tri->v1, &w1);
        Vec3 c2 = _mat4_mul_vec3(&vp, tri->v2, &w2);

        if (w0 <= 0.0f || w1 <= 0.0f || w2 <= 0.0f) continue;

        c0 = vdiv(c0, w0);
        c1 = vdiv(c1, w1);
        c2 = vdiv(c2, w2);

        if (r->backface_culling) if (sub(c1, c0).x * sub(c2, c0).y - sub(c1, c0).y * sub(c2, c0).x <= 0.0f) continue;

        const float z0 = c0.z, z1 = c1.z, z2 = c2.z;
        _to_screen(&c0, r->window->bWidth, r->window->bHeight);
        _to_screen(&c1, r->window->bWidth, r->window->bHeight);
        _to_screen(&c2, r->window->bWidth, r->window->bHeight);

        // Apply lighting to per-triangle base color
        const Vec3 normal = norm(vdiv(add(add(tri->v0, tri->v1), tri->v2), 3.0f));
        float brightness = 1.0f;
        if (r->light) brightness = fmaxf(0.0f, -dot(normal, r->light_dir));
        const Vec3 lit_color = mul(tri->color, brightness); // component-wise multiply
        const uint32_t color = _vec3_to_color(lit_color, 1.0f);

        _fill_triangle(r->window, &r->depth, c0, c1, c2, z0, z1, z2, color);
    }
}

inline void renderScene(Renderer* r, const Model* models, int count)
{
    for (int i = 0; i < count; i++) renderModel(r, &models[i]);
}

#endif // RENDER3D_IMPLEMENTATION
#endif // WRAPPER_RENDER3D_H