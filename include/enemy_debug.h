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

inline void DrawCombatDebug(const Maze::Layout& level, Rectangle visible, Vector2 player,
                            Vector2 playerFacing, const Melee& attack, const PhysicalEnemies& physical)
{
    const float age = MeleeEffect - attack.effect;
    const bool live = attack.effect > 0 && age < MeleeActiveTime;
    const auto arc = live ? SwingArc(age) : MeleeArc{-MeleeHalfArc, MeleeHalfArc};
    const auto direction = live ? attack.direction : playerFacing;
    const float facing = std::atan2(direction.y, direction.x);
    const Color blade = live ? LIME : Color{119, 154, 172, 150};
    constexpr int segments = 24;
    Vector2 previous = player;
    for (int i = 0; i <= segments; ++i)
    {
        const float angle = facing + arc.first + (arc.last - arc.first) * i / segments;
        const Vector2 ray{std::cos(angle), std::sin(angle)};
        const float reach = SwingReach(level, player, ray);
        const Vector2 tip{player.x + reach * ray.x, player.y + reach * ray.y};
        DrawLineV(previous, tip, blade);
        previous = tip;
    }
    DrawLineV(previous, player, blade);
    for (const auto& actor : physical.Actors())
    {
        const auto& body = actor.body;
        if (!actor.active || !CheckCollisionPointRec(body.position, visible)
            || !Maze::HasLineOfSight(level, player, body.position)) continue;
        DrawCircleLinesV(body.position, body.radius, actor.struck ? LIME : ORANGE);
        const auto& stats = Stats(body.kind);
        DrawCircleLinesV(body.position, stats.reach, Color{248, 153, 137, 100});
        if (body.state == State::WindingUp)
        {
            const float heading = std::atan2(body.facing.y, body.facing.x) * RAD2DEG;
            const float half = PhysicalAttackHalfArc * RAD2DEG;
            const float reach = stats.reach + PhysicalAttackTolerance;
            DrawRingLines(body.position, reach - 1, reach, heading - half, heading + half, 24, RED);
        }
    }
}

inline void DrawDebug(int width, int height, const Manager& enemies, const PhysicalEnemies& physical,
                      const Melee& attack)
{
    int active = 0;
    for (const auto& actor : physical.Actors()) active += actor.active;
    const float panelHeight = 150.0f + 42 * active;
    const float scale = std::min({width / 660.0f, height / 550.0f, height * 0.68f / (panelHeight + 14)});
    const float hudClearance = 65 * std::min(width / 720.0f, height / 608.0f);
    BeginMode2D({{12 * scale, height - hudClearance - panelHeight * scale}, {0, 0}, 0, scale});
    Hud::DrawStonePanel({0, 0, 354, panelHeight});
    DrawText("AI diagnostics   F3", 14, 13, 16, Hud::Text);
    DrawText(TextFormat("Melee %.2fs   Buffer %s", attack.cooldown, attack.buffered ? "yes" : "no"),
             14, 38, 13, Hud::DimText);
    DrawDebugRow(enemies.Angel(), 61);
    DrawDebugRow(enemies.Ghost(), 103);
    int y = 145;
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
