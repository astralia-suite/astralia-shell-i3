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
- `build.sh`: Builds, installs dependencies, runs tests, installs or restarts the shell.
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
- `unique_fd.h`: Owning file descriptor.
- `log.{h,cpp}`: Info and error logging to stderr.
- `x_connection.{h,cpp}`: X connection with screen, visuals, atoms, EWMH and RandR outputs.
- `event_loop.{h,cpp}`: Main loop for X events, signals, timers and file descriptors.
- `keyboard.{h,cpp}`: Translates key presses to text and editing keys.
- `async_process.{h,cpp}`: Runs a child process and returns its output on the main thread.
- `spawn.{h,cpp}`: Launches detached shell commands.
- `signal.h`: Change notifications from services to modules.
- `dbus.{h,cpp}`: System and session D-Bus connections, matches, proxies and property reads.

## `src/config/`

- `bar_config.h`: Bar, widget and panel constants.
- `logout_config.h`: Logout overlay constants and actions.
- `launcher_config.h`: Launcher constants and result types.
- `polkit_config.h`: Polkit card constants and prompt texts.
- `notification_config.h`: Notification card and stack constants.
- `osd_config.h`: OSD pill constants and timings.
- `wallpaper_config.h`: Wallpaper config file location and wildcard key.

## `src/modules/`

- `bar.{h,cpp}`: Top bar with logout, workspaces, media, clock and status widgets; opens their panels.
- `launcher.{h,cpp}`: App, file and command launcher overlay.
- `logout.{h,cpp}`: Logout overlay with power action buttons.
- `notification.{h,cpp}`: Desktop notification cards.
- `polkit.{h,cpp}`: Polkit password prompt.
- `osd.{h,cpp}`: Brightness, volume and mic level popup.
- `wallpaper.{h,cpp}`: Per-output desktop wallpaper.

## `src/modules/bar/widget/`

- `bar_widget.h`: Base for bar widgets with the bar's fonts.
- `clock_widget.{h,cpp}`: Clock icon with date and time label.
- `media_widget.{h,cpp}`: Media icon with label.
- `logout_widget.{h,cpp}`: Logout icon with label.
- `workspace_widget.{h,cpp}`: Workspace pill row and click hit-test.
- `bluetooth_widget.{h,cpp}`: Bluetooth icon with device or state label.
- `network_widget.{h,cpp}`: Network icon with SSID label.
- `brightness_widget.{h,cpp}`: Brightness icon with percent label.
- `volume_widget.{h,cpp}`: Volume icon with percent or muted label.
- `battery_widget.{h,cpp}`: Battery icon with percent or plugged-in label.

## `src/modules/bar/panel/`

- `brightness_panel.{h,cpp}`: Brightness slider panel.
- `audio_panel.{h,cpp}`: Output, input and per-app volume, mute and default device panel.
- `battery_panel.{h,cpp}`: Battery state, time left and charge panel.
- `bluetooth_panel.{h,cpp}`: Bluetooth power and device connect, pair and forget panel.
- `media_panel.{h,cpp}`: Now-playing panel with art, track info, position and playback controls.
- `clock_panel.{h,cpp}`: Calendar panel with today's date and month navigation.
- `network_panel.{h,cpp}`: Wi-Fi toggle and network connect, disconnect and forget panel.

## `src/modules/launcher/`

- `app_icon.{h,cpp}`: Finds and loads application icons.
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

## `src/modules/notification/`

- `layout.{h,cpp}`: Notification card sizes, stack position and close hit-test.

## `src/modules/polkit/`

- `layout.{h,cpp}`: Polkit card height and password dot count.

## `src/modules/wallpaper/`

- `config_file.{h,cpp}`: Reads the wallpaper config file.
- `image.{h,cpp}`: Loads and fits wallpaper images.

## `src/render/`

- `text.{h,cpp}`: Measures and draws text.
- `palette.h`: Shared colors, radii and borders.
- `icons.h`: Icon glyph codepoints and level icon pickers.
- `app_fonts.{h,cpp}`: Registers the bundled fonts.
- `image_decode.{h,cpp}`: Decodes raster and SVG images.
- `widget_capsule.{h,cpp}`: Icon and label widget with hover-revealed label.
- `draw.{h,cpp}`: Shared cairo drawing helpers.
- `x_window.{h,cpp}`: Drawable X window.
- `panel_chrome.{h,cpp}`: Shared panel constants and drawing pieces.
- `slider.{h,cpp}`: Draws sliders and maps clicks to percent.
- `panel_window.{h,cpp}`: Popup window for bar panels.

## `src/service/`

- `i3_service.{h,cpp}`: i3 workspace state and switching.
- `bluetooth_service.{h,cpp}`: Bluetooth adapter and device state and control.
- `network_service.{h,cpp}`: Network state and Wi-Fi control.
- `polkit_service.{h,cpp}`: Polkit authentication agent.
- `notification_service.{h,cpp}`: Desktop notification server.
- `brightness_service.{h,cpp}`: Backlight level and control.
- `audio_service.{h,cpp}`: Audio devices, streams, volume and mute.
- `media_service.{h,cpp}`: Media player state and playback control.
- `battery_service.{h,cpp}`: Battery state.

## `test/`

- `main.cpp`: Unit tests for pure logic.
