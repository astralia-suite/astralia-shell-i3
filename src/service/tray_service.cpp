#include <algorithm>
#include <optional>

#include "core/log.h"

#include "service/tray_service.h"

namespace astralia {

namespace {

constexpr const char *watcher_name = "org.kde.StatusNotifierWatcher";
constexpr const char *watcher_path = "/StatusNotifierWatcher";
constexpr const char *item_interface = "org.kde.StatusNotifierItem";
constexpr const char *default_item_path = "/StatusNotifierItem";
constexpr const char *menu_interface = "com.canonical.dbusmenu";
constexpr const char *properties_interface = "org.freedesktop.DBus.Properties";

using Properties = std::map<std::string, sdbus::Variant>;

template <typename T>
std::optional<T> variant_get(const Properties &props, const std::string &name) {
    auto it = props.find(name);
    if (it == props.end() || !it->second.containsValueOfType<T>()) {
        return std::nullopt;
    }
    return it->second.get<T>();
}

void apply_properties(TrayItem &item, const Properties &props) {
    if (auto v = variant_get<std::string>(props, "IconName")) {
        item.icon_name = *v;
    }
    if (auto v = variant_get<std::string>(props, "IconThemePath")) {
        item.icon_theme_path = *v;
    }
    if (auto v = variant_get<sdbus::ObjectPath>(props, "Menu")) {
        item.menu_path = *v;
    }
}

} // namespace

std::string tray_strip_mnemonic(const std::string &label) {
    std::string out;
    for (std::size_t i = 0; i < label.size(); ++i) {
        if (label[i] == '_') {
            if (i + 1 < label.size() && label[i + 1] == '_') {
                out += '_';
                ++i;
            }
            continue;
        }
        out += label[i];
    }
    return out;
}

TrayMenuEntry tray_parse_menu(const TrayMenuLayout &node) {
    TrayMenuEntry entry;
    entry.id = std::get<0>(node);
    const Properties &props = std::get<1>(node);
    entry.label = tray_strip_mnemonic(variant_get<std::string>(props, "label").value_or(""));
    entry.enabled = variant_get<bool>(props, "enabled").value_or(true);
    entry.visible = variant_get<bool>(props, "visible").value_or(true);
    entry.separator = variant_get<std::string>(props, "type").value_or("") == "separator";
    entry.checkbox = !variant_get<std::string>(props, "toggle-type").value_or("").empty();
    entry.checked = variant_get<int32_t>(props, "toggle-state").value_or(0) == 1;
    for (const sdbus::Variant &child : std::get<2>(node)) {
        if (child.containsValueOfType<TrayMenuLayout>()) {
            entry.children.push_back(tray_parse_menu(child.get<TrayMenuLayout>()));
        }
    }
    return entry;
}

TrayService::TrayService(EventLoop &loop) : bus_(loop, BusKind::session) {
    sdbus::IConnection *conn = bus_.conn();
    if (conn == nullptr) {
        return;
    }
    try {
        object_ = sdbus::createObject(*conn, sdbus::ObjectPath(watcher_path));
        object_
            ->addVTable(
                sdbus::registerMethod("RegisterStatusNotifierItem").implementedAs([this](const std::string &service) {
                    std::string sender = object_->getCurrentlyProcessedMessage().getSender();
                    register_item(sender, service.starts_with('/') ? service : default_item_path);
                }),
                sdbus::registerMethod("RegisterStatusNotifierHost").implementedAs([](const std::string &) {}),
                sdbus::registerProperty("RegisteredStatusNotifierItems").withGetter([this] {
                    std::vector<std::string> out;
                    for (const TrayItem &item : items_) {
                        out.push_back(item.bus_name + item.object_path);
                    }
                    return out;
                }),
                sdbus::registerProperty("IsStatusNotifierHostRegistered").withGetter([] { return true; }),
                sdbus::registerProperty("ProtocolVersion").withGetter([] { return int32_t{0}; }))
            .forInterface(watcher_name);
        conn->requestName(sdbus::ServiceName(watcher_name));
    } catch (const sdbus::Error &error) {
        log::error("tray: cannot own {}: {}", watcher_name, error.what());
        object_.reset();
        return;
    }
    daemon_ = bus_.proxy("org.freedesktop.DBus", "/org/freedesktop/DBus");
    if (daemon_) {
        daemon_->uponSignal("NameOwnerChanged").onInterface("org.freedesktop.DBus").call([this](const std::string &name, const std::string &, const std::string &owner) {
            if (owner.empty()) {
                remove_owner(name);
            }
        });
    }
}

const TrayItem *TrayService::find(const std::string &key) const {
    auto it = std::ranges::find_if(items_, [&](const TrayItem &item) { return item.key() == key; });
    return it == items_.end() ? nullptr : &*it;
}

const std::vector<TrayMenuEntry> *TrayService::menu(const std::string &key) const {
    auto it = menus_.find(key);
    return it == menus_.end() ? nullptr : &it->second;
}

void TrayService::register_item(const std::string &bus_name, const std::string &object_path) {
    TrayItem item{bus_name, object_path, {}, {}, {}};
    std::string key = item.key();
    if (find(key) != nullptr) {
        return;
    }
    std::unique_ptr<sdbus::IProxy> proxy = bus_.proxy(bus_name, object_path);
    if (!proxy) {
        return;
    }
    proxy->uponSignal("PropertiesChanged").onInterface(properties_interface).call([this, key](const std::string &, const Properties &, const std::vector<std::string> &) { fetch(key); });
    for (const char *signal : {"NewIcon", "NewStatus", "NewMenu"}) {
        proxy->uponSignal(signal).onInterface(item_interface).call([this, key] { fetch(key); });
    }
    proxies_[key].item = std::move(proxy);
    items_.push_back(std::move(item));
    log::info("tray: registered {}", key);
    fetch(key);
    changed.emit();
}

void TrayService::fetch(const std::string &key) {
    auto it = proxies_.find(key);
    if (it == proxies_.end()) {
        return;
    }
    try {
        it->second.item->callMethodAsync("GetAll").onInterface(properties_interface).withArguments(std::string(item_interface)).uponReplyInvoke([this, key](std::optional<sdbus::Error> error, Properties props) {
            if (error) {
                log::error("tray: GetAll failed for {}: {}", key, error->getMessage());
                return;
            }
            auto item = std::ranges::find_if(items_, [&](const TrayItem &i) { return i.key() == key; });
            if (item == items_.end()) {
                return;
            }
            apply_properties(*item, props);
            changed.emit();
        });
    } catch (const sdbus::Error &error) {
        log::error("tray: GetAll dispatch failed for {}: {}", key, error.what());
    }
}

void TrayService::remove_owner(const std::string &name) {
    std::string prefix = name + "|";
    std::size_t removed = std::erase_if(items_, [&](const TrayItem &item) { return item.bus_name == name; });
    std::erase_if(proxies_, [&](const auto &kv) { return kv.first.starts_with(prefix); });
    std::erase_if(menus_, [&](const auto &kv) { return kv.first.starts_with(prefix); });
    if (removed > 0) {
        changed.emit();
    }
}

void TrayService::activate(const std::string &key) {
    auto it = proxies_.find(key);
    if (it == proxies_.end()) {
        return;
    }
    try {
        it->second.item->callMethodAsync("Activate").onInterface(item_interface).withArguments(int32_t{0}, int32_t{0}).uponReplyInvoke([](std::optional<sdbus::Error> error) {
            if (error) {
                log::error("tray: Activate failed: {}", error->getMessage());
            }
        });
    } catch (const sdbus::Error &error) {
        log::error("tray: Activate dispatch failed: {}", error.what());
    }
}

sdbus::IProxy *TrayService::menu_proxy(const std::string &key) {
    const TrayItem *item = find(key);
    auto it = proxies_.find(key);
    if (item == nullptr || it == proxies_.end() || item->menu_path.empty()) {
        return nullptr;
    }
    Proxies &proxies = it->second;
    if (!proxies.menu || proxies.menu_path != item->menu_path) {
        proxies.menu = bus_.proxy(item->bus_name, item->menu_path);
        proxies.menu_path = item->menu_path;
    }
    return proxies.menu.get();
}

void TrayService::request_menu(const std::string &key) {
    sdbus::IProxy *proxy = menu_proxy(key);
    if (proxy == nullptr) {
        return;
    }
    try {
        proxy->callMethodAsync("AboutToShow").onInterface(menu_interface).withArguments(int32_t{0}).uponReplyInvoke([](std::optional<sdbus::Error>, bool) {});
        proxy->callMethodAsync("GetLayout").onInterface(menu_interface).withArguments(int32_t{0}, int32_t{-1}, std::vector<std::string>{}).uponReplyInvoke([this, key](std::optional<sdbus::Error> error, uint32_t, TrayMenuLayout layout) {
            if (error) {
                log::error("tray: GetLayout failed for {}: {}", key, error->getMessage());
                return;
            }
            if (find(key) == nullptr) {
                return;
            }
            menus_[key] = tray_parse_menu(layout).children;
            changed.emit();
        });
    } catch (const sdbus::Error &error) {
        log::error("tray: menu request failed for {}: {}", key, error.what());
    }
}

void TrayService::menu_clicked(const std::string &key, int32_t id) {
    sdbus::IProxy *proxy = menu_proxy(key);
    if (proxy == nullptr) {
        return;
    }
    try {
        proxy->callMethodAsync("Event").onInterface(menu_interface).withArguments(id, std::string("clicked"), sdbus::Variant(int32_t{0}), uint32_t{0}).uponReplyInvoke([](std::optional<sdbus::Error> error) {
            if (error) {
                log::error("tray: menu Event failed: {}", error->getMessage());
            }
        });
    } catch (const sdbus::Error &error) {
        log::error("tray: menu Event dispatch failed: {}", error.what());
    }
}

} // namespace astralia
