# Development knowledge

## Description

Hard-won rules from astralia-shell's development.
Can be updated if found new knowledge that supersedes old ones, or genuinely new ones.

## Rule

One statement + One explanation, ≤ 20 words each.
Drop an entry once newer knowledge fully supersedes it.

## Entries

- Wall-clock timers use a `CLOCK_BOOTTIME` `timerfd`. Monotonic clocks pause in suspend, leaving the clock stale after resume.
- Never call `xcb_ewmh_connection_wipe` after `xcb_ewmh_init_atoms_replies` fails. The failure path already frees its internals; wiping double-frees.
- Strings with embedded NULs (e.g. `WM_CLASS`) need the `sv` literal. `std::string_view` from `const char*` stops at the first NUL.
- Take the `flock` single-instance lock before `daemonize()`. The child inherits the open file description, so the lock survives the parent's `_exit`.
- The `kill` IPC client fd is released, not closed. The kernel closes it at process exit, so the client returns only after the shell is gone.
- Under i3, dock windows ignore requested x/y and struts. Inset content inside a full-width ARGB dock instead.
- A 32-bit window needs its own colormap and `XCB_CW_BORDER_PIXEL`. Otherwise `xcb_create_window` fails with `BadMatch` against the root's depth.
- The user's shell is zsh; unquoted `$var` does not word-split. Pass file lists through `find -print0 | xargs -0` instead.
- Icons use the explicit `tabler-icons` Pango family. Tabler codepoints collide with Codicons inside the Nerd Font.
- Call `register_app_fonts()` before the first `Text` exists. Pango's fontconfig map snapshots fonts on creation and misses later app fonts.
- Round-trip before `xcb_disconnect`. Unlike `XCloseDisplay`, it does not sync; Xorg drops unprocessed requests on hang-up, losing exit cleanup.
- Never set `ESETROOT_PMAP_ID` on the shell's own pixmap. Esetroot-style setters `xcb_kill_client` it, which would disconnect the whole shell.
- The wallpaper destructor must reset the root background and delete `_XROOTPMAP_ID`. The root keeps its own pixmap reference, so the image outlives the freed resource.
- Never `cairo_device_finish` a cairo-xcb device on the shell connection. The device is shared per connection; finishing it breaks the bar's surface.
- RandR notify events carry no event window. Route them with `EventLoop::on_event`; root's window handler already belongs to `WorkspaceService`.
- Watch a config file's directory with `inotify`, filtered by name. Editors save by rename, which drops a watch on the file itself.
- `SystemBus` polls both `PollData::fd` and `eventFd`. Sync calls queue signals internally; only `eventFd` wakes the loop for them.
- Spawned children need an empty signal mask and default `SIGPIPE`. The shell blocks signals for `signalfd` and ignores `SIGPIPE`; exec inherits both.
- Overlays take input focus, never an active keyboard grab. A grab blocks the WM's passive hotkey grabs, so toggles never arrive.
- Set xkb modifiers from each key press's `state` field, never track them locally. Overlays opened by a Shift hotkey miss its release.
- Disable cairo MIT-SHM on a probe surface right after connecting. Surfaces copy the SHM flag at creation; its upload pool stays resident (9 MB).
- `cairo_xcb_device_debug_cap_xshm_version` disables SHM only with `-1, -1`. Cairo checks for negative versions; `0, 0` leaves SHM on.
- Decode images with `stb_image` and `resvg`, not gdk-pixbuf. gdk-pixbuf 2.44 goes through glycin: extra threads, sandboxed loaders and megabytes of libraries.
- Call `malloc_trim(0)` after bursts like decodes or broad searches. glibc keeps freed small chunks; a broad launcher query otherwise pins ~8 MB.
- Slow idle `[heap]` creep is glyph-cache warm-up plus fragmentation, not a leak. `heaptrack` showed live heap flat after ~3 minutes idle.
- Drive GLib through an `EventLoop` poll source: prepare, query, poll, check, dispatch. Polkit's helper pipe and GDBus wakeups are dynamic fds; fixed `on_fd` misses them.
- Wipe password buffers with `explicit_bzero` after responding or cancelling. `std::string::clear` leaves the bytes in the heap.
- Hold one sdbus proxy per fixed object; keep changing NetworkManager paths one-off. Caching per-reconnect paths would grow without bound.
- Call `EventLoop::reschedule()` after an event moves a timer's next deadline earlier. Deadlines are recomputed only after firing, so idle timers ignore new work.
- Never destroy an sdbus proxy inside its own async reply callback. Prune per-device proxies from the signal-match handler instead.
- A detached reader thread must own what it touches through a `shared_ptr`. The owning object may be destroyed before the child exits.
