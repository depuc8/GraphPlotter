#include <SDL3/SDL.h>
#include <iostream>
#include <string>

#include "GraphPlotter.hpp"

using namespace std;

int main(int argc, char* argv[]){

    GraphPlotter plotter;
    string equation = argc > 1 ? argv[1] : "sin(x)";
    if(!plotter.setEquation(equation)){
        cerr << "Could not parse \"" << equation << "\": " << plotter.error() << '\n';
        return 1;
    }

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

    bool running = true;
    SDL_Event event;

    while(running){
        while(SDL_PollEvent(&event)){
            if(event.type == SDL_EVENT_QUIT){
                running = false;
            }
        }

        plotter.render(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
