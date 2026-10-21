#pragma once

#include <SDL3/SDL.h>
#include <string>

#include "Parser.hpp"

class GraphPlotter {
public:
    // Parses and plots the equation. On error the previous curve is kept
    // and error() says what went wrong.
    bool setEquation(const std::string& text);
    const std::string& error() const { return expression.error(); }

    // Editing the equation box at the top of the window.
    void typeText(const std::string& text) { input += text; }
    void backspace();
    bool submit() { return setEquation(input); }

    void render(SDL_Renderer* renderer) const;

    // View controls. Screen positions are in render pixels.
    void zoom(double factor, float sx, float sy, int w, int h);
    void pan(float dx, float dy);
    void resetView();

private:
    // The view: world point shown at the centre of the screen,
    // and how many pixels one world unit takes up.
    double centerX = 0.0;
    double centerY = 0.0;
    double scale = 50.0;

    Expression expression;
    std::string input;

    double toWorldX(float sx, int w) const { return centerX + (sx - w / 2.0) / scale; }
    double toWorldY(float sy, int h) const { return centerY - (sy - h / 2.0) / scale; }
    float toScreenX(double wx, int w) const { return float((wx - centerX) * scale + w / 2.0); }
    float toScreenY(double wy, int h) const { return float(h / 2.0 - (wy - centerY) * scale); }

    double gridStep() const;
    void drawGrid(SDL_Renderer* renderer, int w, int h) const;
    void drawAxes(SDL_Renderer* renderer, int w, int h) const;
    void drawCurve(SDL_Renderer* renderer, int w, int h) const;
    void drawOverlay(SDL_Renderer* renderer, int w, int h) const;
};
