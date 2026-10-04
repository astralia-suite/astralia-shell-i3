#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <print>
#include <set>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/cli.h"
#include "core/config_file.h"
#include "core/ipc.h"
#include "core/runtime_paths.h"

#include "modules/bar/panel/battery_panel.h"
#include "modules/bar/panel/clock_panel.h"
#include "modules/bar/panel/network_panel.h"
#include "modules/bar/panel/tray_panel.h"
#include "modules/bar/widget/battery_widget.h"
#include "modules/bar/widget/bluetooth_widget.h"
#include "modules/bar/widget/clock_widget.h"
#include "modules/bar/widget/network_widget.h"
#include "modules/bar/widget/workspace_widget.h"
#include "modules/launcher/apps_provider.h"
#include "modules/launcher/desktop_entry.h"
#include "modules/launcher/files_provider.h"
#include "modules/launcher/launch_action.h"
#include "modules/launcher/search.h"
#include "modules/launcher/submenu.h"
#include "modules/launcher/visit_store.h"
#include "modules/logout/layout.h"
#include "modules/notification/layout.h"
#include "modules/polkit/layout.h"
#include "modules/settings/layout.h"

#include "render/cover_cache.h"
#include "render/image_decode.h"

#include "service/settings_service.h"
#include "service/wallpaper_service.h"

#include "render/app_icon.h"
#include "render/icons.h"
#include "render/palette.h"
#include "render/panel_chrome.h"
#include "render/slider.h"

#include "service/audio_service.h"
#include "service/bluetooth_service.h"
#include "service/media_service.h"
#include "service/network_service.h"
#include "service/tray_service.h"

