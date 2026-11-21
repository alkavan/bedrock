# Basic usage — Bedrock

Each section is the smallest call sequence for that module. Longer programs are in `examples/`.

## Argument parsing

`args.h` declares a command and parses `argv`. `examples/args_example.c` is a program with several subcommands.

```c
#include <bedrock/args.h>

static const ProgramArg greet_args[] = {
    BD_ARG_OPTION("name", 'n', "name", "Name of the person", BD_VALUE_STRING, BD_ARG_REQUIRED),
    BD_ARG_FLAG("shout", 's', "shout", "Shout the greeting"),
};

static const ProgramCmd greet = BD_COMMAND("greet", "Greet a person", NULL, greet_args);

int main(int argc, char **argv)
{
    CmdMatches matches;
    char err[128];
    ArgsStatus status = args_command_parse(&greet, argc, argv, &matches, err, sizeof(err));

    if (status != BD_ARGS_OK) {
        return 1;
    }

    const char *name = args_matches_get(&matches, "name");
    args_matches_free(&matches);
    (void)name;
    return 0;
}
```

## Parsing text

`parse.h` turns text into numbers, booleans, and a host with an optional port. `args_split_host_port` in `args.h` is the same split for a command-line value.

```c
#include <bedrock/parse.h>

int64_t value = 0;
uint16_t port = 0;
char host[64];

bd_parse_i64("-7", &value);
bd_parse_host_port("127.0.0.1:8080", host, sizeof(host), &port);
```

## Dynamic vector

`dynvec.h` stores `void *`. A destructor runs from `dynvec_free`. `examples/containers_example.c` keeps `Person` records this way.

```c
#include <bedrock/dynvec.h>

#include <stdlib.h>

typedef struct Person {
    const char *name;
    int score;
} Person;

static void free_person(void *element)
{
    free(element);
}

DynVec *people = dynvec_new(free_person);
Person *ada = malloc(sizeof(*ada));
ada->name = "Ada";
ada->score = 10;
dynvec_push(people, ada);

Person *first = dynvec_at(people, 0);
dynvec_free(people);
(void)first;
```

## Dynamic map

`DEFINE_STRING_KEY_MAP` and `DEFINE_INT_KEY_MAP` wrap `DynMap` with typed insert and lookup. Values are pointers the caller owns. The same containers example fills a map from the vector above.

```c
#include <bedrock/dynmap.h>

DEFINE_STRING_KEY_MAP(PersonMap, Person)

PersonMap by_name;
PersonMap_init(&by_name, 8);
PersonMap_insert(&by_name, ada->name, ada);

Person *found = PersonMap_get(&by_name, "Ada");
PersonMap_destroy(&by_name);
(void)found;
```

## Arena

`arena.h` allocates from one fixed block and can free a single allocation back to that block.

```c
#include <bedrock/arena.h>

MemHeap *heap = arena_create(4096);
char *block = arena_alloc(heap, 64);
arena_free(heap, block);
arena_destroy(heap);
```

## Virtual memory

`vmem.h` reserves address space, then commits the pages that will be touched.

```c
#include <bedrock/vmem.h>

#include <string.h>

VMemoryRegion region;
if (vmem_reserve(&region, vmem_page_size()) == 0) {
    vmem_commit(region.base, 1);
    memset(region.base, 0, vmem_page_size());
    vmem_release(&region);
}
```

## File mapping

`fmap.h` opens a regular file and maps it read-only. The pointer from `fmap_data` lasts until the next view or `fmap_close`.

```c
#include <bedrock/fmap.h>

FileMap *map = fmap_open("notes.txt");
void *data = NULL;
size_t size = 0;

if (map && fmap_data(map, &data, &size) == 0) {
    /* data points at size bytes */
}
fmap_close(map);
```

## Polymorphic allocator

`pmr.h` allocates through a handle. `_Alignof(max_align_t)` is the alignment `malloc` provides. `examples/pmr_example.c` is a resource that counts the bytes it hands out, and `examples/game_example.c` uses one for assets and one for a frame.

```c
#include <bedrock/pmr.h>

#include <stddef.h>

PmrAllocator allocator = pmr_allocator_default();
void *block = pmr_allocator_allocate(allocator, 64, _Alignof(max_align_t));
pmr_allocator_deallocate(allocator, block, 64, _Alignof(max_align_t));
```

## Pinned colony

`pinned_colony.h` keeps element addresses stable until that element is erased. `examples/pinned_colony_example.c` inserts thousands of records and erases some without moving the rest.

```c
#include <bedrock/pinned_colony.h>

int value = 7;
PinnedColony *colony = pinned_colony_create(64, sizeof(value), _Alignof(int), NULL, NULL);
int *slot = pinned_colony_insert(colony, &value);
pinned_colony_erase(colony, slot);
pinned_colony_destroy(colony);
```

## Tests and benchmarks

`unitest.h` registers `TEST_CASE` functions and `REQUIRE` checks. `unibench.h` does the same for `BENCHMARK`. Both use the `TRY` / `THROW` macros in `except.h`.
