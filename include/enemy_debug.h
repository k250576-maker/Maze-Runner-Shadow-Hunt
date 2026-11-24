#ifndef SHADOW_HUNT_ENEMY_DEBUG_H
#define SHADOW_HUNT_ENEMY_DEBUG_H

#ifndef NDEBUG
#include "physical_enemies.h"
#include "game_hud.h"
#include <cstdio>

namespace Enemies
{
inline const char* StateLabel(State state)
{
    switch (state)
    {
        case State::Dormant: return "Dormant";
        case State::Patrolling: return "Patrolling";
        case State::Investigating: return "Investigating";
        case State::Chasing: return "Chasing";
        case State::Returning: return "Returning";
        case State::Frozen: return "Frozen";
        case State::Repelled: return "Repelled";
        case State::WindingUp: return "Wind-up";
        case State::Recovering: return "Recovering";
        case State::Dying: return "Dying";
    }
    return "";
}

inline void DrawDebugRow(const Enemy& enemy, int y)
{
    const char* algorithm = "BFS";
    if (enemy.kind == Kind::Ghost) algorithm = enemy.state == State::Repelled ? "Escape BFS" : "Dijkstra";
    if (enemy.kind == Kind::Sentinel) algorithm = "A*";
    DrawText(TextFormat("%s  |  %s  |  %s", Name(enemy.kind), algorithm, StateLabel(enemy.state)),
             14, y, 15, Hud::Gold);
    DrawText(TextFormat("Path %d   Nodes %d   Searches %d   Updates %u", enemy.route.Remaining(),
                        enemy.route.nodesExplored, enemy.route.searches, enemy.updates),
             14, y + 20, 13, Hud::DimText);
}

inline void DrawDebug(int width, int height, const Manager& enemies, const PhysicalEnemies& physical)
{
    int active = 0;
    for (const auto& actor : physical.Actors()) active += actor.active;
    const float panelHeight = 128.0f + 42 * active;
    const float scale = std::min({width / 660.0f, height / 550.0f, height * 0.68f / (panelHeight + 14)});
    const float hudClearance = 65 * std::min(width / 720.0f, height / 608.0f);
    BeginMode2D({{12 * scale, height - hudClearance - panelHeight * scale}, {0, 0}, 0, scale});
    Hud::DrawStonePanel({0, 0, 354, panelHeight});
    DrawText("AI diagnostics   F3", 14, 13, 16, Hud::Text);
    DrawDebugRow(enemies.Angel(), 41);
    DrawDebugRow(enemies.Ghost(), 83);
    int y = 125;
    for (const auto& actor : physical.Actors())
    {
        if (!actor.active) continue;
        DrawDebugRow(actor.body, y);
        y += 42;
    }
    EndMode2D();
}
}
#endif
#endif
