#include "ECS/World.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("FAIL: %s\n", name);
    }
}

// --- Test components ---

struct Position { float x = 0.0f; float y = 0.0f; };
struct Velocity { float dx = 0.0f; float dy = 0.0f; };
struct Health   { int hp = 100; };

// RAII component -- tracks construction/destruction to detect double-free
static int g_raii_live = 0;

struct RaiiTag {
    std::string label;

    RaiiTag() : label("default") { ++g_raii_live; }
    explicit RaiiTag(const char* s) : label(s) { ++g_raii_live; }
    ~RaiiTag() { --g_raii_live; }

    RaiiTag(const RaiiTag& o) : label(o.label) { ++g_raii_live; }
    RaiiTag& operator=(const RaiiTag& o) { label = o.label; return *this; }

    RaiiTag(RaiiTag&& o) noexcept : label(static_cast<std::string&&>(o.label)) { ++g_raii_live; }
    RaiiTag& operator=(RaiiTag&& o) noexcept { label = static_cast<std::string&&>(o.label); return *this; }
};

// Over-aligned component
#pragma warning(push)
#pragma warning(disable: 4324)  // structure was padded due to alignment specifier
struct alignas(64) BigAligned {
    float data[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};
#pragma warning(pop)

// =====================================================================
//  EntityPool tests
// =====================================================================

static void test_create_and_alive() {
    de::World w;
    de::EntityId e = w.create();
    check(w.alive(e), "created entity is alive");
    check(e.generation >= 1, "generation >= 1");
}

static void test_destroy_invalidates() {
    de::World w;
    de::EntityId e = w.create();
    w.destroy(e);
    check(!w.alive(e), "destroyed entity is not alive");
}

static void test_generation_increments() {
    de::World w;
    de::EntityId e1 = w.create();
    w.destroy(e1);
    de::EntityId e2 = w.create();
    // reuses same index but different generation
    check(e1.index == e2.index, "index reused");
    check(e2.generation == e1.generation + 1, "generation incremented");
    check(!w.alive(e1), "old id is stale");
    check(w.alive(e2), "new id is alive");
}

static void test_null_entity_not_alive() {
    de::World w;
    check(!w.alive(de::null_entity), "null_entity is not alive");
}

static void test_entity_count() {
    de::World w;
    check(w.entity_count() == 0, "empty world has 0 entities");
    de::EntityId a = w.create();
    de::EntityId b = w.create();
    check(w.entity_count() == 2, "2 entities after 2 creates");
    w.destroy(a);
    check(w.entity_count() == 1, "1 entity after 1 destroy");
    w.destroy(b);
    check(w.entity_count() == 0, "0 entities after all destroyed");
}

// =====================================================================
//  Component set / get / has
// =====================================================================

static void test_set_and_get() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, Position{1.0f, 2.0f});
    check(w.has<Position>(e), "has Position after set");

    Position* p = w.get<Position>(e);
    check(p != nullptr, "get Position not null");
    check(p->x == 1.0f && p->y == 2.0f, "Position values correct");
}

static void test_overwrite_component() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, Position{1.0f, 2.0f});
    w.set(e, Position{3.0f, 4.0f});

    Position* p = w.get<Position>(e);
    check(p != nullptr, "get after overwrite not null");
    check(p->x == 3.0f && p->y == 4.0f, "overwritten values correct");
}

static void test_multiple_components() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, Position{5.0f, 6.0f});
    w.set(e, Velocity{1.0f, -1.0f});
    w.set(e, Health{50});

    check(w.has<Position>(e), "has Position");
    check(w.has<Velocity>(e), "has Velocity");
    check(w.has<Health>(e), "has Health");

    check(w.get<Position>(e)->x == 5.0f, "Position.x");
    check(w.get<Velocity>(e)->dy == -1.0f, "Velocity.dy");
    check(w.get<Health>(e)->hp == 50, "Health.hp");
}

static void test_get_missing_component() {
    de::World w;
    de::EntityId e = w.create();
    check(!w.has<Position>(e), "no Position before set");
    check(w.get<Position>(e) == nullptr, "get missing returns nullptr");
}

static void test_get_on_dead_entity() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, Position{1.0f, 2.0f});
    w.destroy(e);
    check(w.get<Position>(e) == nullptr, "get on dead entity returns nullptr");
    check(!w.has<Position>(e), "has on dead entity returns false");
}

// =====================================================================
//  Remove component
// =====================================================================

