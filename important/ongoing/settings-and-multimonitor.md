# Handoff: settings module and multi-monitor

Status: implemented and passing `./build.sh test`. The pieces below are unverified or open.

## What exists

- Plans: `local/plan/15-settings-multimonitor.md` and `local/plan/16-scaled-image-cache.md`, both marked done, with their deviations and results.
- Multi-monitor: `BarSet` (`src/modules/bar_set.{h,cpp}`) keeps one `Bar` per enabled output. Panels anchor to their bar's output through `PanelWindow::set_output`. `OSD` and notifications follow the pointer output and respect per-output switches.
- Services: `OutputService` (RandR outputs), `SettingsService` (`settings.conf`) and `WallpaperService` (`wallpaper.conf`), all owned by `Services`.
- Settings overlay: `src/modules/settings.{h,cpp}` plus `src/modules/settings/`. IPC verb `settings` toggles it. It has `Bar`, `Displays` and `Wallpaper` tabs.
- Image loading: `src/render/image_decode.{h,cpp}` decodes JPEGs at reduced size, and `src/render/cover_cache.{h,cpp}` caches box-sized covers on disk with pruning.
- Allocator: `src/core/allocator.{h,cpp}` sets `M_ARENA_MAX` to `1` and a fixed `M_MMAP_THRESHOLD`, which fixed a `50` MB retention from the decode thread's arena.

## Config formats

- `$XDG_CONFIG_HOME/astralia-shell/wallpaper.conf`: `output = image`, with `*` as the default for outputs without their own line.
- `$XDG_CONFIG_HOME/astralia-shell/settings.conf`: created with defaults when missing. Keys are `bar`, `osd`, `notifications` (`on` or `off`), per-output overrides as `<output>.<key>`, `bar_style` (`continuous` or `okinami`, global) and `wallpaper_dir`.
- Both are hand-parsed `key = value` files, written atomically, preserving comments, and reloaded live through `inotify`. A service compares parsed content before emitting, so its own write does not cause a second reload.
- Cache: `$XDG_CACHE_HOME/astralia-shell/covers/`, PNG files keyed by path, size, mtime and target size. Pruned at `90` days and `128` MiB, least recently used first.

## Behaviors worth knowing

- The `Displays` tab has a `Default` chip and one chip per named output. A non-default output shows `Override default settings`. Turning it on copies the effective values into explicit overrides, and turning it off removes them.
- The `Wallpaper` tab has no `Default` chip. It selects the first named output. If no output has a name, as in the nested test server, it edits `*`.
- The settings window is a card-sized window, up to `920x680`, centered on the pointer output. It styles after `hl`: header with a close button, profile block, and a nav rail that collapses to icons below `700` wide, with no animations. It takes a pointer grab, so a click outside closes it. `Escape` and focus loss also close it.
- The wallpaper grid is `5` columns, centered. Thumbnails load on a worker thread that exists only while the tab is open.
- Outputs with an empty name come from the no-RandR fallback and are skipped in chip rows.

## Not verified

- A real two-output setup. The nested server exposes one output, so bar placement per output, hotplug, per-output overrides and the `Wallpaper` chip row with several outputs rely on unit tests and review only.
- Anything on the X201. See `important/ongoing/x201-optimization.md`.
- The `Wallpaper` chip row with named outputs on screen. Only the single unnamed output was seen.

## Known limits

- `hl` features not ported because `i3` has no counterpart: animated or video wallpaper, columns, fill modes, `Bar Autohide`, and the `Animation`, `Idle`, `Logout`, `Rain` and `Visualizer` tabs.
- `.webp` is unsupported.
- PNGs decode fully on a cache miss, with a peak of about `290` MB for the largest images in the user's folder.
- Cache entries for edited images stay until the age or size limit prunes them.

## Pitfalls for the next editor

- `EventLoop` cannot remove windows or timers, and `Signal` has no disconnect. Hide objects instead of destroying them.
- `EventLoop::on_event` keeps one handler per event type. `OutputService` owns the RandR notify, and others subscribe to its `changed` signal.
- A module may not include another module's files. Shared code belongs in `render/`, `core/` or `service/`.
- `Bar` no longer sends Bluetooth and network notifications. `BarSet` does, so several bars do not duplicate them.
- Thread rules: a detached thread must own what it touches through a `shared_ptr`, as `ThumbnailCache::Shared` does.
