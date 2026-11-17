// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/*
 * One room, three entities, three frames. The engine is a stub.
 *
 * Entities and their components sit in pinned colonies, so despawn does
 * not move the survivors. Asset bytes live on a counting pmr heap.
 * Each frame builds its draw list in a bump-pointer pmr resource and
 * resets that resource when the frame ends.
 */

#include <bedrock.h>
#include <bedrock/pinned_colony.h>
#include <bedrock/pmr.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAX_ENTITIES = 8,
    MAX_ASSETS = 4,
    FRAME_BYTES = 256
};

typedef struct Transform {
    float x;
    float y;
} Transform;

typedef struct Sprite {
    unsigned asset;
} Sprite;

typedef struct Health {
    int hp;
} Health;

typedef struct DrawCmd {
    unsigned asset;
    float x;
    float y;
} DrawCmd;

enum {
    ASSET_PLAYER = 0,
    ASSET_SLIME = 1,
    ASSET_HIT = 2
};

typedef struct Entity {
    unsigned id;
    Transform *transform;
    Sprite *sprite;
    Health *health;
} Entity;

typedef struct Asset {
    const char *name;
    unsigned char *bytes;
    size_t size;
} Asset;

typedef struct CountingResource {
    PmrResource base; /* must be first */
    size_t live_blocks;
    size_t live_bytes;
} CountingResource;

typedef struct FrameResource {
    PmrResource base; /* must be first */
    PmrAllocator heap;
    unsigned char *buffer;
    size_t capacity;
    size_t offset;
    size_t peak;
    size_t buffer_bytes;
    size_t buffer_align;
} FrameResource;

typedef struct World World;

struct World {
    CountingResource asset_mem;
    FrameResource frame_mem;
    PmrAllocator assets;
    PmrAllocator frame;
    PinnedColony *entities;
    PinnedColony *transforms;
    PinnedColony *sprites;
    PinnedColony *healths;
    Asset loaded[MAX_ASSETS];
    size_t asset_count;
    unsigned next_id;
    unsigned player_id;
    unsigned kill_id;
    unsigned kill_frame;
    float wish_x;
    size_t drawn;
};

typedef int (*SystemFn)(World *world, unsigned frame);

static void *counting_allocate(const PmrResource *self, const size_t bytes, const size_t alignment) {
    CountingResource *counting = (CountingResource *)self;
    void *block = pmr_allocate(pmr_new_delete_resource(), bytes, alignment);
    if (!block) {
        return NULL;
    }
    counting->live_blocks += 1;
    counting->live_bytes += bytes;
    return block;
}

static void counting_deallocate(const PmrResource *self, void *p,
                                const size_t bytes, const size_t alignment) {
    CountingResource *counting = (CountingResource *)self;
    pmr_deallocate(pmr_new_delete_resource(), p, bytes, alignment);
    counting->live_blocks -= 1;
    counting->live_bytes -= bytes;
}

static const PmrResourceVtable counting_vtable = {
    .allocate = counting_allocate,
    .deallocate = counting_deallocate,
    .is_equal = pmr_is_same_object
};

static void *frame_allocate(const PmrResource *self, const size_t bytes, const size_t alignment) {
    FrameResource *frame = (FrameResource *)self;
    const uintptr_t mask = (uintptr_t)alignment - 1u;
    const uintptr_t cursor = (uintptr_t)frame->buffer + frame->offset;

    if (frame->offset > frame->capacity || mask > UINTPTR_MAX - cursor) {
        return NULL;
    }

    const uintptr_t aligned = (cursor + mask) & ~mask;
    const size_t padding = (size_t)(aligned - cursor);
    const size_t remain = frame->capacity - frame->offset;
    if (padding > remain || bytes > remain - padding) {
        return NULL;
    }

    frame->offset += padding + bytes;
    if (frame->offset > frame->peak) {
        frame->peak = frame->offset;
    }
    return (void *)aligned;
}

