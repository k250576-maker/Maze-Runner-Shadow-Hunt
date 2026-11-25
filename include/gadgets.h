#ifndef SHADOW_HUNT_GADGETS_H
#define SHADOW_HUNT_GADGETS_H

#include "dungeon_exploration.h"
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace Gadgets
{
enum class Type { SmokeBomb, SoulLure };
constexpr std::array<Type, 2> Types{Type::SmokeBomb, Type::SoulLure};
constexpr int Index(Type type) { return static_cast<int>(type); }
constexpr int SmokePickupCount = 5;
constexpr int LurePickupCount = 4;
constexpr int CarryLimit = 3;
constexpr float SmokeSeconds = 3.0f;
constexpr float LureSeconds = 5.0f;
constexpr float SmokeRadius = 1.65f * Maze::CellSize;
constexpr float LureRadius = 7.0f * Maze::CellSize;
constexpr float PickupRange = 20.0f;
constexpr float FeedbackSeconds = 1.45f;

inline const char* Name(Type type) { return type == Type::SmokeBomb ? "Smoke Bomb" : "Soul Lure"; }

class Inventory
{
    struct Node
    {
        Type type;
        int quantity;
        Node* next;
    };

public:
    Inventory() = default;
    Inventory(const Inventory&) = delete;
    Inventory& operator=(const Inventory&) = delete;
    ~Inventory() { Clear(); }

    int Quantity(Type type) const
    {
        const Node* item = Find(type);
        return item ? item->quantity : 0;
    }

    bool Add(Type type)
    {
        if (Node* item = Find(type))
        {
            if (item->quantity >= CarryLimit) return false;
            ++item->quantity;
        }
        else head = new Node{type, 1, head};
        return true;
    }

    bool Consume(Type type)
    {
        Node** link = &head;
        while (*link && (*link)->type != type) link = &(*link)->next;
        if (!*link) return false;
        if (--(*link)->quantity == 0)
        {
            Node* empty = *link;
            *link = empty->next;
            delete empty;
        }
        return true;
    }

    void Clear()
    {
        while (head)
        {
            Node* next = head->next;
            delete head;
            head = next;
        }
    }

private:
    Node* Find(Type type) const
    {
        for (Node* node = head; node; node = node->next)
            if (node->type == type) return node;
        return nullptr;
    }

    Node* head = nullptr;
};

struct Pickup
{
    int cell = Enemies::NoCell;
    Type type{};
    bool collected = false;
};

struct Effect
{
    Vector2 position{};
    float remaining = 0;
};

enum class Message { None, Collected, Deployed, Empty };

struct Feedback
{
    Type type{};
    Message message = Message::None;
    float remaining = 0;
};

class Run
{
public:
    void Place(const Maze::GeneratedLevel& generated, const Dungeon::Exploration& dungeon, std::uint32_t seed,
               const std::array<int, 2>& enemyHomes = {Enemies::NoCell, Enemies::NoCell})
    {
        const auto& level = generated.layout;
        pickups = {};
        inventory.Clear();
        smoke = {};
        lures = {};
        feedback = {};

        // Measure against the future open layout so locked chambers can reward
        // exploration. Gates themselves are never pickup candidates.
        Maze::Layout open = level;
        for (auto& row : open.locked) std::fill(std::begin(row), std::end(row), false);
        const auto distances = Maze::MeasureDistances(open);
        std::array<bool, Enemies::CellCount> occupied{};
        const auto reserve = [&](Maze::Cell cell) { occupied[Enemies::CellId(cell)] = true; };
        reserve(level.spawn);
        reserve(level.futureExit);
        for (const auto& key : dungeon.keys) reserve(key.cell);
        for (int i = 0; i < dungeon.ChestSize(); ++i) reserve(dungeon.chests[i].cell);
        for (int i = 0; i < dungeon.BarrelSize(); ++i) reserve(dungeon.barrels[i].cell);
        for (int i = 0; i < level.torchCount; ++i) reserve(level.torchSites[i]);
        for (int cell : enemyHomes) if (cell != Enemies::NoCell) occupied[cell] = true;

        std::array<bool, Enemies::CellCount> deadEnds{};
        for (int r = 0; r < Maze::LogicalRows; ++r)
        for (int c = 0; c < Maze::LogicalColumns; ++c)
        {
            int exits = 0;
            for (int d = 0; d < 4; ++d) exits += generated.graph.Has({c, r}, d);
            if (exits != 1) continue;
            const auto x = generated.columns[c], y = generated.rows[r];
            for (int row = y.first; row < y.first + y.width; ++row)
                for (int column = x.first; column < x.first + x.width; ++column)
                    deadEnds[Enemies::CellId({column, row})] = true;
        }

        enum class Region { Passage, Chamber, DeadEnd };
        struct Candidate { int cell, distance, variation; Region region; int separation = 22; };
        std::vector<Candidate> candidates;
        int farthest = 18;
        std::uint32_t random = seed ^ 0x94BE2F31u;
        for (int id = 0; id < Enemies::CellCount; ++id)
        {
            const auto cell = Enemies::CellAt(id);
            const int distance = distances.value[cell.row][cell.column];
            if (occupied[id] || distance < 18 || Enemies::InEntrance(level, cell)) continue;
            bool gate = false, chamber = false;
            for (const auto& room : level.rooms)
            {
                gate |= Dungeon::Exploration::In(room.doorway, cell);
                chamber |= Dungeon::Exploration::In(room.bounds, cell);
            }
            if (gate) continue;
            random = random * 1664525u + 1013904223u;
            candidates.push_back({id, distance, static_cast<int>((random >> 16) % 13),
                chamber ? Region::Chamber : deadEnds[id] ? Region::DeadEnd : Region::Passage});
            farthest = std::max(farthest, distance);
        }

        int placed = 0;
        for (int i = 0; i < std::max(SmokePickupCount, LurePickupCount); ++i)
        for (Type type : Types)
        {
            if (i >= (type == Type::SmokeBomb ? SmokePickupCount : LurePickupCount)) continue;
            const Region preferred = i == 0 ? Region::Chamber : i == 1 ? Region::DeadEnd : Region::Passage;
            const int target = 18 + (farthest - 18) * (placed + 1) / (static_cast<int>(pickups.size()) + 1);
            int best = Enemies::NoCell, bestScore = std::numeric_limits<int>::min();
            for (const auto& candidate : candidates)
            {
                if (occupied[candidate.cell]) continue;
                const int score = (candidate.region == preferred ? 3 * Enemies::CellCount : 0)
                    + 8 * candidate.separation - 2 * std::abs(candidate.distance - target) + candidate.variation;
                if (score > bestScore) { best = candidate.cell; bestScore = score; }
            }
            if (best == Enemies::NoCell) continue;
            occupied[best] = true;
            pickups[placed++] = {best, type, false};
            const auto picked = Enemies::CellAt(best);
            for (auto& candidate : candidates)
            {
                const auto cell = Enemies::CellAt(candidate.cell);
                const int distance = std::abs(cell.column - picked.column) + std::abs(cell.row - picked.row);
                candidate.separation = std::min(candidate.separation, distance);
            }
        }
    }

    void Update(float dt)
    {
        for (auto& effect : smoke) effect.remaining = std::max(0.0f, effect.remaining - dt);
        for (auto& effect : lures) effect.remaining = std::max(0.0f, effect.remaining - dt);
        feedback.remaining = std::max(0.0f, feedback.remaining - dt);
    }

    bool Collect(const Maze::Layout& level, Vector2 player)
    {
        bool collected = false;
        for (auto& pickup : pickups)
        {
            if (pickup.cell == Enemies::NoCell || pickup.collected) continue;
            const auto position = Enemies::Center(pickup.cell);
            if (Enemies::DistanceSquared(position, player) > PickupRange * PickupRange
                || !Maze::HasLineOfSight(level, player, position) || !inventory.Add(pickup.type)) continue;
            pickup.collected = collected = true;
            feedback = {pickup.type, Message::Collected, FeedbackSeconds};
        }
        return collected;
    }

    bool Deploy(Type type, const Maze::Layout& level, Vector2 player)
    {
        if (!Maze::IsWalkablePosition(level, player)) return false;
        if (inventory.Quantity(type) == 0)
        {
            feedback = {type, Message::Empty, FeedbackSeconds};
            return false;
        }
        const bool smokeBomb = type == Type::SmokeBomb;
        Effect* effects = smokeBomb ? smoke.data() : lures.data();
        const int count = smokeBomb ? SmokePickupCount : LurePickupCount;
        for (int i = 0; i < count; ++i)
        {
            if (effects[i].remaining > 0) continue;
            inventory.Consume(type);
            effects[i] = {player, smokeBomb ? SmokeSeconds : LureSeconds};
            feedback = {type, Message::Deployed, FeedbackSeconds};
            return true;
        }
        return false;
    }

    bool Obscures(const Maze::Layout& level, Vector2 player) const
    {
        for (const auto& effect : smoke)
            if (effect.remaining > 0 && Enemies::DistanceSquared(effect.position, player) <= SmokeRadius * SmokeRadius
                && Maze::HasLineOfSight(level, effect.position, player)) return true;
        return false;
    }

    int Quantity(Type type) const { return inventory.Quantity(type); }
    const auto& Pickups() const { return pickups; }
    const auto& Smoke() const { return smoke; }
    const auto& Lures() const { return lures; }
    const Feedback& LastFeedback() const { return feedback; }

private:
    Inventory inventory;
    std::array<Pickup, SmokePickupCount + LurePickupCount> pickups{};
    std::array<Effect, SmokePickupCount> smoke{};
    std::array<Effect, LurePickupCount> lures{};
    Feedback feedback{};
};
}
#endif
