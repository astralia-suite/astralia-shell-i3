# Index

## Rule

- One-line, no break.
- Grouped by `directory`, one `##` heading per directory.
- Entry format: `file`: Purpose (≤ 20 words).
- Reflect current structure and function of each file in the code base.
- No mentions of past fixes.

## `/`

- `meson.build`: Builds the core library, shell executable and unit tests; installs fonts and assets.
- `meson.options`: Optional native CPU tuning for the ThinkPad X201.
- `build.sh`: Builds with X201 tuning unless `NATIVE_CPU=false`, installs dependencies, runs tests, installs or restarts the shell.
- `.clang-format`: Project code style.

## `assets/fonts/`

- `tabler-icons.ttf`: Icon font.
- `ComicShannsMono-Regular.otf`: Text font.
- `YujiMai.ttf`: Brush font for the logout glyphs.

## `assets/constellation/`

- `C1.png`…`C6.png`: Launcher row bullets.

## `assets/logout/`

- `logo.png`: Logout overlay center logo.

## `assets/polkit/`

- `electro.png`: Password echo glyph.

## `src/`

- `main.cpp`: Runs the IPC client, or starts the daemon with its shared services and modules.

## `src/app/`

- `services.{h,cpp}`: Owns every shared service, passed to modules by reference.

## `src/core/`

- `cli.{h,cpp}`: Picks daemon, foreground or IPC client mode from the arguments.
- `runtime_paths.{h,cpp}`: Per-display lock, socket and log paths.
- `single_instance.{h,cpp}`: Ensures one shell per display.
- `daemon.{h,cpp}`: Detaches the shell into the background with output to the log.
- `ipc.{h,cpp}`: Unix socket IPC server, verb dispatch and client.
- `json.{h,cpp}`: Minimal JSON value and parser for i3 IPC replies.
- `unique_fd.h`: Owning file descriptor.
- `log.{h,cpp}`: Info and error logging to stderr.
- `x_connection.{h,cpp}`: X connection with screen, visuals, atoms, EWMH and RandR outputs.
- `event_loop.{h,cpp}`: Main loop for X events, signals, timers and file descriptors.
- `keyboard.{h,cpp}`: Translates key presses to text and editing keys.
- `allocator.{h,cpp}`: Limits `malloc` to one arena and maps large buffers separately so they return to the system.
- `async_process.{h,cpp}`: Runs a child process and returns its output on the main thread.
- `config_file.{h,cpp}`: Parses and edits `key = value` files, writes them atomically and watches their directory.
- `spawn.{h,cpp}`: Launches detached shell commands.
- `signal.h`: Change notifications from services to modules.
- `dbus.{h,cpp}`: System and session D-Bus connections, matches, proxies and property reads.

## `src/config/`

- `bar_config.h`: `BarStyle` and its name/label tables, bar, Okinami, widget and panel constants.
- `logout_config.h`: Logout overlay constants and actions.
- `launcher_config.h`: Launcher constants and result types.
- `polkit_config.h`: Polkit card constants and prompt texts.
- `notification_config.h`: Notification card and stack constants.
- `osd_config.h`: OSD pill constants and timings.
- `overview_config.h`: Overview grid, scale, rounding, border, icon and focus-grace constants.
- `wallpaper_config.h`: Wallpaper config file location and wildcard key.
- `settings_config.h`: Settings file, defaults, overlay, tab, chip, tile and thumbnail grid constants.

## `src/modules/`

- `bar.{h,cpp}`: Top bar on one output with logout, workspaces, media, clock, tray and status widgets; lays them out, paints them through the active `BarStyle` and opens their panels. A style change re-places the window, strut and panel tops.
- `bar_set.{h,cpp}`: Keeps one bar per enabled output, moving or hiding bars on output and settings changes.
- `launcher.{h,cpp}`: App, file and command launcher overlay.
- `logout.{h,cpp}`: Logout overlay with power action buttons.
- `notification.{h,cpp}`: Desktop notification cards.
- `polkit.{h,cpp}`: Polkit password prompt.
- `osd.{h,cpp}`: Brightness, volume and mic level popup.
- `overview.{h,cpp}`: Workspace overview overlay with window tiles, click, drag, arrow, digit and delete keys.
- `wallpaper.{h,cpp}`: Per-output desktop wallpaper.
- `settings.{h,cpp}`: Settings overlay with the Bar, Displays and Wallpaper tabs.

## `src/modules/bar/styles/`

- `geometry.{h,cpp}`: `BarStyleSpec`, window height, panel rect and panel top per style, and the pure Okinami island and fillet placement; test-linked.
- `continuous.{h,cpp}`: `continuous_style_spec` and `paint_continuous`, one rounded capsule with dividers.
- `okinami.{h,cpp}`: `okinami_style_spec` and `paint_okinami`, the top rail with islands and concave fillets, punched with `CAIRO_OPERATOR_SOURCE`.

## `src/modules/bar/widget/`

