#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/dbus.h"
#include "core/event_loop.h"
#include "core/signal.h"

namespace astralia {

struct TrayItem {
    std::string bus_name;
    std::string object_path;
    std::string icon_name;
    std::string icon_theme_path;
    std::string menu_path;

    std::string key() const { return bus_name + "|" + object_path; }
};

struct TrayMenuEntry {
    int32_t id = 0;
    std::string label;
    bool enabled = true;
    bool visible = true;
    bool separator = false;
    bool checkbox = false;
    bool checked = false;
    std::vector<TrayMenuEntry> children;
};

using TrayMenuLayout = sdbus::Struct<int32_t, std::map<std::string, sdbus::Variant>, std::vector<sdbus::Variant>>;

TrayMenuEntry tray_parse_menu(const TrayMenuLayout &node);
std::string tray_strip_mnemonic(const std::string &label);

class TrayService {
  public:
    explicit TrayService(EventLoop &loop);
    TrayService(const TrayService &) = delete;
    TrayService &operator=(const TrayService &) = delete;

    const std::vector<TrayItem> &items() const { return items_; }
    const TrayItem *find(const std::string &key) const;
    const std::vector<TrayMenuEntry> *menu(const std::string &key) const;
    void activate(const std::string &key);
    void request_menu(const std::string &key);
    void menu_clicked(const std::string &key, int32_t id);

    Signal<> changed;

  private:
    struct Proxies {
        std::unique_ptr<sdbus::IProxy> item;
        std::unique_ptr<sdbus::IProxy> menu;
        std::string menu_path;
    };

    void register_item(const std::string &bus_name, const std::string &object_path);
    void fetch(const std::string &key);
    void remove_owner(const std::string &name);
    sdbus::IProxy *menu_proxy(const std::string &key);

    SystemBus bus_;
    std::unique_ptr<sdbus::IObject> object_;
    std::unique_ptr<sdbus::IProxy> daemon_;
    std::vector<TrayItem> items_;
    std::unordered_map<std::string, Proxies> proxies_;
    std::unordered_map<std::string, std::vector<TrayMenuEntry>> menus_;
};

} // namespace astralia
