# Development knowledge

## Description

Hard-won rules from astralia-shell's development.
Can be updated if found new knowledge that supersedes old ones, or genuinely new ones.

## Rule

One statement + One explanation, ≤ 20 words each.
Drop an entry once newer knowledge fully supersedes it.

## Entries

- Wall-clock timers use a `CLOCK_BOOTTIME` `timerfd`. Monotonic clocks pause in suspend, leaving the clock stale after resume.
- Never wipe the EWMH connection after `xcb_ewmh_init_atoms_replies` fails. Its failure path already frees everything, so wiping double-frees.
- Strings with embedded NULs, such as `WM_CLASS`, need the `sv` literal. A `std::string_view` from `const char*` stops at the first NUL.
- Take the single-instance `flock` before `daemonize()`. The child inherits the lock, so it survives the parent's exit.
- The `kill` IPC client releases its fd instead of closing it. The kernel closes it at shell exit, so the client returns afterwards.
- Under i3, dock windows ignore requested position and struts. Inset content inside a full-width ARGB dock instead.
- A 32-bit window needs its own colormap and a border pixel. Otherwise window creation fails with `BadMatch` against the root depth.
- The user's shell is zsh, where unquoted `$var` does not word-split. Pass file lists through `find -print0 | xargs -0` instead.
- Icons use the explicit `tabler-icons` Pango family. Tabler codepoints collide with Codicons inside the Nerd Font.
- Call `register_app_fonts()` before the first `Text` exists. Pango snapshots fonts on creation and misses fonts added later.
- Round-trip before `xcb_disconnect`. It does not sync, so Xorg drops unprocessed requests and loses exit cleanup.
- Never set `ESETROOT_PMAP_ID` on the shell's own pixmap. Wallpaper setters kill its owner, which would disconnect the whole shell.
- The wallpaper destructor must reset the root background and delete `_XROOTPMAP_ID`. Otherwise the root keeps the image alive.
- Never `cairo_device_finish` a cairo-xcb device on the shell connection. The device is shared, so finishing it breaks the bar.
- RandR notify events carry no event window. Route them with `EventLoop::on_event`, since the root handler belongs to the workspace service.
- Watch a config file's directory with `inotify`, filtered by name. Editors save by rename, which drops a watch on the file.
- `SystemBus` polls both the bus fd and its `eventFd`. Sync calls queue signals internally, and only `eventFd` wakes the loop.
- Spawned children need an empty signal mask and default `SIGPIPE`. The shell blocks and ignores signals, and exec inherits both.
- Overlays take input focus, never an active keyboard grab. A grab blocks the window manager's hotkeys, so toggles never arrive.
- Read xkb modifiers from each key press's `state` field. Overlays opened by a Shift hotkey miss its release.
- Disable cairo MIT-SHM on a probe surface right after connecting. Surfaces copy the flag at creation, and its pool stays resident.
- Disable cairo SHM with version `-1, -1`. Cairo checks for negative versions, so `0, 0` leaves SHM on.
- Decode images with `stb_image` and `resvg`, not gdk-pixbuf. gdk-pixbuf pulls in sandboxed loaders, extra threads and megabytes of libraries.
- Call `malloc_trim(0)` after bursts like decodes or broad searches. glibc keeps freed small chunks, pinning several megabytes otherwise.
- Slow idle heap growth is glyph-cache warm-up plus fragmentation, not a leak. `heaptrack` showed live heap flat after three idle minutes.
- Drive GLib through an `EventLoop` poll source. Polkit and GDBus use changing fds that fixed fd watches miss.
- Wipe password buffers with `explicit_bzero` after responding or cancelling. `std::string::clear` leaves the bytes in the heap.
- Hold one sdbus proxy per fixed object, but keep changing NetworkManager paths one-off. Caching per-reconnect paths grows without bound.
- `AudioService::changed` also fires `AudioKind::nodes` after every volume change. Match each kind explicitly, never with a sink-or-else fallback.
- Call `EventLoop::reschedule()` when an event moves a timer's deadline earlier. Deadlines are only recomputed after firing.
- Never destroy an sdbus proxy inside its own async reply callback. Prune per-device proxies from the signal-match handler instead.
- A detached reader thread must own what it touches through a `shared_ptr`. The owner may be destroyed before the child exits.
- Map a popup beside a panel with `show(false)`, never focus. Taking focus fires the panel's focus-out close; its `owner_events` grab still routes clicks.
- SNI items signal `NewIcon`/`NewStatus`, rarely `PropertiesChanged`. Refetch `GetAll` on those signals, or icons go stale.
- Hide bars for unplugged or disabled outputs, never destroy them. `EventLoop` cannot remove windows or timers, so they would dangle.
- Let one `OutputService` own the RandR notify. `EventLoop::on_event` keeps a single handler per type, so a second owner replaces it.
- Compare parsed config before emitting changes. A service's own atomic write also trips its `inotify` watch.
- Decode wallpaper thumbnails off the main thread, through the cover cache. A full `4K` decode takes about half a second.
- Set `M_ARENA_MAX` to 1 and a fixed `M_MMAP_THRESHOLD`. A decode thread's private arena kept about 50 MB after the tab closed.
- Wrap decoded `stb_image` pixels in the cairo surface in place. Copying doubled the transient peak of large wallpapers.
- Decode JPEGs with `libjpeg` scale denominators of 2, 4 or 8. Reduced-size decoding skips most pixels and memory.
- Touch a cache file's mtime on every hit. Pruning treats mtime as last use, so wallpapers in use survive.
- Cache opaque covers as JPEG and transparent ones as PNG. JPEG writes faster and is far smaller on the X201's slow disk.
- Keep thumbnails as X-side surfaces made with `cairo_surface_create_similar` on first paint. Image surfaces upload to the X server on every paint.
- Batch repaints from worker results through a timer, and reschedule only when no repaint is pending. Rescheduling on each arrival delays the paint while results keep coming.
- Run the thumbnail worker at nice `10`. On the X201's two cores, decoding otherwise competes with the UI thread.
- A nested `Xephyr` shows no cursor and ignores XTest clicks for the shell. Click inside its window by hand.
