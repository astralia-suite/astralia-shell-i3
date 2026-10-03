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
- `build.sh`: Build, `setup` dependencies (build libraries plus `pipewire`, `wireplumber`, `bluez`, `networkmanager`, `upower`, `fd`, `brightnessctl`), `test`, `install` to `/usr/bin`, or `run` (kill, install, start `astralia-shell`).
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

- `main.cpp`: Parses the mode; runs the IPC client, or locks, daemonizes, creates X, loop, IPC server, shared `Services`, then wallpaper, bar, launcher, logout, polkit, notifications and OSD.

## `src/app/`

- `services.{h,cpp}`: `Services` owns every shared service (buses, i3, network, Bluetooth, battery, brightness, notifications, polkit, audio); modules take it by reference.

## `src/core/`

- `cli.{h,cpp}`: `parse_invocation()`: no arguments → daemon, `debug` → foreground, anything else → IPC client command.
- `runtime_paths.{h,cpp}`: Per-display lock, socket and log paths under `$XDG_RUNTIME_DIR` or `/tmp`.
- `single_instance.{h,cpp}`: RAII `flock` on the per-display lock file; fails with `already running`.
- `daemon.{h,cpp}`: `daemonize()`: fork, `setsid`, stdin from `/dev/null`, stdout/stderr to the per-display log.
- `ipc.{h,cpp}`: Unix socket `IpcServer` with verb handlers (built-in `help`, `kill`), in-process `dispatch()`, `format_help()`, and `run_ipc_client()`.
- `unique_fd.h`: Move-only RAII file descriptor.
- `log.{h,cpp}`: `log::info` and `log::error` formatted messages to stderr.
- `x_connection.{h,cpp}`: RAII xcb connection (syncs before disconnect, cairo MIT-SHM disabled) with screen, root and ARGB visuals, EWMH, atoms, RandR outputs, `pointer_output()` (the output under the pointer, primary as fallback).
- `event_loop.{h,cpp}`: `poll()` loop over X, `signalfd`, a `CLOCK_BOOTTIME` `timerfd`, extra fds and prepare/dispatch poll sources; window and event-type handlers, timers with `reschedule()`, `stop()`.
- `keyboard.{h,cpp}`: xkbcommon-x11 keymap; translates key presses, with modifiers from the event, to text, backspace, arrows, enter, escape.
- `async_process.{h,cpp}`: `AsyncProcess`: `posix_spawnp` child with captured stdout (optionally stderr), read to EOF on a detached thread; an `eventfd` on the `EventLoop` delivers the output to a main-thread callback; restart or cancel kills and drops the stale result.
- `spawn.{h,cpp}`: `spawn_detached()`: double-forked `sh -c` with an empty signal mask and default `SIGPIPE`.
- `signal.h`: `Signal<Args...>` subscriber list; services expose it, modules `connect()`, services `emit()`.
- `dbus.{h,cpp}`: `SystemBus` sdbus-c++ system or session (`BusKind`) connection driven by `EventLoop` fds, `add_match()`, `proxy()`, and `dbus_property<T>()` via proxy or path.

## `src/config/`

- `bar_config.h`: Bar geometry, corner radius, padding, border, pill sizes, divider and control center sizes, panel size, row/section/slider/empty heights, scroll step, placement and bar-only panel values (volume step, password minimum, `electro.png` echo), fonts, `strftime` clock format, 50 ms panel-close linger and `malloc_trim` interval.
- `logout_config.h`: Logout button ring geometry, Yuji Mai glyph font, logo file and the 8-entry glyph/command action table.
- `launcher_config.h`: Launcher geometry, fonts, launch commands, search limits, and result, submenu and visit plain types.
- `polkit_config.h`: Polkit card geometry, line heights, fonts, prompt texts and echo glyph file.
- `notification_config.h`: Notification stack margins, spacing, 480 px stack cap, 400 px card geometry, wrap width, close x size, app/title/body fonts and the fixed 5 s `hang_time`.
- `osd_config.h`: OSD pill geometry from `hl` widened to 300×50 so content clears the round ends (30 px bottom margin), fonts, 2 s visibility and 1 s startup delay.
- `wallpaper_config.h`: Wallpaper config file path under the config dir, `*` wildcard output key.

## `src/modules/`

