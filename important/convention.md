# Developing conventions

## Commenting

- Applies to C++ sources (`src/**`, `test/**`); scripts such as `build.sh` may keep a usage header.
- No comments across the code base.
- Exceptions:
  - Namespace comment.
  - Comments that group constants together, wherever those constants live (`src/config/` or a file's own constants).
  - License/attribution notices for third-party code.

## Formatting code

- Command: `clang-format -i <filename>`, for every `.h` and `.cpp` touched.
- Style taken from `.clang-format` at the project root.

## Source layout

- `src/main.cpp`: creates `core` objects, the shared `Services` and modules, runs the event loop; nothing else.
- `src/app/`: process-wide shared state (`Services`); includes `core/` and `service/`, never `modules/`.
- `src/core/`: shared infrastructure (X connection, event loop, rendering helpers, logging); includes nothing from `modules/` or `service/`.
- `src/modules/`: user-facing shell parts (bar, launcher, …).
- `src/config/`: per-module constants and plain data types.
- `src/service/`: data providers shared by modules (workspaces, windows, …).
- `src/plugin/`: optional `dlopen`-loaded code.
- `test/`: tests for `src/` code, built as `astralia-shell-test`.

## Module boundary

- A module is `src/modules/<name>.h`+`.cpp` plus, when split, its private components under `src/modules/<name>/`.
- A module is not allowed to include files from another module, its private components included.
- A module shall manage its internal works, without bleeding into `main.cpp`.
- A module registers its own windows, timers and fds with `core/event_loop.h`; `main.cpp` never dispatches events.
- `main.cpp` shall not include specific components belonging to a module.

## Config headers

- `src/config/*.h` holds constants and plain data types only, no function bodies (helpers that compute from a config value live with their consumer).
- One config header per module, named `<module>_config.h`; a module's private components (e.g. `bar`'s panels) share it rather than getting their own.

## Service structure

- `src/service/` holds as many services as needed, but limited to one pair of `**_service.{h,cpp}` per service.
- Every service is owned by `app/services.h` and passed to modules by reference, as `hl`'s `WaylandState` does.
- Services publish changes through `core/signal.h` members; modules `connect()` in their constructor.
- `src/plugin/` holds `dlopen`-loaded `shared_module`s, one `**_plugin.{h,cpp}` pair each, loaded by their owning service.

## Build targets

- Everything in `src/` except `main.cpp` compiles into one `static_library`, linked by both `astralia-shell` and `astralia-shell-test`.
- `meson.build` adds `include_directories('src')` to that library, `astralia-shell` and `astralia-shell-test`.
- Tests use no framework: `test/main.cpp` runs plain check functions and returns non-zero on failure; `meson test -C build` runs it.
- Tests cover pure logic only; nothing in `test/` opens an X connection.

## Includes

- Headers use `.h`, sources use `.cpp`.
- Every local `#include` is root-relative from `src/`, e.g. `#include "core/log.h"`, never `../` or a bare filename.
- `test/**` includes `src/` headers the same root-relative way, e.g. `#include "config/bar_config.h"`.
- Header order:
  - system headers (`<header>`), one block; a blank line may split it only where include order matters and `clang-format` would otherwise reorder it (e.g. `<X11/Xlib.h>` before `<X11/Xlib-xcb.h>`, an `extern "C"` block)
  - one blank line
  - local headers:
    - `"dir1/local_header.h"`
    - blank
    - `"dir2/local_header.h"`