static void test_remove_component() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, Position{1.0f, 2.0f});
    w.set(e, Velocity{3.0f, 4.0f});

    w.remove<Position>(e);
    check(!w.has<Position>(e), "Position removed");
    check(w.has<Velocity>(e), "Velocity still present");
    check(w.get<Velocity>(e)->dx == 3.0f, "Velocity data preserved");
}

static void test_remove_then_readd() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, Position{1.0f, 2.0f});
    w.remove<Position>(e);
    w.set(e, Position{9.0f, 8.0f});

    check(w.has<Position>(e), "re-added Position present");
    check(w.get<Position>(e)->x == 9.0f, "re-added Position value");
}

// =====================================================================
//  Archetype migration preserves data
// =====================================================================

static void test_migration_preserves_data() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, Position{10.0f, 20.0f});
    // adding Velocity triggers migration from {Position} -> {Position, Velocity}
    w.set(e, Velocity{1.0f, 2.0f});

    check(w.get<Position>(e)->x == 10.0f, "Position.x preserved after migration");
    check(w.get<Position>(e)->y == 20.0f, "Position.y preserved after migration");
    check(w.get<Velocity>(e)->dx == 1.0f, "new Velocity.dx");
}

static void test_migration_multiple_entities() {
    de::World w;
    de::EntityId a = w.create();
    de::EntityId b = w.create();

    w.set(a, Position{1.0f, 1.0f});
    w.set(b, Position{2.0f, 2.0f});

    // migrate only a
    w.set(a, Velocity{5.0f, 5.0f});

    check(w.get<Position>(a)->x == 1.0f, "a Position preserved");
    check(w.get<Velocity>(a)->dx == 5.0f, "a Velocity set");
    check(w.get<Position>(b)->x == 2.0f, "b Position untouched");
    check(!w.has<Velocity>(b), "b has no Velocity");
}

// =====================================================================
//  Iteration / each
// =====================================================================

static void test_each_single_component() {
    de::World w;
    de::EntityId a = w.create();
    de::EntityId b = w.create();
    de::EntityId c = w.create();

    w.set(a, Position{1.0f, 0.0f});
    w.set(b, Position{2.0f, 0.0f});
    // c has no Position

    int count = 0;
    float sum_x = 0.0f;
    w.each<Position>([&](de::EntityId, Position& p) {
        ++count;
        sum_x += p.x;
    });
    check(count == 2, "each<Position> visits 2 entities");
    check(sum_x == 3.0f, "each<Position> sum_x = 3");
}

static void test_each_two_components() {
    de::World w;
    de::EntityId a = w.create();
    de::EntityId b = w.create();
    de::EntityId c = w.create();

    w.set(a, Position{0.0f, 0.0f});
    w.set(a, Velocity{1.0f, 0.0f});

    w.set(b, Position{10.0f, 0.0f});
    // b has no Velocity

    w.set(c, Position{0.0f, 0.0f});
    w.set(c, Velocity{2.0f, 0.0f});

    int count = 0;
    w.each<Position, Velocity>([&](de::EntityId, Position& p, Velocity& v) {
        p.x += v.dx;
        ++count;
    });

    check(count == 2, "each<Pos,Vel> visits 2 entities");
    check(w.get<Position>(a)->x == 1.0f, "a.Position.x updated by velocity");
    check(w.get<Position>(b)->x == 10.0f, "b.Position.x unchanged (no Velocity)");
    check(w.get<Position>(c)->x == 2.0f, "c.Position.x updated by velocity");
}

static void test_each_empty_world() {
    de::World w;
    int count = 0;
    w.each<Position>([&](de::EntityId, Position&) { ++count; });
    check(count == 0, "each on empty world visits 0");
}

// =====================================================================
//  Stress: destroy during multi-entity scenario
// =====================================================================

static void test_destroy_middle_entity() {
    de::World w;
    de::EntityId a = w.create();
    de::EntityId b = w.create();
    de::EntityId c = w.create();

    w.set(a, Position{1.0f, 0.0f});
    w.set(b, Position{2.0f, 0.0f});
    w.set(c, Position{3.0f, 0.0f});

    w.destroy(b);

    check(w.entity_count() == 2, "2 entities after destroy");
    check(w.alive(a), "a still alive");
    check(!w.alive(b), "b destroyed");
    check(w.alive(c), "c still alive");

    // remaining data intact
    check(w.get<Position>(a)->x == 1.0f, "a.x intact");
    check(w.get<Position>(c)->x == 3.0f, "c.x intact");
}