- `widget_capsule.{h,cpp}`: Icon and label widget with hover-revealed label.
- `bar_widget.h`: Base for bar widgets with the bar's fonts.
- `clock_widget.{h,cpp}`: Clock icon with date and time label.
- `media_widget.{h,cpp}`: Media icon with label.
- `logout_widget.{h,cpp}`: Logout icon with label.
- `workspace_widget.{h,cpp}`: Workspace pill row with the overview icon, and click hit-tests.
- `bluetooth_widget.{h,cpp}`: Bluetooth icon with device or state label.
- `network_widget.{h,cpp}`: Network icon with SSID label.
- `brightness_widget.{h,cpp}`: Brightness icon with percent label.
- `volume_widget.{h,cpp}`: Volume icon with percent or muted label.
- `battery_widget.{h,cpp}`: Battery icon with percent or plugged-in label.
- `tray_widget.{h,cpp}`: Tray icon with label.

## `src/modules/bar/panel/`

- `brightness_panel.{h,cpp}`: Brightness slider panel.
- `volume_panel.{h,cpp}`: Output, input and per-app volume, mute and default device panel.
- `battery_panel.{h,cpp}`: Battery state, time left and charge panel.
- `bluetooth_panel.{h,cpp}`: Bluetooth power and device connect, pair and forget panel.
- `media_panel.{h,cpp}`: Now-playing panel with art, track info, position and playback controls.
- `clock_panel.{h,cpp}`: Calendar panel with today's date and month navigation.
- `network_panel.{h,cpp}`: Wi-Fi toggle and network connect, disconnect and forget panel.
- `tray_panel.{h,cpp}`: Tray icon grid panel and its separate popup menu window with submenus.

## `src/modules/launcher/`

- `apps_provider.{h,cpp}`: Searches desktop applications.
- `desktop_entry.{h,cpp}`: Reads `.desktop` files.
- `files_provider.{h,cpp}`: Searches and lists files and directories.
- `launch_action.{h,cpp}`: Launches apps, commands, URLs and file actions.
- `search.{h,cpp}`: Ranks launcher results.
- `search_process.{h,cpp}`: Runs the file search child process.
- `submenu.{h,cpp}`: Directory and file action submenu state.
- `visit_store.{h,cpp}`: Stores launch counts.

## `src/modules/logout/`

- `layout.{h,cpp}`: Logout button positions and hit-test.

## `src/modules/overview/`

- `layout.{h,cpp}`: Overview grid cells, paging, tile scaling, hit-test and wrapping steps.

## `src/modules/notification/`


- `layout.{h,cpp}`: Notification card sizes, stack position and close hit-test.

## `src/modules/polkit/`

- `layout.{h,cpp}`: Polkit card height and password dot count.

## `src/modules/settings/`

- `layout.{h,cpp}`: Settings card size, header, close button, profile block, collapsing rail, chip row and thumbnail grid geometry and hit-testing.
- `widgets.{h,cpp}`: Settings card, profile block, nav rail, choice tile, output chip row and toggle tile drawing.
- `bar_tab.{h,cpp}`: Bar style selector tiles.
- `displays_tab.{h,cpp}`: Per-output bar, OSD and notification toggles with default override.
- `wallpaper_tab.{h,cpp}`: Folder field, image grid and per-output wallpaper picking.
- `thumbnail_cache.{h,cpp}`: Loads visible wallpaper thumbnails on a low-priority worker thread and keeps them as X-side surfaces.

## `src/render/`

- `text.{h,cpp}`: Measures and draws text.
- `palette.h`: Shared colors, radii and borders.
- `icons.h`: Icon glyph codepoints and level icon pickers.
- `app_fonts.{h,cpp}`: Registers the bundled fonts.
- `app_icon.{h,cpp}`: Finds and loads application icons from the icon theme, including by window class.
- `image_decode.{h,cpp}`: Decodes raster and SVG images, JPEGs at reduced size, crops covers to a box and writes JPEGs.
- `cover_cache.{h,cpp}`: Loads images cropped to a box size from an on-disk JPEG or PNG cache, pruned by age and size.
- `draw.{h,cpp}`: Shared cairo drawing helpers.
- `x_window.{h,cpp}`: Drawable X window.
- `panel_chrome.{h,cpp}`: Shared panel constants and drawing pieces.
- `slider.{h,cpp}`: Draws sliders and maps clicks to percent.
- `panel_window.{h,cpp}`: Popup window for bar panels, anchored to its bar's output.

## `src/service/`

- `i3_service.{h,cpp}`: i3 workspace state and switching, window tree query, window move and kill.
- `output_service.{h,cpp}`: RandR output list and change notifications.
- `settings_service.{h,cpp}`: Creates, reads, writes and watches `settings.conf`, including the bar style.
- `user_service.{h,cpp}`: Display name and uptime text for the settings profile block.
- `wallpaper_service.{h,cpp}`: Reads, writes and watches `wallpaper.conf`.
- `bluetooth_service.{h,cpp}`: Bluetooth adapter and device state and control.
- `network_service.{h,cpp}`: Network state and Wi-Fi control.
- `polkit_service.{h,cpp}`: Polkit authentication agent.
- `notification_service.{h,cpp}`: Desktop notification server.
- `brightness_service.{h,cpp}`: Backlight level and control.
- `audio_service.{h,cpp}`: Audio devices, streams, volume and mute.
- `media_service.{h,cpp}`: Media player state and playback control.
- `battery_service.{h,cpp}`: Battery state.
- `tray_service.{h,cpp}`: StatusNotifierWatcher host, tray items, activation and `dbusmenu` menus.

## `test/`

- `main.cpp`: Unit tests for pure logic.