static void frame_deallocate(const PmrResource *self, void *p,
                             const size_t bytes, const size_t alignment) {
    (void)self;
    (void)p;
    (void)bytes;
    (void)alignment;
}

static const PmrResourceVtable frame_vtable = {
    .allocate = frame_allocate,
    .deallocate = frame_deallocate,
    .is_equal = pmr_is_same_object
};

static void frame_reset(FrameResource *frame) {
    frame->offset = 0;
}

static void frame_release(FrameResource *frame) {
    if (!frame->buffer) {
        return;
    }
    pmr_allocator_deallocate(frame->heap, frame->buffer, frame->buffer_bytes, frame->buffer_align);
    frame->buffer = NULL;
    frame->offset = 0;
}

static void release_component(PinnedColony *colony, const void *slot) {
    if (colony && slot) {
        pinned_colony_erase(colony, slot);
    }
}

static void entity_dtor(void *element, void *user) {
    Entity *entity = element;
    const World *world = user;
    release_component(world->transforms, entity->transform);
    release_component(world->sprites, entity->sprite);
    release_component(world->healths, entity->health);
    entity->transform = NULL;
    entity->sprite = NULL;
    entity->health = NULL;
}

static void world_destroy(World *world) {
    pinned_colony_destroy(world->entities);
    world->entities = NULL;
    pinned_colony_destroy(world->transforms);
    world->transforms = NULL;
    pinned_colony_destroy(world->sprites);
    world->sprites = NULL;
    pinned_colony_destroy(world->healths);
    world->healths = NULL;

    for (size_t i = 0; i < world->asset_count; ++i) {
        Asset *asset = &world->loaded[i];
        if (!asset->bytes) {
            continue;
        }
        pmr_allocator_deallocate(world->assets, asset->bytes, asset->size, _Alignof(max_align_t));
        asset->bytes = NULL;
    }
    frame_release(&world->frame_mem);
}

static int world_create(World *world) {
    memset(world, 0, sizeof(*world));
    world->asset_mem.base.vtable = &counting_vtable;
    world->assets = pmr_allocator_from(&world->asset_mem.base);
    world->frame_mem.base.vtable = &frame_vtable;
    world->frame_mem.heap = world->assets;
    world->frame_mem.buffer_bytes = FRAME_BYTES;
    world->frame_mem.buffer_align = _Alignof(max_align_t);
    world->frame = pmr_allocator_from(&world->frame_mem.base);

    world->entities = pinned_colony_create(MAX_ENTITIES, sizeof(Entity), _Alignof(Entity),
                                           entity_dtor, world);

    world->transforms = pinned_colony_create(MAX_ENTITIES, sizeof(Transform), _Alignof(Transform),
                                             NULL, NULL);

    world->sprites = pinned_colony_create(MAX_ENTITIES, sizeof(Sprite), _Alignof(Sprite),
                                          NULL, NULL);

    world->healths = pinned_colony_create(MAX_ENTITIES, sizeof(Health), _Alignof(Health),
                                          NULL, NULL);

    world->frame_mem.buffer = pmr_allocator_allocate(world->assets, FRAME_BYTES,
                                                     world->frame_mem.buffer_align);

    world->frame_mem.capacity = FRAME_BYTES;

    if (!world->entities
        || !world->transforms
        || !world->sprites
        || !world->healths
        || !world->frame_mem.buffer) {
        world_destroy(world);
        return -1;
    }

    return 0;
}

static int asset_load(World *world, const char *name, const void *src, const size_t size) {
    if (world->asset_count >= MAX_ASSETS || size == 0) {
        return -1;
    }

    unsigned char *bytes = pmr_allocator_allocate(world->assets, size, _Alignof(max_align_t));
    if (!bytes) {
        return -1;
    }

    memcpy(bytes, src, size);
    Asset *asset = &world->loaded[world->asset_count++];
    asset->name = name;
    asset->bytes = bytes;
    asset->size = size;
    return 0;
}

