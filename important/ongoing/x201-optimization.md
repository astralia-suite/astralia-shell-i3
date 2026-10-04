# Handoff: X201 optimization

Status: proposed, awaiting user approval. No code for this work exists yet.

## Context

- The user's priority is for every feature to work smoothly on the ThinkPad X201: Westmere i5, 2 cores, no AVX, `1280x800`, a slow disk.
- All measurements so far come from the dev machine: i5-8365U, 8 threads, 8 GB RAM, SSD. Nothing has been measured on an X201.
- The recommended course was proposed in chat and the user has not yet approved implementing it. Write a plan into `local/plan/` first and halt for approval, as in plans 15 and 16.

## Measured baseline

- Test setup: nested `Xephyr`, scratch `HOME`, `XDG_CONFIG_HOME` and `XDG_CACHE_HOME`, and the user's `133`-image wallpaper folder (`95` `.jpg`, `5` `.jfif`, `30` `.png`, `3` `.webp`, images up to `10240x4320`).
- Startup peak memory: `31` MB, from decoding the wallpaper.
- Peak while the wallpaper tab loads cold thumbnails: `292` MB, from full PNG decodes.
- Resident memory with the tab open after loading: `27` MB, and `26` MB after closing.
- A warm cache fills the first screen of thumbnails in about `2` seconds.
- Cold first-load timing on slow hardware is unknown.

## Planned work

1. Build for the X201.
   - `meson.options` has `native_cpu` (`-march=westmere`), default `false`.
   - `build.sh` runs `meson setup build --prefix=/usr` in `cmd_build`. Make the X201 build pass `-Dnative_cpu=true` by default, keeping a way to turn it off.
2. Lower the thumbnail worker's priority.
   - File: `src/modules/settings/thumbnail_cache.cpp`, function `work`.
   - Use `setpriority(PRIO_PROCESS, gettid(), 10)` at the start of the thread, so decoding yields to the UI on 2 cores.
3. Adaptive cover cache format.
   - Files: `src/render/cover_cache.{h,cpp}`.
   - Store opaque images as JPEG and images with transparency as PNG. JPEG writes and reads faster and is far smaller, which matters on a slow disk.
   - Detect transparency by scanning the cropped surface for alpha below `255`.
   - Look up both extensions on a hit. Keep the pruning and mtime-touch behavior working for both.
   - `cover_cache_name` returns `<hash>.png` today. Update it and its tests in `test/main.cpp`.
   - Encode with `libjpeg`, which is already a dependency.
4. Cheaper settings repaints.
   - Files: `src/modules/settings.cpp`, `src/modules/settings/wallpaper_tab.cpp`, `src/modules/settings/thumbnail_cache.{h,cpp}`.
   - Every arriving thumbnail triggers a full `Settings::paint`, and every paint re-uploads each visible thumbnail, about `800` KB, to the X server.
   - Keep thumbnails as X-side surfaces, created once with `cairo_surface_create_similar` from the window's target.
   - Batch repaints from thumbnail arrivals into one about every `50` ms with an `EventLoop` timer.
5. Timing logs.
   - Log, with `log::info`, how long each wallpaper apply, thumbnail load and settings paint takes.
   - The user will run the build on the X201 and send the log, so further decisions use real numbers.

## Conditional work

- Only if X201 numbers show a problem.
  - Stream-decode PNGs row by row with `libpng` and shrink them as they load, as `hl` does. This removes the large PNG peak.
  - Move the startup and output-change wallpaper load off the main thread. A warm cache already makes it fast.
  - Share bar timers across bars if a second monitor shows lag.
- Known unsupported: `.webp`, which `stb_image` cannot decode.

## Working rules

- Follow `important/convention.md` and the markdown and git rules in `CLAUDE.md`. Git is only for diffing between commits.
- Write the plan into `local/plan/`, halt for approval, then run `./build.sh test`, update `important/index.md` and `important/knowledge.md`, and mark the plan done.
- The user's real shell runs on display `:0`. Never start a second instance against it.
- Test in a nested server: `DISPLAY=:0 Xephyr :9 -screen 1280x720 -ac`, then run `./build/astralia-shell debug` with `DISPLAY=:9`, a scratch `HOME`, `XDG_CONFIG_HOME` and `XDG_CACHE_HOME`, under `dbus-run-session`.
- The `polkit: cannot register the agent` error in that sandbox is expected, because the real agent already exists.
- Measure memory from `/proc/<pid>/status` (`VmRSS`, `RssAnon`, `VmHWM`) after opening and closing the wallpaper tab.
