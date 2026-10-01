#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/cli.h"
#include "core/icons.h"
#include "core/ipc.h"
#include "core/palette.h"
#include "core/runtime_paths.h"

#include "modules/bar/clock_widget.h"
#include "modules/bar/status_widget.h"
#include "modules/bar/workspace_widget.h"
#include "modules/launcher/app_icon.h"
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
#include "modules/wallpaper/config_file.h"
#include "modules/wallpaper/image.h"

#include "service/bluetooth_service.h"
#include "service/network_service.h"

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
    astralia::WorkspaceStatus status{3, 1};
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
    check(astralia::wallpaper_file_path("/cfg", "/home/u") == "/cfg/astralia-shell/wallpaper.conf",
          "XDG_CONFIG_HOME wins");
    check(astralia::wallpaper_file_path("", "/home/u") ==
              "/home/u/.config/astralia-shell/wallpaper.conf",
          "empty XDG_CONFIG_HOME falls back to ~/.config");
    check(astralia::expand_home("~/a.png", "/home/u") == "/home/u/a.png", "~/ expands");
    check(astralia::expand_home("/a/~b.png", "/home/u") == "/a/~b.png", "inner ~ is kept");

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
    check(offset.x == 1920 + 1280 - 7 - 267 && offset.y == 1024 - 7 - 34,
          "stack follows output offset and scale");
    check(astralia::notification_close_at(390.0, 0.0, heights) == 0, "top card x hits");
    check(astralia::notification_close_at(390.0, 60.0, heights) == 1, "second card x hits");
    check(!astralia::notification_close_at(200.0, 0.0, heights), "card body hits nothing");
    check(!astralia::notification_close_at(390.0, 45.0, heights), "below the x hits nothing");
    check(!astralia::notification_close_at(390.0, 130.0, heights), "below the stack hits nothing");
}

} // namespace

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
    if (failures > 0) {
        std::println(stderr, "{} check(s) failed", failures);
        return EXIT_FAILURE;
    }
    std::println("all checks passed");
    return EXIT_SUCCESS;
}