static void test_bulk_create_destroy() {
    de::World w;
    de::EntityId ids[100];
    for (int i = 0; i < 100; ++i) {
        ids[i] = w.create();
        w.set(ids[i], Position{static_cast<float>(i), 0.0f});
    }
    check(w.entity_count() == 100, "100 entities created");

    // destroy evens
    for (int i = 0; i < 100; i += 2) {
        w.destroy(ids[i]);
    }
    check(w.entity_count() == 50, "50 entities after destroying evens");

    // check odds still intact
    bool all_ok = true;
    for (int i = 1; i < 100; i += 2) {
        Position* p = w.get<Position>(ids[i]);
        if (!p || p->x != static_cast<float>(i)) {
            all_ok = false;
            break;
        }
    }
    check(all_ok, "odd entities data preserved after bulk destroy");
}

// =====================================================================
//  Regression: double-free on migration (RAII component)
// =====================================================================

static void test_raii_migration_no_double_free() {
    g_raii_live = 0;
    {
        de::World w;
        de::EntityId e = w.create();
        w.set(e, RaiiTag("hello"));
        check(g_raii_live == 1, "raii: 1 live after set");

        // migration: {RaiiTag} -> {RaiiTag, Position}
        w.set(e, Position{1.0f, 2.0f});
        check(g_raii_live == 1, "raii: still 1 live after migration");
        check(w.get<RaiiTag>(e)->label == "hello", "raii: label preserved");

        // migration: {RaiiTag, Position} -> {RaiiTag, Position, Velocity}
        w.set(e, Velocity{3.0f, 4.0f});
        check(g_raii_live == 1, "raii: still 1 live after 2nd migration");
        check(w.get<RaiiTag>(e)->label == "hello", "raii: label preserved 2nd");
    }
    check(g_raii_live == 0, "raii: 0 live after World destroyed");
}

static void test_raii_remove_migration() {
    g_raii_live = 0;
    {
        de::World w;
        de::EntityId e = w.create();
        w.set(e, RaiiTag("alpha"));
        w.set(e, Position{1.0f, 2.0f});
        check(g_raii_live == 1, "raii-rm: 1 live");

        // remove Position -> migration {RaiiTag, Position} -> {RaiiTag}
        w.remove<Position>(e);
        check(g_raii_live == 1, "raii-rm: still 1 after remove Position");
        check(w.get<RaiiTag>(e)->label == "alpha", "raii-rm: label intact");

        // remove RaiiTag -> migration {RaiiTag} -> {}
        w.remove<RaiiTag>(e);
        check(g_raii_live == 0, "raii-rm: 0 live after remove RaiiTag");
    }
    check(g_raii_live == 0, "raii-rm: 0 live after World destroyed");
}

static void test_raii_destroy_entity() {
    g_raii_live = 0;
    {
        de::World w;
        de::EntityId a = w.create();
        de::EntityId b = w.create();
        w.set(a, RaiiTag("aa"));
        w.set(b, RaiiTag("bb"));
        check(g_raii_live == 2, "raii-destroy: 2 live");

        w.destroy(a);
        check(g_raii_live == 1, "raii-destroy: 1 live after destroy a");
        check(w.get<RaiiTag>(b)->label == "bb", "raii-destroy: b intact");
    }
    check(g_raii_live == 0, "raii-destroy: 0 live after World destroyed");
}

static void test_raii_multiple_migrations() {
    g_raii_live = 0;
    {
        de::World w;
        de::EntityId e = w.create();
        w.set(e, RaiiTag("bounce"));

        // add then remove repeatedly to trigger many migrations
        for (int i = 0; i < 10; ++i) {
            w.set(e, Position{static_cast<float>(i), 0.0f});
            w.remove<Position>(e);
        }
        check(g_raii_live == 1, "raii-multi: 1 live after 10 add/remove cycles");
        check(w.get<RaiiTag>(e)->label == "bounce", "raii-multi: label intact");
    }
    check(g_raii_live == 0, "raii-multi: 0 live after World destroyed");
}

// =====================================================================
//  Regression: over-aligned component storage
// =====================================================================

static void test_over_aligned_component() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, BigAligned{{1.0f, 2.0f, 3.0f, 4.0f}});

    BigAligned* p = w.get<BigAligned>(e);
    check(p != nullptr, "aligned: get not null");
    check(reinterpret_cast<std::uintptr_t>(p) % 64 == 0,
          "aligned: pointer is 64-byte aligned");
    check(p->data[0] == 1.0f && p->data[3] == 4.0f,
          "aligned: data correct");
}

