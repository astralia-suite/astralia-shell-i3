# Index

## Rule

- One-line, no break.
- Grouped by `directory`, one `##` heading per directory.
- Entry format: `file`: Purpose (≤ 20 words).
- Reflect current structure and function of each file in the code base.
- No mentions of past fixes.

## `/`

- `meson.build`: Builds `astralia-core` static library, `astralia-shell` executable and `astralia-shell-test` unit test; installs fonts and assets.
- `meson.options`: `native_cpu` option adding `-march=westmere` for the ThinkPad X201.
- `build.sh`: Build, `setup` dependencies (including `polkit`), `test`, `install` to `/usr/bin`, or `run` (kill, install, start `astralia-shell`).
- `.clang-format`: Project code style (LLVM base, 4-space indent, no column limit, preserved include blocks).

## `assets/fonts/`

- `tabler-icons.ttf`: Tabler icon font, installed to `/usr/share/astralia-shell/fonts/`.
- `ComicShannsMono-Regular.otf`: Unpatched Comic Shanns Mono text font, installed next to the icon font.
- `YujiMai.ttf`: Yuji Mai brush font for the logout button glyphs, installed next to the icon font.

## `assets/constellation/`

- `C1.png`…`C6.png`: Launcher row bullets, installed to `/usr/share/astralia-shell/constellation/`.

## `assets/logout/`

- `logo.png`: Logout overlay center logo, installed to `/usr/share/astralia-shell/logout/`.

## `assets/polkit/`

- `electro.png`: Polkit password echo glyph, installed to `/usr/share/astralia-shell/polkit/`.

## `src/`

- `main.cpp`: Parses the mode; runs the IPC client, or locks, daemonizes, creates X, loop, IPC server, wallpaper, bar, launcher, logout and polkit.

## `src/core/`

- `cli.{h,cpp}`: `parse_invocation()`: no arguments → daemon, `debug` → foreground, anything else → IPC client command.
- `runtime_paths.{h,cpp}`: Per-display lock, socket and log paths under `$XDG_RUNTIME_DIR` or `/tmp`.
- `single_instance.{h,cpp}`: RAII `flock` on the per-display lock file; fails with `already running`.
- `daemon.{h,cpp}`: `daemonize()`: fork, `setsid`, stdin from `/dev/null`, stdout/stderr to the per-display log.
- `ipc.{h,cpp}`: Unix socket `IpcServer` with verb handlers (built-in `help`, `kill`), in-process `dispatch()`, `format_help()`, and `run_ipc_client()`.
- `unique_fd.h`: Move-only RAII file descriptor.
- `log.{h,cpp}`: `log::info` and `log::error` formatted messages to stderr.
- `x_connection.{h,cpp}`: RAII xcb connection (syncs before disconnect, cairo MIT-SHM disabled) with screen, root and ARGB visuals, EWMH, atoms, RandR outputs.
- `event_loop.{h,cpp}`: `poll()` loop over X, `signalfd`, a `CLOCK_BOOTTIME` `timerfd`, extra fds and prepare/dispatch poll sources; window and event-type handlers, timers, `stop()`.
- `text.{h,cpp}`: Cached `PangoLayout` with a fixed font; set text, measure pixel size, draw (vertically or ink centered) on cairo.
- `keyboard.{h,cpp}`: xkbcommon-x11 keymap; translates key presses, with modifiers from the event, to text, backspace, arrows, enter, escape.
- `spawn.{h,cpp}`: `spawn_detached()`: double-forked `sh -c` with an empty signal mask and default `SIGPIPE`.
- `palette.h`: `Color`, `constexpr` `color("#hex")` parser, shared `palette::` colors and `metrics::` radii/borders.
- `icons.h`: `icon::` Tabler glyph codepoints as UTF-8 strings.
- `app_fonts.{h,cpp}`: Idempotent `register_app_fonts()` adds the icon, text and Yuji Mai fonts to fontconfig from the install or source dir.
- `image_decode.{h,cpp}`: `SurfacePtr` and `decode_image()`: `stb_image` rasters or `resvg` SVGs into premultiplied cairo surfaces, optionally fit to a size.
- `dbus.{h,cpp}`: `SystemBus` sdbus-c++ connection driven by `EventLoop` fds, `add_match()`, `proxy()`, and `dbus_property<T>()` via proxy or path.

## `src/config/`

- `bar_config.h`: Bar geometry, corner radius, padding, border, pill sizes, fonts, colors, `strftime` clock format and `malloc_trim` interval.
- `logout_config.h`: Logout button ring geometry, Yuji Mai glyph font, colors, logo file and the 8-entry glyph/command action table.
- `launcher_config.h`: Launcher geometry, fonts, colors, launch commands, search limits, and result, submenu and visit plain types.
- `polkit_config.h`: Polkit card geometry, line heights, fonts, colors, prompt texts and echo glyph file.
- `wallpaper_config.h`: Wallpaper config file path under the config dir, `*` wildcard output key, fallback color.