- `bar.{h,cpp}`: Top dock with inset pill-shaped panel, EWMH hints and strut; subscribes to shared i3, network, Bluetooth and battery services; logout and workspaces left, clock center, status and control center right, 1 px dividers between widget groups; logout click dispatches `logout` IPC, control center click toggles its panel, Bluetooth, network, volume and battery status items toggle their panels; opening a panel or clicking elsewhere on the bar closes the others, a status item stays expanded while its panel is open and lingers 50 ms after close before re-checking the pointer; sends status-change `Notify` on the session bus; periodic `malloc_trim`.
- `launcher.{h,cpp}`: `launcher` / `launcher global` IPC toggle; override-redirect overlay on the pointer's output; takes input focus, closes on focus loss; `malloc_trim` on close.
- `logout.{h,cpp}`: `logout` IPC toggle; override-redirect overlay on the pointer's output with 8 glyph buttons around the logo; keys, hover, click run actions.
- `notification.{h,cpp}`: Subscribes to the shared `NotificationService`; unfocusable override-redirect card stack at the pointer output's bottom right; top-right x dismisses a card.
- `polkit.{h,cpp}`: Subscribes to the shared `PolkitService`; override-redirect card on the pointer's output while a request is pending; masked password, `Enter` submits, `Escape` cancels.
- `osd.{h,cpp}`: Unfocusable, click-through (empty `SHAPE` input region) override-redirect pill at the pointer output's bottom center; shows brightness, volume or mic level on service changes, hides after 2 s.
- `wallpaper.{h,cpp}`: Per-output root pixmap from `wallpaper.conf` via `_XROOTPMAP_ID`, cleared on exit; repaints on RandR or `inotify` changes, then `malloc_trim`.

## `src/modules/bar/widget/`

- `control_center_widget.{h,cpp}`: `icon::adjustments` button at the bar's right end.
- `clock_widget.{h,cpp}`: Local date and time text (`Mon 1970-01-01 00:00:00`) and `ms_until_next_second()` for per-second redraws.
- `logout_widget.{h,cpp}`: `icon::power` button with a `Logout` label shown only while hovered.
- `workspace_widget.{h,cpp}`: Pill row (active wider, accent), `workspace_row_width()` and click hit-test `workspace_at()`.
- `status_widget.{h,cpp}`: Bluetooth, network, volume and battery icons; `item_at()` maps an x offset to a `StatusItem`; device, `Idle` or `Disabled`, SSID, percent or `Plugged in` label shown while hovered or while `pin()`ed by its open panel; pure selection functions.

## `src/render/`

- `text.{h,cpp}`: Cached `PangoLayout` with a fixed font; set text, optional word wrap or end ellipsis, measure pixel size, draw (vertically centered, ink centered or ink left-aligned and vertically centered) on cairo.
- `palette.h`: `Color`, `constexpr` `color("#hex")` parser, shared `palette::` colors and `metrics::` radii/borders.
- `icons.h`: `icon::` Tabler glyph codepoints as UTF-8 strings; `volume_threshold()` and `brightness_threshold()` level icons, as in `hl`.
- `app_fonts.{h,cpp}`: Idempotent `register_app_fonts()` adds the icon, text and Yuji Mai fonts to fontconfig from the install or source dir.
- `image_decode.{h,cpp}`: `SurfacePtr` and `decode_image()`: `stb_image` rasters or `resvg` SVGs into premultiplied cairo surfaces, optionally fit to a size.
- `draw.{h,cpp}`: `set_source()` and `rounded_rect()` cairo helpers shared by every module.
- `x_window.{h,cpp}`: `XWindow`: 32-bit (or root-depth fallback) window with own colormap, name and class, GC, lazily sized pixmap and cairo context (`place()`, `release()`), raise/map with optional focus take (`show()`, `focus()`), unmap with focus restore (`hide()`), `clear()`, `present()`; optional override-redirect.
- `panel_chrome.{h,cpp}`: `panel_config::` shared panel geometry and fonts; shared panel drawing on cairo: card, header with close button, toggle, icon button, section label, device row, centered message, confirm card; `PanelRect`/`PanelHit` click regions; pure `panel_clamp_scroll()`, `panel_intersect()`, `panel_hit_at()`.
- `slider.{h,cpp}`: `draw_slider()`: track, accent fill ending under a 12 px white knob inset so it never leaves the track, optional accent focus dot for the dragged, hovered or selected slider; pure `slider_percent_at()` over the same inset range.
- `panel_window.{h,cpp}`: `PanelWindow` on an `XWindow`: placed by `open(right_margin, top)` on the primary output, takes focus, grabs the pointer with owner events on first expose so a click outside any shell window closes it, also closes on focus loss, emits `changed` on open and close, height up to a fixed max, pixmap allocated on first paint and freed on close, owner gets the remaining events.