static Entity *spawn(World *world, const float x, const float y,
                     const unsigned asset, const int hp) {
    const Entity blank = {0};
    Entity *entity = pinned_colony_insert(world->entities, &blank);
    const Transform transform = {.x = x, .y = y};
    const Sprite sprite = {.asset = asset};
    const Health health = {.hp = hp};

    if (!entity) {
        return NULL;
    }

    entity->id = ++world->next_id;
    entity->transform = pinned_colony_insert(world->transforms, &transform);
    entity->sprite = pinned_colony_insert(world->sprites, &sprite);
    entity->health = pinned_colony_insert(world->healths, &health);

    if (!entity->transform || !entity->sprite || !entity->health) {
        pinned_colony_erase(world->entities, entity);
        return NULL;
    }

    return entity;
}

static Entity *find_entity(const World *world, const unsigned id) {
    size_t index = SIZE_MAX;
    while ((index = pinned_colony_next_index(world->entities, index)) != SIZE_MAX) {
        Entity *entity = pinned_colony_at(world->entities, index);
        if (entity->id == id) {
            return entity;
        }
    }
    return NULL;
}

/* Stub: a real input system would read the keyboard. Frame 0 steps right. */
static int sys_input(World *world, const unsigned frame) {
    world->wish_x = (frame == 0u) ? 1.f : 0.f;
    return 0;
}

static int sys_move(World *world, const unsigned frame) {
    Entity *player = find_entity(world, world->player_id);
    (void)frame;
    if (!player) {
        return -1;
    }
    player->transform->x += world->wish_x;
    return 0;
}

/* Stub: a real audio mixer would queue PCM. This prints the clip. */
static void engine_play(const Asset *asset) {
    printf("  audio: %s (%u bytes)\n", asset->name, (unsigned)asset->size);
}

static int sys_combat(World *world, const unsigned frame) {
    if (frame != world->kill_frame || world->kill_id == 0u) {
        return 0;
    }

    Entity *target = find_entity(world, world->kill_id);
    if (!target) {
        return -1;
    }

    target->health->hp = 0;
    engine_play(&world->loaded[ASSET_HIT]);

    printf("  combat: entity %u despawned\n", target->id);
    if (pinned_colony_erase(world->entities, target) != 0) {
        return -1;
    }

    world->kill_id = 0u;
    return 0;
}

/* Stub: a real renderer would upload the draw list. */
static void engine_draw(const unsigned asset, const float x, const float y) {
    printf("  draw asset %u at %.0f,%.0f\n", asset, x, y);
}

static int sys_draw(World *world, const unsigned frame) {
    const size_t count = pinned_colony_size(world->entities);
    size_t index = SIZE_MAX;
    size_t n = 0;

    (void)frame;
    world->drawn = 0;
    if (count == 0) {
        return 0;
    }

    DrawCmd *cmds = pmr_allocator_allocate_array(world->frame, count, sizeof(DrawCmd), _Alignof(DrawCmd));
    if (!cmds) {
        return -1;
    }

    while ((index = pinned_colony_next_index(world->entities, index)) != SIZE_MAX) {
        const Entity *entity = pinned_colony_at(world->entities, index);
        cmds[n].asset = entity->sprite->asset;
        cmds[n].x = entity->transform->x;
        cmds[n].y = entity->transform->y;
        n += 1;
    }

    if (n != count) {
        return -1;
    }

    for (index = 0; index < n; ++index) {
        engine_draw(cmds[index].asset, cmds[index].x, cmds[index].y);
    }
    world->drawn = n;
    return 0;
}

static int play_frame(World *world, const unsigned frame) {
    static const SystemFn systems[] = {
        sys_input,
        sys_move,
        sys_combat,
        sys_draw
    };

    printf("frame %u\n", frame);
    for (size_t i = 0; i < sizeof(systems) / sizeof(systems[0]); ++i) {
        if (systems[i](world, frame) != 0) {
            return -1;
        }
    }
    frame_reset(&world->frame_mem);
    return 0;
}