namespace {

int failures = 0;

void check(bool condition, std::string_view what,
           std::source_location where = std::source_location::current()) {
    if (condition) {
        return;
    }
    ++failures;
    std::println(stderr, "FAIL {}:{}: {}", where.file_name(), where.line(), what);
}

std::chrono::system_clock::time_point at_noon_plus(std::chrono::nanoseconds offset) {
    using namespace std::chrono;
    return sys_days{2026y / September / 27} + 12h + offset;
}

void check_ms_until_next_second() {
    using namespace std::chrono_literals;
    using astralia::ms_until_next_second;
    check(ms_until_next_second(at_noon_plus(0s)) == 1s, "on the boundary waits a full second");
    check(ms_until_next_second(at_noon_plus(250ms)) == 750ms, "mid-second waits the remainder");
    check(ms_until_next_second(at_noon_plus(999ms)) == 1ms, "last millisecond waits 1 ms");
    check(ms_until_next_second(at_noon_plus(999500us)) == 1ms,
          "sub-millisecond remainder rounds up");
    check(ms_until_next_second(at_noon_plus(500us)) == 1s,
          "just past the boundary rounds up to a full second");
}

astralia::Invocation parse(std::vector<std::string_view> args) {
    return astralia::parse_invocation(args);
}

void check_parse_invocation() {
    using astralia::Mode;
    check(parse({}).mode == Mode::daemon, "no arguments starts the daemon");
    check(parse({"debug"}).mode == Mode::debug, "debug runs in the foreground");
    astralia::Invocation kill = parse({"kill"});
    check(kill.mode == Mode::client && kill.command == "kill", "other verbs go to the client");
    astralia::Invocation joined = parse({"debug", "now"});
    check(joined.mode == Mode::client && joined.command == "debug now",
          "multiple arguments are joined with spaces");
}

void check_runtime_path() {
    using astralia::runtime_path;
    check(runtime_path(":1", "/run/user/1000", ".sock") == "/run/user/1000/astralia-shell-1.sock",
          "display number names the socket");
    check(runtime_path(":0.0", "/run/user/1000", ".lock") ==
              "/run/user/1000/astralia-shell-0.0.lock",
          "screen number is kept");
    check(runtime_path("/tmp/launch/org:0", "", ".log") ==
              "/tmp/astralia-shell-_tmp_launch_org0.log",
          "slashes become underscores and an empty runtime dir falls back to /tmp");
    check(runtime_path("", "/run/user/1000", ".sock") == "/run/user/1000/astralia-shell.sock",
          "unset display drops the suffix");
}

void check_format_help() {
    std::vector<astralia::IpcHandler> handlers{
        {"kill", {}, "quit"},
        {"help", {}, "list verbs"},
        {"reload", {}, "reload config"},
    };
    check(astralia::format_help(handlers) == "astralia-shell <verb>:\n"
                                             "  help    list verbs\n"
                                             "  kill    quit\n"
                                             "  reload  reload config\n",
          "help is sorted and aligned");
}

void check_color() {
    constexpr astralia::Color opaque = astralia::color("#FF8000");
    check(opaque.r == 1.0f && opaque.g == 128 / 255.0f && opaque.b == 0.0f && opaque.a == 1.0f,
          "six hex digits parse as opaque");
    constexpr astralia::Color translucent = astralia::color("0a061480");
    check(translucent.r == 10 / 255.0f && translucent.a == 128 / 255.0f,
          "eight hex digits carry alpha without a leading #");
}

void check_status_icons() {
    namespace icon = astralia::icon;
    using astralia::NetworkKind;
    check(astralia::bluetooth_icon({true, false, true, ""}) == icon::bluetooth_off,
          "unpowered is off");
    check(astralia::bluetooth_icon({true, true, true, ""}) == icon::bluetooth_connected,
          "a connected device shows connected");
    check(astralia::bluetooth_label({}).empty(), "no adapter has no label");
    check(astralia::bluetooth_label({true, false, false, ""}) == "Disabled", "unpowered label");
    check(astralia::bluetooth_label({true, true, false, ""}) == "Idle", "powered idle label");
    check(astralia::bluetooth_label({true, true, true, "Buds"}) == "Buds", "device name label");
    check(astralia::network_icon({NetworkKind::wifi, 80, "home", false}) == icon::wifi,
          "strong Wi-Fi is full");
    check(astralia::network_icon({NetworkKind::wifi, 30, "home", false}) == icon::wifi1,
          "weak Wi-Fi is one bar");
    check(astralia::network_icon({NetworkKind::ethernet, 0, "", false}) == icon::router,
          "ethernet is a router");
    check(astralia::network_icon({NetworkKind::wifi, 80, "cafe", true}) == icon::lock,
          "a captive portal wins over signal");
    check(astralia::network_icon({}) == icon::wifi_off, "no connection is off");
    check(astralia::battery_icon({true, 20, false, false}) == icon::battery1, "low battery");
    check(astralia::battery_icon({true, 76, false, false}) == icon::battery4, "high battery");
    check(astralia::battery_icon({true, 40, true, false}) == icon::battery_charging, "charging");
    check(astralia::battery_icon({true, 100, false, true}) == icon::plugged_in, "full");
    check(astralia::battery_label({true, 57, false, false}) == "57%", "percent label");
    check(astralia::battery_label({true, 100, false, true}) == "Plugged in", "full label");
}

void check_status_changes() {
    using astralia::NetworkKind;
    using astralia::StatusMessage;
    using Messages = std::vector<StatusMessage>;
    astralia::NetworkStatus none;
    astralia::NetworkStatus home{NetworkKind::wifi, 80, "home", false};
    astralia::NetworkStatus cafe{NetworkKind::wifi, 60, "cafe", false};
    astralia::NetworkStatus portal{NetworkKind::wifi, 60, "cafe", true};
    astralia::NetworkStatus wired{NetworkKind::ethernet, 0, "", false};
    check(astralia::network_changes(home, home).empty(), "unchanged network sends nothing");
    check(astralia::network_changes(home, {NetworkKind::wifi, 30, "home", false}).empty(),
          "signal change sends nothing");
    check(astralia::network_changes(none, home) == Messages{{"Connected", "Connected to home"}},
          "Wi-Fi connect");
    check(astralia::network_changes(home, none) ==
              Messages{{"Disconnected", "Disconnected from home"}},
          "Wi-Fi disconnect");
    check(astralia::network_changes(home, cafe) == Messages{{"Connected", "Connected to cafe"}},
          "Wi-Fi switch");
    check(astralia::network_changes(none, wired) ==
              Messages{{"Connected", "Connected via Ethernet"}},
          "Ethernet up");
    check(astralia::network_changes(wired, none) ==
              Messages{{"Disconnected", "Ethernet disconnected"}},
          "Ethernet down");
    check(astralia::network_changes(cafe, portal) ==
              Messages{{"Captive Portal", "Sign in required for cafe"}},
          "captive portal");
    astralia::BluetoothStatus idle{true, true, false, ""};
    astralia::BluetoothStatus buds{true, true, true, "Buds"};
    check(astralia::bluetooth_changes(idle, idle).empty(), "unchanged Bluetooth sends nothing");
    check(astralia::bluetooth_changes(idle, {true, false, false, ""}).empty(),
          "power off sends nothing");
    check(astralia::bluetooth_changes(idle, buds) == Messages{{"Connected", "Connected to Buds"}},
          "Bluetooth connect");
    check(astralia::bluetooth_changes(buds, idle) ==
              Messages{{"Disconnected", "Disconnected from Buds"}},
          "Bluetooth disconnect");
}

void check_workspace_row() {
    using astralia::workspace_at;
    astralia::I3Status status{3, 1};
    check(astralia::workspace_row_width(status) == 10 + 6 + 26 + 6 + 10, "active pill is wider");
    check(workspace_at(status, 0) == 0u, "first pill");
    check(workspace_at(status, 20) == 1u, "active pill");
    check(workspace_at(status, 50) == 2u, "last pill");
    check(!workspace_at(status, 100), "past the row");
    check(astralia::workspace_row_width({}) == 0, "no workspaces is empty");
}

void check_cover() {
    using astralia::cover;
    astralia::Placement wide = cover(200, 100, 100, 100);
    check(wide.scale == 1.0 && wide.x == -50.0 && wide.y == 0.0,
          "a wider image crops left and right");
    astralia::Placement tall = cover(100, 400, 200, 200);
    check(tall.scale == 2.0 && tall.x == 0.0 && tall.y == -300.0,
          "a taller image scales up and crops top and bottom");
    astralia::Placement exact = cover(640, 360, 1280, 720);
    check(exact.scale == 2.0 && exact.x == 0.0 && exact.y == 0.0, "same aspect fills exactly");
}

void check_wallpaper_file() {
    using astralia::image_for;
    astralia::WallpaperFile file = astralia::parse_wallpaper_file("# comment\n"
                                                                  "\n"
                                                                  "LVDS-1 = ~/laptop.jpg\n"
                                                                  "  VGA-1=/w/wide file.png  \n"
                                                                  "broken line\n"
                                                                  "* = /w/default.jpg\r\n"
                                                                  "HDMI-1 =",
                                                                  "/home/u");
    check(image_for(file, "LVDS-1") == "/home/u/laptop.jpg", "named output, home expanded");
    check(image_for(file, "VGA-1") == "/w/wide file.png", "spaces trimmed, inner kept");
    check(image_for(file, "DP-1") == "/w/default.jpg", "unlisted output uses *");
    check(file.invalid_lines == std::vector<std::size_t>{5, 7}, "malformed lines reported");
    check(!image_for(astralia::parse_wallpaper_file("", "/home/u"), "DP-1"),
          "empty file has no image");
}

void check_cover_cache() {
    using astralia::cover_cache_name;
    check(astralia::jpeg_reduction(0.0266) == 8, "a tiny target decodes at one eighth");
    check(astralia::jpeg_reduction(0.125) == 8, "an exact eighth is allowed");
    check(astralia::jpeg_reduction(0.2) == 4, "a fifth needs a quarter");
    check(astralia::jpeg_reduction(0.5) == 2, "a half is allowed");
    check(astralia::jpeg_reduction(0.6) == 1, "above a half decodes in full");
    check(astralia::jpeg_reduction(1.0) == 1, "full scale decodes in full");

    constexpr int64_t day = 24 * 3600;
    constexpr uintmax_t mib = 1024 * 1024;
    using Entries = std::vector<astralia::CoverCacheEntry>;
    check(astralia::cover_cache_expired({}, 1000 * day).empty(), "an empty cache expires nothing");
    check(astralia::cover_cache_expired(Entries{{"a.png", mib, 1000 * day}, {"b.png", mib, 999 * day}}, 1000 * day).empty(), "recent entries under the cap stay");
    check(astralia::cover_cache_expired(Entries{{"old.png", mib, 900 * day}, {"new.png", mib, 999 * day}}, 1000 * day) == std::vector<std::string>{"old.png"}, "entries unused for 90 days expire");
    check(astralia::cover_cache_expired(Entries{{"a.png", 70 * mib, 990 * day}, {"b.png", 70 * mib, 995 * day}, {"c.png", 70 * mib, 999 * day}}, 1000 * day) == std::vector<std::string>{"a.png", "b.png"}, "the least recently used go first over the cap");
    check(astralia::cover_cache_expired(Entries{{"x.1.tmp", mib, 1000 * day - 7200}, {"y.2.tmp", mib, 1000 * day - 60}}, 1000 * day) == std::vector<std::string>{"x.1.tmp"}, "only stale temporary files expire");

    std::string name = cover_cache_name("/w/a.jpg", 100, 5, 1920, 1200);
    check(name.size() == 20 && name.ends_with(".png"), "cache names are a hash and .png");
    check(name == cover_cache_name("/w/a.jpg", 100, 5, 1920, 1200), "the same input gives the same name");
    check(name != cover_cache_name("/w/b.jpg", 100, 5, 1920, 1200), "a different path changes the name");
    check(name != cover_cache_name("/w/a.jpg", 101, 5, 1920, 1200), "a different size changes the name");
    check(name != cover_cache_name("/w/a.jpg", 100, 6, 1920, 1200), "a new mtime changes the name");
    check(name != cover_cache_name("/w/a.jpg", 100, 5, 1920, 1080), "a different target changes the name");
}

void check_config_file() {
    using astralia::config_file_path;
    check(config_file_path("/cfg", "/home/u", "astralia-shell/wallpaper.conf") ==
              "/cfg/astralia-shell/wallpaper.conf",
          "XDG_CONFIG_HOME wins");
    check(config_file_path("", "/home/u", "astralia-shell/wallpaper.conf") ==
              "/home/u/.config/astralia-shell/wallpaper.conf",
          "empty XDG_CONFIG_HOME falls back to ~/.config");
    check(astralia::expand_home("~/a.png", "/home/u") == "/home/u/a.png", "~/ expands");
    check(astralia::expand_home("/a/~b.png", "/home/u") == "/a/~b.png", "inner ~ is kept");

    astralia::ConfigLines lines = astralia::parse_config_lines("# c\n a = 1 \nbad\n=x\nb=\n");
    check(lines.entries.size() == 1 && lines.entries[0].key == "a" && lines.entries[0].value == "1" &&
              lines.entries[0].line == 2,
          "entries are trimmed and numbered");
    check(lines.invalid == std::vector<std::size_t>{3, 4, 5}, "bad lines are reported");

    std::string text = "# keep\na = 1\nother = 2\n";
    check(astralia::with_entry(text, "a", "9") == "# keep\na = 9\nother = 2\n",
          "an existing key is replaced in place");
    check(astralia::with_entry(text, "c", "3") == "# keep\na = 1\nother = 2\nc = 3\n",
          "a new key is appended");
    check(astralia::with_entry("", "a", "1") == "a = 1\n", "an empty file gains the key");
    check(astralia::without_entry(text, "a") == "# keep\nother = 2\n", "a key is removed");
    check(astralia::without_entry(text, "zzz") == text, "a missing key changes nothing");
}

void check_settings_file() {
    using astralia::Feature;
    astralia::SettingsFile file = astralia::parse_settings_file(astralia::settings_config::default_text);
    check(file.invalid_lines.empty(), "the default text parses cleanly");
    check(astralia::feature_enabled(file, Feature::bar, "DP-1") &&
              astralia::feature_enabled(file, Feature::osd, "DP-1") &&
              astralia::feature_enabled(file, Feature::notifications, "DP-1"),
          "defaults enable every feature");
    check(file.wallpaper_dir == "~/Pictures", "default wallpaper folder");

    file = astralia::parse_settings_file("bar = off\nHDMI-A-1.bar = on\nHDMI-A-1.osd = off\n"
                                         "wallpaper_dir = /w\nosd = maybe\nfoo.baz = on\n.bar = on\n");
    check(!astralia::feature_enabled(file, Feature::bar, "DP-1"), "the default can be off");
    check(astralia::feature_enabled(file, Feature::bar, "HDMI-A-1"), "an override beats the default");
    check(!astralia::feature_enabled(file, Feature::osd, "HDMI-A-1"), "an override can turn off");
    check(astralia::feature_enabled(file, Feature::osd, "DP-1"), "other outputs keep the default");
    check(file.wallpaper_dir == "/w", "wallpaper folder is read");
    check(file.invalid_lines == std::vector<std::size_t>{5, 6, 7}, "bad values and keys are reported");
    check(astralia::feature_key(Feature::notifications, "") == "notifications", "default key");
    check(astralia::feature_key(Feature::osd, "DP-1") == "DP-1.osd", "override key");
}

void check_settings_layout() {
    using namespace astralia;
    check(settings_window_size(1920, 1080) == std::pair<int, int>{860, 540}, "a large output caps the card");
    check(settings_window_size(600, 400) == std::pair<int, int>{520, 320}, "a small output leaves a margin");
    SettingsGeometry geometry = settings_geometry(760, 540);
    check(geometry.content.x == 190 && geometry.content.w == 550 && geometry.content.h == 500,
          "content sits right of the rail inside the padding");
    check(settings_tab_at(geometry, 20, geometry.rail.y + 12 + 44 + 10) == 0u, "first tab");
    check(settings_tab_at(geometry, 20, geometry.rail.y + 12 + 44 + 36 + 4 + 10) == 1u, "second tab");
    check(!settings_tab_at(geometry, 20, 5), "above the tabs");
    check(!settings_tab_at(geometry, 400, 70), "right of the rail");

    std::vector<PanelRect> chips = settings_chip_rects(3, {10, 20, 492, 400});
    check(chips.size() == 3 && chips[0].x == 10 && chips[0].w == 160 && chips[1].x == 176 &&
              chips[2].x + chips[2].w == 502 && chips[2].y == 20,
          "chips split the row evenly and span it");
    check(settings_chip_rects(0, {0, 0, 100, 100}).empty(), "no chips for no outputs");

    PanelRect grid{0, 100, 635, 300};
    check(settings_grid_content_height(0) == 0.0, "no images have no height");
    check(settings_grid_width() == 635.0, "five columns with gaps");
    check(settings_grid_content_height(5) == 115.0, "one row");
    check(settings_grid_content_height(6) == 245.0, "two rows");
    check(settings_clamp_scroll(-30, 30, 300) == 0.0, "scroll stops at the top");
    check(settings_clamp_scroll(9999, 30, 300) == 6 * 130.0 - 15.0 - 300.0, "scroll stops at the bottom");
    check(settings_clamp_scroll(50, 5, 300) == 0.0, "short content never scrolls");
    PanelRect second = settings_grid_cell(grid, 6, 0);
    check(second.x == 130.0 && second.y == 230.0, "cell 6 is on the second row");
    check(settings_grid_cell(grid, 6, 100).y == 130.0, "scrolling moves cells up");
    check(settings_grid_visible(grid, 30, 0) == std::pair<std::size_t, std::size_t>{0, 15},
          "three rows are visible at the top");
    check(settings_grid_visible(grid, 10, 0) == std::pair<std::size_t, std::size_t>{0, 10},
          "visible range stops at the count");
    check(settings_grid_visible(grid, 0, 0) == std::pair<std::size_t, std::size_t>{0, 0}, "empty grid");
    check(settings_grid_cell_at(grid, 30, 0, 5, 105) == 0u, "hit the first cell");
    check(!settings_grid_cell_at(grid, 30, 0, 120, 105), "the gap is no cell");
    check(settings_grid_cell_at(grid, 30, 0, 135, 235) == 6u, "hit a cell on the second row");
    check(!settings_grid_cell_at(grid, 30, 0, 5, 50), "outside the grid");
}

void check_launcher_text() {
    check(astralia::elide("abcdef", 4) == "abc…", "elide keeps max - 1 chars");
    check(astralia::elide("héllo", 5) == "héllo", "elide counts UTF-8 chars");
    check(astralia::elide_middle("abcdefghij", 5) == "ab…ij", "elide_middle keeps both ends");
    check(astralia::collapse_home("/home/u/docs", "/home/u") == "~/docs", "home collapses");
    check(astralia::collapse_home("/home/user2", "/home/u") == "/home/user2",
          "prefix without slash is kept");
    check(astralia::basename_of("/a/b/") == "b" && astralia::basename_of("/") == "/",
          "basename_of");
    check(astralia::parent_of("/a/b") == "/a" && astralia::parent_of("/a") == "/", "parent_of");
}

void check_launcher_modes() {
    using astralia::detect_mode_and_query;
    using astralia::LauncherMode;
    check(detect_mode_and_query("  fire").mode == LauncherMode::drun, "plain text is drun");
    check(detect_mode_and_query("> ls -l").query == "ls -l", "> is run mode");
    check(detect_mode_and_query("gg cats").mode == LauncherMode::google, "gg is google");
    check(detect_mode_and_query("ggx").mode == LauncherMode::drun, "word prefix needs a space");
    check(detect_mode_and_query("yt").mode == LauncherMode::youtube, "bare prefix switches mode");
    check(detect_mode_and_query("url example.com").query == "example.com", "url query");
}

void check_launcher_scoring() {
    check(astralia::score_app("Firefox", "fire") > astralia::score_app("Wildfire", "fire"),
          "prefix match scores higher");
    check(astralia::score_app("Firefox", "zz") < 0.0f, "no match is negative");
    check(astralia::to_glob_pattern(" notes ") == "**/*notes*", "glob wraps a bare word");
    check(astralia::to_glob_pattern("a/b") == "**/a/b", "glob anchors a path");
    check(astralia::split_query_parts("Foo * bar") == std::vector<std::string>{"foo", "bar"},
          "query splits on *");
    check(astralia::score_path("report.pdf", "rep*pdf") > 0.0f, "ordered parts match");
    check(astralia::score_path("report.pdf", "pdf*rep") < 0.0f, "out of order parts fail");
    std::vector<astralia::FileEntry> parsed =
        astralia::fd_search_parse_output("/a/b\n\n  /c/d  \n", true);
    check(parsed.size() == 2 && parsed[1].name == "d" && parsed[1].is_dir, "fd output parses");
}

void check_desktop_entry() {
    std::istringstream in("[Desktop Entry]\nType=Application\nName=Foo\nName=Ignored\n"
                          "Exec=foo %U\nIcon=foo\nTerminal=true\n[Desktop Action x]\nName=X\n");
    auto entry = astralia::parse_desktop_entry(in, "foo.desktop");
    check(entry && entry->name == "Foo" && entry->terminal && entry->icon == "foo",
          "desktop entry parses the main section");
    std::istringstream link("[Desktop Entry]\nType=Link\nName=L\nExec=l\n");
    check(!astralia::parse_desktop_entry(link, "l.desktop"), "non-applications are skipped");
    check(astralia::strip_exec_field_codes("foo %U --x %% %f") == "foo --x %",
          "field codes are stripped");
    check(astralia::desktop_entry_dirs(nullptr, "/x:/y", "/home/u") ==
              std::vector<std::string>{"/home/u/.local/share/applications", "/x/applications",
                                       "/y/applications"},
          "desktop dirs follow XDG");
    check(astralia::icon_theme_order("Papirus").front() == "Papirus" &&
              astralia::icon_theme_order("").back() == "hicolor",
          "icon theme order");
}

void check_launch_urls() {
    check(astralia::normalize_url("example.com") == "http://example.com", "bare host");
    check(astralia::normalize_url("https://a.b/c") == "https://a.b/c", "scheme kept");
    check(astralia::normalize_url("localhost:8080") == "http://localhost:8080", "localhost");
    check(astralia::normalize_url("two words") == "", "spaces are not a URL");
    check(astralia::make_search_url(" a b ", "q=") == "q=a%20b", "search URL encodes");
    check(astralia::shell_quote("it's") == "'it'\\''s'", "shell quoting");
    astralia::DesktopEntry htop{"htop.desktop", "htop", "htop %f", "", true, false, false};
    check(astralia::app_command(htop) == "terminal htop", "terminal apps use the wrapper");
}

void check_drun_results() {
    astralia::DesktopEntry a{"a.desktop", "Alpha", "a", "", false, false, false};
    astralia::DesktopEntry b{"b.desktop", "Beta", "b", "", false, false, false};
    std::vector<astralia::ScoredApp> apps{{&a, 900.0f}, {&b, 100.0f}};
    std::vector<astralia::FileEntry> files{{"f", "/f", false, 999.0f}, {"d", "/d", true, 1.0f}};
    astralia::VisitStore visits;
    visits.counts[astralia::visit_store_app_key("b.desktop")] = 3;
    auto results = astralia::combined_drun_results(apps, files, visits, 3);
    check(results.size() == 3, "results are capped");
    check(results[0].app == &b && results[1].app == &a, "visits outrank score among apps");
    check(results[2].kind == astralia::DrunResult::Kind::dir, "dirs come before files");
    check(astralia::visit_store_get(visits, "file:/none") == 0, "unknown key has no visits");
    check(astralia::visit_store_path("", "/home/u") ==
              "/home/u/.local/state/astralia-shell/launcher_visits",
          "visit store path");
}

void check_submenu() {
    astralia::DirLister lister = [](const std::string &path, bool dirs) {
        return std::vector<astralia::FileEntry>{{dirs ? "sub" : "file", path + "/x", dirs, 0.0f}};
    };
    astralia::SubmenuState s;
    astralia::submenu_open_directory(s, "/a/b", lister);
    check(s.screen == astralia::SubmenuScreen::browse && s.items.size() == 4,
          "browse lists actions, dirs and files");
    astralia::submenu_handle_entry(s, s.items[1], lister);
    check(s.current_path == "/a", "previous directory goes up");
    astralia::submenu_handle_entry(s, s.items[3], lister);
    check(s.screen == astralia::SubmenuScreen::file_actions && s.came_from_browse,
          "a file opens its actions");
    astralia::submenu_go_back(s, lister);
    check(s.screen == astralia::SubmenuScreen::browse && s.current_path == "/a",
          "back from file actions returns to its directory");
    astralia::submenu_go_back(s, lister);
    check(s.screen == astralia::SubmenuScreen::search, "back from browse closes");
}

void check_logout_layout() {
    using astralia::logout_button_at;
    using astralia::logout_button_center;
    astralia::Point center{500.0, 400.0};
    astralia::Point top = logout_button_center(0, center);
    check(std::abs(top.x - 500.0) < 1e-6 && std::abs(top.y - 100.0) < 1e-6,
          "first button sits straight above the center");
    astralia::Point right = logout_button_center(2, center);
    check(std::abs(right.x - 800.0) < 1e-6 && std::abs(right.y - 400.0) < 1e-6,
          "third button sits right of the center");
    check(logout_button_at({500.0, 100.0}, center) == 0, "button center hits");
    check(logout_button_at({554.0, 154.0}, center) == 0, "button corner hits");
    check(!logout_button_at(center, center), "logo center hits no button");
}

void check_polkit_layout() {
    check(astralia::utf8_length("") == 0, "empty utf8 length");
    check(astralia::utf8_length("pass") == 4, "ascii utf8 length");
    check(astralia::utf8_length("mật") == 3, "multibyte utf8 length");
    check(astralia::polkit_card_height(false) == 183.0, "card height without info");
    check(astralia::polkit_card_height(true) == 213.0, "card height with info");
    check(astralia::polkit_visible_dots(5, 404.0) == 5, "dots fit");
    check(astralia::polkit_visible_dots(40, 404.0) == 25, "dots clamp to width");
    check(astralia::polkit_visible_dots(3, -1.0) == 0, "no width shows no dots");
}

void check_notification_layout() {
    check(astralia::notification_card_height(20.0, 0.0, 0.0) == 60.0, "card height with app name only");
    check(astralia::notification_card_height(20.0, 24.0, 0.0) == 94.0, "card height with title");
    check(astralia::notification_card_height(20.0, 24.0, 40.0) == 144.0, "card height with all fields");
    std::vector<double> tall{100.0, 200.0, 200.0};
    check(astralia::notification_fit_count(tall) == 2, "oldest card beyond 480 px is dropped");
    std::vector<double> huge{600.0};
    check(astralia::notification_fit_count(huge) == 1, "newest card always shows");
    check(astralia::notification_fit_count({}) == 0, "no cards fit nothing");
    std::vector<double> heights{50.0, 72.0};
    check(astralia::notification_stack_height(heights) == 130.0, "stack height adds spacing");
    check(astralia::notification_stack_height({}) == 0.0, "empty stack has no height");
    astralia::StackOrigin origin = astralia::notification_stack_origin({0, 0, 1920, 1080}, 130.0);
    check(origin.x == 1920 - 10 - 400 && origin.y == 1080 - 10 - 130,
          "stack sits 10 px from the bottom-right corner");
    astralia::StackOrigin offset = astralia::notification_stack_origin({1920, 0, 1280, 1024}, 50.0);
    check(offset.x == 1920 + 1280 - 410 && offset.y == 1024 - 60, "stack follows output offset");
    check(astralia::notification_close_at(390.0, 0.0, heights) == 0, "top card x hits");
    check(astralia::notification_close_at(390.0, 60.0, heights) == 1, "second card x hits");
    check(!astralia::notification_close_at(200.0, 0.0, heights), "card body hits nothing");
    check(!astralia::notification_close_at(390.0, 45.0, heights), "below the x hits nothing");
    check(!astralia::notification_close_at(390.0, 130.0, heights), "below the stack hits nothing");
}

} // namespace

