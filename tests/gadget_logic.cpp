#include "../include/physical_enemies.h"
#include <cassert>
#include <cstdio>
#include <type_traits>

using namespace Enemies;
using Gadgets::Type;

void Inventory()
{
    static_assert(!std::is_copy_constructible_v<Gadgets::Inventory>);
    static_assert(!std::is_copy_assignable_v<Gadgets::Inventory>);
    Gadgets::Inventory inventory;
    assert(!inventory.Consume(Type::SmokeBomb));
    for (int i = 0; i < Gadgets::CarryLimit; ++i) assert(inventory.Add(Type::SmokeBomb));
    assert(!inventory.Add(Type::SmokeBomb));
    assert(inventory.Add(Type::SoulLure));
    for (int i = 0; i < Gadgets::CarryLimit; ++i) assert(inventory.Consume(Type::SmokeBomb));
    assert(inventory.Quantity(Type::SmokeBomb) == 0 && inventory.Quantity(Type::SoulLure) == 1);
    assert(!inventory.Consume(Type::SmokeBomb) && inventory.Consume(Type::SoulLure));
    assert(inventory.Add(Type::SmokeBomb) && inventory.Add(Type::SoulLure));
    inventory.Clear();
    assert(inventory.Quantity(Type::SmokeBomb) == 0 && inventory.Quantity(Type::SoulLure) == 0);
}

void Placement()
{
    for (unsigned i = 0; i < 64; ++i)
    {
        const unsigned seed = i == 63 ? 0xffffffffu : i * 2654435761u;
        Maze::GeneratedLevel generated;
        assert(Maze::Generate(seed, generated));
        auto& level = generated.layout;
        Dungeon::Exploration dungeon(level);
        assert(dungeon.Initialize(seed));
        Gadgets::Run first, second;
        first.Place(generated, dungeon, seed);
        second.Place(generated, dungeon, seed);
        int chambers = 0, deadEnds = 0, counts[2]{};
        std::array<bool, CellCount> occupied{};
        for (std::size_t p = 0; p < first.Pickups().size(); ++p)
        {
            const auto& pickup = first.Pickups()[p];
            assert(pickup.cell != NoCell && !pickup.collected);
            assert(pickup.cell == second.Pickups()[p].cell && pickup.type == second.Pickups()[p].type);
            const auto cell = CellAt(pickup.cell);
            assert(level.floor[cell.row][cell.column] && !level.locked[cell.row][cell.column] && !InEntrance(level, cell));
            assert(pickup.cell != CellId(level.spawn) && pickup.cell != CellId(level.futureExit));
            assert(!occupied[pickup.cell]);
            occupied[pickup.cell] = true;
            ++counts[Gadgets::Index(pickup.type)];
            for (const auto& room : level.rooms)
            {
                assert(!Dungeon::Exploration::In(room.doorway, cell));
                chambers += Dungeon::Exploration::In(room.bounds, cell);
            }
            for (int r = 0; r < Maze::LogicalRows; ++r)
            for (int c = 0; c < Maze::LogicalColumns; ++c)
            {
                const auto x = generated.columns[c], y = generated.rows[r];
                if (cell.column < x.first || cell.column >= x.first + x.width ||
                    cell.row < y.first || cell.row >= y.first + y.width) continue;
                int exits = 0;
                for (int d = 0; d < 4; ++d) exits += generated.graph.Has({c, r}, d);
                deadEnds += exits == 1;
            }
            for (const auto& key : dungeon.keys) assert(pickup.cell != CellId(key.cell));
            for (int k = 0; k < dungeon.ChestSize(); ++k) assert(pickup.cell != CellId(dungeon.chests[k].cell));
            for (int k = 0; k < dungeon.BarrelSize(); ++k) assert(pickup.cell != CellId(dungeon.barrels[k].cell));
            for (int k = 0; k < level.torchCount; ++k) assert(pickup.cell != CellId(level.torchSites[k]));
        }
        assert(counts[0] == Gadgets::SmokePickupCount && counts[1] == Gadgets::LurePickupCount);
        assert(chambers >= 2 && deadEnds >= 2);
        assert(dungeon.ValidateProgression());
    }
}

void Charge(Gadgets::Run& gadgets)
{
    Maze::GeneratedLevel generated;
    assert(Maze::Generate(42, generated));
    auto& level = generated.layout;
    Dungeon::Exploration dungeon(level);
    assert(dungeon.Initialize(42));
    gadgets.Place(generated, dungeon, 42);
    int smoke = 0, lures = 0;
    for (const auto& pickup : gadgets.Pickups())
    {
        const auto center = Center(pickup.cell);
        const int count = pickup.type == Type::SmokeBomb ? ++smoke : ++lures;
        assert(gadgets.Collect(level, center) == (count <= Gadgets::CarryLimit));
        assert(pickup.collected == (count <= Gadgets::CarryLimit));
    }
    const auto remaining = std::find_if(gadgets.Pickups().begin(), gadgets.Pickups().end(),
        [](const Gadgets::Pickup& pickup) { return !pickup.collected && pickup.type == Type::SmokeBomb; });
    assert(remaining != gadgets.Pickups().end());
    const auto position = Center(remaining->cell);
    assert(gadgets.Deploy(Type::SmokeBomb, level, position) && gadgets.Collect(level, position));
    assert(gadgets.Quantity(Type::SmokeBomb) == Gadgets::CarryLimit);
    gadgets.Update(Gadgets::SmokeSeconds + 0.1f);
}

