#include <utility>
#include <xcb/xcb_ewmh.h>

#include "service/workspace_service.h"

namespace astralia {

WorkspaceService::WorkspaceService(XConnection &x, EventLoop &loop, std::function<void()> on_change)
    : x_(x), on_change_(std::move(on_change)) {
    uint32_t mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(x_.conn(), x_.root(), XCB_CW_EVENT_MASK, &mask);
    refresh();
    loop.on_window(x_.root(), [this](const xcb_generic_event_t &event) {
        const auto &property = reinterpret_cast<const xcb_property_notify_event_t &>(event);
        xcb_ewmh_connection_t *ewmh = x_.ewmh();
        if (property.atom != ewmh->_NET_CURRENT_DESKTOP &&
            property.atom != ewmh->_NET_NUMBER_OF_DESKTOPS) {
            return;
        }
        if (refresh()) {
            on_change_();
        }
    });
}

void WorkspaceService::switch_to(uint32_t index) {
    if (index >= status_.count || index == status_.current) {
        return;
    }
    xcb_ewmh_request_change_current_desktop(x_.ewmh(), 0, index, XCB_CURRENT_TIME);
}

bool WorkspaceService::refresh() {
    xcb_ewmh_connection_t *ewmh = x_.ewmh();
    auto count_cookie = xcb_ewmh_get_number_of_desktops(ewmh, 0);
    auto current_cookie = xcb_ewmh_get_current_desktop(ewmh, 0);
    WorkspaceStatus next;
    if (xcb_ewmh_get_number_of_desktops_reply(ewmh, count_cookie, &next.count, nullptr) == 0) {
        next.count = 0;
    }
    if (xcb_ewmh_get_current_desktop_reply(ewmh, current_cookie, &next.current, nullptr) == 0) {
        next.current = 0;
    }
    if (next == status_) {
        return false;
    }
    status_ = next;
    return true;
}

} // namespace astralia