void check_slider() {
    using astralia::audio_percent;
    using astralia::slider_percent_at;
    std::array<float, 2> forty{0.064f, 0.064f};
    check(audio_percent(forty) == 40, "audio equal channels");
    std::array<float, 2> uneven{0.0f, 1.0f};
    check(audio_percent(uneven) == 50, "audio uneven channels average");
    std::array<float, 1> full{1.0f};
    check(audio_percent(full) == 100, "audio full volume");
    check(audio_percent({}) == 0, "audio no channels");
    check(slider_percent_at(10, 200, 10) == 0, "slider left edge");
    check(slider_percent_at(10, 200, 210) == 100, "slider right edge");
    check(slider_percent_at(10, 200, 110) == 50, "slider middle");
    check(slider_percent_at(10, 200, -50) == 0, "slider clamps left");
    check(slider_percent_at(10, 200, 500) == 100, "slider clamps right");
}

void check_network_parse() {
    using astralia::NetworkMap;
    std::set<std::string> profiles = astralia::network_parse_profiles(
        "home:802-11-wireless\nWired connection 1:802-3-ethernet\nlab\\:5G:802-11-wireless\n");
    check(profiles == std::set<std::string>{"home", "lab:5G"}, "profiles keep Wi-Fi only and unescape colons");
    NetworkMap networks = astralia::network_parse_networks(
        "home:WPA2 WPA3:82:*\ncafe::40: \nlab\\:5G:WPA2:60: \ncafe:WPA2:90: \n:WPA2:30: \n", profiles);
    check(networks.size() == 3, "hidden SSIDs drop and duplicates merge");
    check(networks["home"].connected && networks["home"].existing && networks["home"].security == "WPA2/WPA3",
          "connected saved network");
    check(networks["cafe"].security == "--" && networks["cafe"].signal == 40 && !networks["cafe"].existing,
          "first duplicate wins; open network shows --");
    check(networks["lab:5G"].in_range && networks["lab:5G"].existing, "escaped SSID matches its profile");
    NetworkMap out_of_range = astralia::network_parse_networks("", {"office"});
    check(out_of_range.size() == 1 && !out_of_range["office"].in_range, "profiles out of range stay listed");
    check(astralia::network_visible_count(out_of_range) == 0, "out-of-range profiles are not visible");
    check(astralia::network_parse_wifi_device("wlan0:wifi:connected\nlo:loopback:unmanaged\n"), "wifi device found");
    check(!astralia::network_parse_wifi_device("wlan0:wifi:unmanaged\neth0:ethernet:connected\n"), "unmanaged wifi ignored");
    NetworkMap self_only = astralia::network_parse_networks("home:WPA2:80:*\n", {});
    check(astralia::network_scan_would_collapse(networks, self_only), "a self-only rescan is discarded");
    check(!astralia::network_scan_would_collapse(self_only, networks), "a fuller rescan is kept");
}