static void test_over_aligned_migration() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, BigAligned{{10.0f, 20.0f, 30.0f, 40.0f}});

    // migration: {BigAligned} -> {BigAligned, Position}
    w.set(e, Position{5.0f, 6.0f});

    BigAligned* ba = w.get<BigAligned>(e);
    check(ba != nullptr, "aligned-mig: get not null");
    check(reinterpret_cast<std::uintptr_t>(ba) % 64 == 0,
          "aligned-mig: still 64-byte aligned after migration");
    check(ba->data[0] == 10.0f && ba->data[3] == 40.0f,
          "aligned-mig: data preserved");
    check(w.get<Position>(e)->x == 5.0f, "aligned-mig: Position correct");
}

static void test_over_aligned_multiple_entities() {
    de::World w;
    de::EntityId ids[8];
    for (int i = 0; i < 8; ++i) {
        ids[i] = w.create();
        w.set(ids[i], BigAligned{{static_cast<float>(i), 0.0f, 0.0f, 0.0f}});
    }

    bool all_aligned = true;
    bool all_correct = true;
    for (int i = 0; i < 8; ++i) {
        BigAligned* p = w.get<BigAligned>(ids[i]);
        if (reinterpret_cast<std::uintptr_t>(p) % 64 != 0) all_aligned = false;
        if (p->data[0] != static_cast<float>(i)) all_correct = false;
    }
    check(all_aligned, "aligned-multi: all 8 pointers 64-byte aligned");
    check(all_correct, "aligned-multi: all 8 data values correct");
}

// =====================================================================
//  Iteration guard: RAII and is_iterating() contract
// =====================================================================

static void test_is_iterating_during_each() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, Position{1.0f, 2.0f});

    bool was_iterating = false;
    w.each<Position>([&](de::EntityId, Position&) {
        was_iterating = w.is_iterating();
    });
    check(was_iterating, "is_iterating true inside each()");
    check(!w.is_iterating(), "is_iterating false after each()");
}

static void test_nested_iteration_guard() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, Position{1.0f, 2.0f});
    w.set(e, Velocity{3.0f, 4.0f});

    bool inner_iterating = false;
    w.each<Position>([&](de::EntityId, Position&) {
        w.each<Velocity>([&](de::EntityId, Velocity&) {
            inner_iterating = w.is_iterating();
        });
    });
    check(inner_iterating, "nested: is_iterating true in inner each()");
    check(!w.is_iterating(), "nested: is_iterating false after both each()");
}

static void test_in_place_write_during_iteration() {
    de::World w;
    de::EntityId e = w.create();
    w.set(e, Position{1.0f, 2.0f});
    w.set(e, Velocity{3.0f, 4.0f});

    // In-place component write during iteration is allowed (not structural).
    w.each<Position, Velocity>([](de::EntityId, Position& p, Velocity& v) {
        p.x += v.dx;
        p.y += v.dy;
    });

    check(w.get<Position>(e)->x == 4.0f, "in-place write: Position.x updated");
    check(w.get<Position>(e)->y == 6.0f, "in-place write: Position.y updated");
}

// =====================================================================
//  main
// =====================================================================

int main() {
    // EntityPool
    test_create_and_alive();
    test_destroy_invalidates();
    test_generation_increments();
    test_null_entity_not_alive();
    test_entity_count();

    // Component set/get/has
    test_set_and_get();
    test_overwrite_component();
    test_multiple_components();
    test_get_missing_component();
    test_get_on_dead_entity();

    // Remove component
    test_remove_component();
    test_remove_then_readd();

    // Migration
    test_migration_preserves_data();
    test_migration_multiple_entities();

    // Iteration
    test_each_single_component();
    test_each_two_components();
    test_each_empty_world();

    // Stress / edge cases
    test_destroy_middle_entity();
    test_bulk_create_destroy();

    // Regression: RAII double-free
    test_raii_migration_no_double_free();
    test_raii_remove_migration();
    test_raii_destroy_entity();
    test_raii_multiple_migrations();

    // Regression: over-aligned storage
    test_over_aligned_component();
    test_over_aligned_migration();
    test_over_aligned_multiple_entities();

    // Iteration guard
    test_is_iterating_during_each();
    test_nested_iteration_guard();
    test_in_place_write_during_iteration();

    std::printf("\nEcsTest results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
