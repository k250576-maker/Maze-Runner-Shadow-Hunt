#include "raylib.h"

int main()
{
    InitWindow(800, 450, "Maze Runner - Test");

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Raylib is working!", 220, 200, 30, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}