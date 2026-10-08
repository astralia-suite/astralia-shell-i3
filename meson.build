project(
  'astralia-shell',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.1',
  default_options: ['cpp_std=c++23', 'warning_level=3', 'optimization=2', 'debug=false'],
)

if get_option('native_cpu')
  add_project_arguments('-march=westmere', language: 'cpp')
endif

deps = [
  dependency('xcb', include_type: 'system'),
  dependency('xcb-ewmh', include_type: 'system'),
  dependency('xcb-icccm', include_type: 'system'),
  dependency('xcb-randr', include_type: 'system'),
  dependency('xcb-shape', include_type: 'system'),
  dependency('xcb-keysyms', include_type: 'system'),
  dependency('cairo', include_type: 'system'),
  dependency('cairo-xcb', include_type: 'system'),
  dependency('pangocairo', include_type: 'system'),
  dependency('fontconfig', include_type: 'system'),
  dependency('sdbus-c++', include_type: 'system'),
  dependency('resvg', include_type: 'system'),
  dependency('libjpeg', include_type: 'system'),
  dependency('xkbcommon', include_type: 'system'),
  dependency('xkbcommon-x11', include_type: 'system'),
  dependency('polkit-agent-1', include_type: 'system'),
  dependency('libpipewire-0.3', include_type: 'system'),
  dependency('threads'),
]

data_dir = get_option('prefix') / get_option('datadir') / 'astralia-shell'
font_dir = data_dir / 'fonts'
constellation_dir = data_dir / 'constellation'
logout_dir = data_dir / 'logout'
polkit_dir = data_dir / 'polkit'
add_project_arguments(
  '-DASTRALIA_FONT_DIR="@0@"'.format(font_dir),
  '-DASTRALIA_SOURCE_FONT_DIR="@0@"'.format(meson.project_source_root() / 'assets' / 'fonts'),
  '-DASTRALIA_CONSTELLATION_DIR="@0@"'.format(constellation_dir),
  '-DASTRALIA_SOURCE_CONSTELLATION_DIR="@0@"'.format(
    meson.project_source_root() / 'assets' / 'constellation',
  ),
  '-DASTRALIA_LOGOUT_DIR="@0@"'.format(logout_dir),
  '-DASTRALIA_SOURCE_LOGOUT_DIR="@0@"'.format(meson.project_source_root() / 'assets' / 'logout'),
  '-DASTRALIA_POLKIT_DIR="@0@"'.format(polkit_dir),
  '-DASTRALIA_SOURCE_POLKIT_DIR="@0@"'.format(meson.project_source_root() / 'assets' / 'polkit'),
  language: 'cpp',
)
install_data(
  'assets/fonts/tabler-icons.ttf',
  'assets/fonts/ComicShannsMono-Regular.otf',
  'assets/fonts/YujiMai.ttf',
  install_dir: font_dir,
)
install_data('assets/logout/logo.png', install_dir: logout_dir)
install_data('assets/polkit/electro.png', install_dir: polkit_dir)
install_data(
  'assets/constellation/C1.png',
  'assets/constellation/C2.png',
  'assets/constellation/C3.png',
  'assets/constellation/C4.png',
  'assets/constellation/C5.png',
  'assets/constellation/C6.png',
  install_dir: constellation_dir,
)

inc = include_directories('src')

