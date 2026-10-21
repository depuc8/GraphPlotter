#include <SDL3/SDL.h>
#include <cmath>
#include <iostream>
#include <string>

#include "GraphPlotter.hpp"

using namespace std;

int main(int argc, char* argv[]){

    if(!SDL_Init(SDL_INIT_VIDEO)){
        cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Graph Plotter", 1000, 700, SDL_WINDOW_RESIZABLE);
    if(window == nullptr){
        cerr << "Failed window creation: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if(renderer == nullptr){
        cerr << "Failed renderer creation: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_StartTextInput(window);

    // Optional starting equation from the command line.
    GraphPlotter plotter;
    plotter.setEquation(argc > 1 ? argv[1] : "sin(x)");

    bool running = true;
    bool dragging = false;
    SDL_Event event;

    while(running){
        int w = 0, h = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &w, &h);

        while(SDL_PollEvent(&event)){
            // Mouse positions in render pixels (matters on HiDPI screens).
            SDL_ConvertEventToRenderCoordinates(renderer, &event);

            switch(event.type){
            case SDL_EVENT_QUIT:
                running = false;
                break;

            case SDL_EVENT_TEXT_INPUT:
                plotter.typeText(event.text.text);
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                plotter.zoom(pow(1.15, event.wheel.y), event.wheel.mouse_x, event.wheel.mouse_y, w, h);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if(event.button.button == SDL_BUTTON_LEFT) dragging = true;
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if(event.button.button == SDL_BUTTON_LEFT) dragging = false;
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if(dragging) plotter.pan(event.motion.xrel, event.motion.yrel);
                break;

            case SDL_EVENT_KEY_DOWN:
                switch(event.key.key){
                case SDLK_ESCAPE:    running = false; break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:  plotter.submit(); break;
                case SDLK_BACKSPACE: plotter.backspace(); break;
                case SDLK_V:
                    if(event.key.mod & SDL_KMOD_CTRL){
                        char* clip = SDL_GetClipboardText();
                        plotter.typeText(clip);
                        SDL_free(clip);
                    }
                    break;
                case SDLK_LEFT:      plotter.pan( 40,   0); break;
                case SDLK_RIGHT:     plotter.pan(-40,   0); break;
                case SDLK_UP:        plotter.pan(  0,  40); break;
                case SDLK_DOWN:      plotter.pan(  0, -40); break;
                case SDLK_PAGEUP:    plotter.zoom(1.25, w / 2.0f, h / 2.0f, w, h); break;
                case SDLK_PAGEDOWN:  plotter.zoom(0.8,  w / 2.0f, h / 2.0f, w, h); break;
                case SDLK_HOME:      plotter.resetView(); break;
                }
                break;
            }
        }

        plotter.render(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_StopTextInput(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
