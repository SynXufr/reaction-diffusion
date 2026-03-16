//
// Created by synx on 3/16/26.
//

#include <iostream>
#include <raylib.h>
#include <vector>
#include <cmath>

// ==== SIMULATION CONSTANTS ====
constexpr int SIM_W = 256;
constexpr int SIM_H = 256;
constexpr int SCALE = 3;
constexpr int WIN_W = SIM_W * SCALE;
constexpr int WIN_H = SIM_H * SCALE;

// ==== STRUCTS ====
struct Cell { float u, v; };

std::vector<Cell> gridA(SIM_W * SIM_H);
std::vector<Cell> gridB(SIM_W * SIM_H);

/// Turns 2D coordinates into a 1D index into flat vector
/// @param g 1D Vector of Cells
/// @param x x-index of cell
/// @param y y-index of cell
/// @return Cell reference -> allows read/write to cell
inline Cell& at(std::vector<Cell>& g, int x, int y) {
   return g[y * SIM_W + x];
}

/// Map v in [0,1] to a color (simple grayscale -- later palette)
Color valueToColor(float v) {
   auto c = static_cast<unsigned char>(v * 255.0f);
   return { c, c, c, 255 };
}

int main() {
   InitWindow(WIN_W, WIN_H, "Reaction-Diffusion");
   SetTargetFPS(60);

   // TODO: Init Grid

   Image img = GenImageColor(SIM_W, SIM_H, BLACK);
   Texture2D tex = LoadTextureFromImage(img);
   UnloadImage(img);

   std::vector<Color> pixels(SIM_W * SIM_H);

   while (!WindowShouldClose()) {
      // TODO: Step Simulation

      // Write grid values into pixel buffer and upload to texture
      for (int y = 0; y < SIM_H; y++) {
         for (int x = 0; x < SIM_W; x++) {
            auto c = static_cast<unsigned char>(at(gridA, x, y).v * 255.0f);
            pixels[y * SIM_W + x] = { c, c, c, 255 };
         }
      }

      UpdateTexture(tex, pixels.data());

      BeginDrawing();
      DrawTexturePro(
          tex,
          { 0, 0, static_cast<float>(SIM_W), static_cast<float>(SIM_H) },
          { 0, 0, static_cast<float>(WIN_W),  static_cast<float>(WIN_H) },
          { 0, 0 }, 0.0f, WHITE
      );
      DrawFPS(10, 10);
      EndDrawing();
   }
}