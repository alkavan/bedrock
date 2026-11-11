# Bedrock
A library with basic utilities for build software in C.

## Features

* Argument parsing (`args`, `parse`)
* Dynamic vector (`dynvec`)
* Dynamic map (`dynmap`)
* Virtual memory regions (`vmem`)
* Arena memory allocator (`arena_allocator`)
* Pinned colony (`pinned_colony`)

## Names

Public functions are prefixed with their module, as in `args_init` and `pinned_colony_create`. Add a `bd_` prefix only when the plain name is likely to collide with another library, as with `bd_parse_i64`.

Struct and enum names stay descriptive, as in `ProgramCmd` and `ArgAction`. Macros and other defines always start with `BD_`, as in `BD_ARG_OPTION` and `BD_ARGS_OK`.

## Building
```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBEDROCK_BUILD_TESTS=ON
cmake --build build -j
```

`-DBEDROCK_THREAD_SAFE=ON` locks both `dynvec` and `dynmap` with pthreads and propagates that choice to targets that link Bedrock. `DYNVEC_THREAD_SAFE` and `DYNMAP_THREAD_SAFE` still enable one container on their own.

### CMake Integration

**Source using `FetchContent`**

```cmake
if(NOT TARGET bedrock::bedrock)
    include(FetchContent)
    FetchContent_Declare(bedrock
        GIT_REPOSITORY https://github.com/alkavan/bedrock.git
        GIT_TAG v1.0.0
    )
    FetchContent_MakeAvailable(bedrock)
endif()

target_link_libraries(my_app PRIVATE bedrock::bedrock)
```

### Installed Package
```cmake
find_package(bedrock 1.0 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE bedrock::bedrock)
```

## System integration / Out-of-source build and installation
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DBEDROCK_BUILD_TESTS=ON \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build -j
cmake --install build
```
