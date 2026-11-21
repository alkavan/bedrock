# Bedrock
A library of core utilities for building software in C.

## Features
* Argument parsing (`args.h`/`args.c`, `parse.h`/`parse.c`)
* Dynamic vector (`dynvec.h`)
* Dynamic map (`dynmap.h`)
* Virtual memory regions (`vmem.h`/`vmem.c`)
* File mapping (`fmap.h`/`fmap.c`)
* Arena memory allocator (`arena.h`/`arena.c`)
* Polymorphic allocator (`pmr.h`/`pmr.c`)
* Pinned colony container (`pinned_colony.h`/`pinned_colony.c`)

## Names

Public functions are prefixed with their module, as in `args_init` and `pinned_colony_create`. Add a `bd_` prefix only
when the plain name is likely to collide with another library, as with `bd_parse_i64`.

Struct and enum names stay descriptive, as in `ProgramCmd` and `ArgAction`. Enumerators and other value macros start with
`BD_`, as in `BD_ARG_OPTION` and `BD_ARGS_OK`. `BED_API` marks a function defined in the library. `BED_DEPRECATED` marks
one that will be removed, and its message names the replacement.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DBEDROCK_BUILD_TESTS=ON
cmake --build build -j
```

`-DBEDROCK_THREAD_SAFE=ON` locks both `dynvec` and `dynmap` with pthreads and propagates that choice to targets that
link Bedrock. `DYNVEC_THREAD_SAFE` and `DYNMAP_THREAD_SAFE` still enable one container on their own.

### CMake Integration
**Source using `FetchContent`**

```cmake
if(NOT TARGET bedrock::bedrock)
    include(FetchContent)
    FetchContent_Declare(bedrock
        GIT_REPOSITORY https://github.com/alkavan/bedrock.git
        GIT_TAG v1.0.3
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

## Documentation

Guides, also built as pages on the docs site:

* [Basic usage](doc/basic-usage.md)
* [Online Documentation](https://docs.tekfed.org/bedrock/latest/)

API pages use the [m.css Doxygen theme](https://mcss.mosra.cz/documentation/doxygen/)
with a custom Grayscale palette (`doc/m-theme-grayscale.css`).
`doc/conf.py` and `doc/Doxyfile-mcss` drive that pipeline. `@brief`, `@param`,
and `@return` live on the declarations.

### Generate local docs with m.css
```bash
python3 -m venv .venv && source .venv/bin/activate
python3 -m pip install jinja2 Pygments
git clone --depth 1 https://github.com/mosra/m.css /tmp/m.css
python3 /tmp/m.css/documentation/doxygen.py doc/conf.py
```

HTML lands in `doc/html/`. Doxygen writes XML to `doc/xml/` first; both
directories are git-ignored.