void check_status_panels() {
    using astralia::panel_clamp_scroll;
    check(panel_clamp_scroll(-10, 500, 200) == 0, "scroll clamps at the top");
    check(panel_clamp_scroll(400, 500, 200) == 300, "scroll clamps at the bottom");
    check(panel_clamp_scroll(50, 100, 200) == 0, "short content never scrolls");
    astralia::PanelRect a{0, 0, 100, 100};
    astralia::PanelRect clipped = astralia::panel_intersect(a, {50, 80, 100, 100});
    check(clipped.x == 50 && clipped.y == 80 && clipped.w == 50 && clipped.h == 20, "hit rects clip to the list");
    check(astralia::panel_intersect(a, {200, 0, 10, 10}).w == 0, "disjoint rects do not hit");
    check(astralia::network_signal_icon(80) == astralia::icon::wifi, "strong signal icon");
    check(astralia::network_signal_icon(10) == astralia::icon::wifi0, "weak signal icon");
    check(astralia::battery_time_left(0).empty(), "unknown time is blank");
    check(astralia::battery_time_left(45 * 60) == "45m", "minutes only");
    check(astralia::battery_time_left(2 * 3600 + 5 * 60) == "2h 5m", "hours and minutes");
    check(astralia::battery_state_label({true, 40, true, false, false, 3600}) == "Charging \xE2\x80\x94 1h 0m to full",
          "charging label");
    check(astralia::battery_state_label({true, 40, false, false, false, 600}) == "Discharging \xE2\x80\x94 10m left",
          "discharging label");
    check(astralia::battery_state_label({true, 80, false, false, true, 0}) == "Not charging", "pending label");
    check(astralia::battery_state_label({true, 100, false, true, false, 0}) == "Full", "full label");
}