## `src/modules/`

- `bar.{h,cpp}`: Top dock with inset pill-shaped panel, EWMH hints and strut; owns services; logout and workspaces left, clock center, status right; logout click dispatches `logout` IPC; periodic `malloc_trim`.
- `launcher.{h,cpp}`: `launcher` / `launcher global` IPC toggle; override-redirect overlay on the pointer's output; takes input focus, closes on focus loss; `malloc_trim` on close.
- `logout.{h,cpp}`: `logout` IPC toggle; override-redirect overlay on the pointer's output with 8 glyph buttons around the logo; keys, hover, click run actions.
- `polkit.{h,cpp}`: Owns `PolkitService`; override-redirect card on the pointer's output while a request is pending; masked password, `Enter` submits, `Escape` cancels.
- `wallpaper.{h,cpp}`: Per-output root pixmap from `wallpaper.conf` via `_XROOTPMAP_ID`, cleared on exit; repaints on RandR or `inotify` changes, then `malloc_trim`.

## `src/modules/bar/`

- `clock_widget.{h,cpp}`: Local date and time text (`Mon 1970-01-01 00:00:00`) and `ms_until_next_second()` for per-second redraws.
- `logout_widget.{h,cpp}`: `icon::power` button with a `Logout` label shown only while hovered.
- `workspace_widget.{h,cpp}`: Pill row (active wider, accent), `workspace_row_width()` and click hit-test `workspace_at()`.
- `status_widget.{h,cpp}`: Bluetooth, network and battery icons; device, `Idle` or `Disabled`, SSID, percent or `Plugged in` label shown only while hovered; pure selection functions.

## `src/modules/launcher/`

- `app_icon.{h,cpp}`: Icon theme order, `resolve_app_icon_path()` over GTK theme, fallbacks and pixmaps; `load_app_icon()` via `decode_image()`.
- `apps_provider.{h,cpp}`: `to_lower()`, substring `score_app()` and `search_apps()` over desktop entries.
- `desktop_entry.{h,cpp}`: `.desktop` parser, `Exec` field-code stripping, XDG application dirs and deduplicated scan.
- `files_provider.{h,cpp}`: Path helpers, UTF-8 elision, glob pattern, multi-part `score_path()`, `fd` argv and output parsing, directory listing.
- `launch_action.{h,cpp}`: Shell quoting, URL normalizing, run and web modes, app and submenu launches via `spawn_detached()`.
- `search.{h,cpp}`: Prefix mode detection and ranked app/dir/file results by tier, visits, score, name.
- `search_process.{h,cpp}`: `fd` child via `posix_spawnp` with a non-blocking stdout pipe; cancel kills and reaps.
- `submenu.{h,cpp}`: Directory browse, directory actions, file actions and back navigation state.
- `visit_store.{h,cpp}`: Launch counts in `$XDG_STATE_HOME/astralia-shell/launcher_visits`.

## `src/modules/logout/`

- `layout.{h,cpp}`: `Point`, ring `logout_button_center()` and square hit-test `logout_button_at()`.

## `src/modules/polkit/`

- `layout.{h,cpp}`: `utf8_length()`, `polkit_card_height()` and `polkit_visible_dots()` for the card.

## `src/modules/wallpaper/`

- `config_file.{h,cpp}`: Config path, `output = image` line parser with `~/` expansion and `*` fallback lookup.
- `image.{h,cpp}`: `cover()` fill-and-crop placement; `load_image()` decodes via `decode_image()`.

## `src/service/`

- `workspace_service.{h,cpp}`: EWMH desktop count and current desktop from root property events; `switch_to()` via client message.
- `bluetooth_service.{h,cpp}`: BlueZ `GetManagedObjects` on a held root proxy: adapter present, powered, first connected device alias; refreshes on `org.bluez` signals.
- `network_service.{h,cpp}`: NetworkManager type, captive portal, Wi-Fi strength and SSID via a held manager proxy; refreshes on `PropertiesChanged`.
- `polkit_service.{h,cpp}`: Polkit authentication agent on the session; drives the default `GMainContext` via an `EventLoop` poll source; request, response and info state.
- `battery_service.{h,cpp}`: UPower `DisplayDevice` presence, percent, charging and full state via a held proxy; refreshes on its signals.

## `test/`

- `main.cpp`: Plain check runner for `astralia-shell-test`; covers clock timing, CLI, runtime paths, help, `color()`, status icons, workspace row, wallpaper cover and config, launcher search, parsing, URLs, ranking, submenus, logout and polkit layout.
