# Plan: overview module (i3)

Port of `astralia-shell-hl/src/modules/overview.cpp` without screen capture. Capture is deferred because the ThinkPad X201 cannot sustain XComposite previews.

## Decisions

- Opened by the IPC verb `overview` and by a new bar button using `icon::overview`.
- Grid is 2 rows by 5 columns, 10 workspaces per page, as in `hl`.
- App icons are ported: `hl` `resolve_window_icon_path` (window class to `.desktop` `Icon`/`StartupWMClass`) joins `render/app_icon`.

## Scope

- Included: workspace grid, paging, window tiles as placeholder rects with app icons, click to switch, drag a tile to another workspace, arrow and digit navigation, `Escape` to close, `d` to close the selected workspace windows.
- Deferred: live previews (`xcb-composite`, `xcb-damage`), animations, global (all outputs) mode, `Shift`/`Alt` workspace variants (the `i3` `KeyEvent` has no modifiers).
- Event-driven only: no polling timer, repaint on open, input, and window or workspace changes.

## Data source

Window geometry comes from the i3 IPC `GET_TREE` reply, which is JSON and has no parser in the project. A small `core/json` parser is added. EWMH was rejected because i3 does not keep valid frame geometry for windows on hidden workspaces.

## Files

- `[NEW]` `src/core/json.{h,cpp}`: minimal JSON value and parser.
- `[MODIFY]` `src/service/i3_service.{h,cpp}`:
  - Request/reply helper over the existing socket code.
  - `I3Window { id, window_class, rect, workspace, floating, fullscreen }`, `windows()`, `refresh_windows()`.
  - `move_window(id, workspace)`, `kill_window(id)`.
- `[MODIFY]` `src/render/app_icon.{h,cpp}`: add `resolve_window_icon_path`.
- `[NEW]` `src/config/overview_config.h`: grid, scale, padding, rounding, border constants from `hl`.
- `[NEW]` `src/modules/overview/layout.{h,cpp}`: pure grid math (`workspace_id_at`, `compute_layout`, `cell_at`, tile rect scaling).
- `[NEW]` `src/modules/overview.{h,cpp}`: `Overview` class modeled on `Logout`, with pointer drag, keys and cairo painting.
- `[NEW]` `src/modules/bar/widget/overview_widget.{h,cpp}`: bar button.
- `[MODIFY]` `src/modules/bar.{h,cpp}`: lay out and click-dispatch the overview button.
- `[MODIFY]` `src/main.cpp`, `meson.build`: construct `Overview`, register sources.
- `[MODIFY]` `test/`: tests for `json`, `layout`, `resolve_window_icon_path` order.
- `[MODIFY]` `important/index.md`, `important/knowledge.md`.

## Differences from `hl`

- No animations: the indicator and tiles jump to their targets.
- Closing windows uses i3 `kill` per container id.
- Drag-drop uses `[con_id=ID] move container to workspace number N`.
- Tiles are drawn as placeholders with the app icon.

## Verification

- `./build.sh test` passes.
- Manual run under i3, then on the X201.