int main(void) {
    static const unsigned char player_pixels[] = {0xE0, 0xC0, 0x40, 0xFF};
    static const unsigned char slime_pixels[] = {0x40, 0xC0, 0x40, 0xFF};
    static const unsigned char hit_pcm[] = {0x00, 0x7F, 0x00, 0x80};

    World world;
    Entity *player;
    Entity *slime_a;
    Entity *slime_b;
    Transform *player_at;
    Transform *slime_a_at;
    unsigned dead_id;
    int status = EXIT_FAILURE;
    void *overflow;

    if (world_create(&world) != 0) {
        fprintf(stderr, "game example: world create failed\n");
        return EXIT_FAILURE;
    }

    if (pmr_allocator_is_equal(world.assets, world.frame)) {
        fprintf(stderr, "game example: asset heap and frame scratch compared equal\n");
        goto done;
    }

    if (asset_load(&world, "player.rgba", player_pixels, sizeof(player_pixels)) != 0
        || asset_load(&world, "slime.rgba", slime_pixels, sizeof(slime_pixels)) != 0
        || asset_load(&world, "hit.pcm", hit_pcm, sizeof(hit_pcm)) != 0) {
        fprintf(stderr, "game example: asset load failed\n");
        goto done;
    }

    player = spawn(&world, 0.f, 0.f, ASSET_PLAYER, 10);
    slime_a = spawn(&world, 3.f, 0.f, ASSET_SLIME, 2);
    slime_b = spawn(&world, 5.f, 1.f, ASSET_SLIME, 1);
    if (!player || !slime_a || !slime_b) {
        fprintf(stderr, "game example: spawn failed\n");
        goto done;
    }

    world.player_id = player->id;
    world.kill_id = slime_b->id;
    world.kill_frame = 1u;
    player_at = player->transform;
    slime_a_at = slime_a->transform;
    dead_id = slime_b->id;

    overflow = pmr_allocator_allocate(world.frame, FRAME_BYTES + 1u, _Alignof(max_align_t));
    if (overflow) {
        fprintf(stderr, "game example: frame scratch grew past its buffer\n");
        goto done;
    }

    printf("game example — Bedrock v%s\n", BEDROCK_VERSION);
    printf("loaded %u assets (%u live bytes), frame scratch %u bytes\n",
           (unsigned)world.asset_count,
           (unsigned)world.asset_mem.live_bytes,
           FRAME_BYTES);

    if (play_frame(&world, 0u) != 0 || world.drawn != 3u || player_at->x != 1.f) {
        fprintf(stderr, "game example: frame 0 failed\n");
        goto done;
    }

    if (play_frame(&world, 1u) != 0 || world.drawn != 2u || find_entity(&world, dead_id) != NULL) {
        fprintf(stderr, "game example: frame 1 failed\n");
        goto done;
    }

    if (player->transform != player_at || slime_a->transform != slime_a_at
        || pinned_colony_index(world.entities, player) == SIZE_MAX
        || pinned_colony_size(world.entities) != 2u) {
        fprintf(stderr, "game example: despawn moved a survivor\n");
        goto done;
    }

    if (play_frame(&world, 2u) != 0 || world.drawn != 2u || world.frame_mem.offset != 0u
        || world.frame_mem.peak == 0u) {
        fprintf(stderr, "game example: frame 2 failed\n");
        goto done;
    }

    printf("player stayed at %p, scratch peak %u bytes\n",
           (void *)player, (unsigned)world.frame_mem.peak);

    world_destroy(&world);
    if (world.asset_mem.live_blocks != 0u || world.asset_mem.live_bytes != 0u) {
        fprintf(stderr, "game example: asset heap leaked\n");
        return EXIT_FAILURE;
    }

    puts("game example: entities kept stable addresses, assets and scratch were released");
    status = EXIT_SUCCESS;

done:
    if (status != EXIT_SUCCESS) {
        world_destroy(&world);
    }
    return status;
}
