#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <sdbus-c++/sdbus-c++.h>
#include <set>
#include <string>
#include <vector>

#include "core/async_process.h"
#include "core/dbus.h"
#include "core/event_loop.h"
#include "core/signal.h"

namespace astralia {

enum class NetworkKind { none,
                         ethernet,
                         wifi };

struct NetworkStatus {
    NetworkKind kind = NetworkKind::none;
    int strength = 0;
    std::string ssid;
    bool portal = false;
    bool operator==(const NetworkStatus &) const = default;
};

struct StatusMessage {
    std::string summary;
    std::string body;
    bool operator==(const StatusMessage &) const = default;
};

struct NetworkInfo {
    std::string ssid;
    std::string security;
    int signal = 0;
    bool connected = false;
    bool existing = false;
    bool in_range = false;
    bool operator==(const NetworkInfo &) const = default;
};

using NetworkMap = std::map<std::string, NetworkInfo>;

std::vector<StatusMessage> network_changes(const NetworkStatus &prev, const NetworkStatus &next);
NetworkMap network_parse_networks(const std::string &text, const std::set<std::string> &profiles);
bool network_parse_wifi_device(const std::string &text);
std::set<std::string> network_parse_profiles(const std::string &text);
int network_visible_count(const NetworkMap &networks);
bool network_scan_would_collapse(const NetworkMap &current, const NetworkMap &parsed);

class NetworkService {
  public:
    NetworkService(SystemBus &bus, EventLoop &loop);
    const NetworkStatus &status() const { return status_; }
    const NetworkMap &networks() const { return networks_; }
    bool wifi_available() const { return wifi_available_; }
    bool wifi_enabled() const { return wifi_enabled_; }
    bool scanning() const { return scanning_; }
    const std::string &connecting_to() const { return connecting_to_; }
    const std::string &last_error() const { return last_error_; }

    void start_watch();
    void stop_watch();
    void scan();
    void connect(const std::string &ssid, const std::string &password);
    void disconnect(const std::string &ssid);
    void forget(const std::string &ssid);
    void set_wifi_enabled(bool enabled);
    void clear_error();

    Signal<> changed;
    Signal<const StatusMessage &> messages;

  private:
    bool refresh();
    void on_profiles(const std::string &output);
    void on_quick_scan(const std::string &output);
    void on_scan(const std::string &output);
    void on_connect(const std::string &output);
    void schedule_rescan(std::chrono::milliseconds delay);
    void cancel_scans();

    SystemBus &bus_;
    EventLoop &loop_;
    NetworkStatus status_;
    std::unique_ptr<sdbus::IProxy> manager_;
    sdbus::Slot match_;

    NetworkMap networks_;
    std::set<std::string> profiles_;
    bool watching_ = false;
    bool wifi_available_ = true;
    bool wifi_enabled_ = false;
    bool scanning_ = false;
    bool scan_pending_ = false;
    std::string connecting_to_;
    std::string last_error_;
    AsyncProcess device_proc_;
    AsyncProcess profile_proc_;
    AsyncProcess quick_scan_proc_;
    AsyncProcess scan_proc_;
    AsyncProcess connect_proc_;
    AsyncProcess disconnect_proc_;
    AsyncProcess forget_proc_;
    std::optional<std::chrono::steady_clock::time_point> next_rescan_;
    int rescan_timer_ = -1;
};

} // namespace astralia
