#ifndef SHADOW_HUNT_ENEMY_CONTACT_H
#define SHADOW_HUNT_ENEMY_CONTACT_H

#include "enemy.h"
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

struct Melee
{
    float cooldown = 0;
    float effect = 0;
    Vector2 direction{0, 1};

    bool Update(float dt, bool pressed, Vector2 facing)
    {
        cooldown = std::max(0.0f, cooldown - dt);
        effect = std::max(0.0f, effect - dt);
        if (!pressed || cooldown > 0) return false;
        direction = facing;
        cooldown = MeleeCooldown;
        effect = MeleeEffect;
        return true;
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