struct Arena
{
    Maze::Layout level;
    Lighting::DungeonLight light{level};
    Exploration::Map map{level};
    PhysicalEnemies enemies{level};
    Health health;
    Gadgets::Run gadgets;

    Arena()
    {
        for (int c = 4; c <= 16; ++c) level.floor[7][c] = true;
        Tick(0, Point(4));
    }

    static Vector2 Point(int column) { return Center(CellId({column, 7})); }

    void Tick(float time, Vector2 player)
    {
        constexpr Rectangle wholeMap{0, 0, Maze::Columns * Maze::CellSize, Maze::Rows * Maze::CellSize};
        enemies.Update(time, player, wholeMap, light, map, health, false, {}, &gadgets);
    }

    PhysicalEnemy& Spawn(Kind kind, Vector2 position, float rest = 0)
    {
        // The manager owns mutable actors. This fixture sets up one encounter
        // at known coordinates through the same Spawn path as the real pool.
        auto& actor = const_cast<PhysicalEnemy&>(enemies.Actors()[0]);
        actor.Spawn(kind, position, rest);
        return actor;
    }
};

void SmokeAndLure()
{
    for (Kind kind : {Kind::Sentinel, Kind::Stalker})
    {
        Arena arena;
        Charge(arena.gadgets);
        auto& actor = arena.Spawn(kind, Arena::Point(5));
        const auto oldTarget = Arena::Point(8);
        arena.Tick(0.01f, oldTarget);
        assert(actor.body.state == State::Chasing);
        const auto player = Arena::Point(9);
        assert(arena.gadgets.Deploy(Type::SmokeBomb, arena.level, player));
        assert(arena.gadgets.Obscures(arena.level, player));
        assert(!arena.gadgets.Obscures(arena.level, Arena::Point(11)));
        arena.level.locked[7][8] = true;
        assert(!arena.gadgets.Obscures(arena.level, Arena::Point(8)));
        arena.level.locked[7][8] = false;
        arena.Tick(0.01f, player);
        assert(actor.body.state == State::Investigating && actor.body.target.x == oldTarget.x);
        arena.gadgets.Update(3.01f);
        arena.Tick(0.01f, player);
        assert(actor.body.state == State::Chasing && actor.body.target.x == player.x);

        actor.Spawn(kind, {player.x - 18, player.y}, 0);
        assert(arena.gadgets.Deploy(Type::SmokeBomb, arena.level, player));
        arena.Tick(0.01f, player);
        assert(actor.body.state == State::WindingUp && actor.startedWindup);
        arena.Tick(Stats(kind).windup + 0.02f, player);
        assert(arena.health.points == StartingHealth - 1 && actor.swung && !actor.startedWindup);

        const auto lure = Arena::Point(11);
        const int lureCell = CellAt(lure);
        actor.Spawn(kind, Arena::Point(6), 0);
        arena.level.locked[7][9] = true;
        assert(arena.gadgets.Deploy(Type::SoulLure, arena.level, lure));
        arena.Tick(0.01f, lure);
        assert(actor.lureGoal == NoCell);
        // The first failed route can be followed by a patrol replan. Once
        // the actor is on that edge, identical failed lure queries are cached.
        arena.Tick(0.01f, lure);
        const int searches = actor.body.route.searches;
        arena.Tick(0.01f, lure);
        assert(actor.body.route.searches == searches);
        arena.level.locked[7][9] = false;
        arena.enemies.InvalidateRoutes();
        arena.Tick(0.06f, Arena::Point(5));
        assert(actor.lureGoal == lureCell && actor.body.state == State::Investigating);
        assert(actor.body.traveled > 0 && actor.body.memory == 0);
        for (int i = 0; i < 160; ++i) arena.Tick(0.03f, Arena::Point(5));
        assert(actor.body.position.x > Arena::Point(9).x && actor.lureGoal == lureCell);
        arena.gadgets.Update(5.01f);
        arena.Tick(0.01f, Arena::Point(8));
        assert(actor.lureGoal == NoCell && actor.body.state == State::Chasing);
    }

    Arena arena;
    Charge(arena.gadgets);
    const auto player = Arena::Point(12);
    auto& actor = arena.Spawn(Kind::Stalker, Arena::Point(4));
    assert(arena.gadgets.Deploy(Type::SoulLure, arena.level, player));
    arena.Tick(0.01f, player);
    assert(actor.lureGoal == NoCell);
    auto invalid = player;
    invalid.y += Maze::CellSize;
    const auto count = arena.gadgets.Quantity(Type::SoulLure);
    assert(!arena.gadgets.Deploy(Type::SoulLure, arena.level, invalid));
    assert(arena.gadgets.Quantity(Type::SoulLure) == count);
    assert(!arena.gadgets.Smoke()[0].remaining);
}

int main()
{
    Inventory();
    Placement();
    SmokeAndLure();
    std::puts("64 gadget seeds, inventory, smoke, damage in smoke and Wizard/Demon lure routes passed.");
}
