#include "GraphPlotter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace std;

namespace {

const int CHAR_SIZE = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;

// Retro terminal palette: black screen, green phosphor, white text, blue trace.
const SDL_Color BACKGROUND = {0, 0, 0, 255};
const SDL_Color GRID_MINOR = {0, 38, 12, 255};
const SDL_Color GRID_MAJOR = {0, 80, 25, 255};
const SDL_Color AXIS       = {40, 255, 100, 255};
const SDL_Color LABEL      = {30, 190, 80, 255};
const SDL_Color TEXT       = {235, 255, 235, 255};
const SDL_Color PANEL      = {0, 22, 8, 255};
const SDL_Color CURVE_GLOW = {20, 60, 160, 255};
const SDL_Color CURVE      = {110, 190, 255, 255};
const SDL_Color SCANLINE   = {0, 0, 0, 90};

void setColor(SDL_Renderer* renderer, SDL_Color c){
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

string formatNumber(double v, double step){
    if(fabs(v) < step * 1e-6) v = 0.0;  // avoid "-0" and 1e-17 noise
    char buf[32];
    snprintf(buf, sizeof buf, "%g", v);
    return buf;
}

} // namespace

bool GraphPlotter::setEquation(const string& text){
    input = text;
    return expression.parse(text);
}

void GraphPlotter::backspace(){
    // Remove one whole UTF-8 character, not just its last byte.
    while(!input.empty()){
        unsigned char c = input.back();
        input.pop_back();
        if((c & 0xC0) != 0x80) break;
    }
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
        setColor(renderer, pass == 0 ? GRID_MINOR : GRID_MAJOR);

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

    setColor(renderer, AXIS);
    if(axisY >= 0 && axisY <= h) SDL_RenderLine(renderer, 0, axisY, (float)w, axisY);
    if(axisX >= 0 && axisX <= w) SDL_RenderLine(renderer, axisX, 0, axisX, (float)h);

    // Numbers along the axes. When an axis is off screen its labels
    // stick to the nearest edge so the scale is always readable.
    double step = gridStep();
    float labelY = clamp(axisY + 4.0f, 4.0f, (float)(h - CHAR_SIZE - 4));
    float labelX = clamp(axisX + 4.0f, 4.0f, (float)(w - 4));

    setColor(renderer, LABEL);

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

    vector<SDL_FPoint> strip;
    auto drawShifted = [&](float dx, float dy){
        vector<SDL_FPoint> shifted(strip);
        for(SDL_FPoint& p : shifted){ p.x += dx; p.y += dy; }
        SDL_RenderLines(renderer, shifted.data(), (int)shifted.size());
    };
    auto flush = [&](){
        if(strip.size() >= 2){
            // A wide dim pass for a phosphor glow, then a ~2px bright core.
            setColor(renderer, CURVE_GLOW);
            for(float d = -2; d <= 3; d++){ drawShifted(d, 0); drawShifted(0, d); }
            setColor(renderer, CURVE);
            drawShifted(0, 0); drawShifted(1, 0); drawShifted(0, 1);
        }
        strip.clear();
    };

    // Two samples per pixel column. The curve is split into strips
    // wherever it is undefined (sqrt(-1), 1/0) or jumps across an
    // asymptote (tan(x), 1/x). A big jump counts as an asymptote when the
    // value halfway between the samples doesn't lie between them; a steep
    // but continuous curve passes that test and stays connected.
    const int samplesPerPixel = 2;
    double prevX = 0.0, prevY = 0.0;
    for(int i = 0; i <= w * samplesPerPixel; i++){
        float sx = (float)i / samplesPerPixel;
        double x = toWorldX(sx, w);
        double y = expression.eval(x);

        if(!isfinite(y)){
            flush();
            continue;
        }

        if(!strip.empty() && fabs(y - prevY) * scale > h){
            double mid = expression.eval((x + prevX) / 2.0);
            if(!isfinite(mid) || mid < min(y, prevY) || mid > max(y, prevY)) flush();
        }
        prevX = x;
        prevY = y;

        // Keep coordinates in a sane range for the renderer.
        float sy = clamp(toScreenY(y, h), -10.0f * h, 11.0f * h);
        strip.push_back({sx, sy});
    }
    flush();
}

// Equation box along the top, parse error under it, key help at the bottom.
void GraphPlotter::drawOverlay(SDL_Renderer* renderer, int w, int h) const {
    const float textScale = 2.0f;
    const float charW = CHAR_SIZE * textScale;
    SDL_FRect box = {10, 10, (float)(w - 20), CHAR_SIZE * textScale + 16};

    setColor(renderer, PANEL);
    SDL_RenderFillRect(renderer, &box);
    bool failed = !error().empty();
    setColor(renderer, failed ? TEXT : AXIS);
    SDL_RenderRect(renderer, &box);

    // Show the end of the text if it is wider than the box.
    string text = "> y = ";
    size_t room = (size_t)max(1.0f, (box.w - 20) / charW - (float)text.size() - 1);
    text += input.size() > room ? input.substr(input.size() - room) : input;
    if((SDL_GetTicks() / 500) % 2 == 0) text += '_';

    SDL_SetRenderScale(renderer, textScale, textScale);
    setColor(renderer, TEXT);
    SDL_RenderDebugText(renderer, (box.x + 10) / textScale, (box.y + 8) / textScale, text.c_str());
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);

    if(failed){
        string message = "ERROR: " + error();
        SDL_FRect strip = {box.x, box.y + box.h + 4, CHAR_SIZE * (float)message.size() + 8, CHAR_SIZE + 6.0f};
        setColor(renderer, TEXT);
        SDL_RenderFillRect(renderer, &strip);
        setColor(renderer, BACKGROUND);
        SDL_RenderDebugText(renderer, strip.x + 4, strip.y + 3, message.c_str());
    }

    SDL_FRect helpBar = {0, (float)(h - CHAR_SIZE - 14), (float)w, (float)(CHAR_SIZE + 14)};
    setColor(renderer, PANEL);
    SDL_RenderFillRect(renderer, &helpBar);
    setColor(renderer, LABEL);
    SDL_RenderDebugText(renderer, 10, (float)(h - CHAR_SIZE - 7),
        "Enter: plot   Wheel/PgUp/PgDn: zoom   Drag/Arrows: move   Home: reset   Esc: quit");
}

void GraphPlotter::render(SDL_Renderer* renderer) const {
    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);

    setColor(renderer, BACKGROUND);
    SDL_RenderClear(renderer);

    drawGrid(renderer, w, h);
    drawAxes(renderer, w, h);
    drawCurve(renderer, w, h);
    drawOverlay(renderer, w, h);

    // CRT scanlines: darken every third row.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    setColor(renderer, SCANLINE);
    for(int y = 0; y < h; y += 3) SDL_RenderLine(renderer, 0, (float)y, (float)w, (float)y);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
