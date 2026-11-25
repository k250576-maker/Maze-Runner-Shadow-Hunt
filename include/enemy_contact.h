#ifndef SHADOW_HUNT_ENEMY_CONTACT_H
#define SHADOW_HUNT_ENEMY_CONTACT_H

#include "enemy.h"
#include "maze_visibility.h"
#include "player_movement.h"
#include <algorithm>

namespace Enemies
{
constexpr int StartingHealth = 5;
constexpr float HitCooldown = 1.5f;

constexpr float AngelWarning = 1.0f;
constexpr float GhostWarning = 0.46f;
constexpr float GhostPulse = 0.25f;
constexpr float HitFlash = 0.34f;
constexpr float MeleeCooldown = 0.45f;
constexpr float MeleeEffect = 0.19f;
constexpr float MeleeInputBuffer = 0.10f;
constexpr float MeleeActiveTime = 0.12f;
constexpr float MeleeReach = 38.0f;
constexpr float MeleeHalfArc = 0.91f;

struct Health
{
    int points = StartingHealth;
    float invulnerable = 0;
    float flash = 0;
    Kind lastHit = Kind::Angel;

    bool Dead() const { return points == 0; }
    void Update(float dt)
    {
        invulnerable = std::max(0.0f, invulnerable - dt);
        flash = std::max(0.0f, flash - dt);
    }
    void Hit(Kind enemy)
    {
        if (Dead()) return;
        if (enemy == Kind::Angel) points = 0;
        else
        {
            if (invulnerable > 0) return;
            --points;
            invulnerable = HitCooldown;
        }
        lastHit = enemy;
        flash = HitFlash;
    }
};

inline bool InAttackArc(Vector2 from, Vector2 facing, Vector2 target, float range, float halfAngle)
{
    const float distance = DistanceSquared(from, target);
    const float forward = (target.x - from.x) * facing.x + (target.y - from.y) * facing.y;
    return distance <= range * range && forward >= std::sqrt(distance) * std::cos(halfAngle);
}

struct MeleeArc
{
    float first;
    float last;
};

inline MeleeArc SwingArc(float age)
{
    const float progress = std::clamp(age / MeleeEffect, 0.0f, 1.0f);
    return {(-1.0f + 1.85f * progress) * MeleeHalfArc,
            std::min(1.0f, 0.24f + 2.6f * progress) * MeleeHalfArc};
}

inline float SwingReach(const Maze::Layout& level, Vector2 origin, Vector2 ray)
{
    return std::clamp(Maze::FirstWallDistance(level, origin, ray, MeleeReach) - 1, 0.0f, MeleeReach);
}

struct MeleeFrame
{
    bool started = false;
    bool active = false;
    Vector2 from{}, to{}, direction{};
    float firstAge = 0, lastAge = 0, motionFraction = 0;
};

inline bool MeleeTouches(const Maze::Layout& level, Vector2 origin, Vector2 facing, float age,
                         Vector2 center, float radius)
{
    const Vector2 delta{center.x - origin.x, center.y - origin.y};
    const float distanceSquared = DistanceSquared(origin, center);
    if (distanceSquared > (MeleeReach + radius) * (MeleeReach + radius)) return false;
    const auto arc = SwingArc(age);
    const float angle = std::atan2(facing.x * delta.y - facing.y * delta.x,
                                   facing.x * delta.x + facing.y * delta.y);
    const float offset = std::clamp(angle, arc.first, arc.last);
    const float cosOffset = std::cos(offset), sinOffset = std::sin(offset);
    const Vector2 ray{facing.x * cosOffset - facing.y * sinOffset,
                      facing.y * cosOffset + facing.x * sinOffset};
    const float forward = delta.x * ray.x + delta.y * ray.y;
    // Clamp to the nearest point on the visible, wall-clipped sector.
    const float reach = std::clamp(forward, 0.0f, SwingReach(level, origin, ray));
    const Vector2 contact{origin.x + ray.x * reach, origin.y + ray.y * reach};
    return DistanceSquared(center, contact) <= radius * radius
        && Maze::HasLineOfSight(level, origin, center);
}

inline bool SweptMeleeTouches(const Maze::Layout& level, const MeleeFrame& frame,
                              Vector2 before, Vector2 after, float radius)
{
    const Vector2 end = PointAlong(before, after, frame.motionFraction);
    const Vector2 relativeStep{(end.x - before.x) - (frame.to.x - frame.from.x),
                               (end.y - before.y) - (frame.to.y - frame.from.y)};
    // Sample relative motion and angular travel in small world-space steps.
    // This catches bodies that enter and leave the slash between rendered frames.
    const float angularTravel = (frame.lastAge - frame.firstAge) / MeleeEffect * (2.6f * MeleeHalfArc * MeleeReach);
    const int steps = std::max(1, static_cast<int>(std::ceil(
        (std::hypot(relativeStep.x, relativeStep.y) + angularTravel) / 2.0f)));
    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / steps;
        if (MeleeTouches(level, PointAlong(frame.from, frame.to, t), frame.direction,
                         frame.firstAge + (frame.lastAge - frame.firstAge) * t,
                         PointAlong(before, end, t), radius)) return true;
    }
    return false;
}

struct Melee
{
    float cooldown = 0;
    float effect = 0;
    bool buffered = false;
    Vector2 direction{0, 1};

    MeleeFrame Update(float dt, bool pressed, Vector2 facing, Vector2 previous, Vector2 player,
                      bool enabled = true)
    {
        const float firstAge = MeleeEffect - effect;
        cooldown = std::max(0.0f, cooldown - dt);
        effect = std::max(0.0f, effect - dt);
        if (!enabled)
        {
            buffered = false;
            effect = 0;
            return {};
        }
        if (pressed && cooldown <= MeleeInputBuffer) buffered = true;
        if (buffered && cooldown == 0)
        {
            buffered = false;
            direction = facing;
            cooldown = MeleeCooldown;
            effect = MeleeEffect;
            return {true, true, player, player, direction, 0, 0, 1};
        }
        if (firstAge >= MeleeActiveTime) return {};
        const float elapsed = std::min(dt, MeleeActiveTime - firstAge);
        const float fraction = dt > 0 ? elapsed / dt : 0;
        return {false, true, previous, PointAlong(previous, player, fraction), direction,
                firstAge, firstAge + elapsed, fraction};
    }
};

// One movement segment per actor per update. Check their relative sweep so
// fast crossing or a long frame cannot skip over a contact.
inline float ContactFraction(Vector2 oldPlayer, Vector2 player, Vector2 oldEnemy, const Enemy& enemy)
{
    const Vector2 from{oldPlayer.x - oldEnemy.x, oldPlayer.y - oldEnemy.y};
    const Vector2 to{player.x - enemy.position.x, player.y - enemy.position.y};
    const float t = ClosestFraction(from, to, {0, 0});
    const float reach = Movement::CollisionHalfSize + enemy.radius;
    return DistanceSquared(PointAlong(from, to, t), {0, 0}) <= reach * reach ? t : -1.0f;
}
}
#endif