void check_calendar() {
    using astralia::CalendarDay;
    std::array<CalendarDay, 42> oct = astralia::clock_panel_cells(2026, 9);
    check(oct[0].year == 2026 && oct[0].month == 8 && oct[0].day == 28 && !oct[0].in_month, "October 2026 grid starts on Monday 28 September");
    check(oct[3].month == 9 && oct[3].day == 1 && oct[3].in_month, "1 October 2026 is a Thursday");
    check(oct[41].month == 10 && oct[41].day == 8, "grid ends on 8 November");
    std::array<CalendarDay, 42> jun = astralia::clock_panel_cells(2026, 5);
    check(jun[0].day == 1 && jun[0].in_month, "a month starting on Monday has no leading days");
    check(astralia::clock_panel_same_day(oct[3], 2026, 9, 1), "same day matches");
    check(!astralia::clock_panel_same_day(oct[0], 2026, 9, 28), "same day needs the month");
    astralia::CalendarMonth back = astralia::clock_panel_month_shifted(2026, 0, -1);
    check(back.year == 2025 && back.month == 11, "January minus one is last December");
    astralia::CalendarMonth ahead = astralia::clock_panel_month_shifted(2026, 11, 13);
    check(ahead.year == 2028 && ahead.month == 0, "December plus 13 is January two years on");
    check(astralia::clock_panel_iso_week(2026, 0, 1) == 1, "1 January 2026 is week 1");
    check(astralia::clock_panel_iso_week(2027, 0, 1) == 53, "1 January 2027 is week 53 of 2026");
    check(astralia::clock_panel_iso_week(2024, 11, 30) == 1, "30 December 2024 is week 1 of 2025");
    check(astralia::clock_panel_iso_week(2026, 9, 4) == 40, "4 October 2026 is week 40");
}

