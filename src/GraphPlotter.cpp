#include "GraphPlotter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace std;

namespace {

const int CHAR_SIZE = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;

string formatNumber(double v, double step){
    if(fabs(v) < step * 1e-6) v = 0.0;  // avoid "-0" and 1e-17 noise
    char buf[32];
    snprintf(buf, sizeof buf, "%g", v);
    return buf;
}

} // namespace

bool GraphPlotter::setEquation(const string& text){
    if(!expression.parse(text)) return false;
    equation = text;
    return true;
}

// Zoom so the world point under (sx, sy) stays under the cursor.
void GraphPlotter::zoom(double factor, float sx, float sy, int w, int h){
    double wx = toWorldX(sx, w);
    double wy = toWorldY(sy, h);
    scale = clamp(scale * factor, 1e-6, 1e9);
    centerX = wx - (sx - w / 2.0) / scale;
    centerY = wy + (sy - h / 2.0) / scale;
}

// Move the view by a screen distance, as if dragging the paper.
void GraphPlotter::pan(float dx, float dy){
    centerX -= dx / scale;
    centerY += dy / scale;
}

void GraphPlotter::resetView(){
    centerX = 0.0;
    centerY = 0.0;
    scale = 50.0;
}

// Distance between major grid lines: 1, 2 or 5 times a power of ten,
// chosen so lines stay at least ~80 pixels apart at any zoom.
double GraphPlotter::gridStep() const {
    double raw = 80.0 / scale;
    double magnitude = pow(10.0, floor(log10(raw)));
    double n = raw / magnitude;
    if(n <= 1.0) return magnitude;
    if(n <= 2.0) return 2.0 * magnitude;
    if(n <= 5.0) return 5.0 * magnitude;
    return 10.0 * magnitude;
}

void GraphPlotter::drawGrid(SDL_Renderer* renderer, int w, int h) const {
    double major = gridStep();
    double minor = major / 5.0;

    double left = toWorldX(0, w), right = toWorldX(w, w);
    double bottom = toWorldY(h, h), top = toWorldY(0, h);

    // Minor lines first so major lines draw over them.
    for(int pass = 0; pass < 2; pass++){
        double step = pass == 0 ? minor : major;
        if(pass == 0) SDL_SetRenderDrawColor(renderer, 236, 236, 240, 255);
        else          SDL_SetRenderDrawColor(renderer, 208, 208, 216, 255);

        for(long i = (long)ceil(left / step); i * step <= right; i++){
            float sx = toScreenX(i * step, w);
            SDL_RenderLine(renderer, sx, 0, sx, (float)h);
        }
        for(long i = (long)ceil(bottom / step); i * step <= top; i++){
            float sy = toScreenY(i * step, h);
            SDL_RenderLine(renderer, 0, sy, (float)w, sy);
        }
    }
}

void GraphPlotter::drawAxes(SDL_Renderer* renderer, int w, int h) const {
    float axisX = toScreenX(0.0, w);
    float axisY = toScreenY(0.0, h);

    SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
    if(axisY >= 0 && axisY <= h) SDL_RenderLine(renderer, 0, axisY, (float)w, axisY);
    if(axisX >= 0 && axisX <= w) SDL_RenderLine(renderer, axisX, 0, axisX, (float)h);

    // Numbers along the axes. When an axis is off screen its labels
    // stick to the nearest edge so the scale is always readable.
    double step = gridStep();
    float labelY = clamp(axisY + 4.0f, 4.0f, (float)(h - CHAR_SIZE - 4));
    float labelX = clamp(axisX + 4.0f, 4.0f, (float)(w - 4));

    SDL_SetRenderDrawColor(renderer, 90, 90, 100, 255);

    double left = toWorldX(0, w), right = toWorldX(w, w);
    for(long i = (long)ceil(left / step); i * step <= right; i++){
        if(i == 0) continue;
        string text = formatNumber(i * step, step);
        SDL_RenderDebugText(renderer, toScreenX(i * step, w) + 3, labelY, text.c_str());
    }

    double bottom = toWorldY(h, h), top = toWorldY(0, h);
    for(long i = (long)ceil(bottom / step); i * step <= top; i++){
        if(i == 0) continue;
        string text = formatNumber(i * step, step);
        float x = min(labelX, (float)(w - 4 - CHAR_SIZE * (int)text.size()));
        SDL_RenderDebugText(renderer, x, toScreenY(i * step, h) + 3, text.c_str());
    }

    if(axisX >= 0 && axisX <= w && axisY >= 0 && axisY <= h)
        SDL_RenderDebugText(renderer, axisX + 3, axisY + 4, "0");
}

void GraphPlotter::drawCurve(SDL_Renderer* renderer, int w, int h) const {
    if(!expression.valid()) return;

    SDL_SetRenderDrawColor(renderer, 30, 100, 220, 255);

    vector<SDL_FPoint> strip;
    auto flush = [&](){
        if(strip.size() >= 2){
            // Draw three offset copies for a ~2px thick line.
            const SDL_FPoint offsets[] = {{0, 0}, {1, 0}, {0, 1}};
            for(SDL_FPoint off : offsets){
                vector<SDL_FPoint> shifted(strip);
                for(SDL_FPoint& p : shifted){ p.x += off.x; p.y += off.y; }
                SDL_RenderLines(renderer, shifted.data(), (int)shifted.size());
            }
        }
        strip.clear();
    };

    // Two samples per pixel column. The curve is split into strips
    // wherever it is undefined (sqrt(-1), 1/0) or jumps by more than a
    // screen height between samples (asymptotes like tan(x) or 1/x).
    const int samplesPerPixel = 2;
    for(int i = 0; i <= w * samplesPerPixel; i++){
        float sx = (float)i / samplesPerPixel;
        double y = expression.eval(toWorldX(sx, w));

        if(!isfinite(y)){
            flush();
            continue;
        }

        float sy = toScreenY(y, h);
        if(!strip.empty() && fabs(sy - strip.back().y) > h) flush();

        // Keep coordinates in a sane range for the renderer.
        sy = clamp(sy, -10.0f * h, 11.0f * h);
        strip.push_back({sx, sy});
    }
    flush();
}

void GraphPlotter::render(SDL_Renderer* renderer) const {
    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);

    SDL_SetRenderDrawColor(renderer, 250, 250, 252, 255);
    SDL_RenderClear(renderer);

    drawGrid(renderer, w, h);
    drawAxes(renderer, w, h);
    drawCurve(renderer, w, h);

    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    string label = equation.find('=') == string::npos ? "y = " + equation : equation;
    SDL_RenderDebugText(renderer, 10, 10, label.c_str());
}
