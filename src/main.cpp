//
// Created by synx on 3/16/26.
//

#include <algorithm>
#include <iostream>
#include <raylib.h>
#include <vector>
#include <cstdlib>
#include <random>

// ==== SIMULATION CONSTANTS ====
constexpr int SIM_W = 512;
constexpr int SIM_H = 512;
constexpr int SCALE = 2;
constexpr int WIN_W = SIM_W * SCALE;
constexpr int WIN_H = SIM_H * SCALE;

constexpr float Du = 0.16f;
constexpr float Dv = 0.08f;
constexpr float DT = 1.0f;

constexpr int STEP_AMT = 8;

// === Starting Params ===
float F = 0.04f;
float K = 0.06f;

// === nice combinations ===
// Standard: F = 0.035 K = 0.065
// Flower: F = 0.04 K = 0.06

// ==== STRUCTS ====
struct Cell {
    float u, v;
};

// initialize 2 grids to read from one grid
// write to the other grid, then call std::swap
std::vector<Cell> gridA(SIM_W * SIM_H);
std::vector<Cell> gridB(SIM_W * SIM_H);

// === FUNCTIONS ===

/// Turns 2D coordinates into a 1D index into flat vector
/// @param g 1D Vector of Cells
/// @param x x-index of cell
/// @param y y-index of cell
/// @return Cell reference -> allows read/write to cell
inline Cell &at(std::vector<Cell> &g, int x, int y) {
    return g[y * SIM_W + x];
}

void initGrid() {
    // initialize grid with only U, no V
    // for (auto &c: gridA) { c.u = 1.0, c.v = 0; };


    // initialize grid with slightly random values
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 0.02f);

    for (auto &c: gridA) {
        c.u = 1.0f + dist(rng);
        c.v = dist(rng);
    }

    // set a small portion of V in the center
    // to kick off the reaction with radius 16
    int cx = SIM_W / 2, cy = SIM_H / 2, r = 16;
    for (int y = cy - r; y < cy + r; y++) {
        for (int x = cx - r; x < cx + r; x++) {
            at(gridA, x, y).u = 0.5f;
            at(gridA, x, y).v = 0.25f;
        }
    }
}

void stepSimulation() {
#pragma omp parallel for schedule(static)
    for (int y = 1; y < SIM_H - 1; y++) {
        for (int x = 1; x < SIM_W - 1; x++) {
            float u = at(gridA, x, y).u;
            float v = at(gridA, x, y).v;

            float lapU = at(gridA, x - 1, y).u + at(gridA, x + 1, y).u
                         + at(gridA, x, y - 1).u + at(gridA, x, y + 1).u
                         - 4 * at(gridA, x, y).u;

            float lapV = at(gridA, x - 1, y).v + at(gridA, x + 1, y).v
                         + at(gridA, x, y - 1).v + at(gridA, x, y + 1).v
                         - 4 * at(gridA, x, y).v;

            float uvv = u * v * v;

            at(gridB, x, y).u = u + DT * (Du * lapU - uvv + F * (1.0f - u));
            at(gridB, x, y).v = v + DT * (Dv * lapV + uvv - v * (F + K));

            at(gridB, x, y).u = std::clamp(at(gridB, x, y).u, 0.0f, 1.0f);
            at(gridB, x, y).v = std::clamp(at(gridB, x, y).v, 0.0f, 1.0f);
        }
    }
    std::swap(gridA, gridB);
}

/// Map v in [0,1] to a color (simple grayscale -- later palette)
Color valueToColor(float v) {
    Color a = {90, 0, 0, 255};
    Color b = {255, 255, 255, 255};

    return {
        static_cast<unsigned char>(a.r + v * (b.r - a.r)),
        static_cast<unsigned char>(a.g + v * (b.g - a.g)),
        static_cast<unsigned char>(a.b + v * (b.b - a.b)),
        255
    };
}

int main() {
    InitWindow(WIN_W, WIN_H, "Reaction-Diffusion");
    SetTargetFPS(60);

    initGrid();

    Image img = GenImageColor(SIM_W, SIM_H, BLACK);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    while (!WindowShouldClose()) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            // convert screen coords back to sim coords
            int mx = static_cast<int>(mouse.x / SCALE);
            int my = static_cast<int>(mouse.y / SCALE);
            int r = 5;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    int sx = mx + dx, sy = my + dy;
                    if (sx > 0 && sx < SIM_W - 1 && sy > 0 && sy < SIM_H - 1) {
                        at(gridA, sx, sy).v = 1.0f;
                        at(gridA, sx, sy).u = 0.0f;
                    }
                }
            }
        }

        // --- Parameter tweaking ---
        float step = 0.0001f;
        if (IsKeyDown(KEY_UP)) F += step;
        if (IsKeyDown(KEY_DOWN)) F -= step;
        if (IsKeyDown(KEY_RIGHT)) K += step;
        if (IsKeyDown(KEY_LEFT)) K -= step;

        // run multiple steps per frame for speed
        for (int i = 0; i < STEP_AMT; i++) {
            stepSimulation();
        }

        std::vector<Color> pixels(SIM_W * SIM_H);
        for (int y = 0; y < SIM_H; y++)
            for (int x = 0; x < SIM_W; x++)
                pixels[y * SIM_W + x] = valueToColor(at(gridA, x, y).v);

        UpdateTexture(tex, pixels.data());

        BeginDrawing();
        ClearBackground(BLACK);
        // Scale up to window size
        DrawTexturePro(
            tex,
            {0, 0, static_cast<float>(SIM_W), static_cast<float>(SIM_H)},
            {0, 0, static_cast<float>(WIN_W), static_cast<float>(WIN_H)},
            {0, 0}, 0.0f, WHITE
        );
        char buf[64];
        snprintf(buf, sizeof(buf), "F: %.4f  K: %.4f", F, K);
        DrawText(buf, 10, 30, 20, WHITE);
        DrawFPS(10, 10);
        EndDrawing();
    }
}