void check_media() {
    using astralia::MediaPlayback;
    check(astralia::media_parse_playback("Playing") == MediaPlayback::playing, "Playing parses");
    check(astralia::media_parse_playback("Paused") == MediaPlayback::paused, "Paused parses");
    check(astralia::media_parse_playback("bogus") == MediaPlayback::stopped, "unknown is stopped");
    check(astralia::media_format_position(0) == "0:00", "zero position");
    check(astralia::media_format_position(65'000'000) == "1:05", "pads seconds");
    check(astralia::media_format_position(-5) == "0:00", "negative clamps");
    check(astralia::media_select_player({}) == -1, "no players");
    check(astralia::media_select_player({{"a", MediaPlayback::paused}, {"b", MediaPlayback::playing}}) == 1, "playing wins");
    check(astralia::media_select_player({{"a", MediaPlayback::paused}, {"b", MediaPlayback::stopped}}) == 0, "else the first");
}

void check_tray() {
    using astralia::TrayMenuLayout;
    using Props = std::map<std::string, sdbus::Variant>;
    check(astralia::tray_strip_mnemonic("_Open __file") == "Open _file", "mnemonics stripped, doubled kept");
    TrayMenuLayout leaf{int32_t{3}, Props{{"label", sdbus::Variant(std::string("_Quit"))}, {"enabled", sdbus::Variant(false)}}, std::vector<sdbus::Variant>{}};
    TrayMenuLayout separator{int32_t{2}, Props{{"type", sdbus::Variant(std::string("separator"))}}, std::vector<sdbus::Variant>{}};
    TrayMenuLayout toggle{int32_t{4}, Props{{"label", sdbus::Variant(std::string("Mute"))}, {"toggle-type", sdbus::Variant(std::string("checkmark"))}, {"toggle-state", sdbus::Variant(int32_t{1})}}, std::vector<sdbus::Variant>{}};
    TrayMenuLayout sub{int32_t{1}, Props{{"label", sdbus::Variant(std::string("More"))}, {"children-display", sdbus::Variant(std::string("submenu"))}}, std::vector<sdbus::Variant>{sdbus::Variant(toggle)}};
    TrayMenuLayout root{int32_t{0}, Props{}, std::vector<sdbus::Variant>{sdbus::Variant(sub), sdbus::Variant(separator), sdbus::Variant(leaf)}};
    astralia::TrayMenuEntry menu = astralia::tray_parse_menu(root);
    check(menu.children.size() == 3, "three top-level entries");
    check(menu.children[0].children.size() == 1 && menu.children[0].children[0].checkbox && menu.children[0].children[0].checked, "nested checked toggle");
    check(menu.children[1].separator, "separator parsed");
    check(menu.children[2].label == "Quit" && !menu.children[2].enabled, "disabled leaf label");
    check(astralia::tray_menu_height(&menu.children, false) == 2 * 6 + 2 * 32 + 9, "menu height counts rows and separator");
}

int main() {
    check_ms_until_next_second();
    check_parse_invocation();
    check_runtime_path();
    check_format_help();
    check_color();
    check_status_icons();
    check_workspace_row();
    check_cover();
    check_wallpaper_file();
    check_config_file();
    check_cover_cache();
    check_settings_layout();
    check_settings_file();
    check_launcher_text();
    check_launcher_modes();
    check_launcher_scoring();
    check_desktop_entry();
    check_launch_urls();
    check_drun_results();
    check_submenu();
    check_logout_layout();
    check_polkit_layout();
    check_notification_layout();
    check_status_changes();
    check_slider();
    check_network_parse();
    check_status_panels();
    check_calendar();
    check_media();
    check_tray();
    if (failures > 0) {
        std::println(stderr, "{} check(s) failed", failures);
        return EXIT_FAILURE;
    }
    std::println("all checks passed");
    return EXIT_SUCCESS;
}