core = static_library(
  'astralia-core',
  files(
    'src/app/services.cpp',
    'src/core/allocator.cpp',
    'src/core/async_process.cpp',
    'src/core/cli.cpp',
    'src/core/config_file.cpp',
    'src/core/daemon.cpp',
    'src/core/dbus.cpp',
    'src/core/event_loop.cpp',
    'src/core/ipc.cpp',
    'src/core/json.cpp',
    'src/core/keyboard.cpp',
    'src/core/log.cpp',
    'src/core/runtime_paths.cpp',
    'src/core/single_instance.cpp',
    'src/core/spawn.cpp',
    'src/core/x_connection.cpp',
    'src/modules/bar.cpp',
    'src/modules/bar_set.cpp',
    'src/modules/bar/panel/battery_panel.cpp',
    'src/modules/bar/panel/bluetooth_panel.cpp',
    'src/modules/bar/panel/brightness_panel.cpp',
    'src/modules/bar/panel/clock_panel.cpp',
    'src/modules/bar/panel/media_panel.cpp',
    'src/modules/bar/panel/network_panel.cpp',
    'src/modules/bar/panel/tray_panel.cpp',
    'src/modules/bar/panel/volume_panel.cpp',
    'src/modules/bar/styles/continuous.cpp',
    'src/modules/bar/styles/geometry.cpp',
    'src/modules/bar/styles/okinami.cpp',
    'src/modules/bar/widget/battery_widget.cpp',
    'src/modules/bar/widget/bluetooth_widget.cpp',
    'src/modules/bar/widget/brightness_widget.cpp',
    'src/modules/bar/widget/clock_widget.cpp',
    'src/modules/bar/widget/dock_widget.cpp',
    'src/modules/bar/widget/logout_widget.cpp',
    'src/modules/bar/widget/media_widget.cpp',
    'src/modules/bar/widget/network_widget.cpp',
    'src/modules/bar/widget/tray_widget.cpp',
    'src/modules/bar/widget/volume_widget.cpp',
    'src/modules/bar/widget/widget_capsule.cpp',
    'src/modules/bar/widget/workspace_widget.cpp',
    'src/modules/launcher.cpp',
    'src/modules/launcher/apps_provider.cpp',
    'src/modules/launcher/desktop_entry.cpp',
    'src/modules/launcher/files_provider.cpp',
    'src/modules/launcher/launch_action.cpp',
    'src/modules/launcher/search.cpp',
    'src/modules/launcher/search_process.cpp',
    'src/modules/launcher/submenu.cpp',
    'src/modules/launcher/visit_store.cpp',
    'src/modules/logout.cpp',
    'src/modules/logout/layout.cpp',
    'src/modules/notification.cpp',
    'src/modules/notification/layout.cpp',
    'src/modules/osd.cpp',
    'src/modules/overview.cpp',
    'src/modules/overview/layout.cpp',
    'src/modules/polkit.cpp',
    'src/modules/polkit/layout.cpp',
    'src/modules/settings.cpp',
    'src/modules/settings/bar_tab.cpp',
    'src/modules/settings/displays_tab.cpp',
    'src/modules/settings/layout.cpp',
    'src/modules/settings/thumbnail_cache.cpp',
    'src/modules/settings/wallpaper_tab.cpp',
    'src/modules/settings/widgets.cpp',
    'src/modules/wallpaper.cpp',
    'src/render/app_fonts.cpp',
    'src/render/app_icon.cpp',
    'src/render/cover_cache.cpp',
    'src/render/draw.cpp',
    'src/render/image_decode.cpp',
    'src/render/panel_chrome.cpp',
    'src/render/panel_window.cpp',
    'src/render/slider.cpp',
    'src/render/text.cpp',
    'src/render/x_window.cpp',
    'src/service/audio_service.cpp',
    'src/service/battery_service.cpp',
    'src/service/bluetooth_service.cpp',
    'src/service/brightness_service.cpp',
    'src/service/i3_service.cpp',
    'src/service/media_service.cpp',
    'src/service/network_service.cpp',
    'src/service/notification_service.cpp',
    'src/service/output_service.cpp',
    'src/service/polkit_service.cpp',
    'src/service/settings_service.cpp',
    'src/service/tray_service.cpp',
    'src/service/user_service.cpp',
    'src/service/wallpaper_service.cpp',
  ),
  include_directories: inc,
  dependencies: deps,
)

executable(
  'astralia-shell',
  'src/main.cpp',
  include_directories: inc,
  link_with: core,
  dependencies: deps,
  install: true,
)

unit = executable(
  'astralia-shell-test',
  'test/main.cpp',
  include_directories: inc,
  link_with: core,
  dependencies: deps,
)

test('unit', unit)
