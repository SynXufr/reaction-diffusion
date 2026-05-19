//
// Created by synx on 3/16/26.
//

#include <algorithm>
#include <iostream>
#include <raylib.h>
#include <lo/lo.h>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <random>
#include <string>
#include <atomic>

// ==== SIMULATION CONSTANTS ====
constexpr int SIM_W = 512;
constexpr int SIM_H = 512;
constexpr int SCALE = 2;
constexpr int WIN_W = SIM_W * SCALE;
constexpr int WIN_H = SIM_H * SCALE;

constexpr float Du = 0.16f;
constexpr float Dv = 0.08f;
constexpr float DT = 1.0f;

constexpr int STEP_AMT = 32;

// === Starting Params ===
float F = 0.04f;
float K = 0.06f;
float baseFNorm = 0.4f;
float baseKNorm = 0.6f;
float maskIntensity = 0.35f;
constexpr float K_BASE = 0.06f;
constexpr float K_AMP = 0.02f;
constexpr float K_FREQ = 0.2f;

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
float simTime = 0.0f;
std::atomic<int> pendingSeeds{0};

constexpr int OSC_PORT = 9000;
constexpr float F_MIN = 0.0f;
constexpr float F_MAX = 0.12f;
constexpr float K_MIN = 0.0f;
constexpr float K_MAX = 0.12f;
constexpr float NORM_MIN = 0.0f;
constexpr float NORM_MAX = 1.0f;
constexpr float OSC_GAIN = 1.25f;
constexpr float FILL_F_GAIN = 0.6f;
constexpr float FILL_K_GAIN = 0.6f;
constexpr int FILL_SAMPLE_STRIDE = 4;
constexpr float FEED_MASK_GAIN = 1.0f;
constexpr float KILL_MASK_GAIN = 1.0f;

static int oscFeedHandler(const char *, const char *, lo_arg **argv, int, lo_message, void *) {
    float value = argv[0]->f;
    baseFNorm = std::clamp(value, NORM_MIN, NORM_MAX);
    return 0;
}

static int oscKillHandler(const char *, const char *, lo_arg **argv, int, lo_message, void *) {
    float value = argv[0]->f;
    baseKNorm = std::clamp(value, NORM_MIN, NORM_MAX);
    return 0;
}

static int oscSeedHandler(const char *, const char *, lo_arg **argv, int argc, lo_message, void *) {
    int count = 1;
    if (argc > 0) {
        count = std::clamp(argv[0]->i, 1, 10);
    }
    pendingSeeds.fetch_add(count, std::memory_order_relaxed);
    return 0;
}

static int oscMaskHandler(const char *, const char *, lo_arg **argv, int, lo_message, void *) {
    float value = argv[0]->f;
    maskIntensity = std::clamp(value, NORM_MIN, NORM_MAX);
    return 0;
}

// === FUNCTIONS ===

/// Turns 2D coordinates into a 1D index into flat vector
/// @param g 1D Vector of Cells
/// @param x x-index of cell
/// @param y y-index of cell
/// @return Cell reference -> allows read/write to cell
inline Cell &at(std::vector<Cell> &g, int x, int y) {
    return g[y * SIM_W + x];
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float feedMask(int x, int y) {
    float cx = (SIM_W - 1) * 0.5f;
    float cy = (SIM_H - 1) * 0.5f;
    float dx = static_cast<float>(x) - cx;
    float dy = static_cast<float>(y) - cy;
    float dist = std::sqrt(dx * dx + dy * dy);
    float maxDist = std::sqrt(cx * cx + cy * cy);
    if (maxDist <= 0.0f) {
        return 0.0f;
    }
    float t = std::clamp(1.0f - dist / maxDist, 0.0f, 1.0f);
    return t;
}

float computeFill() {
    float sum = 0.0f;
    int count = 0;
    for (int y = 0; y < SIM_H; y += FILL_SAMPLE_STRIDE) {
        for (int x = 0; x < SIM_W; x += FILL_SAMPLE_STRIDE) {
            sum += at(gridA, x, y).v;
            count++;
        }
    }
    if (count == 0) {
        return 0.0f;
    }
    return std::clamp(sum / static_cast<float>(count), 0.0f, 1.0f);
}

void spawnRandomSeed(std::mt19937 &rng) {
    std::uniform_int_distribution<int> distX(1, SIM_W - 2);
    std::uniform_int_distribution<int> distY(1, SIM_H - 2);
    int mx = distX(rng);
    int my = distY(rng);
    int r = 6;
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy > r2) {
                continue;
            }
            int sx = mx + dx, sy = my + dy;
            if (sx > 0 && sx < SIM_W - 1 && sy > 0 && sy < SIM_H - 1) {
                at(gridA, sx, sy).v = 1.0f;
                at(gridA, sx, sy).u = 0.0f;
            }
        }
    }
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
            float mask = feedMask(x, y);
            float maskCentered = (mask - 0.5f) * 2.0f;
            float feed = std::clamp(F * (1.0f + FEED_MASK_GAIN * maskCentered * maskIntensity), F_MIN, F_MAX);
            float kill = std::clamp(K * (1.0f + KILL_MASK_GAIN * maskCentered * maskIntensity), K_MIN, K_MAX);

            at(gridB, x, y).u = u + DT * (Du * lapU - uvv + feed * (1.0f - u));
            at(gridB, x, y).v = v + DT * (Dv * lapV + uvv - v * (feed + kill));

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

    std::mt19937 rng(std::random_device{}());

    std::string oscPort = std::to_string(OSC_PORT);
    lo_server_thread oscServer = lo_server_thread_new(oscPort.c_str(), nullptr);
    lo_server_thread_add_method(oscServer, "/rd/feed", "f", oscFeedHandler, nullptr);
    lo_server_thread_add_method(oscServer, "/rd/kill", "f", oscKillHandler, nullptr);
    lo_server_thread_add_method(oscServer, "/rd/seed", "i", oscSeedHandler, nullptr);
    lo_server_thread_add_method(oscServer, "/rd/mask", "f", oscMaskHandler, nullptr);
    lo_server_thread_start(oscServer);

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

        int seedsToSpawn = pendingSeeds.exchange(0, std::memory_order_relaxed);
        for (int i = 0; i < seedsToSpawn; i++) {
            spawnRandomSeed(rng);
        }

        // --- Parameter tweaking ---
        float step = 0.01f;
        if (IsKeyDown(KEY_UP)) baseFNorm += step;
        if (IsKeyDown(KEY_DOWN)) baseFNorm -= step;
        if (IsKeyDown(KEY_RIGHT)) baseKNorm += step;
        if (IsKeyDown(KEY_LEFT)) baseKNorm -= step;
        baseFNorm = std::clamp(baseFNorm, NORM_MIN, NORM_MAX);
        baseKNorm = std::clamp(baseKNorm, NORM_MIN, NORM_MAX);

        float fill = computeFill();
        float fNorm = std::clamp(baseFNorm * OSC_GAIN + fill * FILL_F_GAIN, NORM_MIN, NORM_MAX);
        float kNorm = std::clamp(baseKNorm * OSC_GAIN + fill * FILL_K_GAIN, NORM_MIN, NORM_MAX);
        F = lerp(F_MIN, F_MAX, fNorm);
        K = lerp(K_MIN, K_MAX, kNorm);

        // run multiple steps per frame for speed
        for (int i = 0; i < STEP_AMT; i++) {
            simTime += DT;
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

    lo_server_thread_free(oscServer);
}