## `src/modules/bar/panel/`

- `control_center_panel.{h,cpp}`: Panel-width card with `Control Center` header, brightness and volume sliders (click, drag, wheel) on a `PanelWindow`, following live service changes.
- `audio_panel.{h,cpp}`: Output and input sliders, per-application playback sliders, output and input device lists (click sets the default); drag, wheel and `Left`/`Right` step volume; mute buttons.
- `battery_panel.{h,cpp}`: UPower display-device row: state, time to full or empty, colored charge bar and percent; `No battery detected` otherwise; pure `battery_time_left()`, `battery_state_label()`.
- `bluetooth_panel.{h,cpp}`: Power toggle, Connected / Paired / Nearby device rows; click connects, pairs or asks to disconnect, forget button asks to forget; discovery runs while open.
- `network_panel.{h,cpp}`: Wi-Fi toggle, rescan button, error banner, Connected / Known / Available rows sorted by signal; password card for new secured networks echoing typed characters as `electro.png` glyphs, confirm cards for disconnect and forget; scanning runs while open; `network_signal_icon()`.

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

## `src/modules/notification/`

- `layout.{h,cpp}`: `notification_card_height()` from measured text, `notification_fit_count()` under the stack cap, stack height, bottom-right `notification_stack_origin()` and close-x hit-test `notification_close_at()`.

## `src/modules/polkit/`

- `layout.{h,cpp}`: `utf8_length()`, `polkit_card_height()` and `polkit_visible_dots()` for the card.

## `src/modules/wallpaper/`

- `config_file.{h,cpp}`: Config path, `output = image` line parser with `~/` expansion and `*` fallback lookup.
- `image.{h,cpp}`: `cover()` fill-and-crop placement; `load_image()` decodes via `decode_image()`.

## `src/service/`

- `i3_service.{h,cpp}`: i3 workspace numbers, occupied and current from EWMH root property events; `switch_to()` via the i3 IPC socket.
- `bluetooth_service.{h,cpp}`: BlueZ `GetManagedObjects` on a held root proxy: adapter present, powered, first connected device alias, device list with battery; async `Powered`, discovery, `Connect`, `Disconnect`, `Pair`, `RemoveDevice` through per-device proxies pruned from the signal handler; refreshes on `org.bluez` signals; `bluetooth_changes()` connect/disconnect messages.
- `network_service.{h,cpp}`: NetworkManager type, captive portal, Wi-Fi strength, SSID and `WirelessEnabled` via a held manager proxy; refreshes on `PropertiesChanged`; `nmcli` via `AsyncProcess` (port of `hl`) for the Wi-Fi list, connect, disconnect and forget, with a profiles → quick list → rescan chain repeating only between `start_watch()` and `stop_watch()`; pure `nmcli` parsers; `network_changes()` connect/disconnect/portal messages.
- `polkit_service.{h,cpp}`: Polkit authentication agent on the session; drives the default `GMainContext` via an `EventLoop` poll source; request, response and info state.
- `notification_service.{h,cpp}`: `org.freedesktop.Notifications` server on the session bus; FIFO list expiring each entry after `hang_time`.
- `brightness_service.{h,cpp}`: First `/sys/class/backlight` device percent, `inotify` change signal; `set()` via `brightnessctl`.
- `audio_service.{h,cpp}`: `libpipewire` sinks, sources and playback/capture streams with level and mute on the `EventLoop`; route-or-node `set_volume()`/`set_mute()`, `set_default()` via default metadata; `AudioKind::nodes` on any node change; pure `audio_percent()`.
- `battery_service.{h,cpp}`: UPower `DisplayDevice` presence, percent, charging, full and pending state, `TimeToFull`/`TimeToEmpty` via a held proxy; refreshes on its signals.

## `test/`

- `main.cpp`: Plain check runner for `astralia-shell-test`; covers clock timing, CLI, runtime paths, help, `color()`, status icons, workspace row, wallpaper cover and config, launcher search, parsing, URLs, ranking, submenus, logout, polkit and notification layout, network and Bluetooth change messages, `audio_percent()` and slider percent.
